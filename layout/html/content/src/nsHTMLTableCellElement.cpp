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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsIHTMLTableCellElement.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLTableCellElementIID, NS_IDOMHTMLTABLECELLELEMENT_IID);
static NS_DEFINE_IID(kIHTMLTableCellElementIID, NS_IHTMLTABLECELLELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLTableRowElementIID, NS_IDOMHTMLTABLEROWELEMENT_IID);

class nsHTMLTableCellElement :  public nsIHTMLTableCellElement,
                                public nsIDOMHTMLTableCellElement,
                                public nsIJSScriptObject,
                                public nsIHTMLContent
{
public:
  nsHTMLTableCellElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLTableCellElement();

// nsISupports
  NS_DECL_ISUPPORTS

// nsIHTMLTableCellElement

  /** @return the starting column for this cell.  Always >= 1 */
  NS_METHOD GetColIndex (PRInt32* aColIndex);

  /** set the starting column for this cell.  Always >= 1 */
  NS_METHOD SetColIndex (PRInt32 aColIndex);


// nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

// nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

// nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

// nsIDOMHTMLTableCellElement
  NS_DECL_IDOMHTMLTABLECELLELEMENT

// nsIJSScriptObject
  NS_IMPL_IJSSCRIPTOBJECT_USING_GENERIC(mInner)

// nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

// nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC2(mInner)

protected:
  nsresult GetRow(nsIDOMHTMLTableRowElement** aRow);

  nsGenericHTMLContainerElement mInner;
  PRInt32 mColIndex;
};

nsresult
NS_NewHTMLTableCellElement(nsIHTMLContent** aInstancePtrResult,
                           nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLTableCellElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}


nsHTMLTableCellElement::nsHTMLTableCellElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
  mColIndex=0;
}

nsHTMLTableCellElement::~nsHTMLTableCellElement()
{
}

NS_IMPL_ADDREF(nsHTMLTableCellElement)

NS_IMPL_RELEASE(nsHTMLTableCellElement)

nsresult
nsHTMLTableCellElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLTableCellElementIID)) {
    nsIDOMHTMLTableCellElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(kIHTMLTableCellElementIID)) {
    nsIHTMLTableCellElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLTableCellElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLTableCellElement* it = new nsHTMLTableCellElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

/** @return the starting column for this cell in aColIndex.  Always >= 1 */
NS_METHOD nsHTMLTableCellElement::GetColIndex (PRInt32* aColIndex)
{ 
  *aColIndex = mColIndex;
  return NS_OK;
}

/** set the starting column for this cell.  Always >= 1 */
NS_METHOD nsHTMLTableCellElement::SetColIndex (PRInt32 aColIndex)
{ 
  mColIndex = aColIndex;
  return NS_OK;
}

// protected method
nsresult
nsHTMLTableCellElement::GetRow(nsIDOMHTMLTableRowElement** aRow)
{
  nsIDOMNode *rowNode;
  GetParentNode(&rowNode); 
  nsresult result = rowNode->QueryInterface(kIDOMHTMLTableRowElementIID, (void**)aRow);
  NS_RELEASE(rowNode);
  return result;
}

NS_IMETHODIMP
nsHTMLTableCellElement::GetCellIndex(PRInt32* aCellIndex)
{
  *aCellIndex = -1;
  nsIDOMHTMLTableRowElement* row = nsnull;
  GetRow(&row);
  nsIDOMHTMLCollection *cells = nsnull;
  row->GetCells(&cells);
  PRUint32 numCells;
  cells->GetLength(&numCells);
  PRBool found = PR_FALSE;
  for (PRUint32 i = 0; (i < numCells) && !found; i++) {
    nsIDOMNode *node = nsnull;
    cells->Item(i, &node);
    if (this == node) {
      *aCellIndex = i;
	  found = PR_TRUE;
    }
	NS_IF_RELEASE(node);
  }
  NS_RELEASE(cells);
  NS_RELEASE(row);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableCellElement::SetCellIndex(PRInt32 aCellIndex)
{
  PRInt32 oldIndex;
  nsresult result = GetCellIndex(&oldIndex);
  if ((-1 == oldIndex) || (oldIndex == aCellIndex) || (NS_OK != result)) {
    return NS_OK;
  }

  nsIDOMHTMLTableRowElement* row = nsnull;
  GetRow(&row);
  nsIDOMHTMLCollection *cells = nsnull;
  row->GetCells(&cells);
  PRUint32 numCellsU;
  cells->GetLength(&numCellsU);
  PRInt32 numCells = numCellsU;

  // check if it really moves
  if ( !(((0 == oldIndex) && (aCellIndex <= 0)) || ((numCells-1 == oldIndex) && (aCellIndex >= numCells-1)))) {
    AddRef(); // don't use NS_ADDREF_THIS
    row->DeleteCell(oldIndex);       // delete this from the row
    numCells--;
    nsIDOMNode *returnNode;
    if ((numCells <= 0) || (aCellIndex >= numCells)) {
      row->AppendChild(this, &returnNode); // add this back into the row
		} else {
      PRInt32 newIndex = aCellIndex;
      if (aCellIndex <= 0) {
        newIndex = 0;
			} else if (aCellIndex > oldIndex) {
        newIndex--;
			}
      nsIDOMNode *refNode;
      cells->Item(newIndex, &refNode);
      row->InsertBefore(this, refNode, &returnNode); // add this back into the row
      NS_IF_RELEASE(refNode);
		}
  }

  NS_RELEASE(cells);
  NS_RELEASE(row);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableCellElement::GetContentStyleRules(nsISupportsArray* aRules)
{
  // get table, add its rules too
  nsIContent* row = nsnull; // XXX can we safely presume structure or do we need to QI on the way up?
  if (NS_SUCCEEDED(GetParent(row)) && row) {
    nsIContent* section = nsnull;
    if (NS_SUCCEEDED(row->GetParent(section)) && section) {
      nsIContent* table = nsnull;
      if (NS_SUCCEEDED(section->GetParent(table)) && table) {
        nsIStyledContent* styledTable = nsnull;
        if (NS_SUCCEEDED(table->QueryInterface(kIStyledContentIID, (void**)&styledTable))) {
          styledTable->GetContentStyleRules(aRules);
          NS_RELEASE(styledTable);
        }
        NS_RELEASE(table);
      }
      NS_RELEASE(section);
    }
    NS_RELEASE(row);
  }
  return mInner.GetContentStyleRules(aRules);
}

NS_IMETHODIMP
nsHTMLTableCellElement::GetInlineStyleRules(nsISupportsArray* aRules)
{
  return mInner.GetInlineStyleRules(aRules);
}


NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Abbr, abbr)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Axis, axis)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, BgColor, bgcolor)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Ch, ch)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, ChOff, choff)
NS_IMPL_INT_ATTR(nsHTMLTableCellElement, ColSpan, colspan)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Headers, headers)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Height, height)
NS_IMPL_BOOL_ATTR(nsHTMLTableCellElement, NoWrap, nowrap)
NS_IMPL_INT_ATTR(nsHTMLTableCellElement, RowSpan, rowspan)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Scope, scope)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, VAlign, valign)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Width, width)


static nsGenericHTMLElement::EnumTable kCellScopeTable[] = {
  { "row",      NS_STYLE_CELL_SCOPE_ROW },
  { "col",      NS_STYLE_CELL_SCOPE_COL },
  { "rowgroup", NS_STYLE_CELL_SCOPE_ROWGROUP },
  { "colgroup", NS_STYLE_CELL_SCOPE_COLGROUP },
  { 0 }
};

#define MAX_COLSPAN 1000

NS_IMETHODIMP
nsHTMLTableCellElement::StringToAttribute(nsIAtom* aAttribute,
                                   const nsAReadableString& aValue,
                                   nsHTMLValue& aResult)
{
  /* ignore these attributes, stored simply as strings
     abbr, axis, ch, headers
   */
  /* attributes that resolve to integers with a min of 0 */
  if (aAttribute == nsHTMLAtoms::choff) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  else if ((aAttribute == nsHTMLAtoms::colspan) ||
           (aAttribute == nsHTMLAtoms::rowspan)) {
    if (nsGenericHTMLElement::ParseValue(aValue, -1, MAX_COLSPAN, aResult, eHTMLUnit_Integer)) {
      PRInt32 val = aResult.GetIntValue();
      // quirks mode does not honor the special html 4 value of 0
      if ((val < 0) || ((0 == val) && mInner.InNavQuirksMode(mInner.mDocument))) {
        nsHTMLUnit unit = aResult.GetUnit();
        aResult.SetIntValue(1, unit);
      }

      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  /* attributes that resolve to integers or percents */
  else if (aAttribute == nsHTMLAtoms::height) {
    if (nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  /* attributes that resolve to integers or percents or proportions */
  else if (aAttribute == nsHTMLAtoms::width) {
    if (nsGenericHTMLElement::ParseValueOrPercentOrProportional(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  
  /* other attributes */
  else if (aAttribute == nsHTMLAtoms::align) {
    if (mInner.ParseTableCellHAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::bgcolor) {
    if (nsGenericHTMLElement::ParseColor(aValue, mInner.mDocument, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::scope) {
    if (nsGenericHTMLElement::ParseEnumValue(aValue, kCellScopeTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::valign) {
    if (nsGenericHTMLElement::ParseTableVAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableCellElement::AttributeToString(nsIAtom* aAttribute,
                                   const nsHTMLValue& aValue,
                                   nsAWritableString& aResult) const
{
  /* ignore these attributes, stored already as strings
     abbr, axis, ch, headers
   */
  /* ignore attributes that are of standard types
     choff, colspan, rowspan, height, width, nowrap, background, bgcolor
   */
  if (aAttribute == nsHTMLAtoms::align) {
    if (mInner.TableCellHAlignValueToString(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::scope) {
    if (nsGenericHTMLElement::EnumValueToString(aValue, kCellScopeTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::valign) {
    if (nsGenericHTMLElement::TableVAlignValueToString(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  if (nsnull!=aAttributes)
  {
    nsHTMLValue value;
    nsHTMLValue widthValue;
    nsStyleText* textStyle = nsnull;

    // align: enum
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) 
    {
      textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mTextAlign = value.GetIntValue();
    }
  
    // valign: enum
    aAttributes->GetAttribute(nsHTMLAtoms::valign, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) 
    {
      if (nsnull==textStyle)
        textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mVerticalAlign.SetIntValue(value.GetIntValue(), eStyleUnit_Enumerated);
    }

    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nsStylePosition* pos = (nsStylePosition*)
      aContext->GetMutableStyleData(eStyleStruct_Position);
    aAttributes->GetAttribute(nsHTMLAtoms::width, widthValue);
    if (widthValue.GetUnit() == eHTMLUnit_Pixel) {     // width: pixel
      nscoord width = widthValue.GetPixelValue();
      if (width > 0) {
        nscoord twips = NSIntPixelsToTwips(width, p2t);
        pos->mWidth.SetCoordValue(twips);
      }
      // else, 0 implies AUTO for compatibility 
    }
    else if (widthValue.GetUnit() == eHTMLUnit_Percent) { // width: percent
      float widthPercent = widthValue.GetPercentValue();
      if (widthPercent > 0.0f) {
        pos->mWidth.SetPercentValue(widthPercent);
      }
      // else, 0 implies AUTO for compatibility 
    }

    // height: pixel
    aAttributes->GetAttribute(nsHTMLAtoms::height, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) { // height: pixel
      nscoord height = value.GetPixelValue();
      nscoord twips = NSIntPixelsToTwips(height, p2t);
      pos->mHeight.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) { // height: percent
      float heightPercent = value.GetPercentValue();
      pos->mHeight.SetPercentValue(heightPercent);
    }

    // nowrap
    // nowrap depends on the width attribute, so be sure to handle it after width is mapped!
    aAttributes->GetAttribute(nsHTMLAtoms::nowrap, value);
    if (value.GetUnit() != eHTMLUnit_Null)
    {
      if (widthValue.GetUnit() != eHTMLUnit_Pixel)
      {
        if (nsnull==textStyle)
          textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
        textStyle->mWhiteSpace = NS_STYLE_WHITESPACE_NOWRAP;
      }
    }

    nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext, aPresContext);
    nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
  }
}

NS_IMETHODIMP
nsHTMLTableCellElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                                 PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::align) || 
      (aAttribute == nsHTMLAtoms::valign) ||
      (aAttribute == nsHTMLAtoms::nowrap) ||
      (aAttribute == nsHTMLAtoms::abbr) ||
      (aAttribute == nsHTMLAtoms::axis) ||
      (aAttribute == nsHTMLAtoms::headers) ||
      (aAttribute == nsHTMLAtoms::scope) ||
      (aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::height)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (! nsGenericHTMLElement::GetBackgroundAttributesImpact(aAttribute, aHint)) {
      aHint = NS_STYLE_HINT_CONTENT;
    }
  }

  return NS_OK;
}



NS_IMETHODIMP
nsHTMLTableCellElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                     nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableCellElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                nsEvent* aEvent,
                                nsIDOMEvent** aDOMEvent,
                                PRUint32 aFlags,
                                nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}


NS_IMETHODIMP
nsHTMLTableCellElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  return mInner.SizeOf(aSizer, aResult, sizeof(*this));
}
