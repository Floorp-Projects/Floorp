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
#include "nsHTMLParts.h"
#include "nsHTMLImage.h"
#include "nsString.h"
#include "nsLeafFrame.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIFrameImageLoader.h"
#include "nsIPresShell.h"
#include "nsHTMLIIDs.h"
#include "nsIImage.h"
#include "nsIWidget.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLAttributes.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsImageMap.h"
#include "nsILinkHandler.h"
#include "nsIURL.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "prprf.h"
#include "nsISizeOfHandler.h"
#include "nsIFontMetrics.h"
#include "nsCSSRendering.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDeviceContext.h"
#include "nsINameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIStyleSet.h"

#ifndef _WIN32
#define BROKEN_IMAGE_URL "resource:/res/html/broken-image.gif"
#endif

// XXX until I fix aspect scaling...
#undef LET_IMAGE_LIBRARY_SCALE_ASPECT_IMAGES

static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);

// Value's for mSuppress
#define SUPPRESS_UNSET   0
#define DONT_SUPPRESS    1
#define SUPPRESS         2
#define DEFAULT_SUPPRESS 3

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET PRUint8(-1)

//----------------------------------------------------------------------

nsHTMLImageLoader::nsHTMLImageLoader()
  : mHaveImageSize(PR_FALSE),
    mImageSize(0, 0)
{
  mImageLoader = nsnull;
  mLoadImageFailed = PR_FALSE;
#ifndef _WIN32
  mLoadBrokenImageFailed = PR_FALSE;
#endif
  mURLSpec = nsnull;
  mBaseURL = nsnull;
}

nsHTMLImageLoader::~nsHTMLImageLoader()
{
  NS_IF_RELEASE(mImageLoader);
  if (nsnull != mURLSpec) {
    delete mURLSpec;
  }
  NS_IF_RELEASE(mBaseURL);
}

void
nsHTMLImageLoader::StopLoadImage(nsIPresContext& aPresContext,
                                 nsIFrame* aTargetFrame)
{
  aPresContext.StopLoadImage(aTargetFrame);
  NS_RELEASE(mImageLoader);
}

nsIImage*
nsHTMLImageLoader::GetImage()
{
  nsIImage* image = nsnull;
  if (nsnull != mImageLoader) {
    mImageLoader->GetImage(image);
  }
  return image;
}

nsresult
nsHTMLImageLoader::SetURLSpec(const nsString& aURLSpec)
{
  if (nsnull != mURLSpec) {
    delete mURLSpec;
  }
  mURLSpec = new nsString(aURLSpec);
  if (nsnull == mURLSpec) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
nsHTMLImageLoader::SetBaseURL(nsIURL* aBaseURL)
{
  NS_IF_RELEASE(mBaseURL);
  mBaseURL = aBaseURL;
  NS_IF_ADDREF(mBaseURL);
  return NS_OK;
}

void
nsHTMLImageLoader::GetBaseURL(nsIURL*& aResult) const {
  aResult = mBaseURL;
  NS_IF_ADDREF(aResult);
}

nsresult
nsHTMLImageLoader::StartLoadImage(nsIPresContext* aPresContext,
                                  nsIFrame* aForFrame,
                                  nsFrameImageLoaderCB aCallBack,
                                  PRBool aNeedSizeUpdate,
                                  PRIntn& aLoadStatus)
{
  aLoadStatus = NS_IMAGE_LOAD_STATUS_NONE;

  // Get absolute url the first time through
  nsresult rv;
  nsAutoString src;
#ifdef _WIN32
  if (mLoadImageFailed) {
    // We've already notified the pres shell that we're unable to render
    // the image so just return
    return NS_OK;
  } else if (nsnull == mURLSpec) {
    // No URI was specified for the src. Indicate we're unable to load the
    // image and notify the pres shell
    mLoadImageFailed = PR_TRUE;
    nsIPresShell* presShell;
    aPresContext->GetShell(&presShell);
    presShell->CantRenderReplacedElement(aPresContext, aForFrame);
    NS_RELEASE(presShell);
    return NS_OK;
#else
  if (mLoadImageFailed || (nsnull == mURLSpec)) {
    src.Append(BROKEN_IMAGE_URL);
#endif
  } else if (nsnull == mImageLoader) {
    // Create an absolute URL
    if (mBaseURL) {
      nsString empty;
      nsresult rv = NS_MakeAbsoluteURL(mBaseURL, empty, *mURLSpec, src);

      if (NS_OK != rv) {
        return rv;
      }
    } else {
      // Just use the URL spec
      src = *mURLSpec;
    }
  }

  if (nsnull == mImageLoader) {
    // Start image loading. Note that we don't specify a background color
    // so transparent images are always rendered using a transparency mask
#ifdef LET_IMAGE_LIBRARY_SCALE_ASPECT_IMAGES
    printf("loading ");
    fputs(*mURLSpec, stdout);
    printf(" at %d,%d size\n", mImageSize.width, mImageSize.height);
#else
    nsSize zero(0, 0);
#endif
    rv = aPresContext->StartLoadImage(src, nsnull, aForFrame,
#ifdef LET_IMAGE_LIBRARY_SCALE_ASPECT_IMAGES
                                      mImageSize,
#else
                                      mHaveImageSize ? mImageSize : zero,
#endif
                                      aCallBack, aNeedSizeUpdate,
                                      PR_TRUE, &mImageLoader);
    if ((NS_OK != rv) || (nsnull == mImageLoader)) {
      return rv;
    }
  }

  // Examine current image load status
  mImageLoader->GetImageLoadStatus(aLoadStatus);
  if (0 != (aLoadStatus & NS_IMAGE_LOAD_STATUS_ERROR)) {
    NS_RELEASE(mImageLoader);
#ifdef _WIN32
    // Display broken icon along with alt-text
    mLoadImageFailed = PR_TRUE;
#else
    if (mLoadImageFailed) {
      // We are doomed. Loading the broken image has just failed.
      mLoadBrokenImageFailed = PR_TRUE;
    }
    else {
      // Try again, this time using the broke-image url
      mLoadImageFailed = PR_TRUE;
      return StartLoadImage(aPresContext, aForFrame, nsnull,
                            aNeedSizeUpdate, aLoadStatus);
    }
#endif
  }
  return NS_OK;
}

void
nsHTMLImageLoader::GetDesiredSize(nsIPresContext* aPresContext,
                                  const nsHTMLReflowState& aReflowState,
                                  nsIFrame* aTargetFrame,
                                  nsFrameImageLoaderCB aCallBack,
                                  nsHTMLReflowMetrics& aDesiredSize)
{
  // If we have the image size then we are finished
  if (mHaveImageSize) {
    aDesiredSize.width = mImageSize.width;
    aDesiredSize.height = mImageSize.height;
    return;
  }

  // Determine whether the image has fixed content width and height
  PRBool  fixedContentWidth = aReflowState.HaveFixedContentWidth();
  PRBool  fixedContentHeight = aReflowState.HaveFixedContentHeight();
  if (NS_INTRINSICSIZE == aReflowState.computedWidth) {
    fixedContentWidth = PR_FALSE;
  }
  if (NS_INTRINSICSIZE == aReflowState.computedHeight) {
    fixedContentHeight = PR_FALSE;
  }

  // Choose reflow size
  if (fixedContentWidth || fixedContentHeight) {
    if (fixedContentWidth && fixedContentHeight) {
      // The image is fully constrained. Use the constraints directly.
      aDesiredSize.width = aReflowState.computedWidth;
      aDesiredSize.height = aReflowState.computedHeight;

      // Load image using the constrained widths so that it is scaled
      // properly.
      mHaveImageSize = PR_TRUE;
      mImageSize.SizeTo(aDesiredSize.width, aDesiredSize.height);
      PRIntn loadStatus;
      StartLoadImage(aPresContext, aTargetFrame, aCallBack,
                     !(fixedContentWidth && fixedContentHeight),
                     loadStatus);
    }
    else {
      // Get current image loading status (this may kick off an
      // initial load with no specified size).
      PRIntn loadStatus;
      StartLoadImage(aPresContext, aTargetFrame, aCallBack,
                     !(fixedContentWidth && fixedContentHeight),
                     loadStatus);

      // The image is partially constrained. Preserve aspect ratio of
      // image with unbound dimension.
      if ((0 == (loadStatus & NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE)) ||
          (nsnull == mImageLoader)) {
        // Provide a dummy size for now; later on when the image size
        // shows up we will reflow to the new size.
        aDesiredSize.width = 1;
        aDesiredSize.height = 1;
      }
      else {
        // Now we know the correct size (compute it).
        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);
        nsSize imageSize;
        mImageLoader->GetSize(imageSize);
        float imageWidth = imageSize.width * p2t;
        float imageHeight = imageSize.height * p2t;
        if (0.0f != imageHeight) {
          if (fixedContentWidth) {
            // We have a width, and an auto height. Compute height
            // from width.
            aDesiredSize.width = aReflowState.computedWidth;
            aDesiredSize.height = (nscoord)
              NSToIntRound(aReflowState.computedWidth * imageHeight / imageWidth);
          }
          else {
            // We have a height and an auto width. Compute width from
            // height.
            aDesiredSize.height = aReflowState.computedHeight;
            aDesiredSize.width = (nscoord)
              NSToIntRound(aReflowState.computedHeight * imageWidth / imageHeight);
          }
        }
        else {
          // Screwy image
          aDesiredSize.width = 1;
          aDesiredSize.height = 1;
        }

#ifdef LET_IMAGE_LIBRARY_SCALE_ASPECT_IMAGES
        if (!mHaveImageSize) {
          // Now we have an image size, but last time we didn't
          mImageSize.SizeTo(aDesiredSize.width, aDesiredSize.height);
          mHaveImageSize = PR_TRUE;

          // Load image again so that it is scaled properly.

          // XXX: optimization: don't do the second load if the sizes
          // happen to match
          StopLoadImage(*aPresContext, aTargetFrame);
          StartLoadImage(aPresContext, aTargetFrame, aCallBack,
                         !(fixedContentWidth && fixedContentHeight),
                         loadStatus);
        }
#endif
      }
    }
  }
  else {
    // Get current image loading status (this may kick off an
    // initial load with no specified size).
    PRIntn loadStatus;
    StartLoadImage(aPresContext, aTargetFrame, aCallBack,
                   !(fixedContentWidth && fixedContentHeight),
                   loadStatus);

    if ((0 == (loadStatus & NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE)) ||
        (nsnull == mImageLoader)) {
      // Provide a dummy size for now; later on when the image size
      // shows up we will reflow to the new size.
      aDesiredSize.width = 1;
      aDesiredSize.height = 1;
    } else {
      float p2t;
      aPresContext->GetScaledPixelsToTwips(&p2t);
      nsSize imageSize;
      mImageLoader->GetSize(imageSize);
      aDesiredSize.width = NSIntPixelsToTwips(imageSize.width, p2t);
      aDesiredSize.height = NSIntPixelsToTwips(imageSize.height, p2t);

      // Save size away for next time
      mImageSize.SizeTo(aDesiredSize.width, aDesiredSize.height);
      mHaveImageSize = PR_TRUE;
    }
  }
}

PRBool
nsHTMLImageLoader::GetLoadImageFailed() const
{
  PRBool  result = PR_FALSE;

  if (nsnull != mImageLoader) {
    PRIntn  loadStatus;

    // Ask the image loader whether the load failed
    mImageLoader->GetImageLoadStatus(loadStatus);
    result = 0 != (loadStatus & NS_IMAGE_LOAD_STATUS_ERROR);
  }

  result |= PRBool(mLoadImageFailed);
  return result;
}

//----------------------------------------------------------------------

nsresult
NS_NewImageFrame(nsIFrame*& aResult)
{
  nsImageFrame* frame = new nsImageFrame;
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}

nsImageFrame::~nsImageFrame()
{
}

NS_METHOD
nsImageFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  NS_IF_RELEASE(mImageMap);

  // Release image loader first so that it's refcnt can go to zero
  mImageLoader.DestroyLoader();

  return nsLeafFrame::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
nsImageFrame::Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsLeafFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // Set the image loader's source URL and base URL
  //~~~
  PRBool bSourcePresent = PR_FALSE;

  nsAutoString src;
  if ((NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, src)) &&
      (src.Length() > 0)) {
    bSourcePresent = PR_TRUE;
  }
  else if ((NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::data, src)) &&
      (src.Length() > 0)) {
    bSourcePresent = PR_TRUE;
  }

  if (bSourcePresent) {
    mImageLoader.SetURLSpec(src);
    nsIURL* baseURL = nsnull;
    nsIHTMLContent* htmlContent;
    if (NS_SUCCEEDED(mContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent))) {
      htmlContent->GetBaseURL(baseURL);
      NS_RELEASE(htmlContent);
    }
    else {
      nsIDocument* doc;
      if (NS_SUCCEEDED(mContent->GetDocument(doc))) {
        doc->GetBaseURL(baseURL);
        NS_RELEASE(doc);
      }
    }
    mImageLoader.SetBaseURL(baseURL);
    NS_IF_RELEASE(baseURL);
  }

  return rv;
}

static nsresult
UpdateImageFrame(nsIPresContext& aPresContext, nsIFrame* aFrame,
                 PRIntn aStatus)
{
  if (NS_IMAGE_LOAD_STATUS_ERROR & aStatus) {
    // XXX Turned off for the time being until the frame construction code for
    // images that can't be rendered handles floated and absolutely positioned
    // images...
#if 0
    // We failed to load the image. Notify the pres shell
    nsIPresShell* presShell = aPresContext.GetShell();
    presShell->CantRenderReplacedElement(&aPresContext, aFrame);
    NS_RELEASE(presShell);
#endif
  }
  else if (NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    // Now that the size is available, trigger a content-changed reflow
    nsIContent* content = nsnull;
    aFrame->GetContent(&content);
    if (nsnull != content) {
      nsIDocument* document = nsnull;
      content->GetDocument(document);
      if (nsnull != document) {
        document->ContentChanged(content, nsnull);
        NS_RELEASE(document);
      }
      NS_RELEASE(content);
    }
  }
  return NS_OK;
}

void
nsImageFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  if (mSizeFrozen) {
    aDesiredSize.width = mRect.width;
    aDesiredSize.height = mRect.height;
  }
  else {
    // Ask the image loader for the desired size
    mImageLoader.GetDesiredSize(aPresContext, aReflowState,
                                this, UpdateImageFrame,
                                aDesiredSize);
  }
}

void
nsImageFrame::GetInnerArea(nsIPresContext* aPresContext,
                           nsRect& aInnerArea) const
{
  aInnerArea.x = mBorderPadding.left;
  aInnerArea.y = mBorderPadding.top;
  aInnerArea.width = mRect.width -
    (mBorderPadding.left + mBorderPadding.right);
  aInnerArea.height = mRect.height -
    (mBorderPadding.top + mBorderPadding.bottom);
}

NS_IMETHODIMP
nsImageFrame::Reflow(nsIPresContext&          aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsImageFrame::Reflow: aMaxSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  GetDesiredSize(&aPresContext, aReflowState, aMetrics);
  AddBordersAndPadding(&aPresContext, aReflowState, aMetrics, mBorderPadding);
  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }
  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsImageFrame::Reflow: size=%d,%d",
                  aMetrics.width, aMetrics.height));
  return NS_OK;
}

// Computes the width of the specified string. aMaxWidth specifies the maximum
// width available. Once this limit is reached no more characters are measured.
// The number of characters that fit within the maximum width are returned in
// aMaxFit. NOTE: it is assumed that the fontmetrics have already been selected
// into the rendering context before this is called (for performance). MMP
void
nsImageFrame::MeasureString(const PRUnichar*     aString,
                            PRInt32              aLength,
                            nscoord              aMaxWidth,
                            PRUint32&            aMaxFit,
                            nsIRenderingContext& aContext)
{
  nscoord totalWidth = 0;
  nscoord spaceWidth;
  aContext.GetWidth(' ', spaceWidth);

  aMaxFit = 0;
  while (aLength > 0) {
    // Find the next place we can line break
    PRUint32  len = aLength;
    PRBool    trailingSpace = PR_FALSE;
    for (PRInt32 i = 0; i < aLength; i++) {
      if (XP_IS_SPACE(aString[i]) && (i > 0)) {
        len = i;  // don't include the space when measuring
        trailingSpace = PR_TRUE;
        break;
      }
    }
  
    // Measure this chunk of text, and see if it fits
    nscoord width;
    aContext.GetWidth(aString, len, width);
    PRBool  fits = (totalWidth + width) <= aMaxWidth;

    // If it fits on the line, or it's the first word we've processed then
    // include it
    if (fits || (0 == totalWidth)) {
      // New piece fits
      totalWidth += width;

      // If there's a trailing space then see if it fits as well
      if (trailingSpace) {
        if ((totalWidth + spaceWidth) <= aMaxWidth) {
          totalWidth += spaceWidth;
        } else {
          // Space won't fit. Leave it at the end but don't include it in
          // the width
          fits = PR_FALSE;
        }

        len++;
      }

      aMaxFit += len;
      aString += len;
      aLength -= len;
    }

    if (!fits) {
      break;
    }
  }
}

// Formats the alt-text to fit within the specified rectangle. Breaks lines
// between words if a word would extend past the edge of the rectangle
void
nsImageFrame::DisplayAltText(nsIPresContext&      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsString&      aAltText,
                             const nsRect&        aRect)
{
  const nsStyleColor* color =
    (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
  const nsStyleFont* font =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

  // Set font and color
  aRenderingContext.SetColor(color->mColor);
  aRenderingContext.SetFont(font->mFont);

  // Format the text to display within the formatting rect
  nsIFontMetrics* fm;
  aRenderingContext.GetFontMetrics(fm);

  nscoord maxDescent, height;
  fm->GetMaxDescent(maxDescent);
  fm->GetHeight(height);

  // XXX It would be nice if there was a way to have the font metrics tell
  // use where to break the text given a maximum width. At a minimum we need
  // to be able to get the break character...
  const PRUnichar* str = aAltText.GetUnicode();
  PRInt32          strLen = aAltText.Length();
  nscoord          y = aRect.y;
  while ((strLen > 0) && ((y + maxDescent) < aRect.YMost())) {
    // Determine how much of the text to display on this line
    PRUint32  maxFit;  // number of characters that fit
    MeasureString(str, strLen, aRect.width, maxFit, aRenderingContext);
    
    // Display the text
    aRenderingContext.DrawString(str, maxFit, aRect.x, y);

    // Move to the next line
    str += maxFit;
    strLen -= maxFit;
    y += height;
  }

  NS_RELEASE(fm);
}

struct nsRecessedBorder : public nsStyleSpacing {
  nsRecessedBorder(nscoord aBorderWidth)
    : nsStyleSpacing()
  {
    nsStyleCoord  styleCoord(aBorderWidth);

    mBorder.SetLeft(styleCoord);
    mBorder.SetTop(styleCoord);
    mBorder.SetRight(styleCoord);
    mBorder.SetBottom(styleCoord);

    mBorderStyle[0] = NS_STYLE_BORDER_STYLE_INSET;  
    mBorderStyle[1] = NS_STYLE_BORDER_STYLE_INSET;  
    mBorderStyle[2] = NS_STYLE_BORDER_STYLE_INSET;  
    mBorderStyle[3] = NS_STYLE_BORDER_STYLE_INSET;  

    mBorderColor[0] = 0;  
    mBorderColor[1] = 0;  
    mBorderColor[2] = 0;  
    mBorderColor[3] = 0;  

    mHasCachedMargin = mHasCachedPadding = mHasCachedBorder = PR_FALSE;
  }
};

void
nsImageFrame::DisplayAltFeedback(nsIPresContext&      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 PRInt32              aIconId)
{
  // Display a recessed one pixel border in the inner area
  PRBool clipState;
  nsRect  inner;
  GetInnerArea(&aPresContext, inner);

  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  nsRecessedBorder recessedBorder(NSIntPixelsToTwips(1, p2t));
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this, inner,
                              inner, recessedBorder, mStyleContext, 0);

  // Adjust the inner rect to account for the one pixel recessed border,
  // and a six pixel padding on each edge
  inner.Deflate(NSIntPixelsToTwips(7, p2t), NSIntPixelsToTwips(7, p2t));
  if (inner.IsEmpty()) {
    return;
  }

  // Clip so we don't render outside the inner rect
  aRenderingContext.PushState();
  aRenderingContext.SetClipRect(inner, nsClipCombine_kIntersect, clipState);

#ifdef _WIN32
  // Display the icon
  nsIDeviceContext* dc;
  aRenderingContext.GetDeviceContext(dc);
  nsIImage*         icon;

  if (NS_SUCCEEDED(dc->LoadIconImage(aIconId, icon))) {
    aRenderingContext.DrawImage(icon, inner.x, inner.y);

    // Reduce the inner rect by the width of the icon, and leave an
    // additional six pixels padding
    PRInt32 iconWidth = NSIntPixelsToTwips(icon->GetWidth() + 6, p2t);
    inner.x += iconWidth;
    inner.width -= iconWidth;

    NS_RELEASE(icon);
  }

  NS_RELEASE(dc);
#endif

  // If there's still room, display the alt-text
  if (!inner.IsEmpty()) {
    nsAutoString altText;
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::alt, altText)) {
      DisplayAltText(aPresContext, aRenderingContext, altText, inner);
    }
  }

  aRenderingContext.PopState(clipState);
}

NS_METHOD
nsImageFrame::Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer)
{
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);
  if (disp->mVisible) {
    // First paint background and borders
    nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       aWhichLayer);

    nsIImage* image = mImageLoader.GetImage();
    if (nsnull == image) {
      // No image yet, or image load failed. Draw the alt-text and an icon
      // indicating the status
      if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
        DisplayAltFeedback(aPresContext, aRenderingContext,
                           mImageLoader.GetLoadImageFailed()
                           ? NS_ICON_BROKEN_IMAGE
                           : NS_ICON_LOADING_IMAGE);
      }
      return NS_OK;
    }

    if ((NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer)
#ifdef LET_IMAGE_LIBRARY_SCALE_ASPECT_IMAGES
        && mImageLoader.HaveImageSize()
#endif
      ) {
      // Now render the image into our content area (the area inside the
      // borders and padding)
      nsRect inner;
      GetInnerArea(&aPresContext, inner);
      if (mImageLoader.GetLoadImageFailed()) {
        float p2t;
        aPresContext.GetScaledPixelsToTwips(&p2t);
        inner.width = NSIntPixelsToTwips(image->GetWidth(), p2t);
        inner.height = NSIntPixelsToTwips(image->GetHeight(), p2t);
      }
      aRenderingContext.DrawImage(image, inner);
    }

    if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
      nsImageMap* map = GetImageMap();
      if (nsnull != map) {
        nsRect inner;
        GetInnerArea(&aPresContext, inner);
        PRBool clipState;
        aRenderingContext.SetColor(NS_RGB(0, 0, 0));
        aRenderingContext.PushState();
        aRenderingContext.Translate(inner.x, inner.y);
        map->Draw(aPresContext, aRenderingContext);
        aRenderingContext.PopState(clipState);
      }
    }
  }

  return NS_OK;
}

nsImageMap*
nsImageFrame::GetImageMap()
{
  if (nsnull == mImageMap) {
    nsAutoString usemap;
    mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::usemap, usemap);
    if (0 == usemap.Length()) {
      return nsnull;
    }

    nsIDocument* doc = nsnull;
    mContent->GetDocument(doc);
    if (nsnull == doc) {
      return nsnull;
    }

    if (usemap.First() == '#') {
      usemap.Cut(0, 1);
    }
    nsIHTMLDocument* hdoc;
    nsresult rv = doc->QueryInterface(kIHTMLDocumentIID, (void**)&hdoc);
    NS_RELEASE(doc);
    if (NS_SUCCEEDED(rv)) {
      nsIDOMHTMLMapElement* map;
      rv = hdoc->GetImageMap(usemap, &map);
      NS_RELEASE(hdoc);
      if (NS_SUCCEEDED(rv)) {
        mImageMap = new nsImageMap();
        if (nsnull != mImageMap) {
          mImageMap->Init(map);
        }
        NS_IF_RELEASE(map);
      }
    }
  }

  return mImageMap;
}

void
nsImageFrame::TriggerLink(nsIPresContext& aPresContext,
                          const nsString& aURLSpec,
                          const nsString& aTargetSpec,
                          PRBool aClick)
{
  nsILinkHandler* handler = nsnull;
  aPresContext.GetLinkHandler(&handler);
  if (nsnull != handler) {
    if (aClick) {
      handler->OnLinkClick(mContent, eLinkVerb_Replace, aURLSpec, aTargetSpec);
    }
    else {
      handler->OnOverLink(mContent, aURLSpec, aTargetSpec);
    }
    NS_RELEASE(handler);
  }
}

PRBool
nsImageFrame::IsServerImageMap()
{
  nsAutoString ismap;
  return NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::ismap, ismap);
}

PRIntn
nsImageFrame::GetSuppress()
{
  nsAutoString s;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::suppress, s)) {
    if (s.EqualsIgnoreCase("true")) {
      return SUPPRESS;
    } else if (s.EqualsIgnoreCase("false")) {
      return DONT_SUPPRESS;
    }
  }
  return DEFAULT_SUPPRESS;
}

// XXX what should clicks on transparent pixels do?
NS_METHOD
nsImageFrame::HandleEvent(nsIPresContext& aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus& aEventStatus)
{
  nsImageMap* map;
  aEventStatus = nsEventStatus_eIgnore; 

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
  case NS_MOUSE_MOVE:
    map = GetImageMap();
    if ((nsnull != map) || IsServerImageMap()) {
      // Ask map if the x,y coordinates are in a clickable area
      float t2p;
      aPresContext.GetTwipsToPixels(&t2p);
      nsAutoString absURL, target, altText;
      PRBool suppress;
      if (nsnull != map) {
        // Subtract out border and padding here so that we are looking
        // at the right coordinates. Hit detection against area tags
        // is done after the mouse wanders over the image, not over
        // the image's borders.
        nsRect inner;
        GetInnerArea(&aPresContext, inner);

        // XXX Event isn't in our local coordinate space like it should be...
        nsIView*  view;
        GetView(&view);
        if (nsnull == view) {
          nsPoint offset;
          GetOffsetFromView(offset, &view);
          if (nsnull != view) {
            aEvent->point -= offset;
          }
        }

        PRInt32 x = NSTwipsToIntPixels((aEvent->point.x - inner.x), t2p);
        PRInt32 y = NSTwipsToIntPixels((aEvent->point.y - inner.y), t2p);
        nsIURL* docURL = nsnull;
        nsIDocument* doc = nsnull;
        mContent->GetDocument(doc);
        if (nsnull != doc) {
          docURL = doc->GetDocumentURL();
          NS_RELEASE(doc);
        }

        NS_ASSERTION(nsnull != docURL, "nsIDocument->GetDocumentURL() returning nsnull");
        PRBool inside = PR_FALSE;
        if (nsnull != docURL) {
           inside = map->IsInside(x, y, docURL, absURL, target, altText,
                                        &suppress);
        }
        NS_IF_RELEASE(docURL);
        if (inside) {
          // We hit a clickable area. Time to go somewhere...
          PRBool clicked = PR_FALSE;
          if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
            aEventStatus = nsEventStatus_eConsumeNoDefault; 
            clicked = PR_TRUE;
          }
          TriggerLink(aPresContext, absURL, target, clicked);
        }
      }
      else {
        suppress = GetSuppress();
        nsIURL* baseURL = nsnull;
        nsIHTMLContent* htmlContent;
        if (NS_SUCCEEDED(mContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent))) {
          htmlContent->GetBaseURL(baseURL);
          NS_RELEASE(htmlContent);
        }
        else {
          nsIDocument* doc;
          if (NS_SUCCEEDED(mContent->GetDocument(doc))) {
            doc->GetBaseURL(baseURL);
            NS_RELEASE(doc);
          }
        }
        nsAutoString src;
        nsString empty;
        mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, src);
        NS_MakeAbsoluteURL(baseURL, empty, src, absURL);
        NS_IF_RELEASE(baseURL);

        // Note: We don't subtract out the border/padding here to remain
        // compatible with navigator. [ick]
        PRInt32 x = NSTwipsToIntPixels(aEvent->point.x, t2p);
        PRInt32 y = NSTwipsToIntPixels(aEvent->point.y, t2p);
        char cbuf[50];
        PR_snprintf(cbuf, sizeof(cbuf), "?%d,%d", x, y);
        absURL.Append(cbuf);
        PRBool clicked = PR_FALSE;
        if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
          aEventStatus = nsEventStatus_eConsumeNoDefault; 
          clicked = PR_TRUE;
        }
        TriggerLink(aPresContext, absURL, target, clicked);
      }
      break;
    }
    // FALL THROUGH

  default:
    // Let default event handler deal with it
    return nsLeafFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }
  return NS_OK;
}

NS_METHOD
nsImageFrame::GetCursor(nsIPresContext& aPresContext,
                        nsPoint& aPoint,
                        PRInt32& aCursor)
{
  //XXX This will need to be rewritten once we have content for areas
  nsImageMap* map = GetImageMap();
  if (nsnull != map) {
    nsRect inner;
    GetInnerArea(&aPresContext, inner);
    aCursor = NS_STYLE_CURSOR_DEFAULT;
    float t2p;
    aPresContext.GetTwipsToPixels(&t2p);

    // XXX Event isn't in our local coordinate space like it should be...
    nsPoint   pt = aPoint;
    nsIView*  view;
    GetView(&view);
    if (nsnull == view) {
      nsPoint offset;
      GetOffsetFromView(offset, &view);
      if (nsnull != view) {
        pt -= offset;
      }
    }

    PRInt32 x = NSTwipsToIntPixels((pt.x - inner.x), t2p);
    PRInt32 y = NSTwipsToIntPixels((pt.y - inner.y), t2p);
    if (map->IsInside(x, y)) {
      // Use style defined cursor if one is provided, otherwise when
      // the cursor style is "auto" we use the pointer cursor.
      const nsStyleColor* styleColor;
      GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&)styleColor);
      aCursor = styleColor->mCursor;
      if (NS_STYLE_CURSOR_AUTO == aCursor) {
        aCursor = NS_STYLE_CURSOR_POINTER;
      }
    }
    return NS_OK;
  }

  return nsFrame::GetCursor(aPresContext, aPoint, aCursor);
}

NS_IMETHODIMP
nsImageFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsLeafFrame::AttributeChanged(aPresContext, aChild,
                                              aAttribute, aHint);
  if (NS_OK != rv) {
    return rv;
  }
  if (nsHTMLAtoms::src == aAttribute) {
    nsAutoString oldSRC;
    mImageLoader.GetURLSpec(oldSRC);
    nsAutoString newSRC;

    aChild->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, newSRC);
    if (!oldSRC.Equals(newSRC)) {
      mSizeFrozen = PR_TRUE;
      
#ifdef NS_DEBUG
      char oldcbuf[100], newcbuf[100];
      oldSRC.ToCString(oldcbuf, sizeof(oldcbuf));
      newSRC.ToCString(newcbuf, sizeof(newcbuf));
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
         ("nsImageFrame::AttributeChanged: new image source; old='%s' new='%s'",
          oldcbuf, newcbuf));
#endif

      // Get rid of old image loader and start a new image load going
      mImageLoader.DestroyLoader();

      // Fire up a new image load request
      PRIntn loadStatus;
#ifdef LET_IMAGE_LIBRARY_SCALE_ASPECT_IMAGES
      mImageLoader.StopLoadImage(*aPresContext, this);
#endif
      mImageLoader.SetURLSpec(newSRC);
      mImageLoader.StartLoadImage(aPresContext, this, nsnull,
                                  PR_FALSE, loadStatus);

      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                     ("nsImageFrame::AttributeChanged: loadImage status=%x",
                      loadStatus));

      // If the image is already ready then we need to trigger a
      // redraw because the image loader won't.
      if (loadStatus & NS_IMAGE_LOAD_STATUS_IMAGE_READY) {
        // XXX Stuff this into a method on nsIPresShell/Context
        nsRect bounds;
        nsPoint offset;
        nsIView* view;
        GetOffsetFromView(offset, &view);
        nsIViewManager* vm;
        view->GetViewManager(vm);
        bounds.x = offset.x;
        bounds.y = offset.y;
        bounds.width = mRect.width;
        bounds.height = mRect.height;
        vm->UpdateView(view, bounds, 0);
        NS_RELEASE(vm);
      }
    }
  }

  return NS_OK;
}
