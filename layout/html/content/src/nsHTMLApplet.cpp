/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsFrame.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsStyleConsts.h"

#define nsHTMLAppletSuper nsHTMLContainer

class nsHTMLApplet : public nsHTMLAppletSuper {
public:
  nsHTMLApplet(nsIAtom* aTag);

  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

  void SetAttribute(nsIAtom* aAttribute, const nsString& aString);

  virtual void MapAttributesInto(nsIStyleContext* aContext, 
                                 nsIPresContext* aPresContext);

protected:
  virtual ~nsHTMLApplet();

  nsContentAttr AttributeToString(nsIAtom* aAttribute,
                                  nsHTMLValue& aValue,
                                  nsString& aResult) const;
};

nsHTMLApplet::nsHTMLApplet(nsIAtom* aTag)
  : nsHTMLAppletSuper(aTag)
{
}

nsHTMLApplet::~nsHTMLApplet()
{
}

nsresult
nsHTMLApplet::CreateFrame(nsIPresContext*  aPresContext,
                          nsIFrame*        aParentFrame,
                          nsIStyleContext* aStyleContext,
                          nsIFrame*&       aResult)
{
  nsIFrame* frame;
  nsresult rv = NS_NewObjectFrame(frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

void
nsHTMLApplet::SetAttribute(nsIAtom* aAttribute, const nsString& aString)
{
  nsHTMLValue val;
  if (aAttribute == nsHTMLAtoms::align) {
    if (ParseAlignParam(aString, val)) {
      // Reflect the attribute into the syle system
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
  }
  else if (ParseImageProperty(aAttribute, aString, val)) {
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else {
    nsHTMLAppletSuper::SetAttribute(aAttribute, aString);
  }
}

nsContentAttr
nsHTMLApplet::AttributeToString(nsIAtom* aAttribute,
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
  else if (ImagePropertyToString(aAttribute, aValue, aResult)) {
    ca = eContentAttr_HasValue;
  }
  return ca;
}

void
nsHTMLApplet::MapAttributesInto(nsIStyleContext* aContext, 
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
      nsStyleCoord three(nscoord(p2t*3));
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

nsresult
NS_NewHTMLApplet(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLApplet(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
