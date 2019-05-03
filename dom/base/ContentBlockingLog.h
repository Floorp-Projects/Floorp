/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentBlockingLog_h
#define mozilla_dom_ContentBlockingLog_h

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/Tuple.h"
#include "mozilla/UniquePtr.h"
#include "nsReadableUtils.h"
#include "nsTArray.h"
#include "nsWindowSizes.h"

namespace mozilla {
namespace dom {

class ContentBlockingLog final {
  typedef AntiTrackingCommon::StorageAccessGrantedReason
      StorageAccessGrantedReason;

  struct LogEntry {
    uint32_t mType;
    uint32_t mRepeatCount;
    bool mBlocked;
    Maybe<AntiTrackingCommon::StorageAccessGrantedReason> mReason;
    nsTArray<nsCString> mTrackingFullHashes;
  };

  struct OriginDataEntry {
    OriginDataEntry() : mHasTrackingContentLoaded(false) {}

    bool mHasTrackingContentLoaded;
    Maybe<bool> mHasCookiesLoaded;
    nsTArray<LogEntry> mLogs;
  };

  struct OriginEntry {
    OriginEntry() { mData = MakeUnique<OriginDataEntry>(); }

    nsCString mOrigin;
    UniquePtr<OriginDataEntry> mData;
  };

  typedef nsTArray<OriginEntry> OriginDataTable;

  struct StringWriteFunc : public JSONWriteFunc {
    nsACString&
        mBuffer;  // The lifetime of the struct must be bound to the buffer
    explicit StringWriteFunc(nsACString& aBuffer) : mBuffer(aBuffer) {}

    void Write(const char* aStr) override { mBuffer.Append(aStr); }
  };

  struct Comparator {
   public:
    bool Equals(const OriginDataTable::elem_type& aLeft,
                const OriginDataTable::elem_type& aRight) const {
      return aLeft.mOrigin.Equals(aRight.mOrigin);
    }

    bool Equals(const OriginDataTable::elem_type& aLeft,
                const nsACString& aRight) const {
      return aLeft.mOrigin.Equals(aRight);
    }
  };

 public:
  ContentBlockingLog() = default;
  ~ContentBlockingLog() = default;

  void RecordLog(
      const nsACString& aOrigin, uint32_t aType, bool aBlocked,
      const Maybe<AntiTrackingCommon::StorageAccessGrantedReason>& aReason,
      const nsTArray<nsCString>& aTrackingFullHashes) {
    DebugOnly<bool> isCookiesBlockedTracker =
        aType == nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;
    MOZ_ASSERT_IF(aBlocked, aReason.isNothing());
    MOZ_ASSERT_IF(!isCookiesBlockedTracker, aReason.isNothing());
    MOZ_ASSERT_IF(isCookiesBlockedTracker && !aBlocked, aReason.isSome());

    if (aOrigin.IsVoid()) {
      return;
    }
    auto index = mLog.IndexOf(aOrigin, 0, Comparator());
    if (index != OriginDataTable::NoIndex) {
      OriginEntry& entry = mLog[index];
      if (!entry.mData) {
        return;
      }

      if (aType == nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT) {
        entry.mData->mHasTrackingContentLoaded = aBlocked;
        return;
      }
      if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED) {
        if (entry.mData->mHasCookiesLoaded.isSome()) {
          entry.mData->mHasCookiesLoaded.ref() = aBlocked;
        } else {
          entry.mData->mHasCookiesLoaded.emplace(aBlocked);
        }
        return;
      }
      if (!entry.mData->mLogs.IsEmpty()) {
        auto& last = entry.mData->mLogs.LastElement();
        if (last.mType == aType && last.mBlocked == aBlocked) {
          ++last.mRepeatCount;
          // Don't record recorded events.  This helps compress our log.
          // We don't care about if the the reason is the same, just keep the
          // first one.
          // Note: {aReason, aTrackingFullHashes} are not compared here and we
          // simply keep the first ones.
#ifdef DEBUG
          for (const auto& hash : aTrackingFullHashes) {
            MOZ_ASSERT(last.mTrackingFullHashes.Contains(hash));
          }
#endif
          return;
        }
      }
      if (entry.mData->mLogs.Length() ==
          std::max(1u,
                   StaticPrefs::browser_contentblocking_originlog_length())) {
        // Cap the size at the maximum length adjustable by the pref
        entry.mData->mLogs.RemoveElementAt(0);
      }
      entry.mData->mLogs.AppendElement(
          LogEntry{aType, 1u, aBlocked, aReason,
                   nsTArray<nsCString>(aTrackingFullHashes)});
      return;
    }

    // The entry has not been found.

    OriginEntry* entry = mLog.AppendElement();
    if (NS_WARN_IF(!entry || !entry->mData)) {
      return;
    }

    entry->mOrigin = aOrigin;

    if (aType == nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT) {
      entry->mData->mHasTrackingContentLoaded = aBlocked;
    } else if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED) {
      MOZ_ASSERT(entry->mData->mHasCookiesLoaded.isNothing());
      entry->mData->mHasCookiesLoaded.emplace(aBlocked);
    } else {
      entry->mData->mLogs.AppendElement(
          LogEntry{aType, 1u, aBlocked, aReason,
                   nsTArray<nsCString>(aTrackingFullHashes)});
    }
  }

  void ReportLog();

  nsAutoCString Stringify() {
    nsAutoCString buffer;

    JSONWriter w(MakeUnique<StringWriteFunc>(buffer));
    w.Start();

    for (const OriginEntry& entry : mLog) {
      if (!entry.mData) {
        continue;
      }

      w.StartArrayProperty(entry.mOrigin.get(), w.SingleLineStyle);

      if (entry.mData->mHasTrackingContentLoaded) {
        w.StartArrayElement(w.SingleLineStyle);
        {
          w.IntElement(nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT);
          w.BoolElement(true);  // blocked
          w.IntElement(1);      // repeat count
        }
        w.EndArray();
      }
      if (entry.mData->mHasCookiesLoaded.isSome()) {
        w.StartArrayElement(w.SingleLineStyle);
        {
          w.IntElement(nsIWebProgressListener::STATE_COOKIES_LOADED);
          w.BoolElement(entry.mData->mHasCookiesLoaded.value());  // blocked
          w.IntElement(1);  // repeat count
        }
        w.EndArray();
      }
      for (const LogEntry& item : entry.mData->mLogs) {
        w.StartArrayElement(w.SingleLineStyle);
        {
          w.IntElement(item.mType);
          w.BoolElement(item.mBlocked);
          w.IntElement(item.mRepeatCount);
          if (item.mReason.isSome()) {
            w.IntElement(item.mReason.value());
          }
        }
        w.EndArray();
      }
      w.EndArray();
    }

    w.End();

    return buffer;
  }

  bool HasBlockedAnyOfType(uint32_t aType) const {
    // Note: nothing inside this loop should return false, the goal for the
    // loop is to scan the log to see if we find a matching entry, and if so
    // we would return true, otherwise in the end of the function outside of
    // the loop we take the common `return false;` statement.
    for (const OriginEntry& entry : mLog) {
      if (!entry.mData) {
        continue;
      }

      if (aType == nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT) {
        if (entry.mData->mHasTrackingContentLoaded) {
          return true;
        }
      } else if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED) {
        if (entry.mData->mHasCookiesLoaded.isSome() &&
            entry.mData->mHasCookiesLoaded.value()) {
          return true;
        }
      } else {
        for (const auto& item : entry.mData->mLogs) {
          if (((item.mType & aType) != 0) && item.mBlocked) {
            return true;
          }
        }
      }
    }
    return false;
  }

  void AddSizeOfExcludingThis(nsWindowSizes& aSizes) const {
    aSizes.mDOMOtherSize +=
        mLog.ShallowSizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

    // Now add the sizes of each origin log queue.
    for (const OriginEntry& entry : mLog) {
      if (entry.mData) {
        aSizes.mDOMOtherSize += aSizes.mState.mMallocSizeOf(entry.mData.get()) +
                                entry.mData->mLogs.ShallowSizeOfExcludingThis(
                                    aSizes.mState.mMallocSizeOf);
      }
    }
  }

 private:
  OriginDataTable mLog;
};

}  // namespace dom
}  // namespace mozilla

#endif
