/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsDOMError.h"
#include "nsMappedAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsContentList.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsHTMLParts.h"
#include "nsRuleData.h"

// temporary
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"

class nsHTMLTableRowElement : public nsGenericHTMLElement,
                              public nsIDOMHTMLTableRowElement
{
public:
  nsHTMLTableRowElement(nsINodeInfo *aNodeInfo);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLTableRowElement
  NS_DECL_NSIDOMHTMLTABLEROWELEMENT

  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  nsresult GetSection(nsIDOMHTMLTableSectionElement** aSection);
  nsresult GetTable(nsIDOMHTMLTableElement** aTable);
  nsRefPtr<nsContentList> mCells;
};

#ifdef XXX_debugging
static
void DebugList(nsIDOMHTMLTableElement* aTable) {
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTable);
  if (content) {
    nsCOMPtr<nsIDocument> doc;
    result = content->GetDocument(getter_AddRefs(doc));
    if (doc) {
      nsCOMPtr<nsIContent> root;
      doc->GetRootContent(getter_AddRefs(root));
      if (root) {
        root->List();
      }
      nsIPresShell *shell = doc->GetShellAt(0);
      if (shell) {
        nsIFrame* rootFrame = shell->FrameManager()->GetRootFrame();
        if (rootFrame) {
          rootFrame->List(stdout, 0);
        }
      }
    }
  }
}
#endif 


NS_IMPL_NS_NEW_HTML_ELEMENT(TableRow)


nsHTMLTableRowElement::nsHTMLTableRowElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLTableRowElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTableRowElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLTableRowElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLTableRowElement,
                                    nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLTableRowElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLTableRowElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_ELEMENT_CLONE(nsHTMLTableRowElement)


// protected method
nsresult
nsHTMLTableRowElement::GetSection(nsIDOMHTMLTableSectionElement** aSection)
{
  NS_ENSURE_ARG_POINTER(aSection);
  *aSection = nsnull;

  nsCOMPtr<nsIDOMNode> sectionNode;
  nsresult rv = GetParentNode(getter_AddRefs(sectionNode));
  if (NS_SUCCEEDED(rv) && sectionNode) {
    rv = CallQueryInterface(sectionNode, aSection);
  }

  return rv;
}

// protected method
nsresult
nsHTMLTableRowElement::GetTable(nsIDOMHTMLTableElement** aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nsnull;

  nsCOMPtr<nsIDOMNode> sectionNode;
  nsresult rv = GetParentNode(getter_AddRefs(sectionNode));
  if (!sectionNode) {
    return rv;
  }

  // We may not be in a section
  rv = CallQueryInterface(sectionNode, aTable);
  if (NS_SUCCEEDED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDOMNode> tableNode;
  rv = sectionNode->GetParentNode(getter_AddRefs(tableNode));
  if (!tableNode) {
    return rv;
  }
  
  return CallQueryInterface(tableNode, aTable);
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetRowIndex(PRInt32* aValue)
{
  *aValue = -1;
  nsCOMPtr<nsIDOMHTMLTableElement> table;

  nsresult result = GetTable(getter_AddRefs(table));

  if (NS_SUCCEEDED(result) && table) {
    nsCOMPtr<nsIDOMHTMLCollection> rows;

    table->GetRows(getter_AddRefs(rows));

    PRUint32 numRows;
    rows->GetLength(&numRows);

    PRBool found = PR_FALSE;

    for (PRUint32 i = 0; (i < numRows) && !found; i++) {
      nsCOMPtr<nsIDOMNode> node;

      rows->Item(i, getter_AddRefs(node));

      if (node.get() == NS_STATIC_CAST(nsIDOMNode *, this)) {
        *aValue = i;
        found = PR_TRUE;
      }
    }
  }

  return result;
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetSectionRowIndex(PRInt32* aValue)
{
  *aValue = -1;

  nsCOMPtr<nsIDOMHTMLTableSectionElement> section;

  nsresult result = GetSection(getter_AddRefs(section));

  if (NS_SUCCEEDED(result) && section) {
    nsCOMPtr<nsIDOMHTMLCollection> rows;

    section->GetRows(getter_AddRefs(rows));

    PRBool found = PR_FALSE;
    PRUint32 numRows;

    rows->GetLength(&numRows);

    for (PRUint32 i = 0; (i < numRows) && !found; i++) {
      nsCOMPtr<nsIDOMNode> node;
      rows->Item(i, getter_AddRefs(node));

      if (node.get() == NS_STATIC_CAST(nsIDOMNode *, this)) {
        *aValue = i;
        found = PR_TRUE;
      }
    } 
  }

  return NS_OK;
}

PR_STATIC_CALLBACK(PRBool)
IsCell(nsIContent *aContent, PRInt32 aNamespaceID,
       nsIAtom* aAtom, void *aData)
{
  nsIAtom* tag = aContent->Tag();

  return ((tag == nsGkAtoms::td || tag == nsGkAtoms::th) &&
          aContent->IsNodeOfType(nsINode::eHTML));
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetCells(nsIDOMHTMLCollection** aValue)
{
  if (!mCells) {
    mCells = new nsContentList(this,
                               IsCell,
                               nsnull, // destroy func
                               nsnull, // closure data
                               PR_FALSE,
                               nsnull,
                               kNameSpaceID_None,
                               PR_FALSE);

    NS_ENSURE_TRUE(mCells, NS_ERROR_OUT_OF_MEMORY);
  }

  NS_ADDREF(*aValue = mCells);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;

  if (aIndex < -1) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }
  
  nsCOMPtr<nsIDOMHTMLCollection> cells;
  GetCells(getter_AddRefs(cells));

  PRUint32 cellCount;
  cells->GetLength(&cellCount);

  if (aIndex > PRInt32(cellCount)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  PRBool doInsert = (aIndex < PRInt32(cellCount)) && (aIndex != -1);

  // create the cell
  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::td,
                              getter_AddRefs(nodeInfo));

  nsCOMPtr<nsIContent> cellContent = NS_NewHTMLTableCellElement(nodeInfo);
  if (!cellContent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> cellNode(do_QueryInterface(cellContent));
  NS_ASSERTION(cellNode, "Should implement nsIDOMNode!");

  nsCOMPtr<nsIDOMNode> retChild;

  nsresult rv;
  if (doInsert) {
    nsCOMPtr<nsIDOMNode> refCell;
    cells->Item(aIndex, getter_AddRefs(refCell));

    rv = InsertBefore(cellNode, refCell, getter_AddRefs(retChild));
  } else {
    rv = AppendChild(cellNode, getter_AddRefs(retChild));
  }

  if (retChild) {
    CallQueryInterface(retChild, aValue);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableRowElement::DeleteCell(PRInt32 aValue)
{
  if (aValue < -1) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMHTMLCollection> cells;
  GetCells(getter_AddRefs(cells));

  nsresult rv;
  PRUint32 refIndex;
  if (aValue == -1) {
    rv = cells->GetLength(&refIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    if (refIndex == 0) {
      return NS_OK;
    }

    --refIndex;
  }
  else {
    refIndex = (PRUint32)aValue;
  }

  nsCOMPtr<nsIDOMNode> cell;
  rv = cells->Item(refIndex, getter_AddRefs(cell));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!cell) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMNode> retChild;
  return RemoveChild(cell, getter_AddRefs(retChild));
}

NS_IMPL_STRING_ATTR_DEFAULT_VALUE(nsHTMLTableRowElement, Align, align, "left")
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, BgColor, bgcolor)
NS_IMPL_STRING_ATTR_DEFAULT_VALUE(nsHTMLTableRowElement, Ch, _char, ".")
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, ChOff, charoff)
NS_IMPL_STRING_ATTR_DEFAULT_VALUE(nsHTMLTableRowElement, VAlign, valign, "middle")


PRBool
nsHTMLTableRowElement::ParseAttribute(PRInt32 aNamespaceID,
                                      nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsAttrValue& aResult)
{
  /*
   * ignore these attributes, stored simply as strings
   *
   * ch
   */

  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::charoff) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
    if (aAttribute == nsGkAtoms::height) {
      return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
    }
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
    }
    if (aAttribute == nsGkAtoms::align) {
      return ParseTableCellHAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::bgcolor) {
      return aResult.ParseColor(aValue, GetOwnerDoc());
    }
    if (aAttribute == nsGkAtoms::valign) {
      return ParseTableVAlignValue(aValue, aResult);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static 
void MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (aData->mSID == eStyleStruct_Position) {
    // height: value
    if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
      if (value && value->Type() == nsAttrValue::eInteger)
        aData->mPositionData->mHeight.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      else if (value && value->Type() == nsAttrValue::ePercent)
        aData->mPositionData->mHeight.SetPercentValue(value->GetPercentValue());
    }
  }
  else if (aData->mSID == eStyleStruct_Text) {
    if (aData->mTextData->mTextAlign.GetUnit() == eCSSUnit_Null) {
      // align: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);
      if (value && value->Type() == nsAttrValue::eEnum)
        aData->mTextData->mTextAlign.SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }
  else if (aData->mSID == eStyleStruct_TextReset) {
    if (aData->mTextData->mVerticalAlign.GetUnit() == eCSSUnit_Null) {
      // valign: enum
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::valign);
      if (value && value->Type() == nsAttrValue::eEnum)
        aData->mTextData->mVerticalAlign.SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }

  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLTableRowElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::align },
    { &nsGkAtoms::valign }, 
    { &nsGkAtoms::height },
    { nsnull }
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
    sBackgroundAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

nsMapRuleToAttributesFunc
nsHTMLTableRowElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}
