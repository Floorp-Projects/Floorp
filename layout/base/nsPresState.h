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
#include "gfxPoint.h"
#include "nsAutoPtr.h"

class nsPresState
{
public:
  nsPresState()
    : mContentData(nullptr)
    , mScrollState(0, 0)
    , mResolution(1.0)
    , mScaleToResolution(false)
    , mDisabledSet(false)
    , mDisabled(false)
    , mDroppedDown(false)
  {}

  void SetScrollState(const nsPoint& aState)
  {
    mScrollState = aState;
  }

  nsPoint GetScrollPosition() const
  {
    return mScrollState;
  }

  void SetResolution(float aSize)
  {
    mResolution = aSize;
  }

  float GetResolution() const
  {
    return mResolution;
  }

  void SetScaleToResolution(bool aScaleToResolution)
  {
    mScaleToResolution = aScaleToResolution;
  }

  bool GetScaleToResolution() const
  {
    return mScaleToResolution;
  }

  void ClearNonScrollState()
  {
    mContentData = nullptr;
    mDisabledSet = false;
  }

  bool GetDisabled() const
  {
    return mDisabled;
  }

  void SetDisabled(bool aDisabled)
  {
    mDisabled = aDisabled;
    mDisabledSet = true;
  }

  bool IsDisabledSet() const
  {
    return mDisabledSet;
  }

  nsISupports* GetStateProperty() const
  {
    return mContentData;
  }

  void SetStateProperty(nsISupports *aProperty)
  {
    mContentData = aProperty;
  }

  void SetDroppedDown(bool aDroppedDown)
  {
    mDroppedDown = aDroppedDown;
  }

  bool GetDroppedDown() const
  {
    return mDroppedDown;
  }

// MEMBER VARIABLES
protected:
  nsCOMPtr<nsISupports> mContentData;
  nsPoint mScrollState;
  float mResolution;
  bool mScaleToResolution;
  bool mDisabledSet;
  bool mDisabled;
  bool mDroppedDown;
};

#endif /* nsPresState_h_ */
