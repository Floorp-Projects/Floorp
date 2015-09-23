/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DetailedPromise.h"
#include "mozilla/dom/DOMException.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace dom {

DetailedPromise::DetailedPromise(nsIGlobalObject* aGlobal,
                                 const nsACString& aName)
  : Promise(aGlobal)
  , mName(aName)
  , mResponded(false)
  , mStartTime(TimeStamp::Now())
{
}

DetailedPromise::DetailedPromise(nsIGlobalObject* aGlobal,
                                 const nsACString& aName,
                                 Telemetry::ID aSuccessLatencyProbe,
                                 Telemetry::ID aFailureLatencyProbe)
  : DetailedPromise(aGlobal, aName)
{
  mSuccessLatencyProbe.Construct(aSuccessLatencyProbe);
  mFailureLatencyProbe.Construct(aFailureLatencyProbe);
}

DetailedPromise::~DetailedPromise()
{
  MOZ_ASSERT(mResponded == IsPending());
  MaybeReportTelemetry(Failed);
}

void
DetailedPromise::MaybeReject(nsresult aArg, const nsACString& aReason)
{
  nsPrintfCString msg("%s promise rejected 0x%x '%s'", mName.get(), aArg,
                      PromiseFlatCString(aReason).get());
  EME_LOG(msg.get());

  MaybeReportTelemetry(Failed);

  LogToBrowserConsole(NS_ConvertUTF8toUTF16(msg));

  nsRefPtr<DOMException> exception =
    DOMException::Create(aArg, aReason);
  Promise::MaybeRejectBrokenly(exception);
}

void
DetailedPromise::MaybeReject(ErrorResult&, const nsACString& aReason)
{
  NS_NOTREACHED("nsresult expected in MaybeReject()");
}

/* static */ already_AddRefed<DetailedPromise>
DetailedPromise::Create(nsIGlobalObject* aGlobal,
                        ErrorResult& aRv,
                        const nsACString& aName)
{
  nsRefPtr<DetailedPromise> promise = new DetailedPromise(aGlobal, aName);
  promise->CreateWrapper(nullptr, aRv);
  return aRv.Failed() ? nullptr : promise.forget();
}

/* static */ already_AddRefed<DetailedPromise>
DetailedPromise::Create(nsIGlobalObject* aGlobal,
                        ErrorResult& aRv,
                        const nsACString& aName,
                        Telemetry::ID aSuccessLatencyProbe,
                        Telemetry::ID aFailureLatencyProbe)
{
  nsRefPtr<DetailedPromise> promise = new DetailedPromise(aGlobal, aName, aSuccessLatencyProbe, aFailureLatencyProbe);
  promise->CreateWrapper(nullptr, aRv);
  return aRv.Failed() ? nullptr : promise.forget();
}

void
DetailedPromise::MaybeReportTelemetry(Status aStatus)
{
  if (mResponded) {
    return;
  }
  mResponded = true;
  if (!mSuccessLatencyProbe.WasPassed() || !mFailureLatencyProbe.WasPassed()) {
    return;
  }
  uint32_t latency = (TimeStamp::Now() - mStartTime).ToMilliseconds();
  EME_LOG("%s %s latency %ums reported via telemetry", mName.get(),
          ((aStatus == Succeeded) ? "succcess" : "failure"), latency);
  Telemetry::ID tid = (aStatus == Succeeded) ? mSuccessLatencyProbe.Value()
                                             : mFailureLatencyProbe.Value();
  Telemetry::Accumulate(tid, latency);
}

} // namespace dom
} // namespace mozilla
