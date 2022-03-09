/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DetailedPromise.h"

#include "VideoUtils.h"
#include "mozilla/dom/DOMException.h"
#include "nsPrintfCString.h"

namespace mozilla::dom {

DetailedPromise::DetailedPromise(nsIGlobalObject* aGlobal,
                                 const nsACString& aName)
    : Promise(aGlobal),
      mName(aName),
      mResponded(false),
      mStartTime(TimeStamp::Now()) {}

DetailedPromise::DetailedPromise(nsIGlobalObject* aGlobal,
                                 const nsACString& aName,
                                 Telemetry::HistogramID aSuccessLatencyProbe,
                                 Telemetry::HistogramID aFailureLatencyProbe)
    : DetailedPromise(aGlobal, aName) {
  mSuccessLatencyProbe.Construct(aSuccessLatencyProbe);
  mFailureLatencyProbe.Construct(aFailureLatencyProbe);
}

DetailedPromise::~DetailedPromise() {
  // It would be nice to assert that mResponded is identical to
  // GetPromiseState() == PromiseState::Rejected.  But by now we've been
  // unlinked, so don't have a reference to our actual JS Promise object
  // anymore.
  MaybeReportTelemetry(kFailed);
}

void DetailedPromise::LogRejectionReason(uint32_t aErrorCode,
                                         const nsACString& aReason) {
  nsPrintfCString msg("%s promise rejected 0x%" PRIx32 " '%s'", mName.get(),
                      aErrorCode, PromiseFlatCString(aReason).get());
  EME_LOG("%s", msg.get());

  MaybeReportTelemetry(kFailed);

  LogToBrowserConsole(NS_ConvertUTF8toUTF16(msg));
}

void DetailedPromise::MaybeReject(nsresult aArg, const nsACString& aReason) {
  LogRejectionReason(static_cast<uint32_t>(aArg), aReason);

  Promise::MaybeRejectWithDOMException(aArg, aReason);
}

void DetailedPromise::MaybeReject(ErrorResult&& aArg,
                                  const nsACString& aReason) {
  LogRejectionReason(aArg.ErrorCodeAsInt(), aReason);
  Promise::MaybeReject(std::move(aArg));
}

/* static */
already_AddRefed<DetailedPromise> DetailedPromise::Create(
    nsIGlobalObject* aGlobal, ErrorResult& aRv, const nsACString& aName) {
  RefPtr<DetailedPromise> promise = new DetailedPromise(aGlobal, aName);
  promise->CreateWrapper(aRv);
  return aRv.Failed() ? nullptr : promise.forget();
}

void DetailedPromise::MaybeReportTelemetry(eStatus aStatus) {
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
  Telemetry::HistogramID tid = (aStatus == kSucceeded)
                                   ? mSuccessLatencyProbe.Value()
                                   : mFailureLatencyProbe.Value();
  Telemetry::Accumulate(tid, latency);
}

}  // namespace mozilla::dom
