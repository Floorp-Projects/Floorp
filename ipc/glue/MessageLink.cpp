/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/MessageLink.h"
#include "mojo/core/ports/event.h"
#include "mojo/core/ports/node.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/NodeController.h"
#include "chrome/common/ipc_channel.h"
#include "base/task.h"

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "nsDebug.h"
#include "nsExceptionHandler.h"
#include "nsISupportsImpl.h"
#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"

using namespace mozilla;

namespace mozilla {
namespace ipc {

const char* StringFromIPCSide(Side side) {
  switch (side) {
    case ChildSide:
      return "Child";
    case ParentSide:
      return "Parent";
    default:
      return "Unknown";
  }
}

MessageLink::MessageLink(MessageChannel* aChan) : mChan(aChan) {}

MessageLink::~MessageLink() {
#ifdef DEBUG
  mChan = nullptr;
#endif
}

class PortLink::PortObserverThunk : public NodeController::PortObserver {
 public:
  PortObserverThunk(RefCountedMonitor* aMonitor, PortLink* aLink)
      : mMonitor(aMonitor), mLink(aLink) {}

  void OnPortStatusChanged() override {
    MonitorAutoLock lock(*mMonitor);
    if (mLink) {
      mLink->OnPortStatusChanged();
    }
  }

 private:
  friend class PortLink;

  // The monitor from our PortLink's MessageChannel. Guards access to `mLink`.
  RefPtr<RefCountedMonitor> mMonitor;

  // Cleared by `PortLink` in `PortLink::Clear()`.
  PortLink* MOZ_NON_OWNING_REF mLink;
};

PortLink::PortLink(MessageChannel* aChan, ScopedPort aPort)
    : MessageLink(aChan), mNode(aPort.Controller()), mPort(aPort.Release()) {
  mChan->mMonitor->AssertCurrentThreadOwns();

  mObserver = new PortObserverThunk(mChan->mMonitor, this);
  mNode->SetPortObserver(mPort, mObserver);

  // Dispatch an event to the IO loop to trigger an initial
  // `OnPortStatusChanged` to deliver any pending messages. This needs to be run
  // asynchronously from a different thread (or in the case of a same-thread
  // channel, from the current thread), for now due to assertions in
  // `MessageChannel`.
  nsCOMPtr<nsIRunnable> openRunnable = NewRunnableMethod(
      "PortLink::Open", mObserver, &PortObserverThunk::OnPortStatusChanged);
  if (aChan->mIsSameThreadChannel) {
    aChan->mWorkerThread->Dispatch(openRunnable.forget());
  } else {
    XRE_GetIOMessageLoop()->PostTask(openRunnable.forget());
  }
}

PortLink::~PortLink() {
  MOZ_RELEASE_ASSERT(!mObserver, "PortLink destroyed without being closed!");
}

void PortLink::SendMessage(UniquePtr<Message> aMessage) {
  mChan->mMonitor->AssertCurrentThreadOwns();

  if (aMessage->size() > IPC::Channel::kMaximumMessageSize) {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::IPCMessageName,
        nsDependentCString(aMessage->name()));
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::IPCMessageSize,
        static_cast<unsigned int>(aMessage->size()));
    MOZ_CRASH("IPC message size is too large");
  }
  aMessage->AssertAsLargeAsHeader();

  RefPtr<PortObserverThunk> observer = mObserver;
  if (!observer) {
    NS_WARNING("Ignoring message to closed PortLink");
    return;
  }

  // Make local copies of relevant member variables, so we can unlock the
  // monitor for the rest of this function. This protects us in case `this` is
  // deleted during the call (although that shouldn't happen in practice).
  //
  // We don't want the monitor to be held when calling into ports, as we may be
  // re-entrantly called by our `PortObserverThunk` which will attempt to
  // acquire the monitor.
  RefPtr<RefCountedMonitor> monitor = mChan->mMonitor;
  RefPtr<NodeController> node = mNode;
  PortRef port = mPort;

  bool ok = false;
  monitor->AssertCurrentThreadOwns();
  {
    MonitorAutoUnlock guard(*monitor);
    ok = node->SendUserMessage(port, std::move(aMessage));
  }
  if (!ok) {
    // The send failed, but double-check that we weren't closed racily while
    // sending, which could lead to an invalid state error.
    if (observer->mLink) {
      MOZ_CRASH("Invalid argument to SendUserMessage");
    }
    NS_WARNING("Message dropped as PortLink was closed");
  }
}

void PortLink::Close() {
  mChan->mMonitor->AssertCurrentThreadOwns();

  if (!mObserver) {
    // We're already being closed.
    return;
  }

  Clear();
}

void PortLink::Clear() {
  mChan->mMonitor->AssertCurrentThreadOwns();

  // NOTE: We're calling into `ports` with our monitor held! Usually, this could
  // lead to deadlocks due to the PortObserverThunk acquiring the lock
  // re-entrantly, but is OK here as we're immediately clearing the port's
  // observer. We shouldn't have issues with any re-entrant calls on this thread
  // acquiring this MessageChannel's monitor.
  //
  // We also clear out the reference in `mObserver` back to this type so that
  // notifications from other threads won't try to call us again once we release
  // the monitor.
  mNode->SetPortObserver(mPort, nullptr);
  mObserver->mLink = nullptr;
  mObserver = nullptr;
  mNode->ClosePort(mPort);
}

void PortLink::OnPortStatusChanged() {
  mChan->mMonitor->AssertCurrentThreadOwns();

  // Check if the port's remoteness status has updated, and tell our channel if
  // it has.
  if (Maybe<PortStatus> status = mNode->GetStatus(mPort);
      status && status->peer_remote != mChan->IsCrossProcess()) {
    mChan->SetIsCrossProcess(status->peer_remote);
  }

  while (mObserver) {
    UniquePtr<IPC::Message> message;
    if (!mNode->GetMessage(mPort, &message)) {
      Clear();
      mChan->OnChannelErrorFromLink();
      return;
    }
    if (!message) {
      return;
    }

    mChan->OnMessageReceivedFromLink(std::move(message));
  }
}

bool PortLink::IsClosed() const {
  if (Maybe<PortStatus> status = mNode->GetStatus(mPort)) {
    return !(status->has_messages || status->receiving_messages);
  }
  return true;
}

}  // namespace ipc
}  // namespace mozilla
