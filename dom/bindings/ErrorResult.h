/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/Move.h"
#include "nsTArray.h"

namespace IPC {
class Message;
template <typename> struct ParamTraits;
} // namespace IPC

namespace mozilla {

namespace dom {

enum ErrNum {
#define MSG_DEF(_name, _argc, _exn, _str) \
  _name,
#include "mozilla/dom/Errors.msg"
#undef MSG_DEF
  Err_Limit
};

// Debug-only compile-time table of the number of arguments of each error, for use in static_assert.
#if defined(DEBUG) && (defined(__clang__) || defined(__GNUC__))
uint16_t constexpr ErrorFormatNumArgs[] = {
#define MSG_DEF(_name, _argc, _exn, _str) \
  _argc,
#include "mozilla/dom/Errors.msg"
#undef MSG_DEF
};
#endif

uint16_t
GetErrorArgCount(const ErrNum aErrorNumber);

bool
ThrowErrorMessage(JSContext* aCx, const ErrNum aErrorNumber, ...);

struct StringArrayAppender
{
  static void Append(nsTArray<nsString>& aArgs, uint16_t aCount)
  {
    MOZ_RELEASE_ASSERT(aCount == 0, "Must give at least as many string arguments as are required by the ErrNum.");
  }

  template<typename... Ts>
  static void Append(nsTArray<nsString>& aArgs, uint16_t aCount, const nsAString* aFirst, Ts... aOtherArgs)
  {
    if (aCount == 0) {
      MOZ_ASSERT(false, "There should not be more string arguments provided than are required by the ErrNum.");
      return;
    }
    aArgs.AppendElement(*aFirst);
    Append(aArgs, aCount - 1, aOtherArgs...);
  }
};

} // namespace dom

class ErrorResult {
public:
  ErrorResult() {
    mResult = NS_OK;

#ifdef DEBUG
    mMightHaveUnreportedJSException = false;
    mHasMessage = false;
#endif
  }

#ifdef DEBUG
  ~ErrorResult() {
    MOZ_ASSERT_IF(IsErrorWithMessage(), !mMessage);
    MOZ_ASSERT(!mMightHaveUnreportedJSException);
    MOZ_ASSERT(!mHasMessage);
  }
#endif

  ErrorResult(ErrorResult&& aRHS)
  {
    *this = Move(aRHS);
  }
  ErrorResult& operator=(ErrorResult&& aRHS);

  explicit ErrorResult(nsresult aRv)
    : ErrorResult()
  {
    AssignErrorCode(aRv);
  }

  void Throw(nsresult rv) {
    MOZ_ASSERT(NS_FAILED(rv), "Please don't try throwing success");
    AssignErrorCode(rv);
  }

  // Use SuppressException when you want to suppress any exception that might be
  // on the ErrorResult.  After this call, the ErrorResult will be back a "no
  // exception thrown" state.
  void SuppressException();

  // Use StealNSResult() when you want to safely convert the ErrorResult to an
  // nsresult that you will then return to a caller.  This will
  // SuppressException(), since there will no longer be a way to report it.
  nsresult StealNSResult() {
    nsresult rv = ErrorCode();
    SuppressException();
    return rv;
  }

  template<dom::ErrNum errorNumber, typename... Ts>
  void ThrowTypeError(Ts... messageArgs)
  {
    ThrowErrorWithMessage<errorNumber>(NS_ERROR_TYPE_ERR, messageArgs...);
  }

  template<dom::ErrNum errorNumber, typename... Ts>
  void ThrowRangeError(Ts... messageArgs)
  {
    ThrowErrorWithMessage<errorNumber>(NS_ERROR_RANGE_ERR, messageArgs...);
  }

  void ReportErrorWithMessage(JSContext* cx);
  bool IsErrorWithMessage() const { return ErrorCode() == NS_ERROR_TYPE_ERR || ErrorCode() == NS_ERROR_RANGE_ERR; }

  // Facilities for throwing a preexisting JS exception value via this
  // ErrorResult.  The contract is that any code which might end up calling
  // ThrowJSException() must call MightThrowJSException() even if no exception
  // is being thrown.  Code that would call ReportJSException or
  // StealJSException as needed must first call WouldReportJSException even if
  // this ErrorResult has not failed.
  //
  // The exn argument to ThrowJSException can be in any compartment.  It does
  // not have to be in the compartment of cx.  If someone later uses it, they
  // will wrap it into whatever compartment they're working in, as needed.
  void ThrowJSException(JSContext* cx, JS::Handle<JS::Value> exn);
  void ReportJSException(JSContext* cx);
  bool IsJSException() const { return ErrorCode() == NS_ERROR_DOM_JS_EXCEPTION; }

  // Report a generic error.  This should only be used if we're not
  // some more specific exception type.
  void ReportGenericError(JSContext* cx);

  // Support for uncatchable exceptions.
  void ThrowUncatchableException() {
    Throw(NS_ERROR_UNCATCHABLE_EXCEPTION);
  }
  bool IsUncatchableException() const {
    return ErrorCode() == NS_ERROR_UNCATCHABLE_EXCEPTION;
  }

  // StealJSException steals the JS Exception from the object. This method must
  // be called only if IsJSException() returns true. This method also resets the
  // error code to NS_OK.
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
    AssignErrorCode(rv);
  }

  bool Failed() const {
    return NS_FAILED(mResult);
  }

  bool ErrorCodeIs(nsresult rv) const {
    return mResult == rv;
  }

  // For use in logging ONLY.
  uint32_t ErrorCodeAsInt() const {
    return static_cast<uint32_t>(ErrorCode());
  }

protected:
  nsresult ErrorCode() const {
    return mResult;
  }

private:
  friend struct IPC::ParamTraits<ErrorResult>;
  void SerializeMessage(IPC::Message* aMsg) const;
  bool DeserializeMessage(const IPC::Message* aMsg, void** aIter);

  // Helper method that creates a new Message for this ErrorResult,
  // and returns the arguments array from that Message.
  nsTArray<nsString>& CreateErrorMessageHelper(const dom::ErrNum errorNumber, nsresult errorType);

  template<dom::ErrNum errorNumber, typename... Ts>
  void ThrowErrorWithMessage(nsresult errorType, Ts... messageArgs)
  {
#if defined(DEBUG) && (defined(__clang__) || defined(__GNUC__))
    static_assert(dom::ErrorFormatNumArgs[errorNumber] == sizeof...(messageArgs),
                  "Pass in the right number of arguments");
#endif

    if (IsJSException()) {
      // We have rooted our mJSException, and we don't have the info
      // needed to unroot here, so just bail.
      MOZ_ASSERT(false,
                 "Ignoring ThrowErrorWithMessage call because we have a JS exception");
      return;
    }
    nsTArray<nsString>& messageArgsArray = CreateErrorMessageHelper(errorNumber, errorType);
    uint16_t argCount = dom::GetErrorArgCount(errorNumber);
    dom::StringArrayAppender::Append(messageArgsArray, argCount, messageArgs...);
#ifdef DEBUG
    mHasMessage = true;
#endif
  }

  void AssignErrorCode(nsresult aRv) {
    MOZ_ASSERT(aRv != NS_ERROR_TYPE_ERR, "Use ThrowTypeError()");
    MOZ_ASSERT(aRv != NS_ERROR_RANGE_ERR, "Use ThrowRangeError()");
    MOZ_ASSERT(!IsErrorWithMessage(), "Don't overwrite errors with message");
    MOZ_ASSERT(aRv != NS_ERROR_DOM_JS_EXCEPTION, "Use ThrowJSException()");
    MOZ_ASSERT(!IsJSException(), "Don't overwrite JS exceptions");
    MOZ_ASSERT(aRv != NS_ERROR_XPC_NOT_ENOUGH_ARGS, "May need to bring back ThrowNotEnoughArgsError");
    mResult = aRv;
  }

  void ClearMessage();

  nsresult mResult;
  struct Message;
  // mMessage is set by ThrowErrorWithMessage and cleared (and deallocated) by
  // ReportErrorWithMessage.
  // mJSException is set (and rooted) by ThrowJSException and unrooted
  // by ReportJSException.
  union {
    Message* mMessage; // valid when IsErrorWithMessage()
    JS::Value mJSException; // valid when IsJSException()
  };

#ifdef DEBUG
  // Used to keep track of codepaths that might throw JS exceptions,
  // for assertion purposes.
  bool mMightHaveUnreportedJSException;
  // Used to keep track of whether mMessage has ever been assigned to.
  // We need to check this in order to ensure that not attempting to
  // delete mMessage in DeserializeMessage doesn't leak memory.
  bool mHasMessage;
#endif

  // Not to be implemented, to make sure people always pass this by
  // reference, not by value.
  ErrorResult(const ErrorResult&) = delete;
  void operator=(const ErrorResult&) = delete;
};

/******************************************************************************
 ** Macros for checking results
 ******************************************************************************/

#define ENSURE_SUCCESS(res, ret)                                          \
  do {                                                                    \
    if (res.Failed()) {                                                   \
      nsCString msg;                                                      \
      msg.AppendPrintf("ENSURE_SUCCESS(%s, %s) failed with "              \
                       "result 0x%X", #res, #ret, res.ErrorCodeAsInt());  \
      NS_WARNING(msg.get());                                              \
      return ret;                                                         \
    }                                                                     \
  } while(0)

#define ENSURE_SUCCESS_VOID(res)                                          \
  do {                                                                    \
    if (res.Failed()) {                                                   \
      nsCString msg;                                                      \
      msg.AppendPrintf("ENSURE_SUCCESS_VOID(%s) failed with "             \
                       "result 0x%X", #res, res.ErrorCodeAsInt());        \
      NS_WARNING(msg.get());                                              \
      return;                                                             \
    }                                                                     \
  } while(0)

} // namespace mozilla

#endif /* mozilla_ErrorResult_h */
