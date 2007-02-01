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
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventReceiver.h"
#include "nsDOMError.h"
#include "nsContentList.h"
#include "nsGenericDOMHTMLCollection.h"
#include "nsMappedAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsHTMLParts.h"
#include "nsRuleData.h"
#include "nsStyleContext.h"
#include "nsIDocument.h"

/* for collections */
#include "nsIDOMElement.h"
#include "nsGenericHTMLElement.h"
/* end for collections */

class TableRowsCollection;

class nsHTMLTableElement :  public nsGenericHTMLElement,
                            public nsIDOMHTMLTableElement
{
public:
  nsHTMLTableElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLTableElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLTableElement
  NS_DECL_NSIDOMHTMLTABLEELEMENT

  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  already_AddRefed<nsIDOMHTMLTableSectionElement> GetSection(nsIAtom *aTag);

  nsRefPtr<nsContentList> mTBodies;
  nsRefPtr<TableRowsCollection> mRows;
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

  nsresult Init();

  NS_IMETHOD    GetLength(PRUint32* aLength);
  NS_IMETHOD    Item(PRUint32 aIndex, nsIDOMNode** aReturn);
  NS_IMETHOD    NamedItem(const nsAString& aName,
                          nsIDOMNode** aReturn);

  NS_IMETHOD    ParentDestroyed();

protected:
  // Those rows that are not in table sections
  nsRefPtr<nsContentList> mOrphanRows;  
  nsHTMLTableElement * mParent;
};


TableRowsCollection::TableRowsCollection(nsHTMLTableElement *aParent)
  : nsGenericDOMHTMLCollection()
{
  mParent = aParent;
}

TableRowsCollection::~TableRowsCollection()
{
  // we do NOT have a ref-counted reference to mParent, so do NOT
  // release it!  this is to avoid circular references.  The
  // instantiator who provided mParent is responsible for managing our
  // reference for us.
}

nsresult
TableRowsCollection::Init()
{
  mOrphanRows = new nsContentList(mParent,
                                  nsGkAtoms::tr,
                                  mParent->NodeInfo()->NamespaceID(),
                                  PR_FALSE);
  return mOrphanRows ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

// Macro that can be used to avoid copy/pasting code to iterate over the
// rowgroups.  _code should be the code to execute for each rowgroup.  The
// rowgroup's rows will be in the nsIDOMHTMLCollection* named "rows".  Note
// that this may be null at any time.  This macro assumes an nsresult named
// |rv| is in scope.
#define DO_FOR_EACH_ROWGROUP(_code)                                  \
  PR_BEGIN_MACRO                                                     \
    if (mParent) {                                                   \
      /* THead */                                                    \
      nsCOMPtr<nsIDOMHTMLTableSectionElement> rowGroup;              \
      rv = mParent->GetTHead(getter_AddRefs(rowGroup));              \
      NS_ENSURE_SUCCESS(rv, rv);                                     \
      nsCOMPtr<nsIDOMHTMLCollection> rows;                           \
      if (rowGroup) {                                                \
        rowGroup->GetRows(getter_AddRefs(rows));                     \
        do { /* gives scoping */                                     \
          _code                                                      \
        } while (0);                                                 \
      }                                                              \
      nsCOMPtr<nsIDOMHTMLCollection> _tbodies;                       \
      /* TBodies */                                                  \
      rv = mParent->GetTBodies(getter_AddRefs(_tbodies));            \
      NS_ENSURE_SUCCESS(rv, rv);                                     \
      if (_tbodies) {                                                \
        nsCOMPtr<nsIDOMNode> _node;                                  \
        PRUint32 _tbodyIndex = 0;                                    \
        rv = _tbodies->Item(_tbodyIndex, getter_AddRefs(_node));     \
        NS_ENSURE_SUCCESS(rv, rv);                                   \
        while (_node) {                                              \
          rowGroup = do_QueryInterface(_node);                       \
          if (rowGroup) {                                            \
            rowGroup->GetRows(getter_AddRefs(rows));                 \
            do { /* gives scoping */                                 \
              _code                                                  \
            } while (0);                                             \
          }                                                          \
          rv = _tbodies->Item(++_tbodyIndex, getter_AddRefs(_node)); \
          NS_ENSURE_SUCCESS(rv, rv);                                 \
        }                                                            \
      }                                                              \
      /* orphan rows */                                              \
      rows = mOrphanRows;                                            \
      do { /* gives scoping */                                       \
        _code                                                        \
      } while (0);                                                   \
      /* TFoot */                                                    \
      rv = mParent->GetTFoot(getter_AddRefs(rowGroup));              \
      NS_ENSURE_SUCCESS(rv, rv);                                     \
      rows = nsnull;                                                 \
      if (rowGroup) {                                                \
        rowGroup->GetRows(getter_AddRefs(rows));                     \
        do { /* gives scoping */                                     \
          _code                                                      \
        } while (0);                                                 \
      }                                                              \
    }                                                                \
  PR_END_MACRO

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
  nsresult rv = NS_OK;

  DO_FOR_EACH_ROWGROUP(
    *aLength += CountRowsInRowGroup(rows);
  );

  return rv;
}

// Returns the number of items in the row group, only if *aItem ends
// up null.  Otherwise, returns 0.
static PRUint32
GetItemOrCountInRowGroup(nsIDOMHTMLCollection* rows,
                         PRUint32 aIndex, nsIDOMNode** aItem)
{
  NS_PRECONDITION(aItem, "Null out param");

  *aItem = nsnull;
  PRUint32 length = 0;
  
  if (rows) {
    rows->Item(aIndex, aItem);
    if (!*aItem) {
      rows->GetLength(&length);
    }
  }
  
  return length;
}

// increments aReturn refcnt by 1
NS_IMETHODIMP 
TableRowsCollection::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;
  nsresult rv = NS_OK;

  DO_FOR_EACH_ROWGROUP(
    PRUint32 count = GetItemOrCountInRowGroup(rows, aIndex, aReturn);
    if (*aReturn) {
      return NS_OK; 
    }

    NS_ASSERTION(count <= aIndex, "GetItemOrCountInRowGroup screwed up");
    aIndex -= count;
  );

  return rv;
}

static nsresult
GetNamedItemInRowGroup(nsIDOMHTMLCollection* aRows,
                       const nsAString& aName, nsIDOMNode** aNamedItem)
{
  if (aRows) {
    return aRows->NamedItem(aName, aNamedItem);
  }

  *aNamedItem = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
TableRowsCollection::NamedItem(const nsAString& aName,
                               nsIDOMNode** aReturn)
{
  *aReturn = nsnull;
  nsresult rv = NS_OK;
  DO_FOR_EACH_ROWGROUP(
    rv = GetNamedItemInRowGroup(rows, aName, aReturn);
    NS_ENSURE_SUCCESS(rv, rv);
    if (*aReturn) {
      return rv;
    }
  );
  return rv;
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


nsHTMLTableElement::nsHTMLTableElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLTableElement::~nsHTMLTableElement()
{
  if (mRows) {
    mRows->ParentDestroyed();
  }
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTableElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLTableElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLTableElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLTableElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLTableElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLTableElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


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
  PRUint32 childCount = GetChildCount();

  nsCOMPtr<nsIDOMHTMLTableSectionElement> section;

  for (PRUint32 i = 0; i < childCount; ++i) {
    nsIContent *child = GetChildAt(i);

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
  *aValue = GetSection(nsGkAtoms::thead).get();

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
  *aValue = GetSection(nsGkAtoms::tfoot).get();

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
    // XXX why was this here NS_ADDREF(nsGkAtoms::tr);
    mRows = new TableRowsCollection(this);
    NS_ENSURE_TRUE(mRows, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = mRows->Init();
    if (NS_FAILED(rv)) {
      mRows = nsnull;
      return rv;
    }
  }

  *aValue = mRows;
  NS_ADDREF(*aValue);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableElement::GetTBodies(nsIDOMHTMLCollection** aValue)
{
  if (!mTBodies) {
    // Not using NS_GetContentList because this should not be cached
    mTBodies = new nsContentList(this,
                                 nsGkAtoms::tbody,
                                 mNodeInfo->NamespaceID(),
                                 PR_FALSE);

    NS_ENSURE_TRUE(mTBodies, NS_ERROR_OUT_OF_MEMORY);
  }

  NS_ADDREF(*aValue = mTBodies);
  return NS_OK;
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

    nsCOMPtr<nsIContent> newHead = NS_NewHTMLTableSectionElement(nodeInfo);

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

    nsCOMPtr<nsIContent> newFoot = NS_NewHTMLTableSectionElement(nodeInfo);

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

    nsCOMPtr<nsIContent> newCaption = NS_NewHTMLTableCaptionElement(nodeInfo);

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
       get it's parent
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

    nsCOMPtr<nsIContent> newRow = NS_NewHTMLTableRowElement(nodeInfo);

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
    PRUint32 childCount = GetChildCount();
    for (PRUint32 i = 0; i < childCount; ++i) {
      nsIContent* child = GetChildAt(i);
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
        NS_NewHTMLTableSectionElement(nodeInfo);

      if (newRowGroup) {
        rv = AppendChildTo(newRowGroup, PR_TRUE);

        rowGroup = do_QueryInterface(newRowGroup);
      }
    }

    if (rowGroup) {
      nsCOMPtr<nsINodeInfo> nodeInfo;
      nsContentUtils::NameChanged(mNodeInfo, nsGkAtoms::tr,
                                  getter_AddRefs(nodeInfo));

      nsCOMPtr<nsIContent> newRow = NS_NewHTMLTableRowElement(nodeInfo);
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


PRBool
nsHTMLTableElement::ParseAttribute(PRInt32 aNamespaceID,
                                   nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  /* ignore summary, just a string */
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::cellspacing ||
        aAttribute == nsGkAtoms::cellpadding) {
      return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
    }
    if (aAttribute == nsGkAtoms::cols) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
    if (aAttribute == nsGkAtoms::border) {
      if (!aResult.ParseIntWithBounds(aValue, 0)) {
        // XXX this should really be NavQuirks only to allow non numeric value
        aResult.SetTo(1);
      }

      return PR_TRUE;
    }
    if (aAttribute == nsGkAtoms::height) {
      return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
    }
    if (aAttribute == nsGkAtoms::width) {
      if (aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE)) {
        // treat 0 width as auto
        nsAttrValue::ValueType type = aResult.Type();
        if ((type == nsAttrValue::eInteger &&
             aResult.GetIntegerValue() == 0) ||
            (type == nsAttrValue::ePercent &&
             aResult.GetPercentValue() == 0.0f)) {
          return PR_FALSE;
        }
      }
      return PR_TRUE;
    }
    
    if (aAttribute == nsGkAtoms::align) {
      return ParseTableHAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::bgcolor ||
        aAttribute == nsGkAtoms::bordercolor) {
      return aResult.ParseColor(aValue, GetOwnerDoc());
    }
    if (aAttribute == nsGkAtoms::frame) {
      return aResult.ParseEnumValue(aValue, kFrameTable);
    }
    if (aAttribute == nsGkAtoms::layout) {
      return aResult.ParseEnumValue(aValue, kLayoutTable);
    }
    if (aAttribute == nsGkAtoms::rules) {
      return aResult.ParseEnumValue(aValue, kRulesTable);
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
MapTableFrameInto(const nsMappedAttributes* aAttributes,
                  nsRuleData* aData, PRUint8 aBorderStyle)
{
  if (!aData->mMarginData)
    return;

  // 0 out the sides that we want to hide based on the frame attribute
  const nsAttrValue* frameValue = aAttributes->GetAttr(nsGkAtoms::frame);

  if (frameValue && frameValue->Type() == nsAttrValue::eEnum) {
    // adjust the border style based on the value of frame
    switch (frameValue->GetEnumValue())
    {
    case NS_STYLE_TABLE_FRAME_NONE:
      if (aData->mMarginData->mBorderStyle.mLeft.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mLeft.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mRight.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mRight.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mTop.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mTop.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mBottom.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mBottom.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      break;
    case NS_STYLE_TABLE_FRAME_ABOVE:
      if (aData->mMarginData->mBorderStyle.mLeft.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mLeft.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mRight.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mRight.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mBottom.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mBottom.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      break;
    case NS_STYLE_TABLE_FRAME_BELOW: 
      if (aData->mMarginData->mBorderStyle.mLeft.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mLeft.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mRight.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mRight.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mTop.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mTop.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      break;
    case NS_STYLE_TABLE_FRAME_HSIDES:
      if (aData->mMarginData->mBorderStyle.mLeft.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mLeft.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mRight.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mRight.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      break;
    case NS_STYLE_TABLE_FRAME_LEFT:
      if (aData->mMarginData->mBorderStyle.mRight.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mRight.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mTop.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mTop.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mBottom.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mBottom.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      break;
    case NS_STYLE_TABLE_FRAME_RIGHT:
      if (aData->mMarginData->mBorderStyle.mLeft.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mLeft.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mTop.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mTop.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mBottom.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mBottom.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      break;
    case NS_STYLE_TABLE_FRAME_VSIDES:
      if (aData->mMarginData->mBorderStyle.mTop.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mTop.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      if (aData->mMarginData->mBorderStyle.mBottom.GetUnit() == eCSSUnit_Null)
        aData->mMarginData->mBorderStyle.mBottom.SetIntValue(NS_STYLE_BORDER_STYLE_NONE, eCSSUnit_Enumerated);
      break;
    // BOX and BORDER are ignored, the caller has already set all the border sides
    // any illegal value is also ignored
    }
  }

  // set up defaults
  if (aData->mMarginData->mBorderStyle.mLeft.GetUnit() == eCSSUnit_Null)
    aData->mMarginData->mBorderStyle.mLeft.SetIntValue(aBorderStyle, eCSSUnit_Enumerated);
  if (aData->mMarginData->mBorderStyle.mRight.GetUnit() == eCSSUnit_Null)
    aData->mMarginData->mBorderStyle.mRight.SetIntValue(aBorderStyle, eCSSUnit_Enumerated);
  if (aData->mMarginData->mBorderStyle.mTop.GetUnit() == eCSSUnit_Null)
    aData->mMarginData->mBorderStyle.mTop.SetIntValue(aBorderStyle, eCSSUnit_Enumerated);
  if (aData->mMarginData->mBorderStyle.mBottom.GetUnit() == eCSSUnit_Null)
    aData->mMarginData->mBorderStyle.mBottom.SetIntValue(aBorderStyle, eCSSUnit_Enumerated);

}

// XXX The two callsites care about the two different halves of this
// function, so split it, probably by just putting it in inline at the
// callsites.
static void 
MapTableBorderInto(const nsMappedAttributes* aAttributes,
                   nsRuleData* aData, PRUint8 aBorderStyle)
{
  const nsAttrValue* borderValue = aAttributes->GetAttr(nsGkAtoms::border);
  if (!borderValue && !aAttributes->GetAttr(nsGkAtoms::frame))
    return;

  // the absence of "border" with the presence of "frame" implies
  // border = 1 pixel
  PRInt32 borderThickness = 1;

  if (borderValue && borderValue->Type() == nsAttrValue::eInteger)
    borderThickness = borderValue->GetIntegerValue();

  if (aData->mTableData) {
    if (0 != borderThickness) {
      // border != 0 implies rules=all and frame=border
      aData->mTableData->mRules.SetIntValue(NS_STYLE_TABLE_RULES_ALL, eCSSUnit_Enumerated);
      aData->mTableData->mFrame.SetIntValue(NS_STYLE_TABLE_FRAME_BORDER, eCSSUnit_Enumerated);
    }
    else {
      // border = 0 implies rules=none and frame=void
      aData->mTableData->mRules.SetIntValue(NS_STYLE_TABLE_RULES_NONE, eCSSUnit_Enumerated);
      aData->mTableData->mFrame.SetIntValue(NS_STYLE_TABLE_FRAME_NONE, eCSSUnit_Enumerated);
    }
  }

  if (aData->mMarginData) {
    // by default, set all border sides to the specified width
    if (aData->mMarginData->mBorderWidth.mLeft.GetUnit() == eCSSUnit_Null)
      aData->mMarginData->mBorderWidth.mLeft.SetFloatValue((float)borderThickness, eCSSUnit_Pixel);
    if (aData->mMarginData->mBorderWidth.mRight.GetUnit() == eCSSUnit_Null)
      aData->mMarginData->mBorderWidth.mRight.SetFloatValue((float)borderThickness, eCSSUnit_Pixel);
    if (aData->mMarginData->mBorderWidth.mTop.GetUnit() == eCSSUnit_Null)
      aData->mMarginData->mBorderWidth.mTop .SetFloatValue((float)borderThickness, eCSSUnit_Pixel);
    if (aData->mMarginData->mBorderWidth.mBottom.GetUnit() == eCSSUnit_Null)
      aData->mMarginData->mBorderWidth.mBottom.SetFloatValue((float)borderThickness, eCSSUnit_Pixel);

    // now account for the frame attribute
    MapTableFrameInto(aAttributes, aData, aBorderStyle);
  }
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

  nsCompatibility mode = aData->mPresContext->CompatibilityMode();

  if (aData->mSID == eStyleStruct_TableBorder) {
    const nsStyleDisplay* readDisplay = aData->mStyleContext->GetStyleDisplay();
    if (readDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE_CELL) {
      // cellspacing 
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::cellspacing);
      if (value && value->Type() == nsAttrValue::eInteger) {
        if (aData->mTableData->mBorderSpacing.mXValue.GetUnit() == eCSSUnit_Null)
          aData->mTableData->mBorderSpacing.mXValue.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
        if (aData->mTableData->mBorderSpacing.mYValue.GetUnit() == eCSSUnit_Null)
          aData->mTableData->mBorderSpacing.mYValue.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
      }
      else if (value && value->Type() == nsAttrValue::ePercent && eCompatibility_NavQuirks == mode) {
        // in quirks mode, treat a % cellspacing value a pixel value.
        if (aData->mTableData->mBorderSpacing.mXValue.GetUnit() == eCSSUnit_Null)
          aData->mTableData->mBorderSpacing.mXValue.SetFloatValue(100.0f * value->GetPercentValue(), eCSSUnit_Pixel);
        if (aData->mTableData->mBorderSpacing.mYValue.GetUnit() == eCSSUnit_Null)
          aData->mTableData->mBorderSpacing.mYValue.SetFloatValue(100.0f * value->GetPercentValue(), eCSSUnit_Pixel);
      }
    }
  } 
  else if (aData->mSID == eStyleStruct_Table) {
    const nsStyleDisplay* readDisplay = aData->mStyleContext->GetStyleDisplay();
    if (readDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE_CELL) {
      MapTableBorderInto(aAttributes, aData, 0);

      const nsAttrValue* value;
      // layout
      if (aData->mTableData->mLayout.GetUnit() == eCSSUnit_Null) {
        value = aAttributes->GetAttr(nsGkAtoms::layout);
        if (value && value->Type() == nsAttrValue::eEnum)
          aData->mTableData->mLayout.SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
      }
      
      // cols
      value = aAttributes->GetAttr(nsGkAtoms::cols);
      if (value) {
        if (value->Type() == nsAttrValue::eInteger) 
          aData->mTableData->mCols.SetIntValue(value->GetIntegerValue(), eCSSUnit_Integer);
        else // COLS had no value, so it refers to all columns
          aData->mTableData->mCols.SetIntValue(NS_STYLE_TABLE_COLS_ALL, eCSSUnit_Enumerated);
      }

      // rules
      value = aAttributes->GetAttr(nsGkAtoms::rules);
      if (value && value->Type() == nsAttrValue::eEnum)
        aData->mTableData->mRules.SetIntValue(value->GetEnumValue(), eCSSUnit_Enumerated);
    }
  }
  else if (aData->mSID == eStyleStruct_Margin) {
    const nsStyleDisplay* readDisplay = aData->mStyleContext->GetStyleDisplay();
  
    if (readDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE_CELL) {
      // align; Check for enumerated type (it may be another type if
      // illegal)
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::align);

      if (value && value->Type() == nsAttrValue::eEnum) {
        if (value->GetEnumValue() == NS_STYLE_TEXT_ALIGN_CENTER ||
            value->GetEnumValue() == NS_STYLE_TEXT_ALIGN_MOZ_CENTER) {
          nsCSSRect& margin = aData->mMarginData->mMargin;
          if (margin.mLeft.GetUnit() == eCSSUnit_Null)
            margin.mLeft.SetAutoValue();
          if (margin.mRight.GetUnit() == eCSSUnit_Null)
            margin.mRight.SetAutoValue();
        }
      }

      // hspace is mapped into left and right margin, 
      // vspace is mapped into top and bottom margins
      // - *** Quirks Mode only ***
      if (eCompatibility_NavQuirks == mode) {
        value = aAttributes->GetAttr(nsGkAtoms::hspace);

        if (value && value->Type() == nsAttrValue::eInteger) {
          nsCSSRect& margin = aData->mMarginData->mMargin;
          if (margin.mLeft.GetUnit() == eCSSUnit_Null)
            margin.mLeft.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
          if (margin.mRight.GetUnit() == eCSSUnit_Null)
            margin.mRight.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
        }

        value = aAttributes->GetAttr(nsGkAtoms::vspace);

        if (value && value->Type() == nsAttrValue::eInteger) {
          nsCSSRect& margin = aData->mMarginData->mMargin;
          if (margin.mTop.GetUnit() == eCSSUnit_Null)
            margin.mTop.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
          if (margin.mBottom.GetUnit() == eCSSUnit_Null)
            margin.mBottom.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel); 
        }
      }
    }
  }
  else if (aData->mSID == eStyleStruct_Padding) {
    const nsStyleDisplay* readDisplay = aData->mStyleContext->GetStyleDisplay();
    if (readDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL) {
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::cellpadding);
      if (value) {
        nsAttrValue::ValueType valueType = value->Type();
        if (valueType == nsAttrValue::eInteger || valueType == nsAttrValue::ePercent) {
          // We have cellpadding.  This will override our padding values if we don't
          // have any set.
          nsCSSValue padVal;
          if (valueType == nsAttrValue::eInteger)
            padVal.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
          else {
            // when we support % cellpadding in standard mode, uncomment the following
            float pctVal = value->GetPercentValue();
            //if (eCompatibility_NavQuirks == mode) {
              // in quirks mode treat a pct cellpadding value as a pixel value
              padVal.SetFloatValue(100.0f * pctVal, eCSSUnit_Pixel);
            //}
            //else {
            //  padVal.SetPercentValue(pctVal);
            //}
          }
          if (aData->mMarginData->mPadding.mLeft.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mPadding.mLeft = padVal;
          if (aData->mMarginData->mPadding.mRight.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mPadding.mRight = padVal;
          if (aData->mMarginData->mPadding.mTop.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mPadding.mTop = padVal;
          if (aData->mMarginData->mPadding.mBottom.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mPadding.mBottom = padVal;
        }
      }
    }
  }
  else if (aData->mSID == eStyleStruct_Position) {
    const nsStyleDisplay* readDisplay = aData->mStyleContext->GetStyleDisplay();
  
    if (readDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE_CELL) {
      // width: value
      if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
        const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::width);
        if (value && value->Type() == nsAttrValue::eInteger) 
          aData->mPositionData->mWidth.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
        else if (value && value->Type() == nsAttrValue::ePercent)
          aData->mPositionData->mWidth.SetPercentValue(value->GetPercentValue());
      }

      // height: value
      if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
        const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::height);
        if (value && value->Type() == nsAttrValue::eInteger) 
          aData->mPositionData->mHeight.SetFloatValue((float)value->GetIntegerValue(), eCSSUnit_Pixel);
        else if (value && value->Type() == nsAttrValue::ePercent)
          aData->mPositionData->mHeight.SetPercentValue(value->GetPercentValue()); 
      }
    }
  }
  else if (aData->mSID == eStyleStruct_Visibility) {
    const nsStyleDisplay* readDisplay = aData->mStyleContext->GetStyleDisplay();
  
    if (readDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE_CELL)
      nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
  }
  else if (aData->mSID == eStyleStruct_Border) {
    const nsStyleTableBorder* tableStyle = aData->mStyleContext->GetStyleTableBorder();
    const nsStyleDisplay* readDisplay = aData->mStyleContext->GetStyleDisplay();
    if (readDisplay->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL) {
      if (NS_STYLE_BORDER_SEPARATE == tableStyle->mBorderCollapse) {
        // Set the cell's border from the table in the separate border
        // model. If there is a border on the table, then the mapping to
        // rules=all will take care of borders in the collapsing model.
        // But if rules="none", we don't want to do this.
        const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::border);
        const nsAttrValue* rulesValue = aAttributes->GetAttr(nsGkAtoms::rules);
        if ((!rulesValue || rulesValue->Type() != nsAttrValue::eEnum ||
             rulesValue->GetEnumValue() != NS_STYLE_TABLE_RULES_NONE) &&
            value &&
            ((value->Type() == nsAttrValue::eInteger &&
              value->GetIntegerValue() > 0) ||
             value->IsEmptyString())) {
          if (aData->mMarginData->mBorderWidth.mLeft.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mBorderWidth.mLeft.SetFloatValue(1.0f, eCSSUnit_Pixel);
          if (aData->mMarginData->mBorderWidth.mRight.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mBorderWidth.mRight.SetFloatValue(1.0f, eCSSUnit_Pixel);
          if (aData->mMarginData->mBorderWidth.mTop.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mBorderWidth.mTop.SetFloatValue(1.0f, eCSSUnit_Pixel);
          if (aData->mMarginData->mBorderWidth.mBottom.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mBorderWidth.mBottom.SetFloatValue(1.0f, eCSSUnit_Pixel);

          PRUint8 borderStyle = NS_STYLE_BORDER_STYLE_INSET;

          if (aData->mMarginData->mBorderStyle.mLeft.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mBorderStyle.mLeft.SetIntValue(borderStyle, eCSSUnit_Enumerated);
          if (aData->mMarginData->mBorderStyle.mRight.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mBorderStyle.mRight.SetIntValue(borderStyle, eCSSUnit_Enumerated);
          if (aData->mMarginData->mBorderStyle.mTop.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mBorderStyle.mTop.SetIntValue(borderStyle, eCSSUnit_Enumerated);
          if (aData->mMarginData->mBorderStyle.mBottom.GetUnit() == eCSSUnit_Null)
            aData->mMarginData->mBorderStyle.mBottom.SetIntValue(borderStyle, eCSSUnit_Enumerated);
        }
      }
    }
    else {
      PRUint8 borderStyle = NS_STYLE_BORDER_STYLE_OUTSET;
      // bordercolor
      const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::bordercolor);
      nscolor color;
      if (value && value->GetColorValue(color)) {
        if (aData->mMarginData->mBorderColor.mLeft.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderColor.mLeft.SetColorValue(color);
        if (aData->mMarginData->mBorderColor.mRight.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderColor.mRight.SetColorValue(color);
        if (aData->mMarginData->mBorderColor.mTop.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderColor.mTop.SetColorValue(color);
        if (aData->mMarginData->mBorderColor.mBottom.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderColor.mBottom.SetColorValue(color);

        borderStyle = NS_STYLE_BORDER_STYLE_SOLID; // compat, see bug 349655
      }
      else if (NS_STYLE_BORDER_COLLAPSE == tableStyle->mBorderCollapse) {
        // make the color grey
        nscolor color = NS_RGB(80, 80, 80);
        if (aData->mMarginData->mBorderColor.mLeft.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderColor.mLeft.SetColorValue(color);
        if (aData->mMarginData->mBorderColor.mRight.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderColor.mRight.SetColorValue(color);
        if (aData->mMarginData->mBorderColor.mTop.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderColor.mTop.SetColorValue(color);
        if (aData->mMarginData->mBorderColor.mBottom.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderColor.mBottom.SetColorValue(color);
      }

      // border and frame
      MapTableBorderInto(aAttributes, aData, borderStyle);
    }
  }
  else if (aData->mSID == eStyleStruct_Background) {
    const nsStyleDisplay* readDisplay = aData->mStyleContext->GetStyleDisplay();
  
    if (readDisplay->mDisplay != NS_STYLE_DISPLAY_TABLE_CELL)
      nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aData);
  }
}

NS_IMETHODIMP_(PRBool)
nsHTMLTableElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::layout },
    { &nsGkAtoms::cellpadding },
    { &nsGkAtoms::cellspacing },
    { &nsGkAtoms::cols },
    { &nsGkAtoms::border },
    { &nsGkAtoms::frame },
    { &nsGkAtoms::width },
    { &nsGkAtoms::height },
    { &nsGkAtoms::hspace },
    { &nsGkAtoms::vspace },
    
    { &nsGkAtoms::bordercolor },
    
    { &nsGkAtoms::align },
    { &nsGkAtoms::rules },
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
