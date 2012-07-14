// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_HOST_H_
#define CHROME_COMMON_CHILD_PROCESS_HOST_H_

#include "build/build_config.h"

#include <list>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/waitable_event_watcher.h"
class ResourceDispatcherHost;
#include "chrome/common/child_process_info.h"
#include "chrome/common/ipc_channel.h"

class NotificationType;

// Plugins/workers and other child processes that live on the IO thread should
// derive from this class.
class ChildProcessHost :
                         public IPC::Message::Sender,
                         public ChildProcessInfo,
                         public base::WaitableEventWatcher::Delegate,
                         public IPC::Channel::Listener {
 public:
  virtual ~ChildProcessHost();

  // ResourceDispatcherHost::Receiver implementation:
  virtual bool Send(IPC::Message* msg);

  // The Iterator class allows iteration through either all child processes, or
  // ones of a specific type, depending on which constructor is used.  Note that
  // this should be done from the IO thread and that the iterator should not be
  // kept around as it may be invalidated on subsequent event processing in the
  // event loop.
  class Iterator {
   public:
    Iterator();
    Iterator(ProcessType type);
    ChildProcessHost* operator->() { return *iterator_; }
    ChildProcessHost* operator*() { return *iterator_; }
    ChildProcessHost* operator++();
    bool Done();

   private:
    bool all_;
    ProcessType type_;
    std::list<ChildProcessHost*>::iterator iterator_;
  };

 protected:
  ChildProcessHost(ProcessType type,
                   ResourceDispatcherHost* resource_dispatcher_host = 0);

  // Derived classes return true if it's ok to shut down the child process.
  virtual bool CanShutdown() = 0;

  // Creates the IPC channel.  Returns true iff it succeeded.
  bool CreateChannel();

  // Once the subclass gets a handle to the process, it needs to tell
  // ChildProcessHost using this function.
  void SetHandle(base::ProcessHandle handle);

  // Notifies us that an instance has been created on this child process.
  void InstanceCreated();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg) { }
  virtual void OnChannelConnected(int32 peer_pid) { }
  virtual void OnChannelError() { }

  bool opening_channel() { return opening_channel_; }
  const std::wstring& channel_id() { return channel_id_; }

  base::WaitableEvent* GetProcessEvent() { return process_event_.get(); }

  const IPC::Channel& channel() const { return *channel_; }
  IPC::Channel* channelp() const { return channel_.get(); }

 private:
  // Sends the given notification to the notification service on the UI thread.
  void Notify(NotificationType type);

 protected:
  // WaitableEventWatcher::Delegate implementation:
  virtual void OnWaitableEventSignaled(base::WaitableEvent *event);

 private:
  // By using an internal class as the IPC::Channel::Listener, we can intercept
  // OnMessageReceived/OnChannelConnected and do our own processing before
  // calling the subclass' implementation.
  class ListenerHook : public IPC::Channel::Listener {
   public:
    ListenerHook(ChildProcessHost* host);
    virtual void OnMessageReceived(const IPC::Message& msg);
    virtual void OnChannelConnected(int32 peer_pid);
    virtual void OnChannelError();
    virtual void GetQueuedMessages(std::queue<IPC::Message>& queue);
   private:
    ChildProcessHost* host_;
  };

  ListenerHook listener_;

  ResourceDispatcherHost* resource_dispatcher_host_;

  // True while we're waiting the channel to be opened.
  bool opening_channel_;

  // The IPC::Channel.
  scoped_ptr<IPC::Channel> channel_;

  // IPC Channel's id.
  std::wstring channel_id_;

  // Used to watch the child process handle.
  base::WaitableEventWatcher watcher_;

  scoped_ptr<base::WaitableEvent> process_event_;
};

#endif  // CHROME_COMMON_CHILD_PROCESS_HOST_H_
