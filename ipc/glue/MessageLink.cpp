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

// We rely on invariants about the lifetime of the transport:
//
//  - outlives this MessageChannel
//  - deleted on the IO thread
//
// These invariants allow us to send messages directly through the
// transport without having to worry about orphaned Send() tasks on
// the IO thread touching MessageChannel memory after it's been deleted
// on the worker thread.  We also don't need to refcount the
// Transport, because whatever task triggers its deletion only runs on
// the IO thread, and only runs after this MessageChannel is done with
// the Transport.

namespace mozilla {
namespace ipc {

MessageLink::MessageLink(MessageChannel* aChan) : mChan(aChan) {}

MessageLink::~MessageLink() {
#ifdef DEBUG
  mChan = nullptr;
#endif
}

ThreadLink::ThreadLink(MessageChannel* aChan, MessageChannel* aTargetChan)
    : MessageLink(aChan), mTargetChan(aTargetChan) {}

void ThreadLink::PrepareToDestroy() {
  MOZ_ASSERT(mChan);
  MOZ_ASSERT(mChan->mMonitor);
  MonitorAutoLock lock(*mChan->mMonitor);

  // Bug 848949: We need to prevent the other side
  // from sending us any more messages to avoid Use-After-Free.
  // The setup here is as shown:
  //
  //          (Us)         (Them)
  //       MessageChannel  MessageChannel
  //         |  ^     \ /     ^ |
  //         |  |      X      | |
  //         v  |     / \     | v
  //        ThreadLink   ThreadLink
  //
  // We want to null out the diagonal link from their ThreadLink
  // to our MessageChannel.  Note that we must hold the monitor so
  // that we do this atomically with respect to them trying to send
  // us a message.  Since the channels share the same monitor this
  // also protects against the two PrepareToDestroy() calls racing.
  //
  //
  // Why splitting is done in a method separate from ~ThreadLink:
  //
  // ThreadLinks are destroyed in MessageChannel::Clear(), when
  // nullptr is assigned to the UniquePtr<> MessageChannel::mLink.
  // This single line of code gets executed in three separate steps:
  // 1. Load the value of mLink into a temporary.
  // 2. Store nullptr in the mLink field.
  // 3. Call the destructor on the temporary from step 1.
  // This is all done without holding the monitor.
  // The splitting operation, among other things, loads the mLink field
  // of the other thread's MessageChannel while holding the monitor.
  // If splitting was done in the destructor, and the two sides were
  // both running MessageChannel::Clear(), then there would be a race
  // between the store to mLink in Clear() and the load of mLink
  // during splitting.
  // Instead, we call PrepareToDestroy() prior to step 1. One thread or
  // the other will run the entire method before the other thread,
  // because this method acquires the monitor. Once that is done, the
  // mTargetChan of both ThreadLink will be null, so they will no
  // longer be able to access the other and so there won't be any races.
  //
  // An alternate approach would be to hold the monitor in Clear() or
  // make mLink atomic, but MessageLink does not have to worry about
  // Clear() racing with Clear(), so it would be inefficient.
  if (mTargetChan) {
    MOZ_ASSERT(mTargetChan->mLink);
    static_cast<ThreadLink*>(mTargetChan->mLink.get())->mTargetChan = nullptr;
  }
  mTargetChan = nullptr;
}

void ThreadLink::SendMessage(UniquePtr<Message> msg) {
  if (!mChan->mIsPostponingSends) {
    mChan->AssertWorkerThread();
  }
  mChan->mMonitor->AssertCurrentThreadOwns();

  if (mTargetChan) mTargetChan->OnMessageReceivedFromLink(std::move(*msg));
}

void ThreadLink::SendClose() {
  mChan->AssertWorkerThread();
  mChan->mMonitor->AssertCurrentThreadOwns();

  mChan->mChannelState = ChannelClosed;

  // In a ProcessLink, we would close our half the channel.  This
  // would show up on the other side as an error on the I/O thread.
  // The I/O thread would then invoke OnChannelErrorFromLink().
  // As usual, we skip that process and just invoke the
  // OnChannelErrorFromLink() method directly.
  if (mTargetChan) mTargetChan->OnChannelErrorFromLink();
}

bool ThreadLink::Unsound_IsClosed() const {
  MonitorAutoLock lock(*mChan->mMonitor);
  return mChan->mChannelState == ChannelClosed;
}

uint32_t ThreadLink::Unsound_NumQueuedMessages() const {
  // ThreadLinks don't have a message queue.
  return 0;
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
  mObserver = new PortObserverThunk(mChan->mMonitor, this);
  mNode->SetPortObserver(mPort, mObserver);

  mChan->mChannelState = ChannelConnected;

  // Dispatch an event to the IO loop to trigger an initial
  // `OnPortStatusChanged` to deliver any pending messages. This needs to be run
  // asynchronously from a different thread for now due to assertions in
  // `MessageChannel`.
  XRE_GetIOMessageLoop()->PostTask(NewRunnableMethod(
      "PortLink::Open", mObserver, &PortObserverThunk::OnPortStatusChanged));
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

void PortLink::SendClose() {
  mChan->mMonitor->AssertCurrentThreadOwns();

  // Our channel has been closed, mark it as such.
  mChan->mChannelState = ChannelClosed;
  mChan->mMonitor->Notify();

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

    mChan->OnMessageReceivedFromLink(std::move(*message));
  }
}

bool PortLink::Unsound_IsClosed() const {
  if (Maybe<PortStatus> status = mNode->GetStatus(mPort)) {
    return !(status->has_messages || status->receiving_messages);
  }
  return true;
}

uint32_t PortLink::Unsound_NumQueuedMessages() const {
  // There is no easy way to see the number of messages which have been sent to
  // a port but haven't been delivered yet.
  //
  // FIXME: If this is important, we'll need to add a mechanism for this.
  return 0;
}

}  // namespace ipc
}  // namespace mozilla
