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

struct nsStr;
class nsCString;

/*
   Declare the enum list using the magic of preprocessing
   enum values are "eColorName_foo" (where foo is the color name)

   To change the list of colors, see nsColorNameList.h

 */
#define GFX_COLOR(_name, _value) eColorName_##_name,
enum nsColorName {
  eColorName_UNKNOWN = -1,
#include "nsColorNameList.h"
  eColorName_COUNT
};
#undef GFX_COLOR

class NS_GFX nsColorNames {
public:
  static void AddRefTable(void);
  static void ReleaseTable(void);

  // Given a color name, return the color enum value
  // This only functions provided a valid ref on the table
  static nsColorName LookupName(const nsStr& aName);

  static const nsCString& GetStringValue(nsColorName aColorName);

  // Color id to rgb value table
  static const nscolor kColors[];
};

#endif /* nsColorNames_h___ */
