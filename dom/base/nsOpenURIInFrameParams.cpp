/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOpenURIInFrameParams.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/net/ReferrerPolicy.h"

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsOpenURIInFrameParams)
  NS_INTERFACE_MAP_ENTRY(nsIOpenURIInFrameParams)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(nsOpenURIInFrameParams, mOpenerBrowser)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsOpenURIInFrameParams)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsOpenURIInFrameParams)

nsOpenURIInFrameParams::nsOpenURIInFrameParams(
    const mozilla::OriginAttributes& aOriginAttributes,
    mozilla::dom::Element* aOpener)
    : mOpenerOriginAttributes(aOriginAttributes), mOpenerBrowser(aOpener) {}

nsOpenURIInFrameParams::~nsOpenURIInFrameParams() {}

NS_IMETHODIMP
nsOpenURIInFrameParams::GetReferrerInfo(nsIReferrerInfo** aReferrerInfo) {
  NS_IF_ADDREF(*aReferrerInfo = mReferrerInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsOpenURIInFrameParams::SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
  mReferrerInfo = aReferrerInfo;
  return NS_OK;
}

NS_IMETHODIMP
nsOpenURIInFrameParams::GetIsPrivate(bool* aIsPrivate) {
  NS_ENSURE_ARG_POINTER(aIsPrivate);
  *aIsPrivate = mOpenerOriginAttributes.mPrivateBrowsingId > 0;
  return NS_OK;
}

NS_IMETHODIMP
nsOpenURIInFrameParams::GetTriggeringPrincipal(
    nsIPrincipal** aTriggeringPrincipal) {
  NS_ADDREF(*aTriggeringPrincipal = mTriggeringPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsOpenURIInFrameParams::SetTriggeringPrincipal(
    nsIPrincipal* aTriggeringPrincipal) {
  NS_ENSURE_TRUE(aTriggeringPrincipal, NS_ERROR_INVALID_ARG);
  mTriggeringPrincipal = aTriggeringPrincipal;
  return NS_OK;
}

NS_IMETHODIMP
nsOpenURIInFrameParams::GetCsp(nsIContentSecurityPolicy** aCsp) {
  NS_IF_ADDREF(*aCsp = mCsp);
  return NS_OK;
}

NS_IMETHODIMP
nsOpenURIInFrameParams::SetCsp(nsIContentSecurityPolicy* aCsp) {
  NS_ENSURE_TRUE(aCsp, NS_ERROR_INVALID_ARG);
  mCsp = aCsp;
  return NS_OK;
}

nsresult nsOpenURIInFrameParams::GetOpenerBrowser(
    mozilla::dom::Element** aOpenerBrowser) {
  RefPtr<mozilla::dom::Element> owner = mOpenerBrowser;
  owner.forget(aOpenerBrowser);
  return NS_OK;
}

NS_IMETHODIMP
nsOpenURIInFrameParams::GetOpenerOriginAttributes(
    JSContext* aCx, JS::MutableHandle<JS::Value> aValue) {
  bool ok = ToJSValue(aCx, mOpenerOriginAttributes, aValue);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
  return NS_OK;
}
