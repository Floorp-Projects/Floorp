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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#include "nsColorNames.h"
#include "nsString.h"
#include "nsStaticNameTable.h"


// define an array of all color names
#define GFX_COLOR(_name, _value) #_name,
const char* kColorNames[] = {
#include "nsColorNameList.h"
};
#undef GFX_COLOR

// define an array of all color name values
#define GFX_COLOR(_name, _value) _value,
const nscolor nsColorNames::kColors[] = {
#include "nsColorNameList.h"
};
#undef GFX_COLOR

static PRInt32 gTableRefCount;
static nsStaticCaseInsensitiveNameTable* gColorTable;

void
nsColorNames::AddRefTable(void) 
{
  if (0 == gTableRefCount++) {
    NS_ASSERTION(!gColorTable, "pre existing array!");
    gColorTable = new nsStaticCaseInsensitiveNameTable();
    if (gColorTable) {
#ifdef DEBUG
    {
      // let's verify the table...
      for (PRInt32 index = 0; index < eColorName_COUNT; ++index) {
        nsCAutoString temp1(kColorNames[index]);
        nsCAutoString temp2(kColorNames[index]);
        temp1.ToLowerCase();
        NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
      }
    }
#endif      
      gColorTable->Init(kColorNames, eColorName_COUNT); 
    }
  }
}

void
nsColorNames::ReleaseTable(void) 
{
  if (0 == --gTableRefCount) {
    if (gColorTable) {
      delete gColorTable;
      gColorTable = nsnull;
    }
  }
}

nsColorName 
nsColorNames::LookupName(const nsACString& aColor)
{
  NS_ASSERTION(gColorTable, "no lookup table, needs addref");
  if (gColorTable) {
    return nsColorName(gColorTable->Lookup(aColor));
  }  
  return eColorName_UNKNOWN;
}

nsColorName 
nsColorNames::LookupName(const nsAString& aColor)
{
  NS_ASSERTION(gColorTable, "no lookup table, needs addref");
  if (gColorTable) {
    return nsColorName(gColorTable->Lookup(aColor));
  }  
  return eColorName_UNKNOWN;
}

const nsCString& 
nsColorNames::GetStringValue(nsColorName aColor)
{
  NS_ASSERTION(gColorTable, "no lookup table, needs addref");
  if (gColorTable) {
    return gColorTable->GetStringValue(PRInt32(aColor));
  } else {
    static nsCString kNullStr;
    return kNullStr;
  }
}

