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
#include "nsIDOMHTMLTableElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLTableElementIID, NS_IDOMHTMLTABLEELEMENT_IID);


class nsHTMLTableElement :  public nsIDOMHTMLTableElement,
                            public nsIScriptObjectOwner,
                            public nsIDOMEventReceiver,
                            public nsIHTMLContent
{
public:
  nsHTMLTableElement(nsIAtom* aTag);
  ~nsHTMLTableElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLTableElement
  NS_IMETHOD GetCaption(nsIDOMHTMLTableCaptionElement** aCaption);
  NS_IMETHOD SetCaption(nsIDOMHTMLTableCaptionElement* aCaption);
  NS_IMETHOD GetTHead(nsIDOMHTMLTableSectionElement** aTHead);
  NS_IMETHOD SetTHead(nsIDOMHTMLTableSectionElement* aTHead);
  NS_IMETHOD GetTFoot(nsIDOMHTMLTableSectionElement** aTFoot);
  NS_IMETHOD SetTFoot(nsIDOMHTMLTableSectionElement* aTFoot);
  NS_IMETHOD GetRows(nsIDOMHTMLCollection** aRows);
  NS_IMETHOD GetTBodies(nsIDOMHTMLCollection** aTBodies);
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetBgColor(nsString& aBgColor);
  NS_IMETHOD SetBgColor(const nsString& aBgColor);
  NS_IMETHOD GetBorder(nsString& aBorder);
  NS_IMETHOD SetBorder(const nsString& aBorder);
  NS_IMETHOD GetCellPadding(nsString& aCellPadding);
  NS_IMETHOD SetCellPadding(const nsString& aCellPadding);
  NS_IMETHOD GetCellSpacing(nsString& aCellSpacing);
  NS_IMETHOD SetCellSpacing(const nsString& aCellSpacing);
  NS_IMETHOD GetFrame(nsString& aFrame);
  NS_IMETHOD SetFrame(const nsString& aFrame);
  NS_IMETHOD GetRules(nsString& aRules);
  NS_IMETHOD SetRules(const nsString& aRules);
  NS_IMETHOD GetSummary(nsString& aSummary);
  NS_IMETHOD SetSummary(const nsString& aSummary);
  NS_IMETHOD GetWidth(nsString& aWidth);
  NS_IMETHOD SetWidth(const nsString& aWidth);
  NS_IMETHOD CreateTHead(nsIDOMHTMLElement** aReturn);
  NS_IMETHOD DeleteTHead();
  NS_IMETHOD CreateTFoot(nsIDOMHTMLElement** aReturn);
  NS_IMETHOD DeleteTFoot();
  NS_IMETHOD CreateCaption(nsIDOMHTMLElement** aReturn);
  NS_IMETHOD DeleteCaption();
  NS_IMETHOD InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aReturn);
  NS_IMETHOD DeleteRow(PRInt32 aIndex);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
};

nsresult
NS_NewHTMLTableElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLTableElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLTableElement::nsHTMLTableElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLTableElement::~nsHTMLTableElement()
{
}

NS_IMPL_ADDREF(nsHTMLTableElement)

NS_IMPL_RELEASE(nsHTMLTableElement)

nsresult
nsHTMLTableElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLTableElementIID)) {
    nsIDOMHTMLTableElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLTableElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLTableElement* it = new nsHTMLTableElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

// the DOM spec says border, cellpadding, cellSpacing are all "wstring"
// in fact, they are integers or they are meaningless.  so we store them here as ints.

NS_IMPL_STRING_ATTR(nsHTMLTableElement, Align, align, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, BgColor, bgcolor, eSetAttrNotify_Render)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Border, border, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, CellPadding, cellpadding, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, CellSpacing, cellspacing, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Frame, frame, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Rules, rules, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Summary, summary, eSetAttrNotify_None)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Width, width, eSetAttrNotify_Reflow)

NS_IMETHODIMP
nsHTMLTableElement::GetCaption(nsIDOMHTMLTableCaptionElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::SetCaption(nsIDOMHTMLTableCaptionElement* aValue)
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::GetTHead(nsIDOMHTMLTableSectionElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::SetTHead(nsIDOMHTMLTableSectionElement* aValue)
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::GetTFoot(nsIDOMHTMLTableSectionElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::SetTFoot(nsIDOMHTMLTableSectionElement* aValue)
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::GetRows(nsIDOMHTMLCollection** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::GetTBodies(nsIDOMHTMLCollection** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::CreateTHead(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteTHead()
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::CreateTFoot(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteTFoot()
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::CreateCaption(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteCaption()
{
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  return NS_OK; // XXX write me
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteRow(PRInt32 aValue)
{
  return NS_OK; // XXX write me
}

static nsGenericHTMLElement::EnumTable kFrameTable[] = {
  { "void",   NS_STYLE_TABLE_FRAME_NONE },
  { "above",  NS_STYLE_TABLE_FRAME_ABOVE },
  { "below",  NS_STYLE_TABLE_FRAME_BELOW },
  { "hsides", NS_STYLE_TABLE_FRAME_HSIDES },
  { "lhs",    NS_STYLE_TABLE_FRAME_LEFT },
  { "rhs",    NS_STYLE_TABLE_FRAME_RIGHT },
  { "vsides", NS_STYLE_TABLE_FRAME_VSIDES },
  { "box",    NS_STYLE_TABLE_FRAME_BOX },
  { "border", NS_STYLE_TABLE_FRAME_BORDER },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kRulesTable[] = {
  { "none",   NS_STYLE_TABLE_RULES_NONE },
  { "groups", NS_STYLE_TABLE_RULES_GROUPS },
  { "rows",   NS_STYLE_TABLE_RULES_ROWS },
  { "cols",   NS_STYLE_TABLE_RULES_COLS },
  { "all",    NS_STYLE_TABLE_RULES_ALL },
  { 0 }
};

static nsGenericHTMLElement::EnumTable kLayoutTable[] = {
  { "auto",   NS_STYLE_TABLE_LAYOUT_AUTO },
  { "fixed",  NS_STYLE_TABLE_LAYOUT_FIXED },
  { 0 }
};


NS_IMETHODIMP
nsHTMLTableElement::StringToAttribute(nsIAtom* aAttribute,
                                      const nsString& aValue,
                                      nsHTMLValue& aResult)
{
  /* ignore summary, just a string */
  /* attributes that resolve to pixels, with min=0 */
  if ((aAttribute == nsHTMLAtoms::cellspacing) ||
      (aAttribute == nsHTMLAtoms::cellpadding)) {
    nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  /* attributes that are either empty, or integers, with min=0 */
  else if (aAttribute == nsHTMLAtoms::cols) {
    nsAutoString tmp(aValue);
    tmp.StripWhitespace();
    if (0 == tmp.Length()) {
      // Just set COLS, same as COLS=number of columns
      aResult.SetEmptyValue();
    }
    else 
    {
      nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer);
    }    
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  /* attributes that are either empty, or pixels */
  else if (aAttribute == nsHTMLAtoms::border) {
    nsAutoString tmp(aValue);
    tmp.StripWhitespace();
    if (0 == tmp.Length()) {
      // Just enable the border; same as border=1
      aResult.SetEmptyValue();
    }
    else 
    {
      nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel);
    }    
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  /* attributes that resolve to integers or percents */
  else if (aAttribute == nsHTMLAtoms::height) {
    nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  /* attributes that resolve to integers or percents or proportions */
  else if (aAttribute == nsHTMLAtoms::width) {
    nsGenericHTMLElement::ParseValueOrPercentOrProportional(aValue, aResult, eHTMLUnit_Pixel);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  /* other attributes */
  else if (aAttribute == nsHTMLAtoms::align) {
    if (nsGenericHTMLElement::ParseTableHAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::background) {
    nsAutoString href(aValue);
    href.StripWhitespace();
    aResult.SetStringValue(href);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::bgcolor) {
    nsGenericHTMLElement::ParseColor(aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::frame) {
    nsGenericHTMLElement::ParseEnumValue(aValue, kFrameTable, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::layout) {
    nsGenericHTMLElement::ParseEnumValue(aValue, kLayoutTable, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::rules) {
    nsGenericHTMLElement::ParseEnumValue(aValue, kRulesTable, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableElement::AttributeToString(nsIAtom* aAttribute,
                               nsHTMLValue& aValue,
                               nsString& aResult) const
{
  /* ignore summary, just a string */
  /* ignore attributes that are of standard types
     border, cellpadding, cellspacing, cols, height, width, background, bgcolor
   */
  if (aAttribute == nsHTMLAtoms::align) {
    if (nsGenericHTMLElement::TableHAlignValueToString(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::frame) {
    if (nsGenericHTMLElement::EnumValueToString(aValue, kFrameTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::layout) {
    if (nsGenericHTMLElement::EnumValueToString(aValue, kLayoutTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::rules) {
    if (nsGenericHTMLElement::EnumValueToString(aValue, kRulesTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

// XXX: this is only sufficient for Nav4/HTML3.2
// XXX: needs to be filled in for HTML4
static void 
MapTableBorderInto(nsIHTMLAttributes* aAttributes,
                   nsIStyleContext* aContext,
                   nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  nsHTMLValue value;

  aAttributes->GetAttribute(nsHTMLAtoms::border, value);
  if (value.GetUnit() == eHTMLUnit_String)
  {
    nsAutoString borderAsString;
    value.GetStringValue(borderAsString);
    nsGenericHTMLElement::ParseValue(borderAsString, 0, value, eHTMLUnit_Pixel);
  }
  if ((value.GetUnit() == eHTMLUnit_Pixel) || 
      (value.GetUnit() == eHTMLUnit_Empty)) {
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetMutableStyleData(eStyleStruct_Spacing);
    float p2t = aPresContext->GetPixelsToTwips();
    nsStyleCoord twips;
    if (value.GetUnit() == eHTMLUnit_Empty) {
      twips.SetCoordValue(NSIntPixelsToTwips(1, p2t));
    }
    else {
      twips.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
    }

    spacing->mBorder.SetTop(twips);
    spacing->mBorder.SetRight(twips);
    spacing->mBorder.SetBottom(twips);
    spacing->mBorder.SetLeft(twips);

    if (spacing->mBorderStyle[0] == NS_STYLE_BORDER_STYLE_NONE) {
      spacing->mBorderStyle[0] = NS_STYLE_BORDER_STYLE_OUTSET;
    }
    if (spacing->mBorderStyle[1] == NS_STYLE_BORDER_STYLE_NONE) {
      spacing->mBorderStyle[1] = NS_STYLE_BORDER_STYLE_OUTSET;
    }
    if (spacing->mBorderStyle[2] == NS_STYLE_BORDER_STYLE_NONE) {
      spacing->mBorderStyle[2] = NS_STYLE_BORDER_STYLE_OUTSET;
    }
    if (spacing->mBorderStyle[3] == NS_STYLE_BORDER_STYLE_NONE) {
      spacing->mBorderStyle[3] = NS_STYLE_BORDER_STYLE_OUTSET;
    }
  }
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  if (nsnull!=aAttributes)
  {
    float p2t = aPresContext->GetPixelsToTwips();
    nsHTMLValue value;

    // width
    aAttributes->GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() != eHTMLUnit_Null) {
      nsStylePosition* position = (nsStylePosition*)
        aContext->GetMutableStyleData(eStyleStruct_Position);
      switch (value.GetUnit()) {
      case eHTMLUnit_Percent:
        position->mWidth.SetPercentValue(value.GetPercentValue());
        break;

      case eHTMLUnit_Pixel:
        position->mWidth.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
        break;
      }
    }
    // border
    MapTableBorderInto(aAttributes, aContext, aPresContext);

    // align
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {  // it may be another type if illegal
      nsStyleDisplay* display = (nsStyleDisplay*)aContext->GetMutableStyleData(eStyleStruct_Display);
      switch (value.GetIntValue()) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        display->mFloats = NS_STYLE_FLOAT_LEFT;
        break;

      case NS_STYLE_TEXT_ALIGN_RIGHT:
        display->mFloats = NS_STYLE_FLOAT_RIGHT;
        break;
      }
    }

    // layout
    nsStyleTable* tableStyle=nsnull;
    aAttributes->GetAttribute(nsHTMLAtoms::layout, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {  // it may be another type if illegal
      tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
      tableStyle->mLayoutStrategy = value.GetIntValue();
    }

    // cellpadding
    aAttributes->GetAttribute(nsHTMLAtoms::cellpadding, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      if (nsnull==tableStyle)
        tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
      tableStyle->mCellPadding.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
    }

    // cellspacing  (reuses tableStyle if already resolved)
    aAttributes->GetAttribute(nsHTMLAtoms::cellspacing, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      if (nsnull==tableStyle)
        tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
      tableStyle->mCellSpacing.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
    }
    else
    { // XXX: remove me as soon as we get this from the style sheet
      if (nsnull==tableStyle)
        tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
      tableStyle->mCellSpacing.SetCoordValue(NSIntPixelsToTwips(2, p2t));
    }

    // cols
    aAttributes->GetAttribute(nsHTMLAtoms::cols, value);
    if (value.GetUnit() != eHTMLUnit_Null) {
      if (nsnull==tableStyle)
        tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
      if (value.GetUnit() == eHTMLUnit_Integer)
        tableStyle->mCols = value.GetIntValue();
      else // COLS had no value, so it refers to all columns
        tableStyle->mCols = NS_STYLE_TABLE_COLS_ALL;
    }

    //background: color
    nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext, aPresContext);
    nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
  }
}

NS_IMETHODIMP
nsHTMLTableElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableElement::HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
