/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  static nsColorName LookupName(const nsString& aName);
  static nsColorName LookupName(const nsCString& aName);

  static const nsCString& GetStringValue(nsColorName aColorName);

  // Color id to rgb value table
  static const nscolor kColors[];
};

#endif /* nsColorNames_h___ */
