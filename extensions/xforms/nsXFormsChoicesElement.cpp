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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIXFormsSelectChild.h"
#include "nsXFormsStubElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIXTFBindableElementWrapper.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsXFormsUtils.h"
#include "nsArray.h"
#include "nsIXTFXMLVisualWrapper.h"

/**
 * Implementation of XForms \<choices\> element.  This creates an HTML
 * optgroup element and inserts any childrens' anonymous content as children
 * of the optgroup.
 */

class nsXFormsChoicesElement : public nsXFormsBindableStub,
                               public nsIXFormsSelectChild
{
public:
  nsXFormsChoicesElement() : mElement(nsnull) {}

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD OnCreated(nsIXTFBindableElementWrapper *aWrapper);

  // nsIXTFElement overrides
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);
  NS_IMETHOD ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex);
  NS_IMETHOD ChildAppended(nsIDOMNode *aChild);
  NS_IMETHOD ChildRemoved(PRUint32 aIndex);
  NS_IMETHOD BeginAddingChildren();
  NS_IMETHOD DoneAddingChildren();

  // nsIXFormsControlBase overrides
  NS_IMETHOD Refresh();

  // nsIXFormsSelectChild
  NS_DECL_NSIXFORMSSELECTCHILD

private:
  nsIDOMElement*                      mElement;
  nsCOMPtr<nsIDOMHTMLOptGroupElement> mOptGroup;
  PRBool                              mInsideSelect1;
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsChoicesElement,
                             nsXFormsBindableStub,
                             nsIXFormsSelectChild)

NS_IMETHODIMP
nsXFormsChoicesElement::OnCreated(nsIXTFBindableElementWrapper *aWrapper)
{
  nsresult rv = nsXFormsBindableStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_PARENT_CHANGED |
                                nsIXTFElement::NOTIFY_CHILD_INSERTED |
                                nsIXTFElement::NOTIFY_CHILD_APPENDED |
                                nsIXTFElement::NOTIFY_CHILD_REMOVED |
                                nsIXTFElement::NOTIFY_BEGIN_ADDING_CHILDREN);

  mInsideSelect1 = PR_FALSE;

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  // Our anonymous content structure in <select> will look like this:
  //
  // <optgroup label="myLabel"/>   (mOptGroup)
  //
  // <select1>'s anonymous content is created in XBL.

  nsCOMPtr<nsIDOMDocument> domDoc;
  node->GetOwnerDocument(getter_AddRefs(domDoc));

  //XXX Remove this when XBLizing <select>.
  nsCOMPtr<nsIDOMElement> element;
  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("optgroup"),
                          getter_AddRefs(element));

  mOptGroup = do_QueryInterface(element);
  NS_ENSURE_TRUE(mOptGroup, NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsChoicesElement::ParentChanged(nsIDOMElement *aNewParent)
{
  mInsideSelect1 = PR_FALSE;
  if (aNewParent) {
    nsCOMPtr<nsIDOMNode> parent, tmp;
    mElement->GetParentNode(getter_AddRefs(parent));
    while (parent) {
      if (nsXFormsUtils::IsXFormsElement(parent, NS_LITERAL_STRING("select1"))) {
        mInsideSelect1 = PR_TRUE;
        break;
      }
      tmp.swap(parent);
      tmp->GetParentNode(getter_AddRefs(parent));
    }
    Refresh();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsChoicesElement::ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex)
{
  nsCOMPtr<nsIXFormsSelectChild> item = do_QueryInterface(aChild);
  if (item) {
    // XXX get anonymous nodes and insert them at the correct place
  } else if (nsXFormsUtils::IsLabelElement(aChild)) {
    // If a label child was inserted, we need to clone it into our
    // anonymous content.
    Refresh();
  }
    
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsChoicesElement::ChildAppended(nsIDOMNode *aChild)
{
  nsCOMPtr<nsIXFormsSelectChild> item = do_QueryInterface(aChild);
  if (item) {
    // XXX get anonymous nodes and insert them at the correct place
  } else if (nsXFormsUtils::IsLabelElement(aChild)) {
    // If a label child was inserted, we need to clone it into our
    // anonymous content.
    Refresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsChoicesElement::ChildRemoved(PRUint32 aIndex)
{
  // TODO: should remove the anonymous content owned by the removed item.

  // TOOD: only need to Refresh() when a label child is removed, but it's
  // not possible to get ahold of the actual removed child at this point.

  Refresh();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsChoicesElement::BeginAddingChildren()
{
  // Suppress child notifications until we're done getting children.
  nsCOMPtr<nsIXTFElementWrapper> wrapper = do_QueryInterface(mElement);
  NS_ASSERTION(wrapper, "huh? our element must be an xtf wrapper");

  wrapper->SetNotificationMask(nsIXTFElement::NOTIFY_PARENT_CHANGED |
                               nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsChoicesElement::DoneAddingChildren()
{
  // Unsuppress notifications
  nsCOMPtr<nsIXTFElementWrapper> wrapper = do_QueryInterface(mElement);
  NS_ASSERTION(wrapper, "huh? our element must be an xtf wrapper");

  wrapper->SetNotificationMask(nsIXTFElement::NOTIFY_PARENT_CHANGED |
                               nsIXTFElement::NOTIFY_CHILD_INSERTED |
                               nsIXTFElement::NOTIFY_CHILD_APPENDED |
                               nsIXTFElement::NOTIFY_CHILD_REMOVED);

  // Walk our children and get their anonymous content.
  nsCOMPtr<nsIDOMNodeList> children;
  nsresult rv = mElement->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> childNode, childAnonymousNode, nodeReturn;
  nsCOMPtr<nsIXFormsSelectChild> childItem;

  PRUint32 childCount = 0;
  children->GetLength(&childCount);

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(childNode));
    childItem = do_QueryInterface(childNode);
    if (childItem) {
      nsCOMPtr<nsIArray> childItems;
      childItem->GetAnonymousNodes(getter_AddRefs(childItems));
      if (childItems) {
        PRUint32 childNodesLength = 0;
        childItems->GetLength(&childNodesLength);

        for (PRUint32 j = 0; j < childNodesLength; ++j) {
          childAnonymousNode = do_QueryElementAt(childItems, j);
          mOptGroup->AppendChild(childAnonymousNode,
                                 getter_AddRefs(nodeReturn));
        }
      }
    }
  }

  // Assume that we've got something worth refreshing now.
  Refresh();
  return NS_OK;
}

// nsIXFormsSelectChild

NS_IMETHODIMP
nsXFormsChoicesElement::GetAnonymousNodes(nsIArray **aNodes)
{
  nsCOMPtr<nsIMutableArray> array;
  nsresult rv = NS_NewArray(getter_AddRefs(array));
  NS_ENSURE_SUCCESS(rv, rv);

  array->AppendElement(mOptGroup, PR_FALSE);

  NS_ADDREF(*aNodes = array);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsChoicesElement::SelectItemByValue(const nsAString &aValue, nsIDOMNode **aSelected)
{
  NS_ENSURE_ARG_POINTER(aSelected);
  NS_ENSURE_STATE(mElement);
  *aSelected = nsnull;
  nsCOMPtr<nsIDOMNodeList> children;
  nsresult rv = mElement->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount = 0;
  children->GetLength(&childCount);

  nsCOMPtr<nsIDOMNode> childNode;
  nsCOMPtr<nsIXFormsSelectChild> childItem;

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(childNode));
    childItem = do_QueryInterface(childNode);
    if (childItem) {
      childItem->SelectItemByValue(aValue, aSelected);
      if (*aSelected)
        return NS_OK;
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsChoicesElement::SelectItemsByValue(const nsStringArray &aValueList)
{
  nsCOMPtr<nsIDOMNodeList> children;
  nsresult rv = mElement->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount = 0;
  children->GetLength(&childCount);

  nsCOMPtr<nsIDOMNode> childNode;
  nsCOMPtr<nsIXFormsSelectChild> childItem;

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(childNode));
    childItem = do_QueryInterface(childNode);
    if (childItem) {
      childItem->SelectItemsByValue(aValueList);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsChoicesElement::SelectItemsByContent(nsIDOMNode *aNode)
{
  // Not applicable for choices element.
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsChoicesElement::WriteSelectedItems(nsIDOMNode *aContainer, 
                                           nsAString  &aStringBuffer)
{
  nsCOMPtr<nsIDOMNodeList> children;
  nsresult rv = mElement->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount = 0;
  children->GetLength(&childCount);

  nsCOMPtr<nsIDOMNode> childNode;
  nsCOMPtr<nsIXFormsSelectChild> childItem;

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(childNode));
    childItem = do_QueryInterface(childNode);
    if (childItem) {
      childItem->WriteSelectedItems(aContainer, aStringBuffer);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsChoicesElement::SetContext(nsIDOMElement *aContextNode,
                                   PRInt32 aContextPosition,
                                   PRInt32 aContextSize)
{
  // choices cannot be created by an itemset, so we don't need to worry
  // about this case.
  return NS_OK;
}

// internal methods

NS_IMETHODIMP
nsXFormsChoicesElement::Refresh()
{
  if (mInsideSelect1) {
    // <select1> is XBLized, so there is no need to recreate UI -
    // XBL will do it for us.
    return NS_OK;
  }

  // Remove any existing first child that is not an option, to clear out
  // any existing label.
  nsCOMPtr<nsIDOMNode> firstChild, child2;
  nsresult rv = mOptGroup->GetFirstChild(getter_AddRefs(firstChild));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;

  if (firstChild) {
    firstChild->GetLocalName(value);
    if (!value.EqualsLiteral("option")) {
      mOptGroup->RemoveChild(firstChild, getter_AddRefs(child2));
      mOptGroup->GetFirstChild(getter_AddRefs(firstChild));
    }
  }

  // We need to find our label element and clone it into the optgroup.
  // This content will appear before any of the optgroup's options.

  nsCOMPtr<nsIDOMNodeList> children;
  rv = mElement->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount;
  children->GetLength(&childCount);

  nsCOMPtr<nsIDOMNode> child;
  nsAutoString labelValue;

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(child));
    if (nsXFormsUtils::IsLabelElement(child)) {
      // Clone our label into the optgroup
      child->CloneNode(PR_TRUE, getter_AddRefs(child2));
      mOptGroup->InsertBefore(child2, firstChild, getter_AddRefs(child));
    }
  }

  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsChoicesElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsChoicesElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

