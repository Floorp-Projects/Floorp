/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/LoadContext.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptSettings.h" // for AutoJSAPI
#include "nsContentUtils.h"
#include "xpcpublic.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(LoadContext, nsILoadContext, nsIInterfaceRequestor)

LoadContext::LoadContext(nsIPrincipal* aPrincipal,
                         nsILoadContext* aOptionalBase)
  : mTopFrameElement(nullptr)
  , mNestedFrameId(0)
  , mIsContent(true)
  , mUseRemoteTabs(false)
  , mUseTrackingProtection(false)
#ifdef DEBUG
  , mIsNotNull(true)
#endif
{
  mOriginAttributes = aPrincipal->OriginAttributesRef();
  if (!aOptionalBase) {
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(aOptionalBase->GetIsContent(&mIsContent));
  MOZ_ALWAYS_SUCCEEDS(aOptionalBase->GetUseRemoteTabs(&mUseRemoteTabs));
  MOZ_ALWAYS_SUCCEEDS(aOptionalBase->GetUseTrackingProtection(&mUseTrackingProtection));
}

//-----------------------------------------------------------------------------
// LoadContext::nsILoadContext
//-----------------------------------------------------------------------------

NS_IMETHODIMP
LoadContext::GetAssociatedWindow(mozIDOMWindowProxy**)
{
  MOZ_ASSERT(mIsNotNull);

  // can't support this in the parent process
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LoadContext::GetTopWindow(mozIDOMWindowProxy**)
{
  MOZ_ASSERT(mIsNotNull);

  // can't support this in the parent process
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LoadContext::GetTopFrameElement(dom::Element** aElement)
{
  nsCOMPtr<dom::Element> element = do_QueryReferent(mTopFrameElement);
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

  *aUsePrivateBrowsing = mOriginAttributes.mPrivateBrowsingId > 0;
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
LoadContext::GetIsInIsolatedMozBrowserElement(bool* aIsInIsolatedMozBrowserElement)
{
  MOZ_ASSERT(mIsNotNull);

  NS_ENSURE_ARG_POINTER(aIsInIsolatedMozBrowserElement);

  *aIsInIsolatedMozBrowserElement = mOriginAttributes.mInIsolatedMozBrowser;
  return NS_OK;
}

NS_IMETHODIMP
LoadContext::GetScriptableOriginAttributes(JS::MutableHandleValue aAttrs)
{
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  MOZ_ASSERT(cx);

  bool ok = ToJSValue(cx, mOriginAttributes, aAttrs);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP_(void)
LoadContext::GetOriginAttributes(mozilla::OriginAttributes& aAttrs)
{
  aAttrs = mOriginAttributes;
}

NS_IMETHODIMP
LoadContext::GetUseTrackingProtection(bool* aUseTrackingProtection)
{
  MOZ_ASSERT(mIsNotNull);

  NS_ENSURE_ARG_POINTER(aUseTrackingProtection);

  *aUseTrackingProtection = mUseTrackingProtection;
  return NS_OK;
}

NS_IMETHODIMP
LoadContext::SetUseTrackingProtection(bool aUseTrackingProtection)
{
  MOZ_ASSERT_UNREACHABLE("Should only be set through nsDocShell");

  return NS_ERROR_UNEXPECTED;
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

static nsresult
CreateTestInstance(bool aPrivate, nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  // Shamelessly modified from NS_GENERIC_FACTORY_CONSTRUCTOR
  *aResult = nullptr;

  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  OriginAttributes oa;
  oa.mPrivateBrowsingId = aPrivate ? 1 : 0;

  RefPtr<LoadContext> lc = new LoadContext(oa);

  return lc->QueryInterface(aIID, aResult);
}

nsresult
CreateTestLoadContext(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  return CreateTestInstance(false, aOuter, aIID, aResult);
}

nsresult
CreatePrivateTestLoadContext(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  return CreateTestInstance(true, aOuter, aIID, aResult);
}

} // namespace mozilla
