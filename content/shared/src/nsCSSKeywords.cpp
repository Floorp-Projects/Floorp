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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCSSKeywords.h"
#include "nsString.h"
#include "nsAVLTree.h"


// define an array of all CSS keywords
#define CSS_KEY(_key) #_key,
const char* kCSSRawKeywords[] = {
#include "nsCSSKeywordList.h"
};
#undef CSS_KEY

struct KeywordNode {
  KeywordNode(void)
    : mStr(),
      mEnum(eCSSKeyword_UNKNOWN)
  {}

  KeywordNode(const nsStr& aStringValue, nsCSSKeyword aEnumValue)
    : mStr(),
      mEnum(aEnumValue)
  { // point to the incomming buffer
    // note that the incomming buffer may really be 2 byte
    nsStr::Initialize(mStr, aStringValue.mStr, aStringValue.mCapacity, 
                      aStringValue.mLength, aStringValue.mCharSize, PR_FALSE);
  }

  nsCAutoString mStr;
  nsCSSKeyword  mEnum;
};

class KeywordComparitor: public nsAVLNodeComparitor {
public:
  virtual ~KeywordComparitor(void) {}
  virtual PRInt32 operator()(void* anItem1,void* anItem2) {
    KeywordNode* one = (KeywordNode*)anItem1;
    KeywordNode* two = (KeywordNode*)anItem2;
    return one->mStr.Compare(two->mStr, PR_TRUE);
  }
}; 


static PRInt32      gTableRefCount;
static KeywordNode* gKeywordArray;
static nsAVLTree*   gKeywordTree;
static KeywordComparitor* gComparitor;

void
nsCSSKeywords::AddRefTable(void) 
{
  if (0 == gTableRefCount++) {
    if (! gKeywordArray) {
      gKeywordArray = new KeywordNode[eCSSKeyword_COUNT];
      gComparitor = new KeywordComparitor();
      if (gComparitor) {
        gKeywordTree = new nsAVLTree(*gComparitor, nsnull);
      }
      if (gKeywordArray && gKeywordTree) {
        PRInt32 index = -1;
        while (++index < PRInt32(eCSSKeyword_COUNT)) {
          gKeywordArray[index].mStr = kCSSRawKeywords[index];
          gKeywordArray[index].mStr.ReplaceChar('_', '-');
          gKeywordArray[index].mEnum = nsCSSKeyword(index);
          gKeywordTree->AddItem(&(gKeywordArray[index]));
        }
      }
    }
  }
}

void
nsCSSKeywords::ReleaseTable(void) 
{
  if (0 == --gTableRefCount) {
    if (gKeywordArray) {
      delete [] gKeywordArray;
      gKeywordArray = nsnull;
    }
    if (gKeywordTree) {
      delete gKeywordTree;
      gKeywordTree = nsnull;
    }
    if (gComparitor) {
      delete gComparitor;
      gComparitor = nsnull;
    }
  }
}


nsCSSKeyword 
nsCSSKeywords::LookupKeyword(const nsStr& aKeyword)
{
  NS_ASSERTION(gKeywordTree, "no lookup table, needs addref");
  if (gKeywordTree) {
    KeywordNode node(aKeyword, eCSSKeyword_UNKNOWN);
    KeywordNode*  found = (KeywordNode*)gKeywordTree->FindItem(&node);
    if (found) {
      NS_ASSERTION(found->mStr.EqualsIgnoreCase(aKeyword), "bad tree");
      return found->mEnum;
    }
  }
  return eCSSKeyword_UNKNOWN;
}


const nsCString& 
nsCSSKeywords::GetStringValue(nsCSSKeyword aKeyword)
{
  NS_ASSERTION(gKeywordArray, "no lookup table, needs addref");
  if ((eCSSKeyword_UNKNOWN < aKeyword) && 
      (aKeyword < eCSSKeyword_COUNT) && gKeywordArray) {
    return gKeywordArray[aKeyword].mStr;
  }
  else {
    static const nsCString kNullStr;
    return kNullStr;
  }
}

