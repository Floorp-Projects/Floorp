// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit test for SyncChannel.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/ipc_sync_channel.h"
#include "testing/gtest/include/gtest/gtest.h"


#define MESSAGES_INTERNAL_FILE "chrome/common/ipc_sync_message_unittest.h"
#include "chrome/common/ipc_message_macros.h"

using namespace IPC;
using base::WaitableEvent;

namespace {

// Base class for a "process" with listener and IPC threads.
class Worker : public Channel::Listener, public Message::Sender {
 public:
  // Will create a channel without a name.
  Worker(Channel::Mode mode, const std::string& thread_name)
      : done_(new WaitableEvent(false, false)),
        channel_created_(new WaitableEvent(false, false)),
        mode_(mode),
        ipc_thread_((thread_name + "_ipc").c_str()),
        listener_thread_((thread_name + "_listener").c_str()),
        overrided_thread_(NULL),
        shutdown_event_(true, false) { }

  // Will create a named channel and use this name for the threads' name.
  Worker(const std::wstring& channel_name, Channel::Mode mode)
      : done_(new WaitableEvent(false, false)),
        channel_created_(new WaitableEvent(false, false)),
        channel_name_(channel_name),
        mode_(mode),
        ipc_thread_((WideToUTF8(channel_name) + "_ipc").c_str()),
        listener_thread_((WideToUTF8(channel_name) + "_listener").c_str()),
        overrided_thread_(NULL),
        shutdown_event_(true, false) { }

  // The IPC thread needs to outlive SyncChannel, so force the correct order of
  // destruction.
  virtual ~Worker() {
    WaitableEvent listener_done(false, false), ipc_done(false, false);
    ListenerThread()->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Worker::OnListenerThreadShutdown1, &listener_done,
        &ipc_done));
    listener_done.Wait();
    ipc_done.Wait();
    ipc_thread_.Stop();
    listener_thread_.Stop();
  }
  void AddRef() { }
  void Release() { }
  bool Send(Message* msg) { return channel_->Send(msg); }
  bool SendWithTimeout(Message* msg, int timeout_ms) {
    return channel_->SendWithTimeout(msg, timeout_ms);
  }
  void WaitForChannelCreation() { channel_created_->Wait(); }
  void CloseChannel() {
    DCHECK(MessageLoop::current() == ListenerThread()->message_loop());
    channel_->Close();
  }
  void Start() {
    StartThread(&listener_thread_, MessageLoop::TYPE_DEFAULT);
    ListenerThread()->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Worker::OnStart));
  }
  void OverrideThread(base::Thread* overrided_thread) {
    DCHECK(overrided_thread_ == NULL);
    overrided_thread_ = overrided_thread;
  }
  bool SendAnswerToLife(bool pump, int timeout, bool succeed) {
    int answer = 0;
    SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
    if (pump)
      msg->EnableMessagePumping();
    bool result = SendWithTimeout(msg, timeout);
    DCHECK(result == succeed);
    DCHECK(answer == (succeed ? 42 : 0));
    return result;
  }
  bool SendDouble(bool pump, bool succeed) {
    int answer = 0;
    SyncMessage* msg = new SyncChannelTestMsg_Double(5, &answer);
    if (pump)
      msg->EnableMessagePumping();
    bool result = Send(msg);
    DCHECK(result == succeed);
    DCHECK(answer == (succeed ? 10 : 0));
    return result;
  }
  Channel::Mode mode() { return mode_; }
  WaitableEvent* done_event() { return done_.get(); }

 protected:
  // Derived classes need to call this when they've completed their part of
  // the test.
  void Done() { done_->Signal(); }
  // Functions for dervied classes to implement if they wish.
  virtual void Run() { }
  virtual void OnAnswer(int* answer) { NOTREACHED(); }
  virtual void OnAnswerDelay(Message* reply_msg) {
    // The message handler map below can only take one entry for
    // SyncChannelTestMsg_AnswerToLife, so since some classes want
    // the normal version while other want the delayed reply, we
    // call the normal version if the derived class didn't override
    // this function.
    int answer;
    OnAnswer(&answer);
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, answer);
    Send(reply_msg);
  }
  virtual void OnDouble(int in, int* out) { NOTREACHED(); }
  virtual void OnDoubleDelay(int in, Message* reply_msg) {
    int result;
    OnDouble(in, &result);
    SyncChannelTestMsg_Double::WriteReplyParams(reply_msg, result);
    Send(reply_msg);
  }

 private:
  base::Thread* ListenerThread() {
    return overrided_thread_ ? overrided_thread_ : &listener_thread_;
  }
  // Called on the listener thread to create the sync channel.
  void OnStart() {
    // Link ipc_thread_, listener_thread_ and channel_ altogether.
    StartThread(&ipc_thread_, MessageLoop::TYPE_IO);
    channel_.reset(new SyncChannel(
        channel_name_, mode_, this, NULL, ipc_thread_.message_loop(), true,
        &shutdown_event_));
    channel_created_->Signal();
    Run();
  }

  void OnListenerThreadShutdown1(WaitableEvent* listener_event,
                                 WaitableEvent* ipc_event) {
    // SyncChannel needs to be destructed on the thread that it was created on.
    channel_.reset();

    MessageLoop::current()->RunAllPending();

    ipc_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Worker::OnIPCThreadShutdown, listener_event, ipc_event));
  }

  void OnIPCThreadShutdown(WaitableEvent* listener_event,
                           WaitableEvent* ipc_event) {
    MessageLoop::current()->RunAllPending();
    ipc_event->Signal();

    listener_thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &Worker::OnListenerThreadShutdown2, listener_event));
  }

  void OnListenerThreadShutdown2(WaitableEvent* listener_event) {
    MessageLoop::current()->RunAllPending();
    listener_event->Signal();
  }

  void OnMessageReceived(const Message& message) {
    IPC_BEGIN_MESSAGE_MAP(Worker, message)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncChannelTestMsg_Double, OnDoubleDelay)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncChannelTestMsg_AnswerToLife,
                                     OnAnswerDelay)
    IPC_END_MESSAGE_MAP()
  }

  void StartThread(base::Thread* thread, MessageLoop::Type type) {
    base::Thread::Options options;
    options.message_loop_type = type;
    thread->StartWithOptions(options);
  }

  scoped_ptr<WaitableEvent> done_;
  scoped_ptr<WaitableEvent> channel_created_;
  std::wstring channel_name_;
  Channel::Mode mode_;
  scoped_ptr<SyncChannel> channel_;
  base::Thread ipc_thread_;
  base::Thread listener_thread_;
  base::Thread* overrided_thread_;

  base::WaitableEvent shutdown_event_;

  DISALLOW_EVIL_CONSTRUCTORS(Worker);
};


// Starts the test with the given workers.  This function deletes the workers
// when it's done.
void RunTest(std::vector<Worker*> workers) {
  // First we create the workers that are channel servers, or else the other
  // workers' channel initialization might fail because the pipe isn't created..
  for (size_t i = 0; i < workers.size(); ++i) {
    if (workers[i]->mode() == Channel::MODE_SERVER) {
      workers[i]->Start();
      workers[i]->WaitForChannelCreation();
    }
  }

  // now create the clients
  for (size_t i = 0; i < workers.size(); ++i) {
    if (workers[i]->mode() == Channel::MODE_CLIENT)
      workers[i]->Start();
  }

  // wait for all the workers to finish
  for (size_t i = 0; i < workers.size(); ++i)
    workers[i]->done_event()->Wait();

  STLDeleteContainerPointers(workers.begin(), workers.end());
}

}  // namespace

class IPCSyncChannelTest : public testing::Test {
 private:
  MessageLoop message_loop_;
};

//-----------------------------------------------------------------------------

namespace {

class SimpleServer : public Worker {
 public:
  SimpleServer(bool pump_during_send)
      : Worker(Channel::MODE_SERVER, "simpler_server"),
        pump_during_send_(pump_during_send) { }
  void Run() {
    SendAnswerToLife(pump_during_send_, base::kNoTimeout, true);
    Done();
  }

  bool pump_during_send_;
};

class SimpleClient : public Worker {
 public:
  SimpleClient() : Worker(Channel::MODE_CLIENT, "simple_client") { }

  void OnAnswer(int* answer) {
    *answer = 42;
    Done();
  }
};

void Simple(bool pump_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new SimpleServer(pump_during_send));
  workers.push_back(new SimpleClient());
  RunTest(workers);
}

}  // namespace

// Tests basic synchronous call
TEST_F(IPCSyncChannelTest, Simple) {
  Simple(false);
  Simple(true);
}

//-----------------------------------------------------------------------------

namespace {

class DelayClient : public Worker {
 public:
  DelayClient() : Worker(Channel::MODE_CLIENT, "delay_client") { }

  void OnAnswerDelay(Message* reply_msg) {
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
    Send(reply_msg);
    Done();
  }
};

void DelayReply(bool pump_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new SimpleServer(pump_during_send));
  workers.push_back(new DelayClient());
  RunTest(workers);
}

}  // namespace

// Tests that asynchronous replies work
TEST_F(IPCSyncChannelTest, DelayReply) {
  DelayReply(false);
  DelayReply(true);
}

//-----------------------------------------------------------------------------

namespace {

class NoHangServer : public Worker {
 public:
  explicit NoHangServer(WaitableEvent* got_first_reply, bool pump_during_send)
      : Worker(Channel::MODE_SERVER, "no_hang_server"),
        got_first_reply_(got_first_reply),
        pump_during_send_(pump_during_send) { }
  void Run() {
    SendAnswerToLife(pump_during_send_, base::kNoTimeout, true);
    got_first_reply_->Signal();

    SendAnswerToLife(pump_during_send_, base::kNoTimeout, false);
    Done();
  }

  WaitableEvent* got_first_reply_;
  bool pump_during_send_;
};

class NoHangClient : public Worker {
 public:
  explicit NoHangClient(WaitableEvent* got_first_reply)
    : Worker(Channel::MODE_CLIENT, "no_hang_client"),
      got_first_reply_(got_first_reply) { }

  virtual void OnAnswerDelay(Message* reply_msg) {
    // Use the DELAY_REPLY macro so that we can force the reply to be sent
    // before this function returns (when the channel will be reset).
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
    Send(reply_msg);
    got_first_reply_->Wait();
    CloseChannel();
    Done();
  }

  WaitableEvent* got_first_reply_;
};

void NoHang(bool pump_during_send) {
  WaitableEvent got_first_reply(false, false);
  std::vector<Worker*> workers;
  workers.push_back(new NoHangServer(&got_first_reply, pump_during_send));
  workers.push_back(new NoHangClient(&got_first_reply));
  RunTest(workers);
}

}  // namespace

// Tests that caller doesn't hang if receiver dies
TEST_F(IPCSyncChannelTest, NoHang) {
  NoHang(false);
  NoHang(true);
}

//-----------------------------------------------------------------------------

namespace {

class UnblockServer : public Worker {
 public:
  UnblockServer(bool pump_during_send)
    : Worker(Channel::MODE_SERVER, "unblock_server"),
      pump_during_send_(pump_during_send) { }
  void Run() {
    SendAnswerToLife(pump_during_send_, base::kNoTimeout, true);
    Done();
  }

  void OnDouble(int in, int* out) {
    *out = in * 2;
  }

  bool pump_during_send_;
};

class UnblockClient : public Worker {
 public:
  UnblockClient(bool pump_during_send)
    : Worker(Channel::MODE_CLIENT, "unblock_client"),
      pump_during_send_(pump_during_send) { }

  void OnAnswer(int* answer) {
    SendDouble(pump_during_send_, true);
    *answer = 42;
    Done();
  }

  bool pump_during_send_;
};

void Unblock(bool server_pump, bool client_pump) {
  std::vector<Worker*> workers;
  workers.push_back(new UnblockServer(server_pump));
  workers.push_back(new UnblockClient(client_pump));
  RunTest(workers);
}

}  // namespace

// Tests that the caller unblocks to answer a sync message from the receiver.
TEST_F(IPCSyncChannelTest, Unblock) {
  Unblock(false, false);
  Unblock(false, true);
  Unblock(true, false);
  Unblock(true, true);
}

//-----------------------------------------------------------------------------

namespace {

class RecursiveServer : public Worker {
 public:
  explicit RecursiveServer(
    bool expected_send_result, bool pump_first, bool pump_second)
    : Worker(Channel::MODE_SERVER, "recursive_server"),
      expected_send_result_(expected_send_result),
      pump_first_(pump_first), pump_second_(pump_second)  { }
  void Run() {
    SendDouble(pump_first_, expected_send_result_);
    Done();
  }

  void OnDouble(int in, int* out) {
    *out = in * 2;
    SendAnswerToLife(pump_second_, base::kNoTimeout, expected_send_result_);
  }

  bool expected_send_result_, pump_first_, pump_second_;
};

class RecursiveClient : public Worker {
 public:
  explicit RecursiveClient(bool pump_during_send, bool close_channel)
    : Worker(Channel::MODE_CLIENT, "recursive_client"),
      pump_during_send_(pump_during_send), close_channel_(close_channel) { }

  void OnDoubleDelay(int in, Message* reply_msg) {
    SendDouble(pump_during_send_, !close_channel_);
    if (close_channel_) {
      delete reply_msg;
    } else {
      SyncChannelTestMsg_Double::WriteReplyParams(reply_msg, in * 2);
      Send(reply_msg);
    }
    Done();
  }

  void OnAnswerDelay(Message* reply_msg) {
    if (close_channel_) {
      delete reply_msg;
      CloseChannel();
    } else {
      SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
      Send(reply_msg);
    }
  }

  bool pump_during_send_, close_channel_;
};

void Recursive(
    bool server_pump_first, bool server_pump_second, bool client_pump) {
  std::vector<Worker*> workers;
  workers.push_back(
      new RecursiveServer(true, server_pump_first, server_pump_second));
  workers.push_back(new RecursiveClient(client_pump, false));
  RunTest(workers);
}

}  // namespace

// Tests a server calling Send while another Send is pending.
TEST_F(IPCSyncChannelTest, Recursive) {
  Recursive(false, false, false);
  Recursive(false, false, true);
  Recursive(false, true, false);
  Recursive(false, true, true);
  Recursive(true, false, false);
  Recursive(true, false, true);
  Recursive(true, true, false);
  Recursive(true, true, true);
}

//-----------------------------------------------------------------------------

namespace {

void RecursiveNoHang(
    bool server_pump_first, bool server_pump_second, bool client_pump) {
  std::vector<Worker*> workers;
  workers.push_back(
      new RecursiveServer(false, server_pump_first, server_pump_second));
  workers.push_back(new RecursiveClient(client_pump, true));
  RunTest(workers);
}

}  // namespace

// Tests that if a caller makes a sync call during an existing sync call and
// the receiver dies, neither of the Send() calls hang.
TEST_F(IPCSyncChannelTest, RecursiveNoHang) {
  RecursiveNoHang(false, false, false);
  RecursiveNoHang(false, false, true);
  RecursiveNoHang(false, true, false);
  RecursiveNoHang(false, true, true);
  RecursiveNoHang(true, false, false);
  RecursiveNoHang(true, false, true);
  RecursiveNoHang(true, true, false);
  RecursiveNoHang(true, true, true);
}

//-----------------------------------------------------------------------------

namespace {

class MultipleServer1 : public Worker {
 public:
  MultipleServer1(bool pump_during_send)
    : Worker(L"test_channel1", Channel::MODE_SERVER),
      pump_during_send_(pump_during_send) { }

  void Run() {
    SendDouble(pump_during_send_, true);
    Done();
  }

  bool pump_during_send_;
};

class MultipleClient1 : public Worker {
 public:
  MultipleClient1(WaitableEvent* client1_msg_received,
                  WaitableEvent* client1_can_reply) :
      Worker(L"test_channel1", Channel::MODE_CLIENT),
      client1_msg_received_(client1_msg_received),
      client1_can_reply_(client1_can_reply) { }

  void OnDouble(int in, int* out) {
    client1_msg_received_->Signal();
    *out = in * 2;
    client1_can_reply_->Wait();
    Done();
  }

 private:
  WaitableEvent *client1_msg_received_, *client1_can_reply_;
};

class MultipleServer2 : public Worker {
 public:
  MultipleServer2() : Worker(L"test_channel2", Channel::MODE_SERVER) { }

  void OnAnswer(int* result) {
    *result = 42;
    Done();
  }
};

class MultipleClient2 : public Worker {
 public:
  MultipleClient2(
    WaitableEvent* client1_msg_received, WaitableEvent* client1_can_reply,
    bool pump_during_send)
    : Worker(L"test_channel2", Channel::MODE_CLIENT),
      client1_msg_received_(client1_msg_received),
      client1_can_reply_(client1_can_reply),
      pump_during_send_(pump_during_send) { }

  void Run() {
    client1_msg_received_->Wait();
    SendAnswerToLife(pump_during_send_, base::kNoTimeout, true);
    client1_can_reply_->Signal();
    Done();
  }

 private:
  WaitableEvent *client1_msg_received_, *client1_can_reply_;
  bool pump_during_send_;
};

void Multiple(bool server_pump, bool client_pump) {
  std::vector<Worker*> workers;

  // A shared worker thread so that server1 and server2 run on one thread.
  base::Thread worker_thread("Multiple");
  worker_thread.Start();

  // Server1 sends a sync msg to client1, which blocks the reply until
  // server2 (which runs on the same worker thread as server1) responds
  // to a sync msg from client2.
  WaitableEvent client1_msg_received(false, false);
  WaitableEvent client1_can_reply(false, false);

  Worker* worker;

  worker = new MultipleServer2();
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new MultipleClient2(
      &client1_msg_received, &client1_can_reply, client_pump);
  workers.push_back(worker);

  worker = new MultipleServer1(server_pump);
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new MultipleClient1(
      &client1_msg_received, &client1_can_reply);
  workers.push_back(worker);

  RunTest(workers);
}

}  // namespace

// Tests that multiple SyncObjects on the same listener thread can unblock each
// other.
TEST_F(IPCSyncChannelTest, Multiple) {
  Multiple(false, false);
  Multiple(false, true);
  Multiple(true, false);
  Multiple(true, true);
}

//-----------------------------------------------------------------------------

namespace {

class QueuedReplyServer1 : public Worker {
 public:
  QueuedReplyServer1(bool pump_during_send)
    : Worker(L"test_channel1", Channel::MODE_SERVER),
      pump_during_send_(pump_during_send) { }
  void Run() {
    SendDouble(pump_during_send_, true);
    Done();
  }

  bool pump_during_send_;
};

class QueuedReplyClient1 : public Worker {
 public:
  QueuedReplyClient1(WaitableEvent* client1_msg_received,
                     WaitableEvent* server2_can_reply) :
      Worker(L"test_channel1", Channel::MODE_CLIENT),
      client1_msg_received_(client1_msg_received),
      server2_can_reply_(server2_can_reply) { }

  void OnDouble(int in, int* out) {
    client1_msg_received_->Signal();
    *out = in * 2;
    server2_can_reply_->Wait();
    Done();
  }

 private:
  WaitableEvent *client1_msg_received_, *server2_can_reply_;
};

class QueuedReplyServer2 : public Worker {
 public:
  explicit QueuedReplyServer2(WaitableEvent* server2_can_reply) :
      Worker(L"test_channel2", Channel::MODE_SERVER),
      server2_can_reply_(server2_can_reply) { }

  void OnAnswer(int* result) {
    server2_can_reply_->Signal();

    // give client1's reply time to reach the server listener thread
    PlatformThread::Sleep(200);

    *result = 42;
    Done();
  }

  WaitableEvent *server2_can_reply_;
};

class QueuedReplyClient2 : public Worker {
 public:
  explicit QueuedReplyClient2(
    WaitableEvent* client1_msg_received, bool pump_during_send)
    : Worker(L"test_channel2", Channel::MODE_CLIENT),
      client1_msg_received_(client1_msg_received),
      pump_during_send_(pump_during_send){ }

  void Run() {
    client1_msg_received_->Wait();
    SendAnswerToLife(pump_during_send_, base::kNoTimeout, true);
    Done();
  }

  WaitableEvent *client1_msg_received_;
  bool pump_during_send_;
};

void QueuedReply(bool server_pump, bool client_pump) {
  std::vector<Worker*> workers;

  // A shared worker thread so that server1 and server2 run on one thread.
  base::Thread worker_thread("QueuedReply");
  worker_thread.Start();

  WaitableEvent client1_msg_received(false, false);
  WaitableEvent server2_can_reply(false, false);

  Worker* worker;

  worker = new QueuedReplyServer2(&server2_can_reply);
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new QueuedReplyClient2(&client1_msg_received, client_pump);
  workers.push_back(worker);

  worker = new QueuedReplyServer1(server_pump);
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new QueuedReplyClient1(
      &client1_msg_received, &server2_can_reply);
  workers.push_back(worker);

  RunTest(workers);
}

}  // namespace

// While a blocking send is in progress, the listener thread might answer other
// synchronous messages.  This tests that if during the response to another
// message the reply to the original messages comes, it is queued up correctly
// and the original Send is unblocked later.
TEST_F(IPCSyncChannelTest, QueuedReply) {
  QueuedReply(false, false);
  QueuedReply(false, true);
  QueuedReply(true, false);
  QueuedReply(true, true);
}

//-----------------------------------------------------------------------------

namespace {

class BadServer : public Worker {
 public:
  BadServer(bool pump_during_send)
    : Worker(Channel::MODE_SERVER, "simpler_server"),
      pump_during_send_(pump_during_send) { }
  void Run() {
    int answer = 0;

    SyncMessage* msg = new SyncMessage(
        MSG_ROUTING_CONTROL, SyncChannelTestMsg_Double::ID,
        Message::PRIORITY_NORMAL, NULL);
    if (pump_during_send_)
      msg->EnableMessagePumping();

    // Temporarily set the minimum logging very high so that the assertion
    // in ipc_message_utils doesn't fire.
    int log_level = logging::GetMinLogLevel();
    logging::SetMinLogLevel(kint32max);
    bool result = Send(msg);
    logging::SetMinLogLevel(log_level);
    DCHECK(!result);

    // Need to send another message to get the client to call Done().
    result = Send(new SyncChannelTestMsg_AnswerToLife(&answer));
    DCHECK(result);
    DCHECK(answer == 42);

    Done();
  }

  bool pump_during_send_;
};

void BadMessage(bool pump_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new BadServer(pump_during_send));
  workers.push_back(new SimpleClient());
  RunTest(workers);
}

}  // namespace

// Tests that if a message is not serialized correctly, the Send() will fail.
TEST_F(IPCSyncChannelTest, BadMessage) {
  BadMessage(false);
  BadMessage(true);
}

//-----------------------------------------------------------------------------

namespace {

class ChattyClient : public Worker {
 public:
  ChattyClient() :
      Worker(Channel::MODE_CLIENT, "chatty_client") { }

  void OnAnswer(int* answer) {
    // The PostMessage limit is 10k.  Send 20% more than that.
    const int kMessageLimit = 10000;
    const int kMessagesToSend = kMessageLimit * 120 / 100;
    for (int i = 0; i < kMessagesToSend; ++i) {
      if (!SendDouble(false, true))
        break;
    }
    *answer = 42;
    Done();
  }
};

void ChattyServer(bool pump_during_send) {
  std::vector<Worker*> workers;
  workers.push_back(new UnblockServer(pump_during_send));
  workers.push_back(new ChattyClient());
  RunTest(workers);
}

}  // namespace

// Tests http://b/1093251 - that sending lots of sync messages while
// the receiver is waiting for a sync reply does not overflow the PostMessage
// queue.
TEST_F(IPCSyncChannelTest, ChattyServer) {
  ChattyServer(false);
  ChattyServer(true);
}

//------------------------------------------------------------------------------

namespace {

class TimeoutServer : public Worker {
 public:
   TimeoutServer(int timeout_ms,
                 std::vector<bool> timeout_seq,
                 bool pump_during_send)
      : Worker(Channel::MODE_SERVER, "timeout_server"),
        timeout_ms_(timeout_ms),
        timeout_seq_(timeout_seq),
        pump_during_send_(pump_during_send) {
  }

  void Run() {
    for (std::vector<bool>::const_iterator iter = timeout_seq_.begin();
         iter != timeout_seq_.end(); ++iter) {
      SendAnswerToLife(pump_during_send_, timeout_ms_, !*iter);
    }
    Done();
  }

 private:
  int timeout_ms_;
  std::vector<bool> timeout_seq_;
  bool pump_during_send_;
};

class UnresponsiveClient : public Worker {
 public:
   UnresponsiveClient(std::vector<bool> timeout_seq)
      : Worker(Channel::MODE_CLIENT, "unresponsive_client"),
        timeout_seq_(timeout_seq) {
   }

  void OnAnswerDelay(Message* reply_msg) {
    DCHECK(!timeout_seq_.empty());
    if (!timeout_seq_[0]) {
      SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
      Send(reply_msg);
    } else {
      // Don't reply.
      delete reply_msg;
    }
    timeout_seq_.erase(timeout_seq_.begin());
    if (timeout_seq_.empty())
      Done();
  }

 private:
  // Whether we should time-out or respond to the various messages we receive.
  std::vector<bool> timeout_seq_;
};

void SendWithTimeoutOK(bool pump_during_send) {
  std::vector<Worker*> workers;
  std::vector<bool> timeout_seq;
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  workers.push_back(new TimeoutServer(5000, timeout_seq, pump_during_send));
  workers.push_back(new SimpleClient());
  RunTest(workers);
}

void SendWithTimeoutTimeout(bool pump_during_send) {
  std::vector<Worker*> workers;
  std::vector<bool> timeout_seq;
  timeout_seq.push_back(true);
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  workers.push_back(new TimeoutServer(100, timeout_seq, pump_during_send));
  workers.push_back(new UnresponsiveClient(timeout_seq));
  RunTest(workers);
}

void SendWithTimeoutMixedOKAndTimeout(bool pump_during_send) {
  std::vector<Worker*> workers;
  std::vector<bool> timeout_seq;
  timeout_seq.push_back(true);
  timeout_seq.push_back(false);
  timeout_seq.push_back(false);
  timeout_seq.push_back(true);
  timeout_seq.push_back(false);
  workers.push_back(new TimeoutServer(100, timeout_seq, pump_during_send));
  workers.push_back(new UnresponsiveClient(timeout_seq));
  RunTest(workers);
}

}  // namespace

// Tests that SendWithTimeout does not time-out if the response comes back fast
// enough.
TEST_F(IPCSyncChannelTest, SendWithTimeoutOK) {
  SendWithTimeoutOK(false);
  SendWithTimeoutOK(true);
}

// Tests that SendWithTimeout does time-out.
TEST_F(IPCSyncChannelTest, SendWithTimeoutTimeout) {
  SendWithTimeoutTimeout(false);
  SendWithTimeoutTimeout(true);
}

// Sends some message that time-out and some that succeed.
TEST_F(IPCSyncChannelTest, SendWithTimeoutMixedOKAndTimeout) {
  SendWithTimeoutMixedOKAndTimeout(false);
  SendWithTimeoutMixedOKAndTimeout(true);
}

//------------------------------------------------------------------------------

namespace {

class NestedTask : public Task {
 public:
  NestedTask(Worker* server) : server_(server) { }
  void Run() {
    // Sleep a bit so that we wake up after the reply has been received.
    PlatformThread::Sleep(250);
    server_->SendAnswerToLife(true, base::kNoTimeout, true);
  }

  Worker* server_;
};

static bool timeout_occured = false;

class TimeoutTask : public Task {
 public:
  void Run() {
    timeout_occured = true;
  }
};

class DoneEventRaceServer : public Worker {
 public:
  DoneEventRaceServer()
      : Worker(Channel::MODE_SERVER, "done_event_race_server") { }

  void Run() {
    MessageLoop::current()->PostTask(FROM_HERE, new NestedTask(this));
    MessageLoop::current()->PostDelayedTask(FROM_HERE, new TimeoutTask(), 9000);
    // Even though we have a timeout on the Send, it will succeed since for this
    // bug, the reply message comes back and is deserialized, however the done
    // event wasn't set.  So we indirectly use the timeout task to notice if a
    // timeout occurred.
    SendAnswerToLife(true, 10000, true);
    DCHECK(!timeout_occured);
    Done();
  }
};

}  // namespace

// Tests http://b/1474092 - that if after the done_event is set but before
// OnObjectSignaled is called another message is sent out, then after its
// reply comes back OnObjectSignaled will be called for the first message.
TEST_F(IPCSyncChannelTest, DoneEventRace) {
  std::vector<Worker*> workers;
  workers.push_back(new DoneEventRaceServer());
  workers.push_back(new SimpleClient());
  RunTest(workers);
}
