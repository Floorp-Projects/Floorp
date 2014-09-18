// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_LOGGING_H_
#define CHROME_COMMON_IPC_LOGGING_H_

#include "chrome/common/ipc_message.h"  // For IPC_MESSAGE_LOG_ENABLED.

#ifdef IPC_MESSAGE_LOG_ENABLED

#include "base/lock.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/waitable_event_watcher.h"
#include "chrome/common/ipc_message_utils.h"

class MessageLoop;

namespace IPC {

class Message;

// One instance per process.  Needs to be created on the main thread (the UI
// thread in the browser) but OnPreDispatchMessage/OnPostDispatchMessage
// can be called on other threads.
class Logging : public base::WaitableEventWatcher::Delegate,
                public MessageLoop::DestructionObserver {
 public:
  // Implemented by consumers of log messages.
  class Consumer {
   public:
    virtual void Log(const LogData& data) = 0;
  };

  void SetConsumer(Consumer* consumer);

  ~Logging();
  static Logging* current();

  void Enable();
  void Disable();
  bool Enabled() const { return enabled_; }

  // Called by child processes to give the logger object the channel to send
  // logging data to the browser process.
  void SetIPCSender(Message::Sender* sender);

  // Called in the browser process when logging data from a child process is
  // received.
  void OnReceivedLoggingMessage(const Message& message);

  void OnSendMessage(Message* message, const std::wstring& channel_id);
  void OnPreDispatchMessage(const Message& message);
  void OnPostDispatchMessage(const Message& message,
                             const std::wstring& channel_id);

  // Returns the name of the logging enabled/disabled events so that the
  // sandbox can add them to to the policy.  If true, gets the name of the
  // enabled event, if false, gets the name of the disabled event.
  static std::wstring GetEventName(bool enabled);

  // Like the *MsgLog functions declared for each message class, except this
  // calls the correct one based on the message type automatically.  Defined in
  // ipc_logging.cc.
  static void GetMessageText(uint16_t type, std::wstring* name,
                             const Message* message, std::wstring* params);

  // WaitableEventWatcher::Delegate implementation
  void OnWaitableEventSignaled(base::WaitableEvent* event);

  // MessageLoop::DestructionObserver implementation
  void WillDestroyCurrentMessageLoop();

  typedef void (*LogFunction)(uint16_t type,
                             std::wstring* name,
                             const Message* msg,
                             std::wstring* params);

  static void SetLoggerFunctions(LogFunction *functions);

 private:
  friend struct DefaultSingletonTraits<Logging>;
  Logging();

  std::wstring GetEventName(int browser_pid, bool enabled);
  void OnSendLogs();
  void Log(const LogData& data);

  void RegisterWaitForEvent(bool enabled);

  base::WaitableEventWatcher watcher_;

  scoped_ptr<base::WaitableEvent> logging_event_on_;
  scoped_ptr<base::WaitableEvent> logging_event_off_;
  bool enabled_;

  std::vector<LogData> queued_logs_;
  bool queue_invoke_later_pending_;

  Message::Sender* sender_;
  MessageLoop* main_thread_;

  Consumer* consumer_;

  static LogFunction *log_function_mapping_;
};

}  // namespace IPC

#endif // IPC_MESSAGE_LOG_ENABLED

#endif  // CHROME_COMMON_IPC_LOGGING_H_
