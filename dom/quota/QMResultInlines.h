/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QMRESULTINLINES_H_
#define DOM_QUOTA_QMRESULTINLINES_H_

#ifndef DOM_QUOTA_QMRESULT_H_
#  error Must include QMResult.h first
#endif

#include "mozilla/Result.h"
#ifdef QM_ERROR_STACKS_ENABLED
#  include "mozilla/ResultVariant.h"
#  include "nsError.h"
#else
#  include "mozilla/ResultExtensions.h"
#endif

namespace mozilla {

#ifdef QM_ERROR_STACKS_ENABLED
// Allow QMResult errors to use existing stack id and to increase the frame id
// during error propagation.
template <>
class MOZ_MUST_USE_TYPE GenericErrorResult<QMResult> {
  QMResult mErrorValue;

  template <typename V, typename E2>
  friend class Result;

 public:
  explicit GenericErrorResult(const QMResult& aErrorValue)
      : mErrorValue(aErrorValue) {
    MOZ_ASSERT(NS_FAILED(aErrorValue.NSResult()));
  }

  explicit GenericErrorResult(QMResult&& aErrorValue)
      : mErrorValue(std::move(aErrorValue)) {
    MOZ_ASSERT(NS_FAILED(aErrorValue.NSResult()));
  }

  explicit GenericErrorResult(const QMResult& aErrorValue,
                              const ErrorPropagationTag&)
      : GenericErrorResult(aErrorValue.Propagate()) {}

  explicit GenericErrorResult(QMResult&& aErrorValue,
                              const ErrorPropagationTag&)
      : GenericErrorResult(aErrorValue.Propagate()) {}

  operator QMResult() const { return mErrorValue; }

  operator nsresult() const { return mErrorValue.NSResult(); }
};

inline OkOrErr ToResult(const QMResult& aValue) {
  if (NS_FAILED(aValue.NSResult())) {
    return Err(aValue);
  }
  return Ok();
}

inline OkOrErr ToResult(QMResult&& aValue) {
  if (NS_FAILED(aValue.NSResult())) {
    return Err(std::move(aValue));
  }
  return Ok();
}
#endif

}  // namespace mozilla

#endif
