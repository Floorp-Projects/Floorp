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
 *  Patrick Beard <beard@netscape.com>  (Original author)
 *  Brian Ryner <bryner@brianryner.com>
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

#define EXIT_IF_FALSE(x)                                                      \
  do {                                                                        \
    if (!(x)) {                                                               \
      printf("Assertion failure: %s\n"                                        \
             "  at %s line %d\n",                                             \
             #x, __FILE__, __LINE__);                                         \
      exit(1);                                                                \
    }                                                                         \
  } while (0)

// decodes a file into data and resource forks.

static int read_int(FILE* f)
{
  int result;
  EXIT_IF_FALSE(fread(&result, sizeof(result), 1, f) == 1);
  return result;
}

static void copy_range(FILE* input, size_t offset, size_t length,
		       const char* output_name)
{
  FILE* output = fopen(output_name, "wb");
  EXIT_IF_FALSE(output != NULL);
  fseek(input, offset, SEEK_SET);
  while (length != 0) {
    char buffer[4096];
    size_t amount = (length > sizeof(buffer) ? sizeof(buffer) : length);
    EXIT_IF_FALSE(fread(buffer, 1, amount, input) == amount);
    fwrite(buffer, 1, amount, output);
    length -= amount;
  }
  fclose(output);
}

int main(int argc, char** argv)
{
  if (argc < 3) {
    printf("usage:  %s input output\n", argv[0]);
    exit(1);
  }

  FILE* input = fopen(argv[1], "rb");
  if (input == NULL) {
    printf("%s: can't open file `%s'\n", argv[0], argv[1]);
    exit(2);
  }

  struct header {
    int magic_number;
    int version_number;
    char filler[16];
  } header;

  EXIT_IF_FALSE(fread(&header, sizeof(header), 1, input) == 1);
  EXIT_IF_FALSE(header.magic_number == 0x00051600);
  EXIT_IF_FALSE(header.version_number <= 0x00020000);
  // printf("sizeof(header) == %d\n", sizeof(header));

  short entry_count;
  EXIT_IF_FALSE(fread(&entry_count, sizeof(entry_count), 1, input) == 1);

  struct entry {
    unsigned int id;
    unsigned int offset;
    unsigned int length;
  };

  entry* entries = new entry[entry_count];
  EXIT_IF_FALSE(fread(entries, sizeof(entry), entry_count, input) == entry_count);

  entry* data_entry = NULL;
  entry* rez_entry = NULL;

  for (int i = 0; i < entry_count; i++) {
    entry& entry = entries[i];
    switch (entry.id) {
    case 1:
      // data fork.
      data_entry = &entry;
      break;
    case 2:
      rez_entry = &entry;
      break;
    }
  }

  const char* data_name = argv[2];
  if (data_entry && data_entry->length) {
    copy_range(input, data_entry->offset, data_entry->length, data_name);
  } else {
    // always create the data fork, even if the file doesn't have one.
    FILE* tmp = fopen(data_name, "wb");
    EXIT_IF_FALSE(tmp);
    fclose(tmp);
  }

  if (rez_entry && rez_entry->length) {
    char rez_name[512];
    strcpy(rez_name, data_name);
    strcat(rez_name, "/rsrc");
    copy_range(input, rez_entry->offset, rez_entry->length, rez_name);
  }

  delete[] entries;

  fclose(input);

  return 0;
}
