/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include <string.h>
#include <stdio.h>

//-----------------------------------------------------------------------------

#define HEXDUMP_MAX_ROWS 16

static void hex_dump(unsigned int *state, const unsigned char *buf, int n)
{
  const unsigned char *p;
  while (n) {
    printf("  %08x:  ", *state);
    *state += HEXDUMP_MAX_ROWS;

    p = buf;

    int i, row_max = n;
    if (row_max > HEXDUMP_MAX_ROWS)
      row_max = HEXDUMP_MAX_ROWS;

    // print hex codes:
    for (i = 0; i < row_max; ++i) {
      printf("%02x  ", *p++);
    }
    for (i = row_max; i < HEXDUMP_MAX_ROWS; ++i) {
      printf("    ");
    }

    // print ASCII glyphs if possible:
    p = buf;
    for (i = 0; i < row_max; ++i, ++p) {
      if (*p < 0x7F && *p > 0x1F) {
        printf("%c", *p);
      } else {
        printf("%c", '.');
      }
    }
    printf("\n");

    buf += row_max;
    n -= row_max;
  }
}

//-----------------------------------------------------------------------------

typedef int (* TypeHandler)(const unsigned char *buf, int bufLen);

struct TypeMap {
  unsigned char type;
  const char *name;
  TypeHandler handler; 
};

static int dump_profile_event(const unsigned char *buf, int bufLen);
static int dump_custom_event(const unsigned char *buf, int bufLen);
static int dump_load_event(const unsigned char *buf, int bufLen);
static int dump_ui_event(const unsigned char *buf, int bufLen);

static TypeMap g_typemap[] = {
  { '\x00', "PROFILE", dump_profile_event },
  { '\x01', "CUSTOM", dump_custom_event },
  { '\x02', "LOAD", dump_load_event },
  { '\x03', "UI", dump_ui_event }
};

int dump_profile_event(const unsigned char *buf, int bufLen)
{
  return -1;  // XXX Implement me!
}

int dump_load_event(const unsigned char *buf, int bufLen)
{
  return -1;  // XXX Implement me!
}

int dump_ui_event(const unsigned char *buf, int bufLen)
{
  return -1;  // XXX Implement me!
}

int dump_custom_event(const unsigned char *buf, int bufLen)
{
  unsigned short num;
  memcpy(&num, buf, sizeof(num));
  unsigned int state = 0;
  hex_dump(&state, buf + 2, num);
  return 2 + num;
}

static void dump_data(const unsigned char *buf, int bufLen)
{
  const unsigned char *end = buf + bufLen;
  while (buf < end) {
    int n = -1;
    for (size_t i = 0; i < sizeof(g_typemap)/sizeof(g_typemap[0]); ++i) {
      if (g_typemap[i].type == *buf) {
        static unsigned long g_first_timestamp = 0;
        // dump the header, which consists of a single byte identifier
        // for the type, and a 32-bit timestamp (seconds since epoch)
        unsigned long ts;
        memcpy(&ts, buf + 1, 4);
        if (g_first_timestamp == 0) {
          g_first_timestamp = ts; 
          ts = 0;
        } else {
          ts -= g_first_timestamp;
        }
        printf("%s(%lu):\n", g_typemap[i].name, ts);

        buf += 5;

        n = g_typemap[i].handler(buf, bufLen);
        if (n > 0)
          buf += n;
        break;
      }
    }
    if (n == -1) {
      fprintf(stderr, "error: unhandled event type!\n");
      return;
    }
  }
}

//-----------------------------------------------------------------------------

int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "usage: DumpMetrics metrics.dat\n");
    return -1;
  }

  FILE *fp = fopen(argv[1], "rb");
  if (!fp)
    return -1;

  fseek(fp, 0, SEEK_END);
  long len = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  unsigned char *buf = new unsigned char[len];
  if (fread(buf, len, 1, fp) != 1)
    len = 0;
  if (len)
    dump_data(buf, len);
  delete[] buf;

  fclose(fp);
  return 0;
}
