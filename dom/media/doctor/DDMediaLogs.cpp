/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DDMediaLogs.h"

#include "DDLogUtils.h"
#include "nsIThreadManager.h"
#include "mozilla/JSONWriter.h"

namespace mozilla {

/* static */ DDMediaLogs::ConstructionResult
DDMediaLogs::New()
{
  nsCOMPtr<nsIThread> mThread;
  nsresult rv = NS_NewNamedThread("DDMediaLogs",
                                  getter_AddRefs(mThread),
                                  nullptr,
                                  nsIThreadManager::kThreadPoolStackSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return { rv, nullptr };
  }

  return { rv, new DDMediaLogs(std::move(mThread)) };
}

DDMediaLogs::DDMediaLogs(nsCOMPtr<nsIThread>&& aThread)
  : mMediaLogs(1)
  , mMutex("DDMediaLogs")
  , mThread(std::move(aThread))
{
  mMediaLogs.SetLength(1);
  mMediaLogs[0].mMediaElement = nullptr;
  DDL_INFO("DDMediaLogs constructed, processing thread: %p", mThread.get());
}

DDMediaLogs::~DDMediaLogs()
{
  // Perform end-of-life processing, ensure the processing thread is shutdown.
  Shutdown(/* aPanic = */ false);
}

void
DDMediaLogs::Panic()
{
  Shutdown(/* aPanic = */ true);
}

void
DDMediaLogs::Shutdown(bool aPanic)
{
  nsCOMPtr<nsIThread> thread;
  {
    MutexAutoLock lock(mMutex);
    thread.swap(mThread);
  }
  if (!thread) {
    // Already shutdown, nothing more to do.
    return;
  }

  DDL_INFO("DDMediaLogs::Shutdown will shutdown thread: %p", thread.get());
  // Will block until pending tasks have completed, and thread is dead.
  thread->Shutdown();

  if (aPanic) {
    mMessagesQueue.PopAll([](const DDLogMessage&) {});
    MutexAutoLock lock(mMutex);
    mLifetimes.Clear();
    mMediaLogs.Clear();
    mObjectLinks.Clear();
    mPendingPromises.Clear();
    return;
  }

  // Final processing is only necessary to output to MOZ_LOG=DDLoggerEnd,
  // so there's no point doing any of it if that MOZ_LOG is not enabled.
  if (MOZ_LOG_TEST(sDecoderDoctorLoggerEndLog, mozilla::LogLevel::Info)) {
    DDL_DEBUG("Perform final DDMediaLogs processing...");
    // The processing thread is dead, so we can safely call ProcessLog()
    // directly from this thread.
    ProcessLog();

    for (const DDMediaLog& mediaLog : mMediaLogs) {
      if (mediaLog.mMediaElement) {
        DDLE_INFO("---");
      }
      DDLE_INFO("--- Log for HTMLMediaElement[%p] ---", mediaLog.mMediaElement);
      for (const DDLogMessage& message : mediaLog.mMessages) {
        DDLE_LOG(message.mCategory <= DDLogCategory::_Unlink
                   ? mozilla::LogLevel::Debug
                   : mozilla::LogLevel::Info,
                 "%s",
                 message.Print(mLifetimes).get());
      }
      DDLE_DEBUG("--- End log for HTMLMediaElement[%p] ---",
                 mediaLog.mMediaElement);
    }
  }
}

DDMediaLog&
DDMediaLogs::LogForUnassociatedMessages()
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());
  return mMediaLogs[0];
}
const DDMediaLog&
DDMediaLogs::LogForUnassociatedMessages() const
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());
  return mMediaLogs[0];
}

DDMediaLog*
DDMediaLogs::GetLogFor(const dom::HTMLMediaElement* aMediaElement)
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());
  if (!aMediaElement) {
    return &LogForUnassociatedMessages();
  }
  for (DDMediaLog& log : mMediaLogs) {
    if (log.mMediaElement == aMediaElement) {
      return &log;
    }
  }
  return nullptr;
}

DDMediaLog&
DDMediaLogs::LogFor(const dom::HTMLMediaElement* aMediaElement)
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());
  DDMediaLog* log = GetLogFor(aMediaElement);
  if (!log) {
    log = mMediaLogs.AppendElement();
    log->mMediaElement = aMediaElement;
  }
  return *log;
}

void
DDMediaLogs::SetMediaElement(DDLifetime& aLifetime,
                             const dom::HTMLMediaElement* aMediaElement)
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());
  DDMediaLog& log = LogFor(aMediaElement);

  // List of lifetimes that are to be linked to aMediaElement.
  nsTArray<DDLifetime*> lifetimes;
  // We start with the given lifetime.
  lifetimes.AppendElement(&aLifetime);
  for (size_t i = 0; i < lifetimes.Length(); ++i) {
    DDLifetime& lifetime = *lifetimes[i];
    // Link the lifetime to aMediaElement.
    lifetime.mMediaElement = aMediaElement;
    // Classified lifetime's tag is a positive index from the DDMediaLog.
    lifetime.mTag = ++log.mLifetimeCount;
    DDL_DEBUG(
      "%s -> HTMLMediaElement[%p]", lifetime.Printf().get(), aMediaElement);

    // Go through the lifetime's existing linked lifetimes, if any is not
    // already linked to aMediaElement, add it to the list so it will get
    // linked in a later loop.
    for (auto& link : mObjectLinks) {
      if (lifetime.IsAliveAt(link.mLinkingIndex)) {
        if (lifetime.mObject == link.mParent) {
          DDLifetime* childLifetime =
            mLifetimes.FindLifetime(link.mChild, link.mLinkingIndex);
          if (childLifetime && !childLifetime->mMediaElement &&
              !lifetimes.Contains(childLifetime)) {
            lifetimes.AppendElement(childLifetime);
          }
        } else if (lifetime.mObject == link.mChild) {
          DDLifetime* parentLifetime =
            mLifetimes.FindLifetime(link.mParent, link.mLinkingIndex);
          if (parentLifetime && !parentLifetime->mMediaElement &&
              !lifetimes.Contains(parentLifetime)) {
            lifetimes.AppendElement(parentLifetime);
          }
        }
      }
    }
  }

  // Now we need to move yet-unclassified messages related to the just-set
  // elements, to the appropriate MediaElement list.
  DDMediaLog::LogMessages& messages = log.mMessages;
  DDMediaLog::LogMessages& messages0 = LogForUnassociatedMessages().mMessages;
  for (size_t i = 0; i < messages0.Length();
       /* increment inside the loop */) {
    DDLogMessage& message = messages0[i];
    bool found = false;
    for (const DDLifetime* lifetime : lifetimes) {
      if (lifetime->mObject == message.mObject) {
        found = true;
        break;
      }
    }
    if (found) {
      messages.AppendElement(std::move(message));
      messages0.RemoveElementAt(i);
      // No increment, as we've removed this element; next element is now at
      // the same index.
    } else {
      // Not touching this element, increment index to go to next element.
      ++i;
    }
  }
}

DDLifetime&
DDMediaLogs::FindOrCreateLifetime(const DDLogObject& aObject,
                                  DDMessageIndex aIndex,
                                  const DDTimeStamp& aTimeStamp)
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());
  // Try to find lifetime corresponding to message object.
  DDLifetime* lifetime = mLifetimes.FindLifetime(aObject, aIndex);
  if (!lifetime) {
    // No lifetime yet, create one.
    lifetime = &(mLifetimes.CreateLifetime(aObject, aIndex, aTimeStamp));
    if (MOZ_UNLIKELY(aObject.TypeName() ==
                     DDLoggedTypeTraits<dom::HTMLMediaElement>::Name())) {
      const dom::HTMLMediaElement* mediaElement =
        static_cast<const dom::HTMLMediaElement*>(aObject.Pointer());
      SetMediaElement(*lifetime, mediaElement);
      DDL_DEBUG("%s -> new lifetime: %s with MediaElement %p",
                aObject.Printf().get(),
                lifetime->Printf().get(),
                mediaElement);
    } else {
      DDL_DEBUG("%s -> new lifetime: %s",
                aObject.Printf().get(),
                lifetime->Printf().get());
    }
  }

  return *lifetime;
}

void
DDMediaLogs::LinkLifetimes(DDLifetime& aParentLifetime,
                           const char* aLinkName,
                           DDLifetime& aChildLifetime,
                           DDMessageIndex aIndex)
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());
  mObjectLinks.AppendElement(DDObjectLink{
    aParentLifetime.mObject, aChildLifetime.mObject, aLinkName, aIndex });
  if (aParentLifetime.mMediaElement) {
    if (!aChildLifetime.mMediaElement) {
      SetMediaElement(aChildLifetime, aParentLifetime.mMediaElement);
    }
  } else if (aChildLifetime.mMediaElement) {
    if (!aParentLifetime.mMediaElement) {
      SetMediaElement(aParentLifetime, aChildLifetime.mMediaElement);
    }
  }
}

void
DDMediaLogs::UnlinkLifetime(DDLifetime& aLifetime, DDMessageIndex aIndex)
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());
  for (DDObjectLink& link : mObjectLinks) {
    if ((link.mParent == aLifetime.mObject ||
         link.mChild == aLifetime.mObject) &&
        aLifetime.IsAliveAt(link.mLinkingIndex) && !link.mUnlinkingIndex) {
      link.mUnlinkingIndex = Some(aIndex);
    }
  }
};

void
DDMediaLogs::UnlinkLifetimes(DDLifetime& aParentLifetime,
                             DDLifetime& aChildLifetime,
                             DDMessageIndex aIndex)
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());
  for (DDObjectLink& link : mObjectLinks) {
    if ((link.mParent == aParentLifetime.mObject &&
         link.mChild == aChildLifetime.mObject) &&
        aParentLifetime.IsAliveAt(link.mLinkingIndex) &&
        aChildLifetime.IsAliveAt(link.mLinkingIndex) && !link.mUnlinkingIndex) {
      link.mUnlinkingIndex = Some(aIndex);
    }
  }
}

void
DDMediaLogs::DestroyLifetimeLinks(const DDLifetime& aLifetime)
{
  mObjectLinks.RemoveElementsBy([&](DDObjectLink& link) {
    return (link.mParent == aLifetime.mObject ||
            link.mChild == aLifetime.mObject) &&
           aLifetime.IsAliveAt(link.mLinkingIndex);
  });
}

size_t
DDMediaLogs::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t size = aMallocSizeOf(this) +
                // This will usually be called after processing, so negligible
                // external data should still be present in the queue.
                mMessagesQueue.ShallowSizeOfExcludingThis(aMallocSizeOf) +
                mLifetimes.SizeOfExcludingThis(aMallocSizeOf) +
                mMediaLogs.ShallowSizeOfExcludingThis(aMallocSizeOf) +
                mObjectLinks.ShallowSizeOfExcludingThis(aMallocSizeOf) +
                mPendingPromises.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const DDMediaLog& log : mMediaLogs) {
    size += log.SizeOfExcludingThis(aMallocSizeOf);
  }
  return size;
}

void
DDMediaLogs::ProcessBuffer()
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());

  mMessagesQueue.PopAll([this](const DDLogMessage& message) {
    DDL_DEBUG("Processing: %s", message.Print().Data());

    // Either this message will carry a new object for which to create a
    // lifetime, or we'll find an existing one.
    DDLifetime& lifetime =
      FindOrCreateLifetime(message.mObject, message.mIndex, message.mTimeStamp);

    // Copy the message contents (without the mValid flag) to the
    // appropriate MediaLog corresponding to the message's object lifetime.
    LogFor(lifetime.mMediaElement)
      .mMessages.AppendElement(static_cast<const DDLogMessage&>(message));

    switch (message.mCategory) {
      case DDLogCategory::_Construction:
        // The FindOrCreateLifetime above will have set a construction time,
        // so there's nothing more we need to do here.
        MOZ_ASSERT(lifetime.mConstructionTimeStamp);
        break;

      case DDLogCategory::_DerivedConstruction:
        // The FindOrCreateLifetime above will have set a construction time.
        MOZ_ASSERT(lifetime.mConstructionTimeStamp);
        // A derived construction must come with the base object.
        MOZ_ASSERT(message.mValue.is<DDLogObject>());
        {
          const DDLogObject& base = message.mValue.as<DDLogObject>();
          DDLifetime& baseLifetime =
            FindOrCreateLifetime(base, message.mIndex, message.mTimeStamp);
          // FindOrCreateLifetime could have moved `lifetime`.
          DDLifetime* lifetime2 =
            mLifetimes.FindLifetime(message.mObject, message.mIndex);
          MOZ_ASSERT(lifetime2);
          // Assume there's no multiple-inheritance (at least for the types
          // we're watching.)
          if (baseLifetime.mDerivedObject.Pointer()) {
            DDL_WARN(
              "base '%s' was already derived as '%s', now deriving as '%s'",
              baseLifetime.Printf().get(),
              baseLifetime.mDerivedObject.Printf().get(),
              lifetime2->Printf().get());
          }
          baseLifetime.mDerivedObject = lifetime2->mObject;
          baseLifetime.mDerivedObjectLinkingIndex = message.mIndex;
          // Link the base and derived objects, to ensure they go to the same
          // log.
          LinkLifetimes(*lifetime2, "is-a", baseLifetime, message.mIndex);
        }
        break;

      case DDLogCategory::_Destruction:
        lifetime.mDestructionIndex = message.mIndex;
        lifetime.mDestructionTimeStamp = message.mTimeStamp;
        UnlinkLifetime(lifetime, message.mIndex);
        break;

      case DDLogCategory::_Link:
        MOZ_ASSERT(message.mValue.is<DDLogObject>());
        {
          const DDLogObject& child = message.mValue.as<DDLogObject>();
          DDLifetime& childLifetime =
            FindOrCreateLifetime(child, message.mIndex, message.mTimeStamp);
          // FindOrCreateLifetime could have moved `lifetime`.
          DDLifetime* lifetime2 =
            mLifetimes.FindLifetime(message.mObject, message.mIndex);
          MOZ_ASSERT(lifetime2);
          LinkLifetimes(
            *lifetime2, message.mLabel, childLifetime, message.mIndex);
        }
        break;

      case DDLogCategory::_Unlink:
        MOZ_ASSERT(message.mValue.is<DDLogObject>());
        {
          const DDLogObject& child = message.mValue.as<DDLogObject>();
          DDLifetime& childLifetime =
            FindOrCreateLifetime(child, message.mIndex, message.mTimeStamp);
          // FindOrCreateLifetime could have moved `lifetime`.
          DDLifetime* lifetime2 =
            mLifetimes.FindLifetime(message.mObject, message.mIndex);
          MOZ_ASSERT(lifetime2);
          UnlinkLifetimes(*lifetime2, childLifetime, message.mIndex);
        }
        break;

      default:
        // Anything else: Nothing more to do.
        break;
    }
  });
}

struct StringWriteFunc : public JSONWriteFunc
{
  nsCString& mCString;
  explicit StringWriteFunc(nsCString& aCString)
    : mCString(aCString)
  {
  }
  void Write(const char* aStr) override { mCString.Append(aStr); }
};

void
DDMediaLogs::FulfillPromises()
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());

  MozPromiseHolder<LogMessagesPromise> promiseHolder;
  const dom::HTMLMediaElement* mediaElement = nullptr;
  {
    // Grab the first pending promise (if any).
    // Note that we don't pop it yet, so we don't potentially leave the list
    // empty and therefore allow another processing task to be dispatched.
    MutexAutoLock lock(mMutex);
    if (mPendingPromises.IsEmpty()) {
      return;
    }
    promiseHolder = std::move(mPendingPromises[0].mPromiseHolder);
    mediaElement = mPendingPromises[0].mMediaElement;
  }
  for (;;) {
    DDMediaLog* log = GetLogFor(mediaElement);
    if (!log) {
      // No such media element -> Reject this promise.
      DDL_INFO("Rejecting promise for HTMLMediaElement[%p] - Cannot find log",
               mediaElement);
      promiseHolder.Reject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR, __func__);
      // Pop this rejected promise, fetch next one.
      MutexAutoLock lock(mMutex);
      mPendingPromises.RemoveElementAt(0);
      if (mPendingPromises.IsEmpty()) {
        break;
      }
      promiseHolder = std::move(mPendingPromises[0].mPromiseHolder);
      mediaElement = mPendingPromises[0].mMediaElement;
      continue;
    }

    nsCString json;
    JSONWriter jw{ MakeUnique<StringWriteFunc>(json) };
    jw.Start();
    jw.StartArrayProperty("messages");
    for (const DDLogMessage& message : log->mMessages) {
      jw.StartObjectElement(JSONWriter::SingleLineStyle);
      jw.IntProperty("i", message.mIndex.Value());
      jw.DoubleProperty("ts", ToSeconds(message.mTimeStamp));
      DDLifetime* lifetime =
        mLifetimes.FindLifetime(message.mObject, message.mIndex);
      if (lifetime) {
        jw.IntProperty("ob", lifetime->mTag);
      } else {
        jw.StringProperty("ob",
                          nsPrintfCString(R"("%s[%p]")",
                                          message.mObject.TypeName(),
                                          message.mObject.Pointer())
                            .get());
      }
      jw.StringProperty("cat", ToShortString(message.mCategory));
      if (message.mLabel && message.mLabel[0] != '\0') {
        jw.StringProperty("lbl", message.mLabel);
      }
      if (!message.mValue.is<DDNoValue>()) {
        if (message.mValue.is<DDLogObject>()) {
          const DDLogObject& ob2 = message.mValue.as<DDLogObject>();
          DDLifetime* lifetime2 = mLifetimes.FindLifetime(ob2, message.mIndex);
          if (lifetime2) {
            jw.IntProperty("val", lifetime2->mTag);
          } else {
            ToJSON(message.mValue, jw, "val");
          }
        } else {
          ToJSON(message.mValue, jw, "val");
        }
      }
      jw.EndObject();
    }
    jw.EndArray();
    jw.StartObjectProperty("objects");
    mLifetimes.Visit(
      mediaElement,
      [&](const DDLifetime& lifetime) {
        jw.StartObjectProperty(nsPrintfCString("%" PRIi32, lifetime.mTag).get(),
                               JSONWriter::SingleLineStyle);
        jw.IntProperty("tag", lifetime.mTag);
        jw.StringProperty("cls", lifetime.mObject.TypeName());
        jw.StringProperty(
          "ptr", nsPrintfCString("%p", lifetime.mObject.Pointer()).get());
        jw.IntProperty("con", lifetime.mConstructionIndex.Value());
        jw.DoubleProperty("con_ts", ToSeconds(lifetime.mConstructionTimeStamp));
        if (lifetime.mDestructionTimeStamp) {
          jw.IntProperty("des", lifetime.mDestructionIndex.Value());
          jw.DoubleProperty("des_ts",
                            ToSeconds(lifetime.mDestructionTimeStamp));
        }
        if (lifetime.mDerivedObject.Pointer()) {
          DDLifetime* derived = mLifetimes.FindLifetime(
            lifetime.mDerivedObject, lifetime.mDerivedObjectLinkingIndex);
          if (derived) {
            jw.IntProperty("drvd", derived->mTag);
          }
        }
        jw.EndObject();
      },
      // If there were no (new) messages, only give the main HTMLMediaElement
      // object (used to identify this log against the correct element.)
      log->mMessages.IsEmpty());
    jw.EndObject();
    jw.End();
    DDL_DEBUG("RetrieveMessages(%p) ->\n%s", mediaElement, json.get());

    // This log exists (new messages or not) -> Resolve this promise.
    DDL_INFO("Resolving promise for HTMLMediaElement[%p] with messages %" PRImi
             "-%" PRImi,
             mediaElement,
             log->mMessages.IsEmpty() ? 0 : log->mMessages[0].mIndex.Value(),
             log->mMessages.IsEmpty()
               ? 0
               : log->mMessages[log->mMessages.Length() - 1].mIndex.Value());
    promiseHolder.Resolve(std::move(json), __func__);

    // Remove exported messages.
    log->mMessages.Clear();

    // Pop this resolved promise, fetch next one.
    MutexAutoLock lock(mMutex);
    mPendingPromises.RemoveElementAt(0);
    if (mPendingPromises.IsEmpty()) {
      break;
    }
    promiseHolder = std::move(mPendingPromises[0].mPromiseHolder);
    mediaElement = mPendingPromises[0].mMediaElement;
  }
}

void
DDMediaLogs::CleanUpLogs()
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());

  DDTimeStamp now = DDNow();

  // Keep up to 30s of unclassified messages (if a message doesn't get
  // classified this quickly, it probably never will be.)
  static const double sMaxAgeUnclassifiedMessages_s = 30.0;
  // Keep "dead" log (video element and dependents were destroyed) for up to
  // 2 minutes, in case the user wants to look at it after the facts.
  static const double sMaxAgeDeadLog_s = 120.0;
  // Keep old messages related to a live video for up to 5 minutes.
  static const double sMaxAgeClassifiedMessages_s = 300.0;

  for (size_t logIndexPlus1 = mMediaLogs.Length(); logIndexPlus1 != 0;
       --logIndexPlus1) {
    DDMediaLog& log = mMediaLogs[logIndexPlus1 - 1];
    if (log.mMediaElement) {
      // Remove logs for which no lifetime still existed some time ago.
      bool used = mLifetimes.VisitBreakable(
        log.mMediaElement, [&](const DDLifetime& lifetime) {
          // Do we still have a lifetime that existed recently enough?
          return !lifetime.mDestructionTimeStamp ||
                 (now - lifetime.mDestructionTimeStamp).ToSeconds() <=
                   sMaxAgeDeadLog_s;
        });
      if (!used) {
        DDL_INFO("Removed old log for media element %p", log.mMediaElement);
        mLifetimes.Visit(log.mMediaElement, [&](const DDLifetime& lifetime) {
          DestroyLifetimeLinks(lifetime);
        });
        mLifetimes.RemoveLifetimesFor(log.mMediaElement);
        mMediaLogs.RemoveElementAt(logIndexPlus1 - 1);
        continue;
      }
    }

    // Remove old messages.
    size_t old = 0;
    const size_t len = log.mMessages.Length();
    while (old < len && (now - log.mMessages[old].mTimeStamp).ToSeconds() >
                          (log.mMediaElement ? sMaxAgeClassifiedMessages_s
                                             : sMaxAgeUnclassifiedMessages_s)) {
      ++old;
    }
    if (old != 0) {
      // We are going to remove `old` messages.
      // First, remove associated destroyed lifetimes that are not used after
      // these old messages. (We want to keep non-destroyed lifetimes, in
      // case they get used later on.)
      size_t removedLifetimes = 0;
      for (size_t i = 0; i < old; ++i) {
        auto RemoveDestroyedUnusedLifetime = [&](DDLifetime* lifetime) {
          if (!lifetime->mDestructionTimeStamp) {
            // Lifetime is still alive, keep it.
            return;
          }
          bool used = false;
          for (size_t after = old; after < len; ++after) {
            const DDLogMessage message = log.mMessages[i];
            if (!lifetime->IsAliveAt(message.mIndex)) {
              // Lifetime is already dead, and not used yet -> kill it.
              break;
            }
            const DDLogObject& ob = message.mObject;
            if (lifetime->mObject == ob) {
              used = true;
              break;
            }
            if (message.mValue.is<DDLogObject>()) {
              if (lifetime->mObject == message.mValue.as<DDLogObject>()) {
                used = true;
                break;
              }
            }
          }
          if (!used) {
            DestroyLifetimeLinks(*lifetime);
            mLifetimes.RemoveLifetime(lifetime);
            ++removedLifetimes;
          }
        };

        const DDLogMessage message = log.mMessages[i];
        const DDLogObject& ob = message.mObject;

        DDLifetime* lifetime1 = mLifetimes.FindLifetime(ob, message.mIndex);
        if (lifetime1) {
          RemoveDestroyedUnusedLifetime(lifetime1);
        }

        if (message.mValue.is<DDLogObject>()) {
          DDLifetime* lifetime2 = mLifetimes.FindLifetime(
            message.mValue.as<DDLogObject>(), message.mIndex);
          if (lifetime2) {
            RemoveDestroyedUnusedLifetime(lifetime2);
          }
        }
      }
      DDL_INFO("Removed %zu messages (#%" PRImi " %f - #%" PRImi
               " %f) and %zu lifetimes from log for media element %p",
               old,
               log.mMessages[0].mIndex.Value(),
               ToSeconds(log.mMessages[0].mTimeStamp),
               log.mMessages[old - 1].mIndex.Value(),
               ToSeconds(log.mMessages[old - 1].mTimeStamp),
               removedLifetimes,
               log.mMediaElement);
      log.mMessages.RemoveElementsAt(0, old);
    }
  }
}

void
DDMediaLogs::ProcessLog()
{
  MOZ_ASSERT(!mThread || mThread.get() == NS_GetCurrentThread());
  ProcessBuffer();
  FulfillPromises();
  CleanUpLogs();
  DDL_INFO("ProcessLog() completed - DDMediaLog size: %zu",
           SizeOfIncludingThis(moz_malloc_size_of));
}

nsresult
DDMediaLogs::DispatchProcessLog(const MutexAutoLock& aProofOfLock)
{
  if (!mThread) {
    return NS_ERROR_SERVICE_NOT_AVAILABLE;
  }
  return mThread->Dispatch(
    NS_NewRunnableFunction("ProcessLog", [this] { ProcessLog(); }),
    NS_DISPATCH_NORMAL);
}

nsresult
DDMediaLogs::DispatchProcessLog()
{
  DDL_INFO("DispatchProcessLog() - Yet-unprocessed message buffers: %d",
           mMessagesQueue.LiveBuffersStats().mCount);
  MutexAutoLock lock(mMutex);
  return DispatchProcessLog(lock);
}

RefPtr<DDMediaLogs::LogMessagesPromise>
DDMediaLogs::RetrieveMessages(const dom::HTMLMediaElement* aMediaElement)
{
  MozPromiseHolder<LogMessagesPromise> holder;
  RefPtr<LogMessagesPromise> promise = holder.Ensure(__func__);
  {
    MutexAutoLock lock(mMutex);
    // If there were unfulfilled promises, we know processing has already
    // been requested.
    if (mPendingPromises.IsEmpty()) {
      // But if we're the first one, start processing.
      nsresult rv = DispatchProcessLog(lock);
      if (NS_FAILED(rv)) {
        holder.Reject(rv, __func__);
      }
    }
    mPendingPromises.AppendElement(
      PendingPromise{ std::move(holder), aMediaElement });
  }
  return promise;
}

} // namespace mozilla
