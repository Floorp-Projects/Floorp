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
#include "nsHTMLContainer.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleRule.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsBodyFrame.h"
#include "nsIPresContext.h"
#include <limits.h>

static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);

class LayerPart : public nsHTMLContainer {
public:
  LayerPart(nsIAtom* aTag);

  virtual nsresult CreateFrame(nsIPresContext* aPresContext,
                               nsIFrame* aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*& aResult);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aString);

  virtual void MapAttributesInto(nsIStyleContext* aContext, 
                                 nsIPresContext* aPresContext);

protected:
  virtual nsContentAttr AttributeToString(nsIAtom* aAttribute,
                                          nsHTMLValue& aValue,
                                          nsString& aResult) const;
};

// -----------------------------------------------------------

LayerPart::LayerPart(nsIAtom* aTag)
  : nsHTMLContainer(aTag)
{
}

static nsHTMLTagContent::EnumTable kVisibilityTable[] = {
  {"hide", NS_STYLE_VISIBILITY_HIDDEN},
  {"visible", NS_STYLE_VISIBILITY_VISIBLE},
  {0}
};

void
LayerPart::SetAttribute(nsIAtom* aAttribute, const nsString& aString)
{
  // XXX CLIP
  nsHTMLValue val;
  if (aAttribute == nsHTMLAtoms::src) {
    nsAutoString src(aString);
    src.StripWhitespace();
    val.SetStringValue(src);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if ((aAttribute == nsHTMLAtoms::left) ||
      (aAttribute == nsHTMLAtoms::top)) {
    nsHTMLValue val;
    if (ParseValue(aString, _I32_MIN, val, eHTMLUnit_Pixel)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
  }
  else if ((aAttribute == nsHTMLAtoms::width) ||
           (aAttribute == nsHTMLAtoms::height)) {
    nsHTMLValue val;
    if (ParseValueOrPercent(aString, val, eHTMLUnit_Pixel)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
  }
  else if (aAttribute == nsHTMLAtoms::zindex) {
    nsHTMLValue val;
    if (ParseValue(aString, 0, val, eHTMLUnit_Integer)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
  }
  else if (aAttribute == nsHTMLAtoms::visibility) {
    if (ParseEnumValue(aString, kVisibilityTable, val)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
  }
  else if (aAttribute == nsHTMLAtoms::bgcolor) {
    ParseColor(aString, val);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::background) {
    nsAutoString url(aString);
    url.StripWhitespace();
    val.SetStringValue(url);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else {
    // ABOVE, BELOW, OnMouseOver, OnMouseOut, OnFocus, OnBlur, OnLoad
    nsHTMLTagContent::SetAttribute(aAttribute, aString);
  }
}

void
LayerPart::MapAttributesInto(nsIStyleContext* aContext, 
                             nsIPresContext*  aPresContext)
{
  // Note: ua.css specifies that the 'position' is absolute
  if (nsnull != mAttributes) {
    nsHTMLValue      value;
    float            p2t = aPresContext->GetPixelsToTwips();
    nsStylePosition* position = (nsStylePosition*)
      aContext->GetMutableStyleData(eStyleStruct_Position);

    // Left
    GetAttribute(nsHTMLAtoms::left, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      position->mLeftOffset.SetCoordValue(twips);
    }

    // Top
    GetAttribute(nsHTMLAtoms::top, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      position->mTopOffset.SetCoordValue(twips);
    }

    // Width
    GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      position->mWidth.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      position->mWidth.SetPercentValue(value.GetPercentValue());
    }

    // Height
    GetAttribute(nsHTMLAtoms::height, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      position->mHeight.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      position->mHeight.SetPercentValue(value.GetPercentValue());
    }

    // Z-index
    GetAttribute(nsHTMLAtoms::zindex, value);
    if (value.GetUnit() == eHTMLUnit_Integer) {
      position->mZIndex.SetIntValue(value.GetIntValue(), eStyleUnit_Integer);
    }

    // Visibility
    GetAttribute(nsHTMLAtoms::visibility, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      nsStyleDisplay* display = (nsStyleDisplay*)
        aContext->GetMutableStyleData(eStyleStruct_Display);

      display->mVisible = value.GetIntValue() == NS_STYLE_VISIBILITY_VISIBLE;
    }

    // Background and bgcolor
    MapBackgroundAttributesInto(aContext, aPresContext);
  }
}

nsContentAttr
LayerPart::AttributeToString(nsIAtom*     aAttribute,
                             nsHTMLValue& aValue,
                             nsString&    aResult) const
{
  if (aAttribute == nsHTMLAtoms::visibility) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      EnumValueToString(aValue, kVisibilityTable, aResult);
      return eContentAttr_HasValue;
    }
  }
  return eContentAttr_NotThere;
}

nsresult
LayerPart::CreateFrame(nsIPresContext*  aPresContext,
                       nsIFrame*        aParentFrame,
                       nsIStyleContext* aStyleContext,
                       nsIFrame*&       aResult)
{
  nsIFrame* frame = nsnull;
  nsresult  rv;

  // The type of frame depends on whether there is a SRC attribute
  nsAutoString  url;
#if 0
  if (GetAttribute("SRC", url) == eContentAttr_HasValue) {
    rv = new nsHTMLFrameOuterFrame(this, aParentFrame);
  }
  else {
    rv = nsBodyFrame::NewFrame(&frame, this, aParentFrame);
  }
#else
  rv = nsBodyFrame::NewFrame(&frame, this, aParentFrame);
#endif
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

nsresult
NS_NewHTMLLayer(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* body = new LayerPart(aTag);
  if (nsnull == body) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return body->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
