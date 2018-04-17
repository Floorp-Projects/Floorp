/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PlayPromise.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"

extern mozilla::LazyLogModule gMediaElementLog;

#define PLAY_PROMISE_LOG(msg, ...)                                             \
  MOZ_LOG(gMediaElementLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

PlayPromise::PlayPromise(nsIGlobalObject* aGlobal)
  : Promise(aGlobal)
{
}

PlayPromise::~PlayPromise()
{
  if (!mFulfilled && PromiseObj()) {
    MaybeReject(NS_ERROR_DOM_ABORT_ERR);
  }
}

/* static */
already_AddRefed<PlayPromise>
PlayPromise::Create(nsIGlobalObject* aGlobal, ErrorResult& aRv)
{
  RefPtr<PlayPromise> promise = new PlayPromise(aGlobal);
  promise->CreateWrapper(nullptr, aRv);
  return aRv.Failed() ? nullptr : promise.forget();
}

void
PlayPromise::MaybeResolveWithUndefined()
{
  if (mFulfilled) {
    return;
  }
  mFulfilled = true;
  PLAY_PROMISE_LOG("PlayPromise %p resolved with undefined", this);
  auto reason = Telemetry::LABELS_MEDIA_PLAY_PROMISE_RESOLUTION::Resolved;
  Telemetry::AccumulateCategorical(reason);
  Promise::MaybeResolveWithUndefined();
}

using PlayLabel = Telemetry::LABELS_MEDIA_PLAY_PROMISE_RESOLUTION;

struct PlayPromiseTelemetryResult
{
  nsresult mValue;
  PlayLabel mLabel;
  const char* mName;
};

static const PlayPromiseTelemetryResult sPlayPromiseTelemetryResults[] = {
  {
    NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR,
    PlayLabel::NotAllowedErr,
    "NotAllowedErr",
  },
  {
    NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR,
    PlayLabel::SrcNotSupportedErr,
    "SrcNotSupportedErr",
  },
  {
    NS_ERROR_DOM_MEDIA_ABORT_ERR,
    PlayLabel::PauseAbortErr,
    "PauseAbortErr",
  },
  {
    NS_ERROR_DOM_ABORT_ERR,
    PlayLabel::AbortErr,
    "AbortErr",
  },
};

static const PlayPromiseTelemetryResult*
FindPlayPromiseTelemetryResult(nsresult aReason)
{
  for (const auto& p : sPlayPromiseTelemetryResults) {
    if (p.mValue == aReason) {
      return &p;
    }
  }
  return nullptr;
}

static PlayLabel
ToPlayResultLabel(nsresult aReason)
{
  auto p = FindPlayPromiseTelemetryResult(aReason);
  return p ? p->mLabel : PlayLabel::UnknownErr;
}

static const char*
ToPlayResultStr(nsresult aReason)
{
  auto p = FindPlayPromiseTelemetryResult(aReason);
  return p ? p->mName : "UnknownErr";
}

void
PlayPromise::MaybeReject(nsresult aReason)
{
  if (mFulfilled) {
    return;
  }
  mFulfilled = true;
  PLAY_PROMISE_LOG("PlayPromise %p rejected with 0x%x (%s)",
                   this,
                   static_cast<uint32_t>(aReason),
                   ToPlayResultStr(aReason));
  Telemetry::AccumulateCategorical(ToPlayResultLabel(aReason));
  Promise::MaybeReject(aReason);
}

} // namespace dom
} // namespace mozilla
