/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothChild.h"

#include "mozilla/Assertions.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"
#include "nsTraceRefcnt.h"

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothServiceChildProcess.h"

USING_BLUETOOTH_NAMESPACE

namespace {

BluetoothServiceChildProcess* gBluetoothService;

} // anonymous namespace

/*******************************************************************************
 * BluetoothChild
 ******************************************************************************/

BluetoothChild::BluetoothChild(BluetoothServiceChildProcess* aBluetoothService)
: mShutdownState(Running)
{
  MOZ_COUNT_CTOR(BluetoothChild);
  MOZ_ASSERT(!gBluetoothService);
  MOZ_ASSERT(aBluetoothService);

  gBluetoothService = aBluetoothService;
}

BluetoothChild::~BluetoothChild()
{
  MOZ_COUNT_DTOR(BluetoothChild);
  MOZ_ASSERT(gBluetoothService);
  MOZ_ASSERT(mShutdownState == Dead);

  gBluetoothService = nullptr;
}

void
BluetoothChild::BeginShutdown()
{
  // Only do something here if we haven't yet begun the shutdown sequence.
  if (mShutdownState == Running) {
    SendStopNotifying();
    mShutdownState = SentStopNotifying;
  }
}

void
BluetoothChild::ActorDestroy(ActorDestroyReason aWhy)
{
  MOZ_ASSERT(gBluetoothService);

  gBluetoothService->NoteDeadActor();

#ifdef DEBUG
  mShutdownState = Dead;
#endif
}

bool
BluetoothChild::RecvNotify(const BluetoothSignal& aSignal)
{
  MOZ_ASSERT(gBluetoothService);

  gBluetoothService->DistributeSignal(aSignal);
  return true;
}

bool
BluetoothChild::RecvEnabled(const bool& aEnabled)
{
  MOZ_ASSERT(gBluetoothService);

  gBluetoothService->SetEnabled(aEnabled);
  return true;
}

bool
BluetoothChild::RecvBeginShutdown()
{
  if (mShutdownState != Running && mShutdownState != SentStopNotifying) {
    MOZ_ASSERT(false, "Bad state!");
    return false;
  }

  SendStopNotifying();
  mShutdownState = SentStopNotifying;

  return true;
}

bool
BluetoothChild::RecvNotificationsStopped()
{
  if (mShutdownState != SentStopNotifying) {
    MOZ_ASSERT(false, "Bad state!");
    return false;
  }

  Send__delete__(this);
  return true;
}

PBluetoothRequestChild*
BluetoothChild::AllocPBluetoothRequest(const Request& aRequest)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
  return nullptr;
}

bool
BluetoothChild::DeallocPBluetoothRequest(PBluetoothRequestChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * BluetoothRequestChild
 ******************************************************************************/

BluetoothRequestChild::BluetoothRequestChild(
                                         BluetoothReplyRunnable* aReplyRunnable)
: mReplyRunnable(aReplyRunnable)
{
  MOZ_COUNT_CTOR(BluetoothRequestChild);
  MOZ_ASSERT(aReplyRunnable);
}

BluetoothRequestChild::~BluetoothRequestChild()
{
  MOZ_COUNT_DTOR(BluetoothRequestChild);
}

void
BluetoothRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  // Nothing needed here.
}

bool
BluetoothRequestChild::Recv__delete__(const BluetoothReply& aReply)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mReplyRunnable);

  nsRefPtr<BluetoothReplyRunnable> replyRunnable;
  mReplyRunnable.swap(replyRunnable);

  if (replyRunnable) {
    // XXXbent Need to fix this, it copies unnecessarily.
    replyRunnable->SetReply(new BluetoothReply(aReply));
    return NS_SUCCEEDED(NS_DispatchToCurrentThread(replyRunnable));
  }

  return true;
}
