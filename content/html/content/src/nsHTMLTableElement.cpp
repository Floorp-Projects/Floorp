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
#include "nsHTMLTableElement.h"
#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventTarget.h"
#include "nsDOMError.h"
#include "nsContentList.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsHTMLParts.h"
#include "nsRuleData.h"
#include "nsStyleContext.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "nsIDOMElement.h"
#include "nsGenericHTMLElement.h"
#include "nsIHTMLCollection.h"
#include "nsHTMLStyleSheet.h"
#include "dombindings.h"

/* ------------------------------ TableRowsCollection -------------------------------- */
/**
 * This class provides a late-bound collection of rows in a table.
 * mParent is NOT ref-counted to avoid circular references
 */
class TableRowsCollection : public nsIHTMLCollection,
                            public nsWrapperCache
{
public:
  TableRowsCollection(nsHTMLTableElement *aParent);
  virtual ~TableRowsCollection();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMHTMLCOLLECTION

  virtual nsIContent* GetNodeAt(PRUint32 aIndex);
  virtual nsISupports* GetNamedItem(const nsAString& aName,
                                    nsWrapperCache **aCache);
  virtual nsINode* GetParentObject()
  {
    return mParent;
  }

  NS_IMETHOD    ParentDestroyed();

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TableRowsCollection)

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext *cx, XPCWrappedNativeScope *scope,
                               bool *triedToWrap)
  {
    return mozilla::dom::binding::NodeListBase::create(cx, scope, this, this, triedToWrap);
  }

protected:
  // Those rows that are not in table sections
  nsHTMLTableElement* mParent;
  nsRefPtr<nsContentList> mOrphanRows;  
};


TableRowsCollection::TableRowsCollection(nsHTMLTableElement *aParent)
  : mParent(aParent)
  , mOrphanRows(new nsContentList(mParent,
                                  mParent->NodeInfo()->NamespaceID(),
                                  nsGkAtoms::tr,
                                  nsGkAtoms::tr,
                                  PR_FALSE))
{
  // Mark ourselves as a proxy
  SetIsProxy();
}

TableRowsCollection::~TableRowsCollection()
{
  // we do NOT have a ref-counted reference to mParent, so do NOT
  // release it!  this is to avoid circular references.  The
  // instantiator who provided mParent is responsible for managing our
  // reference for us.
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TableRowsCollection)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TableRowsCollection)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TableRowsCollection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mOrphanRows,
                                                       nsIDOMNodeList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(TableRowsCollection)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TableRowsCollection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TableRowsCollection)

NS_INTERFACE_TABLE_HEAD(TableRowsCollection)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_TABLE2(TableRowsCollection, nsIHTMLCollection,
                      nsIDOMHTMLCollection)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(TableRowsCollection)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(HTMLCollection)
NS_INTERFACE_MAP_END

// Macro that can be used to avoid copy/pasting code to iterate over the
// rowgroups.  _code should be the code to execute for each rowgroup.  The
// rowgroup's rows will be in the nsIDOMHTMLCollection* named "rows".  Note
// that this may be null at any time.  This macro assumes an nsresult named
// |rv| is in scope.
#define DO_FOR_EACH_ROWGROUP(_code)                                  \
  do {                                                               \
    if (mParent) {                                                   \
      /* THead */                                                    \
      nsCOMPtr<nsIDOMHTMLTableSectionElement> rowGroup;              \
      rowGroup = mParent->GetTHead();                                \
      nsCOMPtr<nsIDOMHTMLCollection> rows;                           \
      if (rowGroup) {                                                \
        rowGroup->GetRows(getter_AddRefs(rows));                     \
        do { /* gives scoping */                                     \
          _code                                                      \
        } while (0);                                                 \
      }                                                              \
      /* TBodies */                                                  \
      nsContentList *_tbodies = mParent->TBodies();                  \
      nsINode * _node;                                               \
      PRUint32 _tbodyIndex = 0;                                      \
      _node = _tbodies->GetNodeAt(_tbodyIndex);                      \
      while (_node) {                                                \
        rowGroup = do_QueryInterface(_node);                         \
        if (rowGroup) {                                              \
          rowGroup->GetRows(getter_AddRefs(rows));                   \
          do { /* gives scoping */                                   \
            _code                                                    \
          } while (0);                                               \
        }                                                            \
        _node = _tbodies->GetNodeAt(++_tbodyIndex);                  \
      }                                                              \
      /* orphan rows */                                              \
      rows = mOrphanRows;                                            \
      do { /* gives scoping */                                       \
        _code                                                        \
      } while (0);                                                   \
      /* TFoot */                                                    \
      rowGroup = mParent->GetTFoot();                                \
      rows = nsnull;                                                 \
      if (rowGroup) {                                                \
        rowGroup->GetRows(getter_AddRefs(rows));                     \
        do { /* gives scoping */                                     \
          _code                                                      \
        } while (0);                                                 \
      }                                                              \
    }                                                                \
  } while (0)

static PRUint32
CountRowsInRowGroup(nsIDOMHTMLCollection* rows)
{
  PRUint32 length = 0;
  
  if (rows) {
    rows->GetLength(&length);
  }
  
  return length;
}

// we re-count every call.  A better implementation would be to set
// ourselves up as an observer of contentAppended, contentInserted,
// and contentDeleted
NS_IMETHODIMP 
TableRowsCollection::GetLength(PRUint32* aLength)
{
  *aLength=0;

  DO_FOR_EACH_ROWGROUP(
    *aLength += CountRowsInRowGroup(rows);
  );

  return NS_OK;
}

// Returns the item at index aIndex if available. If null is returned,
// then aCount will be set to the number of rows in this row collection.
// Otherwise, the value of aCount is undefined.
static nsIContent*
GetItemOrCountInRowGroup(nsIDOMHTMLCollection* rows,
                         PRUint32 aIndex, PRUint32* aCount)
{
  *aCount = 0;

  if (rows) {
    rows->GetLength(aCount);
    if (aIndex < *aCount) {
      nsCOMPtr<nsINodeList> list = do_QueryInterface(rows);
      return list->GetNodeAt(aIndex);
    }
  }
  
  return nsnull;
}

nsIContent*
TableRowsCollection::GetNodeAt(PRUint32 aIndex)
{
  DO_FOR_EACH_ROWGROUP(
    PRUint32 count;
    nsIContent* node = GetItemOrCountInRowGroup(rows, aIndex, &count);
    if (node) {
      return node; 
    }

    NS_ASSERTION(count <= aIndex, "GetItemOrCountInRowGroup screwed up");
    aIndex -= count;
  );

  return nsnull;
}

NS_IMETHODIMP 
TableRowsCollection::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsISupports* node = GetNodeAt(aIndex);
  if (!node) {
    *aReturn = nsnull;

    return NS_OK;
  }

  return CallQueryInterface(node, aReturn);
}

static nsISupports*
GetNamedItemInRowGroup(nsIDOMHTMLCollection* aRows, const nsAString& aName,
                       nsWrapperCache** aCache)
{
  nsCOMPtr<nsIHTMLCollection> rows = do_QueryInterface(aRows);
  if (rows) {
    return rows->GetNamedItem(aName, aCache);
  }

  return nsnull;
}

nsISupports* 
TableRowsCollection::GetNamedItem(const nsAString& aName,
                                  nsWrapperCache** aCache)
{
  DO_FOR_EACH_ROWGROUP(
    nsISupports* item = GetNamedItemInRowGroup(rows, aName, aCache);
    if (item) {
      return item;
    }
  );
  *aCache = nsnull;
  return nsnull;
}

NS_IMETHODIMP 
TableRowsCollection::NamedItem(const nsAString& aName,
                               nsIDOMNode** aReturn)
{
  nsWrapperCache *cache;
  nsISupports* item = GetNamedItem(aName, &cache);
  if (!item) {
    *aReturn = nsnull;

    return NS_OK;
  }

  return CallQueryInterface(item, aReturn);
}

NS_IMETHODIMP
TableRowsCollection::ParentDestroyed()
{
  // see comment in destructor, do NOT release mParent!
  mParent = nsnull;

  return NS_OK;
}

/* -------------------------- nsHTMLTableElement --------------------------- */
// the class declaration is at the top of this file


NS_IMPL_NS_NEW_HTML_ELEMENT(Table)


nsHTMLTableElement::nsHTMLTableElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mTableInheritedAttributes(TABLE_ATTRS_DIRTY)
{
}

nsHTMLTableElement::~nsHTMLTableElement()
{
  if (mRows) {
    mRows->ParentDestroyed();
  }
  ReleaseInheritedAttributes();
}


NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLTableElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLTableElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mTBodies,
                                                       nsIDOMNodeList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRows)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsHTMLTableElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTableElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLTableElement, nsHTMLTableElement)

// QueryInterface implementation for nsHTMLTableElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLTableElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLTableElement, nsIDOMHTMLTableElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLTableElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLTableElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLTableElement)


// the DOM spec says border, cellpadding, cellSpacing are all "wstring"
// in fact, they are integers or they are meaningless.  so we store them
// here as ints.

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
  GetFirstChild(getter_AddRefs(child));

  while (child) {
    nsCOMPtr<nsIDOMHTMLTableCaptionElement> caption(do_QueryInterface(child));

    if (caption) {
      *aValue = caption;
      NS_ADDREF(*aValue);

      break;
    }

    nsIDOMNode *temp = child.get();
    temp->GetNextSibling(getter_AddRefs(child));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::SetCaption(nsIDOMHTMLTableCaptionElement* aValue)
{
  nsresult rv = DeleteCaption();

  if (NS_SUCCEEDED(rv)) {
    if (aValue) {
      nsCOMPtr<nsIDOMNode> resultingChild;
      AppendChild(aValue, getter_AddRefs(resultingChild));
    }
  }

  return rv;
}

already_AddRefed<nsIDOMHTMLTableSectionElement>
nsHTMLTableElement::GetSection(nsIAtom *aTag)
{
  nsCOMPtr<nsIDOMHTMLTableSectionElement> section;

  for (nsIContent* child = nsINode::GetFirstChild();
       child;
       child = child->GetNextSibling()) {

    section = do_QueryInterface(child);

    if (section && child->NodeInfo()->Equals(aTag)) {
      nsIDOMHTMLTableSectionElement *result = section;
      NS_ADDREF(result);

      return result;
    }
  }

  return nsnull;
}

NS_IMETHODIMP
nsHTMLTableElement::GetTHead(nsIDOMHTMLTableSectionElement** aValue)
{
  *aValue = GetTHead().get();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::SetTHead(nsIDOMHTMLTableSectionElement* aValue)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aValue));
  NS_ENSURE_TRUE(content, NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);

  if (!content->NodeInfo()->Equals(nsGkAtoms::thead)) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }
  
  nsresult rv = DeleteTHead();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aValue) {
    nsCOMPtr<nsIDOMNode> child;
    rv = GetFirstChild(getter_AddRefs(child));
    if (NS_FAILED(rv)) {
      return rv;
    }
     
    nsCOMPtr<nsIDOMNode> resultChild;
    rv = InsertBefore(aValue, child, getter_AddRefs(resultChild));
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTableElement::GetTFoot(nsIDOMHTMLTableSectionElement** aValue)
{
  *aValue = GetTFoot().get();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::SetTFoot(nsIDOMHTMLTableSectionElement* aValue)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aValue));
  NS_ENSURE_TRUE(content, NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);

  if (!content->NodeInfo()->Equals(nsGkAtoms::tfoot)) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }
  
  nsresult rv = DeleteTFoot();
  if (NS_SUCCEEDED(rv)) {
    if (aValue) {
      nsCOMPtr<nsIDOMNode> resultingChild;
      AppendChild(aValue, getter_AddRefs(resultingChild));
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLTableElement::GetRows(nsIDOMHTMLCollection** aValue)
{
  if (!mRows) {
    mRows = new TableRowsCollection(this);
  }

  *aValue = mRows;
  NS_ADDREF(*aValue);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::GetTBodies(nsIDOMHTMLCollection** aValue)
{
  NS_ADDREF(*aValue = TBodies());
  return NS_OK;
}

nsContentList*
nsHTMLTableElement::TBodies()
{
  if (!mTBodies) {
    // Not using NS_GetContentList because this should not be cached
    mTBodies = new nsContentList(this,
                                 mNodeInfo->NamespaceID(),
                                 nsGkAtoms::tbody,
                                 nsGkAtoms::tbody,
                                 PR_FALSE);
  }

  return mTBodies;
}

NS_IMETHODIMP
nsHTMLTableElement::CreateTHead(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMHTMLTableSectionElement> head;

  GetTHead(getter_AddRefs(head));

  if (head) { // return the existing thead
    CallQueryInterface(head, aValue);

    NS_ASSERTION(*aValue, "head must be a DOMHTMLElement");
  }
  else
  { // create a new head rowgroup
    nsCOMPtr<nsINodeInfo> nodeInfo;

    nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::thead,
                                getter_AddRefs(nodeInfo));

    nsCOMPtr<nsIContent> newHead = NS_NewHTMLTableSectionElement(nodeInfo.forget());

    if (newHead) {
      nsCOMPtr<nsIDOMNode> child;

      rv = GetFirstChild(getter_AddRefs(child));

      if (NS_FAILED(rv)) {
        return rv;
      }

      CallQueryInterface(newHead, aValue);

      nsCOMPtr<nsIDOMNode> resultChild;
      rv = InsertBefore(*aValue, child, getter_AddRefs(resultChild));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteTHead()
{
  nsCOMPtr<nsIDOMHTMLTableSectionElement> childToDelete;
  nsresult rv = GetTHead(getter_AddRefs(childToDelete));

  if ((NS_SUCCEEDED(rv)) && childToDelete) {
    nsCOMPtr<nsIDOMNode> resultingChild;
    // mInner does the notification
    RemoveChild(childToDelete, getter_AddRefs(resultingChild));
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

  if (foot) { // return the existing tfoot
    CallQueryInterface(foot, aValue);

    NS_ASSERTION(*aValue, "foot must be a DOMHTMLElement");
  }
  else
  { // create a new foot rowgroup
    nsCOMPtr<nsINodeInfo> nodeInfo;
    nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::tfoot,
                                getter_AddRefs(nodeInfo));

    nsCOMPtr<nsIContent> newFoot = NS_NewHTMLTableSectionElement(nodeInfo.forget());

    if (newFoot) {
      rv = AppendChildTo(newFoot, PR_TRUE);
      CallQueryInterface(newFoot, aValue);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteTFoot()
{
  nsCOMPtr<nsIDOMHTMLTableSectionElement> childToDelete;
  nsresult rv = GetTFoot(getter_AddRefs(childToDelete));

  if ((NS_SUCCEEDED(rv)) && childToDelete) {
    nsCOMPtr<nsIDOMNode> resultingChild;
    // mInner does the notification
    RemoveChild(childToDelete, getter_AddRefs(resultingChild));
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

  if (caption) { // return the existing thead
    CallQueryInterface(caption, aValue);

    NS_ASSERTION(*aValue, "caption must be a DOMHTMLElement");
  }
  else
  { // create a new head rowgroup
    nsCOMPtr<nsINodeInfo> nodeInfo;
    nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::caption,
                                getter_AddRefs(nodeInfo));

    nsCOMPtr<nsIContent> newCaption = NS_NewHTMLTableCaptionElement(nodeInfo.forget());

    if (newCaption) {
      rv = AppendChildTo(newCaption, PR_TRUE);
      CallQueryInterface(newCaption, aValue);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteCaption()
{
  nsCOMPtr<nsIDOMHTMLTableCaptionElement> childToDelete;
  nsresult rv = GetCaption(getter_AddRefs(childToDelete));

  if ((NS_SUCCEEDED(rv)) && childToDelete) {
    nsCOMPtr<nsIDOMNode> resultingChild;
    RemoveChild(childToDelete, getter_AddRefs(resultingChild));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  /* get the ref row at aIndex
     if there is one, 
       get its parent
       insert the new row just before the ref row
     else
       get the first row group
       insert the new row as its first child
  */
  *aValue = nsnull;

  if (aIndex < -1) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsresult rv;

  nsCOMPtr<nsIDOMHTMLCollection> rows;
  GetRows(getter_AddRefs(rows));

  PRUint32 rowCount;
  rows->GetLength(&rowCount);

  if ((PRUint32)aIndex > rowCount && aIndex != -1) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // use local variable refIndex so we can remember original aIndex
  PRUint32 refIndex = (PRUint32)aIndex;

  if (rowCount > 0) {
    if (refIndex == rowCount || aIndex == -1) {
      // we set refIndex to the last row so we can get the last row's
      // parent we then do an AppendChild below if (rowCount<aIndex)

      refIndex = rowCount - 1;
    }

    nsCOMPtr<nsIDOMNode> refRow;
    rows->Item(refIndex, getter_AddRefs(refRow));

    nsCOMPtr<nsIDOMNode> parent;

    refRow->GetParentNode(getter_AddRefs(parent));
    // create the row
    nsCOMPtr<nsINodeInfo> nodeInfo;
    nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::tr,
                                getter_AddRefs(nodeInfo));

    nsCOMPtr<nsIContent> newRow = NS_NewHTMLTableRowElement(nodeInfo.forget());

    if (newRow) {
      nsCOMPtr<nsIDOMNode> newRowNode(do_QueryInterface(newRow));
      nsCOMPtr<nsIDOMNode> retChild;

      // If index is -1 or equal to the number of rows, the new row
      // is appended.
      if (aIndex == -1 || PRUint32(aIndex) == rowCount) {
        rv = parent->AppendChild(newRowNode, getter_AddRefs(retChild));
      }
      else
      {
        // insert the new row before the reference row we found above
        rv = parent->InsertBefore(newRowNode, refRow,
                                  getter_AddRefs(retChild));
      }

      if (retChild) {
        CallQueryInterface(retChild, aValue);
      }
    }
  }
  else
  { // the row count was 0, so 
    // find the first row group and insert there as first child
    nsCOMPtr<nsIDOMNode> rowGroup;

    PRInt32 namespaceID = mNodeInfo->NamespaceID();
    for (nsIContent* child = nsINode::GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      nsINodeInfo *childInfo = child->NodeInfo();
      nsIAtom *localName = childInfo->NameAtom();
      if (childInfo->NamespaceID() == namespaceID &&
          (localName == nsGkAtoms::thead ||
           localName == nsGkAtoms::tbody ||
           localName == nsGkAtoms::tfoot)) {
        rowGroup = do_QueryInterface(child);
        NS_ASSERTION(rowGroup, "HTML node did not QI to nsIDOMNode");
        break;
      }
    }

    if (!rowGroup) { // need to create a TBODY
      nsCOMPtr<nsINodeInfo> nodeInfo;
      nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::tbody,
                                  getter_AddRefs(nodeInfo));

      nsCOMPtr<nsIContent> newRowGroup =
        NS_NewHTMLTableSectionElement(nodeInfo.forget());

      if (newRowGroup) {
        rv = AppendChildTo(newRowGroup, PR_TRUE);

        rowGroup = do_QueryInterface(newRowGroup);
      }
    }

    if (rowGroup) {
      nsCOMPtr<nsINodeInfo> nodeInfo;
      nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::tr,
                                  getter_AddRefs(nodeInfo));

      nsCOMPtr<nsIContent> newRow = NS_NewHTMLTableRowElement(nodeInfo.forget());
      if (newRow) {
        nsCOMPtr<nsIDOMNode> firstRow;

        nsCOMPtr<nsIDOMHTMLTableSectionElement> section =
          do_QueryInterface(rowGroup);

        if (section) {
          nsCOMPtr<nsIDOMHTMLCollection> rows;
          section->GetRows(getter_AddRefs(rows));
          if (rows) {
            rows->Item(0, getter_AddRefs(firstRow));
          }
        }
        
        nsCOMPtr<nsIDOMNode> retNode, newRowNode(do_QueryInterface(newRow));

        rowGroup->InsertBefore(newRowNode, firstRow, getter_AddRefs(retNode));

        if (retNode) {
          CallQueryInterface(retNode, aValue);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::DeleteRow(PRInt32 aValue)
{
  if (aValue < -1) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMHTMLCollection> rows;
  GetRows(getter_AddRefs(rows));

  nsresult rv;
  PRUint32 refIndex;
  if (aValue == -1) {
    rv = rows->GetLength(&refIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    if (refIndex == 0) {
      return NS_OK;
    }

    --refIndex;
  }
  else {
    refIndex = (PRUint32)aValue;
  }

  nsCOMPtr<nsIDOMNode> row;
  rv = rows->Item(refIndex, getter_AddRefs(row));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!row) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsCOMPtr<nsIDOMNode> parent;
  row->GetParentNode(getter_AddRefs(parent));
  NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMNode> deleted_row;
  return parent->RemoveChild(row, getter_AddRefs(deleted_row));
}

static const nsAttrValue::EnumTable kFrameTable[] = {
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

static const nsAttrValue::EnumTable kRulesTable[] = {
  { "none",   NS_STYLE_TABLE_RULES_NONE },
  { "groups", NS_STYLE_TABLE_RULES_GROUPS },
  { "rows",   NS_STYLE_TABLE_RULES_ROWS },
  { "cols",   NS_STYLE_TABLE_RULES_COLS },
  { "all",    NS_STYLE_TABLE_RULES_ALL },
  { 0 }
};

static const nsAttrValue::EnumTable kLayoutTable[] = {
  { "auto",   NS_STYLE_TABLE_LAYOUT_AUTO },
  { "fixed",  NS_STYLE_TABLE_LAYOUT_FIXED },
  { 0 }
};


bool
nsHTMLTableElement::ParseAttribute(PRInt32 aNamespaceID,
                                   nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  /* ignore summary, just a string */
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::cellspacing ||
        aAttribute == nsGkAtoms::cellpadding) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::cols ||
        aAttribute == nsGkAtoms::border) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
    if (aAttribute == nsGkAtoms::height) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::width) {
      if (aResult.ParseSpecialIntValue(aValue)) {
        // treat 0 width as auto
        nsAttrValue::ValueType type = aResult.Type();
        return !((type == nsAttrValue::eInteger &&
                  aResult.GetIntegerValue() == 0) ||
                 (type == nsAttrValue::ePercent &&
                  aResult.GetPercentValue() == 0.0f));
      }
      return PR_FALSE;
    }
    
    if (aAttribute == nsGkAtoms::align) {
      return ParseTableHAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::bgcolor ||
        aAttribute == nsGkAtoms::bordercolor) {
      return aResult.ParseColor(aValue);
    }
    if (aAttribute == nsGkAtoms::frame) {
      return aResult.ParseEnumValue(aValue, kFrameTable, PR_FALSE);
    }
    if (aAttribute == nsGkAtoms::layout) {
      return aResult.ParseEnumValue(aValue, kLayoutTable, PR_FALSE);
    }
    if (aAttribute == nsGkAtoms::rules) {
      return aResult.ParseEnumValue(aValue, kRulesTable, PR_FALSE);
    }
    if (aAttribute == nsGkAtoms::hspace ||
        aAttribute == nsGkAtoms::vspace) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}



static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  // XXX Bug 211636:  This function is used by a single style rule
  // that's used to match two different type of elements -- tables, and
  // table cells.  (nsHTMLTableCellElement overrides
  // WalkContentStyleRules so that this happens.)  This violates the
  // nsIStyleRule contract, since it's the same style rule object doing
  // the mapping in two different ways.  It's also incorrect since it's
  // testing the display type of the style context rather than checking
  // which *element* it's matching (style rules should not stop matching
  // when the display type is changed).

  nsPresContext* presContext = aData->mPresContext;
  nsCompatibility mode = presContext->CompatibilityMode();

  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(TableBorder)) {
    // cellspacing
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::cellspacing);
    nsCSSValue* borderSpacing = aData->ValueForBorderSpacing();
    if (value && value->Type() == nsAttrValue::eInteger) {
      if (borderSpacing->GetUnit() == eCSSUnit_Null) {
        borderSpacing->
          SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      }
    }
    else if (value && value->Type() == nsAttrValue::ePercent &&
             eCompatibility_NavQuirks == mode) {
      // in quirks mode, treat a % cellspacing value a pixel value.
      if (borderSpacing->GetUnit() == eCSSUnit_Null) {
        borderSpacing->
         SetFloatValue(100.0f * value->GetPercentValue(), eCSSUnit_Pixel);
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Table)) {
    const nsAttrValue* value;
    // layout
    nsCSSValue* tableLayout = aData->ValueForTableLayout();
    if (tableLayout->GetUnit() == eCSSUnit_Null) {
      value = aAttributes->GetAttr(nsGkAtoms::layout);
      if (value && value->Type() == nsAttrValue::eEnum)
        tableLayout->SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
    // cols
    value = aAttributes->GetAttr(nsGkAtoms::cols);
    if (value) {
      nsCSSValue* cols = aData->ValueForCols();
      if (value->Type() == nsAttrValue::eInteger)
        cols->SetIntValue(value->GetIntegerValue(), eCSSUnit_Integer);
      else // COLS had no value, so it refers to all columns
        cols->SetIntValue(NS_STYLE_TABLE_COLS_ALL, eCSSUnit_Enumerated);
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Margin)) {
    // align; Check for enumerated type (it may be another type if
    // illegal)
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);

    if (value && value->Type() == nsAttrValue::eEnum) {
      if (value->GetEnumValue() == NS_STYLE_TEXT_ALIGN_CENTER ||
          value->GetEnumValue() == NS_STYLE_TEXT_ALIGN_MOZ_CENTER) {
        nsCSSValue* marginLeft = aData->ValueForMarginLeftValue();
        if (marginLeft->GetUnit() == eCSSUnit_Null)
          marginLeft->SetAutoValue();
        nsCSSValue* marginRight = aData->ValueForMarginRightValue();
        if (marginRight->GetUnit() == eCSSUnit_Null)
          marginRight->SetAutoValue();
      }
    }

    // hspace is mapped into left and right margin,
    // vspace is mapped into top and bottom margins
    // - *** Quirks Mode only ***
    if (eCompatibility_NavQuirks == mode) {
      value = aAttributes->GetAttr(nsGkAtoms::hspace);

      if (value && value->Type() == nsAttrValue::eInteger) {
        nsCSSValue* marginLeft = aData->ValueForMarginLeftValue();
        if (marginLeft->GetUnit() == eCSSUnit_Null)
          marginLeft->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
        nsCSSValue* marginRight = aData->ValueForMarginRightValue();
        if (marginRight->GetUnit() == eCSSUnit_Null)
          marginRight->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      }

      value = aAttributes->GetAttr(nsGkAtoms::vspace);

      if (value && value->Type() == nsAttrValue::eInteger) {
        nsCSSValue* marginTop = aData->ValueForMarginTop();
        if (marginTop->GetUnit() == eCSSUnit_Null)
          marginTop->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
        nsCSSValue* marginBottom = aData->ValueForMarginBottom();
        if (marginBottom->GetUnit() == eCSSUnit_Null)
          marginBottom->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
      }
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Position)) {
    // width: value
    nsCSSValue* width = aData->ValueForWidth();
    if (width->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
      if (value && value->Type() == nsAttrValue::eInteger)
        width->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      else if (value && value->Type() == nsAttrValue::ePercent)
        width->SetPercentValue(value->GetPercentValue());
    }

    // height: value
    nsCSSValue* height = aData->ValueForHeight();
    if (height->GetUnit() == eCSSUnit_Null) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
      if (value && value->Type() == nsAttrValue::eInteger)
        height->SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      else if (value && value->Type() == nsAttrValue::ePercent)
        height->SetPercentValue(value->GetPercentValue());
    }
  }
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Border)) {
    // bordercolor
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::bordercolor);
    nscolor color;
    if (value && presContext->UseDocumentColors() &&
        value->GetColorValue(color)) {
      nsCSSValue* borderLeftColor = aData->ValueForBorderLeftColorValue();
      if (borderLeftColor->GetUnit() == eCSSUnit_Null)
        borderLeftColor->SetColorValue(color);
      nsCSSValue* borderRightColor = aData->ValueForBorderRightColorValue();
      if (borderRightColor->GetUnit() == eCSSUnit_Null)
        borderRightColor->SetColorValue(color);
      nsCSSValue* borderTopColor = aData->ValueForBorderTopColor();
      if (borderTopColor->GetUnit() == eCSSUnit_Null)
        borderTopColor->SetColorValue(color);
      nsCSSValue* borderBottomColor = aData->ValueForBorderBottomColor();
      if (borderBottomColor->GetUnit() == eCSSUnit_Null)
        borderBottomColor->SetColorValue(color);
    }

    // border
    const nsAttrValue* borderValue = aAttributes->GetAttr(nsGkAtoms::border);
    if (borderValue) {
      // border = 1 pixel default
      PRInt32 borderThickness = 1;

      if (borderValue->Type() == nsAttrValue::eInteger)
        borderThickness = borderValue->GetIntegerValue();

      // by default, set all border sides to the specified width
      nsCSSValue* borderLeftWidth = aData->ValueForBorderLeftWidthValue();
      if (borderLeftWidth->GetUnit() == eCSSUnit_Null)
        borderLeftWidth->SetFloatValue((float)borderThickness, eCSSUnit_Pixel);
      nsCSSValue* borderRightWidth = aData->ValueForBorderRightWidthValue();
      if (borderRightWidth->GetUnit() == eCSSUnit_Null)
        borderRightWidth->SetFloatValue((float)borderThickness, eCSSUnit_Pixel);
      nsCSSValue* borderTopWidth = aData->ValueForBorderTopWidth();
      if (borderTopWidth->GetUnit() == eCSSUnit_Null)
        borderTopWidth->SetFloatValue((float)borderThickness, eCSSUnit_Pixel);
      nsCSSValue* borderBottomWidth = aData->ValueForBorderBottomWidth();
      if (borderBottomWidth->GetUnit() == eCSSUnit_Null)
        borderBottomWidth->SetFloatValue((float)borderThickness, eCSSUnit_Pixel);
    }
  }
  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
nsHTMLTableElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::layout },
    { &nsGkAtoms::cellpadding },
    { &nsGkAtoms::cellspacing },
    { &nsGkAtoms::cols },
    { &nsGkAtoms::border },
    { &nsGkAtoms::width },
    { &nsGkAtoms::height },
    { &nsGkAtoms::hspace },
    { &nsGkAtoms::vspace },
    
    { &nsGkAtoms::bordercolor },
    
    { &nsGkAtoms::align },
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
nsHTMLTableElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

static void
MapInheritedTableAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    nsRuleData* aData)
{
  if (aData->mSIDs & NS_STYLE_INHERIT_BIT(Padding)) {
    const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::cellpadding);
    if (value) {
      nsAttrValue::ValueType valueType = value->Type();
      if (valueType == nsAttrValue::eInteger ||
          valueType == nsAttrValue::ePercent) {
        // We have cellpadding.  This will override our padding values if we
        // don't have any set.
        nsCSSValue padVal;
        if (valueType == nsAttrValue::eInteger)
          padVal.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
        else {
          // when we support % cellpadding in standard mode, uncomment the
          // following
          float pctVal = value->GetPercentValue();
          //if (eCompatibility_NavQuirks == mode) {
          // in quirks mode treat a pct cellpadding value as a pixel value
          padVal.SetFloatValue(100.0f * pctVal, eCSSUnit_Pixel);
          //}
          //else {
          //  padVal.SetPercentValue(pctVal);
          //}
        }
        nsCSSValue* paddingLeft = aData->ValueForPaddingLeftValue();
        if (paddingLeft->GetUnit() == eCSSUnit_Null)
          *paddingLeft = padVal;
        nsCSSValue* paddingRight = aData->ValueForPaddingRightValue();
        if (paddingRight->GetUnit() == eCSSUnit_Null)
          *paddingRight = padVal;
        nsCSSValue* paddingTop = aData->ValueForPaddingTop();
        if (paddingTop->GetUnit() == eCSSUnit_Null)
          *paddingTop = padVal;
        nsCSSValue* paddingBottom = aData->ValueForPaddingBottom();
        if (paddingBottom->GetUnit() == eCSSUnit_Null)
          *paddingBottom = padVal;
      }
    }
  }
}

nsMappedAttributes*
nsHTMLTableElement::GetAttributesMappedForCell()
{
  if (mTableInheritedAttributes) {
    if (mTableInheritedAttributes == TABLE_ATTRS_DIRTY)
      BuildInheritedAttributes();
    if (mTableInheritedAttributes != TABLE_ATTRS_DIRTY)
      return mTableInheritedAttributes;
  }
  return nsnull;
}

void
nsHTMLTableElement::BuildInheritedAttributes()
{
  NS_ASSERTION(mTableInheritedAttributes == TABLE_ATTRS_DIRTY,
               "potential leak, plus waste of work");
  nsIDocument *document = GetCurrentDoc();
  nsHTMLStyleSheet* sheet = document ?
                              document->GetAttributeStyleSheet() : nsnull;
  nsRefPtr<nsMappedAttributes> newAttrs;
  if (sheet) {
    const nsAttrValue* value = mAttrsAndChildren.GetAttr(nsGkAtoms::cellpadding);
    if (value) {
      nsRefPtr<nsMappedAttributes> modifiableMapped = new
      nsMappedAttributes(sheet, MapInheritedTableAttributesIntoRule);

      if (modifiableMapped) {
        nsAttrValue val(*value);
        modifiableMapped->SetAndTakeAttr(nsGkAtoms::cellpadding, val);
      }
      newAttrs = sheet->UniqueMappedAttributes(modifiableMapped);
      NS_ASSERTION(newAttrs, "out of memory, but handling gracefully");

      if (newAttrs != modifiableMapped) {
        // Reset the stylesheet of modifiableMapped so that it doesn't
        // spend time trying to remove itself from the hash.  There is no
        // risk that modifiableMapped is in the hash since we created
        // it ourselves and it didn't come from the stylesheet (in which
        // case it would not have been modifiable).
        modifiableMapped->DropStyleSheetReference();
      }
    }
    mTableInheritedAttributes = newAttrs;
    NS_IF_ADDREF(mTableInheritedAttributes);
  }
}

nsresult
nsHTMLTableElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
{
  ReleaseInheritedAttributes();
  return nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                          aBindingParent,
                                          aCompileEventHandlers);
}

void
nsHTMLTableElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  ReleaseInheritedAttributes();
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

nsresult
nsHTMLTableElement::BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                  const nsAString* aValue,
                                  bool aNotify)
{
  if (aName == nsGkAtoms::cellpadding && aNameSpaceID == kNameSpaceID_None) {
    ReleaseInheritedAttributes();
  }
  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName, aValue,
                                             aNotify);
}

nsresult
nsHTMLTableElement::AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAString* aValue,
                                 bool aNotify)
{
  if (aName == nsGkAtoms::cellpadding && aNameSpaceID == kNameSpaceID_None) {
    BuildInheritedAttributes();
  }
  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aNotify);
}
