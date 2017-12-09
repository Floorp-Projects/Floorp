/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PromiseWindowProxy.h"

#include "nsGlobalWindowInner.h"
#include "nsPIDOMWindow.h"
#include "nsIWeakReference.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using namespace dom;


PromiseWindowProxy::PromiseWindowProxy(nsPIDOMWindowInner* aWindow, Promise* aPromise)
  : mPromise(aPromise)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aWindow && aPromise);
  auto* window = nsGlobalWindowInner::Cast(aWindow);
  window->GetWeakReference(getter_AddRefs(mWindow));
  window->AddPendingPromise(aPromise);
}

PromiseWindowProxy::~PromiseWindowProxy()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsPIDOMWindowInner> window = GetWindow();
  if (window && mPromise) {
    nsGlobalWindowInner::Cast(window)->RemovePendingPromise(mPromise);
  }
}

RefPtr<Promise>
PromiseWindowProxy::Get() const
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mPromise) {
    return nullptr;
  }
  RefPtr<Promise> promise(mPromise);
  return promise;
}

nsCOMPtr<nsPIDOMWindowInner>
PromiseWindowProxy::GetWindow() const
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mWindow);
  return window;
}
