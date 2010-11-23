/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * a list of the recomputation that needs to be done in response to a
 * style change
 */

#include "nsStyleChangeList.h"
#include "nsStyleConsts.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsCRT.h"

static const PRUint32 kGrowArrayBy = 10;

nsStyleChangeList::nsStyleChangeList()
  : mArray(mBuffer),
    mArraySize(kStyleChangeBufferSize),
    mCount(0)
{
  MOZ_COUNT_CTOR(nsStyleChangeList);
}

nsStyleChangeList::~nsStyleChangeList()
{
  MOZ_COUNT_DTOR(nsStyleChangeList);
  Clear();
}

nsresult 
nsStyleChangeList::ChangeAt(PRInt32 aIndex, nsIFrame*& aFrame, nsIContent*& aContent, 
                            nsChangeHint& aHint) const
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
nsStyleChangeList::ChangeAt(PRInt32 aIndex, const nsStyleChangeData** aChangeData) const
{
  if ((0 <= aIndex) && (aIndex < mCount)) {
    *aChangeData = &mArray[aIndex];
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult 
nsStyleChangeList::AppendChange(nsIFrame* aFrame, nsIContent* aContent, nsChangeHint aHint)
{
  NS_ASSERTION(aFrame || (aHint & nsChangeHint_ReconstructFrame),
               "must have frame");
  NS_ASSERTION(aContent || !(aHint & nsChangeHint_ReconstructFrame),
               "must have content");
  // XXXbz we should make this take Element instead of nsIContent
  NS_ASSERTION(!aContent || aContent->IsElement(),
               "Shouldn't be trying to restyle non-elements directly");
  NS_ASSERTION(!(aHint & nsChangeHint_ReflowFrame) ||
               (aHint & nsChangeHint_NeedReflow),
               "Reflow hint bits set without actually asking for a reflow");

  if ((0 < mCount) && (aHint & nsChangeHint_ReconstructFrame)) { // filter out all other changes for same content
    if (aContent) {
      for (PRInt32 index = mCount - 1; index >= 0; --index) {
        if (aContent == mArray[index].mContent) { // remove this change
          aContent->Release();
          mCount--;
          if (index < mCount) { // move later changes down
            ::memmove(&mArray[index], &mArray[index + 1], 
                      (mCount - index) * sizeof(nsStyleChangeData));
          }
        }
      }
    }
  }

  PRInt32 last = mCount - 1;
  if ((0 < mCount) && aFrame && (aFrame == mArray[last].mFrame)) { // same as last frame
    NS_UpdateHint(mArray[last].mHint, aHint);
  }
  else {
    if (mCount == mArraySize) {
      PRInt32 newSize = mArraySize + kGrowArrayBy;
      nsStyleChangeData* newArray = new nsStyleChangeData[newSize];
      if (newArray) {
        memcpy(newArray, mArray, mCount * sizeof(nsStyleChangeData));
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
    if (aContent) {
      aContent->AddRef();
    }
    mArray[mCount].mHint = aHint;
    mCount++;
  }
  return NS_OK;
}

void
nsStyleChangeList::Clear()
{
  for (PRInt32 index = mCount - 1; index >= 0; --index) {
    nsIContent* content = mArray[index].mContent;
    if (content) {
      content->Release();
    }
  }
  if (mArray != mBuffer) {
    delete [] mArray;
    mArray = mBuffer;
    mArraySize = kStyleChangeBufferSize;
  }
  mCount = 0;
}

