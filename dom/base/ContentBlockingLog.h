/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentBlockingLog_h
#define mozilla_dom_ContentBlockingLog_h

#include "mozilla/JSONWriter.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/UniquePtr.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsReadableUtils.h"
#include "nsTArray.h"
#include "nsWindowSizes.h"

namespace mozilla {
namespace dom {

class ContentBlockingLog final
{
  struct LogEntry {
    uint32_t mType;
    uint32_t mRepeatCount;
    bool mBlocked;
  };

  // Each element is a tuple of (type, blocked, repeatCount). The type values
  // come from the blocking types defined in nsIWebProgressListener.
  typedef nsTArray<LogEntry> OriginLog;
  typedef nsClassHashtable<nsStringHashKey, OriginLog> OriginLogHashTable;

  struct StringWriteFunc : public JSONWriteFunc
  {
    nsAString& mBuffer; // The lifetime of the struct must be bound to the buffer
    explicit StringWriteFunc(nsAString& aBuffer)
      : mBuffer(aBuffer)
    {}

    void Write(const char* aStr) override
    {
      mBuffer.Append(NS_ConvertUTF8toUTF16(aStr));
    }
  };

public:
  ContentBlockingLog() = default;
  ~ContentBlockingLog() = default;

  void RecordLog(const nsAString& aOrigin, uint32_t aType, bool aBlocked)
  {
    if (aOrigin.IsVoid()) {
      return;
    }
    auto entry = mLog.LookupForAdd(aOrigin);
    if (entry) {
      auto& log = entry.Data();
      if (!log->IsEmpty()) {
        auto& last = log->LastElement();
        if (last.mType == aType &&
            last.mBlocked == aBlocked) {
          ++last.mRepeatCount;
          // Don't record recorded events.  This helps compress our log.
          return;
        }
      }
      if (log->Length() ==
            std::max(1u, StaticPrefs::browser_contentblocking_originlog_length())) {
        // Cap the size at the maximum length adjustable by the pref
        log->RemoveElementAt(0);
      }
      log->AppendElement(LogEntry{aType, 1u, aBlocked});
    } else {
      entry.OrInsert([=] {
        auto log(MakeUnique<OriginLog>());
        log->AppendElement(LogEntry{aType, 1u, aBlocked});
        return log.release();
      });
    }
  }

  nsAutoString Stringify()
  {
    nsAutoString buffer;

    JSONWriter w(MakeUnique<StringWriteFunc>(buffer));
    w.Start();

    for (auto iter = mLog.Iter(); !iter.Done(); iter.Next()) {
      if (!iter.UserData()) {
        w.StartArrayProperty(NS_ConvertUTF16toUTF8(iter.Key()).get(), w.SingleLineStyle);
        w.EndArray();
        continue;
      }

      w.StartArrayProperty(NS_ConvertUTF16toUTF8(iter.Key()).get(), w.SingleLineStyle);
      for (auto& item: *iter.UserData()) {
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

  void AddSizeOfExcludingThis(nsWindowSizes& aSizes) const
  {
    aSizes.mDOMOtherSize += mLog.ShallowSizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

    // Now add the sizes of each origin log queue.
    // The const_cast is needed because the nsTHashtable::Iterator interface is
    // not const-safe.  :-(
    for (auto iter = const_cast<OriginLogHashTable&>(mLog).Iter();
         !iter.Done(); iter.Next()) {
      if (iter.UserData()) {
        aSizes.mDOMOtherSize +=
          iter.UserData()->ShallowSizeOfIncludingThis(aSizes.mState.mMallocSizeOf);
      }
    }
  }

private:
  OriginLogHashTable mLog;
};

} // namespace dom
} // namespace mozilla

#endif
