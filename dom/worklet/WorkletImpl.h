/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_worklet_WorkletImpl_h
#define mozilla_dom_worklet_WorkletImpl_h

#include "MainThreadUtils.h"
#include "mozilla/OriginAttributes.h"

class nsPIDOMWindowInner;
class nsIPrincipal;
class nsIRunnable;

namespace mozilla {

namespace dom {

class Worklet;
class WorkletGlobalScope;
class WorkletThread;

}  // namespace dom

class WorkletLoadInfo {
 public:
  explicit WorkletLoadInfo(nsPIDOMWindowInner* aWindow);

  uint64_t OuterWindowID() const { return mOuterWindowID; }
  uint64_t InnerWindowID() const { return mInnerWindowID; }

 private:
  // Modified only in constructor.
  uint64_t mOuterWindowID;
  const uint64_t mInnerWindowID;
};

/**
 * WorkletImpl is accessed from both the worklet's parent thread (on which the
 * Worklet object lives) and the worklet's execution thread.  It is owned by
 * Worklet and WorkletGlobalScope.  No parent thread cycle collected objects
 * are owned indefinitely by WorkletImpl because WorkletImpl is not cycle
 * collected.
 */
class WorkletImpl {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkletImpl);

  // Methods for parent thread only:

  virtual JSObject* WrapWorklet(JSContext* aCx, dom::Worklet* aWorklet,
                                JS::Handle<JSObject*> aGivenProto);

  virtual nsresult SendControlMessage(already_AddRefed<nsIRunnable> aRunnable);

  nsIPrincipal* Principal() const {
    MOZ_ASSERT(NS_IsMainThread());
    return mPrincipal;
  }

  void NotifyWorkletFinished();

  // Execution thread only.
  dom::WorkletGlobalScope* GetGlobalScope();

  // Any thread.

  const WorkletLoadInfo& LoadInfo() const { return mWorkletLoadInfo; }
  const OriginAttributes& OriginAttributesRef() const {
    return mOriginAttributes;
  }

 protected:
  WorkletImpl(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal);
  virtual ~WorkletImpl();

  virtual already_AddRefed<dom::WorkletGlobalScope> ConstructGlobalScope() = 0;

  const OriginAttributes mOriginAttributes;
  // Accessed on only worklet parent thread.
  nsCOMPtr<nsIPrincipal> mPrincipal;

  const WorkletLoadInfo mWorkletLoadInfo;

  // Parent thread only.
  RefPtr<dom::WorkletThread> mWorkletThread;
  bool mTerminated;

  // Execution thread only.
  RefPtr<dom::WorkletGlobalScope> mGlobalScope;
};

}  // namespace mozilla

#endif  // mozilla_dom_worklet_WorkletImpl_h
