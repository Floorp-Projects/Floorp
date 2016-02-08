/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_MobileConnectionCallback_h
#define mozilla_dom_mobileconnection_MobileConnectionCallback_h

#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/mobileconnection/MobileConnectionIPCSerializer.h"
#include "nsCOMPtr.h"
#include "nsIMobileConnectionService.h"

namespace mozilla {
namespace dom {
namespace mobileconnection {

/**
 * A callback object for handling asynchronous request/response. This object is
 * created when an asynchronous request is made and should be destroyed after
 * Notify*Success/Error is called.
 * The modules hold the reference of MobileConnectionCallback in OOP mode and
 * non-OOP mode are different.
 * - OOP mode: MobileConnectionRequestChild
 * - non-OOP mode: MobileConnectionService
 * The reference should be released after Notify*Success/Error is called.
 */
class MobileConnectionCallback final : public nsIMobileConnectionCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILECONNECTIONCALLBACK

  MobileConnectionCallback(nsPIDOMWindowInner* aWindow, DOMRequest* aRequest);

private:
  ~MobileConnectionCallback() {}

  nsresult
  NotifySuccess(JS::Handle<JS::Value> aResult);

  nsresult
  NotifySuccessWithString(const nsAString& aResult);

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<DOMRequest> mRequest;
};

} // namespace mobileconnection
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobileconnection_MobileConnectionCallback_h
