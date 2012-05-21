/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* keywords used within CSS property values */

#include "nsCSSKeywords.h"
#include "nsString.h"
#include "nsStaticNameTable.h"
#include "nsReadableUtils.h"
#include "nsStyleConsts.h"

// required to make the symbol external, so that TestCSSPropertyLookup.cpp can link with it
extern const char* const kCSSRawKeywords[];

// define an array of all CSS keywords
#define CSS_KEY(_name,_id) #_name,
const char* const kCSSRawKeywords[] = {
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
      PRInt32 index = 0;
      for (; index < eCSSKeyword_COUNT && kCSSRawKeywords[index]; ++index) {
        nsCAutoString temp1(kCSSRawKeywords[index]);
        nsCAutoString temp2(kCSSRawKeywords[index]);
        ToLowerCase(temp1);
        NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
        NS_ASSERTION(-1 == temp1.FindChar('_'), "underscore char in table");
      }
      NS_ASSERTION(index == eCSSKeyword_COUNT, "kCSSRawKeywords and eCSSKeyword_COUNT are out of sync");
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
nsCSSKeywords::LookupKeyword(const nsACString& aKeyword)
{
  NS_ASSERTION(gKeywordTable, "no lookup table, needs addref");
  if (gKeywordTable) {
    return nsCSSKeyword(gKeywordTable->Lookup(aKeyword));
  }  
  return eCSSKeyword_UNKNOWN;
}

nsCSSKeyword 
nsCSSKeywords::LookupKeyword(const nsAString& aKeyword)
{
  NS_ASSERTION(gKeywordTable, "no lookup table, needs addref");
  if (gKeywordTable) {
    return nsCSSKeyword(gKeywordTable->Lookup(aKeyword));
  }  
  return eCSSKeyword_UNKNOWN;
}

const nsAFlatCString& 
nsCSSKeywords::GetStringValue(nsCSSKeyword aKeyword)
{
  NS_ASSERTION(gKeywordTable, "no lookup table, needs addref");
  NS_ASSERTION(0 <= aKeyword && aKeyword < eCSSKeyword_COUNT, "out of range");
  if (gKeywordTable) {
    return gKeywordTable->GetStringValue(PRInt32(aKeyword));
  } else {
    static nsDependentCString kNullStr("");
    return kNullStr;
  }
}

