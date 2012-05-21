/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Inline methods that belong in nsStyleStruct.h, except that they
 * require more headers.
 */

#ifndef nsStyleStructInlines_h_
#define nsStyleStructInlines_h_

#include "nsStyleStruct.h"
#include "imgIRequest.h"
#include "imgIContainer.h"

inline void
nsStyleBorder::SetBorderImage(imgIRequest* aImage)
{
  mBorderImageSource = aImage;
  mSubImages.Clear();
}

inline imgIRequest*
nsStyleBorder::GetBorderImage() const
{
  NS_ABORT_IF_FALSE(!mBorderImageSource || mImageTracked,
                    "Should be tracking any images we're going to use!");
  return mBorderImageSource;
}

inline bool nsStyleBorder::IsBorderImageLoaded() const
{
  PRUint32 status;
  return mBorderImageSource &&
         NS_SUCCEEDED(mBorderImageSource->GetImageStatus(&status)) &&
         (status & imgIRequest::STATUS_LOAD_COMPLETE) &&
         !(status & imgIRequest::STATUS_ERROR);
}

inline void
nsStyleBorder::SetSubImage(PRUint8 aIndex, imgIContainer* aSubImage) const
{
  const_cast<nsStyleBorder*>(this)->mSubImages.ReplaceObjectAt(aSubImage, aIndex);
}

inline imgIContainer*
nsStyleBorder::GetSubImage(PRUint8 aIndex) const
{
  imgIContainer* subImage = nsnull;
  if (aIndex < mSubImages.Count())
    subImage = mSubImages[aIndex];
  return subImage;
}

#endif /* !defined(nsStyleStructInlines_h_) */
