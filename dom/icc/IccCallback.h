/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_icc_IccCallback_h
#define mozilla_dom_icc_IccCallback_h

#include "nsCOMPtr.h"
#include "nsIIccService.h"

namespace mozilla {
namespace dom {

class DOMRequest;
class Promise;

namespace icc {

/**
 * A callback object for handling asynchronous request/response. This object is
 * created when an asynchronous request is made and should be destroyed after
 * Notify*Success/Error is called.
 * The modules hold the reference of IccCallback in OOP mode and non-OOP mode
 * are different.
 * - OOP mode: IccRequestChild
 * - non-OOP mode: IccService
 * The reference should be released after Notify*Success/Error is called.
 */
class IccCallback final : public nsIIccCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIICCCALLBACK

  // TODO: Bug 1125018 - Simplify The Result of GetCardLock and
  // getCardLockRetryCount in MozIcc.webidl without a wrapper object.
  IccCallback(nsPIDOMWindow* aWindow, DOMRequest* aRequest,
              bool aIsCardLockEnabled = false);
  IccCallback(nsPIDOMWindow* aWindow, Promise* aPromise);

private:
  ~IccCallback() {}

  nsresult
  NotifySuccess(JS::Handle<JS::Value> aResult);

  // TODO: Bug 1125018 - Simplify The Result of GetCardLock and
  // getCardLockRetryCount in MozIcc.webidl without a wrapper object.
  nsresult
  NotifyGetCardLockEnabled(bool aResult);

  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<DOMRequest> mRequest;
  nsRefPtr<Promise> mPromise;
  // TODO: Bug 1125018 - Simplify The Result of GetCardLock and
  // getCardLockRetryCount in MozIcc.webidl without a wrapper object.
  bool mIsCardLockEnabled;
};

} // namespace icc
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_icc_IccCallback_h
