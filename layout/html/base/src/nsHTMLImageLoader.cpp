/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsHTMLImageLoader.h"
#include "nsIStyleContext.h"
#include "nsFrame.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

#ifdef DEBUG
#undef NOISY_IMAGE_LOADING
#else
#undef NOISY_IMAGE_LOADING
#endif

MOZ_DECL_CTOR_COUNTER(nsHTMLImageLoader)

nsHTMLImageLoader::nsHTMLImageLoader()
  : mBaseURL(nsnull),
    mFrame(nsnull),
    mCallBack(nsnull),
    mClosure(nsnull),
    mImageLoader(nsnull),
    mAllFlags(0),
    mIntrinsicImageSize(0, 0),
    mComputedImageSize(0, 0)
{
  MOZ_COUNT_CTOR(nsHTMLImageLoader);
}

nsHTMLImageLoader::~nsHTMLImageLoader()
{
  MOZ_COUNT_DTOR(nsHTMLImageLoader);
  NS_IF_RELEASE(mBaseURL);
  NS_IF_RELEASE(mImageLoader);
}

void
nsHTMLImageLoader::Init(nsIFrame* aFrame,
                        nsHTMLImageLoaderCB aCallBack,
                        void* aClosure,
                        nsIURI* aBaseURL,
                        const nsString& aURLSpec)
{
  mFrame = aFrame;
  mCallBack = aCallBack;
  mClosure = aClosure;
  mBaseURL = aBaseURL;
  NS_IF_ADDREF(mBaseURL);
  SetURL(aURLSpec);
}

nsIImage*
nsHTMLImageLoader::GetImage()
{
  nsIImage* image = nsnull;
  if (mImageLoader) {
    mImageLoader->GetImage(&image);
  }
  return image;
}

void
nsHTMLImageLoader::GetNaturalImageSize(PRUint32* naturalWidth, PRUint32* naturalHeight)
{ 
  *naturalWidth = 0;
  *naturalHeight = 0;
  if (mImageLoader) {
    mImageLoader->GetNaturalImageSize(naturalWidth, naturalHeight);
  }
  
}

void
nsHTMLImageLoader::SetURL(const nsString& aNewSpec)
{
  mURLSpec = aNewSpec;
  mURLSpec.Trim(" \t\n\r");
  if (mBaseURL && !aNewSpec.IsEmpty()) {
    nsString empty;
    nsresult rv;
    rv = NS_MakeAbsoluteURI(mURL, mURLSpec, mBaseURL);
    if (NS_FAILED(rv)) {
      mURL = mURLSpec;
    }
  } else {
    mURL = mURLSpec;
  }
}

void
nsHTMLImageLoader::StopLoadImage(nsIPresContext* aPresContext)
{
  if (mImageLoader) {
    aPresContext->StopLoadImage(mFrame, mImageLoader);
    NS_RELEASE(mImageLoader);
  }
}

void
nsHTMLImageLoader::StopAllLoadImages(nsIPresContext* aPresContext)
{
  aPresContext->StopAllLoadImagesFor(mFrame, mFrame);
}

nsresult
nsHTMLImageLoader::ImageLoadCB(nsIPresContext* aPresContext,
                               nsIFrameImageLoader* aLoader,
                               nsIFrame* aFrame,
                               void* aClosure,
                               PRUint32 aStatus)
{
  if (aClosure) {
    ((nsHTMLImageLoader*)aClosure)->Update(aPresContext, aFrame, aStatus);
  }
  return NS_OK;
}

void
nsHTMLImageLoader::Update(nsIPresContext* aPresContext,
                          nsIFrame* aFrame,
                          PRUint32 aStatus)
{
#ifdef NOISY_IMAGE_LOADING
  nsFrame::ListTag(stdout, aFrame);
  printf(": update: status=%x [loader=%p] callBack=%p squelch=%s\n",
         aStatus, mImageLoader, mCallBack,
         mFlags.mSquelchCallback ? "yes" : "no");
#endif

  if (!mFlags.mNeedSizeNotification &&
      NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    // remove the size available bit since it is not needed
    aStatus &= ~NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE;
    // check if any status left
    if (aStatus == NS_IMAGE_LOAD_STATUS_NONE)
      return;
  }

  if (NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    if (mImageLoader) {
      mImageLoader->GetSize(mIntrinsicImageSize);
      if (mFlags.mNeedIntrinsicImageSize) {
        mFlags.mHaveIntrinsicImageSize = PR_TRUE;
      }
    }
  }

  // Pass on update to the user of this object if they want it
  if (mCallBack) {
    // We squelch the status size callback in the case where the client doesn't
    // want the size returned. However, don't squelch a status that says the image
    // failed to load
    if ((NS_IMAGE_LOAD_STATUS_ERROR & aStatus) || !mFlags.mSquelchCallback) {
      (*mCallBack)(aPresContext, this, aFrame, mClosure, aStatus);
    }
  }
}

// The prefix for special "internal" images that are well known
#define GOPHER_SPEC "internal-gopher-"

// Note: sizeof a string constant includes the \0 at the end so
// subtract one
#define GOPHER_SPEC_SIZE (sizeof(GOPHER_SPEC) - 1)

nsresult
nsHTMLImageLoader::StartLoadImage(nsIPresContext* aPresContext)
{
  if (!mFrame) {
    // We were not initialized!
    return NS_ERROR_NULL_POINTER;
  }
  if (mURL.IsEmpty()) {
    return NS_OK;
  }

  // Note: navigator 4.* and earlier releases ignored the base tags
  // effect on the builtin images. So we do too. Use mURLSpec instead
  // of the absolute url...
  nsAutoString internalImageURLSpec;
  nsString* urlSpec = &mURL;
  if (mURLSpec.CompareWithConversion(GOPHER_SPEC, PR_FALSE, GOPHER_SPEC_SIZE) == 0) {
    // We found a special image source value that refers to a
    // builtin image. Rewrite the source url as a resource url.
    urlSpec = &internalImageURLSpec;
    mURLSpec.Mid(internalImageURLSpec, GOPHER_SPEC_SIZE,
                 mURLSpec.Length() - GOPHER_SPEC_SIZE);
    internalImageURLSpec.InsertWithConversion("resource:/res/html/gopher-", 0);
    internalImageURLSpec.AppendWithConversion(".gif");
  }

  // This is kind of sick, but its possible that we will get a
  // notification *before* we have setup mImageLoader. To get around
  // this, we let the pres-context store into mImageLoader and sort
  // things after it returns.
  nsIFrameImageLoader* oldLoader = mImageLoader;
  nsSize* sizeToLoadWidth = nsnull;
  if (!mFlags.mAutoImageSize && !mFlags.mNeedIntrinsicImageSize) {
    sizeToLoadWidth = &mComputedImageSize;
  }
  nsresult rv = aPresContext->StartLoadImage(*urlSpec, nsnull,
                                             sizeToLoadWidth,
                                             mFrame, ImageLoadCB, (void*)this,
                                             mFrame, &mImageLoader);
#ifdef NOISY_IMAGE_LOADING
  nsFrame::ListTag(stdout, mFrame);
  printf(": loading image '");
  fputs(mURL, stdout);
  printf("' @ ");
  if (mFlags.mNeedIntrinsicImageSize) {
    printf("intrinsic size ");
  }
  printf("%d,%d; oldLoader=%p newLoader=%p",
         mComputedImageSize.width, mComputedImageSize.height,
         oldLoader, mImageLoader);
  if (sizeToLoadWidth) {
    printf(" sizeToLoadWidth=%d,%d",
           sizeToLoadWidth->width, sizeToLoadWidth->height);
  }
  else {
    printf(" autoImageSize=%s needIntrinsicImageSize=%s",
           mFlags.mAutoImageSize ? "yes" : "no",
           mFlags.mNeedIntrinsicImageSize ? "yes" : "no");
  }
  printf("\n");
#endif

  if (oldLoader != mImageLoader) {
    if (nsnull != oldLoader) {
      // Tell presentation context we are done with the old image loader
      aPresContext->StopLoadImage(mFrame, oldLoader);
    }
  }

  // Release the old image loader
  NS_IF_RELEASE(oldLoader);

  return rv;
}

void
nsHTMLImageLoader::UpdateURLSpec(nsIPresContext* aPresContext,
                                 const nsString& aNewSpec)
{
  SetURL(aNewSpec);

  // Start image loading with the previously computed size information
  StartLoadImage(aPresContext);
}

#define MINMAX(_value,_min,_max) \
    ((_value) < (_min)           \
     ? (_min)                    \
     : ((_value) > (_max)        \
        ? (_max)                 \
        : (_value)))

PRBool
nsHTMLImageLoader::GetDesiredSize(nsIPresContext* aPresContext,
                                  const nsHTMLReflowState* aReflowState,
                                  nsHTMLReflowMetrics& aDesiredSize)
{
  nscoord widthConstraint = NS_INTRINSICSIZE;
  nscoord heightConstraint = NS_INTRINSICSIZE;
  PRBool fixedContentWidth = PR_FALSE;
  PRBool fixedContentHeight = PR_FALSE;

  if (mFlags.mInRecalcMode)
  { // only true after an unconstrained reflow of a %-width image recieved after we've fetched our intrinsic width
    // really!  This case is for bug 39901 and 38396.  The point is, we've already
    // calculated everything we need and we want to just return it right away.
    // This is somewhat fragile, since it makes an assumption about the way reflow works.
    mFlags.mInRecalcMode=0; // clear the mode
    // block code will lie to us, so use our already-computed values
    aDesiredSize.width = mComputedImageSize.width;
    aDesiredSize.height = mComputedImageSize.height;
    return PR_TRUE;
  }

  nscoord minWidth, maxWidth, minHeight, maxHeight;

  if (aReflowState) {
    // Determine whether the image has fixed content width
    widthConstraint = aReflowState->mComputedWidth;
    minWidth = aReflowState->mComputedMinWidth;
    maxWidth = aReflowState->mComputedMaxWidth;
    if (NS_INTRINSICSIZE != widthConstraint) {
      fixedContentWidth = PR_TRUE;
    }
    else if (mFlags.mHaveIntrinsicImageSize) {
      // At this point we know that the width value was not
      // constrained and that we now know the intrinsic size of the
      // image.
      //
      // The css2 spec states that if a min/max width value is
      // provided then it acts as a substitute value for the "width"
      // property, if it applies. Therefore, we will force the
      // fixedContentWidth flag to true in these cases.
      if ((0 != minWidth) ||
          (NS_UNCONSTRAINEDSIZE != maxWidth))
      {
        // Use the intrinsic image width for the min-max comparisons,
        // since the width property is "auto".
        widthConstraint = mIntrinsicImageSize.width;
        fixedContentWidth = PR_TRUE;
      }
      // Check the special case where we have the intrinsic width 
      // (already tested to get into this block of code)
      // and it's an unconstrained reflow, and we are %-width
      // This special case is for bug 39901 and 38396
      else if (aReflowState && 
              (NS_UNCONSTRAINEDSIZE == aReflowState->availableWidth) &&
              (eStyleUnit_Percent==aReflowState->mStylePosition->mWidth.GetUnit())) 
      {
        // Use the intrinsic image width 
        // since the width property is percent (uncalcuable at this time).
        widthConstraint = mIntrinsicImageSize.width;
        fixedContentWidth = PR_TRUE;
        mFlags.mInRecalcMode = PR_TRUE; // we set up this state for bug 39901 and 38396
      }
    }

    // Determine whether the image has fixed content height
    heightConstraint = aReflowState->mComputedHeight;
    minHeight = aReflowState->mComputedMinHeight;
    maxHeight = aReflowState->mComputedMaxHeight;
    if (NS_UNCONSTRAINEDSIZE != heightConstraint) {
      fixedContentHeight = PR_TRUE;
    }
    else if (mFlags.mHaveIntrinsicImageSize) {
      // At this point we know that the height value was not
      // constrained and that we now know the intrinsic size of the
      // image.
      //
      // The css2 spec states that if a min/max height value is
      // provided then it acts as a substitute value for the "height"
      // property, if it applies. Therefore, we will force the
      // fixedContentHeight flag to true in these cases.
      if ((0 != minHeight) ||
          (NS_UNCONSTRAINEDSIZE != maxHeight)) {
        // Use the intrinsic image height for the min-max comparisons,
        // since the height property is "auto".
        heightConstraint = mIntrinsicImageSize.height;
        fixedContentHeight = PR_TRUE;
      }
    }
  }
  else {
    minWidth = minHeight = 0;
    maxWidth = maxHeight = NS_UNCONSTRAINEDSIZE;
  }

  float p2t, t2p;
  aPresContext->GetPixelsToTwips(&p2t);
  aPresContext->GetTwipsToPixels(&t2p);

  for (;;) {
    PRBool haveComputedSize = PR_FALSE;
    PRBool needIntrinsicImageSize = PR_FALSE;

    nscoord newWidth, newHeight;
    mFlags.mAutoImageSize = PR_FALSE;
    mFlags.mNeedSizeNotification = PR_FALSE;
    if (fixedContentWidth) {
      if (fixedContentHeight) {
        newWidth = MINMAX(widthConstraint, minWidth, maxWidth);
        newHeight = MINMAX(heightConstraint, minHeight, maxHeight);
        haveComputedSize = PR_TRUE;
      }
      else {
        // We have a width, and an auto height. Compute height from
        // width once we have the intrinsic image size.
        newWidth = MINMAX(widthConstraint, minWidth, maxWidth);
        if (mFlags.mHaveIntrinsicImageSize) {
          float width = mIntrinsicImageSize.width
            ? (float) mIntrinsicImageSize.width
            : (float) mIntrinsicImageSize.height;     // avoid divide by zero
          float height = (float) mIntrinsicImageSize.height;

          // snap the width to the nearest pixel value to prevent a
          // feedback loop.
          PRInt32 pixelWidth = NSTwipsToIntPixels(newWidth + nscoord(p2t / 2), t2p);
          newWidth = NSIntPixelsToTwips(pixelWidth, p2t);

          newHeight = (nscoord)
            NSToIntRound(newWidth * height / width);
          newHeight = MINMAX(newHeight, minHeight, maxHeight);
          haveComputedSize = PR_TRUE;
        }
        else {
          newHeight = 1;
          needIntrinsicImageSize = PR_TRUE;
          mFlags.mNeedSizeNotification = PR_TRUE;
        }
      }
    }
    else if (fixedContentHeight) {
      // We have a height, and an auto width. Compute width from height
      // once we have the intrinsic image size.

      // otherwise, we really have auto-width based on height
      newHeight = MINMAX(heightConstraint, minHeight, maxHeight);
      if (mFlags.mHaveIntrinsicImageSize) {
        float width = (float) mIntrinsicImageSize.width;
        float height = mIntrinsicImageSize.height
          ? (float) mIntrinsicImageSize.height
          : (float) mIntrinsicImageSize.width;         // avoid divide by zero

        // snap the height to the nearest pixel value to prevent a
        // feedback loop.
        PRInt32 pixelHeight = NSTwipsToIntPixels(newHeight + nscoord(p2t / 2), t2p);
        newHeight = NSIntPixelsToTwips(pixelHeight, p2t);

        newWidth = (nscoord)
          NSToIntRound(newHeight * width / height);
        newWidth = MINMAX(newWidth, minWidth, maxWidth);
        haveComputedSize = PR_TRUE;
      }
      else {
        newWidth = 1;
        needIntrinsicImageSize = PR_TRUE;
        mFlags.mNeedSizeNotification = PR_TRUE;
      }
    }
    else {
      mFlags.mAutoImageSize = PR_TRUE;
      if (mFlags.mHaveIntrinsicImageSize) {
        newWidth = MINMAX(mIntrinsicImageSize.width, minWidth, maxWidth);
        newHeight = MINMAX(mIntrinsicImageSize.height, minHeight, maxHeight);
        haveComputedSize = PR_TRUE;
      }
      else {
        newWidth = 1;
        newHeight = 1;
        needIntrinsicImageSize = PR_TRUE;
        mFlags.mNeedSizeNotification = PR_TRUE;
      }
    }

    mFlags.mNeedIntrinsicImageSize = needIntrinsicImageSize;
    mFlags.mHaveComputedSize = haveComputedSize;
    mComputedImageSize.width = newWidth;
    mComputedImageSize.height = newHeight;
#ifdef NOISY_IMAGE_LOADING
    nsFrame::ListTag(stdout, mFrame);
    printf(": %s%scomputedSize=%d,%d min=%d,%d max=%d,%d fixed=%s,%s\n",
           mFlags.mNeedIntrinsicImageSize ? "need-instrinsic-size " : "",
           mFlags.mHaveComputedSize ? "have-computed-size " : "",
           mComputedImageSize.width, mComputedImageSize.height,
           minWidth, minHeight, maxWidth, maxHeight,
           fixedContentWidth ? "yes" : "no",
           fixedContentHeight ? "yes" : "no");
#endif

    // Load the image at the desired size
    if ((0 != newWidth) && (0 != newHeight))
    {
      // Make sure we squelch a callback to the client of this image
      // loader during a start-load-image. Its possible the image we
      // want is ready to go and will therefore fire a notification
      // during the StartLoadImage call. Since this routine is already
      // returning size information there is no point in passing on the
      // callbacks to the client.
      mFlags.mSquelchCallback = PR_TRUE;
      StartLoadImage(aPresContext);
      mFlags.mSquelchCallback = PR_FALSE;

      // See if we just got the intrinsic size
      if (mFlags.mNeedIntrinsicImageSize && mFlags.mHaveIntrinsicImageSize) {
        // We just learned our intrinisic size. Start over from the top...
#ifdef NOISY_IMAGE_LOADING
        printf("  *** size arrived during StartLoadImage, looping...\n");
#endif
        continue;
      }
    }
    break;
  }

  aDesiredSize.width = mComputedImageSize.width;
  aDesiredSize.height = mComputedImageSize.height;
#ifdef NOISY_IMAGE_LOADING
  printf("nsHTMLImageLoader::GetDesiredSize returning %d, %d\n",
         aDesiredSize.width, aDesiredSize.height);
#endif
  if ((mFlags.mNeedIntrinsicImageSize && !mFlags.mHaveIntrinsicImageSize) ||
      mFlags.mNeedSizeNotification) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
nsHTMLImageLoader::GetLoadImageFailed() const
{
  PRBool  result = PR_FALSE;

  if (nsnull != mImageLoader) {
    // Ask the image loader whether the load failed
    PRUint32  loadStatus;
    mImageLoader->GetImageLoadStatus(&loadStatus);
    result = 0 != (loadStatus & NS_IMAGE_LOAD_STATUS_ERROR);
    return result;
  }

  result = mFlags.mLoadImageFailed ? PR_TRUE : PR_FALSE;
  return result;
}

PRUint32
nsHTMLImageLoader::GetLoadStatus() const
{
  PRUint32 loadStatus = NS_IMAGE_LOAD_STATUS_NONE;
  if (mImageLoader) {
    mImageLoader->GetImageLoadStatus(&loadStatus);
  }
  return loadStatus;
}
  
void
nsHTMLImageLoader::GetIntrinsicSize(nsSize& aSize)
{
  if (mImageLoader) {
    mImageLoader->GetIntrinsicSize(aSize);
  } else {
    aSize.SizeTo(0, 0);
  }
}

#ifdef DEBUG
// Note: this doesn't factor in:
// -- the mBaseURL (it might be shared)
// -- the mFrame (that will be counted elsewhere most likely)
// -- the mClosure (we don't know what type it is)
// -- the mImageLoader (it might be shared)
void
nsHTMLImageLoader::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  PRUint32 sum = sizeof(*this) - sizeof(mURLSpec) - sizeof(mURL);
  PRUint32 ss;
  mURLSpec.SizeOf(aHandler, &ss);       sum += ss;
  mURL.SizeOf(aHandler, &ss);           sum += ss;
  *aResult = sum;
}
#endif
