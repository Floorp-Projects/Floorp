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

#include "nsXFormsInstanceElement.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOM3Node.h"
#include "nsMemory.h"
#include "nsXFormsAtoms.h"
#include "nsString.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIXTFGenericElementWrapper.h"
#include "nsXFormsUtils.h"

NS_IMPL_ISUPPORTS_INHERITED3(nsXFormsInstanceElement,
                             nsXFormsStubElement,
                             nsIInstanceElementPrivate,
                             nsIDOMLoadListener,
                             nsIDOMEventListener)

nsXFormsInstanceElement::nsXFormsInstanceElement()
  : mElement(nsnull)
{
}

NS_IMETHODIMP
nsXFormsInstanceElement::OnDestroyed()
{
  nsCOMPtr<nsIDOMEventReceiver> rec = do_QueryInterface(mDocument);
  if (rec)
    rec->RemoveEventListenerByIID(this, NS_GET_IID(nsIDOMLoadListener));

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::AttributeSet(nsIAtom *aName,
                                      const nsAString &aNewValue)
{
  if (aName == nsXFormsAtoms::src) {
    // Note that this will fail if encountered during document construction,
    // because we won't be in the document yet, so CreateInstanceDocument
    // won't find a document to work with.  That's ok, we'll fix things after
    // our children are appended and we're in the document (DoneAddingChildren)

    LoadExternalInstance(aNewValue);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::AttributeRemoved(nsIAtom *aName)
{
  if (aName == nsXFormsAtoms::src) {
    // We no longer have an external instance to use.
    // Reset our instance document to whatever inline content we have.
    return CloneInlineInstance();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::DoneAddingChildren()
{
  // By the time this is called, we should be inserted in the document
  // and have all of our child elements, so this is our first opportunity
  // to create the instance document.

  nsAutoString src;
  mElement->GetAttribute(NS_LITERAL_STRING("src"), src);

  if (src.IsEmpty()) {
    // If we don't have a linked external instance, use our inline data.
    CloneInlineInstance();
  } else {
    LoadExternalInstance(src);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::OnCreated(nsIXTFGenericElementWrapper *aWrapper)
{
  aWrapper->SetNotificationMask (nsIXTFElement::NOTIFY_PARENT_CHANGED |
                                 nsIXTFElement::NOTIFY_ATTRIBUTE_SET |
                                 nsIXTFElement::NOTIFY_ATTRIBUTE_REMOVED |
                                 nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  return NS_OK;
}

// nsIDOMLoadListener

NS_IMETHODIMP
nsXFormsInstanceElement::Load(nsIDOMEvent *aEvent)
{
  nsCOMPtr<nsIModelElementPrivate> model = GetModel();
  if (model)
    model->InstanceLoadFinished(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::BeforeUnload(nsIDOMEvent *aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::Unload(nsIDOMEvent *aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::Abort(nsIDOMEvent *aEvent)
{
  nsCOMPtr<nsIModelElementPrivate> model = GetModel();
  if (model)
    model->InstanceLoadFinished(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::Error(nsIDOMEvent *aEvent)
{
  nsCOMPtr<nsIModelElementPrivate> model = GetModel();
  if (model)
    model->InstanceLoadFinished(PR_FALSE);

  return NS_OK;
}

// nsIDOMEventListener

NS_IMETHODIMP
nsXFormsInstanceElement::HandleEvent(nsIDOMEvent *aEvent)
{
  return NS_OK;
}

// nsIInstanceElementPrivate

NS_IMETHODIMP
nsXFormsInstanceElement::GetDocument(nsIDOMDocument **aDocument)
{
  NS_IF_ADDREF(*aDocument = mDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsInstanceElement::SetDocument(nsIDOMDocument *aDocument)
{
  mDocument = aDocument;
  return NS_OK;
}

// private methods

nsresult
nsXFormsInstanceElement::CloneInlineInstance()
{
  // Clear out our existing instance data
  nsresult rv = CreateInstanceDocument();
  if (NS_FAILED(rv))
    return rv; // don't warn, we might just not be in the document yet

  // look for our first child element (skip over text nodes, etc.)
  nsCOMPtr<nsIDOMNode> child, temp;
  mElement->GetFirstChild(getter_AddRefs(child));
  while (child) {
    PRUint16 nodeType;
    child->GetNodeType(&nodeType);

    if (nodeType == nsIDOMNode::ELEMENT_NODE)
      break;

    temp.swap(child);
    temp->GetNextSibling(getter_AddRefs(child));
  }

  if (child) {
    nsCOMPtr<nsIDOMNode> newNode;
    rv = mDocument->ImportNode(child, PR_TRUE, getter_AddRefs(newNode));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> nodeReturn;
    rv = mDocument->AppendChild(newNode, getter_AddRefs(nodeReturn));
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "failed to append root instance node");
  }

  return rv;
}

void
nsXFormsInstanceElement::LoadExternalInstance(const nsAString &aSrc)
{
  // Clear out our existing instance data
  if (NS_FAILED(CreateInstanceDocument()))
    return;

  nsCOMPtr<nsIDOMEventReceiver> rec = do_QueryInterface(mDocument);
  rec->AddEventListenerByIID(this, NS_GET_IID(nsIDOMLoadListener));

  nsCOMPtr<nsIDOMXMLDocument> xmlDoc = do_QueryInterface(mDocument);
  NS_ASSERTION(xmlDoc, "we created a document but it's not an XMLDocument?");

  PRBool success;
  xmlDoc->Load(aSrc, &success);

  nsCOMPtr<nsIModelElementPrivate> model = GetModel();
  if (model) {
    model->InstanceLoadStarted();
    if (!success) {
      model->InstanceLoadFinished(PR_FALSE);
    }
  }
}

nsresult
nsXFormsInstanceElement::CreateInstanceDocument()
{
  nsCOMPtr<nsIDOMDocument> doc;
  nsresult rv = mElement->GetOwnerDocument(getter_AddRefs(doc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!doc) // could be we just aren't inserted yet, so don't warn
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDOMImplementation> domImpl;
  rv = doc->GetImplementation(getter_AddRefs(domImpl));
  NS_ENSURE_SUCCESS(rv, rv);

  return domImpl->CreateDocument(EmptyString(), EmptyString(), nsnull,
                                 getter_AddRefs(mDocument));
}

already_AddRefed<nsIModelElementPrivate>
nsXFormsInstanceElement::GetModel()
{
  nsCOMPtr<nsIDOMNode> parentNode;
  mElement->GetParentNode(getter_AddRefs(parentNode));

  nsIModelElementPrivate *model = nsnull;
  if (parentNode)
    CallQueryInterface(parentNode, &model);
  return model;
}

nsresult
NS_NewXFormsInstanceElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsInstanceElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
