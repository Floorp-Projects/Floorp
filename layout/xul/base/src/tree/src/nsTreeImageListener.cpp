/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTreeImageListener.h"
#include "nsITreeBoxObject.h"
#include "imgIRequest.h"
#include "imgIContainer.h"

NS_IMPL_ISUPPORTS2(nsTreeImageListener, imgIDecoderObserver, imgIContainerObserver)

nsTreeImageListener::nsTreeImageListener(nsTreeBodyFrame* aTreeFrame)
  : mTreeFrame(aTreeFrame),
    mInvalidationSuppressed(true),
    mInvalidationArea(nsnull)
{
}

nsTreeImageListener::~nsTreeImageListener()
{
  delete mInvalidationArea;
}

NS_IMETHODIMP
nsTreeImageListener::OnImageIsAnimated(imgIRequest *aRequest)
{
  if (!mTreeFrame) {
    return NS_OK;
  }

  return mTreeFrame->OnImageIsAnimated(aRequest);
}

NS_IMETHODIMP nsTreeImageListener::OnStartContainer(imgIRequest *aRequest,
                                                    imgIContainer *aImage)
{
  // Ensure the animation (if any) is started. Note: There is no
  // corresponding call to Decrement for this. This Increment will be
  // 'cleaned up' by the Request when it is destroyed, but only then.
  aRequest->IncrementAnimationConsumers();
  return NS_OK;
}

NS_IMETHODIMP nsTreeImageListener::OnDataAvailable(imgIRequest *aRequest,
                                                   bool aCurrentFrame,
                                                   const nsIntRect *aRect)
{
  Invalidate();
  return NS_OK;
}

NS_IMETHODIMP nsTreeImageListener::FrameChanged(imgIRequest *aRequest,
                                                imgIContainer *aContainer,
                                                const nsIntRect *aDirtyRect)
{
  Invalidate();
  return NS_OK;
}


void
nsTreeImageListener::AddCell(PRInt32 aIndex, nsITreeColumn* aCol)
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
      for (PRInt32 i = currArea->GetMin(); i <= currArea->GetMax(); ++i) {
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

nsTreeImageListener::InvalidationArea::InvalidationArea(nsITreeColumn* aCol)
  : mCol(aCol),
    mMin(-1), // min should start out "undefined"
    mMax(0),
    mNext(nsnull)
{
}

void
nsTreeImageListener::InvalidationArea::AddRow(PRInt32 aIndex)
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
  mTreeFrame = nsnull;
  return NS_OK;
}
