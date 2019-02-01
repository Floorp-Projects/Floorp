/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentBlockingLog_h
#define mozilla_dom_ContentBlockingLog_h

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
  struct LogEntry {
    uint32_t mType;
    uint32_t mRepeatCount;
    bool mBlocked;
  };

  // Each element is a tuple of (type, blocked, repeatCount). The type values
  // come from the blocking types defined in nsIWebProgressListener.
  typedef nsTArray<LogEntry> OriginLog;
  typedef Tuple<bool, Maybe<bool>, OriginLog> OriginData;
  typedef nsTArray<Tuple<nsCString, UniquePtr<OriginData>>> OriginDataTable;

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
      return Get<0>(aLeft).Equals(Get<0>(aRight));
    }

    bool Equals(const OriginDataTable::elem_type& aLeft,
                const nsACString& aRight) const {
      return Get<0>(aLeft).Equals(aRight);
    }
  };

 public:
  ContentBlockingLog() = default;
  ~ContentBlockingLog() = default;

  void RecordLog(const nsACString& aOrigin, uint32_t aType, bool aBlocked) {
    if (aOrigin.IsVoid()) {
      return;
    }
    auto index = mLog.IndexOf(aOrigin, 0, Comparator());
    if (index != OriginDataTable::NoIndex) {
      auto& data = Get<1>(mLog[index]);
      if (aType == nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT) {
        Get<0>(*data) = aBlocked;
        return;
      }
      if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED) {
        if (Get<1>(*data).isSome()) {
          Get<1>(*data).ref() = aBlocked;
        } else {
          Get<1>(*data).emplace(aBlocked);
        }
        return;
      }
      auto& log = Get<2>(*data);
      if (!log.IsEmpty()) {
        auto& last = log.LastElement();
        if (last.mType == aType && last.mBlocked == aBlocked) {
          ++last.mRepeatCount;
          // Don't record recorded events.  This helps compress our log.
          return;
        }
      }
      if (log.Length() ==
          std::max(1u,
                   StaticPrefs::browser_contentblocking_originlog_length())) {
        // Cap the size at the maximum length adjustable by the pref
        log.RemoveElementAt(0);
      }
      log.AppendElement(LogEntry{aType, 1u, aBlocked});
    } else {
      auto data = MakeUnique<OriginData>(false, Maybe<bool>(), OriginLog());
      if (aType == nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT) {
        Get<0>(*data) = aBlocked;
      } else if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED) {
        if (Get<1>(*data).isSome()) {
          Get<1>(*data).ref() = aBlocked;
        } else {
          Get<1>(*data).emplace(aBlocked);
        }
      } else {
        Get<2>(*data).AppendElement(LogEntry{aType, 1u, aBlocked});
      }
      nsAutoCString origin(aOrigin);
      mLog.AppendElement(Tuple<nsCString, UniquePtr<OriginData>>(
          std::move(origin), std::move(data)));
    }
  }

  nsAutoCString Stringify() {
    nsAutoCString buffer;

    JSONWriter w(MakeUnique<StringWriteFunc>(buffer));
    w.Start();

    const auto end = mLog.end();
    for (auto iter = mLog.begin(); iter != end; ++iter) {
      if (!Get<1>(*iter)) {
        w.StartArrayProperty(Get<0>(*iter).get(), w.SingleLineStyle);
        w.EndArray();
        continue;
      }

      w.StartArrayProperty(Get<0>(*iter).get(), w.SingleLineStyle);
      auto& data = Get<1>(*iter);
      if (Get<0>(*data)) {
        w.StartArrayElement(w.SingleLineStyle);
        {
          w.IntElement(nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT);
          w.BoolElement(true);  // blocked
          w.IntElement(1);      // repeat count
        }
        w.EndArray();
      }
      if (Get<1>(*data).isSome()) {
        w.StartArrayElement(w.SingleLineStyle);
        {
          w.IntElement(nsIWebProgressListener::STATE_COOKIES_LOADED);
          w.BoolElement(Get<1>(*data).value());  // blocked
          w.IntElement(1);                       // repeat count
        }
        w.EndArray();
      }
      for (auto& item : Get<2>(*data)) {
        w.StartArrayElement(w.SingleLineStyle);
        {
          w.IntElement(item.mType);
          w.BoolElement(item.mBlocked);
          w.IntElement(item.mRepeatCount);
        }
        w.EndArray();
      }
      w.EndArray();
    }

    w.End();

    return buffer;
  }

  bool HasBlockedAnyOfType(uint32_t aType) {
    // Note: nothing inside this loop should return false, the goal for the
    // loop is to scan the log to see if we find a matching entry, and if so
    // we would return true, otherwise in the end of the function outside of
    // the loop we take the common `return false;` statement.
    const auto end = mLog.end();
    for (auto iter = mLog.begin(); iter != end; ++iter) {
      if (!Get<1>(*iter)) {
        continue;
      }

      if (aType == nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT) {
        if (Get<0>(*Get<1>(*iter))) {
          return true;
        }
      } else if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED) {
        if (Get<1>(*Get<1>(*iter)).isSome() && Get<1>(*Get<1>(*iter)).value()) {
          return true;
        }
      } else {
        for (auto& item : Get<2>(*Get<1>(*iter))) {
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
    const auto end = mLog.end();
    for (auto iter = mLog.begin(); iter != end; ++iter) {
      if (!Get<1>(*iter)) {
        aSizes.mDOMOtherSize +=
            aSizes.mState.mMallocSizeOf(Get<1>(*iter).get()) +
            Get<2>(*Get<1>(*iter))
                .ShallowSizeOfExcludingThis(aSizes.mState.mMallocSizeOf);
      }
    }
  }

 private:
  OriginDataTable mLog;
};

}  // namespace dom
}  // namespace mozilla

#endif
