/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMMozPromiseRequestHolder_h
#define mozilla_dom_DOMMozPromiseRequestHolder_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/MozPromise.h"

namespace mozilla {
namespace dom {

/**
 * This is a helper class that can be used when MozPromises are
 * being consumed by binding layer code.  It effectively creates
 * a MozPromiseRequestHolder that auto-disconnects when the binding's
 * global is disconnected.
 *
 * It can be used like this:
 *
 *    RefPtr<Promise>
 *    SomeAsyncAPI(Args& aArgs, ErrorResult& aRv)
 *    {
 *      nsIGlobalObject* global = GetParentObject();
 *      if (!global) {
 *        aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
 *        return nullptr;
 *      }
 *
 *      RefPtr<Promise> outer = Promise::Create(global, aRv);
 *      if (aRv.Failed()) {
 *        return nullptr;
 *      }
 *
 *      RefPtr<DOMMozPromiseRequestHolder> holder =
 *        new DOMMozPromiseRequestHolder(global);
 *
 *      DoAsyncStuff()->Then(
 *        global->EventTargetFor(TaskCategory::Other), __func__,
 *        [holder, outer] (const Result& aResult) {
 *          holder->Complete();
 *          outer->MaybeResolve(aResult);
 *        }, [holder, outer] (nsresult aRv) {
 *          holder->Complete();
 *          outer->MaybeReject(aRv);
 *        })->Track(*holder);
 *
 *      return outer.forget();
 *    }
 *
 * NOTE: Currently this helper class extends DETH.  This is only
 *       so that it can bind to the global and receive the
 *       DisconnectFromOwner() method call.  In this future the
 *       binding code should be factored out so DETH is not
 *       needed here.  See bug 1456893.
 */
template<typename PromiseType>
class DOMMozPromiseRequestHolder final : public DOMEventTargetHelper
{
  MozPromiseRequestHolder<PromiseType> mHolder;

  ~DOMMozPromiseRequestHolder() = default;

  void
  DisconnectFromOwner() override
  {
    mHolder.DisconnectIfExists();
    DOMEventTargetHelper::DisconnectFromOwner();
  }

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    // We are extending DETH to get notified when the global goes
    // away, but this object should never actually be exposed to
    // script.
    MOZ_CRASH("illegal method");
  }

public:
  explicit DOMMozPromiseRequestHolder(nsIGlobalObject* aGlobal)
    : DOMEventTargetHelper(aGlobal)
  {
    MOZ_DIAGNOSTIC_ASSERT(aGlobal);
  }

  operator MozPromiseRequestHolder<PromiseType>& ()
  {
    return mHolder;
  }

  operator const MozPromiseRequestHolder<PromiseType>& () const
  {
    return mHolder;
  }

  void
  Complete()
  {
    mHolder.Complete();
    DisconnectFromOwner();
  }

  void
  DisconnectIfExists()
  {
    mHolder.DisconnectIfExists();
  }

  bool
  Exists() const
  {
    return mHolder.Exists();
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(DOMMozPromiseRequestHolder, DOMEventTargetHelper)
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DOMMozPromiseRequestHolder_h
