// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_host.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/waitable_event.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
typedef mozilla::ipc::BrowserProcessSubThread ChromeThread;
#include "chrome/common/ipc_logging.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/result_codes.h"


namespace {
typedef std::list<ChildProcessHost*> ChildProcessList;

// The NotificationTask is used to notify about plugin process connection/
// disconnection. It is needed because the notifications in the
// NotificationService must happen in the main thread.
class ChildNotificationTask : public Task {
 public:
  ChildNotificationTask(
      NotificationType notification_type, ChildProcessInfo* info)
      : notification_type_(notification_type), info_(*info) { }

  virtual void Run() {
    NotificationService::current()->
        Notify(notification_type_, NotificationService::AllSources(),
               Details<ChildProcessInfo>(&info_));
  }

 private:
  NotificationType notification_type_;
  ChildProcessInfo info_;
};

}  // namespace



ChildProcessHost::ChildProcessHost(ProcessType type)
    :
      ChildProcessInfo(type),
      ALLOW_THIS_IN_INITIALIZER_LIST(listener_(this)),
      opening_channel_(false),
      process_event_(NULL) {
  Singleton<ChildProcessList>::get()->push_back(this);
}


ChildProcessHost::~ChildProcessHost() {
  Singleton<ChildProcessList>::get()->remove(this);

  if (handle()) {
    watcher_.StopWatching();
    ProcessWatcher::EnsureProcessTerminated(handle());

#if defined(OS_WIN)
    // Above call took ownership, so don't want WaitableEvent to assert because
    // the handle isn't valid anymore.
    process_event_->Release();
#endif
  }
}

bool ChildProcessHost::CreateChannel() {
  channel_id_ = GenerateRandomChannelID(this);
  channel_.reset(new IPC::Channel(
      channel_id_, IPC::Channel::MODE_SERVER, &listener_));
  if (!channel_->Connect())
    return false;

  opening_channel_ = true;

  return true;
}

void ChildProcessHost::SetHandle(base::ProcessHandle process) {
#if defined(OS_WIN)
  process_event_.reset(new base::WaitableEvent(process));

  DCHECK(!handle());
  set_handle(process);
  watcher_.StartWatching(process_event_.get(), this);
#endif
}

void ChildProcessHost::InstanceCreated() {
  Notify(NotificationType::CHILD_INSTANCE_CREATED);
}

bool ChildProcessHost::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }
  return channel_->Send(msg);
}

void ChildProcessHost::Notify(NotificationType type) {
  MessageLoop* loop = ChromeThread::GetMessageLoop(ChromeThread::IO);
  if (!loop)
      loop = mozilla::ipc::ProcessChild::message_loop();
  if (!loop)
      loop = MessageLoop::current();
  loop->PostTask(
      FROM_HERE, new ChildNotificationTask(type, this));
}

void ChildProcessHost::OnWaitableEventSignaled(base::WaitableEvent *event) {
#if defined(OS_WIN)
  HANDLE object = event->handle();
  DCHECK(handle());
  DCHECK_EQ(object, handle());

  bool did_crash = base::DidProcessCrash(NULL, object);
  if (did_crash) {
    // Report that this child process crashed.
    Notify(NotificationType::CHILD_PROCESS_CRASHED);
  }
  // Notify in the main loop of the disconnection.
  Notify(NotificationType::CHILD_PROCESS_HOST_DISCONNECTED);
#endif
}

ChildProcessHost::ListenerHook::ListenerHook(ChildProcessHost* host)
    : host_(host) {
}

void ChildProcessHost::ListenerHook::OnMessageReceived(
    const IPC::Message& msg) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging* logger = IPC::Logging::current();
  if (msg.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(msg);
    return;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(msg);
#endif

  bool msg_is_ok = true;
  bool handled = false;

  if (!handled) {
      host_->OnMessageReceived(msg);
  }

  if (!msg_is_ok)
    base::KillProcess(host_->handle(), ResultCodes::KILLED_BAD_MESSAGE, false);

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (logger->Enabled())
    logger->OnPostDispatchMessage(msg, host_->channel_id_);
#endif
}

void ChildProcessHost::ListenerHook::OnChannelConnected(int32_t peer_pid) {
  host_->opening_channel_ = false;
  host_->OnChannelConnected(peer_pid);

  // Notify in the main loop of the connection.
  host_->Notify(NotificationType::CHILD_PROCESS_HOST_CONNECTED);
}

void ChildProcessHost::ListenerHook::OnChannelError() {
  host_->opening_channel_ = false;
  host_->OnChannelError();
}

void ChildProcessHost::ListenerHook::GetQueuedMessages(std::queue<IPC::Message>& queue) {
  host_->GetQueuedMessages(queue);
}

ChildProcessHost::Iterator::Iterator() : all_(true) {
  iterator_ = Singleton<ChildProcessList>::get()->begin();
}

ChildProcessHost::Iterator::Iterator(ProcessType type)
    : all_(false), type_(type) {
  iterator_ = Singleton<ChildProcessList>::get()->begin();
  if (!Done() && (*iterator_)->type() != type_)
    ++(*this);
}

ChildProcessHost* ChildProcessHost::Iterator::operator++() {
  do {
    ++iterator_;
    if (Done())
      break;

    if (!all_ && (*iterator_)->type() != type_)
      continue;

    return *iterator_;
  } while (true);

  return NULL;
}

bool ChildProcessHost::Iterator::Done() {
  return iterator_ == Singleton<ChildProcessList>::get()->end();
}
