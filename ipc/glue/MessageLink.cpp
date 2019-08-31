/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/MessageLink.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "chrome/common/ipc_channel.h"

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

ProcessLink::ProcessLink(MessageChannel* aChan)
    : MessageLink(aChan),
      mTransport(nullptr),
      mIOLoop(nullptr),
      mExistingListener(nullptr) {}

ProcessLink::~ProcessLink() {
#ifdef DEBUG
  mTransport = nullptr;
  mIOLoop = nullptr;
  mExistingListener = nullptr;
#endif
}

void ProcessLink::Open(mozilla::ipc::Transport* aTransport,
                       MessageLoop* aIOLoop, Side aSide) {
  mChan->AssertWorkerThread();

  MOZ_ASSERT(aTransport, "need transport layer");

  // FIXME need to check for valid channel

  mTransport = aTransport;

  // FIXME figure out whether we're in parent or child, grab IO loop
  // appropriately
  bool needOpen = true;
  if (aIOLoop) {
    // We're a child or using the new arguments.  Either way, we
    // need an open.
    needOpen = true;
    mChan->mSide = (aSide == UnknownSide) ? ChildSide : aSide;
  } else {
    MOZ_ASSERT(aSide == UnknownSide, "expected default side arg");

    // parent
    mChan->mSide = ParentSide;
    needOpen = false;
    aIOLoop = XRE_GetIOMessageLoop();
  }

  mIOLoop = aIOLoop;

  NS_ASSERTION(mIOLoop, "need an IO loop");
  NS_ASSERTION(mChan->mWorkerLoop, "need a worker loop");

  // If we were never able to open the transport, immediately post an error
  // message.
  if (mTransport->Unsound_IsClosed()) {
    mIOLoop->PostTask(
        NewNonOwningRunnableMethod("ipc::ProcessLink::OnChannelConnectError",
                                   this, &ProcessLink::OnChannelConnectError));
    return;
  }

  {
    MonitorAutoLock lock(*mChan->mMonitor);

    if (needOpen) {
      // Transport::Connect() has not been called.  Call it so
      // we start polling our pipe and processing outgoing
      // messages.
      mIOLoop->PostTask(
          NewNonOwningRunnableMethod("ipc::ProcessLink::OnChannelOpened", this,
                                     &ProcessLink::OnChannelOpened));
    } else {
      // Transport::Connect() has already been called.  Take
      // over the channel from the previous listener and process
      // any queued messages.
      mIOLoop->PostTask(NewNonOwningRunnableMethod(
          "ipc::ProcessLink::OnTakeConnectedChannel", this,
          &ProcessLink::OnTakeConnectedChannel));
    }

    // Wait until one of the runnables above changes the state of the
    // channel. Note that the state could be changed again after that (to
    // ChannelClosing, for example, by the IO thread). We can rely on it not
    // changing back to Closed: only the worker thread changes it to closed,
    // and we're on the worker thread, blocked.
    while (mChan->mChannelState == ChannelClosed) {
      mChan->mMonitor->Wait();
    }
  }
}

void ProcessLink::EchoMessage(Message* msg) {
  mChan->AssertWorkerThread();
  mChan->mMonitor->AssertCurrentThreadOwns();

  mIOLoop->PostTask(NewNonOwningRunnableMethod<Message*>(
      "ipc::ProcessLink::OnEchoMessage", this, &ProcessLink::OnEchoMessage,
      msg));
  // OnEchoMessage takes ownership of |msg|
}

void ProcessLink::SendMessage(Message* msg) {
  if (msg->size() > IPC::Channel::kMaximumMessageSize) {
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::IPCMessageName,
        nsDependentCString(msg->name()));
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::IPCMessageSize,
        static_cast<int>(msg->size()));
    MOZ_CRASH("IPC message size is too large");
  }

  if (!mChan->mIsPostponingSends) {
    mChan->AssertWorkerThread();
  }
  mChan->mMonitor->AssertCurrentThreadOwns();

  mIOLoop->PostTask(NewNonOwningRunnableMethod<Message*>(
      "IPC::Channel::Send", mTransport, &Transport::Send, msg));
}

void ProcessLink::SendClose() {
  mChan->AssertWorkerThread();
  mChan->mMonitor->AssertCurrentThreadOwns();

  mIOLoop->PostTask(NewNonOwningRunnableMethod(
      "ipc::ProcessLink::OnCloseChannel", this, &ProcessLink::OnCloseChannel));
}

ThreadLink::ThreadLink(MessageChannel* aChan, MessageChannel* aTargetChan)
    : MessageLink(aChan), mTargetChan(aTargetChan) {}

ThreadLink::~ThreadLink() {
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
  // also protects against the two ~ThreadLink() calls racing.
  if (mTargetChan) {
    MOZ_ASSERT(mTargetChan->mLink);
    static_cast<ThreadLink*>(mTargetChan->mLink)->mTargetChan = nullptr;
  }
  mTargetChan = nullptr;
}

void ThreadLink::EchoMessage(Message* msg) {
  mChan->AssertWorkerThread();
  mChan->mMonitor->AssertCurrentThreadOwns();

  mChan->OnMessageReceivedFromLink(std::move(*msg));
  delete msg;
}

void ThreadLink::SendMessage(Message* msg) {
  if (!mChan->mIsPostponingSends) {
    mChan->AssertWorkerThread();
  }
  mChan->mMonitor->AssertCurrentThreadOwns();

  if (mTargetChan) mTargetChan->OnMessageReceivedFromLink(std::move(*msg));
  delete msg;
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

//
// The methods below run in the context of the IO thread
//

void ProcessLink::OnMessageReceived(Message&& msg) {
  AssertIOThread();
  NS_ASSERTION(mChan->mChannelState != ChannelError, "Shouldn't get here!");
  MonitorAutoLock lock(*mChan->mMonitor);
  mChan->OnMessageReceivedFromLink(std::move(msg));
}

void ProcessLink::OnEchoMessage(Message* msg) {
  AssertIOThread();
  OnMessageReceived(std::move(*msg));
  delete msg;
}

void ProcessLink::OnChannelOpened() {
  AssertIOThread();

  {
    MonitorAutoLock lock(*mChan->mMonitor);

    mExistingListener = mTransport->set_listener(this);
#ifdef DEBUG
    if (mExistingListener) {
      std::queue<Message> pending;
      mExistingListener->GetQueuedMessages(pending);
      MOZ_ASSERT(pending.empty());
    }
#endif  // DEBUG

    mChan->mChannelState = ChannelOpening;
    lock.Notify();
  }
  /*assert*/ mTransport->Connect();
}

void ProcessLink::OnTakeConnectedChannel() {
  AssertIOThread();

  std::queue<Message> pending;
  {
    MonitorAutoLock lock(*mChan->mMonitor);

    mChan->mChannelState = ChannelConnected;

    mExistingListener = mTransport->set_listener(this);
    if (mExistingListener) {
      mExistingListener->GetQueuedMessages(pending);
    }
    lock.Notify();
  }

  // Dispatch whatever messages the previous listener had queued up.
  while (!pending.empty()) {
    OnMessageReceived(std::move(pending.front()));
    pending.pop();
  }
}

void ProcessLink::OnChannelConnected(int32_t peer_pid) {
  AssertIOThread();

  bool notifyChannel = false;

  {
    MonitorAutoLock lock(*mChan->mMonitor);
    // Do not force it into connected if it has errored out, started
    // closing, etc. Note that we can be in the Connected state already
    // since the parent starts out Connected.
    if (mChan->mChannelState == ChannelOpening ||
        mChan->mChannelState == ChannelConnected) {
      mChan->mChannelState = ChannelConnected;
      mChan->mMonitor->Notify();
      notifyChannel = true;
    }
  }

  if (mExistingListener) {
    mExistingListener->OnChannelConnected(peer_pid);
  }

  if (notifyChannel) {
    mChan->OnChannelConnected(peer_pid);
  }
}

void ProcessLink::OnChannelConnectError() {
  AssertIOThread();

  MonitorAutoLock lock(*mChan->mMonitor);

  mChan->OnChannelErrorFromLink();
}

void ProcessLink::OnChannelError() {
  AssertIOThread();

  MonitorAutoLock lock(*mChan->mMonitor);

  MOZ_ALWAYS_TRUE(this == mTransport->set_listener(mExistingListener));

  mChan->OnChannelErrorFromLink();
}

void ProcessLink::OnCloseChannel() {
  AssertIOThread();

  mTransport->Close();

  MonitorAutoLock lock(*mChan->mMonitor);

  DebugOnly<IPC::Channel::Listener*> previousListener =
      mTransport->set_listener(mExistingListener);

  // OnChannelError may have reset the listener already.
  MOZ_ASSERT(previousListener == this || previousListener == mExistingListener);

  mChan->mChannelState = ChannelClosed;
  mChan->mMonitor->Notify();
}

bool ProcessLink::Unsound_IsClosed() const {
  return mTransport->Unsound_IsClosed();
}

uint32_t ProcessLink::Unsound_NumQueuedMessages() const {
  return mTransport->Unsound_NumQueuedMessages();
}

}  // namespace ipc
}  // namespace mozilla
