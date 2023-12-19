/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGlobalStatsHistory.h"

#include "domstubs.h"
#include "mozilla/LinkedList.h"
#include "mozilla/dom/RTCStatsReportBinding.h"  // for RTCStatsReportInternal
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/fallible.h"
#include "mozilla/mozalloc_oom.h"
#include "nsDOMNavigationTiming.h"

namespace mozilla::dom {

constexpr auto SEC_TO_MS(const DOMHighResTimeStamp sec) -> DOMHighResTimeStamp {
  return sec * 1000.0;
}

constexpr auto MIN_TO_MS(const DOMHighResTimeStamp min) -> DOMHighResTimeStamp {
  return SEC_TO_MS(min * 60.0);
}

// Prefs
auto WebrtcGlobalStatsHistory::Pref::Enabled() -> bool {
  return mozilla::StaticPrefs::media_aboutwebrtc_hist_enabled();
}

auto WebrtcGlobalStatsHistory::Pref::PollIntervalMs() -> uint32_t {
  return mozilla::StaticPrefs::media_aboutwebrtc_hist_poll_interval_ms();
}

auto WebrtcGlobalStatsHistory::Pref::StorageWindowS() -> uint32_t {
  return mozilla::StaticPrefs::media_aboutwebrtc_hist_storage_window_s();
}

auto WebrtcGlobalStatsHistory::Pref::PruneAfterM() -> uint32_t {
  return mozilla::StaticPrefs::media_aboutwebrtc_hist_prune_after_m();
}

auto WebrtcGlobalStatsHistory::Pref::ClosedStatsToRetain() -> uint32_t {
  return mozilla::StaticPrefs::media_aboutwebrtc_hist_closed_stats_to_retain();
}

auto WebrtcGlobalStatsHistory::Get() -> WebrtcGlobalStatsHistory::StatsMap& {
  static StaticAutoPtr<StatsMap> sHist;
  if (!sHist) {
    sHist = new StatsMap();
    ClearOnShutdown(&sHist);
  }
  return *sHist;
}

auto WebrtcGlobalStatsHistory::Entry::ReportElement::Timestamp() const
    -> DOMHighResTimeStamp {
  return report->mTimestamp;
}

auto WebrtcGlobalStatsHistory::Entry::SdpElement::Timestamp() const
    -> DOMHighResTimeStamp {
  return sdp.mTimestamp;
}

auto WebrtcGlobalStatsHistory::Entry::MakeReportElement(
    UniquePtr<RTCStatsReportInternal> aReport)
    -> WebrtcGlobalStatsHistory::Entry::ReportElement* {
  auto* elem = new ReportElement();
  elem->report = std::move(aReport);
  // We don't want to store a copy of the SDP history with each stats entry.
  // SDP History is stored seperately, see MakeSdpElements.
  elem->report->mSdpHistory.Clear();
  return elem;
}

auto WebrtcGlobalStatsHistory::Entry::MakeSdpElementsSince(
    Sequence<RTCSdpHistoryEntryInternal>&& aSdpHistory,
    const Maybe<DOMHighResTimeStamp>& aSdpAfter)
    -> AutoCleanLinkedList<WebrtcGlobalStatsHistory::Entry::SdpElement> {
  AutoCleanLinkedList<WebrtcGlobalStatsHistory::Entry::SdpElement> result;
  for (auto& sdpHist : aSdpHistory) {
    if (!aSdpAfter || aSdpAfter.value() < sdpHist.mTimestamp) {
      auto* element = new SdpElement();
      element->sdp = sdpHist;
      result.insertBack(element);
    }
  }
  return result;
}

template <typename T>
auto FindFirstEntryAfter(const T* first,
                         const Maybe<DOMHighResTimeStamp>& aAfter) -> const T* {
  const auto* current = first;
  while (aAfter && current && current->Timestamp() <= aAfter.value()) {
    current = current->getNext();
  }
  return current;
}

template <typename T>
auto CountElementsToEndInclusive(const LinkedListElement<T>* elem) -> size_t {
  size_t count = 0;
  const auto* cursor = elem;
  while (cursor && cursor->isInList()) {
    count++;
    cursor = cursor->getNext();
  }
  return count;
}

auto WebrtcGlobalStatsHistory::Entry::Since(
    const Maybe<DOMHighResTimeStamp>& aAfter) const
    -> nsTArray<RTCStatsReportInternal> {
  nsTArray<RTCStatsReportInternal> results;
  const auto* cursor = FindFirstEntryAfter(mReports.getFirst(), aAfter);
  const auto count = CountElementsToEndInclusive(cursor);
  if (!results.SetCapacity(count, fallible)) {
    mozalloc_handle_oom(0);
  }
  while (cursor) {
    results.AppendElement(RTCStatsReportInternal(*cursor->report));
    cursor = cursor->getNext();
  }
  return results;
}

auto WebrtcGlobalStatsHistory::Entry::SdpSince(
    const Maybe<DOMHighResTimeStamp>& aAfter) const -> RTCSdpHistoryInternal {
  RTCSdpHistoryInternal results;
  results.mPcid = mPcid;
  // If no timestamp was passed copy the entire history
  const auto* cursor = FindFirstEntryAfter(mSdp.getFirst(), aAfter);
  const auto count = CountElementsToEndInclusive(cursor);
  if (!results.mSdpHistory.SetCapacity(count, fallible)) {
    mozalloc_handle_oom(0);
  }
  while (cursor) {
    if (!results.mSdpHistory.AppendElement(
            RTCSdpHistoryEntryInternal(cursor->sdp), fallible)) {
      mozalloc_handle_oom(0);
    }
    cursor = cursor->getNext();
  }
  return results;
}

auto WebrtcGlobalStatsHistory::Entry::Prune(const DOMHighResTimeStamp aBefore)
    -> void {
  // Clear everything in the case that we don't keep stats
  if (mIsLongTermStatsDisabled) {
    mReports.clear();
  }
  // Clear everything before the cutoff
  for (auto* element = mReports.getFirst();
       element && element->report->mTimestamp < aBefore;
       element = mReports.getFirst()) {
    delete mReports.popFirst();
  }
  // I don't think we should prune SDPs but if we did it would look like this:
  // Note: we always keep the most recent SDP
}

auto WebrtcGlobalStatsHistory::InitHistory(const nsAString& aPcId,
                                           const bool aIsLongTermStatsDisabled)
    -> void {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (WebrtcGlobalStatsHistory::Get().MaybeGet(aPcId)) {
    return;
  }
  WebrtcGlobalStatsHistory::Get().GetOrInsertNew(aPcId, nsString(aPcId),
                                                 aIsLongTermStatsDisabled);
};

auto WebrtcGlobalStatsHistory::Record(UniquePtr<RTCStatsReportInternal> aReport)
    -> void {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Use the report timestamp as "now" for determining time depth
  // based pruning.
  const auto now = aReport->mTimestamp;
  const auto earliest = now - SEC_TO_MS(Pref::StorageWindowS());

  // Store closed state before moving the report
  const auto closed = aReport->mClosed;
  const auto pcId = aReport->mPcid;

  auto history = WebrtcGlobalStatsHistory::GetHistory(aReport->mPcid);
  if (history && Pref::Enabled()) {
    auto entry = history.value();
    // Remove expired entries
    entry->Prune(earliest);
    // Find new SDP entries
    auto sdpAfter = Maybe<DOMHighResTimeStamp>(Nothing());
    if (auto* lastSdp = entry->mSdp.getLast(); lastSdp) {
      sdpAfter = Some(lastSdp->Timestamp());
    }
    entry->mSdp.extendBack(
        Entry::MakeSdpElementsSince(std::move(aReport->mSdpHistory), sdpAfter));
    // Reports must be in ascending order by mTimestamp
    const auto* latest = entry->mReports.getLast();
    // Maintain sorted order
    if (!latest || latest->report->mTimestamp < aReport->mTimestamp) {
      entry->mReports.insertBack(Entry::MakeReportElement(std::move(aReport)));
    }
  }
  // Close the history if needed
  if (closed) {
    CloseHistory(pcId);
  }
}

auto WebrtcGlobalStatsHistory::CloseHistory(const nsAString& aPcId) -> void {
  MOZ_ASSERT(XRE_IsParentProcess());
  auto maybeHist = WebrtcGlobalStatsHistory::Get().MaybeGet(aPcId);
  if (!maybeHist) {
    return;
  }
  {
    auto&& hist = maybeHist.value();
    hist->mIsClosed = true;

    if (hist->mIsLongTermStatsDisabled) {
      WebrtcGlobalStatsHistory::Get().Remove(aPcId);
      return;
    }
  }
  size_t remainingClosedStatsToRetain =
      WebrtcGlobalStatsHistory::Pref::ClosedStatsToRetain();
  WebrtcGlobalStatsHistory::Get().RemoveIf([&](auto& iter) {
    auto& entry = iter.Data();
    if (!entry->mIsClosed) {
      return false;
    }
    if (entry->mIsLongTermStatsDisabled) {
      return true;
    }
    if (remainingClosedStatsToRetain > 0) {
      remainingClosedStatsToRetain -= 1;
      return false;
    }
    return true;
  });
}

auto WebrtcGlobalStatsHistory::Clear() -> void {
  MOZ_ASSERT(XRE_IsParentProcess());
  WebrtcGlobalStatsHistory::Get().RemoveIf([](auto& aIter) {
    // First clear all the closed histories.
    if (aIter.Data()->mIsClosed) {
      return true;
    }
    // For all remaining histories clear their stored reports
    aIter.Data()->mReports.clear();
    // As an optimization we don't clear the SDP, because that would
    // be reconstitued in the very next stats gathering polling period.
    // Those are potentially large allocations which we can skip.
    return false;
  });
}

auto WebrtcGlobalStatsHistory::PcIds() -> dom::Sequence<nsString> {
  MOZ_ASSERT(XRE_IsParentProcess());
  dom::Sequence<nsString> pcIds;
  for (const auto& pcId : WebrtcGlobalStatsHistory::Get().Keys()) {
    if (!pcIds.AppendElement(pcId, fallible)) {
      mozalloc_handle_oom(0);
    }
  }
  return pcIds;
}

auto WebrtcGlobalStatsHistory::GetHistory(const nsAString& aPcId)
    -> Maybe<RefPtr<Entry> > {
  MOZ_ASSERT(XRE_IsParentProcess());
  const auto pcid = NS_ConvertUTF16toUTF8(aPcId);

  return WebrtcGlobalStatsHistory::Get().MaybeGet(aPcId);
}
}  // namespace mozilla::dom
