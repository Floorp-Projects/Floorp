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
#include "nsIDOMHTMLTableSectionElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLParts.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "GenericElementCollection.h"

// you will see the phrases "rowgroup" and "section" used interchangably

class nsHTMLTableSectionElement : public nsIDOMHTMLTableSectionElement,
                                  public nsIJSScriptObject,
                                  public nsIHTMLContent
{
public:
  nsHTMLTableSectionElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLTableSectionElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLTableSectionElement
  NS_DECL_IDOMHTMLTABLESECTIONELEMENT

  // nsIJSScriptObject
  NS_IMPL_IJSSCRIPTOBJECT_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
  GenericElementCollection *mRows;
};

nsresult
NS_NewHTMLTableSectionElement(nsIHTMLContent** aInstancePtrResult,
                              nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLTableSectionElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**) aInstancePtrResult);
}


nsHTMLTableSectionElement::nsHTMLTableSectionElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
  mRows = nsnull;
}

nsHTMLTableSectionElement::~nsHTMLTableSectionElement()
{
  if (nsnull!=mRows) {
    mRows->ParentDestroyed();
    NS_RELEASE(mRows);
  }
}

NS_IMPL_ADDREF(nsHTMLTableSectionElement)

NS_IMPL_RELEASE(nsHTMLTableSectionElement)

nsresult
nsHTMLTableSectionElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(NS_GET_IID(nsIDOMHTMLTableSectionElement))) {
    nsIDOMHTMLTableSectionElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLTableSectionElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLTableSectionElement* it = new nsHTMLTableSectionElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLTableSectionElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLTableSectionElement, VAlign, valign)
NS_IMPL_STRING_ATTR(nsHTMLTableSectionElement, Ch, ch)
NS_IMPL_STRING_ATTR(nsHTMLTableSectionElement, ChOff, choff)

NS_IMETHODIMP
nsHTMLTableSectionElement::GetRows(nsIDOMHTMLCollection** aValue)
{
  *aValue = nsnull;
  if (nsnull == mRows) {
    //XXX why was this here NS_ADDREF(nsHTMLAtoms::tr);
    mRows = new GenericElementCollection(this, nsHTMLAtoms::tr);
    NS_ADDREF(mRows); // this table's reference, released in the destructor
  }
  mRows->QueryInterface(NS_GET_IID(nsIDOMHTMLCollection), (void **)aValue);   // caller's addref 
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableSectionElement::InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;

  nsCOMPtr<nsIDOMHTMLCollection> rows;
  GetRows(getter_AddRefs(rows));
  PRUint32 rowCount;
  rows->GetLength(&rowCount);
  PRBool doInsert = (aIndex < PRInt32(rowCount));

  // create the row
  nsIHTMLContent* rowContent = nsnull;
  nsCOMPtr<nsINodeInfo> nodeInfo;
  mInner.mNodeInfo->NameChanged(nsHTMLAtoms::tr, *getter_AddRefs(nodeInfo));

  nsresult rv = NS_NewHTMLTableRowElement(&rowContent, nodeInfo);
  if (NS_SUCCEEDED(rv) && (nsnull != rowContent)) {
    nsIDOMNode* rowNode = nsnull;
    rv = rowContent->QueryInterface(NS_GET_IID(nsIDOMNode), (void **)&rowNode); 
    if (NS_SUCCEEDED(rv) && (nsnull != rowNode)) {
      if (doInsert) {
        PRInt32 refIndex = PR_MAX(aIndex, 0);   
        nsCOMPtr<nsIDOMNode> refRow;
        rows->Item(refIndex, getter_AddRefs(refRow));
        rv = InsertBefore(rowNode, refRow, (nsIDOMNode **)aValue);
      } else {
        rv = AppendChild(rowNode, (nsIDOMNode **)aValue);
      }
      NS_RELEASE(rowNode);
    }
    NS_RELEASE(rowContent);
  } 
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableSectionElement::DeleteRow(PRInt32 aValue)
{
  nsIDOMHTMLCollection *rows;
  GetRows(&rows);
  nsIDOMNode *row = nsnull;
  rows->Item(aValue, &row);
  if (nsnull != row) {
    RemoveChild(row, &row);
  }
  NS_RELEASE(rows);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableSectionElement::StringToAttribute(nsIAtom* aAttribute,
                                      const nsAReadableString& aValue,
                                      nsHTMLValue& aResult)
{
  /* ignore these attributes, stored simply as strings
     ch
   */
  /* attributes that resolve to integers */
  if (aAttribute == nsHTMLAtoms::choff) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  /* attributes that resolve to integers or percents */
  else if (aAttribute == nsHTMLAtoms::height) {
    if (nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
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
  else if (aAttribute == nsHTMLAtoms::valign) {
    if (nsGenericHTMLElement::ParseTableVAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableSectionElement::AttributeToString(nsIAtom* aAttribute,
                                      const nsHTMLValue& aValue,
                                      nsAWritableString& aResult) const
{
  /* ignore these attributes, stored already as strings
     ch
   */
  /* ignore attributes that are of standard types
     choff, height, width, background, bgcolor
   */
  if (aAttribute == nsHTMLAtoms::align) {
    if (mInner.TableCellHAlignValueToString(aValue, aResult)) {
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
nsHTMLTableSectionElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                                    PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::align) || 
      (aAttribute == nsHTMLAtoms::valign) ||
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
nsHTMLTableSectionElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                        nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableSectionElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}


NS_IMETHODIMP
nsHTMLTableSectionElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
  PRUint32 sum = 0;
  mInner.SizeOf(aSizer, &sum, sizeof(*this));
  if (mRows) {
    PRUint32 asize;
    mRows->SizeOf(aSizer, &asize);
    sum += asize;
  }
  *aResult = sum;
#endif
  return NS_OK;
}
