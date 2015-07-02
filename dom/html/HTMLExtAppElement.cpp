/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLExtAppElement.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/dom/HTMLExtAppElementBinding.h"
#include "mozilla/dom/ExternalAppEvent.h"

#include "nsGkAtoms.h"
#include "nsIAtom.h"
#include "nsIPermissionManager.h"
#include "nsStyleConsts.h"
#include "nsRuleData.h"

nsGenericHTMLElement*
NS_NewHTMLExtAppElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                        mozilla::dom::FromParser aFromParser) {
  // Return HTMLUnknownElement if the document doesn't have the 'external-app' permission.
  nsCOMPtr<nsIPermissionManager> permissionManager =
    mozilla::services::GetPermissionManager();
  nsRefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  nsIPrincipal* principal = ni->GetDocument()->NodePrincipal();

  already_AddRefed<mozilla::dom::NodeInfo> aarni = ni.forget();

  if (!permissionManager) {
    return new HTMLUnknownElement(aarni);
  }

  uint32_t perm = nsIPermissionManager::UNKNOWN_ACTION;
  permissionManager->TestExactPermissionFromPrincipal(principal,
                                                      "external-app",
                                                      &perm);
  if (perm != nsIPermissionManager::ALLOW_ACTION) {
    return new HTMLUnknownElement(aarni);
  }

  return new HTMLExtAppElement(aarni);
}

namespace mozilla {
namespace dom {

HTMLExtAppElement::HTMLExtAppElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  mCustomEventDispatch = new nsCustomEventDispatch(this);
  mCustomPropertyBag = new nsCustomPropertyBag();

  nsCOMPtr<nsIExternalApplication> app =
    do_CreateInstance(NS_EXTERNALAPP_CONTRACTID);
  if (app) {
    nsresult rv = app->Init(OwnerDoc()->GetInnerWindow(), mCustomPropertyBag, mCustomEventDispatch);
    if (NS_SUCCEEDED(rv)) {
      mApp = app;
    }
  }
}

HTMLExtAppElement::~HTMLExtAppElement()
{
  mCustomEventDispatch->ClearEventTarget();
}

void
HTMLExtAppElement::GetCustomProperty(const nsAString& aName, nsString& aReturn)
{
  mCustomPropertyBag->GetCustomProperty(aName, aReturn);
}

void
HTMLExtAppElement::PostMessage(const nsAString& aMessage, ErrorResult& aRv)
{
  if (!mApp) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aRv = mApp->PostMessage(aMessage);
}

NS_IMPL_ADDREF_INHERITED(HTMLExtAppElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLExtAppElement, Element)

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLExtAppElement)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLExtAppElement,
                                                nsGenericHTMLElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLExtAppElement,
                                                  nsGenericHTMLElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// QueryInterface implementation for HTMLExtAppElement
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLExtAppElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLExtAppElement)

JSObject*
HTMLExtAppElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLExtAppElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

NS_IMPL_ISUPPORTS(nsCustomPropertyBag, nsICustomPropertyBag)

nsCustomPropertyBag::nsCustomPropertyBag()
{
}

nsCustomPropertyBag::~nsCustomPropertyBag()
{
}

NS_IMETHODIMP
nsCustomPropertyBag::SetProperty(const nsAString& aName, const nsAString& aValue)
{
  mBag.Put(nsString(aName), new nsString(aValue));
  return NS_OK;
}

NS_IMETHODIMP
nsCustomPropertyBag::RemoveProperty(const nsAString& aName)
{
  mBag.Remove(nsString(aName));
  return NS_OK;
}

void
nsCustomPropertyBag::GetCustomProperty(const nsAString& aName, nsString& aReturn)
{
  nsString* value;
  if (!mBag.Get(nsString(aName), &value)) {
    aReturn.Truncate();
    return;
  }

  MOZ_ASSERT(value);
  aReturn.Assign(*value);
}

NS_IMPL_ISUPPORTS(nsCustomEventDispatch, nsICustomEventDispatch)

nsCustomEventDispatch::nsCustomEventDispatch(mozilla::dom::EventTarget* aEventTarget)
  : mEventTarget(aEventTarget)
{
  MOZ_ASSERT(mEventTarget);
}

void
nsCustomEventDispatch::ClearEventTarget()
{
  mEventTarget = nullptr;
}

nsCustomEventDispatch::~nsCustomEventDispatch()
{
}

NS_IMETHODIMP
nsCustomEventDispatch::DispatchExternalEvent(const nsAString& value)
{
  if (!mEventTarget) {
    return NS_OK;
  }

  mozilla::dom::ExternalAppEventInit init;
  init.mData = value;

  nsRefPtr<mozilla::dom::ExternalAppEvent> event =
    mozilla::dom::ExternalAppEvent::Constructor(mEventTarget,
                                                NS_LITERAL_STRING("externalappevent"),
                                                init);

  bool defaultActionEnabled;
  return mEventTarget->DispatchEvent(event, &defaultActionEnabled);
}
