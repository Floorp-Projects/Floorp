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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 */

#include "nsCOMPtr.h"
#include "nsOutlinerSelection.h"
#include "nsIOutlinerBoxObject.h"

// A helper class for managing our ranges of selection.
struct nsOutlinerRange
{
  PRInt32 mMin;
  PRInt32 mMax;

  nsOutlinerRange* mNext;
  nsOutlinerRange* mPrev;

  nsOutlinerRange(PRInt32 aSingleVal):mNext(nsnull), mMin(aSingleVal), mMax(aSingleVal) {};
  nsOutlinerRange(PRInt32 aMin, PRInt32 aMax) :mNext(nsnull), mMin(aMin), mMax(aMax) {};

  ~nsOutlinerRange() { delete mNext; };

  void Connect(nsOutlinerRange* aPrev = nsnull, nsOutlinerRange* aNext = nsnull) {
    mPrev = aPrev;
    mNext = aNext;
  };

  PRBool Contains(PRInt32 aIndex) {
    if (aIndex >= mMin && aIndex <= mMax)
      return PR_TRUE;

    if (mNext)
      return mNext->Contains(aIndex);

    return PR_FALSE;
  };
};

nsOutlinerSelection::nsOutlinerSelection(nsIOutlinerBoxObject* aOutliner)
{
  NS_INIT_ISUPPORTS();
  mOutliner = aOutliner;
  mSuppressed = PR_FALSE;
  mFirstRange = nsnull;
}

nsOutlinerSelection::~nsOutlinerSelection()
{
}

NS_IMPL_ISUPPORTS1(nsOutlinerSelection, nsIOutlinerSelection)

NS_IMETHODIMP nsOutlinerSelection::GetOutliner(nsIOutlinerBoxObject * *aOutliner)
{
  NS_IF_ADDREF(mOutliner);
  *aOutliner = mOutliner;
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerSelection::SetOutliner(nsIOutlinerBoxObject * aOutliner)
{
  mOutliner = aOutliner; // WEAK
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerSelection::IsSelected(PRInt32 aIndex, PRBool* aResult)
{
  if (mFirstRange)
    *aResult = mFirstRange->Contains(aIndex);
  else
    *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerSelection::Select(PRInt32 index)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsOutlinerSelection::ToggleSelect(PRInt32 index)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsOutlinerSelection::RangedSelect(PRInt32 startIndex, PRInt32 endIndex)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsOutlinerSelection::ClearSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsOutlinerSelection::InvertSelection()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsOutlinerSelection::SelectAll()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsOutlinerSelection::GetRangeCount(PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsOutlinerSelection::GetRangeAt(PRInt32 i, PRInt32 *min, PRInt32 *max)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsOutlinerSelection::GetSelectEventsSuppressed(PRBool *aSelectEventsSuppressed)
{
  *aSelectEventsSuppressed = mSuppressed;
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerSelection::SetSelectEventsSuppressed(PRBool aSelectEventsSuppressed)
{
  mSuppressed = aSelectEventsSuppressed;
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerSelection::GetCurrentIndex(PRInt32 *aCurrentIndex)
{
  *aCurrentIndex = mCurrentIndex;
  return NS_OK;
}

NS_IMETHODIMP nsOutlinerSelection::SetCurrentIndex(PRInt32 aCurrentIndex)
{
  mCurrentIndex = aCurrentIndex;
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewOutlinerSelection(nsIOutlinerBoxObject* aOutliner, nsIOutlinerSelection** aResult)
{
  *aResult = new nsOutlinerSelection(aOutliner);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
