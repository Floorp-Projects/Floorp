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
#include "nsIDOMHTMLTableCaptionElement.h"
#include "nsIDOMHTMLTableSectionElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "GenericElementCollection.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsHTMLParts.h"

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
                            public nsIScriptObjectOwner,
                            public nsIDOMEventReceiver,
                            public nsIHTMLContent
{
public:
  nsHTMLTableElement(nsIAtom* aTag);
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
  NS_IMETHOD    NamedItem(const nsString& aName, nsIDOMNode** aReturn);

  NS_IMETHOD    ParentDestroyed();

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
      PRUint32 index=0;
      tbodies->Item(index, &node);
      while (nsnull!=node)
      {
        nsIContent *content=nsnull;
        node->QueryInterface(kIContentIID, (void **)&content);
        GenericElementCollection body(content, nsHTMLAtoms::tr);
        PRUint32 rows;
        body.GetLength(&rows);
        *aLength += rows;
        index++;
        NS_RELEASE(content);
        NS_RELEASE(node);
        tbodies->Item(index, &node);
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
      PRUint32 index=0;
      tbodies->Item(index, &node);
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
        index++;
        tbodies->Item(index, &node);
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
TableRowsCollection::NamedItem(const nsString& aName, nsIDOMNode** aReturn)
{
  nsresult rv = NS_OK;
  if (nsnull!=mParent)
  {
  }
  return rv;
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
  nsIDOMNode* child=nsnull;
  mInner.GetFirstChild(&child);
  while (nsnull!=child)
  {
    nsIDOMHTMLTableCaptionElement *caption=nsnull;
    nsresult rv = child->QueryInterface(kIDOMHTMLTableCaptionElementIID, (void**)&caption);
    if ((NS_SUCCEEDED(rv)) && (nsnull!=caption))
    {
      *aValue = caption;
      break;
    }
    child->GetNextSibling(&child);
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
      nsIDOMNode* resultingChild;
      mInner.AppendChild(aValue, &resultingChild);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLTableElement::GetTHead(nsIDOMHTMLTableSectionElement** aValue)
{
  /* this is a better implementation, but GetElementsByTagName isn't implemented yet */
  /*
  *aValue = nsnull;
  nsIDOMNodeList *kids=nsnull;
  nsAutoString theadAsString;
  nsHTMLAtoms::thead->ToString(theadAsString);
  nsresult rv = mInner.GetElementsByTagName(theadAsString, &kids);
  if ((NS_SUCCEEDED(rv)) && (nsnull!=kids))
  {
    nsIDOMNode *thead=nsnull;
    rv = kids->Item(0, &thead);
    if (NS_SUCCEEDED(rv))
      *aValue = (nsIDOMHTMLTableSectionElement*)thead;
  }
  */

  *aValue = nsnull;
  nsIDOMNode* child=nsnull;
  mInner.GetFirstChild(&child);
  while (nsnull!=child)
  {
    nsIDOMHTMLTableSectionElement *section=nsnull;
    nsresult rv = child->QueryInterface(kIDOMHTMLTableSectionElementIID, (void**)&section);
    if ((NS_SUCCEEDED(rv)) && (nsnull!=section))
    {
      nsString tag;
      section->GetTagName(tag);
      nsAutoString theadAsString;
      nsHTMLAtoms::thead->ToString(theadAsString);
      if (theadAsString==tag)
      {
        *aValue = section;
        break;
      }
    }
    child->GetNextSibling(&child);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::SetTHead(nsIDOMHTMLTableSectionElement* aValue)
{
  nsresult rv = DeleteTHead();
  if (NS_SUCCEEDED(rv)) {
    if (nsnull!=aValue)
    {
      nsIDOMNode* resultingChild;
      mInner.AppendChild(aValue, &resultingChild);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLTableElement::GetTFoot(nsIDOMHTMLTableSectionElement** aValue)
{
  *aValue = nsnull;
  nsIDOMNode* child=nsnull;
  mInner.GetFirstChild(&child);
  while (nsnull!=child)
  {
    nsIDOMHTMLTableSectionElement *section=nsnull;
    nsresult rv = child->QueryInterface(kIDOMHTMLTableSectionElementIID, (void**)&section);
    if ((NS_SUCCEEDED(rv)) && (nsnull!=section))
    {
      nsString tag;
      section->GetTagName(tag);
      nsAutoString tfootAsString;
      nsHTMLAtoms::tfoot->ToString(tfootAsString);
      if (tfootAsString==tag)
      {
        *aValue = section;
        break;
      }
    }
    child->GetNextSibling(&child);
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
      nsIDOMNode* resultingChild;
      mInner.AppendChild(aValue, &resultingChild);
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
  nsIDOMHTMLTableSectionElement *head=nsnull;
  GetTHead(&head);
  if (nsnull!=head)
  { // return the existing thead
    head->QueryInterface(kIDOMHTMLElementIID, (void **)aValue);  // caller's addref
    NS_ASSERTION(nsnull!=*aValue, "head must be a DOMHTMLElement");
    NS_RELEASE(head);
  }
  else
  { // create a new head rowgroup
    nsIHTMLContent *newHead=nsnull;
    rv = NS_NewHTMLTableSectionElement(&newHead,nsHTMLAtoms::thead);
    if (NS_SUCCEEDED(rv) && (nsnull!=newHead))
    {
      rv = mInner.AppendChildTo(newHead, PR_TRUE);
      newHead->QueryInterface(kIDOMHTMLElementIID, (void **)aValue); // caller's addref
      NS_RELEASE(newHead);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteTHead()
{
  nsIDOMHTMLTableSectionElement *childToDelete;
  nsresult rv = GetTHead(&childToDelete);
  if ((NS_SUCCEEDED(rv)) && (nsnull!=childToDelete))
  {
    nsIDOMNode* resultingChild;
    mInner.RemoveChild(childToDelete, &resultingChild); // mInner does the notification
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::CreateTFoot(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  nsresult rv = NS_OK;
  nsIDOMHTMLTableSectionElement *foot=nsnull;
  GetTFoot(&foot);
  if (nsnull!=foot)
  { // return the existing tfoot
    foot->QueryInterface(kIDOMHTMLElementIID, (void **)aValue);  // caller's addref
    NS_ASSERTION(nsnull!=*aValue, "foot must be a DOMHTMLElement");
    NS_RELEASE(foot);
  }
  else
  { // create a new foot rowgroup
    nsIHTMLContent *newFoot=nsnull;
    rv = NS_NewHTMLTableSectionElement(&newFoot,nsHTMLAtoms::tfoot);
    if (NS_SUCCEEDED(rv) && (nsnull!=newFoot))
    {
      rv = mInner.AppendChildTo(newFoot, PR_TRUE);
      newFoot->QueryInterface(kIDOMHTMLElementIID, (void **)aValue); // caller's addref
      NS_RELEASE(newFoot);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteTFoot()
{
{
  nsIDOMHTMLTableSectionElement *childToDelete;
  nsresult rv = GetTFoot(&childToDelete);
  if ((NS_SUCCEEDED(rv)) && (nsnull!=childToDelete))
  {
    nsIDOMNode* resultingChild;
    mInner.RemoveChild(childToDelete, &resultingChild); // mInner does the notification
  }
  return NS_OK;
}

}

NS_IMETHODIMP
nsHTMLTableElement::CreateCaption(nsIDOMHTMLElement** aValue)
{
{
  *aValue = nsnull;
  nsresult rv = NS_OK;
  nsIDOMHTMLTableCaptionElement *caption=nsnull;
  GetCaption(&caption);
  if (nsnull!=caption)
  { // return the existing thead
    caption->QueryInterface(kIDOMHTMLElementIID, (void **)aValue);  // caller's addref
    NS_ASSERTION(nsnull!=*aValue, "caption must be a DOMHTMLElement");
    NS_RELEASE(caption);
  }
  else
  { // create a new head rowgroup
    nsIHTMLContent *newCaption=nsnull;
    rv = NS_NewHTMLTableCaptionElement(&newCaption,nsHTMLAtoms::caption);
    if (NS_SUCCEEDED(rv) && (nsnull!=newCaption))
    {
      rv = mInner.AppendChildTo(newCaption, PR_TRUE);
      newCaption->QueryInterface(kIDOMHTMLElementIID, (void **)aValue); // caller's addref
      NS_RELEASE(newCaption);
    }
  }
  return NS_OK;
}
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteCaption()
{
  nsIDOMHTMLTableCaptionElement *childToDelete;
  nsresult rv = GetCaption(&childToDelete);
  if ((NS_SUCCEEDED(rv)) && (nsnull!=childToDelete))
  {
    nsIDOMNode* resultingChild;
    mInner.RemoveChild(childToDelete, &resultingChild); // mInner does the notification
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
    nsresult rv = NS_NewHTMLTableRowElement(&newRow, nsHTMLAtoms::tr);
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
      rv = NS_NewHTMLTableSectionElement(&newRowGroup, nsHTMLAtoms::tr);
      if (NS_SUCCEEDED(rv) && (nsnull!=newRowGroup))
      {
        rv = mInner.AppendChildTo(newRowGroup, PR_FALSE);
        newRowGroup->QueryInterface(kIDOMNodeIID, (void **)&rowGroup);
        NS_RELEASE(newRowGroup);
      }
    }
    if (nsnull!=rowGroup)
    {
      nsIHTMLContent *newRow=nsnull;
      rv = NS_NewHTMLTableRowElement(&newRow, nsHTMLAtoms::tr);
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
  nsIDOMHTMLCollection *rows;
  GetRows(&rows);
  nsIDOMNode *row=nsnull;
  rows->Item(aValue, &row);
  if (nsnull!=row)
  {
    nsIDOMNode *parent=nsnull;
    row->GetParentNode(&parent);
    if (nsnull!=parent)
    {
      parent->RemoveChild(row, &row);
    }
  }
  NS_RELEASE(rows);
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
                               const nsHTMLValue& aValue,
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

static void 
MapTableFrameInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext, 
                  nsStyleSpacing* aSpacing)
{
  // set up defaults
  if (aSpacing->GetBorderStyle(0) == NS_STYLE_BORDER_STYLE_NONE) {
    aSpacing->SetBorderStyle(0, NS_STYLE_BORDER_STYLE_BG_OUTSET);
  }
  if (aSpacing->GetBorderStyle(1) == NS_STYLE_BORDER_STYLE_NONE) {
    aSpacing->SetBorderStyle(1, NS_STYLE_BORDER_STYLE_BG_OUTSET);
  }
  if (aSpacing->GetBorderStyle(2) == NS_STYLE_BORDER_STYLE_NONE) {
    aSpacing->SetBorderStyle(2, NS_STYLE_BORDER_STYLE_BG_OUTSET);
  }
  if (aSpacing->GetBorderStyle(3) == NS_STYLE_BORDER_STYLE_NONE) {
    aSpacing->SetBorderStyle(3, NS_STYLE_BORDER_STYLE_BG_OUTSET);
  }

  nsHTMLValue frameValue;
  // 0 out the sides that we want to hide based on the frame attribute
  aAttributes->GetAttribute(nsHTMLAtoms::frame, frameValue);
  if (frameValue.GetUnit() == eHTMLUnit_Enumerated)
  {
    // store the value of frame
    nsStyleTable *tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
    tableStyle->mFrame = frameValue.GetIntValue();
    tableStyle->mRules=NS_STYLE_TABLE_RULES_ALL;  // most values of frame imply default rules=all
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
MapTableBorderInto(nsIHTMLAttributes* aAttributes,
                   nsIStyleContext* aContext,
                   nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  nsHTMLValue borderValue;

  aAttributes->GetAttribute(nsHTMLAtoms::border, borderValue);
  if (borderValue.GetUnit() == eHTMLUnit_String)
  {
    nsAutoString borderAsString;
    borderValue.GetStringValue(borderAsString);
    nsGenericHTMLElement::ParseValue(borderAsString, 0, borderValue, eHTMLUnit_Pixel);
  }
  else if (borderValue.GetUnit() == eHTMLUnit_Null)
  { // the absence of "border" with the presence of "frame" implies border = 1 pixel
    nsHTMLValue frameValue;
    aAttributes->GetAttribute(nsHTMLAtoms::frame, frameValue);
    if (frameValue.GetUnit() != eHTMLUnit_Null)
      borderValue.SetPixelValue(1);
  }
  if ((borderValue.GetUnit() == eHTMLUnit_Pixel) || 
      (borderValue.GetUnit() == eHTMLUnit_Empty)) {
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetMutableStyleData(eStyleStruct_Spacing);
    nsStyleTable *tableStyle = (nsStyleTable*)
      aContext->GetMutableStyleData(eStyleStruct_Table);
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    nsStyleCoord twips;
    if (borderValue.GetUnit() == eHTMLUnit_Empty) {
      tableStyle->mRules=NS_STYLE_TABLE_RULES_ALL;  // non-0 values of border imply default rules=all
      twips.SetCoordValue(NSIntPixelsToTwips(1, p2t));
    }
    else {
      PRInt32 borderThickness = borderValue.GetPixelValue();
      twips.SetCoordValue(NSIntPixelsToTwips(borderThickness, p2t));
      if (0!=borderThickness)
        tableStyle->mRules=NS_STYLE_TABLE_RULES_ALL;  // non-0 values of border imply default rules=all
      else
        tableStyle->mRules=NS_STYLE_TABLE_RULES_NONE; // 0 value of border imply default rules=none
    }

    // by default, set all border sides to the specified width
    spacing->mBorder.SetTop(twips);
    spacing->mBorder.SetRight(twips);
    spacing->mBorder.SetBottom(twips);
    spacing->mBorder.SetLeft(twips);
    // then account for the frame attribute
    MapTableFrameInto(aAttributes, aContext, aPresContext, spacing);
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
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
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
        position->mHeight.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
        break;
      default:
        break;
      }
    }

    // border and frame
    MapTableBorderInto(aAttributes, aContext, aPresContext);

    // align; Check for enumerated type (it may be another type if
    // illegal)
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      if (NS_STYLE_TEXT_ALIGN_CENTER == value.GetIntValue()) {
        nsStyleSpacing* spacing = (nsStyleSpacing*)
          aContext->GetMutableStyleData(eStyleStruct_Spacing);
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
      tableStyle->mCellPadding.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
    }

    // cellspacing  (reuses tableStyle if already resolved)
    aAttributes->GetAttribute(nsHTMLAtoms::cellspacing, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      if (nsnull==tableStyle)
        tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
      tableStyle->mBorderSpacingX.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
      tableStyle->mBorderSpacingY.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
    }
    else
    { // XXX: remove me as soon as we get this from the style sheet
      if (nsnull==tableStyle)
        tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
      tableStyle->mBorderSpacingX.SetCoordValue(NSIntPixelsToTwips(2, p2t));
      tableStyle->mBorderSpacingY.SetCoordValue(NSIntPixelsToTwips(2, p2t));
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

    //background: color
    nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext, aPresContext);
    nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
  }
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
nsHTMLTableElement::HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLTableElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  if (PR_TRUE == nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, 
    aAttribute, aHint)) {
    // Do nothing
  }
  else if (nsHTMLAtoms::summary != aAttribute)
  {
    // XXX put in real handling for known attributes, return CONTENT for anything else
    *aHint = NS_STYLE_HINT_CONTENT;
  }
  return NS_OK;
}
