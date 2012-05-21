/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Image.h"

namespace mozilla {
namespace image {

// Constructor
Image::Image(imgStatusTracker* aStatusTracker) :
  mInnerWindowId(0),
  mAnimationConsumers(0),
  mAnimationMode(kNormalAnimMode),
  mInitialized(false),
  mAnimating(false),
  mError(false)
{
  if (aStatusTracker) {
    mStatusTracker = aStatusTracker;
    mStatusTracker->SetImage(this);
  } else {
    mStatusTracker = new imgStatusTracker(this);
  }
}

PRUint32
Image::SizeOfData()
{
  if (mError)
    return 0;
  
  // This is not used by memory reporters, but for sizing the cache, which is
  // why it uses |moz_malloc_size_of| rather than an
  // |NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN|.
  return PRUint32(HeapSizeOfSourceWithComputedFallback(moz_malloc_size_of) +
                  HeapSizeOfDecodedWithComputedFallback(moz_malloc_size_of) +
                  NonHeapSizeOfDecoded() +
                  OutOfProcessSizeOfDecoded());
}

// Translates a mimetype into a concrete decoder
Image::eDecoderType
Image::GetDecoderType(const char *aMimeType)
{
  // By default we don't know
  eDecoderType rv = eDecoderType_unknown;

  // PNG
  if (!strcmp(aMimeType, "image/png"))
    rv = eDecoderType_png;
  else if (!strcmp(aMimeType, "image/x-png"))
    rv = eDecoderType_png;

  // GIF
  else if (!strcmp(aMimeType, "image/gif"))
    rv = eDecoderType_gif;


  // JPEG
  else if (!strcmp(aMimeType, "image/jpeg"))
    rv = eDecoderType_jpeg;
  else if (!strcmp(aMimeType, "image/pjpeg"))
    rv = eDecoderType_jpeg;
  else if (!strcmp(aMimeType, "image/jpg"))
    rv = eDecoderType_jpeg;

  // BMP
  else if (!strcmp(aMimeType, "image/bmp"))
    rv = eDecoderType_bmp;
  else if (!strcmp(aMimeType, "image/x-ms-bmp"))
    rv = eDecoderType_bmp;


  // ICO
  else if (!strcmp(aMimeType, "image/x-icon"))
    rv = eDecoderType_ico;
  else if (!strcmp(aMimeType, "image/vnd.microsoft.icon"))
    rv = eDecoderType_ico;

  // Icon
  else if (!strcmp(aMimeType, "image/icon"))
    rv = eDecoderType_icon;

  return rv;
}

void
Image::IncrementAnimationConsumers()
{
  mAnimationConsumers++;
  EvaluateAnimation();
}

void
Image::DecrementAnimationConsumers()
{
  NS_ABORT_IF_FALSE(mAnimationConsumers >= 1, "Invalid no. of animation consumers!");
  mAnimationConsumers--;
  EvaluateAnimation();
}

//******************************************************************************
/* attribute unsigned short animationMode; */
NS_IMETHODIMP
Image::GetAnimationMode(PRUint16* aAnimationMode)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aAnimationMode);
  
  *aAnimationMode = mAnimationMode;
  return NS_OK;
}

//******************************************************************************
/* attribute unsigned short animationMode; */
NS_IMETHODIMP
Image::SetAnimationMode(PRUint16 aAnimationMode)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ASSERTION(aAnimationMode == kNormalAnimMode ||
               aAnimationMode == kDontAnimMode ||
               aAnimationMode == kLoopOnceAnimMode,
               "Wrong Animation Mode is being set!");
  
  mAnimationMode = aAnimationMode;

  EvaluateAnimation();

  return NS_OK;
}

void
Image::EvaluateAnimation()
{
  if (!mAnimating && ShouldAnimate()) {
    nsresult rv = StartAnimation();
    mAnimating = NS_SUCCEEDED(rv);
  } else if (mAnimating && !ShouldAnimate()) {
    StopAnimation();
    mAnimating = false;
  }
}

} // namespace image
} // namespace mozilla
