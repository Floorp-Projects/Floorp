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
#include "plstr.h"
#include "nsHTMLTags.h"

static const char* kJunkNames[] = {
  nsnull,
  "",
  "123",
  "ab",
  "zzzzzz",
  "#@$&@#*@*$@$#"
};

int main(int argc, char** argv)
{
  PRInt32 id;

  // First make sure we can find all of the tags that are supposed to
  // be in the table. Futz with the case to make sure any case will
  // work
  const nsHTMLTags::NameTableEntry* et = &nsHTMLTags::kNameTable[0];
  const nsHTMLTags::NameTableEntry* end = &nsHTMLTags::kNameTable[TAG_MAX];
  while (et < end) {
    char tagName[100];
    id = nsHTMLTags::LookupName(et->name);
    if (id < 0) {
      printf("bug: can't find '%s'\n", et->name);
    }
    if (et->id != id) {
      printf("bug: name='%s' et->id=%d id=%d\n", et->name, et->id, id);
    }

    // fiddle with the case to make sure we can still find it
    PL_strcpy(tagName, et->name);
    tagName[0] = tagName[0] - 32;
    id = nsHTMLTags::LookupName(tagName);
    if (id < 0) {
      printf("bug: can't find '%s'\n", tagName);
    }
    if (et->id != id) {
      printf("bug: name='%s' et->id=%d id=%d\n", et->name, et->id, id);
    }
    et++;
  }

  // Now make sure we don't find some garbage
  for (int i = 0; i < sizeof(kJunkNames) / sizeof(const char*); i++) {
    const char* tag = kJunkNames[i];
    id = nsHTMLTags::LookupName(tag);
    if (id >= 0) {
      printf("bug: found '%s'\n", tag ? tag : "(null)");
    }
  }

  return 0;
}
