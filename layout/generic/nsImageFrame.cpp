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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsHTMLParts.h"
#include "nsCOMPtr.h"
#include "nsImageFrame.h"
#include "nsString.h"
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
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "prprf.h"
#include "nsIFontMetrics.h"
#include "nsCSSRendering.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDeviceContext.h"
#include "nsINameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIStyleSet.h"
#include "nsLayoutAtoms.h"
#include "nsISizeOfHandler.h"

#include "nsIFrameManager.h"
#include "nsIScriptSecurityManager.h"


#ifdef DEBUG
#undef NOISY_IMAGE_LOADING
#else
#undef NOISY_IMAGE_LOADING
#endif

static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);

// Value's for mSuppress
#define SUPPRESS_UNSET   0
#define DONT_SUPPRESS    1
#define SUPPRESS         2
#define DEFAULT_SUPPRESS 3

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET PRUint8(-1)

nsresult
NS_NewImageFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsImageFrame* it = new (aPresShell) nsImageFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsImageFrame::nsImageFrame() :
  mLowSrcImageLoader(nsnull)
{
}

nsImageFrame::~nsImageFrame()
{
  if (mLowSrcImageLoader != nsnull) {
    delete mLowSrcImageLoader;
  }
}

NS_METHOD
nsImageFrame::Destroy(nsIPresContext* aPresContext)
{
  // Tell our image map, if there is one, to clean up
  // This causes the nsImageMap to unregister itself as
  // a DOM listener.
    if(mImageMap) {
      mImageMap->Destroy();
      NS_RELEASE(mImageMap);
    }

  // Release image loader first so that it's refcnt can go to zero
  mImageLoader.StopAllLoadImages(aPresContext);
  if (mLowSrcImageLoader != nsnull) {
    mLowSrcImageLoader->StopAllLoadImages(aPresContext);
  }
  
  return nsLeafFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsImageFrame::Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsLeafFrame::Init(aPresContext, aContent, aParent,
                                   aContext, aPrevInFlow);

  // See if we have a SRC attribute
  nsAutoString src;
  nsresult ca;
  ca = mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, src);
  if ((NS_CONTENT_ATTR_HAS_VALUE != ca) || (src.Length() == 0))
  {
    // Let's see if this is an object tag and we have a DATA attribute
    nsIAtom*  tag;
    mContent->GetTag(tag);

    if(tag == nsHTMLAtoms::object)
      mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::data, src);
  
    NS_IF_RELEASE(tag);
  }

  nsAutoString lowSrc;
  nsresult lowSrcResult;
  lowSrcResult = mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::lowsrc, lowSrc);

  // Set the image loader's source URL and base URL
  nsIURI* baseURL = nsnull;
  nsIHTMLContent* htmlContent;
  rv = mContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
  if (NS_SUCCEEDED(rv)) {
    htmlContent->GetBaseURL(baseURL);
    NS_RELEASE(htmlContent);
  }
  else {
    nsIDocument* doc;
    rv = mContent->GetDocument(doc);
    if (NS_SUCCEEDED(rv)) {
      doc->GetBaseURL(baseURL);
      NS_RELEASE(doc);
    }
  }

  if (NS_CONTENT_ATTR_HAS_VALUE == lowSrcResult && lowSrc.Length() > 0) {
    mLowSrcImageLoader = new nsHTMLImageLoader;
    if (mLowSrcImageLoader != nsnull) {
      mLowSrcImageLoader->Init(this, UpdateImageFrame, (void*)mLowSrcImageLoader, baseURL, lowSrc);
    }
  }
  mImageLoader.Init(this, UpdateImageFrame, (void*)&mImageLoader, baseURL, src);
  NS_IF_RELEASE(baseURL);

  mInitialLoadCompleted = PR_FALSE;
  mCanSendLoadEvent = PR_TRUE;
  return rv;
}

nsresult
nsImageFrame::UpdateImageFrame(nsIPresContext* aPresContext,
                               nsHTMLImageLoader* aLoader,
                               nsIFrame* aFrame,
                               void* aClosure,
                               PRUint32 aStatus)
{
  nsImageFrame* frame = (nsImageFrame*) aFrame;
  return frame->UpdateImage(aPresContext, aStatus, aClosure);
}

nsresult
nsImageFrame::UpdateImage(nsIPresContext* aPresContext, PRUint32 aStatus, void* aClosure)
{
#ifdef NOISY_IMAGE_LOADING
  ListTag(stdout);
  printf(": UpdateImage: status=%x\n", aStatus);
#endif

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  // check to see if an image error occurred
  PRBool imageFailedToLoad = PR_FALSE;
  if (NS_IMAGE_LOAD_STATUS_ERROR & aStatus) { // We failed to load the image. Notify the pres shell

    // One of the two images didn't load, which one?
    // see if we are loading the lowsrc image
    if (mLowSrcImageLoader != nsnull) {
      // We ARE loading the lowsrc image
      // check to see if the closure data is the lowsrc
      if (aClosure == mLowSrcImageLoader) {
        // The lowsrc failed to load, but only set the bool to true
        // true if the src image failed.
        imageFailedToLoad = mImageLoader.GetImage() == nsnull || (mImageLoader.GetLoadStatus() & NS_IMAGE_LOAD_STATUS_ERROR);
      } else { 
        // src failed to load, 
        // see if the lowsrc is here so we can paint it instead
        imageFailedToLoad = (mLowSrcImageLoader->GetLoadStatus() & NS_IMAGE_LOAD_STATUS_ERROR) != 0;
      }
    } else {
      imageFailedToLoad = PR_TRUE;
    }
  }

  // if src failed and there is no lowsrc
  // or both failed to load, then notify the PresShell
  if (imageFailedToLoad) {    
    if (presShell) {
      // Also send error event
      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event;
      event.eventStructType = NS_EVENT;
      event.message = NS_IMAGE_ERROR;
      presShell->HandleEventWithTarget(&event,this,mContent,NS_EVENT_FLAG_INIT,&status);

      nsAutoString usemap;
      mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::usemap, usemap);    
      // We failed to load the image. Notify the pres shell if we aren't an image map
      if (usemap.Length() == 0) {
        presShell->CantRenderReplacedElement(aPresContext, this);      
      }
    }
  } else if (NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    if (mParent) {
      mState |= NS_FRAME_IS_DIRTY;
	    mParent->ReflowDirtyChild(presShell, (nsIFrame*) this);
    }
    else {
      NS_ASSERTION(0, "No parent to pass the reflow request up to.");
    }
  }

  if (mCanSendLoadEvent &&
      !imageFailedToLoad && 
      (NS_IMAGE_LOAD_STATUS_IMAGE_READY & aStatus) &&
      presShell) {
    // Send load event
    mCanSendLoadEvent = PR_FALSE;

    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_IMAGE_LOAD;
    presShell->HandleEventWithTarget(&event,this,mContent,NS_EVENT_FLAG_INIT | NS_EVENT_FLAG_CANT_BUBBLE,&status);
  }

#if 0 // ifdef'ing out the deleting of the lowsrc image
      // if the "src" image was change via script to an iage that
      // didn't exist, the the author would expect the lowsrc to be displayed.
  // now check to see if we have the entire low
  if (mLowSrcImageLoader != nsnull) {
    nsIImage * image = mImageLoader.GetImage();
    if (image) {
      PRInt32 imgSrcLinesLoaded = image != nsnull?image->GetDecodedY2():-1;
      if (image->GetDecodedY2() == image->GetHeight()) {
        delete mLowSrcImageLoader;
        mLowSrcImageLoader = nsnull;
      }
      NS_RELEASE(image);
    }
  }
#endif

  return NS_OK;
}

void
nsImageFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  PRBool cancelledReflow = PR_FALSE;

  if (mLowSrcImageLoader != nsnull && !(mImageLoader.GetLoadStatus() & NS_IMAGE_LOAD_STATUS_IMAGE_READY)) {
    PRBool gotDesiredSize = mLowSrcImageLoader->GetDesiredSize(aPresContext, &aReflowState, aDesiredSize);
    /*if (gotDesiredSize) {
      // We have our "final" desired size. Cancel any pending
      // incremental reflows aimed at this frame.
      nsIPresShell* shell;
      aPresContext->GetShell(&shell);
      if (shell) {
        shell->CancelReflowCommand(this, nsnull);
        NS_RELEASE(shell);
      }
    }*/
  }

  // Must ask for desiredSize to start loading of image
  // that seems like a bogus API
  if (mImageLoader.GetDesiredSize(aPresContext, &aReflowState, aDesiredSize)) {
    if (!cancelledReflow) {
      // We have our "final" desired size. Cancel any pending
      // incremental reflows aimed at this frame.
      nsIPresShell* shell;
      aPresContext->GetShell(&shell);
      if (shell) {
        shell->CancelReflowCommand(this, nsnull);
        NS_RELEASE(shell);
      }
    }
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
nsImageFrame::Reflow(nsIPresContext*          aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsImageFrame", aReflowState.reason);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsImageFrame::Reflow: availSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  GetDesiredSize(aPresContext, aReflowState, aMetrics);
  AddBordersAndPadding(aPresContext, aReflowState, aMetrics, mBorderPadding);
  if (nsnull != aMetrics.maxElementSize) {
    // If we have a percentage based width, then our MES width is 0
    if (eStyleUnit_Percent == aReflowState.mStylePosition->mWidth.GetUnit()) {
      aMetrics.maxElementSize->width = 0;
    } else {
      aMetrics.maxElementSize->width = aMetrics.width;
    }
    aMetrics.maxElementSize->height = aMetrics.height;
  }
  if (aMetrics.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
    aMetrics.mMaximumWidth = aMetrics.width;
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
nsImageFrame::DisplayAltText(nsIPresContext*      aPresContext,
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
nsImageFrame::DisplayAltFeedback(nsIPresContext*      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 PRInt32              aIconId)
{
  // Calculate the inner area
  nsRect  inner;
  GetInnerArea(aPresContext, inner);

  // Display a recessed one pixel border
  float   p2t;
  nscoord borderEdgeWidth;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  borderEdgeWidth = NSIntPixelsToTwips(1, p2t);

  // Make sure we have enough room to actually render the border within
  // our frame bounds
  if ((inner.width < 2 * borderEdgeWidth) || (inner.height < 2 * borderEdgeWidth)) {
    return;
  }

  // Paint the border
  nsRecessedBorder recessedBorder(borderEdgeWidth);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this, inner,
                              inner, recessedBorder, mStyleContext, 0);

  // Adjust the inner rect to account for the one pixel recessed border,
  // and a six pixel padding on each edge
  inner.Deflate(NSIntPixelsToTwips(7, p2t), NSIntPixelsToTwips(7, p2t));
  if (inner.IsEmpty()) {
    return;
  }

  // Clip so we don't render outside the inner rect
  PRBool clipState;
  aRenderingContext.PushState();
  aRenderingContext.SetClipRect(inner, nsClipCombine_kIntersect, clipState);

  // Display the icon
  nsIDeviceContext* dc;
  aRenderingContext.GetDeviceContext(dc);
  nsIImage*         icon;

  if (NS_SUCCEEDED(dc->LoadIconImage(aIconId, icon)) && icon) {
    aRenderingContext.DrawImage(icon, inner.x, inner.y);

    // Reduce the inner rect by the width of the icon, and leave an
    // additional six pixels padding
    PRInt32 iconWidth = NSIntPixelsToTwips(icon->GetWidth() + 6, p2t);
    inner.x += iconWidth;
    inner.width -= iconWidth;

    NS_RELEASE(icon);
  }

  NS_RELEASE(dc);

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
nsImageFrame::Paint(nsIPresContext* aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer)
{
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);
  if (disp->IsVisible() && mRect.width && mRect.height) {
    // First paint background and borders
    nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       aWhichLayer);

    // first get to see if lowsrc image is here
    PRInt32 lowSrcLinesLoaded = -1;
    PRInt32 imgSrcLinesLoaded = -1;
    nsIImage * lowImage = nsnull;
    nsIImage * image    = nsnull;

    // if lowsrc is here 
    if (mLowSrcImageLoader != nsnull) {
      lowImage = mLowSrcImageLoader->GetImage();
      lowSrcLinesLoaded = lowImage != nsnull?lowImage->GetDecodedY2():-1;
    }

    image = mImageLoader.GetImage();
    imgSrcLinesLoaded = image != nsnull?image->GetDecodedY2():-1;

    if (nsnull == image && nsnull == lowImage) {
      // No image yet, or image load failed. Draw the alt-text and an icon
      // indicating the status
      if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer &&
          !mInitialLoadCompleted) {
        DisplayAltFeedback(aPresContext, aRenderingContext,
                           mImageLoader.GetLoadImageFailed()
                           ? NS_ICON_BROKEN_IMAGE
                           : NS_ICON_LOADING_IMAGE);
      }
    }
    else {
      mInitialLoadCompleted = PR_TRUE;
      if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
        // Now render the image into our content area (the area inside the
        // borders and padding)
        nsRect inner;
        GetInnerArea(aPresContext, inner);
        if (mImageLoader.GetLoadImageFailed()) {
          float p2t;
          aPresContext->GetScaledPixelsToTwips(&p2t);
          if (image != nsnull) {
            inner.width  = NSIntPixelsToTwips(image->GetWidth(), p2t);
            inner.height = NSIntPixelsToTwips(image->GetHeight(), p2t);
          } else if (lowImage != nsnull) {
            inner.width  = NSIntPixelsToTwips(lowImage->GetWidth(), p2t);
            inner.height = NSIntPixelsToTwips(lowImage->GetHeight(), p2t);
          }
        }

        if (image != nsnull && imgSrcLinesLoaded > 0) {
          aRenderingContext.DrawImage(image, inner);
        } else if (lowImage != nsnull && lowSrcLinesLoaded > 0) {
          aRenderingContext.DrawImage(lowImage, inner);
        }

      }

      nsImageMap* map = GetImageMap(aPresContext);
      if (nsnull != map) {
        nsRect inner;
        GetInnerArea(aPresContext, inner);
        PRBool clipState;
        aRenderingContext.SetColor(NS_RGB(0, 0, 0));
        aRenderingContext.SetLineStyle(nsLineStyle_kDotted);
        aRenderingContext.PushState();
        aRenderingContext.Translate(inner.x, inner.y);
        map->Draw(aPresContext, aRenderingContext);
        aRenderingContext.PopState(clipState);
      }

#ifdef DEBUG
      if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) &&
          GetShowFrameBorders()) {
        nsImageMap* map = GetImageMap(aPresContext);
        if (nsnull != map) {
          nsRect inner;
          GetInnerArea(aPresContext, inner);
          PRBool clipState;
          aRenderingContext.SetColor(NS_RGB(0, 0, 0));
          aRenderingContext.PushState();
          aRenderingContext.Translate(inner.x, inner.y);
          map->Draw(aPresContext, aRenderingContext);
          aRenderingContext.PopState(clipState);
        }
      }
#endif
    }

    NS_IF_RELEASE(lowImage);
    NS_IF_RELEASE(image);
  }
  
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

nsImageMap*
nsImageFrame::GetImageMap(nsIPresContext* aPresContext)
{
  if (nsnull == mImageMap) {
    nsAutoString usemap;
    mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::usemap, usemap);
    if (0 == usemap.Length()) {
      return nsnull;
    }

    // Strip out whitespace in the name for navigator compatability
    // XXX NAV QUIRK
    usemap.StripWhitespace();

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
        nsCOMPtr<nsIPresShell> presShell;
        aPresContext->GetShell(getter_AddRefs(presShell));

        mImageMap = new nsImageMap();
        if (nsnull != mImageMap) {
          NS_ADDREF(mImageMap);
          mImageMap->Init(presShell, this, map);
        }
        NS_IF_RELEASE(map);
      }
    }
  }

  return mImageMap;
}

void
nsImageFrame::TriggerLink(nsIPresContext* aPresContext,
                          const nsString& aURLSpec,
                          const nsString& aTargetSpec,
                          PRBool aClick)
{
  // We get here with server side image map
  nsILinkHandler* handler = nsnull;
  aPresContext->GetLinkHandler(&handler);
  if (nsnull != handler) {
    if (aClick) {
      nsresult proceed = NS_OK;
      // Check that this page is allowed to load this URI.
      // Almost a copy of the similarly named method in nsGenericElement
      nsresult rv;
      NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                      NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);

      nsCOMPtr<nsIPresShell> ps;
      if (NS_SUCCEEDED(rv)) 
        rv = aPresContext->GetShell(getter_AddRefs(ps));
      nsCOMPtr<nsIDocument> doc;
      if (NS_SUCCEEDED(rv) && ps) 
        rv = ps->GetDocument(getter_AddRefs(doc));
      
      nsCOMPtr<nsIURI> baseURI;
      if (NS_SUCCEEDED(rv) && doc) 
        baseURI = dont_AddRef(doc->GetDocumentURL());
      nsCOMPtr<nsIURI> absURI;
      if (NS_SUCCEEDED(rv)) 
        rv = NS_NewURI(getter_AddRefs(absURI), aURLSpec, baseURI);
      
      if (NS_SUCCEEDED(rv)) 
        proceed = securityManager->CheckLoadURI(baseURI, absURI, nsIScriptSecurityManager::STANDARD);

      // Only pass off the click event if the script security manager
      // says it's ok.
      if (NS_SUCCEEDED(proceed))
        handler->OnLinkClick(mContent, eLinkVerb_Replace, aURLSpec.GetUnicode(), aTargetSpec.GetUnicode());
    }
    else {
      handler->OnOverLink(mContent, aURLSpec.GetUnicode(), aTargetSpec.GetUnicode());
    }
    NS_RELEASE(handler);
  }
}

PRBool
nsImageFrame::IsServerImageMap()
{
  nsAutoString ismap;
  return NS_CONTENT_ATTR_HAS_VALUE ==
    mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::ismap, ismap);
}

PRIntn
nsImageFrame::GetSuppress()
{
  nsAutoString s;
  if (NS_CONTENT_ATTR_HAS_VALUE ==
      mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::suppress, s)) {
    if (s.EqualsIgnoreCase("true")) {
      return SUPPRESS;
    } else if (s.EqualsIgnoreCase("false")) {
      return DONT_SUPPRESS;
    }
  }
  return DEFAULT_SUPPRESS;
}

//XXX the event come's in in view relative coords, but really should
//be in frame relative coords by the time it hits our frame.

// Translate an point that is relative to our view (or a containing
// view) into a localized pixel coordinate that is relative to the
// content area of this frame (inside the border+padding).
void
nsImageFrame::TranslateEventCoords(nsIPresContext* aPresContext,
                                   const nsPoint& aPoint,
                                   nsPoint& aResult)
{
  nscoord x = aPoint.x;
  nscoord y = aPoint.y;

  // If we have a view then the event coordinates are already relative
  // to this frame; otherwise we have to adjust the coordinates
  // appropriately.
  nsIView* view;
  GetView(aPresContext, &view);
  if (nsnull == view) {
    nsPoint offset;
    GetOffsetFromView(aPresContext, offset, &view);
    if (nsnull != view) {
      x -= offset.x;
      y -= offset.y;
    }
  }

  // Subtract out border and padding here so that the coordinates are
  // now relative to the content area of this frame.
  nsRect inner;
  GetInnerArea(aPresContext, inner);
  x -= inner.x;
  y -= inner.y;

  // Translate the coordinates from twips to pixels
  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);
  aResult.x = NSTwipsToIntPixels(x, t2p);
  aResult.y = NSTwipsToIntPixels(y, t2p);
}

PRBool
nsImageFrame::GetAnchorHREF(nsString& aResult)
{
  PRBool status = PR_FALSE;
  aResult.Truncate();

  // Walk up the content tree, looking for an nsIDOMAnchorElement
  nsCOMPtr<nsIContent> content;
  mContent->GetParent(*getter_AddRefs(content));
  while (content) {
    nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(content));
    if (anchor) {
      anchor->GetHref(aResult);
      if (aResult.Length() > 0) {
        status = PR_TRUE;
      }
      break;
    }
    nsCOMPtr<nsIContent> parent;
    content->GetParent(*getter_AddRefs(parent));
    content = parent;
  }
  return status;
}

NS_IMETHODIMP  
nsImageFrame::GetContentForEvent(nsIPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIContent** aContent)
{
  NS_ENSURE_ARG_POINTER(aContent);
  nsImageMap* map;
  map = GetImageMap(aPresContext);

  if (nsnull != map) {
    nsPoint p;
    TranslateEventCoords(aPresContext, aEvent->point, p);
    nsAutoString absURL, target, altText;
    PRBool suppress;
    PRBool inside = PR_FALSE;
    nsCOMPtr<nsIContent> area;
    inside = map->IsInside(p.x, p.y, getter_AddRefs(area),
                           absURL, target, altText,
                           &suppress);
    if (inside && area) {
      *aContent = area;
      NS_ADDREF(*aContent);
      return NS_OK;
    }
  }

  return GetContent(aContent);
}

// XXX what should clicks on transparent pixels do?
NS_METHOD
nsImageFrame::HandleEvent(nsIPresContext* aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  nsImageMap* map;

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
  case NS_MOUSE_MOVE:
    {
      map = GetImageMap(aPresContext);
      PRBool isServerMap = IsServerImageMap();
      if ((nsnull != map) || isServerMap) {
        nsPoint p;
        TranslateEventCoords(aPresContext, aEvent->point, p);
        nsAutoString absURL, target, altText;
        PRBool suppress;
        PRBool inside = PR_FALSE;
        // Even though client-side image map triggering happens
        // through content, we need to make sure we're not inside
        // (in case we deal with a case of both client-side and
        // sever-side on the same image - it happens!)
        if (nsnull != map) {
          nsCOMPtr<nsIContent> area;
          inside = map->IsInside(p.x, p.y, getter_AddRefs(area),
                                 absURL, target, altText,
                                 &suppress);
        }
        
        if (!inside && isServerMap) {
          suppress = GetSuppress();
          nsIURI* baseURL = nsnull;
          nsIHTMLContent* htmlContent;
          if (NS_SUCCEEDED(mContent->QueryInterface(kIHTMLContentIID,
                                                    (void**)&htmlContent))) {
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
          
          // Server side image maps use the href in a containing anchor
          // element to provide the basis for the destination url.
          nsAutoString src;
          if (GetAnchorHREF(src)) {
            NS_MakeAbsoluteURI(absURL, src, baseURL);
            NS_IF_RELEASE(baseURL);
            
            // XXX if the mouse is over/clicked in the border/padding area
            // we should probably just pretend nothing happened. Nav4
            // keeps the x,y coordinates positive as we do; IE doesn't
            // bother. Both of them send the click through even when the
            // mouse is over the border.
            if (p.x < 0) p.x = 0;
            if (p.y < 0) p.y = 0;
            char cbuf[50];
            PR_snprintf(cbuf, sizeof(cbuf), "?%d,%d", p.x, p.y);
            absURL.AppendWithConversion(cbuf);
            PRBool clicked = PR_FALSE;
            if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
              *aEventStatus = nsEventStatus_eConsumeDoDefault; 
              clicked = PR_TRUE;
            }
            TriggerLink(aPresContext, absURL, target, clicked);
          }
        }
      }
      break;
    }
    default:
      break;
  }

  return nsLeafFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

//XXX This will need to be rewritten once we have content for areas
NS_METHOD
nsImageFrame::GetCursor(nsIPresContext* aPresContext,
                        nsPoint& aPoint,
                        PRInt32& aCursor)
{
  nsImageMap* map = GetImageMap(aPresContext);
  if (nsnull != map) {
    nsPoint p;
    TranslateEventCoords(aPresContext, aPoint, p);
    aCursor = NS_STYLE_CURSOR_DEFAULT;
    if (map->IsInside(p.x, p.y)) {
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
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsLeafFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aHint);
  if (NS_OK != rv) {
    return rv;
  }
  if (nsHTMLAtoms::src == aAttribute) {
    nsAutoString oldSRC, newSRC;
    mImageLoader.GetURLSpec(oldSRC);
    aChild->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::src, newSRC);
    if (!oldSRC.Equals(newSRC)) {
#ifdef NOISY_IMAGE_LOADING
      ListTag(stdout);
      printf(": new image source; old='");
      fputs(oldSRC, stdout);
      printf("' new='");
      fputs(newSRC, stdout);
      printf("'\n");
#endif

      if (mImageLoader.IsImageSizeKnown()) {
        mImageLoader.UpdateURLSpec(aPresContext, newSRC);
        PRUint32 loadStatus = mImageLoader.GetLoadStatus();
        if (loadStatus & NS_IMAGE_LOAD_STATUS_IMAGE_READY) {
          // Trigger a paint now because image-loader won't if the
          // image is already loaded and ready to go.
          Invalidate(aPresContext, nsRect(0, 0, mRect.width, mRect.height), PR_FALSE);
        }
      }
      else {        
        // Stop the earlier image load
        mImageLoader.StopLoadImage(aPresContext);

        mCanSendLoadEvent = PR_TRUE;

        // Update the URL and start the new image load
        mImageLoader.UpdateURLSpec(aPresContext, newSRC);
      }
    }
  }
  else if (nsHTMLAtoms::width == aAttribute || nsHTMLAtoms::height == aAttribute)
  { // XXX: could check for new width == old width, and make that a no-op
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    mState |= NS_FRAME_IS_DIRTY;
	  mParent->ReflowDirtyChild(presShell, (nsIFrame*) this);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::imageFrame;
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP 
nsImageFrame::GetIntrinsicImageSize(nsSize& aSize)
{
  mImageLoader.GetIntrinsicSize(aSize);
  return NS_OK;
}

NS_IMETHODIMP
nsImageFrame::GetNaturalImageSize(PRUint32* naturalWidth, 
                                  PRUint32 *naturalHeight)
{ 
  mImageLoader.GetNaturalImageSize(naturalWidth, naturalHeight);
  return NS_OK;
}

NS_IMETHODIMP 
nsImageFrame::IsImageComplete(PRBool* aComplete)
{
  NS_ENSURE_ARG_POINTER(aComplete);
  *aComplete = ((mImageLoader.GetLoadStatus() & NS_IMAGE_LOAD_STATUS_IMAGE_READY) != 0);
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsImageFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum;
  mImageLoader.SizeOf(aHandler, &sum);
  sum += sizeof(*this) - sizeof(mImageLoader);
  if (mImageMap) {
    PRBool recorded;
    aHandler->RecordObject((void*) mImageMap, &recorded);
    if (!recorded) {
      PRUint32 mapSize;
      mImageMap->SizeOf(aHandler, &mapSize);
      aHandler->AddSize(nsLayoutAtoms::imageMap, mapSize);
    }
  }
  *aResult = sum;
  return NS_OK;
}
#endif
