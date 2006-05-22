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
 * Olli Pettay.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (original author)
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

#include "nsXFormsActionElement.h"
#include "nsIXFormsModelElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMElement.h"
#include "nsIXTFBindableElementWrapper.h"

#define DEFERRED_REBUILD     0x01
#define DEFERRED_RECALCULATE 0x02
#define DEFERRED_REVALIDATE  0x04
#define DEFERRED_REFRESH     0x08

nsXFormsActionElement::nsXFormsActionElement() : mElement(nsnull)
{
}

NS_IMPL_ADDREF_INHERITED(nsXFormsActionElement, nsXFormsBindableStub)
NS_IMPL_RELEASE_INHERITED(nsXFormsActionElement, nsXFormsBindableStub)

NS_INTERFACE_MAP_BEGIN(nsXFormsActionElement)
  NS_INTERFACE_MAP_ENTRY(nsIXFormsActionModuleElement)
  NS_INTERFACE_MAP_ENTRY(nsIXFormsActionElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END_INHERITING(nsXFormsBindableStub)

NS_IMETHODIMP
nsXFormsActionElement::OnCreated(nsIXTFBindableElementWrapper* aWrapper)
{
  nsresult rv = nsXFormsBindableStub::OnCreated(aWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.
  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));
  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsActionElement::OnDestroyed() {
  mParentAction = nsnull;
  mElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsActionElement::HandleEvent(nsIDOMEvent* aEvent)
{
  return nsXFormsUtils::EventHandlingAllowed(aEvent, mElement) ?
           HandleAction(aEvent, nsnull) : NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator) DoDeferredActions(nsISupports * aModel, 
                                                      PRUint32 aDeferred,
                                                      void * data)
{
  if (aModel && aDeferred) {
    nsCOMPtr<nsIModelElementPrivate> model =
      NS_STATIC_CAST(nsIModelElementPrivate*, aModel);
    if (aDeferred & DEFERRED_REBUILD)
      model->RequestRebuild();
    if (aDeferred & DEFERRED_RECALCULATE)
      model->RequestRecalculate();
    if (aDeferred & DEFERRED_REVALIDATE)
      model->RequestRevalidate();
    if (aDeferred & DEFERRED_REFRESH)
      model->RequestRefresh();
  }
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsXFormsActionElement::HandleAction(nsIDOMEvent* aEvent,
                                    nsIXFormsActionElement *aParentAction)
{
  if (!mElement) {
    return NS_OK;
  }

  if (!mDeferredUpdates.IsInitialized()) {
    NS_ENSURE_TRUE(mDeferredUpdates.Init(), NS_ERROR_OUT_OF_MEMORY);
  } else {
    mDeferredUpdates.Clear();
  }

  mParentAction = aParentAction;
  nsCOMPtr<nsIDOMNodeList> childNodes;
  mElement->GetChildNodes(getter_AddRefs(childNodes));
  if (!childNodes) {
    return NS_OK;
  }

  PRUint32 count;
  childNodes->GetLength(&count);
  nsCOMPtr<nsIXFormsActionModuleElement> actionChild;
  nsCOMPtr<nsIDOMEvent> event(aEvent);
  for (PRUint32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMNode> child;
    childNodes->Item(i, getter_AddRefs(child));
    actionChild = do_QueryInterface(child);
    if (actionChild) {
      actionChild->HandleAction(event, this);
    }
  }
  if (!aParentAction) { //Otherwise parent will handle deferred updates
    mDeferredUpdates.EnumerateRead(DoDeferredActions, nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsActionElement::SetRebuild(nsIModelElementPrivate* aModel,
                                  PRBool aEnable)
{
  if (mParentAction) {
    return mParentAction->SetRebuild(aModel, aEnable);
  }

  PRUint32 deferred = 0;
  mDeferredUpdates.Get(aModel, &deferred);
  if (aEnable) {
    deferred |= DEFERRED_REBUILD;
  } else {
    deferred &= ~DEFERRED_REBUILD;
  }
  mDeferredUpdates.Put(aModel, deferred);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsActionElement::SetRecalculate(nsIModelElementPrivate* aModel,
                                      PRBool aEnable)
{
  if (mParentAction) {
    return mParentAction->SetRecalculate(aModel, aEnable);
  }

  PRUint32 deferred = 0;
  mDeferredUpdates.Get(aModel, &deferred);
  if (aEnable) {
    deferred |= DEFERRED_RECALCULATE;
  } else {
    deferred &= ~DEFERRED_RECALCULATE;
  }
  mDeferredUpdates.Put(aModel, deferred);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsActionElement::SetRevalidate(nsIModelElementPrivate* aModel,
                                     PRBool aEnable)
{
  if (mParentAction) {
    return mParentAction->SetRevalidate(aModel, aEnable);
  }

  PRUint32 deferred = 0;
  mDeferredUpdates.Get(aModel, &deferred);
  if (aEnable) {
    deferred |= DEFERRED_REVALIDATE;
  } else {
    deferred &= ~DEFERRED_REVALIDATE;
  }
  mDeferredUpdates.Put(aModel, deferred);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsActionElement::SetRefresh(nsIModelElementPrivate* aModel,
                                  PRBool aEnable)
{
  if (mParentAction) {
    return mParentAction->SetRefresh(aModel, aEnable);
  }

  PRUint32 deferred = 0;
  mDeferredUpdates.Get(aModel, &deferred);
  if (aEnable) {
    deferred |= DEFERRED_REFRESH;
  } else {
    deferred &= ~DEFERRED_REFRESH;
  }
  mDeferredUpdates.Put(aModel, deferred);
  return NS_OK;
}

NS_HIDDEN_(nsresult)
NS_NewXFormsActionElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsActionElement();
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);
  return NS_OK;
}

