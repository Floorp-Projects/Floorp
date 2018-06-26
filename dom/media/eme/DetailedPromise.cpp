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
                                 Telemetry::HistogramID aSuccessLatencyProbe,
                                 Telemetry::HistogramID aFailureLatencyProbe)
  : DetailedPromise(aGlobal, aName)
{
  mSuccessLatencyProbe.Construct(aSuccessLatencyProbe);
  mFailureLatencyProbe.Construct(aFailureLatencyProbe);
}

DetailedPromise::~DetailedPromise()
{
  // It would be nice to assert that mResponded is identical to
  // GetPromiseState() == PromiseState::Rejected.  But by now we've been
  // unlinked, so don't have a reference to our actual JS Promise object
  // anymore.
  MaybeReportTelemetry(kFailed);
}

void
DetailedPromise::MaybeReject(nsresult aArg, const nsACString& aReason)
{
  nsPrintfCString msg("%s promise rejected 0x%" PRIx32 " '%s'", mName.get(),
                      static_cast<uint32_t>(aArg), PromiseFlatCString(aReason).get());
  EME_LOG("%s", msg.get());

  MaybeReportTelemetry(kFailed);

  LogToBrowserConsole(NS_ConvertUTF8toUTF16(msg));

  ErrorResult rv;
  rv.ThrowDOMException(aArg, aReason);
  Promise::MaybeReject(rv);
}

void
DetailedPromise::MaybeReject(ErrorResult&, const nsACString& aReason)
{
  MOZ_ASSERT_UNREACHABLE("nsresult expected in MaybeReject()");
}

/* static */ already_AddRefed<DetailedPromise>
DetailedPromise::Create(nsIGlobalObject* aGlobal,
                        ErrorResult& aRv,
                        const nsACString& aName)
{
  RefPtr<DetailedPromise> promise = new DetailedPromise(aGlobal, aName);
  promise->CreateWrapper(nullptr, aRv);
  return aRv.Failed() ? nullptr : promise.forget();
}

/* static */ already_AddRefed<DetailedPromise>
DetailedPromise::Create(nsIGlobalObject* aGlobal,
                        ErrorResult& aRv,
                        const nsACString& aName,
                        Telemetry::HistogramID aSuccessLatencyProbe,
                        Telemetry::HistogramID aFailureLatencyProbe)
{
  RefPtr<DetailedPromise> promise = new DetailedPromise(aGlobal, aName, aSuccessLatencyProbe, aFailureLatencyProbe);
  promise->CreateWrapper(nullptr, aRv);
  return aRv.Failed() ? nullptr : promise.forget();
}

void
DetailedPromise::MaybeReportTelemetry(eStatus aStatus)
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
          ((aStatus == kSucceeded) ? "succcess" : "failure"), latency);
  Telemetry::HistogramID tid = (aStatus == kSucceeded) ? mSuccessLatencyProbe.Value()
                                                      : mFailureLatencyProbe.Value();
  Telemetry::Accumulate(tid, latency);
}

} // namespace dom
} // namespace mozilla
