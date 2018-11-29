/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsChild.h"

#include "LSDatabase.h"

namespace mozilla {
namespace dom {

/*******************************************************************************
 * LSDatabaseChild
 ******************************************************************************/

LSDatabaseChild::LSDatabaseChild(LSDatabase* aDatabase)
  : mDatabase(aDatabase)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabase);

  MOZ_COUNT_CTOR(LSDatabaseChild);
}

LSDatabaseChild::~LSDatabaseChild()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(LSDatabaseChild);
}

void
LSDatabaseChild::SendDeleteMeInternal()
{
  AssertIsOnOwningThread();

  if (mDatabase) {
    mDatabase->ClearActor();
    mDatabase = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundLSDatabaseChild::SendDeleteMe());
  }
}

void
LSDatabaseChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  if (mDatabase) {
    mDatabase->ClearActor();
#ifdef DEBUG
    mDatabase = nullptr;
#endif
  }
}

mozilla::ipc::IPCResult
LSDatabaseChild::RecvRequestAllowToClose()
{
  AssertIsOnOwningThread();

  if (mDatabase) {
    mDatabase->AllowToClose();

    // TODO: A new datastore will be prepared at first LocalStorage API
    //       synchronous call. It would be better to start preparing a new
    //       datastore right here, but asynchronously.
    //       However, we probably shouldn't do that if we are shutting down.
  }

  return IPC_OK();
}

/*******************************************************************************
 * LocalStorageRequestChild
 ******************************************************************************/

LSRequestChild::LSRequestChild(LSRequestChildCallback* aCallback)
  : mCallback(aCallback)
  , mFinishing(false)
{
  AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(LSRequestChild);
}

LSRequestChild::~LSRequestChild()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(LSRequestChild);
}

bool
LSRequestChild::Finishing() const
{
  AssertIsOnOwningThread();

  return mFinishing;
}

void
LSRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();
}

mozilla::ipc::IPCResult
LSRequestChild::Recv__delete__(const LSRequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCallback);

  mCallback->OnResponse(aResponse);

  return IPC_OK();
}

mozilla::ipc::IPCResult
LSRequestChild::RecvReady()
{
  AssertIsOnOwningThread();

  mFinishing = true;

  SendFinish();

  return IPC_OK();
}

} // namespace dom
} // namespace mozilla
