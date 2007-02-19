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
#include "nsIDOMHTMLOptionElement.h"
#include "nsXFormsAtoms.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsString.h"
#include "nsXFormsUtils.h"
#include "nsIXFormsValueElement.h"
#include "nsVoidArray.h"
#include "nsIDOMText.h"
#include "nsIXTFElementWrapper.h"
#include "nsIXFormsContextControl.h"
#include "nsIModelElementPrivate.h"
#include "nsIXFormsItemElement.h"
#include "nsIXFormsControl.h"
#include "nsIDocument.h"
#include "nsXFormsModelElement.h"
#include "nsIXFormsCopyElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIXFormsDelegate.h"
#include "nsIXFormsAccessors.h"

/**
 * nsXFormsItemElement implements the XForms \<item\> element.
 * It creates an anonymous HTML option element with the appropriate label,
 * which is inserted into the anonymous content tree of the containing
 * select element.
 */

class nsXFormsItemElement : public nsXFormsStubElement,
                            public nsIXFormsSelectChild,
                            public nsIXFormsItemElement
{
public:
  nsXFormsItemElement() : mElement(nsnull), mDoneAddingChildren(PR_FALSE),
                          mIsCopyItem(PR_FALSE)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXFORMSITEMELEMENT

  NS_IMETHOD OnCreated(nsIXTFElementWrapper *aWrapper);

  // nsIXTFElement overrides
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);
  NS_IMETHOD ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex);
  NS_IMETHOD ChildAppended(nsIDOMNode *aChild);
  NS_IMETHOD WillRemoveChild(PRUint32 aIndex);
  NS_IMETHOD BeginAddingChildren();
  NS_IMETHOD DoneAddingChildren();

  void Refresh();

  // nsIXFormsSelectChild
  NS_DECL_NSIXFORMSSELECTCHILD


private:
  nsIDOMElement* mElement;
  PRBool         mDoneAddingChildren;

  // If true, indicates that this item contains a xf:copy element (via
  // xf:itemset) rather than a xf:value element
  PRBool         mIsCopyItem;
};

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsItemElement,
                             nsXFormsStubElement,
                             nsIXFormsSelectChild,
                             nsIXFormsItemElement)

NS_IMETHODIMP
nsXFormsItemElement::OnCreated(nsIXTFElementWrapper *aWrapper)
{
  nsresult rv = nsXFormsStubElement::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);
  
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_PARENT_CHANGED |
                                nsIXTFElement::NOTIFY_CHILD_INSERTED |
                                nsIXTFElement::NOTIFY_CHILD_APPENDED |
                                nsIXTFElement::NOTIFY_WILL_REMOVE_CHILD |
                                nsIXTFElement::NOTIFY_BEGIN_ADDING_CHILDREN |
                                nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::ParentChanged(nsIDOMElement *aNewParent)
{
  if (aNewParent) {
    Refresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex)
{
  // If a label child was inserted, we need to clone it into our
  // anonymous content.

  if (nsXFormsUtils::IsLabelElement(aChild))
    Refresh();

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::ChildAppended(nsIDOMNode *aChild)
{
  // If a label child was inserted, we need to clone it into our
  // anonymous content.

  if (nsXFormsUtils::IsLabelElement(aChild))
    Refresh();

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::WillRemoveChild(PRUint32 aIndex)
{
  nsCOMPtr<nsIDOMNodeList> children;
  nsresult rv = mElement->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> child;
  children->Item(aIndex, getter_AddRefs(child));

  if (child && nsXFormsUtils::IsLabelElement(child)) {
    // If a label child was removed, we need to remove the label from our
    // anonymous content.
    Refresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::BeginAddingChildren()
{
  // Suppress child notifications until we're done getting children.
  nsCOMPtr<nsIXTFElementWrapper> wrapper = do_QueryInterface(mElement);
  NS_ASSERTION(wrapper, "huh? our element must be an xtf wrapper");

  wrapper->SetNotificationMask(nsIXTFElement::NOTIFY_PARENT_CHANGED |
                               nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::DoneAddingChildren()
{
  mDoneAddingChildren = PR_TRUE;

  // Unsuppress notifications
  nsCOMPtr<nsIXTFElementWrapper> wrapper = do_QueryInterface(mElement);
  NS_ASSERTION(wrapper, "huh? our element must be an xtf wrapper");

  wrapper->SetNotificationMask(nsIXTFElement::NOTIFY_PARENT_CHANGED |
                               nsIXTFElement::NOTIFY_CHILD_INSERTED |
                               nsIXTFElement::NOTIFY_CHILD_APPENDED |
                               nsIXTFElement::NOTIFY_WILL_REMOVE_CHILD);

  // Assume that we've got something worth refreshing now.
  Refresh();
  return NS_OK;
}

// nsIXFormsSelectChild

NS_IMETHODIMP
nsXFormsItemElement::SelectItemByValue(const nsAString &aValue, nsIDOMNode **aSelected)
{
  NS_ENSURE_ARG_POINTER(aSelected);
  NS_ENSURE_STATE(mElement);

  *aSelected = nsnull;
  if (mIsCopyItem) {
    // copy items are selected by node, not by value
    return NS_OK;
  }

  nsAutoString value;
  nsresult rv = GetValue(value);

  if (NS_SUCCEEDED(rv) && aValue.Equals(value)) {
    NS_ADDREF(*aSelected = mElement);
  }

  return rv;
}

NS_IMETHODIMP
nsXFormsItemElement::SelectItemByNode(nsIDOMNode *aNode, nsIDOMNode **aSelected)
{
  NS_ENSURE_ARG_POINTER(aSelected);
  NS_ENSURE_STATE(mElement);
  PRBool isCopyItem;
  *aSelected = nsnull;

  // If this item doesn't contain a copy element but instead has a value
  // element, then there is no sense testing further.
  GetIsCopyItem(&isCopyItem);
  if (!isCopyItem) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNode> copyNode;
  GetCopyNode(getter_AddRefs(copyNode));
  NS_ENSURE_STATE(copyNode);

  PRUint16 nodeType;
  copyNode->GetNodeType(&nodeType);

  // copy elements are only allowed to bind to ELEMENT_NODEs per spec.  But
  // test first before doing all of this work.
  if ((nodeType == nsIDOMNode::ELEMENT_NODE) && 
      (nsXFormsUtils::AreNodesEqual(copyNode, aNode))) {
    NS_ADDREF(*aSelected = mElement);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::GetValue(nsAString &aValue)
{
  PRBool isCopyItem;
  GetIsCopyItem(&isCopyItem);
  if (isCopyItem) {
    // if this item was built by an itemset and the itemset's template used
    // a copy element, then there is no value element to be had.  No sense
    // continuing.
    aValue.Truncate(0);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNode> firstChild, container;
  mElement->GetFirstChild(getter_AddRefs(firstChild));

  // If this element is generated inside an <itemset>,
  // there is a <contextcontainer> element between this element
  // and the actual childnodes.
  if (nsXFormsUtils::IsXFormsElement(firstChild,
                                     NS_LITERAL_STRING("contextcontainer"))) {
    container = firstChild;
  } else {
    container = mElement;
  }
  
  // Find our value child and get its text content.
  nsCOMPtr<nsIDOMNodeList> children;
  nsresult rv = container->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount;
  children->GetLength(&childCount);

  nsCOMPtr<nsIDOMNode> child;
  nsAutoString value;

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(child));
    nsCOMPtr<nsIXFormsValueElement> valueElement = do_QueryInterface(child);
    if (valueElement) {
      return valueElement->GetValue(aValue);
    }
  }     

  // No value children, so our value is empty
  aValue.Truncate(0);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::GetCopyNode(nsIDOMNode **aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);

  PRBool isCopyItem;
  GetIsCopyItem(&isCopyItem);
  if (!isCopyItem) {
    // If this item doesn't contain a copy element but instead has a value
    // element, then there is no sense continuing.
    *aNode = nsnull;
    return NS_ERROR_FAILURE;
  }

  // Since this item really contains a copy element, then firstChild MUST be
  // a contextcontainer since copy elements can only exist as a child of an
  // itemset.
  nsCOMPtr<nsIDOMNode> container;
  mElement->GetFirstChild(getter_AddRefs(container));

  // Find the copy element contained by this item and get the copyNode from it.
  nsCOMPtr<nsIDOMNodeList> children;
  nsresult rv = container->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 childCount;
  children->GetLength(&childCount);

  nsCOMPtr<nsIDOMNode> child;
  nsAutoString value;

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(child));
    nsCOMPtr<nsIXFormsCopyElement> copyElement = do_QueryInterface(child);
    if (copyElement) {
      return copyElement->GetCopyNode(aNode);
    }
  }     

  // No copy element as a child.  Set return node to null and set the copyitem
  // boolean to false so we don't go through this unnecessary pain again.
  aNode = nsnull;
  SetIsCopyItem(PR_FALSE);
  return NS_OK;
}

void
nsXFormsItemElement::Refresh()
{
  if (mDoneAddingChildren) {
    nsCOMPtr<nsIDOMNode> parent, current;
    current = mElement;
    do {
      current->GetParentNode(getter_AddRefs(parent));
      if (nsXFormsUtils::IsXFormsElement(parent, NS_LITERAL_STRING("select1")) ||
          nsXFormsUtils::IsXFormsElement(parent, NS_LITERAL_STRING("select"))) {
        nsCOMPtr<nsIXFormsControl> select(do_QueryInterface(parent));
        if (select) {
          select->Refresh();
        }
        return;
      }
      current = parent;
    } while(current);
  }
}
NS_IMETHODIMP
nsXFormsItemElement::SetActive(PRBool aActive)
{
  /// @see comment in nsIXFormsItemElement.idl

  if (aActive) {
    // Fire 'DOMMenuItemActive' event. This event is used by accessible module.
    nsCOMPtr<nsIDOMDocument> domDoc;
    mElement->GetOwnerDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDOMDocumentEvent> doc = do_QueryInterface(domDoc);
    NS_ENSURE_STATE(doc);

    nsCOMPtr<nsIDOMEvent> event;
    doc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

    event->InitEvent(NS_LITERAL_STRING("DOMMenuItemActive"),
                     PR_TRUE, PR_TRUE);

    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mElement));
    NS_ENSURE_STATE(node);

    nsXFormsUtils::SetEventTrusted(event, node);

    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mElement));
    NS_ENSURE_STATE(target);

    PRBool defaultActionEnabled = PR_TRUE;
    nsresult rv = target->DispatchEvent(event, &defaultActionEnabled);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_NAMED_LITERAL_STRING(active, "_moz_active");

  return aActive ?
    mElement->SetAttribute(active, NS_LITERAL_STRING("1")) :
    mElement->RemoveAttribute(active);
}

NS_IMETHODIMP
nsXFormsItemElement::GetLabelText(nsAString& aValue)
{
  NS_ENSURE_STATE(mElement);
  aValue.Truncate(0);
  
  nsCOMPtr<nsIDOMNode> firstChild, container;
  mElement->GetFirstChild(getter_AddRefs(firstChild));

  // If this element is generated inside an <itemset>,
  // there is a <contextcontainer> element between this element
  // and the actual childnodes.
  if (nsXFormsUtils::IsXFormsElement(firstChild,
                                     NS_LITERAL_STRING("contextcontainer"))) {
    container = firstChild;
  } else {
    container = mElement;
  }
  
  nsCOMPtr<nsIDOMNodeList> children;
  container->GetChildNodes(getter_AddRefs(children));
  NS_ENSURE_STATE(children);

  PRUint32 childCount;
  children->GetLength(&childCount);

  nsCOMPtr<nsIDOMDocument> doc;
  mElement->GetOwnerDocument(getter_AddRefs(doc));

  nsCOMPtr<nsIDOMNode> child;
  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(child));
    if (nsXFormsUtils::IsXFormsElement(child, NS_LITERAL_STRING("label"))) {
      nsCOMPtr<nsIXFormsDelegate> label(do_QueryInterface(child));
      NS_ENSURE_STATE(label);

      nsCOMPtr<nsIXFormsAccessors> accessors;
      label->GetXFormsAccessors(getter_AddRefs(accessors));
      NS_ENSURE_STATE(accessors);

      return accessors->GetValue(aValue);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::GetIsCopyItem(PRBool *aIsCopyItem)
{
  NS_ENSURE_ARG(aIsCopyItem);
  *aIsCopyItem = mIsCopyItem;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::SetIsCopyItem(PRBool aIsCopyItem)
{
  mIsCopyItem = aIsCopyItem;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::CopyNodeEquals(nsIDOMNode *aNode, PRBool *aIsCopyNode)
{
  NS_ENSURE_ARG(aNode);
  NS_ENSURE_ARG_POINTER(aIsCopyNode);

  nsCOMPtr<nsIDOMNode> selectedNode;
  nsresult rv = SelectItemByNode(aNode, getter_AddRefs(selectedNode));
  *aIsCopyNode = !!(selectedNode);

  return rv;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsItemElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsItemElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
