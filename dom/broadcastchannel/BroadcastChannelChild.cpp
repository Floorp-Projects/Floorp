/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BroadcastChannelChild.h"
#include "BroadcastChannel.h"
#include "jsapi.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "WorkerPrivate.h"

namespace mozilla {

using namespace ipc;

namespace dom {

using namespace workers;

BroadcastChannelChild::BroadcastChannelChild(const nsACString& aOrigin)
  : mBC(nullptr)
  , mActorDestroyed(false)
{
  CopyUTF8toUTF16(aOrigin, mOrigin);
}

BroadcastChannelChild::~BroadcastChannelChild()
{
  MOZ_ASSERT(!mBC);
}

mozilla::ipc::IPCResult
BroadcastChannelChild::RecvNotify(const ClonedMessageData& aData)
{
  // Make sure to retrieve all blobs from the message before returning to avoid
  // leaking their actors.
  ipc::StructuredCloneDataNoTransfers cloneData;
  cloneData.BorrowFromClonedMessageDataForBackgroundChild(aData);

  nsCOMPtr<DOMEventTargetHelper> helper = mBC;
  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(helper);

  // The object is going to be deleted soon. No notify is required.
  if (!eventTarget) {
    return IPC_OK();
  }

  // CheckInnerWindowCorrectness can be used also without a window when
  // BroadcastChannel is running in a worker. In this case, it's a NOP.
  if (NS_FAILED(mBC->CheckInnerWindowCorrectness())) {
    return IPC_OK();
  }

  mBC->RemoveDocFromBFCache();

  AutoJSAPI jsapi;
  nsCOMPtr<nsIGlobalObject> globalObject;

  if (NS_IsMainThread()) {
    globalObject = do_QueryInterface(mBC->GetParentObject());
  } else {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    globalObject = workerPrivate->GlobalScope();
  }

  if (!globalObject || !jsapi.Init(globalObject)) {
    NS_WARNING("Failed to initialize AutoJSAPI object.");
    return IPC_OK();
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> value(cx, JS::NullValue());
  if (cloneData.DataLength()) {
    ErrorResult rv;
    cloneData.Read(cx, &value, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      return IPC_OK();
    }
  }

  RootedDictionary<MessageEventInit> init(cx);
  init.mBubbles = false;
  init.mCancelable = false;
  init.mOrigin = mOrigin;
  init.mData = value;

  ErrorResult rv;
  RefPtr<MessageEvent> event =
    MessageEvent::Constructor(mBC, NS_LITERAL_STRING("message"), init, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return IPC_OK();
  }

  event->SetTrusted(true);

  bool status;
  mBC->DispatchEvent(static_cast<Event*>(event.get()), &status);

  return IPC_OK();
}

void
BroadcastChannelChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
}

} // namespace dom
} // namespace mozilla
