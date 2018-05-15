/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Promise_h
#define mozilla_dom_Promise_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/WeakPtr.h"
#include "nsWrapperCache.h"
#include "nsAutoPtr.h"
#include "js/TypeDecls.h"
#include "jspubtd.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class AnyCallback;
class MediaStreamError;
class PromiseInit;
class PromiseNativeHandler;
class PromiseDebugging;

class Promise : public SupportsWeakPtr<Promise>
{
  friend class PromiseTask;
  friend class PromiseWorkerProxy;
  friend class PromiseWorkerProxyRunnable;

public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(Promise)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(Promise)
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(Promise)

  // Promise creation tries to create a JS reflector for the Promise, so is
  // fallible.  Furthermore, we don't want to do JS-wrapping on a 0-refcount
  // object, so we addref before doing that and return the addrefed pointer
  // here.
  static already_AddRefed<Promise>
  Create(nsIGlobalObject* aGlobal, ErrorResult& aRv);

  // Reports a rejected Promise by sending an error report.
  static void ReportRejectedPromise(JSContext* aCx, JS::HandleObject aPromise);

  typedef void (Promise::*MaybeFunc)(JSContext* aCx,
                                     JS::Handle<JS::Value> aValue);

  void MaybeResolve(JSContext* aCx,
                    JS::Handle<JS::Value> aValue);
  void MaybeReject(JSContext* aCx,
                   JS::Handle<JS::Value> aValue);

  // Helpers for using Promise from C++.
  // Most DOM objects are handled already.  To add a new type T, add a
  // ToJSValue overload in ToJSValue.h.
  // aArg is a const reference so we can pass rvalues like integer constants
  template <typename T>
  void MaybeResolve(const T& aArg) {
    MaybeSomething(aArg, &Promise::MaybeResolve);
  }

  void MaybeResolveWithUndefined();

  inline void MaybeReject(nsresult aArg) {
    MOZ_ASSERT(NS_FAILED(aArg));
    MaybeSomething(aArg, &Promise::MaybeReject);
  }

  inline void MaybeReject(ErrorResult& aArg) {
    MOZ_ASSERT(aArg.Failed());
    MaybeSomething(aArg, &Promise::MaybeReject);
  }

  void MaybeReject(const RefPtr<MediaStreamError>& aArg);

  void MaybeRejectWithUndefined();

  // DO NOT USE MaybeRejectBrokenly with in new code.  Promises should be
  // rejected with Error instances.
  // Note: MaybeRejectBrokenly is a template so we can use it with DOMException
  // without instantiating the DOMException specialization of MaybeSomething in
  // every translation unit that includes this header, because that would
  // require use to include DOMException.h either here or in all those
  // translation units.
  template<typename T>
  void MaybeRejectBrokenly(const T& aArg); // Not implemented by default; see
                                           // specializations in the .cpp for
                                           // the T values we support.

  // WebIDL

  nsIGlobalObject* GetParentObject() const
  {
    return mGlobal;
  }

  // Do the equivalent of Promise.resolve in the compartment of aGlobal.  The
  // compartment of aCx is ignored.  Errors are reported on the ErrorResult; if
  // aRv comes back !Failed(), this function MUST return a non-null value.
  static already_AddRefed<Promise>
  Resolve(nsIGlobalObject* aGlobal, JSContext* aCx,
          JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  // Do the equivalent of Promise.reject in the compartment of aGlobal.  The
  // compartment of aCx is ignored.  Errors are reported on the ErrorResult; if
  // aRv comes back !Failed(), this function MUST return a non-null value.
  static already_AddRefed<Promise>
  Reject(nsIGlobalObject* aGlobal, JSContext* aCx,
         JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  // Do the equivalent of Promise.all in the current compartment of aCx.  Errors
  // are reported on the ErrorResult; if aRv comes back !Failed(), this function
  // MUST return a non-null value.
  static already_AddRefed<Promise>
  All(JSContext* aCx, const nsTArray<RefPtr<Promise>>& aPromiseList,
      ErrorResult& aRv);

  void
  Then(JSContext* aCx,
       // aCalleeGlobal may not be in the compartment of aCx, when called over
       // Xrays.
       JS::Handle<JSObject*> aCalleeGlobal,
       AnyCallback* aResolveCallback, AnyCallback* aRejectCallback,
       JS::MutableHandle<JS::Value> aRetval,
       ErrorResult& aRv);

  JSObject* PromiseObj() const
  {
    return mPromiseObj;
  }

  void AppendNativeHandler(PromiseNativeHandler* aRunnable);

  JSObject* GlobalJSObject() const;

  JSCompartment* Compartment() const;

  // Create a dom::Promise from a given SpiderMonkey Promise object.
  // aPromiseObj MUST be in the compartment of aGlobal's global JS object.
  static already_AddRefed<Promise>
  CreateFromExisting(nsIGlobalObject* aGlobal,
                     JS::Handle<JSObject*> aPromiseObj);

  enum class PromiseState {
    Pending,
    Resolved,
    Rejected
  };

  PromiseState State() const;

protected:
  struct PromiseCapability;

  // Do NOT call this unless you're Promise::Create or
  // Promise::CreateFromExisting.  I wish we could enforce that from inside this
  // class too, somehow.
  explicit Promise(nsIGlobalObject* aGlobal);

  virtual ~Promise();

  // Do JS-wrapping after Promise creation.  Passing null for aDesiredProto will
  // use the default prototype for the sort of Promise we have.
  void CreateWrapper(JS::Handle<JSObject*> aDesiredProto, ErrorResult& aRv);

private:
  template <typename T>
  void MaybeSomething(T& aArgument, MaybeFunc aFunc) {
    MOZ_ASSERT(PromiseObj()); // It was preserved!

    AutoEntryScript aes(mGlobal, "Promise resolution or rejection");
    JSContext* cx = aes.cx();

    JS::Rooted<JS::Value> val(cx);
    if (!ToJSValue(cx, aArgument, &val)) {
      HandleException(cx);
      return;
    }

    (this->*aFunc)(cx, val);
  }

  void HandleException(JSContext* aCx);

  RefPtr<nsIGlobalObject> mGlobal;

  JS::Heap<JSObject*> mPromiseObj;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Promise_h
