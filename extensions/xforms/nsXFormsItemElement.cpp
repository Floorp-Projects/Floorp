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
#include "nsString.h"
#include "nsXFormsUtils.h"
#include "nsIXFormsValueElement.h"
#include "nsArray.h"
#include "nsVoidArray.h"
#include "nsIDOMText.h"
#include "nsIXTFBindableElementWrapper.h"
#include "nsIXFormsContextControl.h"
#include "nsIModelElementPrivate.h"
#include "nsIXFormsItemElement.h"
#include "nsIXFormsControl.h"
#include "nsIXFormsLabelElement.h"
#include "nsIDocument.h"
#include "nsXFormsModelElement.h"

/**
 * nsXFormsItemElement implements the XForms \<item\> element.
 * It creates an anonymous HTML option element with the appropriate label,
 * which is inserted into the anonymous content tree of the containing
 * select element.
 */

class nsXFormsItemElement : public nsXFormsBindableStub,
                            public nsIXFormsSelectChild,
                            public nsIXFormsItemElement
{
public:
  nsXFormsItemElement() : mElement(nsnull), mDoneAddingChildren(PR_FALSE)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXFORMSITEMELEMENT

  NS_IMETHOD OnCreated(nsIXTFBindableElementWrapper *aWrapper);

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
};

NS_IMPL_ISUPPORTS_INHERITED2(nsXFormsItemElement,
                             nsXFormsBindableStub,
                             nsIXFormsSelectChild,
                             nsIXFormsItemElement)

NS_IMETHODIMP
nsXFormsItemElement::OnCreated(nsIXTFBindableElementWrapper *aWrapper)
{
  nsresult rv = nsXFormsBindableStub::OnCreated(aWrapper);
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
  nsAutoString value;
  GetValue(value);
  if (aValue.Equals(value)) {
    NS_ADDREF(*aSelected = mElement);
  } else {
    *aSelected = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::GetValue(nsAString &aValue)
{
 
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
      nsCOMPtr<nsIXFormsLabelElement> label(do_QueryInterface(child));
      if (label) {
        label->GetTextValue(aValue);
        return NS_OK;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::LabelRefreshed()
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  // This is an optimization. It prevents us doing some of the unnecessary
  // refreshes.
  if (doc && doc->GetProperty(nsXFormsAtoms::deferredBindListProperty)) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mElement);
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
      return NS_OK;
    }
    current = parent;
  } while(current);
  return NS_OK;
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
