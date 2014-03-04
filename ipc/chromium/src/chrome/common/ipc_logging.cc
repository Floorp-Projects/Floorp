// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_logging.h"

#if defined(OS_POSIX)
#ifdef IPC_MESSAGE_LOG_ENABLED
// This will cause render_messages.h etc to define ViewMsgLog and friends.
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#endif
#endif

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "base/waitable_event_watcher.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ipc_sync_message.h"
#include "chrome/common/ipc_message_utils.h"

#if defined(OS_POSIX)
#include "base/string_util.h"
#include <unistd.h>
#endif

#ifdef IPC_MESSAGE_LOG_ENABLED

using base::Time;

// IPC::Logging is allocated as a singleton, so we don't need any kind of
// special retention program.
template <>
struct RunnableMethodTraits<IPC::Logging> {
  static void RetainCallee(IPC::Logging*) {}
  static void ReleaseCallee(IPC::Logging*) {}
};

namespace IPC {

const wchar_t kLoggingEventName[] = L"ChromeIPCLog.%d";
const int kLogSendDelayMs = 100;

// We use a pointer to the function table to avoid any linker dependencies on
// all the traits used as IPC message parameters.
Logging::LogFunction *Logging::log_function_mapping_;

Logging::Logging()
    : logging_event_on_(NULL),
      logging_event_off_(NULL),
      enabled_(false),
      queue_invoke_later_pending_(false),
      sender_(NULL),
      main_thread_(MessageLoop::current()),
      consumer_(NULL) {
  // Create an event for this browser instance that's set when logging is
  // enabled, so child processes can know when logging is enabled.

#if defined(OS_WIN)
  // On Windows we have a couple of named events which switch logging on and
  // off.
  int browser_pid;
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  std::wstring process_type =
    parsed_command_line.GetSwitchValue(switches::kProcessType);
  if (process_type.empty()) {
    browser_pid = GetCurrentProcessId();
  } else {
    std::wstring channel_name =
        parsed_command_line.GetSwitchValue(switches::kProcessChannelID);

    browser_pid = _wtoi(channel_name.c_str());
    DCHECK(browser_pid != 0);
  }

  std::wstring event_name = GetEventName(browser_pid, true);
  logging_event_on_.reset(new base::WaitableEvent(
      CreateEventW(NULL, TRUE, FALSE, event_name.c_str())));

  event_name = GetEventName(browser_pid, false);
  logging_event_off_.reset(new base::WaitableEvent(
      CreateEventW(NULL, TRUE, FALSE, event_name.c_str())));

  RegisterWaitForEvent(true);
#endif

  MessageLoop::current()->AddDestructionObserver(this);
}

Logging::~Logging() {
}

Logging* Logging::current() {
  return Singleton<Logging>::get();
}

void Logging::RegisterWaitForEvent(bool enabled) {
  watcher_.StopWatching();
  watcher_.StartWatching(
      enabled ? logging_event_on_.get() : logging_event_off_.get(), this);
}

void Logging::OnWaitableEventSignaled(base::WaitableEvent* event) {
  enabled_ = event == logging_event_on_.get();
  RegisterWaitForEvent(!enabled_);
}

void Logging::WillDestroyCurrentMessageLoop() {
  watcher_.StopWatching();
}

void Logging::SetLoggerFunctions(LogFunction *functions) {
  log_function_mapping_ = functions;
}

#if defined(OS_WIN)
std::wstring Logging::GetEventName(bool enabled) {
  return current()->GetEventName(GetCurrentProcessId(), enabled);
}
#endif

std::wstring Logging::GetEventName(int browser_pid, bool enabled) {
  std::wstring result = StringPrintf(kLoggingEventName, browser_pid);
  result += enabled ? L"on" : L"off";
  return result;
}

void Logging::SetConsumer(Consumer* consumer) {
  consumer_ = consumer;
}

void Logging::Enable() {
  logging_event_off_->Reset();
  logging_event_on_->Signal();
}

void Logging::Disable() {
  logging_event_on_->Reset();
  logging_event_off_->Signal();
}

void Logging::OnSendLogs() {
  queue_invoke_later_pending_ = false;
  if (!sender_)
    return;

  Message* msg = new Message(
      MSG_ROUTING_CONTROL, IPC_LOGGING_ID, Message::PRIORITY_NORMAL);
  WriteParam(msg, queued_logs_);
  queued_logs_.clear();
  sender_->Send(msg);
}

void Logging::SetIPCSender(IPC::Message::Sender* sender) {
  sender_ = sender;
}

void Logging::OnReceivedLoggingMessage(const Message& message) {
  std::vector<LogData> data;
  void* iter = NULL;
  if (!ReadParam(&message, &iter, &data))
    return;

  for (size_t i = 0; i < data.size(); ++i) {
    Log(data[i]);
  }
}

void Logging::OnSendMessage(Message* message, const std::wstring& channel_id) {
  if (!Enabled())
    return;

  if (message->is_reply()) {
    LogData* data = message->sync_log_data();
    if (!data)
      return;

    // This is actually the delayed reply to a sync message.  Create a string
    // of the output parameters, add it to the LogData that was earlier stashed
    // with the reply, and log the result.
    data->channel = channel_id;
    GenerateLogData(L"", *message, data);
    Log(*data);
    delete data;
    message->set_sync_log_data(NULL);
  } else {
    // If the time has already been set (i.e. by ChannelProxy), keep that time
    // instead as it's more accurate.
    if (!message->sent_time())
      message->set_sent_time(Time::Now().ToInternalValue());
  }
}

void Logging::OnPreDispatchMessage(const Message& message) {
  message.set_received_time(Time::Now().ToInternalValue());
}

void Logging::OnPostDispatchMessage(const Message& message,
                                    const std::wstring& channel_id) {
  if (!Enabled() ||
#if defined(OS_WIN)
      !message.sent_time() ||
#endif
      message.dont_log())
    return;

  LogData data;
  GenerateLogData(channel_id, message, &data);

  if (MessageLoop::current() == main_thread_) {
    Log(data);
  } else {
    main_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Logging::Log, data));
  }
}

void Logging::GetMessageText(uint16_t type, std::wstring* name,
                             const Message* message,
                             std::wstring* params) {
  if (!log_function_mapping_)
    return;

  int message_class = type >> 12;
  if (log_function_mapping_[message_class] != NULL) {
    log_function_mapping_[message_class](type, name, message, params);
  } else {
    DLOG(INFO) << "No logger function associated with message class " <<
        message_class;
  }
}

void Logging::Log(const LogData& data) {
#if defined(OS_WIN)
  if (consumer_) {
    // We're in the browser process.
    consumer_->Log(data);
  } else {
    // We're in the renderer or plugin processes.
    if (sender_) {
      queued_logs_.push_back(data);
      if (!queue_invoke_later_pending_) {
        queue_invoke_later_pending_ = true;
        MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableMethod(
            this, &Logging::OnSendLogs), kLogSendDelayMs);
      }
    }
  }
#elif defined(OS_POSIX)
  // On POSIX, for now, we just dump the log to stderr
  fprintf(stderr, "ipc %s %d %d %s %s %s\n",
          WideToUTF8(data.channel).c_str(),
          data.routing_id,
          data.type,
          WideToUTF8(data.flags).c_str(),
          WideToUTF8(data.message_name).c_str(),
          WideToUTF8(data.params).c_str());
#endif
}

void GenerateLogData(const std::wstring& channel, const Message& message,
                     LogData* data) {
  if (message.is_reply()) {
    // "data" should already be filled in.
    std::wstring params;
    Logging::GetMessageText(data->type, NULL, &message, &params);

    if (!data->params.empty() && !params.empty())
      data->params += L", ";

    data->flags += L" DR";

    data->params += params;
  } else {
    std::wstring flags;
    if (message.is_sync())
      flags = L"S";

    if (message.is_reply())
      flags += L"R";

    if (message.is_reply_error())
      flags += L"E";

    std::wstring params, message_name;
    Logging::GetMessageText(message.type(), &message_name, &message, &params);

    data->channel = channel;
    data->routing_id = message.routing_id();
    data->type = message.type();
    data->flags = flags;
    data->sent = message.sent_time();
    data->receive = message.received_time();
    data->dispatch = Time::Now().ToInternalValue();
    data->params = params;
    data->message_name = message_name;
  }
}

}

#endif  // IPC_MESSAGE_LOG_ENABLED
