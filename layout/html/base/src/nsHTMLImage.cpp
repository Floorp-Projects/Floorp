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
#include "nsHTMLTagContent.h"
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
#include "nsIImageMap.h"
#include "nsILinkHandler.h"
#include "nsIURL.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsCSSLayout.h"
#include "nsHTMLBase.h"
#include "prprf.h"
#include "nsISizeOfHandler.h"
#include "nsIFontMetrics.h"
#include "nsCSSRendering.h"
#include "nsIDOMHTMLImageElement.h"

#define BROKEN_IMAGE_URL "resource:/res/html/broken-image.gif"

#define XP_IS_SPACE(_ch) \
  (((_ch) == ' ') || ((_ch) == '\t') || ((_ch) == '\n'))

// XXX image frame layout can be 100% decoupled from the content
// object; all it needs are attributes to work properly

static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);

#define nsHTMLImageSuper nsHTMLTagContent
class nsHTMLImage : public nsHTMLImageSuper, public nsIDOMHTMLImageElement {
public:
  nsHTMLImage(nsIAtom* aTag);

  NS_DECL_ISUPPORTS

  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;
  virtual nsresult CreateFrame(nsIPresContext* aPresContext,
                               nsIFrame* aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*& aResult);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);
  virtual void MapAttributesInto(nsIStyleContext* aContext, 
                                 nsIPresContext* aPresContext);

  NS_FORWARD_IDOMNODE(nsHTMLImageSuper::)
  NS_FORWARD_IDOMELEMENT(nsHTMLImageSuper::)
  NS_FORWARD_IDOMHTMLELEMENT(nsHTMLImageSuper::)

  NS_DECL_IDOMHTMLIMAGEELEMENT

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);

protected:
  virtual ~nsHTMLImage();
  void SizeOfWithoutThis(nsISizeOfHandler* aHandler) const;

  virtual nsContentAttr AttributeToString(nsIAtom* aAttribute,
                                          nsHTMLValue& aValue,
                                          nsString& aResult) const;
  void TriggerReflow();
};

#define ImageFrameSuper nsLeafFrame
class ImageFrame : public ImageFrameSuper {
public:
  ImageFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);
  NS_METHOD HandleEvent(nsIPresContext& aPresContext,
                        nsGUIEvent* aEvent,
                        nsEventStatus& aEventStatus);
  NS_IMETHOD GetCursorAndContentAt(nsIPresContext& aPresContext,
                         const nsPoint& aPoint,
                         nsIFrame** aFrame,
                         nsIContent** aContent,
                         PRInt32& aCursor);
  NS_IMETHOD ContentChanged(nsIPresShell*   aShell,
                            nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent);

protected:
  virtual ~ImageFrame();
  void SizeOfWithoutThis(nsISizeOfHandler* aHandler) const;

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize);

  nsIImageMap* GetImageMap();

  nsHTMLImageLoader mImageLoader;
  nsIImageMap* mImageMap;
  PRBool mSizeFrozen;

  void TriggerLink(nsIPresContext& aPresContext,
                   const nsString& aURLSpec,
                   const nsString& aTargetSpec,
                   PRBool aClick);

  PRBool IsServerImageMap();
  PRIntn GetSuppress();

  nscoord MeasureString(nsIFontMetrics*  aFontMetrics,
                        const PRUnichar* aString,
                        PRInt32          aLength,
                        nscoord          aMaxWidth,
                        PRUint32&        aMaxFit);

  void DisplayAltText(nsIPresContext&      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsString&      aAltText,
                      const nsRect&        aRect);
};

// Value's for mSuppress
#define SUPPRESS_UNSET   0
#define DONT_SUPPRESS    1
#define SUPPRESS         2
#define DEFAULT_SUPPRESS 3

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET PRUint8(-1)

//----------------------------------------------------------------------

nsHTMLImageLoader::nsHTMLImageLoader()
{
  mImageLoader = nsnull;
  mLoadImageFailed = PR_FALSE;
  mLoadBrokenImageFailed = PR_FALSE;
  mURLSpec = nsnull;
  mBaseHREF = nsnull;
}

nsHTMLImageLoader::~nsHTMLImageLoader()
{
  NS_IF_RELEASE(mImageLoader);
  if (nsnull != mURLSpec) {
    delete mURLSpec;
  }
  if (nsnull != mBaseHREF) {
    delete mBaseHREF;
  }
}

void
nsHTMLImageLoader::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  if (!aHandler->HaveSeen(mURLSpec)) {
    mURLSpec->SizeOf(aHandler);
  }
  if (!aHandler->HaveSeen(mImageLoader)) {
    mImageLoader->SizeOf(aHandler);
  }
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
nsHTMLImageLoader::SetURL(const nsString& aURLSpec)
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
nsHTMLImageLoader::SetBaseHREF(const nsString& aBaseHREF)
{
  if (nsnull != mBaseHREF) {
    delete mBaseHREF;
  }
  mBaseHREF = new nsString(aBaseHREF);
  if (nsnull == mBaseHREF) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
nsHTMLImageLoader::StartLoadImage(nsIPresContext* aPresContext,
                                  nsIFrame* aForFrame,
                                  PRBool aNeedSizeUpdate,
                                  PRIntn& aLoadStatus)
{
  aLoadStatus = NS_IMAGE_LOAD_STATUS_NONE;

  // Get absolute url the first time through
  nsresult rv;
  nsAutoString src;
  if (mLoadImageFailed || (nsnull == mURLSpec)) {
    src.Append(BROKEN_IMAGE_URL);
  } else {
    nsAutoString baseURL;
    if (nsnull != mBaseHREF) {
      baseURL = *mBaseHREF;
    }

    // Get documentURL
    nsIPresShell* shell;
    shell = aPresContext->GetShell();
    nsIDocument* doc = shell->GetDocument();
    nsIURL* docURL = doc->GetDocumentURL();

    // Create an absolute URL
    nsresult rv = NS_MakeAbsoluteURL(docURL, baseURL, *mURLSpec, src);

    // Release references
    NS_RELEASE(shell);
    NS_RELEASE(docURL);
    NS_RELEASE(doc);
    if (NS_OK != rv) {
      return rv;
    }
  }

  if (nsnull == mImageLoader) {
    // Start image loading. Note that we don't specify a background color
    // so transparent images are always rendered using a transparency mask
    rv = aPresContext->StartLoadImage(src, nsnull, aForFrame, aNeedSizeUpdate,
                                      mImageLoader);
    if (NS_OK != rv) {
      return rv;
    }
  }

  // Examine current image load status
  mImageLoader->GetImageLoadStatus(aLoadStatus);
  if (0 != (aLoadStatus & NS_IMAGE_LOAD_STATUS_ERROR)) {
    NS_RELEASE(mImageLoader);
    if (mLoadImageFailed) {
      // We are doomed. Loading the broken image has just failed.
      mLoadBrokenImageFailed = PR_TRUE;
    }
    else {
      // Try again, this time using the broke-image url
      mLoadImageFailed = PR_TRUE;
      return StartLoadImage(aPresContext, aForFrame, aNeedSizeUpdate, aLoadStatus);
    }
  }
  return NS_OK;
}

void
nsHTMLImageLoader::GetDesiredSize(nsIPresContext* aPresContext,
                                  const nsReflowState& aReflowState,
                                  nsReflowMetrics& aDesiredSize)
{
  nsSize styleSize;
  PRIntn ss = nsCSSLayout::GetStyleSize(aPresContext, aReflowState, styleSize);
  PRIntn loadStatus;
  if (0 != ss) {
    if (NS_SIZE_HAS_BOTH == ss) {
      StartLoadImage(aPresContext, aReflowState.frame, PR_FALSE, loadStatus);
      aDesiredSize.width = styleSize.width;
      aDesiredSize.height = styleSize.height;
    }
    else {
      // Preserve aspect ratio of image with unbound dimension.
      StartLoadImage(aPresContext, aReflowState.frame, PR_TRUE, loadStatus);
      if ((0 == (loadStatus & NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE)) ||
          (nsnull == mImageLoader)) {
        // Provide a dummy size for now; later on when the image size
        // shows up we will reflow to the new size.
        aDesiredSize.width = 1;
        aDesiredSize.height = 1;
      }
      else {
        float p2t = aPresContext->GetPixelsToTwips();
        nsSize imageSize;
        mImageLoader->GetSize(imageSize);
        float imageWidth = imageSize.width * p2t;
        float imageHeight = imageSize.height * p2t;
        if (0.0f != imageHeight) {
          if (0 != (ss & NS_SIZE_HAS_WIDTH)) {
            // We have a width, and an auto height. Compute height
            // from width.
            aDesiredSize.width = styleSize.width;
            aDesiredSize.height =
              (nscoord)NSToIntRound(styleSize.width * imageHeight / imageWidth);
          }
          else {
            // We have a height and an auto width. Compute width from
            // height.
            aDesiredSize.height = styleSize.height;
            aDesiredSize.width =
              (nscoord)NSToIntRound(styleSize.height * imageWidth / imageHeight);
          }
        }
        else {
          // Screwy image
          aDesiredSize.width = 1;
          aDesiredSize.height = 1;
        }
      }
    }
  }
  else {
    StartLoadImage(aPresContext, aReflowState.frame, PR_TRUE, loadStatus);
    if ((0 == (loadStatus & NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE)) ||
        (nsnull == mImageLoader)) {
      // Provide a dummy size for now; later on when the image size
      // shows up we will reflow to the new size.
      aDesiredSize.width = 1;
      aDesiredSize.height = 1;
      printf ("in image loader, dummy size of 1 returned\n");
    } else {
      float p2t = aPresContext->GetPixelsToTwips();
      nsSize imageSize;
      mImageLoader->GetSize(imageSize);
      aDesiredSize.width = NSIntPixelsToTwips(imageSize.width, p2t);
      aDesiredSize.height = NSIntPixelsToTwips(imageSize.height, p2t);
      printf ("in image loader, real size of %d returned\n", aDesiredSize.width);
    }
  }
}

//----------------------------------------------------------------------

ImageFrame::ImageFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsLeafFrame(aContent, aParentFrame)
{
}

ImageFrame::~ImageFrame()
{
}

NS_METHOD
ImageFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  NS_IF_RELEASE(mImageMap);

  // Release image loader first so that it's refcnt can go to zero
  mImageLoader.DestroyLoader();

  return nsLeafFrame::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
ImageFrame::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  ImageFrame::SizeOfWithoutThis(aHandler);
  return NS_OK;
}

void
ImageFrame::SizeOfWithoutThis(nsISizeOfHandler* aHandler) const
{
  ImageFrameSuper::SizeOfWithoutThis(aHandler);
  mImageLoader.SizeOf(aHandler);
  if (!aHandler->HaveSeen(mImageMap)) {
    mImageMap->SizeOf(aHandler);
  }
}

void
ImageFrame::GetDesiredSize(nsIPresContext* aPresContext,
                           const nsReflowState& aReflowState,
                           nsReflowMetrics& aDesiredSize)
{
  if (mSizeFrozen) {
    aDesiredSize.width = mRect.width;
    aDesiredSize.height = mRect.height;
  }
  else {
    // XXX Don't create a view, because we want whatever is below the image
    // to show through while the image is loading; Likewise for transparent
    // images and broken images
    //
    // What we really want to do is to create a view, and indicate that the
    // view has a transparent content area. Do this while it's loading,
    // and then when it's fully loaded mark the view as opaque if the
    // image is opaque.
    //
    // We can't use that approach yet, because currently the compositor doesn't
    // support transparent views...
#if 0
    nsHTMLBase::CreateViewForFrame(aPresContext, this, mStyleContext, PR_TRUE);
#endif

    // Setup url before starting the image load
    nsAutoString src, base;
    if (eContentAttr_HasValue == mContent->GetAttribute("SRC", src)) {
      mImageLoader.SetURL(src);
      if (eContentAttr_HasValue ==
          mContent->GetAttribute(NS_HTML_BASE_HREF, base)) {
        mImageLoader.SetBaseHREF(base);
      }
    }
    mImageLoader.GetDesiredSize(aPresContext, aReflowState, aDesiredSize);
  }
}

// Computes the width of the specified string. aMaxWidth specifies the maximum
// width available. Once this limit is reached no more characters are measured.
// The number of characters that fit within the maximum width are returned in
// aMaxFit
nscoord
ImageFrame::MeasureString(nsIFontMetrics*  aFontMetrics,
                          const PRUnichar* aString,
                          PRInt32          aLength,
                          nscoord          aMaxWidth,
                          PRUint32&        aMaxFit)
{
  nscoord totalWidth = 0;
  nscoord spaceWidth;
  aFontMetrics->GetWidth(' ', spaceWidth);

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
    aFontMetrics->GetWidth(aString, len, width);
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

  return totalWidth;
}

// Formats the alt-text to fit within the specified rectangle. Breaks lines
// between words if a word would extend past the edge of the rectangle
void
ImageFrame::DisplayAltText(nsIPresContext&      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsString&      aAltText,
                           const nsRect&        aRect)
{
  // Clip so we don't render outside of the rect.
  aRenderingContext.PushState();
  aRenderingContext.SetClipRect(aRect, nsClipCombine_kIntersect);

  const nsStyleColor* color =
    (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
  const nsStyleFont* font =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

  // Set font and color
  aRenderingContext.SetColor(color->mColor);
  aRenderingContext.SetFont(font->mFont);

  // Format the text to display within the formatting rect
  nsIFontMetrics* fm = aRenderingContext.GetFontMetrics();

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
    nscoord   width = MeasureString(fm, str, strLen, aRect.width, maxFit);
    
    // Display the text
    aRenderingContext.DrawString(str, maxFit, aRect.x, y, 0);

    // Move to the next line
    str += maxFit;
    strLen -= maxFit;
    y += height;
  }

  NS_RELEASE(fm);
  aRenderingContext.PopState();
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

NS_METHOD
ImageFrame::Paint(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect)
{
  if ((0 == mRect.width) || (0 == mRect.height)) {
    // Do not render when given a zero area. This avoids some useless
    // scaling work while we wait for our image dimensions to arrive
    // asynchronously.
    return NS_OK;
  }

  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible) {
    // First paint background and borders
    nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);

    nsIImage* image = mImageLoader.GetImage();
    if (nsnull == image) {
      // No image yet. Draw the icon that indicates we're loading, and display
      // the alt-text
      nsAutoString altText;
      if (eContentAttr_HasValue == mContent->GetAttribute("ALT", altText)) {
        // Display a recessed one-pixel border in the inner area
        nsRect  inner;
        GetInnerArea(&aPresContext, inner);

        float p2t = aPresContext.GetPixelsToTwips();
        nsRecessedBorder recessedBorder(NSIntPixelsToTwips(1, p2t));
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this, inner,
                                    inner, recessedBorder, 0);
        inner.Deflate(NSIntPixelsToTwips(1, p2t), NSIntPixelsToTwips(1, p2t));

        // Leave a 8 pixel left/right padding, and a 5 pixel top/bottom padding
        inner.Deflate(NSIntPixelsToTwips(8, p2t), NSIntPixelsToTwips(5, p2t));

        // If there's room, then display the alt-text
        if (!inner.IsEmpty()) {
          DisplayAltText(aPresContext, aRenderingContext, altText, inner);
        }
      }
      return NS_OK;
    }

    // Now render the image into our inner area (the area without the
    // borders and padding)
    nsRect inner;
    GetInnerArea(&aPresContext, inner);
    if (mImageLoader.GetLoadImageFailed()) {
      float p2t = aPresContext.GetPixelsToTwips();
      inner.width = NSIntPixelsToTwips(image->GetWidth(), p2t);
      inner.height = NSIntPixelsToTwips(image->GetHeight(), p2t);
    }
    aRenderingContext.DrawImage(image, inner);

    if (GetShowFrameBorders()) {
      nsIImageMap* map = GetImageMap();
      if (nsnull != map) {
        aRenderingContext.SetColor(NS_RGB(0, 0, 0));
        aRenderingContext.PushState();
        aRenderingContext.Translate(inner.x, inner.y);
          map->Draw(aPresContext, aRenderingContext);
          aRenderingContext.PopState();
      }
    }
  }

  return NS_OK;
}

nsIImageMap*
ImageFrame::GetImageMap()
{
  if (nsnull == mImageMap) {
    nsAutoString usemap;
    mContent->GetAttribute("usemap", usemap);
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
    if (NS_OK == rv) {
      nsIImageMap* map;
      rv = hdoc->GetImageMap(usemap, &map);
      NS_RELEASE(hdoc);
      if (NS_OK == rv) {
        mImageMap = map;
      }
    }
  }
  NS_IF_ADDREF(mImageMap);
  return mImageMap;
}

void
ImageFrame::TriggerLink(nsIPresContext& aPresContext,
                        const nsString& aURLSpec,
                        const nsString& aTargetSpec,
                        PRBool aClick)
{
  nsILinkHandler* handler = nsnull;
  aPresContext.GetLinkHandler(&handler);
  if (nsnull != handler) {
    if (aClick) {
      handler->OnLinkClick(this, aURLSpec, aTargetSpec);
    }
    else {
      handler->OnOverLink(this, aURLSpec, aTargetSpec);
    }
  }
}

PRBool
ImageFrame::IsServerImageMap()
{
  nsAutoString ismap;
  return eContentAttr_HasValue == mContent->GetAttribute("ismap", ismap);
}

PRIntn
ImageFrame::GetSuppress()
{
  nsAutoString s;
  if (eContentAttr_HasValue == mContent->GetAttribute("ismap", s)) {
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
ImageFrame::HandleEvent(nsIPresContext& aPresContext,
                        nsGUIEvent* aEvent,
                        nsEventStatus& aEventStatus)
{
  nsIImageMap* map;
  aEventStatus = nsEventStatus_eIgnore; 

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
  case NS_MOUSE_MOVE:
    map = GetImageMap();
    if ((nsnull != map) || IsServerImageMap()) {
      nsIURL* docURL = nsnull;
      nsIDocument* doc = nsnull;
      mContent->GetDocument(doc);
      if (nsnull != doc) {
        docURL = doc->GetDocumentURL();
        NS_RELEASE(doc);
      }

      // Ask map if the x,y coordinates are in a clickable area
      float t2p = aPresContext.GetTwipsToPixels();
      nsAutoString absURL, target, altText;
      PRBool suppress;
      if (nsnull != map) {
        // Subtract out border and padding here so that we are looking
        // at the right coordinates. Hit detection against area tags
        // is done after the mouse wanders over the image, not over
        // the image's borders.
        nsRect inner;
        GetInnerArea(&aPresContext, inner);
        PRInt32 x = NSTwipsToIntPixels((aEvent->point.x - inner.x), t2p);
        PRInt32 y = NSTwipsToIntPixels((aEvent->point.y - inner.y), t2p);
        nsresult r = map->IsInside(x, y, docURL, absURL, target, altText,
                                   &suppress);
        NS_IF_RELEASE(docURL);
        NS_RELEASE(map);
        if (NS_OK == r) {
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
        nsAutoString baseURL;
        mContent->GetAttribute(NS_HTML_BASE_HREF, baseURL);
        nsAutoString src;
        mContent->GetAttribute("src", src);
        NS_MakeAbsoluteURL(docURL, baseURL, src, absURL);

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
ImageFrame::GetCursorAndContentAt(nsIPresContext& aPresContext,
                                  const nsPoint& aPoint,
                                  nsIFrame** aFrame,
                                  nsIContent** aContent,
                                  PRInt32& aCursor)
{
  // The default cursor is to have no cursor
  aCursor = NS_STYLE_CURSOR_INHERIT;
  *aContent = mContent;

  const nsStyleColor* styleColor = (const nsStyleColor*)
    mStyleContext->GetStyleData(eStyleStruct_Color);
  if (styleColor->mCursor != NS_STYLE_CURSOR_INHERIT) {
    // If we have a particular cursor, use it
    *aFrame = this;
    aCursor = (PRInt32) styleColor->mCursor;
  }

  nsIImageMap* map = GetImageMap();
  if (nsnull != map) {
    nsRect inner;
    GetInnerArea(&aPresContext, inner);
    aCursor = NS_STYLE_CURSOR_DEFAULT;
    float t2p = aPresContext.GetTwipsToPixels();
    PRInt32 x = NSTwipsToIntPixels((aPoint.x - inner.x), t2p);
    PRInt32 y = NSTwipsToIntPixels((aPoint.y - inner.y), t2p);
    if (NS_OK == map->IsInside(x, y)) {
      aCursor = NS_STYLE_CURSOR_HAND;
    }
    NS_RELEASE(map);
  }

  return NS_OK;
}

NS_IMETHODIMP
ImageFrame::ContentChanged(nsIPresShell*   aShell,
                           nsIPresContext* aPresContext,
                           nsIContent*     aChild,
                           nsISupports*    aSubContent)
{
  // See if the src attribute changed; if it did then trigger a redraw
  // by firing up a new image load request. Otherwise let our base
  // class handle the content-changed request.
  nsAutoString oldSRC;
  mImageLoader.GetURL(oldSRC);

  // Get src attribute's value and construct a new absolute url from it
  nsAutoString newSRC;
  if (eContentAttr_HasValue == mContent->GetAttribute("SRC", newSRC)) {
    if (!oldSRC.Equals(newSRC)) {
      mSizeFrozen = PR_TRUE;

#ifdef NS_DEBUG
      char oldcbuf[100], newcbuf[100];
      oldSRC.ToCString(oldcbuf, sizeof(oldcbuf));
      newSRC.ToCString(newcbuf, sizeof(newcbuf));
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
         ("ImageFrame::ContentChanged: new image source; old='%s' new='%s'",
          oldcbuf, newcbuf));
#endif

      // Get rid of old image loader and start a new image load going
      mImageLoader.DestroyLoader();

      // Fire up a new image load request
      PRIntn loadStatus;
      mImageLoader.SetURL(newSRC);
      mImageLoader.StartLoadImage(aPresContext, this, PR_FALSE, loadStatus);

      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
         ("ImageFrame::ContentChanged: loadImage status=%x",
          loadStatus));

      // If the image is already ready then we need to trigger a
      // redraw because the image loader won't.
      if (loadStatus & NS_IMAGE_LOAD_STATUS_IMAGE_READY) {
        // XXX Stuff this into a method on nsIPresShell/Context
        nsRect bounds;
        nsPoint offset;
        nsIView* view;
        GetOffsetFromView(offset, view);
        nsIViewManager* vm = view->GetViewManager();
        bounds.x = offset.x;
        bounds.y = offset.y;
        bounds.width = mRect.width;
        bounds.height = mRect.height;
        vm->UpdateView(view, bounds, 0);
        NS_RELEASE(vm);
      }

      return NS_OK;
    }
  }

  return ImageFrameSuper::ContentChanged(aShell, aPresContext, aChild,
                                         aSubContent);
}

//----------------------------------------------------------------------

nsHTMLImage::nsHTMLImage(nsIAtom* aTag)
  : nsHTMLTagContent(aTag)
{
}

nsHTMLImage::~nsHTMLImage()
{
}

static NS_DEFINE_IID(kIDOMHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);

nsresult nsHTMLImage::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult res = nsHTMLTagContent::QueryInterface(aIID, aInstancePtr); 
  if (NS_NOINTERFACE == res) {
    if (aIID.Equals(kIDOMHTMLImageElementIID)) {
      *aInstancePtr = (void*)(nsIDOMHTMLImageElement*)this;
      AddRef();
      return NS_OK;
    }
  }
  
  return res;
}

nsrefcnt nsHTMLImage::AddRef(void)
{
  return nsHTMLTagContent::AddRef(); 
}

nsrefcnt nsHTMLImage::Release(void)
{
  return nsHTMLTagContent::Release(); 
}


NS_IMETHODIMP
nsHTMLImage::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  nsHTMLImage::SizeOfWithoutThis(aHandler);
  return NS_OK;
}

void
nsHTMLImage::SizeOfWithoutThis(nsISizeOfHandler* aHandler) const
{
}

void
nsHTMLImage::SetAttribute(nsIAtom* aAttribute, const nsString& aString)
{
  nsHTMLValue val;
  if (aAttribute == nsHTMLAtoms::ismap) {
    val.SetEmptyValue();
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::usemap) {
    nsAutoString usemap(aString);
    usemap.StripWhitespace();
    val.SetStringValue(usemap);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (ParseAlignParam(aString, val)) {
      // Reflect the attribute into the syle system
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
    else {
      val.SetStringValue(aString);
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
  }
  else if (aAttribute == nsHTMLAtoms::src) {
    nsAutoString src(aString);
    src.StripWhitespace();
    val.SetStringValue(src);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    TriggerReflow();
  }
  else if (aAttribute == nsHTMLAtoms::lowsrc) {
    nsAutoString lowsrc(aString);
    lowsrc.StripWhitespace();
    val.SetStringValue(lowsrc);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::alt) {
    val.SetStringValue(aString);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::suppress) {
    PRIntn suppress = DEFAULT_SUPPRESS;
    if (aString.EqualsIgnoreCase("true")) {
      suppress = SUPPRESS;
    }
    else if (aString.EqualsIgnoreCase("false")) {
      suppress = DONT_SUPPRESS;
    }
    val.SetIntValue(suppress, eHTMLUnit_Enumerated);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (ParseImageProperty(aAttribute, aString, val)) {
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else {
    // Use default attribute catching code
    nsHTMLImageSuper::SetAttribute(aAttribute, aString);
  }
}

nsContentAttr
nsHTMLImage::AttributeToString(nsIAtom* aAttribute,
                               nsHTMLValue& aValue,
                               nsString& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      AlignParamToString(aValue, aResult);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::suppress) {
    aResult.Truncate();
    switch (aValue.GetIntValue()) {
    case SUPPRESS:         aResult.Append("true"); break;
    case DONT_SUPPRESS:    aResult.Append("false"); break;
    case DEFAULT_SUPPRESS: break;
    }
    ca = eContentAttr_HasValue;
  }
  else if (ImagePropertyToString(aAttribute, aValue, aResult)) {
    ca = eContentAttr_HasValue;
  }
  return ca;
}

void
nsHTMLImage::MapAttributesInto(nsIStyleContext* aContext, 
                               nsIPresContext* aPresContext)
{
  if (nsnull != mAttributes) {
    nsHTMLValue value;
    GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      PRUint8 align = value.GetIntValue();
      nsStyleDisplay* display = (nsStyleDisplay*)
        aContext->GetMutableStyleData(eStyleStruct_Display);
      nsStyleText* text = (nsStyleText*)
        aContext->GetMutableStyleData(eStyleStruct_Text);
      nsStyleSpacing* spacing = (nsStyleSpacing*)
        aContext->GetMutableStyleData(eStyleStruct_Spacing);
      float p2t = aPresContext->GetPixelsToTwips();
      nsStyleCoord three(NSIntPixelsToTwips(3, p2t));
      switch (align) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        display->mFloats = NS_STYLE_FLOAT_LEFT;
        spacing->mMargin.SetLeft(three);
        spacing->mMargin.SetRight(three);
        break;
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        display->mFloats = NS_STYLE_FLOAT_RIGHT;
        spacing->mMargin.SetLeft(three);
        spacing->mMargin.SetRight(three);
        break;
      default:
        text->mVerticalAlign.SetIntValue(align, eStyleUnit_Enumerated);
        break;
      }
    }
  }
  MapImagePropertiesInto(aContext, aPresContext);
  MapImageBorderInto(aContext, aPresContext, nsnull);
}

void
nsHTMLImage::TriggerReflow()
{
  nsIDocument* doc = mDocument;
  if (nsnull != doc) {
    doc->ContentChanged(this, nsnull);
  }
}

nsresult
nsHTMLImage::CreateFrame(nsIPresContext* aPresContext,
                         nsIFrame* aParentFrame,
                         nsIStyleContext* aStyleContext,
                         nsIFrame*& aResult)
{
  ImageFrame* frame = new ImageFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  frame->SetStyleContext(aPresContext, aStyleContext);
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetLowSrc(nsString& aLowSrc)
{
  GetAttribute(nsHTMLAtoms::lowsrc, aLowSrc);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetLowSrc(const nsString& aLowSrc)
{
  SetAttribute(nsHTMLAtoms::lowsrc, aLowSrc);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetName(nsString& aName)
{
  GetAttribute(nsHTMLAtoms::name, aName);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetName(const nsString& aName)
{
  SetAttribute(nsHTMLAtoms::name, aName);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetAlign(nsString& aAlign)
{
  GetAttribute(nsHTMLAtoms::align, aAlign);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetAlign(const nsString& aAlign)
{
  SetAttribute(nsHTMLAtoms::align, aAlign);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetAlt(nsString& aAlt)
{
  GetAttribute(nsHTMLAtoms::alt, aAlt);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetAlt(const nsString& aAlt)
{
  SetAttribute(nsHTMLAtoms::alt, aAlt);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetBorder(nsString& aBorder)
{
  GetAttribute(nsHTMLAtoms::border, aBorder);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetBorder(const nsString& aBorder)
{
  SetAttribute(nsHTMLAtoms::border, aBorder);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetHeight(nsString& aHeight)
{
  GetAttribute(nsHTMLAtoms::height, aHeight);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetHeight(const nsString& aHeight)
{
  SetAttribute(nsHTMLAtoms::height, aHeight);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetHspace(nsString& aHspace)
{
  GetAttribute(nsHTMLAtoms::hspace, aHspace);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetHspace(const nsString& aHspace)
{
  SetAttribute(nsHTMLAtoms::hspace, aHspace);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetIsMap(PRBool* aIsMap)
{
  nsAutoString result;

  *aIsMap = (PRBool)(eContentAttr_HasValue == GetAttribute(nsHTMLAtoms::ismap, result)); 

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetIsMap(PRBool aIsMap)
{
  if (PR_TRUE == aIsMap) {
    SetAttribute(nsHTMLAtoms::ismap, "");
  }
  else {
    UnsetAttribute(nsHTMLAtoms::ismap);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetLongDesc(nsString& aLongDesc)
{
  GetAttribute(nsHTMLAtoms::longdesc, aLongDesc);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetLongDesc(const nsString& aLongDesc)
{
  SetAttribute(nsHTMLAtoms::longdesc, aLongDesc);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetSrc(nsString& aSrc)
{
  GetAttribute(nsHTMLAtoms::src, aSrc);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetSrc(const nsString& aSrc)
{
  SetAttribute(nsHTMLAtoms::src, aSrc);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetUseMap(nsString& aUseMap)
{
  GetAttribute(nsHTMLAtoms::usemap, aUseMap);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetUseMap(const nsString& aUseMap)
{
  SetAttribute(nsHTMLAtoms::usemap, aUseMap);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetVspace(nsString& aVspace)
{
  GetAttribute(nsHTMLAtoms::vspace, aVspace);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetVspace(const nsString& aVspace)
{
  SetAttribute(nsHTMLAtoms::vspace, aVspace);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::GetWidth(nsString& aWidth)
{
  GetAttribute(nsHTMLAtoms::width, aWidth);

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLImage::SetWidth(const nsString& aWidth)
{
  SetAttribute(nsHTMLAtoms::width, aWidth);

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLImage::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptHTMLImageElement(aContext, this, mParent, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;  
}

nsresult
NS_NewHTMLImage(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* img = new nsHTMLImage(aTag);
  if (nsnull == img) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return img->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
