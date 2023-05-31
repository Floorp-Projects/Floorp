/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <memory>
#include "domstubs.h"
#include "mozilla/LinkedList.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DOMString.h"
#include "nsDOMNavigationTiming.h"
#include "nsHashKeys.h"
#include "nsTHashMap.h"

namespace mozilla::dom {
class WebrtcGlobalStatisticsHistoryCallback;
struct RTCStatsReportInternal;

struct WebrtcGlobalStatsHistory {
  // History preferences
  struct Pref {
    static auto Enabled() -> bool;
    static auto PollIntervalMs() -> uint32_t;
    static auto StorageWindowS() -> uint32_t;
    static auto PruneAfterM() -> uint32_t;
    static auto ClosedStatsToRetain() -> uint32_t;
    Pref() = delete;
    ~Pref() = delete;
  };

  struct Entry {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Entry)
    // We need to wrap the report in an element
    struct ReportElement : public LinkedListElement<ReportElement> {
      UniquePtr<RTCStatsReportInternal> report;
      auto Timestamp() const -> DOMHighResTimeStamp;
      virtual ~ReportElement() = default;
    };
    AutoCleanLinkedList<ReportElement> mReports;
    bool mIsLongTermStatsDisabled;
    bool mIsClosed = false;
    auto Since(const Maybe<DOMHighResTimeStamp>& aAfter) const
        -> nsTArray<RTCStatsReportInternal>;

    static auto MakeElement(UniquePtr<RTCStatsReportInternal> aReport)
        -> ReportElement*;
    auto Prune(const DOMHighResTimeStamp aBefore) -> void;

   private:
    virtual ~Entry() = default;
  };
  using StatsMap = nsTHashMap<nsStringHashKey, RefPtr<Entry> >;
  static auto InitHistory(const nsAString& aPcId,
                          const bool aIsLongTermStatsDisabled) -> void;
  static auto Record(UniquePtr<RTCStatsReportInternal> aReport) -> void;
  static auto CloseHistory(const nsAString& aPcId) -> void;
  static auto GetHistory(const nsAString& aPcId) -> Maybe<RefPtr<Entry> >;
  static auto Clear() -> void;
  static auto PcIds() -> dom::Sequence<nsString>;

  WebrtcGlobalStatsHistory() = delete;

 private:
  static auto GetOrCreateHistory(UniquePtr<RTCStatsReportInternal> aReport)
      -> RefPtr<Entry>;
  static auto Get() -> StatsMap&;
};
}  // namespace mozilla::dom
