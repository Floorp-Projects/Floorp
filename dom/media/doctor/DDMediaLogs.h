/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDMediaLogs_h_
#define DDMediaLogs_h_

#include "DDLifetimes.h"
#include "DDMediaLog.h"
#include "mozilla/MozPromise.h"
#include "MultiWriterQueue.h"

namespace mozilla {

// Main object managing all processed logs, and yet-unprocessed messages.
struct DDMediaLogs
{
public:
  // Construct a DDMediaLogs object if possible.
  struct ConstructionResult
  {
    nsresult mRv;
    DDMediaLogs* mMediaLogs;
  };
  static ConstructionResult New();

  // If not already shutdown, performs normal end-of-life processing, and shuts
  // down the processing thread (blocking).
  ~DDMediaLogs();

  // Shutdown the processing thread (blocking), and free as much memory as
  // possible.
  void Panic();

  inline void Log(const char* aSubjectTypeName,
                  const void* aSubjectPointer,
                  DDLogCategory aCategory,
                  const char* aLabel,
                  DDLogValue&& aValue)
  {
    if (mMessagesQueue.PushF(
          [&](DDLogMessage& aMessage, MessagesQueue::Index i) {
            aMessage.mIndex = i;
            aMessage.mTimeStamp = DDNow();
            aMessage.mObject.Set(aSubjectTypeName, aSubjectPointer);
            aMessage.mCategory = aCategory;
            aMessage.mLabel = aLabel;
            aMessage.mValue = std::move(aValue);
          })) {
      // Filled a buffer-full of messages, process it in another thread.
      DispatchProcessLog();
    }
  }

  // Process the log right now; should only be used on the processing thread,
  // or after shutdown for end-of-life log retrieval. Work includes:
  // - Processing incoming buffers, to update object lifetimes and links;
  // - Resolve pending promises that requested logs;
  // - Clean-up old logs from memory.
  void ProcessLog();

  using LogMessagesPromise =
    MozPromise<nsCString, nsresult, /* IsExclusive = */ true>;

  // Retrieve all messages associated with an HTMLMediaElement.
  // This will trigger an async processing run (to ensure most recent messages
  // get retrieved too), and the returned promise will be resolved with all
  // found log messages.
  RefPtr<LogMessagesPromise> RetrieveMessages(
    const dom::HTMLMediaElement* aMediaElement);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

private:
  // Constructor, takes the given thread to use for log processing.
  explicit DDMediaLogs(nsCOMPtr<nsIThread>&& aThread);

  // Shutdown the processing thread, blocks until that thread exits.
  // If aPanic is true, just free as much memory as possible.
  // Otherwise, perform a final processing run, output end-logs (if enabled).
  void Shutdown(bool aPanic);

  // Get the log of yet-unassociated messages.
  DDMediaLog& LogForUnassociatedMessages();
  const DDMediaLog& LogForUnassociatedMessages() const;

  // Get the log for the given HTMLMediaElement. Returns nullptr if there is no
  // such log yet.
  DDMediaLog* GetLogFor(const dom::HTMLMediaElement* aMediaElement);

  // Get the log for the given HTMLMediaElement.
  // A new log is created if that element didn't already have one.
  DDMediaLog& LogFor(const dom::HTMLMediaElement* aMediaElement);

  // Associate a lifetime, and all its already-linked lifetimes, with an
  // HTMLMediaElement.
  // All messages involving the modified lifetime(s) are moved to the
  // corresponding log.
  void SetMediaElement(DDLifetime& aLifetime,
                       const dom::HTMLMediaElement* aMediaElement);

  // Find the lifetime corresponding to an object (known type and pointer) that
  // was known to be alive at aIndex.
  // If there is no such lifetime yet, create it with aTimeStamp as implicit
  // construction timestamp.
  // If the object is of type HTMLMediaElement, run SetMediaElement() on it.
  DDLifetime& FindOrCreateLifetime(const DDLogObject& aObject,
                                   DDMessageIndex aIndex,
                                   const DDTimeStamp& aTimeStamp);

  // Link two lifetimes together (at a given time corresponding to aIndex).
  // If only one is associated with an HTMLMediaElement, run SetMediaElement on
  // the other one.
  void LinkLifetimes(DDLifetime& aParentLifetime,
                     const char* aLinkName,
                     DDLifetime& aChildLifetime,
                     DDMessageIndex aIndex);

  // Unlink all lifetimes linked to aLifetime; only used to know when links
  // expire, so that they won't be used after this time.
  void UnlinkLifetime(DDLifetime& aLifetime, DDMessageIndex aIndex);

  // Unlink two lifetimes; only used to know when a link expires, so that it
  // won't be used after this time.
  void UnlinkLifetimes(DDLifetime& aParentLifetime,
                       DDLifetime& aChildLifetime,
                       DDMessageIndex aIndex);

  // Remove all links involving aLifetime from the database.
  void DestroyLifetimeLinks(const DDLifetime& aLifetime);

  // Process all incoming log messages.
  // This will create the appropriate DDLifetime and links objects, and then
  // move processed messages to logs associated with different
  // HTMLMediaElements.
  void ProcessBuffer();

  // Pending promises (added by RetrieveMessages) are resolved with all new
  // log messages corresponding to requested HTMLMediaElements -- These
  // messages are removed from our logs.
  void FulfillPromises();

  // Remove processed messages that have a low chance of being requested,
  // based on the assumption that users/scripts will regularly call
  // RetrieveMessages for HTMLMediaElements they are interested in.
  void CleanUpLogs();

  // Request log-processing on the processing thread. Thread-safe.
  nsresult DispatchProcessLog();

  // Request log-processing on the processing thread.
  nsresult DispatchProcessLog(const MutexAutoLock& aProofOfLock);

  using MessagesQueue = MultiWriterQueue<DDLogMessage,
                                         MultiWriterQueueDefaultBufferSize,
                                         MultiWriterQueueReaderLocking_None>;
  MessagesQueue mMessagesQueue;

  DDLifetimes mLifetimes;

  // mMediaLogs[0] contains unsorted message (with mMediaElement=nullptr).
  // mMediaLogs[1+] contains sorted messages for each media element.
  nsTArray<DDMediaLog> mMediaLogs;

  struct DDObjectLink
  {
    const DDLogObject mParent;
    const DDLogObject mChild;
    const char* const mLinkName;
    const DDMessageIndex mLinkingIndex;
    Maybe<DDMessageIndex> mUnlinkingIndex;

    DDObjectLink(DDLogObject aParent,
                 DDLogObject aChild,
                 const char* aLinkName,
                 DDMessageIndex aLinkingIndex)
      : mParent(aParent)
      , mChild(aChild)
      , mLinkName(aLinkName)
      , mLinkingIndex(aLinkingIndex)
      , mUnlinkingIndex(Nothing{})
    {
    }
  };
  // Links between live objects, updated while messages are processed.
  nsTArray<DDObjectLink> mObjectLinks;

  // Protects members below.
  Mutex mMutex;

  // Processing thread.
  nsCOMPtr<nsIThread> mThread;

  struct PendingPromise
  {
    MozPromiseHolder<LogMessagesPromise> mPromiseHolder;
    const dom::HTMLMediaElement* mMediaElement;
  };
  // Most cases should have 1 media panel requesting 1 promise at a time.
  AutoTArray<PendingPromise, 2> mPendingPromises;
};

} // namespace mozilla

#endif // DDMediaLogs_h_
