/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <stdio.h>
#include <string.h>
#include "nsColorNames.h"
#include "prprf.h"

static const char* kJunkNames[] = {
  nsnull,
  "",
  "123",
  "backgroundz",
  "zzzzzz",
  "#@$&@#*@*$@$#"
};

int main(int argc, char** argv)
{
  PRInt32 id;
  int rv = 0;

  // First make sure we can find all of the tags that are supposed to
  // be in the table. Futz with the case to make sure any case will
  // work
  const nsColorNames::NameTableEntry* et = &nsColorNames::kNameTable[0];
  const nsColorNames::NameTableEntry* end = &nsColorNames::kNameTable[COLOR_MAX];
  while (et < end) {
    // Lookup color by name and make sure it has the right id
    char tagName[100];
    id = nsColorNames::LookupName(et->name);
    if (id < 0) {
      printf("bug: can't find '%s'\n", et->name);
      rv = -1;
    }
    if (et->id != id) {
      printf("bug: name='%s' et->id=%d id=%d\n", et->name, et->id, id);
      rv = -1;
    }

    // fiddle with the case to make sure we can still find it
    strcpy(tagName, et->name);
    tagName[0] = tagName[0] - 32;
    id = nsColorNames::LookupName(tagName);
    if (id < 0) {
      printf("bug: can't find '%s'\n", tagName);
      rv = -1;
    }
    if (et->id != id) {
      printf("bug: name='%s' et->id=%d id=%d\n", et->name, et->id, id);
      rv = -1;
    }

    // Check that color lookup by name gets the right rgb value
    nscolor rgb;
    if (!NS_ColorNameToRGB(et->name, &rgb)) {
      printf("bug: name='%s' didn't NS_ColorNameToRGB\n", et->name);
      rv = -1;
    }
    if (nsColorNames::kColors[et->id] != rgb) {
      printf("bug: name='%s' ColorNameToRGB=%x kColors[%d]=%x\n",
             et->name, rgb, nsColorNames::kColors[et->id]);
      rv = -1;
    }

    // Check that parsing an RGB value in hex gets the right values
    PRUint8 r = NS_GET_R(rgb);
    PRUint8 g = NS_GET_G(rgb);
    PRUint8 b = NS_GET_B(rgb);
    char cbuf[50];
    PR_snprintf(cbuf, sizeof(cbuf), "#%02x%02x%02x", r, g, b);
    nscolor hexrgb;
    if (!NS_HexToRGB(cbuf, &hexrgb)) {
      printf("bug: hex conversion to color of '%s' failed\n", cbuf);
      rv = -1;
    }
    if (!NS_HexToRGB(cbuf + 1, &hexrgb)) {
      printf("bug: hex conversion to color of '%s' failed\n", cbuf);
      rv = -1;
    }
    if (hexrgb != rgb) {
      printf("bug: rgb=%x hexrgb=%x\n", rgb, hexrgb);
      rv = -1;
    }
    et++;
  }

  // Now make sure we don't find some garbage
  for (int i = 0; i < sizeof(kJunkNames) / sizeof(const char*); i++) {
    const char* tag = kJunkNames[i];
    id = nsColorNames::LookupName(tag);
    if (id >= 0) {
      printf("bug: found '%s'\n", tag ? tag : "(null)");
      rv = -1;
    }
  }

  return rv;
}
