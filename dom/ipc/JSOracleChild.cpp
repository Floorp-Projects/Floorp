/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSOracleChild.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/JSValidatorChild.h"
#include "mozilla/dom/PJSValidatorChild.h"
#include "mozilla/ipc/Endpoint.h"

using namespace mozilla::dom;

static mozilla::StaticRefPtr<JSOracleChild> sOracleSingletonChild;

static mozilla::StaticAutoPtr<JSFrontendContextHolder> sJSFrontendContextHolder;

/* static */
void JSFrontendContextHolder::MaybeInit() {
  if (!sJSFrontendContextHolder) {
    sJSFrontendContextHolder = new JSFrontendContextHolder();
    ClearOnShutdown(&sJSFrontendContextHolder);
  }
}

/* static */
JS::FrontendContext* JSOracleChild::JSFrontendContext() {
  MOZ_ASSERT(sJSFrontendContextHolder);
  return sJSFrontendContextHolder->mFc;
}

JSOracleChild* JSOracleChild::GetSingleton() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sOracleSingletonChild) {
    sOracleSingletonChild = new JSOracleChild();
    ClearOnShutdown(&sOracleSingletonChild);
  }
  return sOracleSingletonChild;
}

already_AddRefed<PJSValidatorChild> JSOracleChild::AllocPJSValidatorChild() {
  return MakeAndAddRef<JSValidatorChild>();
}

void JSOracleChild::Start(Endpoint<PJSOracleChild>&& aEndpoint) {
  DebugOnly<bool> ok = std::move(aEndpoint).Bind(this);
  JSFrontendContextHolder::MaybeInit();
  MOZ_ASSERT(ok);
}
