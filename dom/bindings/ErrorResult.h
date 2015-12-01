/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A struct for tracking exceptions that need to be thrown to JS.
 *
 * Conceptually, an ErrorResult represents either success or an exception in the
 * process of being thrown.  This means that a failing ErrorResult _must_ be
 * handled in one of the following ways before coming off the stack:
 *
 * 1) Suppressed via SuppressException().
 * 2) Converted to a pure nsresult return value via StealNSResult().
 * 3) Converted to an actual pending exception on a JSContext via
 *    MaybeSetPendingException.
 * 4) Converted to an exception JS::Value (probably to then reject a Promise
 *    with) via dom::ToJSValue.
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
  static void Append(nsTArray<nsString>& aArgs, uint16_t aCount, const nsAString& aFirst, Ts&&... aOtherArgs)
  {
    if (aCount == 0) {
      MOZ_ASSERT(false, "There should not be more string arguments provided than are required by the ErrNum.");
      return;
    }
    aArgs.AppendElement(aFirst);
    Append(aArgs, aCount - 1, Forward<Ts>(aOtherArgs)...);
  }
};

} // namespace dom

class ErrorResult {
public:
  ErrorResult()
    : mResult(NS_OK)
#ifdef DEBUG
    , mMightHaveUnreportedJSException(false)
    , mUnionState(HasNothing)
#endif
  {
  }

#ifdef DEBUG
  ~ErrorResult() {
    // Consumers should have called one of MaybeSetPendingException
    // (possibly via ToJSValue), StealNSResult, and SuppressException
    MOZ_ASSERT(!Failed());
    MOZ_ASSERT(!mMightHaveUnreportedJSException);
    MOZ_ASSERT(mUnionState == HasNothing);
  }
#endif // DEBUG

  ErrorResult(ErrorResult&& aRHS)
    // Initialize mResult and whatever else we need to default-initialize, so
    // the ClearUnionData call in our operator= will do the right thing
    // (nothing).
    : ErrorResult()
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

  // Duplicate our current state on the given ErrorResult object.  Any existing
  // errors or messages on the target will be suppressed before cloning.  Our
  // own error state remains unchanged.
  void CloneTo(ErrorResult& aRv) const;

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

  // Use MaybeSetPendingException to convert an ErrorResult to a pending
  // exception on the given JSContext.  This is the normal "throw an exception"
  // codepath.
  //
  // The return value is false if the ErrorResult represents success, true
  // otherwise.  This does mean that in JSAPI method implementations you can't
  // just use this as |return rv.MaybeSetPendingException(cx)| (though you could
  // |return !rv.MaybeSetPendingException(cx)|), but in practice pretty much any
  // consumer would want to do some more work on the success codepath.  So
  // instead the way you use this is:
  //
  //   if (rv.MaybeSetPendingException(cx)) {
  //     bail out here
  //   }
  //   go on to do something useful
  //
  // The success path is inline, since it should be the common case and we don't
  // want to pay the price of a function call in some of the consumers of this
  // method in the common case.
  //
  // Note that a true return value does NOT mean there is now a pending
  // exception on aCx, due to uncatchable exceptions.  It should still be
  // considered equivalent to a JSAPI failure in terms of what callers should do
  // after true is returned.
  //
  // After this call, the ErrorResult will no longer return true from Failed(),
  // since the exception will have moved to the JSContext.
  bool MaybeSetPendingException(JSContext* cx)
  {
    WouldReportJSException();
    if (!Failed()) {
      return false;
    }

    SetPendingException(cx);
    return true;
  }

  template<dom::ErrNum errorNumber, typename... Ts>
  void ThrowTypeError(Ts&&... messageArgs)
  {
    ThrowErrorWithMessage<errorNumber>(NS_ERROR_TYPE_ERR,
                                       Forward<Ts>(messageArgs)...);
  }

  template<dom::ErrNum errorNumber, typename... Ts>
  void ThrowRangeError(Ts&&... messageArgs)
  {
    ThrowErrorWithMessage<errorNumber>(NS_ERROR_RANGE_ERR,
                                       Forward<Ts>(messageArgs)...);
  }

  bool IsErrorWithMessage() const { return ErrorCode() == NS_ERROR_TYPE_ERR || ErrorCode() == NS_ERROR_RANGE_ERR; }

  // Facilities for throwing a preexisting JS exception value via this
  // ErrorResult.  The contract is that any code which might end up calling
  // ThrowJSException() must call MightThrowJSException() even if no exception
  // is being thrown.  Code that conditionally calls ToJSValue on this
  // ErrorResult only if Failed() must first call WouldReportJSException even if
  // this ErrorResult has not failed.
  //
  // The exn argument to ThrowJSException can be in any compartment.  It does
  // not have to be in the compartment of cx.  If someone later uses it, they
  // will wrap it into whatever compartment they're working in, as needed.
  void ThrowJSException(JSContext* cx, JS::Handle<JS::Value> exn);
  bool IsJSException() const { return ErrorCode() == NS_ERROR_DOM_JS_EXCEPTION; }

  // Facilities for throwing a DOMException.  If an empty message string is
  // passed to ThrowDOMException, the default message string for the given
  // nsresult will be used.  The passed-in string must be UTF-8.  The nsresult
  // passed in must be one we create DOMExceptions for; otherwise you may get an
  // XPConnect Exception.
  void ThrowDOMException(nsresult rv, const nsACString& message = EmptyCString());
  bool IsDOMException() const { return ErrorCode() == NS_ERROR_DOM_DOMEXCEPTION; }

  // Flag on the ErrorResult that whatever needs throwing has been
  // thrown on the JSContext already and we should not mess with it.
  void NoteJSContextException() {
    mResult = NS_ERROR_DOM_EXCEPTION_ON_JSCONTEXT;
  }
  // Check whether the ErrorResult says to just throw whatever is on
  // the JSContext already.
  bool IsJSContextException() {
    return ErrorCode() == NS_ERROR_DOM_EXCEPTION_ON_JSCONTEXT;
  }

  // Support for uncatchable exceptions.
  void ThrowUncatchableException() {
    Throw(NS_ERROR_UNCATCHABLE_EXCEPTION);
  }
  bool IsUncatchableException() const {
    return ErrorCode() == NS_ERROR_UNCATCHABLE_EXCEPTION;
  }

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
#ifdef DEBUG
  enum UnionState {
    HasMessage,
    HasDOMExceptionInfo,
    HasJSException,
    HasNothing
  };
#endif // DEBUG

  friend struct IPC::ParamTraits<ErrorResult>;
  void SerializeMessage(IPC::Message* aMsg) const;
  bool DeserializeMessage(const IPC::Message* aMsg, void** aIter);

  void SerializeDOMExceptionInfo(IPC::Message* aMsg) const;
  bool DeserializeDOMExceptionInfo(const IPC::Message* aMsg, void** aIter);

  // Helper method that creates a new Message for this ErrorResult,
  // and returns the arguments array from that Message.
  nsTArray<nsString>& CreateErrorMessageHelper(const dom::ErrNum errorNumber, nsresult errorType);

  template<dom::ErrNum errorNumber, typename... Ts>
  void ThrowErrorWithMessage(nsresult errorType, Ts&&... messageArgs)
  {
#if defined(DEBUG) && (defined(__clang__) || defined(__GNUC__))
    static_assert(dom::ErrorFormatNumArgs[errorNumber] == sizeof...(messageArgs),
                  "Pass in the right number of arguments");
#endif

    ClearUnionData();

    nsTArray<nsString>& messageArgsArray = CreateErrorMessageHelper(errorNumber, errorType);
    uint16_t argCount = dom::GetErrorArgCount(errorNumber);
    dom::StringArrayAppender::Append(messageArgsArray, argCount,
                                     Forward<Ts>(messageArgs)...);
#ifdef DEBUG
    mUnionState = HasMessage;
#endif // DEBUG
  }

  void AssignErrorCode(nsresult aRv) {
    MOZ_ASSERT(aRv != NS_ERROR_TYPE_ERR, "Use ThrowTypeError()");
    MOZ_ASSERT(aRv != NS_ERROR_RANGE_ERR, "Use ThrowRangeError()");
    MOZ_ASSERT(!IsErrorWithMessage(), "Don't overwrite errors with message");
    MOZ_ASSERT(aRv != NS_ERROR_DOM_JS_EXCEPTION, "Use ThrowJSException()");
    MOZ_ASSERT(!IsJSException(), "Don't overwrite JS exceptions");
    MOZ_ASSERT(aRv != NS_ERROR_DOM_DOMEXCEPTION, "Use ThrowDOMException()");
    MOZ_ASSERT(!IsDOMException(), "Don't overwrite DOM exceptions");
    MOZ_ASSERT(aRv != NS_ERROR_XPC_NOT_ENOUGH_ARGS, "May need to bring back ThrowNotEnoughArgsError");
    MOZ_ASSERT(aRv != NS_ERROR_DOM_EXCEPTION_ON_JSCONTEXT,
               "Use NoteJSContextException");
    mResult = aRv;
  }

  void ClearMessage();
  void ClearDOMExceptionInfo();

  // ClearUnionData will try to clear the data in our
  // mMessage/mJSException/mDOMExceptionInfo union.  After this the union may be
  // in an uninitialized state (e.g. mMessage or mDOMExceptionInfo may be
  // pointing to deleted memory) and the caller must either reinitialize it or
  // change mResult to something that will not involve us touching the union
  // anymore.
  void ClearUnionData();

  // Implementation of MaybeSetPendingException for the case when we're a
  // failure result.
  void SetPendingException(JSContext* cx);

  // Methods for setting various specific kinds of pending exceptions.
  void SetPendingExceptionWithMessage(JSContext* cx);
  void SetPendingJSException(JSContext* cx);
  void SetPendingDOMException(JSContext* cx);
  void SetPendingGenericErrorException(JSContext* cx);


  // Special values of mResult:
  // NS_ERROR_TYPE_ERR -- ThrowTypeError() called on us.
  // NS_ERROR_RANGE_ERR -- ThrowRangeError() called on us.
  // NS_ERROR_DOM_JS_EXCEPTION -- ThrowJSException() called on us.
  // NS_ERROR_UNCATCHABLE_EXCEPTION -- ThrowUncatchableException called on us.
  // NS_ERROR_DOM_DOMEXCEPTION -- ThrowDOMException() called on us.
  nsresult mResult;

  struct Message;
  struct DOMExceptionInfo;
  // mMessage is set by ThrowErrorWithMessage and reported (and deallocated) by
  // SetPendingExceptionWithMessage.
  // mJSException is set (and rooted) by ThrowJSException and reported
  // (and unrooted) by SetPendingJSException.
  // mDOMExceptionInfo is set by ThrowDOMException and reported
  // (and deallocated) by SetPendingDOMException.
  union {
    Message* mMessage; // valid when IsErrorWithMessage()
    JS::Value mJSException; // valid when IsJSException()
    DOMExceptionInfo* mDOMExceptionInfo; // valid when IsDOMException()
  };

#ifdef DEBUG
  // Used to keep track of codepaths that might throw JS exceptions,
  // for assertion purposes.
  bool mMightHaveUnreportedJSException;

  // Used to keep track of what's stored in our union right now.  Note
  // that this may be set to HasNothing even if our mResult suggests
  // we should have something, if we have already cleaned up the
  // something.
  UnionState mUnionState;
#endif

  // Not to be implemented, to make sure people always pass this by
  // reference, not by value.
  ErrorResult(const ErrorResult&) = delete;
  void operator=(const ErrorResult&) = delete;
};

// A class for use when an ErrorResult should just automatically be ignored.
class IgnoredErrorResult : public ErrorResult
{
public:
  ~IgnoredErrorResult()
  {
    SuppressException();
  }
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
