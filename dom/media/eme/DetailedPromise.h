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

namespace mozilla::dom {

/*
 * This is pretty horrible; bug 1160445.
 * Extend Promise to add custom DOMException messages on rejection.
 * Get rid of this once we've ironed out EME errors in the wild.
 */
class DetailedPromise : public Promise {
 public:
  static already_AddRefed<DetailedPromise> Create(nsIGlobalObject* aGlobal,
                                                  ErrorResult& aRv,
                                                  const nsACString& aName);

  template <typename T>
  void MaybeResolve(T&& aArg) {
    EME_LOG("%s promise resolved", mName.get());
    MaybeReportTelemetry(eStatus::kSucceeded);
    Promise::MaybeResolve(std::forward<T>(aArg));
  }

  void MaybeReject(nsresult aArg) = delete;
  void MaybeReject(nsresult aArg, const nsACString& aReason);

  void MaybeReject(ErrorResult&& aArg) = delete;
  void MaybeReject(ErrorResult&& aArg, const nsACString& aReason);

  // Facilities for rejecting with various spec-defined exception values.
#define DOMEXCEPTION(name, err)                                   \
  inline void MaybeRejectWith##name(const nsACString& aMessage) { \
    LogRejectionReason(static_cast<uint32_t>(err), aMessage);     \
    Promise::MaybeRejectWith##name(aMessage);                     \
  }                                                               \
  template <int N>                                                \
  void MaybeRejectWith##name(const char(&aMessage)[N]) {          \
    MaybeRejectWith##name(nsLiteralCString(aMessage));            \
  }

#include "mozilla/dom/DOMExceptionNames.h"

#undef DOMEXCEPTION

  template <ErrNum errorNumber, typename... Ts>
  void MaybeRejectWithTypeError(Ts&&... aMessageArgs) = delete;

  inline void MaybeRejectWithTypeError(const nsACString& aMessage) {
    ErrorResult res;
    res.ThrowTypeError(aMessage);
    MaybeReject(std::move(res), aMessage);
  }

  template <int N>
  void MaybeRejectWithTypeError(const char (&aMessage)[N]) {
    MaybeRejectWithTypeError(nsLiteralCString(aMessage));
  }

  template <ErrNum errorNumber, typename... Ts>
  void MaybeRejectWithRangeError(Ts&&... aMessageArgs) = delete;

  inline void MaybeRejectWithRangeError(const nsACString& aMessage) {
    ErrorResult res;
    res.ThrowRangeError(aMessage);
    MaybeReject(std::move(res), aMessage);
  }

  template <int N>
  void MaybeRejectWithRangeError(const char (&aMessage)[N]) {
    MaybeRejectWithRangeError(nsLiteralCString(aMessage));
  }

 private:
  explicit DetailedPromise(nsIGlobalObject* aGlobal, const nsACString& aName);

  explicit DetailedPromise(nsIGlobalObject* aGlobal, const nsACString& aName,
                           Telemetry::HistogramID aSuccessLatencyProbe,
                           Telemetry::HistogramID aFailureLatencyProbe);
  virtual ~DetailedPromise();

  enum eStatus { kSucceeded, kFailed };
  void MaybeReportTelemetry(eStatus aStatus);
  void LogRejectionReason(uint32_t aErrorCode, const nsACString& aReason);

  nsCString mName;
  bool mResponded;
  TimeStamp mStartTime;
  Optional<Telemetry::HistogramID> mSuccessLatencyProbe;
  Optional<Telemetry::HistogramID> mFailureLatencyProbe;
};

}  // namespace mozilla::dom

#endif  // __DetailedPromise_h__
