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

#ifndef nsColorNames_h___
#define nsColorNames_h___

#include "nsColor.h"
#include "nsColorNameIDs.h"

class NS_GFX nsColorNames {
public:
  // Given a null terminated string of 7 bit characters, return the
  // tag id (see nsHTMLTagIDs.h) for the tag or -1 if the tag is not
  // known. The lookup function uses a perfect hash.
  static PRInt32 LookupName(const char* str);

  // Color id to rgb value table
  static nscolor kColors[];

  struct NameTableEntry {
    const char* name;
    PRInt32 id;
  };

   // A table whose index is the id (from LookupName)
  static const NameTableEntry kNameTable[];
};

#endif /* nsColorNames_h___ */
