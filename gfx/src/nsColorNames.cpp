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

#include "nsColorNames.h"
#include "nsString.h"
#include "nsAVLTree.h"


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

struct ColorNode {
  ColorNode(void)
    : mStr(),
      mEnum(eColorName_UNKNOWN)
  {}

  ColorNode(const nsStr& aStringValue, nsColorName aEnumValue)
    : mStr(),
      mEnum(aEnumValue)
  { // point to the incomming buffer
    // note that the incomming buffer may really be 2 byte
    nsStr::Initialize(mStr, aStringValue.mStr, aStringValue.mCapacity, 
                      aStringValue.mLength, aStringValue.mCharSize, PR_FALSE);
  }

  nsCAutoString mStr;
  nsColorName   mEnum;
};

class ColorComparitor: public nsAVLNodeComparitor {
public:
  virtual ~ColorComparitor(void) {}
  virtual PRInt32 operator()(void* anItem1,void* anItem2) {
    ColorNode* one = (ColorNode*)anItem1;
    ColorNode* two = (ColorNode*)anItem2;
    return one->mStr.Compare(two->mStr, PR_TRUE);
  }
}; 


static PRInt32      gTableRefCount;
static ColorNode*   gColorArray;
static nsAVLTree*   gColorTree;
static ColorComparitor* gComparitor;

void
nsColorNames::AddRefTable(void) 
{
  if (0 == gTableRefCount++) {
    if (! gColorArray) {
      gColorArray = new ColorNode[eColorName_COUNT];
      gComparitor = new ColorComparitor();
      if (gComparitor) {
        gColorTree = new nsAVLTree(*gComparitor, nsnull);
      }
      if (gColorArray && gColorTree) {
        PRInt32 index = -1;
        while (++index < PRInt32(eColorName_COUNT)) {
          gColorArray[index].mStr = kColorNames[index];
          gColorArray[index].mEnum = nsColorName(index);
          gColorTree->AddItem(&(gColorArray[index]));
        }
      }
    }
  }
}

void
nsColorNames::ReleaseTable(void) 
{
  if (0 == --gTableRefCount) {
    if (gColorArray) {
      delete [] gColorArray;
      gColorArray = nsnull;
    }
    if (gColorTree) {
      delete gColorTree;
      gColorTree = nsnull;
    }
    if (gComparitor) {
      delete gComparitor;
      gComparitor = nsnull;
    }
  }
}


nsColorName 
nsColorNames::LookupName(const nsStr& aColorName)
{
  NS_ASSERTION(gColorTree, "no lookup table, needs addref");
  if (gColorTree) {
    ColorNode node(aColorName, eColorName_UNKNOWN);
    ColorNode*  found = (ColorNode*)gColorTree->FindItem(&node);
    if (found) {
      NS_ASSERTION(found->mStr.EqualsIgnoreCase(aColorName), "bad tree");
      return found->mEnum;
    }
  }
  return eColorName_UNKNOWN;
}


const nsCString& 
nsColorNames::GetStringValue(nsColorName aColorName)
{
  nsCAutoString* value = nsnull;

  NS_ASSERTION(gColorArray, "no lookup table, needs addref");
  if ((eColorName_UNKNOWN < aColorName) && 
      (aColorName < eColorName_COUNT) && gColorArray) {
    return gColorArray[aColorName].mStr;
  }
  else {
    static const nsCString  kNullStr;
    return kNullStr;
  }
}

