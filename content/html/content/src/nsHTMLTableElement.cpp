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
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableCaptionElement.h"
#include "nsIDOMHTMLTableSectionElement.h"
#include "nsCOMPtr.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "GenericElementCollection.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsHTMLParts.h"
#include "nsStyleUtil.h"

/* for collections */
#include "nsIDOMElement.h"
#include "nsGenericHTMLElement.h"
/* end for collections */

static NS_DEFINE_IID(kIDOMHTMLTableElementIID, NS_IDOMHTMLTABLEELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLTableCaptionElementIID, NS_IDOMHTMLTABLECAPTIONELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLTableSectionElementIID, NS_IDOMHTMLTABLESECTIONELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);

class GenericElementCollection;
class TableRowsCollection;

class nsHTMLTableElement :  public nsIDOMHTMLTableElement,
                            public nsIJSScriptObject,
                            public nsIHTMLContent
{
public:
  nsHTMLTableElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLTableElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLTableElement
  NS_DECL_IDOMHTMLTABLEELEMENT

  // nsIJSScriptObject
  NS_IMPL_IJSSCRIPTOBJECT_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
  GenericElementCollection *mTBodies;
  TableRowsCollection *mRows;
};


/* ------------------------------ TableRowsCollection -------------------------------- */
/**
 * This class provides a late-bound collection of rows in a table.
 * mParent is NOT ref-counted to avoid circular references
 */
class TableRowsCollection : public nsGenericDOMHTMLCollection 
{
public:
  TableRowsCollection(nsHTMLTableElement *aParent);
  virtual ~TableRowsCollection();

  NS_IMETHOD    GetLength(PRUint32* aLength);
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);
  NS_IMETHOD    NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn);

  NS_IMETHOD    ParentDestroyed();

#ifdef DEBUG
  void SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const {
    *aResult = sizeof(*this);
  }
#endif

protected:
  nsHTMLTableElement * mParent;
};


TableRowsCollection::TableRowsCollection(nsHTMLTableElement *aParent)
  : nsGenericDOMHTMLCollection()
{
  mParent = aParent;
}

TableRowsCollection::~TableRowsCollection()
{
  // we do NOT have a ref-counted reference to mParent, so do NOT release it!
  // this is to avoid circular references.  The instantiator who provided mParent
  // is responsible for managing our reference for us.
}

// we re-count every call.  A better implementation would be to set ourselves up as
// an observer of contentAppended, contentInserted, and contentDeleted
NS_IMETHODIMP 
TableRowsCollection::GetLength(PRUint32* aLength)
{
  if (nsnull==aLength)
    return NS_ERROR_NULL_POINTER;
  *aLength=0;
  nsresult rv = NS_OK;
  if (nsnull!=mParent)
  {
    // count the rows in the thead, tfoot, and all tbodies
    nsIDOMHTMLTableSectionElement *rowGroup;
    mParent->GetTHead(&rowGroup);
    if (nsnull!=rowGroup)
    {
      nsIContent *content=nsnull;
      rowGroup->QueryInterface(kIContentIID, (void **)&content);
      GenericElementCollection head(content, nsHTMLAtoms::tr);
      PRUint32 rows;
      head.GetLength(&rows);
      *aLength = rows;
      NS_RELEASE(content);
      NS_RELEASE(rowGroup);
    }
    mParent->GetTFoot(&rowGroup);
    if (nsnull!=rowGroup)
    {
      nsIContent *content=nsnull;
      rowGroup->QueryInterface(kIContentIID, (void **)&content);
      GenericElementCollection foot(content, nsHTMLAtoms::tr);
      PRUint32 rows;
      foot.GetLength(&rows);
      *aLength += rows;
      NS_RELEASE(content);
      NS_RELEASE(rowGroup);
    }
    nsIDOMHTMLCollection *tbodies;
    mParent->GetTBodies(&tbodies);
    if (nsnull!=tbodies)
    {
      rowGroup = nsnull;
      nsIDOMNode *node;
      PRUint32 theIndex=0;
      tbodies->Item(theIndex, &node);
      while (nsnull!=node)
      {
        nsIContent *content=nsnull;
        node->QueryInterface(kIContentIID, (void **)&content);
        GenericElementCollection body(content, nsHTMLAtoms::tr);
        PRUint32 rows;
        body.GetLength(&rows);
        *aLength += rows;
        theIndex++;
        NS_RELEASE(content);
        NS_RELEASE(node);
        tbodies->Item(theIndex, &node);
      }
      NS_RELEASE(tbodies);
    }
  }
  return rv;
}

// increments aReturn refcnt by 1
NS_IMETHODIMP 
TableRowsCollection::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;
  nsresult rv = NS_OK;
  PRUint32 count = 0;
 
  if (nsnull != mParent) {
    nsIDOMHTMLTableSectionElement *rowGroup;

    // check the thead
    mParent->GetTHead(&rowGroup);
    if (nsnull != rowGroup) {
      nsIContent *content = nsnull;
      rowGroup->QueryInterface(kIContentIID, (void **)&content);
      GenericElementCollection head(content, nsHTMLAtoms::tr);
      PRUint32 rowsInHead;
      head.GetLength(&rowsInHead);
      count = rowsInHead;
      NS_RELEASE(content);
      NS_RELEASE(rowGroup);
      if (count > aIndex) {
        head.Item(aIndex, aReturn);
        return NS_OK; 
      }
    }

	// check the tbodies
    nsIDOMHTMLCollection *tbodies;
    mParent->GetTBodies(&tbodies);
    if (nsnull != tbodies) {
      rowGroup = nsnull;
      nsIDOMNode *node;
      PRUint32 theIndex=0;
      tbodies->Item(theIndex, &node);
      while (nsnull != node) {
        nsIContent *content = nsnull;
        node->QueryInterface(kIContentIID, (void **)&content);
        GenericElementCollection body(content, nsHTMLAtoms::tr);
        NS_RELEASE(content);
        NS_RELEASE(node);
        PRUint32 rows;
        body.GetLength(&rows);
        if ((count+rows) > aIndex) {
          body.Item(aIndex-count, aReturn);
          NS_RELEASE(tbodies);
		      return NS_OK;
        }
        count += rows;
        theIndex++;
        tbodies->Item(theIndex, &node);
      }
      NS_RELEASE(tbodies);
    }

    // check the tfoot
    mParent->GetTFoot(&rowGroup);
    if (nsnull != rowGroup) {
      nsIContent *content = nsnull;
      rowGroup->QueryInterface(kIContentIID, (void **)&content);
      GenericElementCollection foot(content, nsHTMLAtoms::tr);
      foot.Item(aIndex-count, aReturn);
      NS_RELEASE(content);
      NS_RELEASE(rowGroup);
    }
  }
  return rv;
}

NS_IMETHODIMP 
TableRowsCollection::NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  // FIXME: Implement this!

  *aReturn = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
TableRowsCollection::ParentDestroyed()
{
  // see comment in destructor, do NOT release mParent!
  mParent = nsnull;
  return NS_OK;
}

/* ------------------------------ nsHTMLTableElement -------------------------------- */
// the class declaration is at the top of this file

nsresult
NS_NewHTMLTableElement(nsIHTMLContent** aInstancePtrResult,
                       nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLTableElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}


nsHTMLTableElement::nsHTMLTableElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
  mTBodies=nsnull;
  mRows=nsnull;
}

nsHTMLTableElement::~nsHTMLTableElement()
{
  if (nsnull!=mTBodies)
  {
    mTBodies->ParentDestroyed();
    NS_RELEASE(mTBodies);
  }
  if (nsnull!=mRows)
  {
    mRows->ParentDestroyed();
    NS_RELEASE(mRows);
  }
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
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLTableElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLTableElement* it = new nsHTMLTableElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

// the DOM spec says border, cellpadding, cellSpacing are all "wstring"
// in fact, they are integers or they are meaningless.  so we store them here as ints.

NS_IMPL_STRING_ATTR(nsHTMLTableElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, BgColor, bgcolor)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Border, border)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, CellPadding, cellpadding)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, CellSpacing, cellspacing)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Frame, frame)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Rules, rules)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Summary, summary)
NS_IMPL_STRING_ATTR(nsHTMLTableElement, Width, width)

NS_IMETHODIMP
nsHTMLTableElement::GetCaption(nsIDOMHTMLTableCaptionElement** aValue)
{
  *aValue = nsnull;
  nsCOMPtr<nsIDOMNode> child;
  mInner.GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    nsCOMPtr<nsIDOMHTMLTableCaptionElement> caption = do_QueryInterface(child);
    if (caption)
    {
      *aValue = caption;
      NS_ADDREF(*aValue);
      break;
    }
    nsCOMPtr<nsIDOMNode> temp = child;
    temp->GetNextSibling(getter_AddRefs(child));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::SetCaption(nsIDOMHTMLTableCaptionElement* aValue)
{
  nsresult rv = DeleteCaption();
  if (NS_SUCCEEDED(rv)) {
    if (nsnull!=aValue)
    {
      nsCOMPtr<nsIDOMNode> resultingChild;
      mInner.AppendChild(aValue, getter_AddRefs(resultingChild));
    }
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLTableElement::GetTHead(nsIDOMHTMLTableSectionElement** aValue)
{
  *aValue = nsnull;
  nsCOMPtr<nsIDOMNode> child;
  mInner.GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    nsCOMPtr<nsIDOMHTMLTableSectionElement> section = do_QueryInterface(child);
    if (section)
    {
      nsCOMPtr<nsIAtom> tag;
      nsCOMPtr<nsIContent> content = do_QueryInterface(section);
      
      content->GetTag(*getter_AddRefs(tag));
      if (tag.get() == nsHTMLAtoms::thead)
      {
        *aValue = section;
        NS_ADDREF(*aValue);
        break;
      }
    }
    nsCOMPtr<nsIDOMNode> temp = child;
    temp->GetNextSibling(getter_AddRefs(child));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::SetTHead(nsIDOMHTMLTableSectionElement* aValue)
{
  nsresult rv = DeleteTHead();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (nsnull!=aValue) {
    nsCOMPtr<nsIDOMNode> child;
    rv = mInner.GetFirstChild(getter_AddRefs(child));
    if (NS_FAILED(rv)) {
      return rv;
    }
     
    nsCOMPtr<nsIDOMNode> resultChild;
    rv = mInner.InsertBefore(aValue, child, getter_AddRefs(resultChild));
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTableElement::GetTFoot(nsIDOMHTMLTableSectionElement** aValue)
{
  *aValue = nsnull;
  nsCOMPtr<nsIDOMNode> child;
  mInner.GetFirstChild(getter_AddRefs(child));
  while (child)
  {
    nsCOMPtr<nsIDOMHTMLTableSectionElement> section=do_QueryInterface(child);
    if (section)
    {
      nsCOMPtr<nsIAtom> tag;
      nsCOMPtr<nsIContent> content = do_QueryInterface(section);

      content->GetTag(*getter_AddRefs(tag));
      if (tag.get() == nsHTMLAtoms::tfoot)
      {
        *aValue = section;
        NS_ADDREF(*aValue);
        break;
      }
    }
    nsCOMPtr<nsIDOMNode> temp = child;
    temp->GetNextSibling(getter_AddRefs(child));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::SetTFoot(nsIDOMHTMLTableSectionElement* aValue)
{
  nsresult rv = DeleteTFoot();
  if (NS_SUCCEEDED(rv)) {
    if (nsnull!=aValue)
    {
      nsCOMPtr<nsIDOMNode> resultingChild;
      mInner.AppendChild(aValue, getter_AddRefs(resultingChild));
    }
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLTableElement::GetRows(nsIDOMHTMLCollection** aValue)
{
  if (nsnull==mRows)
  {
    // XXX why was this here NS_ADDREF(nsHTMLAtoms::tr);
    mRows = new TableRowsCollection(this);
    NS_ADDREF(mRows); // this table's reference, released in the destructor
  }
  mRows->QueryInterface(kIDOMHTMLCollectionIID, (void **)aValue);   // caller's addref 
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::GetTBodies(nsIDOMHTMLCollection** aValue)
{
  if (nsnull==mTBodies)
  {
    mTBodies = new GenericElementCollection((nsIContent*)this, nsHTMLAtoms::tbody);
    NS_ADDREF(mTBodies); // this table's reference, released in the destructor
  }
  mTBodies->QueryInterface(kIDOMHTMLCollectionIID, (void **)aValue);  // caller's addref 
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::CreateTHead(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMHTMLTableSectionElement> head;
  GetTHead(getter_AddRefs(head));
  if (head)
  { // return the existing thead
    head->QueryInterface(kIDOMHTMLElementIID, (void **)aValue);  // caller's addref
    NS_ASSERTION(nsnull!=*aValue, "head must be a DOMHTMLElement");
  }
  else
  { // create a new head rowgroup
    nsCOMPtr<nsIHTMLContent> newHead;

    nsCOMPtr<nsINodeInfo> nodeInfo;
    mInner.mNodeInfo->NameChanged(nsHTMLAtoms::thead,
                                  *getter_AddRefs(nodeInfo));

    rv = NS_NewHTMLTableSectionElement(getter_AddRefs(newHead),nodeInfo);
    if (NS_SUCCEEDED(rv) && newHead)
    {
      nsCOMPtr<nsIDOMNode> child;
      rv = mInner.GetFirstChild(getter_AddRefs(child));
      if (NS_FAILED(rv)) {
        return rv;
      }
     
      newHead->QueryInterface(kIDOMHTMLElementIID, (void **)aValue); // caller's addref

      nsCOMPtr<nsIDOMNode> resultChild;
      rv = mInner.InsertBefore(*aValue, child, getter_AddRefs(resultChild));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteTHead()
{
  nsCOMPtr<nsIDOMHTMLTableSectionElement> childToDelete;
  nsresult rv = GetTHead(getter_AddRefs(childToDelete));
  if ((NS_SUCCEEDED(rv)) && childToDelete)
  {
    nsCOMPtr<nsIDOMNode> resultingChild;
    mInner.RemoveChild(childToDelete, getter_AddRefs(resultingChild)); // mInner does the notification
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::CreateTFoot(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMHTMLTableSectionElement> foot;
  GetTFoot(getter_AddRefs(foot));
  if (nsnull!=foot)
  { // return the existing tfoot
    foot->QueryInterface(kIDOMHTMLElementIID, (void **)aValue);  // caller's addref
    NS_ASSERTION(nsnull!=*aValue, "foot must be a DOMHTMLElement");
  }
  else
  { // create a new foot rowgroup
    nsCOMPtr<nsIHTMLContent> newFoot;

    nsCOMPtr<nsINodeInfo> nodeInfo;
    mInner.mNodeInfo->NameChanged(nsHTMLAtoms::tfoot,
                                  *getter_AddRefs(nodeInfo));

    rv = NS_NewHTMLTableSectionElement(getter_AddRefs(newFoot),nodeInfo);
    if (NS_SUCCEEDED(rv) && newFoot)
    {
      rv = mInner.AppendChildTo(newFoot, PR_TRUE);
      newFoot->QueryInterface(kIDOMHTMLElementIID, (void **)aValue); // caller's addref
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteTFoot()
{
  nsCOMPtr<nsIDOMHTMLTableSectionElement> childToDelete;
  nsresult rv = GetTFoot(getter_AddRefs(childToDelete));
  if ((NS_SUCCEEDED(rv)) && childToDelete)
  {
    nsCOMPtr<nsIDOMNode> resultingChild;
    mInner.RemoveChild(childToDelete, getter_AddRefs(resultingChild)); // mInner does the notification
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::CreateCaption(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMHTMLTableCaptionElement> caption;
  GetCaption(getter_AddRefs(caption));
  if (caption)
  { // return the existing thead
    caption->QueryInterface(kIDOMHTMLElementIID, (void **)aValue);  // caller's addref
    NS_ASSERTION(nsnull!=*aValue, "caption must be a DOMHTMLElement");
  }
  else
  { // create a new head rowgroup
    nsCOMPtr<nsIHTMLContent> newCaption;

    nsCOMPtr<nsINodeInfo> nodeInfo;
    mInner.mNodeInfo->NameChanged(nsHTMLAtoms::caption,
                                  *getter_AddRefs(nodeInfo));

    rv = NS_NewHTMLTableCaptionElement(getter_AddRefs(newCaption),nodeInfo);
    if (NS_SUCCEEDED(rv) && newCaption)
    {
      rv = mInner.AppendChildTo(newCaption, PR_TRUE);
      newCaption->QueryInterface(kIDOMHTMLElementIID, (void **)aValue); // caller's addref
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteCaption()
{
  nsCOMPtr<nsIDOMHTMLTableCaptionElement> childToDelete;
  nsresult rv = GetCaption(getter_AddRefs(childToDelete));
  if ((NS_SUCCEEDED(rv)) && childToDelete)
  {
    nsCOMPtr<nsIDOMNode> resultingChild;
    mInner.RemoveChild(childToDelete, getter_AddRefs(resultingChild)); // mInner does the notification
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  /* get the ref row at aIndex
     if there is one, 
       get it's parent
       insert the new row just before the ref row
     else
       get the first row group
       insert the new row as its first child
  */
  *aValue = nsnull;
  nsresult rv;
  PRInt32 refIndex = aIndex;  // use local variable refIndex so we can remember original aIndex
  if (0>refIndex) // negative aIndex treated as 0
    refIndex=0;
  nsIDOMHTMLCollection *rows;
  GetRows(&rows);
  PRUint32 rowCount;
  rows->GetLength(&rowCount);
  if (0<rowCount)
  {
    if (rowCount<=PRUint32(refIndex))
      refIndex=rowCount-1;  // we set refIndex to the last row so we can get the last row's parent
                            // we then do an AppendChild below if (rowCount<aIndex)
    nsIDOMNode *refRow;
    rows->Item(refIndex, &refRow);
    nsIDOMNode *parent;
    refRow->GetParentNode(&parent);
    // create the row
    nsIHTMLContent *newRow=nsnull;

    nsCOMPtr<nsINodeInfo> nodeInfo;
    mInner.mNodeInfo->NameChanged(nsHTMLAtoms::tr, *getter_AddRefs(nodeInfo));

    rv = NS_NewHTMLTableRowElement(&newRow, nodeInfo);
    if (NS_SUCCEEDED(rv) && (nsnull!=newRow))
    {
      nsIDOMNode *newRowNode=nsnull;
      newRow->QueryInterface(kIDOMNodeIID, (void **)&newRowNode); // caller's addref
      if ((0<=aIndex) && (PRInt32(rowCount)<=aIndex)) // the index is greater than the number of rows, so just append
        rv = parent->AppendChild(newRowNode, (nsIDOMNode **)aValue);
      else  // insert the new row before the reference row we found above
        rv = parent->InsertBefore(newRowNode, refRow, (nsIDOMNode **)aValue);
      NS_RELEASE(newRow);
    }
    NS_RELEASE(parent);
    NS_RELEASE(refRow);
    NS_RELEASE(rows);
  }
  else
  { // the row count was 0, so 
    // find the first row group and insert there as first child
    nsIDOMNode *rowGroup=nsnull;
    GenericElementCollection head((nsIContent*)this, nsHTMLAtoms::thead);
    PRUint32 length=0;
    head.GetLength(&length);
    if (0!=length)
    {
      head.Item(0, &rowGroup);
    }
    else
    {
      GenericElementCollection body((nsIContent*)this, nsHTMLAtoms::tbody);
      length=0;
      body.GetLength(&length);
      if (0!=length)
      {
        body.Item(0, &rowGroup);
      }
      else
      {
        GenericElementCollection foot((nsIContent*)this, nsHTMLAtoms::tfoot);
        length=0;
        foot.GetLength(&length);
        if (0!=length)
        {
          foot.Item(0, &rowGroup);
        }
      }
    }
    if (nsnull==rowGroup)
    { // need to create a TBODY
      nsIHTMLContent *newRowGroup=nsnull;

      nsCOMPtr<nsINodeInfo> nodeInfo;
      mInner.mNodeInfo->NameChanged(nsHTMLAtoms::tbody,
                                    *getter_AddRefs(nodeInfo));

      rv = NS_NewHTMLTableSectionElement(&newRowGroup, nodeInfo);
      if (NS_SUCCEEDED(rv) && (nsnull!=newRowGroup))
      {
        rv = mInner.AppendChildTo(newRowGroup, PR_TRUE);
        newRowGroup->QueryInterface(kIDOMNodeIID, (void **)&rowGroup);
        NS_RELEASE(newRowGroup);
      }
    }
    if (nsnull!=rowGroup)
    {
      nsIHTMLContent *newRow=nsnull;

      nsCOMPtr<nsINodeInfo> nodeInfo;
      mInner.mNodeInfo->NameChanged(nsHTMLAtoms::tr,
                                    *getter_AddRefs(nodeInfo));

      rv = NS_NewHTMLTableRowElement(&newRow, nodeInfo);
      nsIContent *rowGroupContent=nsnull;
      rowGroup->QueryInterface(kIContentIID, (void **)&rowGroupContent);
      GenericElementCollection rowGroupRows(rowGroupContent, nsHTMLAtoms::tr);
      nsIDOMNode *firstRow=nsnull;
      rowGroupRows.Item(0, &firstRow);  // it's ok if this returns nsnull
      if (NS_SUCCEEDED(rv) && (nsnull!=newRow))
      {
        nsIDOMNode *newRowNode;
        newRow->QueryInterface(kIDOMNodeIID, (void **)&newRowNode);
        rowGroup->InsertBefore(newRowNode, firstRow, (nsIDOMNode **)aValue);
        NS_RELEASE(newRowNode);
        NS_RELEASE(newRow);
      }
      NS_IF_RELEASE(firstRow);  // it's legal for firstRow to be nsnull
      NS_RELEASE(rowGroupContent);
      NS_RELEASE(rowGroup);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteRow(PRInt32 aValue)
{
  nsCOMPtr<nsIDOMHTMLCollection> rows;
  GetRows(getter_AddRefs(rows));
  nsCOMPtr<nsIDOMNode> row;
  rows->Item(aValue, getter_AddRefs(row));
  if (row)
  {
    nsCOMPtr<nsIDOMNode> parent=nsnull;
    row->GetParentNode(getter_AddRefs(parent));
    if (parent)
    {
      nsCOMPtr<nsIDOMNode> deleted_row;
      parent->RemoveChild(row, getter_AddRefs(deleted_row));
    }
  }
  return NS_OK;
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
                                      const nsAReadableString& aValue,
                                      nsHTMLValue& aResult)
{
  /* ignore summary, just a string */
  /* attributes that resolve to pixels, with min=0 */
  if ((aAttribute == nsHTMLAtoms::cellspacing) ||
      (aAttribute == nsHTMLAtoms::cellpadding)) {
    if (nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  /* attributes that are either empty, or integers, with min=0 */
  else if (aAttribute == nsHTMLAtoms::cols) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  /* attributes that are either empty, or pixels */
  else if (aAttribute == nsHTMLAtoms::border) {
    PRInt32 min = (aValue.IsEmpty()) ? 1 : 0;
    if (nsGenericHTMLElement::ParseValue(aValue, min, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
    else { 
      // XXX this should really be NavQuirks only to allow non numeric value
      aResult.SetPixelValue(1);
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
    if (mInner.ParseTableHAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::bgcolor) {
    if (nsGenericHTMLElement::ParseColor(aValue, mInner.mDocument, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::bordercolor) {
    if (nsGenericHTMLElement::ParseColor(aValue, mInner.mDocument, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::frame) {
    if (nsGenericHTMLElement::ParseEnumValue(aValue, kFrameTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::layout) {
    if (nsGenericHTMLElement::ParseEnumValue(aValue, kLayoutTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::rules) {
    if (nsGenericHTMLElement::ParseEnumValue(aValue, kRulesTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::hspace) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::vspace) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableElement::AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const
{
  /* ignore summary, just a string */
  /* ignore attributes that are of standard types
     border, cellpadding, cellspacing, cols, height, width, background, bgcolor
   */
  if (aAttribute == nsHTMLAtoms::align) {
    if (mInner.TableHAlignValueToString(aValue, aResult)) {
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

static void 
MapTableFrameInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext*        aContext,
                  nsIPresContext*                aPresContext, 
                  nsStyleSpacing*                aSpacing,
                  PRUint8                        aBorderStyle)
{
  // set up defaults
  for (PRInt32 sideX = NS_SIDE_TOP; sideX <= NS_SIDE_LEFT; sideX++) {
    if (aSpacing->GetBorderStyle(sideX) == NS_STYLE_BORDER_STYLE_NONE) {
      aSpacing->SetBorderStyle(sideX, aBorderStyle);
    }
  }

  nsHTMLValue frameValue;
  // 0 out the sides that we want to hide based on the frame attribute
  aAttributes->GetAttribute(nsHTMLAtoms::frame, frameValue);
  if (frameValue.GetUnit() == eHTMLUnit_Enumerated)
  {
    // adjust the border style based on the value of frame
    switch (frameValue.GetIntValue())
    {
    case NS_STYLE_TABLE_FRAME_NONE:
      aSpacing->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
      break;
    case NS_STYLE_TABLE_FRAME_ABOVE:
      aSpacing->SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
      break;
    case NS_STYLE_TABLE_FRAME_BELOW:
      aSpacing->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
      break;
    case NS_STYLE_TABLE_FRAME_HSIDES:
      aSpacing->SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
      break;
    case NS_STYLE_TABLE_FRAME_LEFT:
      aSpacing->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
      break;
    case NS_STYLE_TABLE_FRAME_RIGHT:
      aSpacing->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
      break;
    case NS_STYLE_TABLE_FRAME_VSIDES:
      aSpacing->SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
      aSpacing->SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
      break;
    // BOX and BORDER are ignored, the caller has already set all the border sides
    // any illegal value is also ignored
    }
  }
}

static void 
MapTableBorderInto(const nsIHTMLMappedAttributes* aAttributes,
                   nsIMutableStyleContext*        aContext,
                   nsIPresContext*                aPresContext,
                   PRUint8                        aBorderStyle)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  nsHTMLValue borderValue;

  aAttributes->GetAttribute(nsHTMLAtoms::border, borderValue);
  if (borderValue.GetUnit() == eHTMLUnit_Null)
  { // the absence of "border" with the presence of "frame" implies border = 1 pixel
    nsHTMLValue frameValue;
    aAttributes->GetAttribute(nsHTMLAtoms::frame, frameValue);
    if (frameValue.GetUnit() != eHTMLUnit_Null)
      borderValue.SetPixelValue(1);
  }
  if (borderValue.GetUnit() != eHTMLUnit_Null) {
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetMutableStyleData(eStyleStruct_Spacing);
    nsStyleTable *tableStyle = (nsStyleTable*)
      aContext->GetMutableStyleData(eStyleStruct_Table);
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nsStyleCoord twips;
    if (borderValue.GetUnit() != eHTMLUnit_Pixel) {
      // empty values of border get rules=all and frame=border
      tableStyle->mRules = NS_STYLE_TABLE_RULES_ALL;  
      tableStyle->mFrame = NS_STYLE_TABLE_FRAME_BORDER;
      twips.SetCoordValue(NSIntPixelsToTwips(1, p2t));
    }
    else {
      PRInt32 borderThickness = borderValue.GetPixelValue();
      twips.SetCoordValue(NSIntPixelsToTwips(borderThickness, p2t));
      if (0 != borderThickness) {
        // border != 0 implies rules=all and frame=border
        tableStyle->mRules = NS_STYLE_TABLE_RULES_ALL;  
        tableStyle->mFrame = NS_STYLE_TABLE_FRAME_BORDER;
      }
      else {
        // border = 0 implies rules=none and frame=void
        tableStyle->mRules = NS_STYLE_TABLE_RULES_NONE; 
        tableStyle->mFrame = NS_STYLE_TABLE_FRAME_NONE;
      }
    }

    // by default, set all border sides to the specified width
    spacing->mBorder.SetTop(twips);
    spacing->mBorder.SetRight(twips);
    spacing->mBorder.SetBottom(twips);
    spacing->mBorder.SetLeft(twips);
    // then account for the frame attribute
    MapTableFrameInto(aAttributes, aContext, aPresContext, spacing, aBorderStyle);
  }
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext*        aContext,
                  nsIPresContext*                aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  if (nsnull!=aAttributes)
  {
    float sp2t;
    aPresContext->GetScaledPixelsToTwips(&sp2t);
    nsHTMLValue value;

    const nsStyleDisplay* readDisplay = (nsStyleDisplay*)
                aContext->GetStyleData(eStyleStruct_Display);
    if (readDisplay && (readDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL)) {
      // set the cell's border from the table
      aAttributes->GetAttribute(nsHTMLAtoms::border, value);
      if (((value.GetUnit() == eHTMLUnit_Pixel) && (value.GetPixelValue() > 0)) ||
          (value.GetUnit() == eHTMLUnit_Empty)) {
        float p2t;
        aPresContext->GetPixelsToTwips(&p2t);
        nscoord onePixel = NSIntPixelsToTwips(1, p2t);
        nsStyleSpacing* spacingStyle = (nsStyleSpacing*)aContext->GetMutableStyleData(eStyleStruct_Spacing);
        nsStyleCoord width;
        width.SetCoordValue(onePixel);

        spacingStyle->mBorder.SetTop(width);
        spacingStyle->mBorder.SetLeft(width);
        spacingStyle->mBorder.SetBottom(width);
        spacingStyle->mBorder.SetRight(width);

        nsCompatibility mode;
        aPresContext->GetCompatibilityMode(&mode);
        PRUint8 borderStyle = (eCompatibility_NavQuirks == mode) 
                              ? NS_STYLE_BORDER_STYLE_BG_INSET : NS_STYLE_BORDER_STYLE_INSET;
          // BG_INSET results in a border color based on background colors
          // used for NavQuirks only...

        spacingStyle->SetBorderStyle(NS_SIDE_TOP,    borderStyle);
        spacingStyle->SetBorderStyle(NS_SIDE_LEFT,   borderStyle);
        spacingStyle->SetBorderStyle(NS_SIDE_BOTTOM, borderStyle);
        spacingStyle->SetBorderStyle(NS_SIDE_RIGHT,  borderStyle);
      }
    }
    else {  // handle attributes for table
      // width
      aAttributes->GetAttribute(nsHTMLAtoms::width, value);
      if (value.GetUnit() != eHTMLUnit_Null) {
        nsStylePosition* position = (nsStylePosition*)
          aContext->GetMutableStyleData(eStyleStruct_Position);
        switch (value.GetUnit()) {
        case eHTMLUnit_Percent:
          // 0 width remains default auto
          //if (value.GetPercentValue() > 0.0f) {
            position->mWidth.SetPercentValue(value.GetPercentValue());
          //}
          break;

        case eHTMLUnit_Pixel:
          // 0 width remains default auto
          //if (value.GetPixelValue() > 0) {
            position->mWidth.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), sp2t));
          //}
          break;
        default:
          break;
        }
      }

      // height
      aAttributes->GetAttribute(nsHTMLAtoms::height, value);
      if (value.GetUnit() != eHTMLUnit_Null) {
        nsStylePosition* position = (nsStylePosition*)
          aContext->GetMutableStyleData(eStyleStruct_Position);
        switch (value.GetUnit()) {
        case eHTMLUnit_Percent:
          position->mHeight.SetPercentValue(value.GetPercentValue());
          break;

        case eHTMLUnit_Pixel:
          position->mHeight.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), sp2t));
          break;
        default:
          break;
        }
      }

      nsStyleSpacing* spacing = (nsStyleSpacing*)
        aContext->GetMutableStyleData(eStyleStruct_Spacing);

      // default border style is the Nav4.6 extension which uses the background color as the
      // basis of the outset border. If the table has a transparant background then it finds
      // the closest ancestor that has a non transparant backgound. NS_STYLE_BORDER_OUTSET 
      // uses the border color of the table and if that is not set, then it uses the color.
      nsCompatibility mode;
      aPresContext->GetCompatibilityMode(&mode);
      PRUint8 borderStyle = (eCompatibility_NavQuirks == mode) 
                            ? NS_STYLE_BORDER_STYLE_BG_OUTSET : NS_STYLE_BORDER_STYLE_OUTSET;

      // bordercolor
      aAttributes->GetAttribute(nsHTMLAtoms::bordercolor, value);
      if ((eHTMLUnit_Color == value.GetUnit()) || (eHTMLUnit_ColorName == value.GetUnit())) {
        nscolor color = value.GetColorValue();
        spacing->SetBorderColor(0, color);
        spacing->SetBorderColor(1, color);
        spacing->SetBorderColor(2, color);
        spacing->SetBorderColor(3, color);
        borderStyle = NS_STYLE_BORDER_STYLE_OUTSET; // use css outset
      }

      // border and frame
      MapTableBorderInto(aAttributes, aContext, aPresContext, borderStyle);

      // align; Check for enumerated type (it may be another type if
      // illegal)
      aAttributes->GetAttribute(nsHTMLAtoms::align, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated) {
        if ((NS_STYLE_TEXT_ALIGN_CENTER == value.GetIntValue()) ||
            (NS_STYLE_TEXT_ALIGN_MOZ_CENTER == value.GetIntValue())) {
          nsStyleCoord otto(eStyleUnit_Auto);
          spacing->mMargin.SetLeft(otto);
          spacing->mMargin.SetRight(otto);
        }
        else {
          nsStyleDisplay* display = (nsStyleDisplay*)
            aContext->GetMutableStyleData(eStyleStruct_Display);
          switch (value.GetIntValue()) {
          case NS_STYLE_TEXT_ALIGN_LEFT:
            display->mFloats = NS_STYLE_FLOAT_LEFT;
            break;

          case NS_STYLE_TEXT_ALIGN_RIGHT:
          case NS_STYLE_TEXT_ALIGN_MOZ_RIGHT:
            display->mFloats = NS_STYLE_FLOAT_RIGHT;
            break;
          }
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
        tableStyle->mCellPadding.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), sp2t));
      }
      else if (value.GetUnit() == eHTMLUnit_Percent) {
        if (nsnull==tableStyle)
          tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
        tableStyle->mCellPadding.SetPercentValue(value.GetPercentValue());
      }

      // cellspacing  (reuses tableStyle if already resolved)
      // ua.css sets cellspacing
      aAttributes->GetAttribute(nsHTMLAtoms::cellspacing, value);
      if (value.GetUnit() == eHTMLUnit_Pixel) {
        if (nsnull==tableStyle)
          tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
        tableStyle->mBorderSpacingX.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), sp2t));
        tableStyle->mBorderSpacingY.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), sp2t));
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

      // rules, must come after handling of border which set the default
      aAttributes->GetAttribute(nsHTMLAtoms::rules, value);
      if (value.GetUnit() == eHTMLUnit_Enumerated) {
        if (nsnull==tableStyle)
          tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
        tableStyle->mRules = value.GetIntValue();
      }

      // hspace is mapped into left and right margin, 
      // vspace is mapped into top and bottom margins
      // - *** Quirks Mode only ***
      if (eCompatibility_NavQuirks == mode) {
        aAttributes->GetAttribute(nsHTMLAtoms::hspace, value);
        if (value.GetUnit() == eHTMLUnit_Pixel) {
          nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), sp2t);
          nsStyleCoord hspace(twips);
          spacing->mMargin.SetLeft(hspace);
          spacing->mMargin.SetRight(hspace);
        }
        aAttributes->GetAttribute(nsHTMLAtoms::vspace, value);
        if (value.GetUnit() == eHTMLUnit_Pixel) {
          nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), sp2t);
          nsStyleCoord vspace(twips);
          spacing->mMargin.SetTop(vspace);
          spacing->mMargin.SetBottom(vspace);
        }
      }

      //background: color
      nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext, aPresContext);
      nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
    }
  }
}

NS_IMETHODIMP
nsHTMLTableElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::align) || 
      (aAttribute == nsHTMLAtoms::layout) ||
      (aAttribute == nsHTMLAtoms::cellpadding) ||
      (aAttribute == nsHTMLAtoms::cellspacing) ||
      (aAttribute == nsHTMLAtoms::cols) ||
      (aAttribute == nsHTMLAtoms::rules) ||
      (aAttribute == nsHTMLAtoms::border) ||
      (aAttribute == nsHTMLAtoms::frame) ||
      (aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::height) ||
      (aAttribute == nsHTMLAtoms::hspace) ||
      (aAttribute == nsHTMLAtoms::vspace)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (aAttribute == nsHTMLAtoms::bordercolor) {
    aHint = NS_STYLE_HINT_VISUAL;
  }
  else if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (! nsGenericHTMLElement::GetBackgroundAttributesImpact(aAttribute, aHint)) {
      aHint = NS_STYLE_HINT_CONTENT;
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                 nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLTableElement::HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}


NS_IMETHODIMP
nsHTMLTableElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
  PRUint32 sum = 0;
  mInner.SizeOf(aSizer, &sum, sizeof(*this));
  if (mTBodies) {
    PRUint32 asize;
    mTBodies->SizeOf(aSizer, &asize);
    sum += asize;
  }
  if (mRows) {
    PRUint32 asize;
    mRows->SizeOf(aSizer, &asize);
    sum += asize;
  }
  *aResult = sum;
#endif
  return NS_OK;
}
