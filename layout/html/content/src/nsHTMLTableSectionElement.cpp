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

class nsHTMLTableSectionElement : public nsGenericHTMLContainerElement,
                                  public nsIDOMHTMLTableSectionElement
{
public:
  nsHTMLTableSectionElement();
  virtual ~nsHTMLTableSectionElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLTableSectionElement
  NS_DECL_IDOMHTMLTABLESECTIONELEMENT

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc, 
                                          nsMapAttributesFunc& aMapFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32& aHint) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  GenericElementCollection *mRows;
};

nsresult
NS_NewHTMLTableSectionElement(nsIHTMLContent** aInstancePtrResult,
                              nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLTableSectionElement* it = new nsHTMLTableSectionElement();

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


nsHTMLTableSectionElement::nsHTMLTableSectionElement()
{
  mRows = nsnull;
}

nsHTMLTableSectionElement::~nsHTMLTableSectionElement()
{
  if (nsnull!=mRows) {
    mRows->ParentDestroyed();
    NS_RELEASE(mRows);
  }
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTableSectionElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTableSectionElement, nsGenericElement) 

NS_IMPL_HTMLCONTENT_QI(nsHTMLTableSectionElement,
                       nsGenericHTMLContainerElement,
                       nsIDOMHTMLTableSectionElement);


nsresult
nsHTMLTableSectionElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLTableSectionElement* it = new nsHTMLTableSectionElement();

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


NS_IMPL_STRING_ATTR(nsHTMLTableSectionElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLTableSectionElement, VAlign, valign)
NS_IMPL_STRING_ATTR(nsHTMLTableSectionElement, Ch, ch)
NS_IMPL_STRING_ATTR(nsHTMLTableSectionElement, ChOff, choff)


NS_IMETHODIMP
nsHTMLTableSectionElement::GetRows(nsIDOMHTMLCollection** aValue)
{
  *aValue = nsnull;

  if (!mRows) {
    //XXX why was this here NS_ADDREF(nsHTMLAtoms::tr);
    mRows = new GenericElementCollection(this, nsHTMLAtoms::tr);

    NS_ENSURE_TRUE(mRows, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mRows); // this table's reference, released in the destructor
  }

  mRows->QueryInterface(NS_GET_IID(nsIDOMHTMLCollection), (void **)aValue);

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableSectionElement::InsertRow(PRInt32 aIndex,
                                     nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;

  nsCOMPtr<nsIDOMHTMLCollection> rows;
  GetRows(getter_AddRefs(rows));

  PRUint32 rowCount;
  rows->GetLength(&rowCount);

  PRBool doInsert = (aIndex < PRInt32(rowCount));

  // create the row
  nsCOMPtr<nsIHTMLContent> rowContent;
  nsCOMPtr<nsINodeInfo> nodeInfo;

  mNodeInfo->NameChanged(nsHTMLAtoms::tr, *getter_AddRefs(nodeInfo));

  nsresult rv = NS_NewHTMLTableRowElement(getter_AddRefs(rowContent),
                                          nodeInfo);

  if (NS_SUCCEEDED(rv) && rowContent) {
    nsCOMPtr<nsIDOMNode> rowNode(do_QueryInterface(rowContent));

    if (NS_SUCCEEDED(rv) && rowNode) {
      nsCOMPtr<nsIDOMNode> retChild;

      if (doInsert) {
        PRInt32 refIndex = PR_MAX(aIndex, 0);   
        nsCOMPtr<nsIDOMNode> refRow;
        rows->Item(refIndex, getter_AddRefs(refRow));
        rv = InsertBefore(rowNode, refRow, getter_AddRefs(retChild));
      } else {
        rv = AppendChild(rowNode, getter_AddRefs(retChild));
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
nsHTMLTableSectionElement::DeleteRow(PRInt32 aValue)
{
  nsCOMPtr<nsIDOMHTMLCollection> rows;

  GetRows(getter_AddRefs(rows));

  nsCOMPtr<nsIDOMNode> row;

  rows->Item(aValue, getter_AddRefs(row));

  if (row) {
    nsCOMPtr<nsIDOMNode> retChild;

    RemoveChild(row, getter_AddRefs(retChild));
  }

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

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  if (aAttributes) {
    nsHTMLValue value;
    nsHTMLValue widthValue;
    nsStyleText* textStyle = nsnull;

    // align: enum
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mTextAlign = value.GetIntValue();
    }
  
    // valign: enum
    aAttributes->GetAttribute(nsHTMLAtoms::valign, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      if (nsnull==textStyle)
        textStyle = (nsStyleText*)aContext->GetMutableStyleData(eStyleStruct_Text);
      textStyle->mVerticalAlign.SetIntValue(value.GetIntValue(),
                                            eStyleUnit_Enumerated);
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

    nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext,
                                                      aPresContext);
    nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                  aPresContext);
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
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (!GetBackgroundAttributesImpact(aAttribute, aHint)) {
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
nsHTMLTableSectionElement::SizeOf(nsISizeOfHandler* aSizer,
                                  PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
