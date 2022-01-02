/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PlayPromise.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"

extern mozilla::LazyLogModule gMediaElementLog;

#define PLAY_PROMISE_LOG(msg, ...) \
  MOZ_LOG(gMediaElementLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla::dom {

PlayPromise::PlayPromise(nsIGlobalObject* aGlobal) : Promise(aGlobal) {}

PlayPromise::~PlayPromise() {
  if (!mFulfilled && PromiseObj()) {
    MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }
}

/* static */
already_AddRefed<PlayPromise> PlayPromise::Create(nsIGlobalObject* aGlobal,
                                                  ErrorResult& aRv) {
  RefPtr<PlayPromise> promise = new PlayPromise(aGlobal);
  promise->CreateWrapper(aRv);
  return aRv.Failed() ? nullptr : promise.forget();
}

/* static */
void PlayPromise::ResolvePromisesWithUndefined(
    const PlayPromiseArr& aPromises) {
  for (const auto& promise : aPromises) {
    promise->MaybeResolveWithUndefined();
  }
}

/* static */
void PlayPromise::RejectPromises(const PlayPromiseArr& aPromises,
                                 nsresult aError) {
  for (const auto& promise : aPromises) {
    promise->MaybeReject(aError);
  }
}

void PlayPromise::MaybeResolveWithUndefined() {
  if (mFulfilled) {
    return;
  }
  mFulfilled = true;
  PLAY_PROMISE_LOG("PlayPromise %p resolved with undefined", this);
  Promise::MaybeResolveWithUndefined();
}

static const char* ToPlayResultStr(nsresult aReason) {
  switch (aReason) {
    case NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR:
      return "NotAllowedErr";
    case NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR:
      return "SrcNotSupportedErr";
    case NS_ERROR_DOM_MEDIA_ABORT_ERR:
      return "PauseAbortErr";
    case NS_ERROR_DOM_ABORT_ERR:
      return "AbortErr";
    default:
      return "UnknownErr";
  }
}

void PlayPromise::MaybeReject(nsresult aReason) {
  if (mFulfilled) {
    return;
  }
  mFulfilled = true;
  PLAY_PROMISE_LOG("PlayPromise %p rejected with 0x%x (%s)", this,
                   static_cast<uint32_t>(aReason), ToPlayResultStr(aReason));
  Promise::MaybeReject(aReason);
}

}  // namespace mozilla::dom
