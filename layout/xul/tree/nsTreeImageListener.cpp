/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTreeImageListener.h"
#include "nsITreeBoxObject.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "nsIContent.h"
#include "nsTreeColumns.h"

NS_IMPL_ISUPPORTS(nsTreeImageListener, imgINotificationObserver)

nsTreeImageListener::nsTreeImageListener(nsTreeBodyFrame* aTreeFrame)
  : mTreeFrame(aTreeFrame),
    mInvalidationSuppressed(true),
    mInvalidationArea(nullptr)
{
}

nsTreeImageListener::~nsTreeImageListener()
{
  delete mInvalidationArea;
}

NS_IMETHODIMP
nsTreeImageListener::Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData)
{
  if (aType == imgINotificationObserver::IS_ANIMATED) {
    return mTreeFrame ? mTreeFrame->OnImageIsAnimated(aRequest) : NS_OK;
  }

  if (aType == imgINotificationObserver::SIZE_AVAILABLE) {
    // Ensure the animation (if any) is started. Note: There is no
    // corresponding call to Decrement for this. This Increment will be
    // 'cleaned up' by the Request when it is destroyed, but only then.
    aRequest->IncrementAnimationConsumers();
  }

  if (aType == imgINotificationObserver::FRAME_UPDATE) {
    Invalidate();
  }

  return NS_OK;
}

void
nsTreeImageListener::AddCell(int32_t aIndex, nsTreeColumn* aCol)
{
  if (!mInvalidationArea) {
    mInvalidationArea = new InvalidationArea(aCol);
    mInvalidationArea->AddRow(aIndex);
  }
  else {
    InvalidationArea* currArea;
    for (currArea = mInvalidationArea; currArea; currArea = currArea->GetNext()) {
      if (currArea->GetCol() == aCol) {
        currArea->AddRow(aIndex);
        break;
      }
    }
    if (!currArea) {
      currArea = new InvalidationArea(aCol);
      currArea->SetNext(mInvalidationArea);
      mInvalidationArea = currArea;
      mInvalidationArea->AddRow(aIndex);
    }
  }
}


void
nsTreeImageListener::Invalidate()
{
  if (!mInvalidationSuppressed) {
    for (InvalidationArea* currArea = mInvalidationArea; currArea;
         currArea = currArea->GetNext()) {
      // Loop from min to max, invalidating each cell that was listening for this image.
      for (int32_t i = currArea->GetMin(); i <= currArea->GetMax(); ++i) {
        if (mTreeFrame) {
          nsITreeBoxObject* tree = mTreeFrame->GetTreeBoxObject();
          if (tree) {
            tree->InvalidateCell(i, currArea->GetCol());
          }
        }
      }
    }
  }
}

nsTreeImageListener::InvalidationArea::InvalidationArea(nsTreeColumn* aCol)
  : mCol(aCol),
    mMin(-1), // min should start out "undefined"
    mMax(0),
    mNext(nullptr)
{
}

void
nsTreeImageListener::InvalidationArea::AddRow(int32_t aIndex)
{
  if (mMin == -1)
    mMin = mMax = aIndex;
  else if (aIndex < mMin)
    mMin = aIndex;
  else if (aIndex > mMax)
    mMax = aIndex;
}

NS_IMETHODIMP
nsTreeImageListener::ClearFrame()
{
  mTreeFrame = nullptr;
  return NS_OK;
}
