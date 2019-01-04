/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSWindowActorService.h"

#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace dom {
struct WindowActorOptions;
namespace {
StaticRefPtr<JSWindowActorService> gJSWindowActorService;
}

JSWindowActorService::JSWindowActorService() {
  MOZ_ASSERT(XRE_IsParentProcess());
}

JSWindowActorService::~JSWindowActorService() {
  MOZ_ASSERT(XRE_IsParentProcess());
}

/* static */ already_AddRefed<JSWindowActorService>
JSWindowActorService::GetSingleton() {
  if (!gJSWindowActorService) {
    gJSWindowActorService = new JSWindowActorService();
    ClearOnShutdown(&gJSWindowActorService);
  }

  RefPtr<JSWindowActorService> service = gJSWindowActorService.get();
  return service.forget();
}

void JSWindowActorService::RegisterWindowActor(
    const nsAString& aName, const WindowActorOptions& aOptions,
    ErrorResult& aRv) {
  MOZ_ASSERT(XRE_IsParentProcess());

  auto entry = mDescriptors.LookupForAdd(aName);
  if (entry) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  entry.OrInsert([&] { return &aOptions; });
}

}  // namespace dom
}  // namespace mozilla