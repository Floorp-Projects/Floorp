/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIHTMLTableCellElement.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIRuleNode.h"

class nsHTMLTableCellElement : public nsGenericHTMLContainerElement,
                               public nsIHTMLTableCellElement,
                               public nsIDOMHTMLTableCellElement
{
public:
  nsHTMLTableCellElement();
  virtual ~nsHTMLTableCellElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLTableCellElement
  NS_DECL_NSIDOMHTMLTABLECELLELEMENT

  // nsIHTMLTableCellElement
  NS_METHOD GetColIndex (PRInt32* aColIndex);
  NS_METHOD SetColIndex (PRInt32 aColIndex);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD WalkContentStyleRules(nsIRuleWalker* aRuleWalker);
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  // This does not retunr a nsresult since all we care about is if we
  // found the row element that this cell is in or not.
  void GetRow(nsIDOMHTMLTableRowElement** aRow);

  PRInt32 mColIndex;
};

nsresult
NS_NewHTMLTableCellElement(nsIHTMLContent** aInstancePtrResult,
                           nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLTableCellElement* it = new nsHTMLTableCellElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLTableCellElement::nsHTMLTableCellElement()
{
  mColIndex=0;
}

nsHTMLTableCellElement::~nsHTMLTableCellElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTableCellElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTableCellElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLTableCellElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLTableCellElement,
                                    nsGenericHTMLContainerElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLTableCellElement)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLTableCellElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLTableCellElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLTableCellElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLTableCellElement* it = new nsHTMLTableCellElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
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
void
nsHTMLTableCellElement::GetRow(nsIDOMHTMLTableRowElement** aRow)
{
  *aRow = nsnull;

  nsCOMPtr<nsIDOMNode> rowNode;
  GetParentNode(getter_AddRefs(rowNode));

  if (rowNode) {
    rowNode->QueryInterface(NS_GET_IID(nsIDOMHTMLTableRowElement),
                            (void**)aRow);
  }
}

NS_IMETHODIMP
nsHTMLTableCellElement::GetCellIndex(PRInt32* aCellIndex)
{
  *aCellIndex = -1;

  nsCOMPtr<nsIDOMHTMLTableRowElement> row;

  GetRow(getter_AddRefs(row));

  if (!row) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLCollection> cells;

  row->GetCells(getter_AddRefs(cells));

  if (!cells) {
    return NS_OK;
  }

  PRUint32 numCells;
  cells->GetLength(&numCells);

  PRBool found = PR_FALSE;
  PRUint32 i;

  for (i = 0; (i < numCells) && !found; i++) {
    nsCOMPtr<nsIDOMNode> node;
    cells->Item(i, getter_AddRefs(node));

    if (node.get() == NS_STATIC_CAST(nsIDOMNode *, this)) {
      *aCellIndex = i;
      found = PR_TRUE;
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableCellElement::WalkContentStyleRules(nsIRuleWalker* aRuleWalker)
{
  // get table, add its rules too
  // XXX can we safely presume structure or do we need to QI on the way up?
  nsCOMPtr<nsIContent> row;

  GetParent(*getter_AddRefs(row));

  if (row) {
    nsCOMPtr<nsIContent> section;

    row->GetParent(*getter_AddRefs(section));

    if (section) {
      nsCOMPtr<nsIContent> table;

      section->GetParent(*getter_AddRefs(table));

      if (table) {
        nsCOMPtr<nsIStyledContent> styledTable(do_QueryInterface(table));

        if (styledTable) {
          styledTable->WalkContentStyleRules(aRuleWalker);
        }
      }
    }
  }

  return nsGenericHTMLContainerElement::WalkContentStyleRules(aRuleWalker);
}


NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Abbr, abbr)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Axis, axis)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, BgColor, bgcolor)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, Ch, ch)
NS_IMPL_STRING_ATTR(nsHTMLTableCellElement, ChOff, charoff)
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
  if (aAttribute == nsHTMLAtoms::charoff) {
    /* attributes that resolve to integers with a min of 0 */

    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if ((aAttribute == nsHTMLAtoms::colspan) ||
           (aAttribute == nsHTMLAtoms::rowspan)) {
    PRBool parsed = (aAttribute == nsHTMLAtoms::colspan)
      ? ParseValue(aValue, -1, MAX_COLSPAN, aResult, eHTMLUnit_Integer)
      : ParseValue(aValue, -1, aResult, eHTMLUnit_Integer);
    if (parsed) {
      PRInt32 val = aResult.GetIntValue();
      // quirks mode does not honor the special html 4 value of 0
      if ((val < 0) || ((0 == val) && InNavQuirksMode(mDocument))) {
        nsHTMLUnit unit = aResult.GetUnit();
        aResult.SetIntValue(1, unit);
      }

      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::height) {
    /* attributes that resolve to integers or percents */

    if (ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::width) {
    /* attributes that resolve to integers or percents or proportions */

    if (ParseValueOrPercentOrProportional(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    /* other attributes */

    if (ParseTableCellHAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::bgcolor) {
    if (ParseColor(aValue, mDocument, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::scope) {
    if (ParseEnumValue(aValue, kCellScopeTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::valign) {
    if (ParseTableVAlignValue(aValue, aResult)) {
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
     charoff, colspan, rowspan, height, width, nowrap, background, bgcolor
   */
  if (aAttribute == nsHTMLAtoms::align) {
    if (TableCellHAlignValueToString(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::scope) {
    if (EnumValueToString(aValue, kCellScopeTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::valign) {
    if (TableVAlignValueToString(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLContainerElement::AttributeToString(aAttribute, aValue,
                                                          aResult);
}

static 
void MapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (!aAttributes || !aData)
    return;
 
  if (aData->mPositionData) {
    // width: value
    nsHTMLValue value;
    if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::width, value);
      if (value.GetUnit() == eHTMLUnit_Pixel) {
        if (value.GetPixelValue() > 0)
          aData->mPositionData->mWidth.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel); 
        // else 0 implies auto for compatibility.
      }
      else if (value.GetUnit() == eHTMLUnit_Percent) {
        if (value.GetPercentValue() > 0.0f)
          aData->mPositionData->mWidth.SetPercentValue(value.GetPercentValue());
        // else 0 implies auto for compatibility
      }
    }

    // height: value
    if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::height, value);
      if (value.GetUnit() == eHTMLUnit_Pixel) {
        if (value.GetPixelValue() > 0)
          aData->mPositionData->mHeight.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel);
        // else 0 implies auto for compatibility.
      }
      else if (value.GetUnit() == eHTMLUnit_Percent) {
        if (value.GetPercentValue() > 0.0f)
          aData->mPositionData->mHeight.SetPercentValue(value.GetPercentValue());
        // else 0 implies auto for compatibility
      }
    }
  }
  else if (aData->mTextData) {
    if (aData->mSID == eStyleStruct_Text) {
      if (aData->mTextData->mTextAlign.GetUnit() == eCSSUnit_Null) {
        // align: enum
        nsHTMLValue value;
        aAttributes->GetAttribute(nsHTMLAtoms::align, value);
        if (value.GetUnit() == eHTMLUnit_Enumerated)
          aData->mTextData->mTextAlign.SetIntValue(value.GetIntValue(), eCSSUnit_Enumerated);
      }

      if (aData->mTextData->mWhiteSpace.GetUnit() == eCSSUnit_Null) {
        // nowrap: enum
        nsHTMLValue value;
        aAttributes->GetAttribute(nsHTMLAtoms::nowrap, value);
        if (value.GetUnit() != eHTMLUnit_Null) {
          // See if our width is not a pixel width.
          nsHTMLValue widthValue;
          aAttributes->GetAttribute(nsHTMLAtoms::width, widthValue);
          if (widthValue.GetUnit() != eHTMLUnit_Pixel)
            aData->mTextData->mWhiteSpace.SetIntValue(NS_STYLE_WHITESPACE_NOWRAP, eCSSUnit_Enumerated);
        }
      }
    }
    else {
      if (aData->mTextData->mVerticalAlign.GetUnit() == eCSSUnit_Null) {
        // valign: enum
        nsHTMLValue value;
        aAttributes->GetAttribute(nsHTMLAtoms::valign, value);
        if (value.GetUnit() == eHTMLUnit_Enumerated) 
          aData->mTextData->mVerticalAlign.SetIntValue(value.GetIntValue(), eCSSUnit_Enumerated);
      }
    }
  }
  
  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLTableCellElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
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
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (!GetBackgroundAttributesImpact(aAttribute, aHint)) {
      aHint = NS_STYLE_HINT_CONTENT;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableCellElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableCellElement::SizeOf(nsISizeOfHandler* aSizer,
                               PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
