/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothUtils.h"
#include "DOMRequest.h"
#include "nsContentUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/Promise.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

BluetoothReplyRunnable::BluetoothReplyRunnable(nsIDOMDOMRequest* aReq,
                                               Promise* aPromise)
  : mPromise(aPromise)
  , mDOMRequest(aReq)
  , mErrorStatus(STATUS_FAIL)
{}

void
BluetoothReplyRunnable::SetReply(BluetoothReply* aReply)
{
  mReply.reset(aReply);
}

void
BluetoothReplyRunnable::ReleaseMembers()
{
  mDOMRequest = nullptr;
  mPromise = nullptr;
}

BluetoothReplyRunnable::~BluetoothReplyRunnable()
{}

nsresult
BluetoothReplyRunnable::FireReplySuccess(JS::Handle<JS::Value> aVal)
{
  MOZ_ASSERT(mReply->type() == BluetoothReply::TBluetoothReplySuccess);

  // DOMRequest
  if (mDOMRequest) {
    nsCOMPtr<nsIDOMRequestService> rs =
      do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

    return rs->FireSuccessAsync(mDOMRequest, aVal);
  }

  // Promise
  if (mPromise) {
    mPromise->MaybeResolve(aVal);
  }

  OnSuccessFired();
  return NS_OK;
}

nsresult
BluetoothReplyRunnable::FireErrorString()
{
  // DOMRequest
  if (mDOMRequest) {
    nsCOMPtr<nsIDOMRequestService> rs =
      do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

    return rs->FireErrorAsync(mDOMRequest, mErrorString);
  }

  // Promise
  if (mPromise) {
    nsresult rv =
      NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM_BLUETOOTH, mErrorStatus);
    mPromise->MaybeReject(rv);
  }

  OnErrorFired();
  return NS_OK;
}

NS_IMETHODIMP
BluetoothReplyRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mReply);

  JS::Rooted<JS::Value> v(RootingCx(), JS::UndefinedValue());

  nsresult rv;
  if (mReply->type() != BluetoothReply::TBluetoothReplySuccess) {
    ParseErrorStatus();
    rv = FireErrorString();
  } else if (!ParseSuccessfulReply(&v)) {
    rv = FireErrorString();
  } else {
    rv = FireReplySuccess(v);
  }

  if (NS_FAILED(rv)) {
    BT_WARNING("Could not fire DOMRequest/Promise!");
  }

  ReleaseMembers();
  MOZ_ASSERT(!mDOMRequest,
             "mDOMRequest is still alive! Deriving class should call "
             "BluetoothReplyRunnable::ReleaseMembers()!");
  MOZ_ASSERT(!mPromise,
             "mPromise is still alive! Deriving class should call "
             "BluetoothReplyRunnable::ReleaseMembers()!");

  return rv;
}

void
BluetoothReplyRunnable::OnSuccessFired()
{}

void
BluetoothReplyRunnable::OnErrorFired()
{}

void
BluetoothReplyRunnable::ParseErrorStatus()
{
  MOZ_ASSERT(mReply);
  MOZ_ASSERT(mReply->type() == BluetoothReply::TBluetoothReplyError);

  if (mReply->get_BluetoothReplyError().errorStatus().type() ==
      BluetoothErrorStatus::TBluetoothStatus) {
    SetError(
      mReply->get_BluetoothReplyError().errorString(),
      mReply->get_BluetoothReplyError().errorStatus().get_BluetoothStatus());
  } else {
    SetError(mReply->get_BluetoothReplyError().errorString(), STATUS_FAIL);
  }
}

BluetoothVoidReplyRunnable::BluetoothVoidReplyRunnable(nsIDOMDOMRequest* aReq,
                                                       Promise* aPromise)
  : BluetoothReplyRunnable(aReq, aPromise)
{}

BluetoothVoidReplyRunnable::~BluetoothVoidReplyRunnable()
{}

BluetoothReplyTaskQueue::SubReplyRunnable::SubReplyRunnable(
  nsIDOMDOMRequest* aReq,
  Promise* aPromise,
  BluetoothReplyTaskQueue* aRootQueue)
  : BluetoothReplyRunnable(aReq, aPromise)
  , mRootQueue(aRootQueue)
{}

BluetoothReplyTaskQueue::SubReplyRunnable::~SubReplyRunnable()
{}

BluetoothReplyTaskQueue*
BluetoothReplyTaskQueue::SubReplyRunnable::GetRootQueue() const
{
  return mRootQueue;
}

void
BluetoothReplyTaskQueue::SubReplyRunnable::OnSuccessFired()
{
  mRootQueue->OnSubReplySuccessFired(this);
}

void
BluetoothReplyTaskQueue::SubReplyRunnable::OnErrorFired()
{
  mRootQueue->OnSubReplyErrorFired(this);
}

BluetoothReplyTaskQueue::VoidSubReplyRunnable::VoidSubReplyRunnable(
  nsIDOMDOMRequest* aReq,
  Promise* aPromise,
  BluetoothReplyTaskQueue* aRootQueue)
  : BluetoothReplyTaskQueue::SubReplyRunnable(aReq, aPromise, aRootQueue)
{}

BluetoothReplyTaskQueue::VoidSubReplyRunnable::~VoidSubReplyRunnable()
{}

bool
BluetoothReplyTaskQueue::VoidSubReplyRunnable::ParseSuccessfulReply(
  JS::MutableHandle<JS::Value> aValue)
{
  aValue.setUndefined();
  return true;
}

BluetoothReplyTaskQueue::SubTask::SubTask(
  BluetoothReplyTaskQueue* aRootQueue,
  SubReplyRunnable* aReply)
  : mRootQueue(aRootQueue)
  , mReply(aReply)
{
  if (!mReply) {
    mReply = new VoidSubReplyRunnable(nullptr, nullptr, mRootQueue);
  }
}

BluetoothReplyTaskQueue::SubTask::~SubTask()
{}

BluetoothReplyTaskQueue*
BluetoothReplyTaskQueue::SubTask::GetRootQueue() const
{
  return mRootQueue;
}

BluetoothReplyTaskQueue::SubReplyRunnable*
BluetoothReplyTaskQueue::SubTask::GetReply() const
{
  return mReply;
}

BluetoothReplyTaskQueue::BluetoothReplyTaskQueue(
  BluetoothReplyRunnable* aReply)
  : mReply(aReply)
{}

BluetoothReplyTaskQueue::~BluetoothReplyTaskQueue()
{
  Clear();
}

void
BluetoothReplyTaskQueue::AppendTask(already_AddRefed<SubTask> aTask)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<SubTask> task(aTask);

  if (task) {
    mTasks.AppendElement(task.forget());
  }
}

NS_IMETHODIMP
BluetoothReplyTaskQueue::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mTasks.IsEmpty()) {
    RefPtr<SubTask> task = mTasks[0];
    mTasks.RemoveElementAt(0);

    MOZ_ASSERT(task);

    if (!task->Execute()) {
      FireErrorReply();
    }
  }

  return NS_OK;
}

void
BluetoothReplyTaskQueue::OnSubReplySuccessFired(SubReplyRunnable* aSubReply)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSubReply);

  if (mTasks.IsEmpty()) {
    FireSuccessReply();
  } else {
    if (NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(this)))) {
      FireErrorReply();
    }
  }
}

void
BluetoothReplyTaskQueue::OnSubReplyErrorFired(SubReplyRunnable* aSubReply)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSubReply);

  FireErrorReply();
}

void
BluetoothReplyTaskQueue::FireSuccessReply()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mReply) {
    DispatchReplySuccess(mReply);
  }
  OnSuccessFired();
  Clear();
}

void
BluetoothReplyTaskQueue::FireErrorReply()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mReply) {
    DispatchReplyError(mReply, STATUS_FAIL);
  }
  OnErrorFired();
  Clear();
}

void
BluetoothReplyTaskQueue::OnSuccessFired()
{}

void
BluetoothReplyTaskQueue::OnErrorFired()
{}

void
BluetoothReplyTaskQueue::Clear()
{
  MOZ_ASSERT(NS_IsMainThread());

  mReply = nullptr;
  mTasks.Clear();
}
