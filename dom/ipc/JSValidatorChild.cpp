/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSValidatorChild.h"
#include "js/JSON.h"
#include "mozilla/dom/JSOracleChild.h"

#include "mozilla/ipc/Endpoint.h"

#include "js/experimental/JSStencil.h"
#include "js/SourceText.h"
#include "js/Exception.h"
#include "js/GlobalObject.h"
#include "js/CompileOptions.h"
#include "js/RealmOptions.h"

using namespace mozilla::dom;

mozilla::ipc::IPCResult JSValidatorChild::RecvIsOpaqueResponseAllowed(
    IsOpaqueResponseAllowedResolver&& aResolver) {
  mResolver.emplace(aResolver);

  return IPC_OK();
}

mozilla::ipc::IPCResult JSValidatorChild::RecvOnDataAvailable(Shmem&& aData) {
  if (!mResolver) {
    MOZ_ASSERT(!CanSend());
    return IPC_OK();
  }

  if (!mSourceBytes.Append(Span(aData.get<char>(), aData.Size<char>()),
                           mozilla::fallible)) {
    // To prevent an attacker from flood the validation process,
    // we don't validate here.
    Resolve(ValidatorResult::Failure);
  }
  DeallocShmem(aData);

  return IPC_OK();
}

mozilla::ipc::IPCResult JSValidatorChild::RecvOnStopRequest(
    const nsresult& aReason) {
  if (!mResolver) {
    return IPC_OK();
  }

  if (NS_FAILED(aReason)) {
    Resolve(ValidatorResult::Failure);
  } else {
    Resolve(ShouldAllowJS());
  }

  return IPC_OK();
}

void JSValidatorChild::ActorDestroy(ActorDestroyReason aReason) {
  if (mResolver) {
    Resolve(ValidatorResult::Failure);
  }
};

void JSValidatorChild::Resolve(ValidatorResult aResult) {
  MOZ_ASSERT(mResolver);
  Maybe<Shmem> data = Nothing();
  if (aResult == ValidatorResult::JavaScript && !mSourceBytes.IsEmpty()) {
    Shmem sharedData;
    nsresult rv =
        JSValidatorUtils::CopyCStringToShmem(this, mSourceBytes, sharedData);
    if (NS_SUCCEEDED(rv)) {
      data = Some(std::move(sharedData));
    }
  }

  mResolver.ref()(Tuple<mozilla::Maybe<Shmem>&&, const ValidatorResult&>(
      std::move(data), aResult));
  mResolver.reset();
}

JSValidatorChild::ValidatorResult JSValidatorChild::ShouldAllowJS() const {
  // The empty document parses as JavaScript, so for clarity we have a condition
  // separately for that.
  if (mSourceBytes.IsEmpty()) {
    return ValidatorResult::JavaScript;
  }

  JSContext* cx = JSOracleChild::JSContext();
  if (!cx) {
    return ValidatorResult::Failure;
  }

  JSObject* global = JSOracleChild::JSObject();
  if (!global) {
    return ValidatorResult::Failure;
  }

  JS::SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, mSourceBytes.BeginReading(), mSourceBytes.Length(),
                   JS::SourceOwnership::Borrowed)) {
    JS_ClearPendingException(cx);
    return ValidatorResult::Failure;
  }

  JSAutoRealm ar(cx, global);

  // Parse to JavaScript
  RefPtr<JS::Stencil> stencil =
      CompileGlobalScriptToStencil(cx, JS::CompileOptions(cx), srcBuf);

  if (!stencil) {
    JS_ClearPendingException(cx);
    return ValidatorResult::Other;
  }

  // The stencil parsing lacks the JSON support, so we have to do the
  // JSON parsing separately.
  if (StringBeginsWith(NS_ConvertUTF8toUTF16(mSourceBytes), u"{"_ns)) {
    return ValidatorResult::JSON;
  }

  return ValidatorResult::JavaScript;
}
