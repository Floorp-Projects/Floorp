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
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableSectionElement.h"
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIScriptObjectOwner.h"
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

// temporary
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"

static NS_DEFINE_IID(kIDOMHTMLTableRowElementIID, NS_IDOMHTMLTABLEROWELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLTableElementIID, NS_IDOMHTMLTABLEELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLTableSectionElementIID, NS_IDOMHTMLTABLESECTIONELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLTableCellElementIID, NS_IDOMHTMLTABLECELLELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);

class nsHTMLTableRowElement : public nsIDOMHTMLTableRowElement,
                              public nsIScriptObjectOwner,
                              public nsIDOMEventReceiver,
                              public nsIHTMLContent
{
public:
  nsHTMLTableRowElement(nsIAtom* aTag);
  virtual ~nsHTMLTableRowElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLTableRowElement
  NS_IMETHOD GetRowIndex(PRInt32* aRowIndex);
  NS_IMETHOD SetRowIndex(PRInt32 aRowIndex);
  NS_IMETHOD GetSectionRowIndex(PRInt32* aSectionRowIndex);
  NS_IMETHOD SetSectionRowIndex(PRInt32 aSectionRowIndex);
  NS_IMETHOD GetCells(nsIDOMHTMLCollection** aCells);
  NS_IMETHOD SetCells(nsIDOMHTMLCollection* aCells);
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetBgColor(nsString& aBgColor);
  NS_IMETHOD SetBgColor(const nsString& aBgColor);
  NS_IMETHOD GetCh(nsString& aCh);
  NS_IMETHOD SetCh(const nsString& aCh);
  NS_IMETHOD GetChOff(nsString& aChOff);
  NS_IMETHOD SetChOff(const nsString& aChOff);
  NS_IMETHOD GetVAlign(nsString& aVAlign);
  NS_IMETHOD SetVAlign(const nsString& aVAlign);
  NS_IMETHOD InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aReturn);
  NS_IMETHOD DeleteCell(PRInt32 aIndex);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsresult GetSection(nsIDOMHTMLTableSectionElement** aSection);
  nsresult GetTable(nsIDOMHTMLTableElement** aTable);
  nsGenericHTMLContainerElement mInner;
  GenericElementCollection* mCells;
};

#ifdef XXX_debugging
static
void DebugList(nsIDOMHTMLTableElement* aTable) {
  nsIHTMLContent* content = nsnull;
  nsresult result = aTable->QueryInterface(kIHTMLContentIID, (void**)&content);
  if (NS_SUCCEEDED(result) && (nsnull != content)) {
    nsIDocument* doc = nsnull;
    result = content->GetDocument(doc);
    if (NS_SUCCEEDED(result) && (nsnull != doc)) {
      nsIContent* root = doc->GetRootContent();
      if (root) {
        root->List();
      }
      nsIPresShell* shell = doc->GetShellAt(0);
      if (nsnull != shell) {
        nsIFrame* rootFrame;
        shell->GetRootFrame(rootFrame);
        if (nsnull != rootFrame) {
          rootFrame->List(stdout, 0);
        }
      }
      NS_RELEASE(shell);
      NS_RELEASE(doc);
    }
    NS_RELEASE(content);
  }
}
#endif 

nsresult
NS_NewHTMLTableRowElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLTableRowElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLTableRowElement::nsHTMLTableRowElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
  mCells = nsnull;
}

nsHTMLTableRowElement::~nsHTMLTableRowElement()
{
  if (nsnull != mCells) {
    mCells->ParentDestroyed();
    NS_RELEASE(mCells);
  }
}

NS_IMPL_ADDREF(nsHTMLTableRowElement)

NS_IMPL_RELEASE(nsHTMLTableRowElement)

nsresult
nsHTMLTableRowElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLTableRowElementIID)) {
    nsIDOMHTMLTableRowElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLTableRowElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLTableRowElement* it = new nsHTMLTableRowElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
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
    result = sectionNode->QueryInterface(kIDOMHTMLTableSectionElementIID, (void**)aSection);
    NS_RELEASE(sectionNode);
  }
  return result;
}

// protected method
nsresult
nsHTMLTableRowElement::GetTable(nsIDOMHTMLTableElement** aTable)
{
  *aTable = nsnull;
  if (nsnull == aTable) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsIDOMNode *sectionNode = nsnull;
  nsresult result = GetParentNode(&sectionNode); 
  if (NS_SUCCEEDED(result) && (nsnull != sectionNode)) {
    nsIDOMNode *tableNode = nsnull;
    result = sectionNode->GetParentNode(&tableNode);
    if (NS_SUCCEEDED(result) && (nsnull != tableNode)) {
      result = tableNode->QueryInterface(kIDOMHTMLTableElementIID, (void**)aTable);
      NS_RELEASE(tableNode);
    }
    NS_RELEASE(sectionNode);
  }
  return result;
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetRowIndex(PRInt32* aValue)
{
  *aValue = -1;
  nsIDOMHTMLTableElement* table = nsnull;
  nsresult result = GetTable(&table);
  if (NS_SUCCEEDED(result) && (nsnull != table)) {
    nsIDOMHTMLCollection *rows = nsnull;
    table->GetRows(&rows);
    PRUint32 numRows;
    rows->GetLength(&numRows);
	PRBool found = PR_FALSE;
    for (PRUint32 i = 0; (i < numRows) && !found; i++) {
      nsIDOMNode *node = nsnull;
      rows->Item(i, &node);
      if (this == node) {
        *aValue = i;
        found = PR_TRUE;
      }
	  NS_IF_RELEASE(node);
    }
    NS_RELEASE(rows);
    NS_RELEASE(table);
  }

  return result;
}

// this tells the table to delete a row and then insert it in a different place. This will generate 2 reflows
// until things get fixed at a higher level (e.g. DOM batching).
NS_IMETHODIMP
nsHTMLTableRowElement::SetRowIndex(PRInt32 aValue)
{
  PRInt32 oldIndex;
  nsresult result = GetRowIndex(&oldIndex);
  if ((-1 == oldIndex) || (oldIndex == aValue) || (NS_OK != result)) {
    return result;
  }

  nsIDOMHTMLTableElement* table = nsnull;
  result = GetTable(&table);
  if (NS_FAILED(result) || (nsnull == table)) {
    return result;
  }

  nsIDOMHTMLCollection *rows = nsnull;
  table->GetRows(&rows);
  PRUint32 numRowsU;
  rows->GetLength(&numRowsU);
  PRInt32 numRows = numRowsU; // numRows will be > 0 since this must be a row

  // check if it really moves
  if ( !(((0 == oldIndex) && (aValue <= 0)) || ((numRows-1 == oldIndex) && (aValue >= numRows-1)))) {
    nsIDOMNode *section = nsnull;
    nsIDOMNode* refRow = nsnull;
	PRInt32 refIndex = aValue;
    if (aValue < numRows) {
	  refIndex = 0;
    } else {
	  refIndex = numRows-1;
	}
	rows->Item(refIndex, &refRow);
    refRow->GetParentNode(&section);

    AddRef(); // don't use NS_ADDREF_THIS
    table->DeleteRow(oldIndex);     // delete this from the table
    nsIDOMNode *returnNode;
    if (aValue >= numRows) {
      section->AppendChild(this, &returnNode); // add this back into the table
    } else {
      section->InsertBefore(this, refRow, &returnNode); // add this back into the table
    }
    Release(); // from addref above, can't use NS_RELEASE

    NS_RELEASE(section);
    NS_RELEASE(refRow);  // XXX is this right, check nsHTMLTableElement also
  }

  NS_RELEASE(rows);
  NS_RELEASE(table);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetSectionRowIndex(PRInt32* aValue)
{
  *aValue = -1;
  nsIDOMHTMLTableSectionElement* section = nsnull;
  nsresult result = GetSection(&section);
  if (NS_SUCCEEDED(result) && (nsnull != section)) {
    nsIDOMHTMLCollection *rows = nsnull;
    section->GetRows(&rows);
    PRUint32 numRows;
    rows->GetLength(&numRows);
	PRBool found = PR_FALSE;
    for (PRUint32 i = 0; (i < numRows) && !found; i++) {
      nsIDOMNode *node = nsnull;
      rows->Item(i, &node);
      if (this == node) {
        *aValue = i;
        found = PR_TRUE;
      }
	  NS_IF_RELEASE(node);
    } 
    NS_RELEASE(rows);
    NS_RELEASE(section);
  }

  return NS_OK;
}

// this generates 2 reflows like SetRowIndex
NS_IMETHODIMP
nsHTMLTableRowElement::SetSectionRowIndex(PRInt32 aValue)
{
  PRInt32 oldIndex;
  nsresult result = GetRowIndex(&oldIndex);
  if ((-1 == oldIndex) || (oldIndex == aValue) || (NS_OK != result)) {
    return result;
  }

  nsIDOMHTMLTableSectionElement* section = nsnull;
  result = GetSection(&section);
  if (NS_FAILED(result) || (nsnull == section)) {
    return result;
  }

  nsIDOMHTMLCollection *rows = nsnull;
  section->GetRows(&rows);
  PRUint32 numRowsU;
  rows->GetLength(&numRowsU);
  PRInt32 numRows = numRowsU; 

  // check if it really moves
  if ( !(((0 == oldIndex) && (aValue <= 0)) || ((numRows-1 == oldIndex) && (aValue >= numRows-1)))) {
    AddRef(); // don't use NS_ADDREF_THIS
    section->DeleteRow(oldIndex);     // delete this from the section
    numRows--;
    nsIDOMNode *returnNode;
    if ((numRows <= 0) || (aValue >= numRows)) {
      section->AppendChild(this, &returnNode); // add this back into the section
    } else {
      PRInt32 newIndex = aValue;
      if (aValue <= 0) {
        newIndex = 0;
      } else if (aValue > oldIndex) {
        newIndex--;                   // since this got removed before GetLength was called
      }
      nsIDOMNode *refNode;
      rows->Item(newIndex, &refNode);
      section->InsertBefore(this, refNode, &returnNode); // add this back into the section
	  NS_IF_RELEASE(refNode);
    }
    Release(); // from addref above, can't use NS_RELEASE
  }

  NS_RELEASE(rows);
  NS_RELEASE(section);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetCells(nsIDOMHTMLCollection** aValue)
{
  if (nsnull == mCells) {
    mCells = new GenericElementCollection(this, nsHTMLAtoms::td);
    NS_ADDREF(mCells); // this table's reference, released in the destructor
  }
  mCells->QueryInterface(kIDOMHTMLCollectionIID, (void **)aValue);   // caller's addref 
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableRowElement::SetCells(nsIDOMHTMLCollection* aValue)
{
  nsIDOMHTMLCollection* cells;
  GetCells(&cells);
  PRUint32 numCells;
  cells->GetLength(&numCells);
  PRUint32 i;
  for (i = 0; i < numCells; i++) {
    DeleteCell(i);
  }

  aValue->GetLength(&numCells);
  for (i = 0; i < numCells; i++) {
    nsIDOMNode *node = nsnull;
    cells->Item(i, &node);
    nsIDOMNode* aReturn;
    AppendChild(node, (nsIDOMNode**)&aReturn);
  }
  NS_RELEASE(cells);
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableRowElement::InsertCell(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;

  PRInt32 refIndex = (0 <= aIndex) ? aIndex : 0;
    
  nsIDOMHTMLCollection *cells;
  GetCells(&cells);
  PRUint32 cellCount;
  cells->GetLength(&cellCount);
  if (cellCount <= PRUint32(aIndex)) {
    refIndex = cellCount - 1; // refIndex will be -1 if there are no cells 
  }
  // create the cell
  nsIHTMLContent *cellContent = nsnull;
  nsresult rv = NS_NewHTMLTableCellElement(&cellContent, nsHTMLAtoms::td);
  if (NS_SUCCEEDED(rv) && (nsnull != cellContent)) {
    nsIDOMNode *cellNode = nsnull;
    rv = cellContent->QueryInterface(kIDOMNodeIID, (void **)&cellNode); 
    if (NS_SUCCEEDED(rv) && (nsnull != cellNode)) {
      if (refIndex >= 0) {
        nsIDOMNode *refCell;
        cells->Item(refIndex, &refCell);
        rv = InsertBefore(cellNode, refCell, (nsIDOMNode **)aValue);
      } else {
        rv = AppendChild(cellNode, (nsIDOMNode **)aValue);
      }

      NS_RELEASE(cellNode);
    }
    NS_RELEASE(cellContent);
  }
  NS_RELEASE(cells);

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableRowElement::DeleteCell(PRInt32 aValue)
{
  nsIDOMHTMLCollection *cells;
  GetCells(&cells);
  nsIDOMNode *cell = nsnull;
  cells->Item(aValue, &cell);
  if (nsnull != cell) {
    RemoveChild(cell, &cell);
  }
  NS_RELEASE(cells);
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, BgColor, bgcolor)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, Ch, ch)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, ChOff, choff)
NS_IMPL_STRING_ATTR(nsHTMLTableRowElement, VAlign, valign)


NS_IMETHODIMP
nsHTMLTableRowElement::StringToAttribute(nsIAtom* aAttribute,
                                  const nsString& aValue,
                                  nsHTMLValue& aResult)
{
  /* ignore these attributes, stored simply as strings
     ch
   */
  /* attributes that resolve to integers with default=0*/
  if (aAttribute == nsHTMLAtoms::choff) {
    nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer);
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
  else if (aAttribute == nsHTMLAtoms::valign) {
    if (nsGenericHTMLElement::ParseTableVAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableRowElement::AttributeToString(nsIAtom* aAttribute,
                                  const nsHTMLValue& aValue,
                                  nsString& aResult) const
{
  /* ignore these attributes, stored already as strings
     ch
   */
  /* ignore attributes that are of standard types
     choff, height, width, background, bgcolor
   */
  if (aAttribute == nsHTMLAtoms::align) {
    if (nsGenericHTMLElement::TableHAlignValueToString(aValue, aResult)) {
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
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
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

    // height: pixel
    aAttributes->GetAttribute(nsHTMLAtoms::height, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      float p2t;
      aPresContext->GetScaledPixelsToTwips(&p2t);
      nsStylePosition* pos = (nsStylePosition*)
        aContext->GetMutableStyleData(eStyleStruct_Position);
      nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
      pos->mHeight.SetCoordValue(twips);
    }
    nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext, aPresContext);
    nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
  }
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableRowElement::HandleDOMEvent(nsIPresContext& aPresContext,
                               nsEvent* aEvent,
                               nsIDOMEvent** aDOMEvent,
                               PRUint32 aFlags,
                               nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLTableRowElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  if (PR_TRUE == nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, 
    aAttribute, aHint)) {
    // Do nothing
  }
  else {
    // XXX put in real handling for known attributes, return CONTENT for anything else
    *aHint = NS_STYLE_HINT_CONTENT;
  }
  return NS_OK;
}
