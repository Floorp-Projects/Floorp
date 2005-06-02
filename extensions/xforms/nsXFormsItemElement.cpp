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
#include "nsIXTFGenericElementWrapper.h"
#include "nsIDOMDocument.h"
#include "nsString.h"
#include "nsXFormsUtils.h"
#include "nsIXFormsValueElement.h"
#include "nsArray.h"
#include "nsVoidArray.h"
#include "nsIDOMText.h"
#include "nsIXTFXMLVisualWrapper.h"
#include "nsIXFormsContextControl.h"
#include "nsIModelElementPrivate.h"

/**
 * nsXFormsItemElement implements the XForms \<item\> element.
 * It creates an anonymous HTML option element with the appropriate label,
 * which is inserted into the anonymous content tree of the containing
 * select element.
 */

class nsXFormsItemElement : public nsXFormsStubElement,
                            public nsIXFormsSelectChild
{
public:
  nsXFormsItemElement() : mElement(nsnull),
                          mContextPosition(0),
                          mContextSize(0)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFGenericElement overrides
  NS_IMETHOD OnCreated(nsIXTFGenericElementWrapper *aWrapper);

  // nsIXTFElement overrides
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);
  NS_IMETHOD ChildInserted(nsIDOMNode *aChild, PRUint32 aIndex);
  NS_IMETHOD ChildAppended(nsIDOMNode *aChild);
  NS_IMETHOD WillRemoveChild(PRUint32 aIndex);
  NS_IMETHOD BeginAddingChildren();
  NS_IMETHOD DoneAddingChildren();

  // nsIXFormsSelectChild
  NS_DECL_NSIXFORMSSELECTCHILD

private:
  NS_HIDDEN_(void) Refresh();
  NS_HIDDEN_(nsresult) GetValue(nsString &aValue);

  nsIDOMElement *mElement;
  nsCOMPtr<nsIDOMHTMLOptionElement> mOption;

  // context node (used by itemset)
  nsCOMPtr<nsIDOMElement> mContextNode;
  PRInt32 mContextPosition;
  PRInt32 mContextSize;
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsItemElement,
                             nsXFormsStubElement,
                             nsIXFormsSelectChild)

NS_IMETHODIMP
nsXFormsItemElement::OnCreated(nsIXTFGenericElementWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_PARENT_CHANGED |
                                nsIXTFElement::NOTIFY_CHILD_INSERTED |
                                nsIXTFElement::NOTIFY_CHILD_APPENDED |
                                nsIXTFElement::NOTIFY_WILL_REMOVE_CHILD |
                                nsIXTFElement::NOTIFY_BEGIN_ADDING_CHILDREN);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  // Our anonymous content structure will look like this:
  //
  // <option/>   (mOption, also insertion point)
  //

  nsCOMPtr<nsIDOMDocument> domDoc;
  node->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDOMElement> element;
  domDoc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XHTML),
                          NS_LITERAL_STRING("option"),
                          getter_AddRefs(element));

  mOption = do_QueryInterface(element);
  NS_ENSURE_TRUE(mOption, NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::ParentChanged(nsIDOMElement *aNewParent)
{
  if (aNewParent)
    Refresh();

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
nsXFormsItemElement::GetAnonymousNodes(nsIArray **aNodes)
{
  nsCOMPtr<nsIMutableArray> array;
  nsresult rv = NS_NewArray(getter_AddRefs(array));
  NS_ENSURE_SUCCESS(rv, rv);

  array->AppendElement(mOption, PR_FALSE);

  NS_ADDREF(*aNodes = array);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::SelectItemsByValue(const nsStringArray &aValueList)
{
  nsAutoString value;
  nsresult rv = GetValue(value);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 count = aValueList.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    if (aValueList[i]->Equals(value)) {
      mOption->SetSelected(PR_TRUE);
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::SelectItemsByContent(nsIDOMNode *aNode)
{
  // TODO: implement support for \<copy\> element.
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::WriteSelectedItems(nsIDOMNode *aContainer, 
                                        nsAString  &aStringBuffer)
{
  PRBool selected;
  mOption->GetSelected(&selected);
  if (!selected)
    return NS_OK;

  nsAutoString value;
  nsresult rv = GetValue(value);
  NS_ENSURE_SUCCESS(rv, rv);

  if(aStringBuffer.IsEmpty()){
    aStringBuffer.Append(value);
  }
  else{
    aStringBuffer.Append(NS_LITERAL_STRING(" ") + value);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsItemElement::SetContext(nsIDOMElement *aContextNode,
                                PRInt32 aContextPosition, PRInt32 aContextSize)
{
  mContextNode = aContextNode;
  mContextPosition = aContextPosition;
  mContextSize = aContextSize;
  return NS_OK;
}

// internal methods

nsresult
nsXFormsItemElement::GetValue(nsString &aValue)
{
  // Find our value child and get its text content.
  nsCOMPtr<nsIDOMNodeList> children;
  nsresult rv = mElement->GetChildNodes(getter_AddRefs(children));
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
  // Clear out all of the existing option text
  nsCOMPtr<nsIDOMNode> child, child2;
  while (NS_SUCCEEDED(mOption->GetFirstChild(getter_AddRefs(child))) && child)
    mOption->RemoveChild(child, getter_AddRefs(child2));
    
  // We need to find our label child, serialize its contents
  // (which can be xtf-generated), and set that as our label attribute.

  nsCOMPtr<nsIDOMNodeList> children;
  nsresult rv = mElement->GetChildNodes(getter_AddRefs(children));
  if (NS_FAILED(rv))
    return;

  PRUint32 childCount;
  children->GetLength(&childCount);

  nsAutoString value;

  nsCOMPtr<nsIDOMDocument> doc;
  mElement->GetOwnerDocument(getter_AddRefs(doc));

  nsCOMPtr<nsIDOMElement> container;
  doc->CreateElementNS(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS),
                       NS_LITERAL_STRING("contextcontainer-inline"),
                       getter_AddRefs(container));

  NS_NAMED_LITERAL_STRING(modelStr, "model");

  nsAutoString modelID;
  mElement->GetAttribute(modelStr, modelID);
  container->SetAttribute(modelStr, modelID);

  nsCOMPtr<nsIXFormsContextControl> context = do_QueryInterface(container);
  context->SetContext(mContextNode, 1, 1);

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(child));
    child->GetLocalName(value);
    if (value.EqualsLiteral("label")) {
      child->GetNamespaceURI(value);
      if (value.EqualsLiteral(NS_NAMESPACE_XFORMS)) {
        child->CloneNode(PR_TRUE, getter_AddRefs(child2));
        container->AppendChild(child2, getter_AddRefs(child));
      }
    }
  }

  nsCOMPtr<nsIDOMNode> childReturn;
  mOption->AppendChild(container, getter_AddRefs(childReturn));
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
