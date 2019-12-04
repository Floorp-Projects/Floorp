/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idlerequest_h
#define mozilla_dom_idlerequest_h

#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMNavigationTiming.h"
#include "nsICancelableRunnable.h"
#include "nsIRunnable.h"
#include "nsString.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class IdleRequestCallback;

class IdleRequest final : public LinkedListElement<RefPtr<IdleRequest>> {
 public:
  IdleRequest(IdleRequestCallback* aCallback, uint32_t aHandle);

  MOZ_CAN_RUN_SCRIPT
  void IdleRun(nsPIDOMWindowInner* aWindow, DOMHighResTimeStamp aDeadline,
               bool aDidTimeout);

  void SetTimeoutHandle(int32_t aHandle);
  bool HasTimeout() const { return mTimeoutHandle.isSome(); }
  uint32_t GetTimeoutHandle() const;

  uint32_t Handle() const { return mHandle; }

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(IdleRequest)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(IdleRequest)
 private:
  ~IdleRequest();

  RefPtr<IdleRequestCallback> mCallback;
  const uint32_t mHandle;
  mozilla::Maybe<int32_t> mTimeoutHandle;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idlerequest_h
