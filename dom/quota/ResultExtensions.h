/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QMRESULTINLINES_H_
#define DOM_QUOTA_QMRESULTINLINES_H_

#include "nsError.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/quota/Config.h"
#include "mozilla/dom/quota/RemoveParen.h"

#ifdef QM_ERROR_STACKS_ENABLED
#  include "mozilla/ResultVariant.h"
#endif

namespace mozilla {

// Allow bool errors to automatically convert to bool values, so MOZ_TRY/QM_TRY
// can be used in bool returning methods with Result<T, bool> results.
template <>
class [[nodiscard]] GenericErrorResult<bool> {
  bool mErrorValue;

  template <typename V, typename E2>
  friend class Result;

 public:
  explicit GenericErrorResult(bool aErrorValue) : mErrorValue(aErrorValue) {
    MOZ_ASSERT(!aErrorValue);
  }

  GenericErrorResult(bool aErrorValue, const ErrorPropagationTag&)
      : GenericErrorResult(aErrorValue) {}

  MOZ_IMPLICIT operator bool() const { return mErrorValue; }
};

// Allow MOZ_TRY/QM_TRY to handle `bool` values.
template <typename E = nsresult>
inline Result<Ok, E> ToResult(bool aValue) {
  if (aValue) {
    return Ok();
  }
  return Err(ResultTypeTraits<E>::From(NS_ERROR_FAILURE));
}

constexpr nsresult ToNSResult(nsresult aError) { return aError; }

#ifdef QM_ERROR_STACKS_ENABLED

inline nsresult ToNSResult(const QMResult& aError) { return aError.NSResult(); }

// Allow QMResult errors to use existing stack id and to increase the frame id
// during error propagation.
template <>
class [[nodiscard]] GenericErrorResult<QMResult> {
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

template <>
struct ResultTypeTraits<QMResult> {
  static QMResult From(nsresult aValue) { return ToQMResult(aValue); }

  static QMResult From(const QMResult& aValue) { return aValue; }

  static QMResult From(QMResult&& aValue) { return std::move(aValue); }
};

template <typename E>
inline Result<Ok, E> ToResult(const QMResult& aValue) {
  if (NS_FAILED(aValue.NSResult())) {
    return Err(ResultTypeTraits<E>::From(aValue));
  }
  return Ok();
}

template <typename E>
inline Result<Ok, E> ToResult(QMResult&& aValue) {
  if (NS_FAILED(aValue.NSResult())) {
    return Err(ResultTypeTraits<E>::From(aValue));
  }
  return Ok();
}
#endif

template <typename E = nsresult, typename V, typename E2>
inline Result<V, E> ToResultTransform(Result<V, E2>&& aValue) {
  return std::forward<Result<V, E2>>(aValue).mapErr(
      [](auto&& err) { return ResultTypeTraits<E>::From(err); });
}

// TODO: Maybe move this to mfbt/ResultExtensions.h
template <typename R, typename Func, typename... Args>
Result<R, nsresult> ToResultGet(const Func& aFunc, Args&&... aArgs) {
  nsresult rv;
  R res = aFunc(std::forward<Args>(aArgs)..., &rv);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }
  return res;
}

}  // namespace mozilla

// TODO: Maybe move this to mfbt/ResultExtensions.h
#define MOZ_TO_RESULT(expr) ToResult(expr)

#define QM_TO_RESULT(expr) ToResult<QMResult>(expr)

#define QM_TO_RESULT_TRANSFORM(value) ToResultTransform<QMResult>(value)

#define MOZ_TO_RESULT_GET_TYPED(resultType, ...) \
  ::mozilla::ToResultGet<MOZ_REMOVE_PAREN(resultType)>(__VA_ARGS__)

#define MOZ_TO_RESULT_INVOKE_TYPED(resultType, ...) \
  ::mozilla::ToResultInvoke<MOZ_REMOVE_PAREN(resultType)>(__VA_ARGS__)

#define QM_TO_RESULT_INVOKE_MEMBER(obj, methodname, ...)                 \
  ::mozilla::ToResultInvokeMember<QMResult>(                             \
      (obj), &::mozilla::detail::DerefedType<decltype(obj)>::methodname, \
      ##__VA_ARGS__)

#define QM_TO_RESULT_INVOKE_MEMBER_TYPED(resultType, obj, methodname, ...) \
  (::mozilla::ToResultInvoke<resultType, QMResult>(                        \
      ::std::mem_fn(                                                       \
          &::mozilla::detail::DerefedType<decltype(obj)>::methodname),     \
      (obj), ##__VA_ARGS__))

#endif
