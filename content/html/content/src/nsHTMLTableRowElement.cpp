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
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "GenericElementCollection.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsHTMLParts.h"
#include "nsIRuleNode.h"

// temporary
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"

// nsTableCellCollection is needed because GenericElementCollection 
// only supports element of a single tag. This collection supports
// elements <td> or <th> elements.

class nsTableCellCollection : public GenericElementCollection
{
public:
  nsTableCellCollection(nsIContent* aParent, 
                        nsIAtom*    aTag);
  ~nsTableCellCollection();

  NS_IMETHOD GetLength(PRUint32* aLength);
  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMNode** aReturn);
};


nsTableCellCollection::nsTableCellCollection(nsIContent* aParent, 
                                             nsIAtom*    aTag)
  : GenericElementCollection(aParent, aTag)
{
}

nsTableCellCollection::~nsTableCellCollection()
{
}

NS_IMETHODIMP 
nsTableCellCollection::GetLength(PRUint32* aLength)
{
  if (!aLength) {
    return NS_ERROR_NULL_POINTER;
  }

  *aLength = 0;

  nsresult result = NS_OK;

  if (mParent) {
    nsCOMPtr<nsIContent> child;
    PRUint32 childIndex = 0;

    mParent->ChildAt(childIndex, *getter_AddRefs(child));

    while (child) {
      nsCOMPtr<nsIAtom> childTag;
      child->GetTag(*getter_AddRefs(childTag));

      if ((nsHTMLAtoms::td == childTag.get()) ||
          (nsHTMLAtoms::th == childTag.get())) {
        (*aLength)++;
      }

      childIndex++;
      mParent->ChildAt(childIndex, *getter_AddRefs(child));
    }
  }

  return result;
}

NS_IMETHODIMP 
nsTableCellCollection::Item(PRUint32     aIndex, 
                            nsIDOMNode** aReturn)
{
  *aReturn = nsnull;
  PRUint32 theIndex = 0;
  nsresult rv = NS_OK;

  if (mParent) {
    nsCOMPtr<nsIContent> child;
    PRUint32 childIndex = 0;

    mParent->ChildAt(childIndex, *getter_AddRefs(child));

    while (child) {
      nsCOMPtr<nsIAtom> childTag;

      child->GetTag(*getter_AddRefs(childTag));

      if ((nsHTMLAtoms::td == childTag.get()) ||
          (nsHTMLAtoms::th == childTag.get())) {
        if (aIndex == theIndex) {
          child->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aReturn);

          NS_ASSERTION(aReturn, "content element must be an nsIDOMNode");

          break;
        }
        theIndex++;
      }

      childIndex++;
      mParent->ChildAt(childIndex, *getter_AddRefs(child));
    }
  }

  return rv;
}

//----------------------------------------------------------------------

class nsHTMLTableRowElement : public nsGenericHTMLContainerElement,
                              public nsIDOMHTMLTableRowElement
{
public:
  nsHTMLTableRowElement();
  virtual ~nsHTMLTableRowElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLTableRowElement
  NS_DECL_NSIDOMHTMLTABLEROWELEMENT

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  nsresult GetSection(nsIDOMHTMLTableSectionElement** aSection);
  nsresult GetTable(nsIDOMHTMLTableElement** aTable);
  nsTableCellCollection* mCells;
};

#ifdef XXX_debugging
static
void DebugList(nsIDOMHTMLTableElement* aTable) {
  nsIHTMLContent* content = nsnull;
  nsresult result = aTable->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**)&content);
  if (NS_SUCCEEDED(result) && (nsnull != content)) {
    nsIDocument* doc = nsnull;
    result = content->GetDocument(doc);
    if (NS_SUCCEEDED(result) && (nsnull != doc)) {
      nsIContent* root = nsnull;
      doc->GetRootContent(&root);
      if (root) {
        root->List();
      }
      nsCOMPtr<nsIPresShell> shell;
      doc->GetShellAt(0, getter_AddRefs(shell));
      if (nsnull != shell) {
        nsIFrame* rootFrame;
        shell->GetRootFrame(rootFrame);
        if (nsnull != rootFrame) {
          rootFrame->List(stdout, 0);
        }
      }
      NS_RELEASE(doc);
    }
    NS_RELEASE(content);
  }
}
#endif 

nsresult
NS_NewHTMLTableRowElement(nsIHTMLContent** aInstancePtrResult,
                          nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLTableRowElement* it = new nsHTMLTableRowElement();

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


nsHTMLTableRowElement::nsHTMLTableRowElement()
{
  mCells = nsnull;
}

nsHTMLTableRowElement::~nsHTMLTableRowElement()
{
  if (nsnull != mCells) {
    mCells->ParentDestroyed();
    NS_RELEASE(mCells);
  }
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTableRowElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTableRowElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLTableRowElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLTableRowElement,
                                    nsGenericHTMLContainerElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLTableRowElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLTableRowElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLTableRowElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLTableRowElement* it = new nsHTMLTableRowElement();

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

// protected method
nsresult
nsHTMLTableRowElement::GetSection(nsIDOMHTMLTableSectionElement** aSection)
{
  *aSection = nsnull;
  if (nsnull == aSection) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsIDOMNode *sectionNode = nsnull;
  nsresult result = GetParentNode(&sectionNode);
  if (NS_SUCCEEDED(result) && (nsnull != sectionNode)) {
    result = sectionNode->QueryInterface(NS_GET_IID(nsIDOMHTMLTableSectionElement), (void**)aSection);
    NS_RELEASE(sectionNode);
  }
  return result;
}

// protected method
nsresult
nsHTMLTableRowElement::GetTable(nsIDOMHTMLTableElement** aTable)
{
  *aTable = nsnull;

  if (!aTable) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> sectionNode;

  nsresult result = GetParentNode(getter_AddRefs(sectionNode));

  if (NS_SUCCEEDED(result) && sectionNode) {
    nsCOMPtr<nsIDOMNode> tableNode;

    result = sectionNode->GetParentNode(getter_AddRefs(tableNode));

    if (NS_SUCCEEDED(result) && tableNode) {
      result = tableNode->QueryInterface(NS_GET_IID(nsIDOMHTMLTableElement),
                                         (void**)aTable);
    }
  }

  return result;
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

NS_IMETHODIMP
nsHTMLTableRowElement::GetCells(nsIDOMHTMLCollection** aValue)
{
  if (!mCells) {
    mCells = new nsTableCellCollection(this, nsHTMLAtoms::td);

    NS_ENSURE_TRUE(mCells, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mCells); // this table's reference, released in the destructor
  }

  mCells->QueryInterface(NS_GET_IID(nsIDOMHTMLCollection), (void **)aValue);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;

  nsCOMPtr<nsIDOMHTMLCollection> cells;

  GetCells(getter_AddRefs(cells));

  PRUint32 cellCount;
  cells->GetLength(&cellCount);

  PRBool doInsert = (aIndex < PRInt32(cellCount));

  // create the cell
  nsCOMPtr<nsIHTMLContent> cellContent;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  mNodeInfo->NameChanged(nsHTMLAtoms::td, *getter_AddRefs(nodeInfo));

  nsresult rv = NS_NewHTMLTableCellElement(getter_AddRefs(cellContent),
                                           nodeInfo);

  if (NS_SUCCEEDED(rv) && cellContent) {
    nsCOMPtr<nsIDOMNode> cellNode(do_QueryInterface(cellContent));

    if (NS_SUCCEEDED(rv) && cellNode) {
      nsCOMPtr<nsIDOMNode> retChild;

      if (doInsert) {
        PRInt32 refIndex = PR_MAX(aIndex, 0);   
        nsCOMPtr<nsIDOMNode> refCell;

        cells->Item(refIndex, getter_AddRefs(refCell));

        rv = InsertBefore(cellNode, refCell, getter_AddRefs(retChild));
      } else {
        rv = AppendChild(cellNode, getter_AddRefs(retChild));
      }

      if (retChild) {
        retChild->QueryInterface(NS_GET_IID(nsIDOMHTMLElement),
                                 (void **)aValue);
      }
    }
  }  

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableRowElement::DeleteCell(PRInt32 aValue)
{
  nsCOMPtr<nsIDOMHTMLCollection> cells;

  GetCells(getter_AddRefs(cells));

  nsCOMPtr<nsIDOMNode> cell;

  cells->Item(aValue, getter_AddRefs(cell));

  if (cell) {
    nsCOMPtr<nsIDOMNode> retChild;

    RemoveChild(cell, getter_AddRefs(retChild));
  }

  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, BgColor, bgcolor)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, Ch, _char)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, ChOff, charoff)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, VAlign, valign)


NS_IMETHODIMP
nsHTMLTableRowElement::StringToAttribute(nsIAtom* aAttribute,
                                  const nsAReadableString& aValue,
                                  nsHTMLValue& aResult)
{
  /* ignore these attributes, stored simply as strings
     ch
   */
  /* attributes that resolve to integers with default=0*/
  if (aAttribute == nsHTMLAtoms::charoff) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
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
  else if (aAttribute == nsHTMLAtoms::valign) {
    if (ParseTableVAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableRowElement::AttributeToString(nsIAtom* aAttribute,
                                  const nsHTMLValue& aValue,
                                  nsAWritableString& aResult) const
{
  /* ignore these attributes, stored already as strings
     ch
   */
  /* ignore attributes that are of standard types
     charoff, height, width, background, bgcolor
   */
  if (aAttribute == nsHTMLAtoms::align) {
    if (TableCellHAlignValueToString(aValue, aResult)) {
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
    // height: value
    nsHTMLValue value;
    if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::height, value);
      if (value.GetUnit() == eHTMLUnit_Pixel)
        aData->mPositionData->mHeight.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel);
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
nsHTMLTableRowElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                                PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::align) ||
      (aAttribute == nsHTMLAtoms::valign) ||
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
nsHTMLTableRowElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::SizeOf(nsISizeOfHandler* aSizer,
                              PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
