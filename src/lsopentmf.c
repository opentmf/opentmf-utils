/**
 * Utility to list OpenTMF driver information.
 *
 * Copyright (C) 2016 Reinder Feenstra <reinderfeenstra@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <opentmf.h>

#define VERBOSE_MAX  2

static void print_version()
{
  printf("lsopentmf (opentmf-utils) " PACKAGE_VERSION "\n");
}

static void print_help()
{
  printf("Usage: lsopentmf [options]\n"
         "List OpenTMF drivers\n"
         "  -d, --devices\n"
         "    Show available devices per driver\n"
         "  -v, --verbose\n"
         "    Show more driver details, may be given multiple times\n"
         "  -h, --help\n"
         "    Show usage and help\n"
         "  -V, --version\n"
         "    Show program version\n");
}

static void print_multi_line(const char* label, const char* text)
{
  const char* new_line = strchr(text, '\n');

  if(new_line)
  {
    printf("%s:\n", label);

    do
    {
      int length = 1 + new_line - text;
      printf("  %.*s", length, text);
      text += length;
    }
    while(new_line = strchr(text, '\n'));

    if(text[0] != '\0')
      printf("  %s\n", text);
  }
  else
    printf("%s: %s\n", label, text);
}

int main(int argc, char* argv[])
{
  static const struct option options[] = {
    { "devices" , no_argument , 0 , 'd' } ,
    { "verbose" , no_argument , 0 , 'v' } ,
    { "help"    , no_argument , 0 , 'h' } ,
    { "version" , no_argument , 0 , 'V' } ,
    { NULL , 0 , 0 , 0 }
  };

  int status = EXIT_SUCCESS;
  int r;

  // Parse options:
  int verbose = 0;
  int show_devices = 0;

  while((r = getopt_long(argc, argv, "dvhV", options, 0)) != -1 )
  {
    switch(r)
    {
      case 'h':
        print_help();
        exit(EXIT_SUCCESS);

      case 'd':
        show_devices = 1;
        break;

      case 'v':
        if(verbose < VERBOSE_MAX)
          verbose++;
        break;

      case 'V':
        print_version();
        exit(EXIT_SUCCESS);

      default:
        print_help();
        exit(EXIT_FAILURE);
    }
  }

  struct opentmf_context* ctx;
  if((r = opentmf_init(&ctx)) != OPENTMF_SUCCESS)
  {
    fprintf(stderr, "Error initializing library: %s (%d)\n", opentmf_get_status_str(r), r);
    return EXIT_FAILURE;
  }

  char** list = NULL;
  if((r = opentmf_get_driver_list(ctx, &list)) == OPENTMF_SUCCESS)
  {
    size_t url_size = 1024;
    char* url = malloc(url_size);
    char** driver_name = list;
    struct opentmf_handle* drv;

    while(*driver_name)
    {
      if(10 + strlen(*driver_name) >= url_size)
      {
        url_size *= 2;
        url = realloc(url, url_size);
      }

      url[0] = '\0';
      strcat(url, "opentmf://");
      strcat(url, *driver_name);

      if((r = opentmf_open(ctx, url, &drv)) == OPENTMF_SUCCESS)
      {
        const struct opentmf_driver_info* info = opentmf_drv_get_info(drv);

        switch(verbose)
        {
          case 0:
            printf("%s\n", info->name);
            break;

          case 1:
            printf("%s\t%u.%u", info->name, info->version.major, info->version.minor);
            if(info->version.patch > 0)
              printf(".%u", info->version.patch);
            printf("%s\t%s\t%s\n", info->version.extra, info->license, info->non_free ? "non-free" : "free");
            break;

          case 2:
            printf("Driver: %s\n", info->name);
            printf("Version: %u.%u", info->version.major, info->version.minor);
            if(info->version.patch > 0)
              printf(".%u", info->version.patch);
            printf("%s\n", info->version.extra);
            print_multi_line("Description", info->description);
            print_multi_line("Authors", info->authors);
            printf("License: %s\n",info->license);
            printf("Free: %s\n", info->non_free ? "no" : "yes");
            printf("\n");
            break;
        }

        if(show_devices)
        {
          char** device_list = NULL;

          if((r = opentmf_drv_get_device_list(drv, &device_list)) == OPENTMF_SUCCESS)
          {
            struct opentmf_handle* dev;
            char** device = device_list;
            size_t url_base_len = strlen(url);

            while(*device)
            {
              url[url_base_len] = '\0';
              strcat(url, *device);

              if((r = opentmf_open(ctx, url, &dev)) == OPENTMF_SUCCESS)
              {
                const struct opentmf_device_info* device_info = opentmf_dev_get_info(dev);

                switch(verbose)
                {
                  case 0:
                    printf("  %s\n", *device);
                    break;

                  case 1:
                    printf("  %s\t%s\t%s\n", *device, device_info->name, device_info->serial);
                    break;

                  case 2:
                    printf("  Path: %s\n", *device);
                    printf("  Name: %s\n", device_info->name);
                    printf("  Serial: %s\n", device_info->serial);
                    printf("\n");
                    break;
                }

                opentmf_close(dev);
              }
              else
                fprintf(stderr, "Error opening device `%s`: %s (%d)\n", *device, opentmf_get_status_str(r), r);

              device++;
            }

            if((r = opentmf_drv_free_device_list(drv, device_list)) != OPENTMF_SUCCESS)
              fprintf(stderr, "Error freeing device list: %s (%d)\n", opentmf_get_status_str(r), r);
          }
          else
            fprintf(stderr, "Error getting device list: %s (%d)\n", opentmf_get_status_str(r), r);
        }
        opentmf_close(drv);
      }
      else
        fprintf(stderr, "Error opening driver `%s`: %s (%d)\n", *driver_name, opentmf_get_status_str(r), r);

      driver_name++;
    }

    free(url);

    if((r = opentmf_free_driver_list(ctx, list)) != OPENTMF_SUCCESS)
      fprintf(stderr, "Error freeing driver list: %s (%d)\n", opentmf_get_status_str(r), r);
  }
  else
  {
    fprintf(stderr, "Error getting driver list: %s (%d)\n", opentmf_get_status_str(r), r);
    status = EXIT_FAILURE;
  }

  if((r = opentmf_exit(ctx)) != OPENTMF_SUCCESS)
  {
    fprintf(stderr, "Error finalizing library: %s (%d)\n", opentmf_get_status_str(r), r);
    status = EXIT_FAILURE;
  }

  return status;
}
