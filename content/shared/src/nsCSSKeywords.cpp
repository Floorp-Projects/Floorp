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
static nsCString* kNullStr;

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
    kNullStr = new nsCString();
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
    delete kNullStr;
  }
}


nsCSSKeyword 
nsCSSKeywords::LookupKeyword(const nsCString& aKeyword)
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

nsCSSKeyword 
nsCSSKeywords::LookupKeyword(const nsString& aKeyword) {
  nsCAutoString theKeyword; theKeyword.AssignWithConversion(aKeyword);
  return LookupKeyword(theKeyword);
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
    return *kNullStr;
  }
}

