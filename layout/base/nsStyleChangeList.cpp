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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsStyleChangeList.h"
#include "nsStyleConsts.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsCRT.h"

static const PRUint32 kGrowArrayBy = 10;

MOZ_DECL_CTOR_COUNTER(nsStyleChangeList)

nsStyleChangeList::nsStyleChangeList(void)
  : mArray(mBuffer),
    mArraySize(kStyleChangeBufferSize),
    mCount(0)
{
  MOZ_COUNT_CTOR(nsStyleChangeList);
}

nsStyleChangeList::~nsStyleChangeList(void)
{
  MOZ_COUNT_DTOR(nsStyleChangeList);
  Clear();
}

nsresult 
nsStyleChangeList::ChangeAt(PRInt32 aIndex, nsIFrame*& aFrame, nsIContent*& aContent, 
                            PRInt32& aHint) const
{
  if ((0 <= aIndex) && (aIndex < mCount)) {
    aFrame = mArray[aIndex].mFrame;
    aContent = mArray[aIndex].mContent;
    aHint = mArray[aIndex].mHint;
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult 
nsStyleChangeList::AppendChange(nsIFrame* aFrame, nsIContent* aContent, PRInt32 aHint)
{
  NS_ASSERTION(aFrame || (aHint >= NS_STYLE_HINT_FRAMECHANGE), "must have frame");
  NS_ASSERTION(aContent || (aHint < NS_STYLE_HINT_FRAMECHANGE), "must have content");

  if ((0 < mCount) && (NS_STYLE_HINT_FRAMECHANGE == aHint)) { // filter out all other changes for same content
    if (aContent) {
      PRInt32 index = mCount;
      while (0 < index--) {
        if (aContent == mArray[index].mContent) { // remove this change
          mCount--;
          if (index < mCount) { // move later changes down
            nsCRT::memcpy(&(mArray[index]), &(mArray[index + 1]), 
                          (mCount - index) * sizeof(nsStyleChangeData));
          }
        }
      }
    }
  }

  PRInt32 last = mCount - 1;
  if ((0 < mCount) && aFrame && (aFrame == mArray[last].mFrame)) { // same as last frame
    if (mArray[last].mHint < aHint) {
      mArray[last].mHint = aHint;
    }
  }
  else {
    if (mCount == mArraySize) {
      PRInt32 newSize = mArraySize + kGrowArrayBy;
      nsStyleChangeData* newArray = new nsStyleChangeData[newSize];
      if (newArray) {
        nsCRT::memcpy(newArray, mArray, mCount * sizeof(nsStyleChangeData));
        if (mArray != mBuffer) {
          delete [] mArray;
        }
        mArray = newArray;
        mArraySize = newSize;
      }
      else {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    mArray[mCount].mFrame = aFrame;
    mArray[mCount].mContent = aContent;
    mArray[mCount].mHint = aHint;
    mCount++;
  }
  return NS_OK;
}

void 
nsStyleChangeList::Clear() 
{
  if (mArray != mBuffer) {
    delete [] mArray;
    mArray = mBuffer;
    mArraySize = kStyleChangeBufferSize;
  }
  mCount = 0;
}

