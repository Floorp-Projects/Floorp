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
#if 0
#include "nsStyleConsts.h"
#include "nsIHTMLAttributes.h"
#endif

// Spacer type's
#define TYPE_WORD  0            // horizontal space
#define TYPE_LINE  1            // line-break + vertical space
#define TYPE_IMAGE 2            // acts like a sized image with nothing to see

class SpacerFrame : public nsFrame
{
public:
  SpacerFrame(nsIContent* aContent,
              PRInt32 aIndexInParent,
              nsIFrame* aParentFrame);

  virtual ReflowStatus ResizeReflow(nsIPresContext* aPresContext,
                                    nsReflowMetrics& aDesiredSize,
                                    const nsSize& aMaxSize,
                                    nsSize* aMaxElementSize);
protected:
  virtual ~SpacerFrame();
};

class SpacerPart : public nsHTMLTagContent {
public:
  SpacerPart(nsIAtom* aTag);

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aValue) const;

  virtual nsContentAttr GetAttribute(const nsString& aName,
                                     nsString& aResult) const;

  PRUint8 mType;

protected:
  virtual ~SpacerPart();
};

//----------------------------------------------------------------------

SpacerFrame::SpacerFrame(nsIContent* aContent,
                       PRInt32 aIndexInParent,
                       nsIFrame* aParentFrame)
  : nsFrame(aContent, aIndexInParent, aParentFrame)
{
}

SpacerFrame::~SpacerFrame()
{
}

nsIFrame::ReflowStatus
SpacerFrame::ResizeReflow(nsIPresContext* aPresContext,
                      nsReflowMetrics& aDesiredSize,
                      const nsSize& aMaxSize,
                      nsSize* aMaxElementSize)
{
  // Get cached state for containing block frame
  nsBlockReflowState* state = nsnull;
  nsIFrame* parent = mGeometricParent;
  while (nsnull != parent) {
    nsIHTMLFrameType* ft;
    nsresult status = parent->QueryInterface(kIHTMLFrameTypeIID, (void**) &ft);
    if (NS_OK == status) {
      nsHTMLFrameType type = ft->GetFrameType();
      if (eHTMLFrame_Block == type) {
        break;
      }
    }
    parent = parent->GetGeometricParent();
  }
  if (nsnull != parent) {
    nsIPresShell* shell = aPresContext->GetShell();
    state = (nsBlockReflowState*) shell->GetCachedData(parent);
    NS_RELEASE(shell);
  }

  // By default, we have no area
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;

  if (nsnull == state) {
    // We don't do anything unless we are in a block container somewhere
    return frComplete;
  }

  nscoord width = 0;
  nscoord height = 0;
  SpacerPart* part = (SpacerPart*) mContent;
  PRUint8 type = part->mType;
  nsContentAttr ca;
  if (type != TYPE_IMAGE) {
    nsHTMLValue val;
    ca = part->GetAttribute(nsHTMLAtoms::size, val);
    if (eContentAttr_HasValue == ca) {
      width = val.GetIntValue();
    }
  } else {
    nsHTMLValue val;
    ca = part->GetAttribute(nsHTMLAtoms::width, val);
    if (eContentAttr_HasValue == ca) {
      if (eHTMLUnit_Absolute == val.GetUnit()) {
        width = val.GetIntValue();
      }
      else if (eHTMLUnit_Percent == val.GetUnit()) {
        // XXX percent of what?
      }
    }
    ca = part->GetAttribute(nsHTMLAtoms::height, val);
    if (eContentAttr_HasValue == ca) {
      if (eHTMLUnit_Absolute == val.GetUnit()) {
        height = val.GetIntValue();
      }
      else if (eHTMLUnit_Percent == val.GetUnit()) {
        // XXX percent of what?
      }
    }
  }

  float p2t = aPresContext->GetPixelsToTwips();
  switch (type) {
  case TYPE_WORD:
    if (0 != width) {
      aDesiredSize.width = nscoord(width * p2t);
    }
    else {
      // XXX navigator ignores spacers of size 0 and won't cause a word
      // break; we need a keepWithPrevious flag to fix this bug
    }
    break;

  case TYPE_LINE:
    if (0 != width) {
      state->breakBeforeChild = PR_TRUE;
      state->breakAfterChild = PR_TRUE;
      aDesiredSize.height = nscoord(width * p2t);
      aDesiredSize.ascent = aDesiredSize.height;
    }
    else {
      // XXX navigator ignores spacers of size 0 and won't cause a word
      // break; we need a keepWithPrevious flag to fix this bug
    }
    break;

  case TYPE_IMAGE:
    aDesiredSize.width = nscoord(width * p2t);
    aDesiredSize.height = nscoord(height * p2t);
    aDesiredSize.ascent = aDesiredSize.height;
    break;
  }

  return frComplete;
}

//----------------------------------------------------------------------

SpacerPart::SpacerPart(nsIAtom* aTag)
  : nsHTMLTagContent(aTag)
{
}

SpacerPart::~SpacerPart()
{
}

nsIFrame* SpacerPart::CreateFrame(nsIPresContext* aPresContext,
                                  PRInt32 aIndexInParent,
                                  nsIFrame* aParentFrame)
{
  nsIFrame* rv = new SpacerFrame(this, aIndexInParent, aParentFrame);
  return rv;
}

void SpacerPart::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::type) {
    PRUint8 type = TYPE_WORD;
    if (aValue.EqualsIgnoreCase("line") ||
        aValue.EqualsIgnoreCase("vert") ||
        aValue.EqualsIgnoreCase("vertical")) {
      type = TYPE_LINE;
    }
    else if (aValue.EqualsIgnoreCase("block")) {
      type = TYPE_IMAGE;
    }
    mType = type;
  }
  else if (aAttribute == nsHTMLAtoms::size) {
    nsHTMLValue val;
    ParseValue(aValue, 0, val);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    nsHTMLValue val;
    ParseAlignParam(aValue, val);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if ((aAttribute == nsHTMLAtoms::width) ||
           (aAttribute == nsHTMLAtoms::height)) {
    nsHTMLValue val;
    ParseValueOrPercent(aValue, val);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
}

nsContentAttr SpacerPart::GetAttribute(const nsString& aName,
                                       nsString& aResult) const
{
  aResult.SetLength(0);
  nsIAtom* atom = NS_NewAtom(aName);
  nsContentAttr ca = eContentAttr_NotThere;
  nsHTMLValue value;
  if (atom == nsHTMLAtoms::type) {
    switch (mType) {
    case TYPE_WORD:
      break;
    case TYPE_LINE:
      aResult.Append("line");
      ca = eContentAttr_HasValue;
      break;
    case TYPE_IMAGE:
      aResult.Append("block");
      ca = eContentAttr_HasValue;
      break;
    }
  }
  else if ((atom == nsHTMLAtoms::size) ||
           (atom == nsHTMLAtoms::width) ||
           (atom == nsHTMLAtoms::height)) {
    if (eContentAttr_HasValue == nsHTMLTagContent::GetAttribute(atom, value)) {
      ValueOrPercentToString(value, aResult);
      ca = eContentAttr_HasValue;
    }
  }
  else if (atom == nsHTMLAtoms::align) {
    if (eContentAttr_HasValue == nsHTMLTagContent::GetAttribute(atom, value)) {
      AlignParamToString(value, aResult);
      ca = eContentAttr_HasValue;
    }
  }
  NS_RELEASE(atom);
  return ca;
}

nsContentAttr SpacerPart::GetAttribute(nsIAtom* aAttribute,
                                       nsHTMLValue& aValue) const
{
  aValue.Reset();
  if ((mType != TYPE_IMAGE) &&
      ((aAttribute == nsHTMLAtoms::width) ||
       (aAttribute == nsHTMLAtoms::height) ||
       (aAttribute == nsHTMLAtoms::align))) {
    // Pretend that these attributes are not present when the spacer
    // is not a block spacer.
    return eContentAttr_NotThere;
  }
  return nsHTMLTagContent::GetAttribute(aAttribute, aValue);
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
