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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Aaron Reed <aaronr@us.ibm.com>
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

#include "nsIXFormsCopyElement.h"
#include "nsXFormsStubElement.h"
#include "nsIXTFGenericElementWrapper.h"
#include "nsXFormsUtils.h"
#include "nsIDOMElement.h"
#include "nsString.h"
#include "nsIXFormsItemElement.h"

/**
 * Implementation of the XForms \<copy\> element.
 *
 * @note The copy element does not display any content, it simply provides
 * a node to be copied to instance data when the containing XForms item
 * element is selected.
 */

class nsXFormsCopyElement : public nsXFormsStubElement,
                            public nsIXFormsCopyElement
{
public:
  nsXFormsCopyElement() : mElement(nsnull) {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFGenericElement overrides
  NS_IMETHOD OnCreated(nsIXTFGenericElementWrapper *aWrapper);

  // nsIXTFElement overrides
  NS_IMETHOD ParentChanged(nsIDOMElement *aNewParent);
  NS_IMETHOD DocumentChanged(nsIDOMDocument *aNewParent);

  // nsIXFormsCopyElement
  NS_DECL_NSIXFORMSCOPYELEMENT

private:
  nsIDOMElement *mElement;
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsCopyElement,
                             nsXFormsStubElement,
                             nsIXFormsCopyElement)

NS_IMETHODIMP
nsXFormsCopyElement::OnCreated(nsIXTFGenericElementWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_PARENT_CHANGED |
                                nsIXTFElement::NOTIFY_DOCUMENT_CHANGED);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  return NS_OK;
}

// nsIXTFElement

NS_IMETHODIMP
nsXFormsCopyElement::ParentChanged(nsIDOMElement *aNewParent)
{
  if (aNewParent) {
    if (!nsXFormsUtils::IsXFormsElement(aNewParent, 
                                        NS_LITERAL_STRING("itemset")) &&
        !nsXFormsUtils::IsXFormsElement(aNewParent, 
          NS_LITERAL_STRING("contextcontainer"))) {

        // parent of a copy element must always be an itemset.  We really can't
        // enforce this all that well until we have full schema support but for
        // now we'll at least warn the author.  We are also checking for
        // contextcontainer because under Mozilla, the children of an itemset
        // element are cloned underneath a contextcontainer which is in turn
        // contained in a nsXFormsItemElement.  Each such item element is then
        // appended as anonymous content of the itemset.
        nsXFormsUtils::ReportError(NS_LITERAL_STRING("copyError"), mElement);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsCopyElement::DocumentChanged(nsIDOMDocument* aNewDocument)
{
  if (!aNewDocument)
    return NS_OK;
  
  // tell grandparent (xf:item) that it contains a xf:copy element and
  // not a xf:value element.
  nsCOMPtr<nsIDOMNode> contextContainer;
  nsresult rv = mElement->GetParentNode(getter_AddRefs(contextContainer));
  NS_ENSURE_TRUE(contextContainer, rv);

  nsCOMPtr<nsIDOMNode> itemNode;
  rv = contextContainer->GetParentNode(getter_AddRefs(itemNode));
  NS_ENSURE_TRUE(itemNode, rv);

  nsCOMPtr<nsIXFormsItemElement> item = do_QueryInterface(itemNode);

  // It is possible that the grandparent ISN'T an xf:item, if this is the
  // original template copy element whose parent is the xf:itemset and
  // grandparent is the xf:select.  We'll ignore a copy element in that case
  // since it really isn't in play.
  if (item) {
    item->SetIsCopyItem(PR_TRUE);
  }
  return NS_OK;
}

// nsIXFormsCopyElement

NS_IMETHODIMP
nsXFormsCopyElement::GetCopyNode(nsIDOMNode **aNode)
{
  NS_ENSURE_ARG_POINTER(aNode);
  *aNode = nsnull;

  nsCOMPtr<nsIDOMNode> singleNode;
  nsCOMPtr<nsIModelElementPrivate> model;
  PRBool succeeded =
    nsXFormsUtils::GetSingleNodeBinding(mElement,
                                        getter_AddRefs(singleNode),
                                        getter_AddRefs(model));

  if (succeeded && singleNode)
    NS_ADDREF(*aNode = singleNode);

  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsCopyElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsCopyElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
