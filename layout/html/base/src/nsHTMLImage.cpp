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
#include "nsHTMLTagContent.h"
#include "nsString.h"
#include "nsLeafFrame.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
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

static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kStyleColorSID, NS_STYLECOLOR_SID);
static NS_DEFINE_IID(kStyleDisplaySID, NS_STYLEDISPLAY_SID);
static NS_DEFINE_IID(kStyleTextSID, NS_STYLETEXT_SID);

class ImageFrame : public nsLeafFrame {
public:
  ImageFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_METHOD HandleEvent(nsIPresContext& aPresContext,
                        nsGUIEvent* aEvent,
                        nsEventStatus& aEventStatus);

  NS_IMETHOD GetCursorAt(nsIPresContext& aPresContext,
                         const nsPoint& aPoint,
                         nsIFrame** aFrame,
                         PRInt32& aCursor);

protected:
  virtual ~ImageFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              nsReflowMetrics& aDesiredSize,
                              const nsSize& aMaxSize);

  nsIImage* GetImage(nsIPresContext& aPresContext);

  nsIImageMap* GetImageMap();

  void TriggerLink(nsIPresContext& aPresContext,
                   const nsString& aURLSpec,
                   const nsString& aTargetSpec);
};

class ImagePart : public nsHTMLTagContent {
public:
  ImagePart(nsIAtom* aTag);

  virtual nsresult CreateFrame(nsIPresContext* aPresContext,
                               nsIFrame* aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*& aResult);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);
  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aResult) const;
  virtual void UnsetAttribute(nsIAtom* aAttribute);

  virtual void MapAttributesInto(nsIStyleContext* aContext, 
                                 nsIPresContext* aPresContext);

  PRPackedBool mIsMap;
  PRUint8 mSuppress;
  PRUint8 mAlign;
  nsString* mAltText;
  nsString* mSrc;
  nsString* mLowSrc;
  nsString* mUseMap;

protected:
  virtual ~ImagePart();

  virtual nsContentAttr AttributeToString(nsIAtom* aAttribute,
                                          nsHTMLValue& aValue,
                                          nsString& aResult) const;
};

// Value's for mSuppress
#define SUPPRESS_UNSET   0
#define DONT_SUPPRESS    1
#define SUPPRESS         2
#define DEFAULT_SUPPRESS 3

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET PRUint8(-1)

//----------------------------------------------------------------------

ImageFrame::ImageFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsLeafFrame(aContent, aParentFrame)
{
}

ImageFrame::~ImageFrame()
{
}

nsIImage* ImageFrame::GetImage(nsIPresContext& aPresContext)
{
  nsAutoString src;
  if (eContentAttr_HasValue == mContent->GetAttribute("SRC", src)) {
    return aPresContext.LoadImage(src, this);
  }
  return nsnull;
}

NS_METHOD ImageFrame::Paint(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
  nsIImage* image = GetImage(aPresContext);
  if (nsnull == image) {
    return NS_OK;
  }

  // First paint background and borders
  nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);

  // Now render the image into our inner area (the area without the
  // borders and padding)
  nsRect inner;
  GetInnerArea(&aPresContext, inner);
  aRenderingContext.DrawImage(image, inner);

  if (GetShowFrameBorders()) {
    nsIImageMap* map = GetImageMap();
    if (nsnull != map) {
      aRenderingContext.SetColor(NS_RGB(0, 0, 0));
      map->Draw(aPresContext, aRenderingContext);
    }
  }

  return NS_OK;
}

nsIImageMap* ImageFrame::GetImageMap()
{
  ImagePart* part = (ImagePart*)mContent;
  if (nsnull == part->mUseMap) {
    return nsnull;
  }

  nsIDocument* doc = mContent->GetDocument();
  if (nsnull == doc) {
    return nsnull;
  }

  nsAutoString mapName(*part->mUseMap);
  if (mapName.First() == '#') {
    mapName.Cut(0, 1);
  }
  nsIHTMLDocument* hdoc;
  nsresult rv = doc->QueryInterface(kIHTMLDocumentIID, (void**)&hdoc);
  NS_RELEASE(doc);
  if (NS_OK == rv) {
    nsIImageMap* map;
    rv = hdoc->GetImageMap(mapName, &map);
    NS_RELEASE(hdoc);
    if (NS_OK == rv) {
      return map;
    }
  }

  return nsnull;
}

void ImageFrame::TriggerLink(nsIPresContext& aPresContext,
                             const nsString& aURLSpec,
                             const nsString& aTargetSpec)
{
  nsILinkHandler* handler;
  if (NS_OK == aPresContext.GetLinkHandler(&handler)) {
    // Now pass on absolute url to the click handler
    handler->OnLinkClick(this, aURLSpec, aTargetSpec);
  }
}

// XXX what about transparent pixels?
NS_METHOD ImageFrame::HandleEvent(nsIPresContext& aPresContext,
                                  nsGUIEvent* aEvent,
                                  nsEventStatus& aEventStatus)
{
  aEventStatus = nsEventStatus_eIgnore; 

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
    {
      nsIImageMap* map = GetImageMap();
      if (nsnull != map) {
        nsIURL* docURL = nsnull;
        nsIDocument* doc = mContent->GetDocument();
        if (nsnull != doc) {
          docURL = doc->GetDocumentURL();
          NS_RELEASE(doc);
        }

        // Translate coordinates to pixels
        float t2p = aPresContext.GetTwipsToPixels();
        nscoord x = nscoord(t2p * aEvent->point.x);
        nscoord y = nscoord(t2p * aEvent->point.y);

        // Ask map if the x,y coordinates are in a clickable area
        PRBool suppress;
        nsAutoString absURL, target, altText;
        nsresult r = map->IsInside(x, y, docURL, absURL, target, altText,
                                   &suppress);
        NS_IF_RELEASE(docURL);
        NS_RELEASE(map);
        if (NS_OK == r) {
          // We hit a clickable area. Time to go somewhere...
          TriggerLink(aPresContext, absURL, target);
          aEventStatus = nsEventStatus_eConsumeNoDefault; 
        }
        break;
      }
    }
    // FALL THROUGH

  default:
    // Let default event handler deal with it
    return nsLeafFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }

  return NS_OK;
}

NS_METHOD ImageFrame::GetCursorAt(nsIPresContext& aPresContext,
                                  const nsPoint& aPoint,
                                  nsIFrame** aFrame,
                                  PRInt32& aCursor)
{
  // The default cursor is to have no cursor
  aCursor = NS_STYLE_CURSOR_INHERIT;

  nsStyleColor* styleColor = (nsStyleColor*)
    mStyleContext->GetData(kStyleColorSID);
  if (styleColor->mCursor != NS_STYLE_CURSOR_INHERIT) {
    // If we have a particular cursor, use it
    *aFrame = this;
    aCursor = (PRInt32) styleColor->mCursor;
  }

  nsIImageMap* map = GetImageMap();
  if (nsnull != map) {
    aCursor = NS_STYLE_CURSOR_DEFAULT;
    float t2p = aPresContext.GetTwipsToPixels();
    nscoord x = nscoord(t2p * aPoint.x);
    nscoord y = nscoord(t2p * aPoint.y);
    if (NS_OK == map->IsInside(x, y)) {
      aCursor = NS_STYLE_CURSOR_HAND;
    }
    NS_RELEASE(map);
  }

  return NS_OK;
}

void ImageFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                nsReflowMetrics& aDesiredSize,
                                const nsSize& aMaxSize)
{
  // XXX temporary hack: Get width & height from attributes
  nscoord width = -1;
  nscoord height = -1;
  ImagePart* part = (ImagePart*) mContent;
  nsHTMLValue value;
  nsContentAttr ca;
  ca = part->nsHTMLTagContent::GetAttribute(nsHTMLAtoms::width, value);
  if (eContentAttr_HasValue == ca) {
    // XXX Percents
    width = value.GetPixelValue();
  }
  ca = part->nsHTMLTagContent::GetAttribute(nsHTMLAtoms::height, value);
  if (eContentAttr_HasValue == ca) {
    // XXX Percents
    height = value.GetPixelValue();
  }

  float p2t = aPresContext->GetPixelsToTwips();
  if ((0 < width) && (0 < height)) {
    // Use dimensions from style attributes
    aDesiredSize.width = nscoord(width * p2t);
    aDesiredSize.height = nscoord(height * p2t);
  } else {
    nsIImage* image = GetImage(*aPresContext);
    if (nsnull == image) {
      // XXX Here is where we trigger a resize-reflow later on; or block
      // layout or whatever our policy wants to be
      aDesiredSize.width = nscoord(50 * p2t);
      aDesiredSize.height = nscoord(50 * p2t);
    } else {
      aDesiredSize.width = nscoord(image->GetWidth() * p2t);
      aDesiredSize.height = nscoord(image->GetHeight() * p2t);
    }
  }
}

//----------------------------------------------------------------------

ImagePart::ImagePart(nsIAtom* aTag)
  : nsHTMLTagContent(aTag)
{
  mAlign = ALIGN_UNSET;
  mSuppress = SUPPRESS_UNSET;
}

ImagePart::~ImagePart()
{
  if (nsnull != mAltText) delete mAltText;
  if (nsnull != mSrc) delete mSrc;
  if (nsnull != mLowSrc) delete mLowSrc;
  if (nsnull != mUseMap) delete mUseMap;
}

void ImagePart::SetAttribute(nsIAtom* aAttribute, const nsString& aString)
{
  if (aAttribute == nsHTMLAtoms::ismap) {
    mIsMap = PR_TRUE;
    return;
  }
  if (aAttribute == nsHTMLAtoms::usemap) {
    nsAutoString src(aString);
    src.StripWhitespace();
    if (nsnull == mUseMap) {
      mUseMap = new nsString(src);
    } else {
      *mUseMap = src;
    }
    return;
  }
  if (aAttribute == nsHTMLAtoms::align) {
    nsHTMLValue val;
    if (ParseAlignParam(aString, val)) {
      mAlign = val.GetIntValue();
      // Reflect the attribute into the syle system
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    } else {
      mAlign = ALIGN_UNSET;
    }
    return;
  }
  if (aAttribute == nsHTMLAtoms::src) {
    nsAutoString src(aString);
    src.StripWhitespace();
    if (nsnull == mSrc) {
      mSrc = new nsString(src);
    } else {
      *mSrc = src;
    }
    return;
  }
  if (aAttribute == nsHTMLAtoms::lowsrc) {
    nsAutoString src(aString);
    src.StripWhitespace();
    if (nsnull == mLowSrc) {
      mLowSrc = new nsString(src);
    } else {
      *mLowSrc = src;
    }
    return;
  }
  if (aAttribute == nsHTMLAtoms::alt) {
    if (nsnull == mAltText) {
      mAltText = new nsString(aString);
    } else {
      *mAltText = aString;
    }
    return;
  }
  if (aAttribute == nsHTMLAtoms::suppress) {
    if (aString.EqualsIgnoreCase("true")) {
      mSuppress = SUPPRESS;
    } else if (aString.EqualsIgnoreCase("false")) {
      mSuppress = DONT_SUPPRESS;
    } else {
      mSuppress = DEFAULT_SUPPRESS;
    }
    return;
  }

  // Try other attributes
  nsHTMLValue val;
  if (ParseImageProperty(aAttribute, aString, val)) {
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    return;
  }

  // Use default attribute catching code
  nsHTMLTagContent::SetAttribute(aAttribute, aString);
}

nsContentAttr ImagePart::GetAttribute(nsIAtom* aAttribute,
                                      nsHTMLValue& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::ismap) {
    if (mIsMap) {
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::usemap) {
    if (nsnull != mUseMap) {
      aResult.SetStringValue(*mUseMap);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (ALIGN_UNSET != mAlign) {
      aResult.SetIntValue(mAlign, eHTMLUnit_Enumerated);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::src) {
    if (nsnull != mSrc) {
      aResult.SetStringValue(*mSrc);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::lowsrc) {
    if (nsnull != mLowSrc) {
      aResult.SetStringValue(*mLowSrc);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::alt) {
    if (nsnull != mAltText) {
      aResult.SetStringValue(*mAltText);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::suppress) {
    if (SUPPRESS_UNSET != mSuppress) {
      switch (mSuppress) {
      case SUPPRESS:         aResult.SetIntValue(1, eHTMLUnit_Integer); break;
      case DONT_SUPPRESS:    aResult.SetIntValue(0, eHTMLUnit_Integer); break;
      case DEFAULT_SUPPRESS: aResult.SetEmptyValue(); break;
      }
      ca = eContentAttr_HasValue;
    }
  }
  else {
    ca = nsHTMLTagContent::GetAttribute(aAttribute, aResult);
  }
  return ca;
}

void ImagePart::UnsetAttribute(nsIAtom* aAttribute)
{
  nsContentAttr ca = eContentAttr_NotThere;
  if (aAttribute == nsHTMLAtoms::ismap) {
    mIsMap = PR_FALSE;
  }
  else if (aAttribute == nsHTMLAtoms::usemap) {
    if (nsnull != mUseMap) {
      delete mUseMap;
      mUseMap = nsnull;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    mAlign = ALIGN_UNSET;
  }
  else if (aAttribute == nsHTMLAtoms::src) {
    if (nsnull != mSrc) {
      delete mSrc;
      mSrc = nsnull;
    }
  }
  else if (aAttribute == nsHTMLAtoms::lowsrc) {
    if (nsnull != mLowSrc) {
      delete mLowSrc;
      mSrc = nsnull;
    }
  }
  else if (aAttribute == nsHTMLAtoms::alt) {
    if (nsnull != mAltText) {
      delete mAltText;
      mAltText = nsnull;
    }
  }
  else if (aAttribute == nsHTMLAtoms::suppress) {
    mSuppress = SUPPRESS_UNSET;
  }
  else {
    nsHTMLTagContent::UnsetAttribute(aAttribute);
  }
}

nsContentAttr ImagePart::AttributeToString(nsIAtom* aAttribute,
                                           nsHTMLValue& aValue,
                                           nsString& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  if (aAttribute == nsHTMLAtoms::align) {
    if ((eHTMLUnit_Enumerated == aValue.GetUnit()) &&
        (ALIGN_UNSET != aValue.GetIntValue())) {
      AlignParamToString(aValue, aResult);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::suppress) {
    if (SUPPRESS_UNSET != mSuppress) {
      aResult.Truncate();
      switch (mSuppress) {
      case SUPPRESS:         aResult.Append("true"); break;
      case DONT_SUPPRESS:    aResult.Append("false"); break;
      case DEFAULT_SUPPRESS: break;
      }
      ca = eContentAttr_HasValue;
    }
  }
  else if (ImagePropertyToString(aAttribute, aValue, aResult)) {
    ca = eContentAttr_HasValue;
  }
  return ca;
}

void ImagePart::MapAttributesInto(nsIStyleContext* aContext, 
                                  nsIPresContext* aPresContext)
{
  if (ALIGN_UNSET != mAlign) {
    nsStyleDisplay* display = (nsStyleDisplay*)
      aContext->GetData(kStyleDisplaySID);
    nsStyleText* text = (nsStyleText*)
      aContext->GetData(kStyleTextSID);
    switch (mAlign) {
    case NS_STYLE_TEXT_ALIGN_LEFT:
      display->mFloats = NS_STYLE_FLOAT_LEFT;
      break;
    case NS_STYLE_TEXT_ALIGN_RIGHT:
      display->mFloats = NS_STYLE_FLOAT_RIGHT;
      break;
    default:
      text->mVerticalAlign.SetIntValue(mAlign, eStyleUnit_Enumerated);
      break;
    }
  }
}

nsresult
ImagePart::CreateFrame(nsIPresContext* aPresContext,
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

nsresult
NS_NewHTMLImage(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* img = new ImagePart(aTag);
  if (nsnull == img) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return img->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
