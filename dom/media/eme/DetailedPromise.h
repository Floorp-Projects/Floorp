/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DetailedPromise_h__
#define __DetailedPromise_h__

#include "mozilla/dom/Promise.h"
#include "mozilla/Telemetry.h"
#include "EMEUtils.h"

namespace mozilla {
namespace dom {

/*
 * This is pretty horrible; bug 1160445.
 * Extend Promise to add custom DOMException messages on rejection.
 * Get rid of this once we've ironed out EME errors in the wild.
 */
class DetailedPromise : public Promise
{
public:
  static already_AddRefed<DetailedPromise>
  Create(nsIGlobalObject* aGlobal,
         ErrorResult& aRv,
         const nsACString& aName);

  static already_AddRefed<DetailedPromise>
  Create(nsIGlobalObject* aGlobal, ErrorResult& aRv,
         const nsACString& aName,
         Telemetry::HistogramID aSuccessLatencyProbe,
         Telemetry::HistogramID aFailureLatencyProbe);

  template <typename T>
  void MaybeResolve(T&& aArg)
  {
    EME_LOG("%s promise resolved", mName.get());
    MaybeReportTelemetry(eStatus::kSucceeded);
    Promise::MaybeResolve(std::forward<T>(aArg));
  }

  void MaybeReject(nsresult aArg) = delete;
  void MaybeReject(nsresult aArg, const nsACString& aReason);

  void MaybeReject(ErrorResult& aArg) = delete;
  void MaybeReject(ErrorResult&, const nsACString& aReason);

private:
  explicit DetailedPromise(nsIGlobalObject* aGlobal,
                           const nsACString& aName);

  explicit DetailedPromise(nsIGlobalObject* aGlobal,
                           const nsACString& aName,
                           Telemetry::HistogramID aSuccessLatencyProbe,
                           Telemetry::HistogramID aFailureLatencyProbe);
  virtual ~DetailedPromise();

  enum eStatus { kSucceeded, kFailed };
  void MaybeReportTelemetry(eStatus aStatus);

  nsCString mName;
  bool mResponded;
  TimeStamp mStartTime;
  Optional<Telemetry::HistogramID> mSuccessLatencyProbe;
  Optional<Telemetry::HistogramID> mFailureLatencyProbe;
};

} // namespace dom
} // namespace mozilla

#endif // __DetailedPromise_h__
