/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a piece of state that is stored in session history when the document
 * is not
 */

#ifndef nsPresState_h_
#define nsPresState_h_

#include "nsPoint.h"
#include "nsAutoPtr.h"

class nsPresState
{
public:
  nsPresState()
    : mContentData(nullptr)
    , mScrollState(0, 0)
    , mDisabledSet(false)
    , mDisabled(false)
  {}

  void SetScrollState(const nsPoint& aState)
  {
    mScrollState = aState;
  }

  nsPoint GetScrollState()
  {
    return mScrollState;
  }

  void ClearNonScrollState()
  {
    mContentData = nullptr;
    mDisabledSet = false;
  }

  bool GetDisabled()
  {
    return mDisabled;
  }

  void SetDisabled(bool aDisabled)
  {
    mDisabled = aDisabled;
    mDisabledSet = true;
  }

  bool IsDisabledSet()
  {
    return mDisabledSet;
  }

  nsISupports* GetStateProperty()
  {
    return mContentData;
  }

  void SetStateProperty(nsISupports *aProperty)
  {
    mContentData = aProperty;
  }

// MEMBER VARIABLES
protected:
  nsCOMPtr<nsISupports> mContentData;
  nsPoint mScrollState;
  bool mDisabledSet;
  bool mDisabled;
};

#endif /* nsPresState_h_ */
