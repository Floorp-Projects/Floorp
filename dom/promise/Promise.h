/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Promise_h
#define mozilla_dom_Promise_h

#include <type_traits>
#include <utility>

#include "js/Promise.h"
#include "js/TypeDecls.h"
#include "jspubtd.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

namespace dom {

class AnyCallback;
class MediaStreamError;
class PromiseInit;
class PromiseNativeHandler;
class PromiseDebugging;

#define NS_PROMISE_IID                               \
  {                                                  \
    0x1b8d6215, 0x3e67, 0x43ba, {                    \
      0x8a, 0xf9, 0x31, 0x5e, 0x8f, 0xce, 0x75, 0x65 \
    }                                                \
  }

class Promise : public nsISupports, public SupportsWeakPtr<Promise> {
  friend class PromiseTask;
  friend class PromiseWorkerProxy;
  friend class PromiseWorkerProxyRunnable;

 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PROMISE_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS_FINAL
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Promise)
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(Promise)

  enum PropagateUserInteraction {
    eDontPropagateUserInteraction,
    ePropagateUserInteraction
  };

  // Promise creation tries to create a JS reflector for the Promise, so is
  // fallible.  Furthermore, we don't want to do JS-wrapping on a 0-refcount
  // object, so we addref before doing that and return the addrefed pointer
  // here.
  // Pass ePropagateUserInteraction for aPropagateUserInteraction if you want
  // the promise resolve handler to be called as if we were handling user
  // input events in case we are currently handling user input events.
  static already_AddRefed<Promise> Create(
      nsIGlobalObject* aGlobal, ErrorResult& aRv,
      PropagateUserInteraction aPropagateUserInteraction =
          eDontPropagateUserInteraction);

  // Reports a rejected Promise by sending an error report.
  static void ReportRejectedPromise(JSContext* aCx, JS::HandleObject aPromise);

  typedef void (Promise::*MaybeFunc)(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue);

  // Helpers for using Promise from C++.
  // Most DOM objects are handled already.  To add a new type T, add a
  // ToJSValue overload in ToJSValue.h.
  // aArg is a const reference so we can pass rvalues like integer constants
  template <typename T>
  void MaybeResolve(T&& aArg) {
    MaybeSomething(std::forward<T>(aArg), &Promise::MaybeResolve);
  }

  void MaybeResolveWithUndefined();

  void MaybeReject(JS::Handle<JS::Value> aValue) {
    MaybeSomething(aValue, &Promise::MaybeReject);
  }

  // This method is deprecated.  Consumers should MaybeRejectWithDOMException if
  // they are rejecting with a DOMException, or use one of the other
  // MaybeReject* methods otherwise.  If they have a random nsresult which may
  // or may not correspond to a DOMException type, they should consider using an
  // appropriate DOMException-type nsresult with an informative message and
  // calling MaybeRejectWithDOMException.
  inline void MaybeReject(nsresult aArg) {
    MOZ_ASSERT(NS_FAILED(aArg));
    MaybeSomething(aArg, &Promise::MaybeReject);
  }

  inline void MaybeReject(ErrorResult&& aArg) {
    MOZ_ASSERT(aArg.Failed());
    MaybeSomething(std::move(aArg), &Promise::MaybeReject);
    // That should have consumed aArg.
    MOZ_ASSERT(!aArg.Failed());
  }

  void MaybeReject(const RefPtr<MediaStreamError>& aArg);

  void MaybeRejectWithUndefined();

  void MaybeResolveWithClone(JSContext* aCx, JS::Handle<JS::Value> aValue);
  void MaybeRejectWithClone(JSContext* aCx, JS::Handle<JS::Value> aValue);

  // Facilities for rejecting with various spec-defined exception values.
#define DOMEXCEPTION(name, err)                                   \
  inline void MaybeRejectWith##name(const nsACString& aMessage) { \
    ErrorResult res;                                              \
    res.Throw##name(aMessage);                                    \
    MaybeReject(std::move(res));                                  \
  }                                                               \
  template <int N>                                                \
  void MaybeRejectWith##name(const char(&aMessage)[N]) {          \
    MaybeRejectWith##name(nsLiteralCString(aMessage));            \
  }

#include "mozilla/dom/DOMExceptionNames.h"

#undef DOMEXCEPTION

  template <ErrNum errorNumber, typename... Ts>
  void MaybeRejectWithTypeError(Ts&&... aMessageArgs) {
    ErrorResult res;
    res.ThrowTypeError<errorNumber>(std::forward<Ts>(aMessageArgs)...);
    MaybeReject(std::move(res));
  }

  inline void MaybeRejectWithTypeError(const nsACString& aMessage) {
    ErrorResult res;
    res.ThrowTypeError(aMessage);
    MaybeReject(std::move(res));
  }

  template <int N>
  void MaybeRejectWithTypeError(const char (&aMessage)[N]) {
    MaybeRejectWithTypeError(nsLiteralCString(aMessage));
  }

  template <ErrNum errorNumber, typename... Ts>
  void MaybeRejectWithRangeError(Ts&&... aMessageArgs) {
    ErrorResult res;
    res.ThrowRangeError<errorNumber>(std::forward<Ts>(aMessageArgs)...);
    MaybeReject(std::move(res));
  }

  inline void MaybeRejectWithRangeError(const nsACString& aMessage) {
    ErrorResult res;
    res.ThrowRangeError(aMessage);
    MaybeReject(std::move(res));
  }

  template <int N>
  void MaybeRejectWithRangeError(const char (&aMessage)[N]) {
    MaybeRejectWithRangeError(nsLiteralCString(aMessage));
  }

  // DO NOT USE MaybeRejectBrokenly with in new code.  Promises should be
  // rejected with Error instances.
  // Note: MaybeRejectBrokenly is a template so we can use it with DOMException
  // without instantiating the DOMException specialization of MaybeSomething in
  // every translation unit that includes this header, because that would
  // require use to include DOMException.h either here or in all those
  // translation units.
  template <typename T>
  void MaybeRejectBrokenly(const T& aArg);  // Not implemented by default; see
                                            // specializations in the .cpp for
                                            // the T values we support.

  // Mark a settled promise as already handled so that rejections will not
  // be reported as unhandled.
  void SetSettledPromiseIsHandled() {
    AutoEntryScript aes(mGlobal, "Set settled promise handled");
    JSContext* cx = aes.cx();
    JS::RootedObject promiseObj(cx, mPromiseObj);
    JS::SetSettledPromiseIsHandled(cx, promiseObj);
  }

  // WebIDL

  nsIGlobalObject* GetParentObject() const { return GetGlobalObject(); }

  // Do the equivalent of Promise.resolve in the compartment of aGlobal.  The
  // compartment of aCx is ignored.  Errors are reported on the ErrorResult; if
  // aRv comes back !Failed(), this function MUST return a non-null value.
  // Pass ePropagateUserInteraction for aPropagateUserInteraction if you want
  // the promise resolve handler to be called as if we were handling user
  // input events in case we are currently handling user input events.
  static already_AddRefed<Promise> Resolve(
      nsIGlobalObject* aGlobal, JSContext* aCx, JS::Handle<JS::Value> aValue,
      ErrorResult& aRv,
      PropagateUserInteraction aPropagateUserInteraction =
          eDontPropagateUserInteraction);

  // Do the equivalent of Promise.reject in the compartment of aGlobal.  The
  // compartment of aCx is ignored.  Errors are reported on the ErrorResult; if
  // aRv comes back !Failed(), this function MUST return a non-null value.
  static already_AddRefed<Promise> Reject(nsIGlobalObject* aGlobal,
                                          JSContext* aCx,
                                          JS::Handle<JS::Value> aValue,
                                          ErrorResult& aRv);

  // Do the equivalent of Promise.all in the current compartment of aCx.  Errors
  // are reported on the ErrorResult; if aRv comes back !Failed(), this function
  // MUST return a non-null value.
  // Pass ePropagateUserInteraction for aPropagateUserInteraction if you want
  // the promise resolve handler to be called as if we were handling user
  // input events in case we are currently handling user input events.
  static already_AddRefed<Promise> All(
      JSContext* aCx, const nsTArray<RefPtr<Promise>>& aPromiseList,
      ErrorResult& aRv,
      PropagateUserInteraction aPropagateUserInteraction =
          eDontPropagateUserInteraction);

  void Then(JSContext* aCx,
            // aCalleeGlobal may not be in the compartment of aCx, when called
            // over Xrays.
            JS::Handle<JSObject*> aCalleeGlobal, AnyCallback* aResolveCallback,
            AnyCallback* aRejectCallback, JS::MutableHandle<JS::Value> aRetval,
            ErrorResult& aRv);

  template <typename Callback, typename... Args>
  using IsHandlerCallback =
      std::is_same<already_AddRefed<Promise>,
                   decltype(std::declval<Callback>()(
                       (JSContext*)(nullptr),
                       std::declval<JS::Handle<JS::Value>>(),
                       std::declval<Args>()...))>;

  template <typename Callback, typename... Args>
  using ThenResult =
      std::enable_if_t<IsHandlerCallback<Callback, Args...>::value,
                       Result<RefPtr<Promise>, nsresult>>;

  // Similar to the JavaScript Then() function. Accepts a single lambda function
  // argument, which it attaches as a native resolution handler, and returns a
  // new promise which resolves with that handler's return value, or propagates
  // any rejections from this promise.
  //
  // Any additional arguments passed after the callback function are stored and
  // passed as additional arguments to the function when it is called. These
  // values will participate in cycle collection for the promise handler, and
  // therefore may safely form reference cycles with the promise chain.
  //
  // Any strong references required by the callback should be passed in this
  // manner, rather than using lambda capture, lambda captures do not support
  // cycle collection, and can easily lead to leaks.
  //
  // Does not currently support rejection handlers.
  template <typename Callback, typename... Args>
  ThenResult<Callback, Args...> ThenWithCycleCollectedArgs(
      Callback&& aOnResolve, Args&&... aArgs);

  Result<RefPtr<Promise>, nsresult> ThenWithoutCycleCollection(
      const std::function<
          already_AddRefed<Promise>(JSContext*, JS::HandleValue)>& aCallback);

  JSObject* PromiseObj() const { return mPromiseObj; }

  void AppendNativeHandler(PromiseNativeHandler* aRunnable);

  nsIGlobalObject* GetGlobalObject() const { return mGlobal; }

  // Create a dom::Promise from a given SpiderMonkey Promise object.
  // aPromiseObj MUST be in the compartment of aGlobal's global JS object.
  // Pass ePropagateUserInteraction for aPropagateUserInteraction if you want
  // the promise resolve handler to be called as if we were handling user
  // input events in case we are currently handling user input events.
  static already_AddRefed<Promise> CreateFromExisting(
      nsIGlobalObject* aGlobal, JS::Handle<JSObject*> aPromiseObj,
      PropagateUserInteraction aPropagateUserInteraction =
          eDontPropagateUserInteraction);

  enum class PromiseState { Pending, Resolved, Rejected };

  PromiseState State() const;

 protected:
  // Legacy method for throwing DOMExceptions.  Only used by media code at this
  // point, via DetailedPromise.  Do NOT add new uses!  When this is removed,
  // remove the friend declaration in ErrorResult.h.
  inline void MaybeRejectWithDOMException(nsresult rv,
                                          const nsACString& aMessage) {
    ErrorResult res;
    res.ThrowDOMException(rv, aMessage);
    MaybeReject(std::move(res));
  }

  struct PromiseCapability;

  // Do NOT call this unless you're Promise::Create or
  // Promise::CreateFromExisting.  I wish we could enforce that from inside this
  // class too, somehow.
  explicit Promise(nsIGlobalObject* aGlobal);

  virtual ~Promise();

  // Do JS-wrapping after Promise creation.
  // Pass ePropagateUserInteraction for aPropagateUserInteraction if you want
  // the promise resolve handler to be called as if we were handling user
  // input events in case we are currently handling user input events.
  void CreateWrapper(ErrorResult& aRv,
                     PropagateUserInteraction aPropagateUserInteraction =
                         eDontPropagateUserInteraction);

 private:
  void MaybeResolve(JSContext* aCx, JS::Handle<JS::Value> aValue);
  void MaybeReject(JSContext* aCx, JS::Handle<JS::Value> aValue);

  template <typename T>
  void MaybeSomething(T&& aArgument, MaybeFunc aFunc) {
    MOZ_ASSERT(PromiseObj());  // It was preserved!

    AutoEntryScript aes(mGlobal, "Promise resolution or rejection");
    JSContext* cx = aes.cx();

    JS::Rooted<JS::Value> val(cx);
    if (!ToJSValue(cx, std::forward<T>(aArgument), &val)) {
      HandleException(cx);
      return;
    }

    (this->*aFunc)(cx, val);
  }

  void HandleException(JSContext* aCx);

  bool MaybePropagateUserInputEventHandling();

  RefPtr<nsIGlobalObject> mGlobal;

  JS::Heap<JSObject*> mPromiseObj;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Promise, NS_PROMISE_IID)

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Promise_h
