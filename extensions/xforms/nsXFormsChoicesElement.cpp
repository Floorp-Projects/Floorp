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
#include "nsIXFormsControl.h"
#include "nsXFormsUtils.h"
#include "nsArray.h"
#include "nsIXTFXMLVisualWrapper.h"

class nsXFormsChoicesElement : public nsXFormsBindableStub,
                               public nsIXFormsSelectChild
{
public:
  nsXFormsChoicesElement() : mElement(nsnull), mDoneAddingChildren(PR_FALSE) {}

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD OnCreated(nsIXTFBindableElementWrapper *aWrapper);

  // nsIXTFElement overrides
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);
  NS_IMETHOD ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex);
  NS_IMETHOD ChildAppended(nsIDOMNode *aChild);
  NS_IMETHOD ChildRemoved(PRUint32 aIndex);
  NS_IMETHOD BeginAddingChildren();
  NS_IMETHOD DoneAddingChildren();

  // nsIXFormsSelectChild
  NS_DECL_NSIXFORMSSELECTCHILD

private:
  nsIDOMElement* mElement;
  PRBool         mDoneAddingChildren;

  void Refresh();

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
nsXFormsChoicesElement::ParentChanged(nsIDOMElement *aNewParent)
{
  if (aNewParent) {
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
  // XXX: only need to Refresh() when a label child is removed, but it's
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
  mDoneAddingChildren = PR_TRUE;

  // Unsuppress notifications
  nsCOMPtr<nsIXTFElementWrapper> wrapper = do_QueryInterface(mElement);
  NS_ASSERTION(wrapper, "huh? our element must be an xtf wrapper");

  wrapper->SetNotificationMask(nsIXTFElement::NOTIFY_PARENT_CHANGED |
                               nsIXTFElement::NOTIFY_CHILD_INSERTED |
                               nsIXTFElement::NOTIFY_CHILD_APPENDED |
                               nsIXTFElement::NOTIFY_CHILD_REMOVED);

  // Assume that we've got something worth refreshing now.
  Refresh();
  return NS_OK;
}

// nsIXFormsSelectChild

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

// internal methods

void
nsXFormsChoicesElement::Refresh()
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

NS_HIDDEN_(nsresult)
NS_NewXFormsChoicesElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsChoicesElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

