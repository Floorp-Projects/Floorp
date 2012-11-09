/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A struct for tracking exceptions that need to be thrown to JS.
 */

#ifndef mozilla_ErrorResult_h
#define mozilla_ErrorResult_h

#include <stdarg.h>

#include "jsapi.h"
#include "nscore.h"
#include "mozilla/Assertions.h"

namespace mozilla {

namespace dom {

enum ErrNum {
#define MSG_DEF(_name, _argc, _str) \
  _name,
#include "mozilla/dom/Errors.msg"
#undef MSG_DEF
  Err_Limit
};

} // namespace dom

class ErrorResult {
public:
  ErrorResult() {
    mResult = NS_OK;
  }
#ifdef DEBUG
  ~ErrorResult() {
    MOZ_ASSERT_IF(IsTypeError(), !mMessage);
  }
#endif

  void Throw(nsresult rv) {
    MOZ_ASSERT(NS_FAILED(rv), "Please don't try throwing success");
    MOZ_ASSERT(rv != NS_ERROR_TYPE_ERR, "Use ThrowTypeError()");
    MOZ_ASSERT(!IsTypeError(), "Don't overwite TypeError");
    mResult = rv;
  }

  void ThrowTypeError(const dom::ErrNum errorNumber, ...);
  void ReportTypeError(JSContext* cx);
  bool IsTypeError() const { return ErrorCode() == NS_ERROR_TYPE_ERR; }

  // In the future, we can add overloads of Throw that take more
  // interesting things, like strings or DOM exception types or
  // something if desired.

  // Backwards-compat to make conversion simpler.  We don't call
  // Throw() here because people can easily pass success codes to
  // this.
  void operator=(nsresult rv) {
    MOZ_ASSERT(rv != NS_ERROR_TYPE_ERR, "Use ThrowTypeError()");
    MOZ_ASSERT(!IsTypeError(), "Don't overwite TypeError");
    mResult = rv;
  }

  bool Failed() const {
    return NS_FAILED(mResult);
  }

  nsresult ErrorCode() const {
    return mResult;
  }

private:
  nsresult mResult;
  struct Message;
  // Do not use nsAutoPtr to avoid extra initalizatoin and check.
  Message* mMessage;

  // Not to be implemented, to make sure people always pass this by
  // reference, not by value.
  ErrorResult(const ErrorResult&) MOZ_DELETE;
};

} // namespace mozilla

#endif /* mozilla_ErrorResult_h */
