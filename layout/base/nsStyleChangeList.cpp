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

#include "nsStyleChangeList.h"
#include "nsCRT.h"

static const PRUint32 kGrowArrayBy = 10;

nsStyleChangeList::nsStyleChangeList(void)
  : mArray(mBuffer),
    mArraySize(kStyleChangeBufferSize),
    mCount(0)
{
}

nsStyleChangeList::~nsStyleChangeList(void)
{
  Clear();
}

nsresult 
nsStyleChangeList::ChangeAt(PRInt32 aIndex, nsIFrame*& aFrame, PRInt32& aHint) const
{
  if ((0 <= aIndex) && (aIndex < mCount)) {
    aFrame = mArray[aIndex].mFrame;
    aHint = mArray[aIndex].mHint;
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult 
nsStyleChangeList::AppendChange(nsIFrame* aFrame, PRInt32 aHint)
{
  NS_ASSERTION(aFrame, "must have frame");
  PRInt32 last = mCount - 1;
  if ((0 < mCount) && (aFrame == mArray[last].mFrame)) { // same as last frame
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

