/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCSSKeywords.h"
#include "nsString.h"
#include "nsStaticNameTable.h"

// define an array of all CSS keywords
#define CSS_KEY(_name,_id) #_name,
const char* kCSSRawKeywords[] = {
#include "nsCSSKeywordList.h"
};
#undef CSS_KEY

static PRInt32 gTableRefCount;
static nsStaticCaseInsensitiveNameTable* gKeywordTable;

void
nsCSSKeywords::AddRefTable(void) 
{
  if (0 == gTableRefCount++) {
    NS_ASSERTION(!gKeywordTable, "pre existing array!");
    gKeywordTable = new nsStaticCaseInsensitiveNameTable();
    if (gKeywordTable) {
#ifdef DEBUG
    {
      // let's verify the table...
      for (PRInt32 index = 0; index < eCSSKeyword_COUNT; ++index) {
        nsCAutoString temp1(kCSSRawKeywords[index]);
        nsCAutoString temp2(kCSSRawKeywords[index]);
        temp1.ToLowerCase();
        NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
        NS_ASSERTION(-1 == temp1.FindChar('_'), "underscore char in table");
      }
    }
#endif      
      gKeywordTable->Init(kCSSRawKeywords, eCSSKeyword_COUNT); 
    }
  }
}

void
nsCSSKeywords::ReleaseTable(void) 
{
  if (0 == --gTableRefCount) {
    if (gKeywordTable) {
      delete gKeywordTable;
      gKeywordTable = nsnull;
    }
  }
}

nsCSSKeyword 
nsCSSKeywords::LookupKeyword(const nsCString& aKeyword)
{
  NS_ASSERTION(gKeywordTable, "no lookup table, needs addref");
  if (gKeywordTable) {
    return nsCSSKeyword(gKeywordTable->Lookup(aKeyword));
  }  
  return eCSSKeyword_UNKNOWN;
}

nsCSSKeyword 
nsCSSKeywords::LookupKeyword(const nsString& aKeyword)
{
  NS_ASSERTION(gKeywordTable, "no lookup table, needs addref");
  if (gKeywordTable) {
    return nsCSSKeyword(gKeywordTable->Lookup(aKeyword));
  }  
  return eCSSKeyword_UNKNOWN;
}

const nsCString& 
nsCSSKeywords::GetStringValue(nsCSSKeyword aKeyword)
{
  NS_ASSERTION(gKeywordTable, "no lookup table, needs addref");
  if (gKeywordTable) {
    return gKeywordTable->GetStringValue(PRInt32(aKeyword));
  } else {
    static nsCString kNullStr;
    return kNullStr;
  }
}

