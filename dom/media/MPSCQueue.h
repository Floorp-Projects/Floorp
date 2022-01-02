/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MPSCQueue_h
#define mozilla_dom_MPSCQueue_h

namespace mozilla {

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
const size_t MPSC_MSG_RESERVED = sizeof(std::atomic<void*>);

template <typename T>
class MPSCQueue {
 public:
  struct Message {
    Message() { mNext.store(nullptr, std::memory_order_relaxed); }
    Message(const Message& aMessage) = delete;
    void operator=(const Message& aMessage) = delete;

    std::atomic<Message*> mNext;
    T data;
  };

  // Creates a new MPSCQueue. Initially, the queue has a single sentinel node,
  // pointed to by both mHead and mTail.
  MPSCQueue()
      // At construction, the initial message points to nullptr (it has no
      // successor). It is a sentinel node, that does not contain meaningful
      // data.
      : mHead(new Message()), mTail(mHead.load(std::memory_order_relaxed)) {}

  ~MPSCQueue() {
    Message dummy;
    while (Pop(&dummy.data)) {
    }
    Message* front = mHead.load(std::memory_order_relaxed);
    delete front;
  }

  void Push(MPSCQueue<T>::Message* aMessage) {
    // The next two non-commented line are called A and B in this paragraph.
    // Producer threads i, i-1, etc. are numbered in the order they reached
    // A in time, thread i being the thread that has reached A first.
    // Atomically, on line A the new `mHead` is set to be the node that was
    // just allocated, with strong memory order. From now on, any thread
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
    // Before line B, since mNext for the node was the last element of
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

  // Copy the content of the first message of the queue to aOutput, and
  // frees the message. Returns true if there was a message, in which case
  // `aOutput` contains a valid value. If the queue was empty, returns false,
  // in which case `aOutput` is left untouched.
  bool Pop(T* aOutput) {
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

    *aOutput = next->data;

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
};

}  // namespace mozilla

#endif  // mozilla_dom_MPSCQueue_h
