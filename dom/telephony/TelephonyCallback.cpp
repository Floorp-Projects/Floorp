/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyCallback.h"

#include "mozilla/dom/Promise.h"
#include "nsJSUtils.h"

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;

NS_IMPL_ISUPPORTS(TelephonyCallback, nsITelephonyCallback)

TelephonyCallback::TelephonyCallback(Promise* aPromise)
  : mPromise(aPromise)
{
}

// nsITelephonyCallback

NS_IMETHODIMP
TelephonyCallback::NotifySuccess()
{
  mPromise->MaybeResolve(JS::UndefinedHandleValue);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallback::NotifyError(const nsAString& aError)
{
  mPromise->MaybeRejectBrokenly(aError);
  return NS_OK;
}
