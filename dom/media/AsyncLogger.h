/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of an asynchronous lock-free logging system. */

#ifndef mozilla_dom_AsyncLogger_h
#define mozilla_dom_AsyncLogger_h

#include <atomic>
#include <thread>
#include "mozilla/Logging.h"
#include "mozilla/Attributes.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Sprintf.h"

namespace mozilla {

namespace detail {

 // This class implements a lock-free multiple producer single consumer queue of
 // fixed size log messages, with the following characteristics:
 // - Unbounded (uses a intrinsic linked list)
 // - Allocates on Push. Push can be called on any thread.
 // - Deallocates on Pop. Pop MUST always be called on the same thread for the
 // life-time of the queue.
 //
 // In our scenario, the producer threads are real-time, they can't block. The
 // consummer thread runs every now and then and empties the queue to a log
 // file, on disk.
 //
 // Having fixed size messages and jemalloc is probably not the fastest, but
 // allows having a simpler design, we count on the fact that jemalloc will get
 // the memory from a thread-local source most of the time.
template<size_t MESSAGE_LENGTH>
class MPSCQueue
{
public:
    struct Message {
        Message()
        {
           mNext.store(nullptr, std::memory_order_relaxed);
        }
        Message(const Message& aMessage) = delete;
        void operator=(const Message& aMessage) = delete;

        char data[MESSAGE_LENGTH];
        std::atomic<Message*> mNext;
    };
    // Creates a new MPSCQueue. Initially, the queue has a single sentinel node,
    // pointed to by both mHead and mTail.
    MPSCQueue()
    // At construction, the initial message points to nullptr (it has no
    // successor). It is a sentinel node, that does not contain meaningful
    // data.
    : mHead(new Message())
    , mTail(mHead.load(std::memory_order_relaxed))
    { }

    ~MPSCQueue()
    {
        Message dummy;
        while (this->Pop(dummy.data)) {}
        Message* front = mHead.load(std::memory_order_relaxed);
        delete front;
    }

    void
    Push(MPSCQueue<MESSAGE_LENGTH>::Message* aMessage)
    {
        // The next two non-commented line are called A and B in this paragraph.
        // Producer threads i, i-1, etc. are numbered in the order they reached
        // A in time, thread i being the thread that has reached A first.
        // Atomically, on line A the new `mHead` is set to be the node that was
        // just allocated, with strong memory order. From now one, any thread
        // that reaches A will see that the node just allocated is
        // effectively the head of the list, and will make itself the new head
        // of the list.
        // In a bad case (when thread i executes A and then
        // is not scheduled for a long time), it is possible that thread i-1 and
        // subsequent threads create a seemingly disconnected set of nodes, but
        // they all have the correct value for the next node to set as their
        // mNext member on their respective stacks (in `prev`), and this is
        // always correct. When the scheduler resumes, and line B is executed,
        // the correct linkage is resumed.
        // Before line B, since mNext for the node the was the last element of
        // the queue still has an mNext of nullptr, Pop will not see the node
        // added.
        // For line A, it's critical to have strong ordering both ways (since
        // it's going to possibly be read and write repeatidly by multiple
        // threads)
        // Line B can have weaker guarantees, it's only going to be written by a
        // single thread, and we just need to ensure it's read properly by a
        // single other one.
        Message* prev = mHead.exchange(aMessage, std::memory_order_acq_rel);
        prev->mNext.store(aMessage, std::memory_order_release);
    }

    // Allocates a new node, copy aInput to the new memory location, and pushes
    // it to the end of the list.
    void
    Push(const char aInput[MESSAGE_LENGTH])
    {
        // Create a new message, and copy the messages passed on argument to the
        // new memory location. We are not touching the queue right now. The
        // successor for this new node is set to be nullptr.
        Message* msg = new Message();
        strncpy(msg->data, aInput, MESSAGE_LENGTH);

        Push(msg);
    }

    // Copy the content of the first message of the queue to aOutput, and
    // frees the message. Returns true if there was a message, in which case
    // `aOutput` contains a valid value. If the queue was empty, returns false,
    // in which case `aOutput` is left untouched.
    bool
    Pop(char aOutput[MESSAGE_LENGTH])
    {
        // Similarly, in this paragraph, the two following lines are called A
        // and B, and threads are called thread i, i-1, etc. in order of
        // execution of line A.
        // On line A, the first element of the queue is acquired. It is simply a
        // sentinel node.
        // On line B, we acquire the node that has the data we want. If B is
        // null, then only the sentinel node was present in the queue, we can
        // safely return false.
        // mTail can be loaded with relaxed ordering, since it's not written nor
        // read by any other thread (this queue is single consumer).
        // mNext can be written to by one of the producer, so it's necessary to
        // ensure those writes are seen, hence the stricter ordering.
        Message* tail = mTail.load(std::memory_order_relaxed);
        Message* next = tail->mNext.load(std::memory_order_acquire);

        if (next == nullptr) {
            return false;
        }

        strncpy(aOutput, next->data, MESSAGE_LENGTH);

        // Simply shift the queue one node further, so that the sentinel node is
        // now pointing to the correct most ancient node. It contains stale data,
        // but this data will never be read again.
        // It's only necessary to ensure the previous load on this thread is not
        // reordered past this line, so release ordering is sufficient here.
        mTail.store(next, std::memory_order_release);

        // This thread is now the only thing that points to `tail`, it can be
        // safely deleted.
        delete tail;

        return true;
    }

private:
    // An atomic pointer to the most recent message in the queue.
    std::atomic<Message*> mHead;
    // An atomic pointer to a sentinel node, that points to the oldest message
    // in the queue.
    std::atomic<Message*> mTail;

    MPSCQueue(const MPSCQueue&) = delete;
    void operator=(const MPSCQueue&) = delete;
public:
    // The goal here is to make it easy on the allocator. We pack a pointer in the
    // message struct, and we still want to do power of two allocations to
    // minimize allocator slop. The allocation size are going to be constant, so
    // the allocation is probably going to hit the thread local cache in jemalloc,
    // making it cheap and, more importantly, lock-free enough.
    static const size_t MESSAGE_PADDING = sizeof(Message::mNext);
private:
    static_assert(IsPowerOfTwo(MESSAGE_LENGTH + MESSAGE_PADDING),
                  "MPSCQueue internal allocations must have a size that is a"
                  "power of two ");
};
} // end namespace detail

// This class implements a lock-free asynchronous logger, that outputs to
// MOZ_LOG.
// Any thread can use this logger without external synchronization and without
// being blocked. This log is suitable for use in real-time audio threads.
// Log formatting is best done externally, this class implements the output
// mechanism only.
// This class uses a thread internally, and must be started and stopped
// manually.
// If logging is disabled, all the calls are no-op.
class AsyncLogger
{
public:
  static const uint32_t MAX_MESSAGE_LENGTH = 512 - detail::MPSCQueue<sizeof(void*)>::MESSAGE_PADDING;

  // aLogModuleName is the name of the MOZ_LOG module.
  explicit AsyncLogger(const char* aLogModuleName)
  : mThread(nullptr)
  , mLogModule(aLogModuleName)
  , mRunning(false)
  { }

  ~AsyncLogger()
  {
    if (Enabled()) {
      Stop();
    }
  }

  void Start()
  {
    MOZ_ASSERT(!mRunning, "Double calls to AsyncLogger::Start");
    if (Enabled()) {
      mRunning = true;
      Run();
    }
  }

  void Stop()
  {
    if (Enabled()) {
      if (mRunning) {
        mRunning = false;
        mThread->join();
      }
    } else {
      MOZ_ASSERT(!mRunning && !mThread);
    }
  }

  void Log(const char* format, ...) MOZ_FORMAT_PRINTF(2,3)
  {
    if (Enabled()) {
      auto* msg = new detail::MPSCQueue<MAX_MESSAGE_LENGTH>::Message();
      va_list args;
      va_start(args, format);
      VsprintfLiteral(msg->data, format, args);
      va_end(args);
      mMessageQueue.Push(msg);
    }
  }

  bool Enabled()
  {
    return MOZ_LOG_TEST(mLogModule, mozilla::LogLevel::Verbose);
  }

private:
  void Run()
  {
    MOZ_ASSERT(Enabled());
    mThread.reset(new std::thread([this]() {
      while (mRunning) {
        char message[MAX_MESSAGE_LENGTH];
        while (mMessageQueue.Pop(message) && mRunning) {
          MOZ_LOG(mLogModule, mozilla::LogLevel::Verbose, ("%s", message));
        }
        Sleep();
      }
    }));
  }

  void Sleep() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }

  std::unique_ptr<std::thread> mThread;
  mozilla::LazyLogModule mLogModule;
  detail::MPSCQueue<MAX_MESSAGE_LENGTH> mMessageQueue;
  std::atomic<bool> mRunning;
};

} // end namespace mozilla

#endif // mozilla_dom_AsyncLogger_h
