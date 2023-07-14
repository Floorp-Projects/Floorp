/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_ORIGINOPERATIONBASE_H_
#define DOM_QUOTA_ORIGINOPERATIONBASE_H_

#include "ErrorList.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::quota {

class QuotaManager;

// We expect callers to be aware of what thread they're on and either call
// `RunImmediately` if they're on the owning (PBackground) thread or `Dispatch`
// if they are not and therefore the runnable needs to be dispatched to the
// owning thread (PBackground).
class OriginOperationBase : public BackgroundThreadObject, public Runnable {
 protected:
  nsresult mResultCode;

  enum State {
    // Not yet run.
    State_Initial,

    // Running on the owning thread in the listener for OpenDirectory.
    State_DirectoryOpenPending,

    // Running on the IO thread.
    State_DirectoryWorkOpen,

    // Running on the owning thread after all work is done.
    State_UnblockingOpen,

    // All done.
    State_Complete
  };

 private:
  State mState;
  bool mActorDestroyed;

 public:
  void NoteActorDestroyed() {
    AssertIsOnOwningThread();

    mActorDestroyed = true;
  }

  bool IsActorDestroyed() const {
    AssertIsOnOwningThread();

    return mActorDestroyed;
  }

  void RunImmediately() {
    AssertIsOnOwningThread();
    MOZ_ASSERT(GetState() == State_Initial);

    MOZ_ALWAYS_SUCCEEDS(this->Run());
  }

  void Dispatch();

 protected:
  explicit OriginOperationBase(nsISerialEventTarget* aOwningThread,
                               const char* aRunnableName)
      : BackgroundThreadObject(aOwningThread),
        Runnable(aRunnableName),
        mResultCode(NS_OK),
        mState(State_Initial),
        mActorDestroyed(false) {}

  // Reference counted.
  virtual ~OriginOperationBase() {
    MOZ_ASSERT(mState == State_Complete);
    MOZ_ASSERT(mActorDestroyed);
  }

#ifdef DEBUG
  State GetState() const { return mState; }
#endif

  void SetState(State aState) {
    MOZ_ASSERT(mState == State_Initial);
    mState = aState;
  }

  void AdvanceState() {
    switch (mState) {
      case State_Initial:
        mState = State_DirectoryOpenPending;
        return;
      case State_DirectoryOpenPending:
        mState = State_DirectoryWorkOpen;
        return;
      case State_DirectoryWorkOpen:
        mState = State_UnblockingOpen;
        return;
      case State_UnblockingOpen:
        mState = State_Complete;
        return;
      default:
        MOZ_CRASH("Bad state!");
    }
  }

  NS_IMETHOD
  Run() override;

  virtual nsresult DoInit(QuotaManager& aQuotaManager);

  virtual void Open() = 0;

#ifdef DEBUG
  virtual nsresult DirectoryOpen();
#else
  nsresult DirectoryOpen();
#endif

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) = 0;

  void Finish(nsresult aResult);

  virtual void UnblockOpen() = 0;

 private:
  nsresult Init();

  nsresult FinishInit();

  nsresult DirectoryWork();
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_ORIGINOPERATIONBASE_H_
