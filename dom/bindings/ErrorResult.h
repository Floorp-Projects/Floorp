/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A set of structs for tracking exceptions that need to be thrown to JS:
 * ErrorResult and IgnoredErrorResult.
 *
 * Conceptually, these structs represent either success or an exception in the
 * process of being thrown.  This means that a failing ErrorResult _must_ be
 * handled in one of the following ways before coming off the stack:
 *
 * 1) Suppressed via SuppressException().
 * 2) Converted to a pure nsresult return value via StealNSResult().
 * 3) Converted to an actual pending exception on a JSContext via
 *    MaybeSetPendingException.
 * 4) Converted to an exception JS::Value (probably to then reject a Promise
 *    with) via dom::ToJSValue.
 *
 * An IgnoredErrorResult will automatically do the first of those four things.
 */

#ifndef mozilla_ErrorResult_h
#define mozilla_ErrorResult_h

#include <stdarg.h>

#include "js/GCAnnotations.h"
#include "js/Value.h"
#include "nscore.h"
#include "nsString.h"
#include "mozilla/Assertions.h"
#include "mozilla/Move.h"
#include "nsTArray.h"
#include "nsISupportsImpl.h"

namespace IPC {
class Message;
template <typename> struct ParamTraits;
} // namespace IPC
class PickleIterator;

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

namespace binding_detail {
void ThrowErrorMessage(JSContext* aCx, const unsigned aErrorNumber, ...);
} // namespace binding_detail

template<typename... Ts>
inline bool
ThrowErrorMessage(JSContext* aCx, const ErrNum aErrorNumber, Ts&&... aArgs)
{
  binding_detail::ThrowErrorMessage(aCx, static_cast<unsigned>(aErrorNumber),
                                    mozilla::Forward<Ts>(aArgs)...);
  return false;
}

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

class ErrorResult;
class OOMReporter;

namespace binding_danger {

/**
 * Templated implementation class for various ErrorResult-like things.  The
 * instantiations differ only in terms of their cleanup policies (used in the
 * destructor), which they can specify via the template argument.  Note that
 * this means it's safe to reinterpret_cast between the instantiations unless
 * you plan to invoke the destructor through such a cast pointer.
 *
 * A cleanup policy consists of two booleans: whether to assert that we've been
 * reported or suppressed, and whether to then go ahead and suppress the
 * exception.
 */
template<typename CleanupPolicy>
class TErrorResult {
public:
  TErrorResult()
    : mResult(NS_OK)
#ifdef DEBUG
    , mMightHaveUnreportedJSException(false)
    , mUnionState(HasNothing)
#endif
  {
  }

  ~TErrorResult() {
    AssertInOwningThread();

    if (CleanupPolicy::assertHandled) {
      // Consumers should have called one of MaybeSetPendingException
      // (possibly via ToJSValue), StealNSResult, and SuppressException
      AssertReportedOrSuppressed();
    }

    if (CleanupPolicy::suppress) {
      SuppressException();
    }

    // And now assert that we're in a good final state.
    AssertReportedOrSuppressed();
  }

  TErrorResult(TErrorResult&& aRHS)
    // Initialize mResult and whatever else we need to default-initialize, so
    // the ClearUnionData call in our operator= will do the right thing
    // (nothing).
    : TErrorResult()
  {
    *this = Move(aRHS);
  }
  TErrorResult& operator=(TErrorResult&& aRHS);

  explicit TErrorResult(nsresult aRv)
    : TErrorResult()
  {
    AssignErrorCode(aRv);
  }

  operator ErrorResult&();
  operator OOMReporter&();

  void MOZ_MUST_RETURN_FROM_CALLER Throw(nsresult rv) {
    MOZ_ASSERT(NS_FAILED(rv), "Please don't try throwing success");
    AssignErrorCode(rv);
  }

  // This method acts identically to the `Throw` method, however, it does not
  // have the MOZ_MUST_RETURN_FROM_CALLER static analysis annotation. It is
  // intended to be used in situations when additional work needs to be
  // performed in the calling function after the Throw method is called.
  //
  // In general you should prefer using `Throw`, and returning after an error,
  // for example:
  //
  //   if (condition) {
  //     aRv.Throw(NS_ERROR_FAILURE);
  //     return;
  //   }
  //
  // or
  //
  //   if (condition) {
  //     aRv.Throw(NS_ERROR_FAILURE);
  //   }
  //   return;
  //
  // However, if you need to do some other work after throwing, such as:
  //
  //   if (condition) {
  //     aRv.ThrowWithCustomCleanup(NS_ERROR_FAILURE);
  //   }
  //   // Do some important clean-up work which couldn't happen earlier.
  //   // We want to do this clean-up work in both the success and failure cases.
  //   CleanUpImportantState();
  //   return;
  //
  // Then you'll need to use ThrowWithCustomCleanup to get around the static
  // analysis, which would complain that you are doing work after the call to
  // `Throw()`.
  void ThrowWithCustomCleanup(nsresult rv) {
    Throw(rv);
  }

  // Duplicate our current state on the given TErrorResult object.  Any
  // existing errors or messages on the target will be suppressed before
  // cloning.  Our own error state remains unchanged.
  void CloneTo(TErrorResult& aRv) const;

  // Use SuppressException when you want to suppress any exception that might be
  // on the TErrorResult.  After this call, the TErrorResult will be back a "no
  // exception thrown" state.
  void SuppressException();

  // Use StealNSResult() when you want to safely convert the TErrorResult to
  // an nsresult that you will then return to a caller.  This will
  // SuppressException(), since there will no longer be a way to report it.
  nsresult StealNSResult() {
    nsresult rv = ErrorCode();
    SuppressException();
    // Don't propagate out our internal error codes that have special meaning.
    if (rv == NS_ERROR_INTERNAL_ERRORRESULT_TYPEERROR ||
        rv == NS_ERROR_INTERNAL_ERRORRESULT_RANGEERROR ||
        rv == NS_ERROR_INTERNAL_ERRORRESULT_JS_EXCEPTION ||
        rv == NS_ERROR_INTERNAL_ERRORRESULT_DOMEXCEPTION) {
      // What to pick here?
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    return rv;
  }

  // Use MaybeSetPendingException to convert a TErrorResult to a pending
  // exception on the given JSContext.  This is the normal "throw an exception"
  // codepath.
  //
  // The return value is false if the TErrorResult represents success, true
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
  // After this call, the TErrorResult will no longer return true from Failed(),
  // since the exception will have moved to the JSContext.
  MOZ_MUST_USE
  bool MaybeSetPendingException(JSContext* cx)
  {
    WouldReportJSException();
    if (!Failed()) {
      return false;
    }

    SetPendingException(cx);
    return true;
  }

  // Use StealExceptionFromJSContext to convert a pending exception on a
  // JSContext to a TErrorResult.  This function must be called only when a
  // JSAPI operation failed.  It assumes that lack of pending exception on the
  // JSContext means an uncatchable exception was thrown.
  //
  // Codepaths that might call this method must call MightThrowJSException even
  // if the relevant JSAPI calls do not fail.
  //
  // When this function returns, JS_IsExceptionPending(cx) will definitely be
  // false.
  void StealExceptionFromJSContext(JSContext* cx);

  template<dom::ErrNum errorNumber, typename... Ts>
  void ThrowTypeError(Ts&&... messageArgs)
  {
    ThrowErrorWithMessage<errorNumber>(NS_ERROR_INTERNAL_ERRORRESULT_TYPEERROR,
                                       Forward<Ts>(messageArgs)...);
  }

  template<dom::ErrNum errorNumber, typename... Ts>
  void ThrowRangeError(Ts&&... messageArgs)
  {
    ThrowErrorWithMessage<errorNumber>(NS_ERROR_INTERNAL_ERRORRESULT_RANGEERROR,
                                       Forward<Ts>(messageArgs)...);
  }

  bool IsErrorWithMessage() const
  {
    return ErrorCode() == NS_ERROR_INTERNAL_ERRORRESULT_TYPEERROR ||
           ErrorCode() == NS_ERROR_INTERNAL_ERRORRESULT_RANGEERROR;
  }

  // Facilities for throwing a preexisting JS exception value via this
  // TErrorResult.  The contract is that any code which might end up calling
  // ThrowJSException() or StealExceptionFromJSContext() must call
  // MightThrowJSException() even if no exception is being thrown.  Code that
  // conditionally calls ToJSValue on this TErrorResult only if Failed() must
  // first call WouldReportJSException even if this TErrorResult has not failed.
  //
  // The exn argument to ThrowJSException can be in any compartment.  It does
  // not have to be in the compartment of cx.  If someone later uses it, they
  // will wrap it into whatever compartment they're working in, as needed.
  void ThrowJSException(JSContext* cx, JS::Handle<JS::Value> exn);
  bool IsJSException() const
  {
    return ErrorCode() == NS_ERROR_INTERNAL_ERRORRESULT_JS_EXCEPTION;
  }

  // Facilities for throwing a DOMException.  If an empty message string is
  // passed to ThrowDOMException, the default message string for the given
  // nsresult will be used.  The passed-in string must be UTF-8.  The nsresult
  // passed in must be one we create DOMExceptions for; otherwise you may get an
  // XPConnect Exception.
  void ThrowDOMException(nsresult rv, const nsACString& message = EmptyCString());
  bool IsDOMException() const
  {
    return ErrorCode() == NS_ERROR_INTERNAL_ERRORRESULT_DOMEXCEPTION;
  }

  // Flag on the TErrorResult that whatever needs throwing has been
  // thrown on the JSContext already and we should not mess with it.
  // If nothing was thrown, this becomes an uncatchable exception.
  void NoteJSContextException(JSContext* aCx);

  // Check whether the TErrorResult says to just throw whatever is on
  // the JSContext already.
  bool IsJSContextException() {
    return ErrorCode() == NS_ERROR_INTERNAL_ERRORRESULT_EXCEPTION_ON_JSCONTEXT;
  }

  // Support for uncatchable exceptions.
  void MOZ_MUST_RETURN_FROM_CALLER ThrowUncatchableException() {
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

  friend struct IPC::ParamTraits<TErrorResult>;
  friend struct IPC::ParamTraits<ErrorResult>;
  void SerializeMessage(IPC::Message* aMsg) const;
  bool DeserializeMessage(const IPC::Message* aMsg, PickleIterator* aIter);

  void SerializeDOMExceptionInfo(IPC::Message* aMsg) const;
  bool DeserializeDOMExceptionInfo(const IPC::Message* aMsg, PickleIterator* aIter);

  // Helper method that creates a new Message for this TErrorResult,
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

  MOZ_ALWAYS_INLINE void AssertInOwningThread() const {
#ifdef DEBUG
    NS_ASSERT_OWNINGTHREAD(TErrorResult);
#endif
  }

  void AssignErrorCode(nsresult aRv) {
    MOZ_ASSERT(aRv != NS_ERROR_INTERNAL_ERRORRESULT_TYPEERROR,
               "Use ThrowTypeError()");
    MOZ_ASSERT(aRv != NS_ERROR_INTERNAL_ERRORRESULT_RANGEERROR,
               "Use ThrowRangeError()");
    MOZ_ASSERT(!IsErrorWithMessage(), "Don't overwrite errors with message");
    MOZ_ASSERT(aRv != NS_ERROR_INTERNAL_ERRORRESULT_JS_EXCEPTION,
               "Use ThrowJSException()");
    MOZ_ASSERT(!IsJSException(), "Don't overwrite JS exceptions");
    MOZ_ASSERT(aRv != NS_ERROR_INTERNAL_ERRORRESULT_DOMEXCEPTION,
               "Use ThrowDOMException()");
    MOZ_ASSERT(!IsDOMException(), "Don't overwrite DOM exceptions");
    MOZ_ASSERT(aRv != NS_ERROR_XPC_NOT_ENOUGH_ARGS, "May need to bring back ThrowNotEnoughArgsError");
    MOZ_ASSERT(aRv != NS_ERROR_INTERNAL_ERRORRESULT_EXCEPTION_ON_JSCONTEXT,
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

  MOZ_ALWAYS_INLINE void AssertReportedOrSuppressed()
  {
    MOZ_ASSERT(!Failed());
    MOZ_ASSERT(!mMightHaveUnreportedJSException);
    MOZ_ASSERT(mUnionState == HasNothing);
  }

  // Special values of mResult:
  // NS_ERROR_INTERNAL_ERRORRESULT_TYPEERROR -- ThrowTypeError() called on us.
  // NS_ERROR_INTERNAL_ERRORRESULT_RANGEERROR -- ThrowRangeError() called on us.
  // NS_ERROR_INTERNAL_ERRORRESULT_JS_EXCEPTION -- ThrowJSException() called
  //                                               on us.
  // NS_ERROR_UNCATCHABLE_EXCEPTION -- ThrowUncatchableException called on us.
  // NS_ERROR_INTERNAL_ERRORRESULT_DOMEXCEPTION -- ThrowDOMException() called
  //                                               on us.
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

  // The thread that created this TErrorResult
  NS_DECL_OWNINGTHREAD;
#endif

  // Not to be implemented, to make sure people always pass this by
  // reference, not by value.
  TErrorResult(const TErrorResult&) = delete;
  void operator=(const TErrorResult&) = delete;
};

struct JustAssertCleanupPolicy {
  static const bool assertHandled = true;
  static const bool suppress = false;
};

struct AssertAndSuppressCleanupPolicy {
  static const bool assertHandled = true;
  static const bool suppress = true;
};

struct JustSuppressCleanupPolicy {
  static const bool assertHandled = false;
  static const bool suppress = true;
};

} // namespace binding_danger

// A class people should normally use on the stack when they plan to actually
// do something with the exception.
class ErrorResult :
    public binding_danger::TErrorResult<binding_danger::AssertAndSuppressCleanupPolicy>
{
  typedef binding_danger::TErrorResult<binding_danger::AssertAndSuppressCleanupPolicy> BaseErrorResult;

public:
  ErrorResult()
    : BaseErrorResult()
  {}

  ErrorResult(ErrorResult&& aRHS)
    : BaseErrorResult(Move(aRHS))
  {}

  explicit ErrorResult(nsresult aRv)
    : BaseErrorResult(aRv)
  {}

  void operator=(nsresult rv)
  {
    BaseErrorResult::operator=(rv);
  }

  ErrorResult& operator=(ErrorResult&& aRHS)
  {
    BaseErrorResult::operator=(Move(aRHS));
    return *this;
  }

private:
  // Not to be implemented, to make sure people always pass this by
  // reference, not by value.
  ErrorResult(const ErrorResult&) = delete;
  void operator=(const ErrorResult&) = delete;
};

template<typename CleanupPolicy>
binding_danger::TErrorResult<CleanupPolicy>::operator ErrorResult&()
{
  return *static_cast<ErrorResult*>(
     reinterpret_cast<TErrorResult<AssertAndSuppressCleanupPolicy>*>(this));
}

// A class for use when an ErrorResult should just automatically be ignored.
// This doesn't inherit from ErrorResult so we don't make two separate calls to
// SuppressException.
class IgnoredErrorResult :
    public binding_danger::TErrorResult<binding_danger::JustSuppressCleanupPolicy>
{
};

namespace dom {
namespace binding_detail {
class FastErrorResult :
    public mozilla::binding_danger::TErrorResult<
      mozilla::binding_danger::JustAssertCleanupPolicy>
{
};
} // namespace binding_detail
} // namespace dom

// This part is a bit annoying.  We want an OOMReporter class that has the
// following properties:
//
// 1) Can be cast to from any ErrorResult-like type.
// 2) Has a fast destructor (because we want to use it from bindings).
// 3) Won't be randomly instantiated by non-binding code (because the fast
//    destructor is not so safe.
// 4) Doesn't look ugly on the callee side (e.g. isn't in the binding_detail or
//    binding_danger namespace).
//
// We do this by having two classes: The class callees should use, which has the
// things we want and a private constructor, and a friend subclass in the
// binding_danger namespace that can be used to construct it.
namespace binding_danger {
class OOMReporterInstantiator;
} // namespace binding_danger

class OOMReporter : private dom::binding_detail::FastErrorResult
{
public:
  void ReportOOM()
  {
    Throw(NS_ERROR_OUT_OF_MEMORY);
  }

private:
  // OOMReporterInstantiator is a friend so it can call our constructor and
  // MaybeSetPendingException.
  friend class binding_danger::OOMReporterInstantiator;

  // TErrorResult is a friend so its |operator OOMReporter&()| can work.
  template<typename CleanupPolicy>
  friend class binding_danger::TErrorResult;

  OOMReporter()
    : dom::binding_detail::FastErrorResult()
  {
  }
};

namespace binding_danger {
class OOMReporterInstantiator : public OOMReporter
{
public:
  OOMReporterInstantiator()
    : OOMReporter()
  {
  }

  // We want to be able to call MaybeSetPendingException from codegen.  The one
  // on OOMReporter is not callable directly, because it comes from a private
  // superclass.  But we're a friend, so _we_ can call it.
  bool MaybeSetPendingException(JSContext* cx)
  {
    return OOMReporter::MaybeSetPendingException(cx);
  }
};
} // namespace binding_danger

template<typename CleanupPolicy>
binding_danger::TErrorResult<CleanupPolicy>::operator OOMReporter&()
{
  return *static_cast<OOMReporter*>(
     reinterpret_cast<TErrorResult<JustAssertCleanupPolicy>*>(this));
}

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
