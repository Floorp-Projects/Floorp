/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/LoadContext.h"
#include "mozilla/dom/ScriptSettings.h" // for AutoJSAPI
#include "nsContentUtils.h"
#include "xpcpublic.h"

bool
nsILoadContext::GetOriginAttributes(mozilla::DocShellOriginAttributes& aAttrs)
{
  mozilla::dom::AutoJSAPI jsapi;
  bool ok = jsapi.Init(xpc::PrivilegedJunkScope());
  NS_ENSURE_TRUE(ok, false);
  JS::Rooted<JS::Value> v(jsapi.cx());
  nsresult rv = GetOriginAttributes(&v);
  NS_ENSURE_SUCCESS(rv, false);
  NS_ENSURE_TRUE(v.isObject(), false);
  JS::Rooted<JSObject*> obj(jsapi.cx(), &v.toObject());

  // If we're JS-implemented, the object will be left in a different (System-Principaled)
  // scope, so we may need to enter its compartment.
  MOZ_ASSERT(nsContentUtils::IsSystemPrincipal(nsContentUtils::ObjectPrincipal(obj)));
  JSAutoCompartment ac(jsapi.cx(), obj);

  mozilla::DocShellOriginAttributes attrs;
  ok = attrs.Init(jsapi.cx(), v);
  NS_ENSURE_TRUE(ok, false);
  aAttrs = attrs;
  return true;
}
  
namespace mozilla {

NS_IMPL_ISUPPORTS(LoadContext, nsILoadContext, nsIInterfaceRequestor)

LoadContext::LoadContext(nsIPrincipal* aPrincipal,
                         nsILoadContext* aOptionalBase)
  : mTopFrameElement(nullptr)
  , mNestedFrameId(0)
  , mIsContent(true)
  , mUsePrivateBrowsing(false)
  , mUseRemoteTabs(false)
#ifdef DEBUG
  , mIsNotNull(true)
#endif
{
  PrincipalOriginAttributes poa = BasePrincipal::Cast(aPrincipal)->OriginAttributesRef();
  mOriginAttributes = DocShellOriginAttributes(poa.mAppId, poa.mInBrowser);

  if (!aOptionalBase) {
    return;
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aOptionalBase->GetIsContent(&mIsContent)));
  MOZ_ALWAYS_TRUE(
    NS_SUCCEEDED(aOptionalBase->GetUsePrivateBrowsing(&mUsePrivateBrowsing)));
  MOZ_ALWAYS_TRUE(
    NS_SUCCEEDED(aOptionalBase->GetUseRemoteTabs(&mUseRemoteTabs)));
}

//-----------------------------------------------------------------------------
// LoadContext::nsILoadContext
//-----------------------------------------------------------------------------

NS_IMETHODIMP
LoadContext::GetAssociatedWindow(nsIDOMWindow**)
{
  MOZ_ASSERT(mIsNotNull);

  // can't support this in the parent process
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LoadContext::GetTopWindow(nsIDOMWindow**)
{
  MOZ_ASSERT(mIsNotNull);

  // can't support this in the parent process
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LoadContext::GetTopFrameElement(nsIDOMElement** aElement)
{
  nsCOMPtr<nsIDOMElement> element = do_QueryReferent(mTopFrameElement);
  element.forget(aElement);
  return NS_OK;
}

NS_IMETHODIMP
LoadContext::GetNestedFrameId(uint64_t* aId)
{
  NS_ENSURE_ARG(aId);
  *aId = mNestedFrameId;
  return NS_OK;
}

NS_IMETHODIMP
LoadContext::IsAppOfType(uint32_t, bool*)
{
  MOZ_ASSERT(mIsNotNull);

  // don't expect we need this in parent (Thunderbird/SeaMonkey specific?)
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LoadContext::GetIsContent(bool* aIsContent)
{
  MOZ_ASSERT(mIsNotNull);

  NS_ENSURE_ARG_POINTER(aIsContent);

  *aIsContent = mIsContent;
  return NS_OK;
}

NS_IMETHODIMP
LoadContext::GetUsePrivateBrowsing(bool* aUsePrivateBrowsing)
{
  MOZ_ASSERT(mIsNotNull);

  NS_ENSURE_ARG_POINTER(aUsePrivateBrowsing);

  *aUsePrivateBrowsing = mUsePrivateBrowsing;
  return NS_OK;
}

NS_IMETHODIMP
LoadContext::SetUsePrivateBrowsing(bool aUsePrivateBrowsing)
{
  MOZ_ASSERT(mIsNotNull);

  // We shouldn't need this on parent...
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LoadContext::SetPrivateBrowsing(bool aUsePrivateBrowsing)
{
  MOZ_ASSERT(mIsNotNull);

  // We shouldn't need this on parent...
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LoadContext::GetUseRemoteTabs(bool* aUseRemoteTabs)
{
  MOZ_ASSERT(mIsNotNull);

  NS_ENSURE_ARG_POINTER(aUseRemoteTabs);

  *aUseRemoteTabs = mUseRemoteTabs;
  return NS_OK;
}

NS_IMETHODIMP
LoadContext::SetRemoteTabs(bool aUseRemoteTabs)
{
  MOZ_ASSERT(mIsNotNull);

  // We shouldn't need this on parent...
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LoadContext::GetIsInBrowserElement(bool* aIsInBrowserElement)
{
  MOZ_ASSERT(mIsNotNull);

  NS_ENSURE_ARG_POINTER(aIsInBrowserElement);

  *aIsInBrowserElement = mOriginAttributes.mInBrowser;
  return NS_OK;
}

NS_IMETHODIMP
LoadContext::GetAppId(uint32_t* aAppId)
{
  MOZ_ASSERT(mIsNotNull);

  NS_ENSURE_ARG_POINTER(aAppId);

  *aAppId = mOriginAttributes.mAppId;
  return NS_OK;
}

NS_IMETHODIMP
LoadContext::GetOriginAttributes(JS::MutableHandleValue aAttrs)
{
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  MOZ_ASSERT(cx);

  bool ok = ToJSValue(cx, mOriginAttributes, aAttrs);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// LoadContext::nsIInterfaceRequestor
//-----------------------------------------------------------------------------
NS_IMETHODIMP
LoadContext::GetInterface(const nsIID& aIID, void** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;

  if (aIID.Equals(NS_GET_IID(nsILoadContext))) {
    *aResult = static_cast<nsILoadContext*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

} // namespace mozilla
