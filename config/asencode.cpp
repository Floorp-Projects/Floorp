/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the AppleSingle decoder.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Johnny Stenback <jst@mozilla.org>  (Original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>

#define EXIT_IF_FALSE(x)                                                      \
  do {                                                                        \
    if (!(x)) {                                                               \
      printf("Assertion failure: %s\n"                                        \
             "  at %s line %d\n",                                             \
             #x, __FILE__, __LINE__);                                         \
      exit(1);                                                                \
    }                                                                         \
  } while (0)

// encodes a file with data and resource forks into a single
// AppleSingle encoded file..

static void append_file(FILE* output, const char* input_name)
{
  FILE* input = fopen(input_name, "rb");
  EXIT_IF_FALSE(input != NULL);

  while (1) {
    char buffer[4096];
    size_t amount = fread(buffer, 1, sizeof(buffer), input);
    if (amount == 0) {
      EXIT_IF_FALSE(feof(input) != 0);
      break;
    }
    fwrite(buffer, 1, amount, output);
  }
  fclose(input);
}

int main(int argc, char** argv)
{
  if (argc < 3) {
    printf("usage: %s input output\n", argv[0]);
    exit(1);
  }

  const char *input_name = argv[1];

  struct stat input_st;
  if (stat(input_name, &input_st) != 0) {
    printf("%s: can't open file `%s'\n", argv[0], input_name);
    exit(2);
  }

  if ((input_st.st_mode & S_IFMT) != S_IFREG) {
    printf("%s: file `%s' not a regular file\n", argv[0], input_name);
    exit(3);
  }

  char rez_name[512];
  strcpy(rez_name, input_name);
  strcat(rez_name, "/rsrc");

  struct stat rez_st;
  EXIT_IF_FALSE(stat(rez_name, &rez_st) == 0);

  if (rez_st.st_size == 0) {
    printf("%s: no resource fork found on file `%s'\n", argv[0], argv[1]);
    exit(4);
  }

  FILE* output = fopen(argv[2], "wb");
  if (output == NULL) {
    printf("%s: can't open file `%s'\n", argv[0], argv[2]);
    exit(5);
  }

  struct header {
    int magic_number;
    int version_number;
    char filler[16];
  } header;

  header.magic_number = 0x00051600;
  header.version_number = 0x00020000;

  EXIT_IF_FALSE(fwrite(&header, sizeof(header), 1, output) == 1);

  short entry_count = 4;
  EXIT_IF_FALSE(fwrite(&entry_count, sizeof(entry_count), 1, output) == 1);

  struct entry {
    unsigned int id;
    unsigned int offset;
    unsigned int length;
  };

  struct dates
  {
    int create; /* file creation date/time */
    int modify; /* last modification date/time */
    int backup; /* last backup date/time */
    int access; /* last access date/time */
  } dates;

  char *name_buf = strdup(input_name);
  char *orig_name = basename(name_buf);
  int orig_name_len = strlen(orig_name);

  entry entries[entry_count];

  int header_end = sizeof(header) + sizeof(entry_count) + sizeof(entries);

  entries[0].id = 1; // data fork
  entries[0].offset = header_end;
  entries[0].length = input_st.st_size;

  entries[1].id = 2; // data fork
  entries[1].offset = entries[0].offset + entries[0].length;
  entries[1].length = rez_st.st_size;

  entries[2].id = 3; // file name
  entries[2].offset = entries[1].offset + entries[1].length;
  entries[2].length = orig_name_len;

  entries[3].id = 8; // file dates
  entries[3].offset = entries[2].offset + entries[2].length;
  entries[3].length = sizeof(dates);

  EXIT_IF_FALSE(fwrite(entries, sizeof(entry), entry_count, output) ==
                entry_count);

  append_file(output, input_name);
  append_file(output, rez_name);

  EXIT_IF_FALSE(fwrite(orig_name, 1, orig_name_len, output) == orig_name_len);

  // Dates in an AppleSingle encoded file should be the number of
  // seconds since (or to) 00:00:00, January 1, 2000 UTC
#define Y2K_SECONDS (946710000U)

  dates.create = input_st.st_ctime - Y2K_SECONDS;
  dates.modify = input_st.st_mtime - Y2K_SECONDS;
  dates.backup = 0x80000000; // earliest possible time
  dates.access = input_st.st_atime - Y2K_SECONDS;

  EXIT_IF_FALSE(fwrite(&dates, 1, sizeof(dates), output) == sizeof(dates));

  fclose(output);

  return 0;
}
