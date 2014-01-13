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

#include "js/Value.h"
#include "nscore.h"
#include "nsStringGlue.h"
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

bool
ThrowErrorMessage(JSContext* aCx, const ErrNum aErrorNumber, ...);

} // namespace dom

class ErrorResult {
public:
  ErrorResult() {
    mResult = NS_OK;

#ifdef DEBUG
    // ErrorResult is extremely performance-sensitive code, where literally
    // every machine instruction matters. Initialize mMessage only to suppress
    // a debug-only warning from gcc 4.6.
    mMessage = nullptr;
    mMightHaveUnreportedJSException = false;
#endif
  }

#ifdef DEBUG
  ~ErrorResult() {
    MOZ_ASSERT_IF(IsTypeError(), !mMessage);
    MOZ_ASSERT(!mMightHaveUnreportedJSException);
  }
#endif

  void Throw(nsresult rv) {
    MOZ_ASSERT(NS_FAILED(rv), "Please don't try throwing success");
    MOZ_ASSERT(rv != NS_ERROR_TYPE_ERR, "Use ThrowTypeError()");
    MOZ_ASSERT(!IsTypeError(), "Don't overwite TypeError");
    MOZ_ASSERT(rv != NS_ERROR_DOM_JS_EXCEPTION, "Use ThrowJSException()");
    MOZ_ASSERT(!IsJSException(), "Don't overwrite JS exceptions");
    MOZ_ASSERT(rv != NS_ERROR_XPC_NOT_ENOUGH_ARGS, "Use ThrowNotEnoughArgsError()");
    MOZ_ASSERT(!IsNotEnoughArgsError(), "Don't overwrite not enough args error");
    mResult = rv;
  }

  void ThrowTypeError(const dom::ErrNum errorNumber, ...);
  void ReportTypeError(JSContext* cx);
  void ClearMessage();
  bool IsTypeError() const { return ErrorCode() == NS_ERROR_TYPE_ERR; }

  // Facilities for throwing a preexisting JS exception value via this
  // ErrorResult.  The contract is that any code which might end up calling
  // ThrowJSException() must call MightThrowJSException() even if no exception
  // is being thrown.  Code that would call ReportJSException* or
  // StealJSException as needed must first call WouldReportJSException even if
  // this ErrorResult has not failed.
  void ThrowJSException(JSContext* cx, JS::Handle<JS::Value> exn);
  void ReportJSException(JSContext* cx);
  // Used to implement throwing exceptions from the JS implementation of
  // bindings to callers of the binding.
  void ReportJSExceptionFromJSImplementation(JSContext* aCx);
  bool IsJSException() const { return ErrorCode() == NS_ERROR_DOM_JS_EXCEPTION; }

  void ThrowNotEnoughArgsError() { mResult = NS_ERROR_XPC_NOT_ENOUGH_ARGS; }
  void ReportNotEnoughArgsError(JSContext* cx,
                                const char* ifaceName,
                                const char* memberName);
  bool IsNotEnoughArgsError() const { return ErrorCode() == NS_ERROR_XPC_NOT_ENOUGH_ARGS; }

  // StealJSException steals the JS Exception from the object. This method must
  // be called only if IsJSException() returns true. This method also resets the
  // ErrorCode() to NS_OK.
  void StealJSException(JSContext* cx, JS::MutableHandle<JS::Value> value);

  void MOZ_ALWAYS_INLINE MightThrowJSException()
  {
#ifdef DEBUG
    mMightHaveUnreportedJSException = true;
#endif
  }
  void MOZ_ALWAYS_INLINE WouldReportJSException()
  {
#ifdef DEBUG
    mMightHaveUnreportedJSException = false;
#endif
  }

  // In the future, we can add overloads of Throw that take more
  // interesting things, like strings or DOM exception types or
  // something if desired.

  // Backwards-compat to make conversion simpler.  We don't call
  // Throw() here because people can easily pass success codes to
  // this.
  void operator=(nsresult rv) {
    MOZ_ASSERT(rv != NS_ERROR_TYPE_ERR, "Use ThrowTypeError()");
    MOZ_ASSERT(!IsTypeError(), "Don't overwite TypeError");
    MOZ_ASSERT(rv != NS_ERROR_DOM_JS_EXCEPTION, "Use ThrowJSException()");
    MOZ_ASSERT(!IsJSException(), "Don't overwrite JS exceptions");
    MOZ_ASSERT(rv != NS_ERROR_XPC_NOT_ENOUGH_ARGS, "Use ThrowNotEnoughArgsError()");
    MOZ_ASSERT(!IsNotEnoughArgsError(), "Don't overwrite not enough args error");
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
  // mMessage is set by ThrowTypeError and cleared (and deallocatd) by
  // ReportTypeError.
  // mJSException is set (and rooted) by ThrowJSException and unrooted
  // by ReportJSException.
  union {
    Message* mMessage; // valid when IsTypeError()
    JS::Value mJSException; // valid when IsJSException()
  };

#ifdef DEBUG
  // Used to keep track of codepaths that might throw JS exceptions,
  // for assertion purposes.
  bool mMightHaveUnreportedJSException;
#endif

  // Not to be implemented, to make sure people always pass this by
  // reference, not by value.
  ErrorResult(const ErrorResult&) MOZ_DELETE;
};

/******************************************************************************
 ** Macros for checking results
 ******************************************************************************/

#define ENSURE_SUCCESS(res, ret)                                          \
  do {                                                                    \
    if (res.Failed()) {                                                   \
      nsCString msg;                                                      \
      msg.AppendPrintf("ENSURE_SUCCESS(%s, %s) failed with "              \
                       "result 0x%X", #res, #ret, res.ErrorCode());       \
      NS_WARNING(msg.get());                                              \
      return ret;                                                         \
    }                                                                     \
  } while(0)

#define ENSURE_SUCCESS_VOID(res)                                          \
  do {                                                                    \
    if (res.Failed()) {                                                   \
      nsCString msg;                                                      \
      msg.AppendPrintf("ENSURE_SUCCESS_VOID(%s) failed with "             \
                       "result 0x%X", #res, res.ErrorCode());             \
      NS_WARNING(msg.get());                                              \
      return;                                                             \
    }                                                                     \
  } while(0)

} // namespace mozilla

#endif /* mozilla_ErrorResult_h */
