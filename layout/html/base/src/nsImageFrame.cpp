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
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "prprf.h"
#include "nsIFontMetrics.h"
#include "nsCSSRendering.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDeviceContext.h"
#include "nsINameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIStyleSet.h"

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
  mImageLoader.StopAllLoadImages(&aPresContext);

  return nsLeafFrame::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
nsImageFrame::Init(nsIPresContext&  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsLeafFrame::Init(aPresContext, aContent, aParent,
                                   aContext, aPrevInFlow);

#if 0
  // See if we have a SRC attribute
  PRBool sourcePresent = PR_FALSE;
  nsAutoString src;
  nsresult ca;
  ca = mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, src);
  if ((NS_CONTENT_ATTR_HAS_VALUE == ca) && (src.Length() > 0)) {
    sourcePresent = PR_TRUE;
  }
  else {
    // XXX ick: this should only work for OBJECT tags
    ca = mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::data, src);
    if ((NS_CONTENT_ATTR_HAS_VALUE == ca) && (src.Length() > 0)) {
      sourcePresent = PR_TRUE;
    }
  }
#else
  nsAutoString src;
  mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, src);
#endif

  // Set the image loader's source URL and base URL
  nsIURL* baseURL = nsnull;
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
  mImageLoader.Init(this, UpdateImageFrame, nsnull, baseURL, src);
  NS_IF_RELEASE(baseURL);

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
  return frame->UpdateImage(aPresContext, aStatus);
}

nsresult
nsImageFrame::UpdateImage(nsIPresContext* aPresContext, PRUint32 aStatus)
{
#ifdef NOISY_IMAGE_LOADING
  ListTag(stdout);
  printf(": UpdateImage: status=%x\n", aStatus);
#endif
  if (NS_IMAGE_LOAD_STATUS_ERROR & aStatus) {
    // We failed to load the image. Notify the pres shell
    nsIPresShell* presShell;
    aPresContext->GetShell(&presShell);
    if (presShell) {
      presShell->CantRenderReplacedElement(aPresContext, this);
      NS_RELEASE(presShell);
    }
  }
  else if (NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    // Now that the size is available, trigger a content-changed reflow
    if (nsnull != mContent) {
      nsIDocument* document = nsnull;
      mContent->GetDocument(document);
      if (nsnull != document) {
        document->ContentChanged(mContent, nsnull);
        NS_RELEASE(document);
      }
    }
  }
  return NS_OK;
}

void
nsImageFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  if (mImageLoader.GetDesiredSize(aPresContext, &aReflowState, aDesiredSize)) {
    // We have our "final" desired size. Cancel any pending
    // incremental reflows aimed at this frame.
    nsIPresShell* shell;
    aPresContext->GetShell(&shell);
    if (shell) {
      shell->CancelReflowCommand(this);
      NS_RELEASE(shell);
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
  if (disp->mVisible && mRect.width && mRect.height) {
    if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
      aRenderingContext.PushState();
      SetClipRect(aRenderingContext);
    }

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
    }
    else {
      if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
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

      if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) &&
          GetShowFrameBorders()) {
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

    if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
      PRBool clipState;
      aRenderingContext.PopState(clipState);
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
            aEventStatus = nsEventStatus_eConsumeDoDefault; 
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
          aEventStatus = nsEventStatus_eConsumeDoDefault; 
          clicked = PR_TRUE;
        }
        TriggerLink(aPresContext, absURL, target, clicked);
      }
      break;
    }
    default:
      break;
  }

  return nsLeafFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
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
    nsAutoString oldSRC, newSRC;
    mImageLoader.GetURLSpec(oldSRC);
    aChild->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, newSRC);
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
          Invalidate(nsRect(0, 0, mRect.width, mRect.height), PR_FALSE);
        }
      }
      else {
        // Force a reflow when the image size isn't already known
        if (nsnull != mContent) {
          nsIDocument* document = nsnull;
          mContent->GetDocument(document);
          if (nsnull != document) {
            document->ContentChanged(mContent, nsnull);
            NS_RELEASE(document);
          }
        }
      }
    }
  }

  return NS_OK;
}
