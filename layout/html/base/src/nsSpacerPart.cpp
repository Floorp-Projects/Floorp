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
#include "nsBlockFrame.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"

// Spacer type's
#define TYPE_WORD  0            // horizontal space
#define TYPE_LINE  1            // line-break + vertical space
#define TYPE_IMAGE 2            // acts like a sized image with nothing to see

class SpacerFrame : public nsFrame
{
public:
  SpacerFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);
protected:
  virtual ~SpacerFrame();
};

class SpacerPart : public nsHTMLTagContent {
public:
  SpacerPart(nsIAtom* aTag);

  virtual nsresult CreateFrame(nsIPresContext* aPresContext,
                               nsIFrame* aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*& aResult);
  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);
  virtual void MapAttributesInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext);

  PRUint8 GetType();

protected:
  virtual ~SpacerPart();
  nsContentAttr AttributeToString(nsIAtom*     aAttribute,
                                  nsHTMLValue& aValue,
                                  nsString&    aResult) const;
};

//----------------------------------------------------------------------

SpacerFrame::SpacerFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsFrame(aContent, aParentFrame)
{
}

SpacerFrame::~SpacerFrame()
{
}

NS_METHOD SpacerFrame::Reflow(nsIPresContext*      aPresContext,
                              nsReflowMetrics&     aDesiredSize,
                              const nsReflowState& aReflowState,
                              nsReflowStatus&      aStatus)
{
  // Get cached state for containing block frame
  nsLineLayout* lineLayoutState = nsnull;
  nsBlockReflowState* state =
    nsBlockFrame::FindBlockReflowState(aPresContext, this);
  if (nsnull != state) {
    lineLayoutState = state->mCurrentLine;
  }

  // By default, we have no area
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;

  if (nsnull == state) {
    // We don't do anything unless we are in a block container somewhere
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  nscoord width = 0;
  nscoord height = 0;
  SpacerPart* part = (SpacerPart*) mContent;
  PRUint8 type = part->GetType();
  nsContentAttr ca;
  if (type != TYPE_IMAGE) {
    nsHTMLValue val;
    ca = part->GetAttribute(nsHTMLAtoms::size, val);
    if (eContentAttr_HasValue == ca) {
      width = val.GetPixelValue();
    }
  } else {
    nsHTMLValue val;
    ca = part->GetAttribute(nsHTMLAtoms::width, val);
    if (eContentAttr_HasValue == ca) {
      if (eHTMLUnit_Pixel == val.GetUnit()) {
        width = val.GetPixelValue();
      }
    }
    ca = part->GetAttribute(nsHTMLAtoms::height, val);
    if (eContentAttr_HasValue == ca) {
      if (eHTMLUnit_Pixel == val.GetUnit()) {
        height = val.GetPixelValue();
      }
    }
  }

  float p2t = aPresContext->GetPixelsToTwips();
  switch (type) {
  case TYPE_WORD:
    if (0 != width) {
      aDesiredSize.width = nscoord(width * p2t);
    }
    break;

  case TYPE_LINE:
    if (0 != width) {
      if (nsnull != lineLayoutState) {
        lineLayoutState->mReflowResult =
          NS_LINE_LAYOUT_REFLOW_RESULT_BREAK_AFTER;
        lineLayoutState->mPendingBreak = NS_STYLE_CLEAR_LINE;
      }
      aDesiredSize.height = nscoord(width * p2t);
      aDesiredSize.ascent = aDesiredSize.height;
    }
    break;

  case TYPE_IMAGE:
    aDesiredSize.width = nscoord(width * p2t);
    aDesiredSize.height = nscoord(height * p2t);
    aDesiredSize.ascent = aDesiredSize.height;
    break;
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

//----------------------------------------------------------------------

SpacerPart::SpacerPart(nsIAtom* aTag)
  : nsHTMLTagContent(aTag)
{
}

SpacerPart::~SpacerPart()
{
}

nsresult
SpacerPart::CreateFrame(nsIPresContext*  aPresContext,
                        nsIFrame*        aParentFrame,
                        nsIStyleContext* aStyleContext,
                        nsIFrame*&       aResult)
{
  nsIFrame* frame = new SpacerFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

PRUint8
SpacerPart::GetType()
{
  PRUint8 type = TYPE_WORD;
  nsHTMLValue value;
  if (eContentAttr_HasValue ==
      nsHTMLTagContent::GetAttribute(nsHTMLAtoms::type, value)) {
    if (eHTMLUnit_Enumerated == value.GetUnit()) {
      type = value.GetIntValue();
    }
  }
  return type;
}

static nsHTMLTagContent::EnumTable kTypeTable[] = {
  { "line", TYPE_LINE },
  { "vert", TYPE_LINE },
  { "vertical", TYPE_LINE },
  { "block", TYPE_IMAGE },
  { 0 }
};

void
SpacerPart::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  nsHTMLValue val;
  if (aAttribute == nsHTMLAtoms::type) {
    if (ParseEnumValue(aValue, kTypeTable, val)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
  }
  else if (aAttribute == nsHTMLAtoms::size) {
    ParseValue(aValue, 0, val, eHTMLUnit_Pixel);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    ParseAlignParam(aValue, val);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if ((aAttribute == nsHTMLAtoms::width) ||
           (aAttribute == nsHTMLAtoms::height)) {
    ParseValueOrPercent(aValue, val, eHTMLUnit_Pixel);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else {
    nsHTMLTagContent::SetAttribute(aAttribute, aValue);
  }
}

void
SpacerPart::MapAttributesInto(nsIStyleContext* aContext,
                              nsIPresContext* aPresContext)
{
  if (nsnull != mAttributes) {
    nsHTMLValue value;
    nsStyleDisplay* display = (nsStyleDisplay*)
      aContext->GetMutableStyleData(eStyleStruct_Display);
    GetAttribute(nsHTMLAtoms::align, value);
    if (eHTMLUnit_Enumerated == value.GetUnit()) {
      switch (value.GetIntValue()) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        display->mFloats = NS_STYLE_FLOAT_LEFT;
        break;
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        display->mFloats = NS_STYLE_FLOAT_RIGHT;
        break;
      default:
        break;
      }
    }
    PRUint8 type = GetType();
    switch (type) {
    case TYPE_WORD:
    case TYPE_IMAGE:
      break;

    case TYPE_LINE:
      // This is not strictly 100% compatible: if the spacer is given
      // a width of zero then it is basically ignored.
      display->mDisplay = NS_STYLE_DISPLAY_BLOCK;
      break;
    }
  }
}

nsContentAttr
SpacerPart::AttributeToString(nsIAtom*     aAttribute,
                              nsHTMLValue& aValue,
                              nsString&    aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      EnumValueToString(aValue, kTypeTable, aResult);
      return eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::type) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      AlignParamToString(aValue, aResult);
      return eContentAttr_HasValue;
    }
  }
  return eContentAttr_NotThere;
}

nsresult
NS_NewHTMLSpacer(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new SpacerPart(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}
