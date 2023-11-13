/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMNavigationTiming_h___
#define nsDOMNavigationTiming_h___

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/RelativeTimeline.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/BaseProfilerMarkersPrerequisites.h"
#include "nsITimer.h"

class nsDocShell;
class nsIURI;

using DOMTimeMilliSec = unsigned long long;
using DOMHighResTimeStamp = double;

class PickleIterator;
namespace IPC {
class Message;
class MessageReader;
class MessageWriter;
}  // namespace IPC
namespace mozilla::ipc {
class IProtocol;
template <typename>
struct IPDLParamTraits;
}  // namespace mozilla::ipc

class nsDOMNavigationTiming final : public mozilla::RelativeTimeline {
 public:
  enum Type {
    TYPE_NAVIGATE = 0,
    TYPE_RELOAD = 1,
    TYPE_BACK_FORWARD = 2,
    TYPE_RESERVED = 255,
  };

  explicit nsDOMNavigationTiming(nsDocShell* aDocShell);

  NS_INLINE_DECL_REFCOUNTING(nsDOMNavigationTiming)

  Type GetType() const { return mNavigationType; }

  inline DOMHighResTimeStamp GetNavigationStartHighRes() const {
    return mNavigationStartHighRes;
  }

  DOMTimeMilliSec GetNavigationStart() const {
    return static_cast<int64_t>(GetNavigationStartHighRes());
  }

  mozilla::TimeStamp GetNavigationStartTimeStamp() const {
    return mNavigationStart;
  }

  mozilla::TimeStamp GetLoadEventStartTimeStamp() const {
    return mLoadEventStart;
  }

  mozilla::TimeStamp GetDOMContentLoadedEventStartTimeStamp() const {
    return mDOMContentLoadedEventStart;
  }

  mozilla::TimeStamp GetFirstContentfulCompositeTimeStamp() const {
    return mContentfulComposite;
  }

  mozilla::TimeStamp GetLargestContentfulRenderTimeStamp() const {
    return mLargestContentfulRender;
  }

  DOMTimeMilliSec GetUnloadEventStart() {
    return TimeStampToDOM(GetUnloadEventStartTimeStamp());
  }

  DOMTimeMilliSec GetUnloadEventEnd() {
    return TimeStampToDOM(GetUnloadEventEndTimeStamp());
  }

  DOMTimeMilliSec GetDomLoading() const { return TimeStampToDOM(mDOMLoading); }
  DOMTimeMilliSec GetDomInteractive() const {
    return TimeStampToDOM(mDOMInteractive);
  }
  DOMTimeMilliSec GetDomContentLoadedEventStart() const {
    return TimeStampToDOM(mDOMContentLoadedEventStart);
  }
  DOMTimeMilliSec GetDomContentLoadedEventEnd() const {
    return TimeStampToDOM(mDOMContentLoadedEventEnd);
  }
  DOMTimeMilliSec GetDomComplete() const {
    return TimeStampToDOM(mDOMComplete);
  }
  DOMTimeMilliSec GetLoadEventStart() const {
    return TimeStampToDOM(mLoadEventStart);
  }
  DOMTimeMilliSec GetLoadEventEnd() const {
    return TimeStampToDOM(mLoadEventEnd);
  }
  DOMTimeMilliSec GetTimeToNonBlankPaint() const {
    return TimeStampToDOM(mNonBlankPaint);
  }
  DOMTimeMilliSec GetTimeToContentfulComposite() const {
    return TimeStampToDOM(mContentfulComposite);
  }
  DOMTimeMilliSec GetTimeToLargestContentfulRender() const {
    return TimeStampToDOM(mLargestContentfulRender);
  }
  DOMTimeMilliSec GetTimeToTTFI() const { return TimeStampToDOM(mTTFI); }
  DOMTimeMilliSec GetTimeToDOMContentFlushed() const {
    return TimeStampToDOM(mDOMContentFlushed);
  }

  DOMHighResTimeStamp GetUnloadEventStartHighRes() {
    mozilla::TimeStamp stamp = GetUnloadEventStartTimeStamp();
    if (stamp.IsNull()) {
      return 0;
    }
    return TimeStampToDOMHighRes(stamp);
  }
  DOMHighResTimeStamp GetUnloadEventEndHighRes() {
    mozilla::TimeStamp stamp = GetUnloadEventEndTimeStamp();
    if (stamp.IsNull()) {
      return 0;
    }
    return TimeStampToDOMHighRes(stamp);
  }
  DOMHighResTimeStamp GetDomInteractiveHighRes() const {
    return TimeStampToDOMHighRes(mDOMInteractive);
  }
  DOMHighResTimeStamp GetDomContentLoadedEventStartHighRes() const {
    return TimeStampToDOMHighRes(mDOMContentLoadedEventStart);
  }
  DOMHighResTimeStamp GetDomContentLoadedEventEndHighRes() const {
    return TimeStampToDOMHighRes(mDOMContentLoadedEventEnd);
  }
  DOMHighResTimeStamp GetDomCompleteHighRes() const {
    return TimeStampToDOMHighRes(mDOMComplete);
  }
  DOMHighResTimeStamp GetLoadEventStartHighRes() const {
    return TimeStampToDOMHighRes(mLoadEventStart);
  }
  DOMHighResTimeStamp GetLoadEventEndHighRes() const {
    return TimeStampToDOMHighRes(mLoadEventEnd);
  }

  enum class DocShellState : uint8_t { eActive, eInactive };

  void NotifyNavigationStart(DocShellState aDocShellState);
  void NotifyFetchStart(nsIURI* aURI, Type aNavigationType);
  // A restoration occurs when the document is loaded from the
  // bfcache. This method sets the appropriate parameters of the
  // navigation timing object in this case.
  void NotifyRestoreStart();
  void NotifyBeforeUnload();
  void NotifyUnloadAccepted(nsIURI* aOldURI);
  void NotifyUnloadEventStart();
  void NotifyUnloadEventEnd();
  void NotifyLoadEventStart();
  void NotifyLoadEventEnd();

  // Document changes state to 'loading' before connecting to timing
  void SetDOMLoadingTimeStamp(nsIURI* aURI, mozilla::TimeStamp aValue);
  void NotifyDOMLoading(nsIURI* aURI);
  void NotifyDOMInteractive(nsIURI* aURI);
  void NotifyDOMComplete(nsIURI* aURI);
  void NotifyDOMContentLoadedStart(nsIURI* aURI);
  void NotifyDOMContentLoadedEnd(nsIURI* aURI);

  static void TTITimeoutCallback(nsITimer* aTimer, void* aClosure);
  void TTITimeout(nsITimer* aTimer);

  void NotifyLongTask(mozilla::TimeStamp aWhen);
  void NotifyNonBlankPaintForRootContentDocument();
  void NotifyContentfulCompositeForRootContentDocument(
      const mozilla::TimeStamp& aCompositeEndTime);
  void NotifyLargestContentfulRenderForRootContentDocument(
      const DOMHighResTimeStamp& aRenderTime);
  void NotifyDOMContentFlushedForRootContentDocument();
  void NotifyDocShellStateChanged(DocShellState aDocShellState);

  void MaybeAddLCPProfilerMarker(mozilla::MarkerInnerWindowId aInnerWindowID);

  DOMTimeMilliSec TimeStampToDOM(mozilla::TimeStamp aStamp) const;

  inline DOMHighResTimeStamp TimeStampToDOMHighRes(
      mozilla::TimeStamp aStamp) const {
    if (aStamp.IsNull()) {
      return 0;
    }
    mozilla::TimeDuration duration = aStamp - mNavigationStart;
    return duration.ToMilliseconds();
  }

  // Called by the DocumentLoadListener before sending the timing information
  // to the new content process.
  void Anonymize(nsIURI* aFinalURI);

  inline already_AddRefed<nsDOMNavigationTiming> CloneNavigationTime(
      nsDocShell* aDocShell) const {
    RefPtr<nsDOMNavigationTiming> timing = new nsDOMNavigationTiming(aDocShell);
    timing->mNavigationStartHighRes = mNavigationStartHighRes;
    timing->mNavigationStart = mNavigationStart;
    return timing.forget();
  }

  bool DocShellHasBeenActiveSinceNavigationStart() {
    return mDocShellHasBeenActiveSinceNavigationStart;
  }

  mozilla::TimeStamp LoadEventEnd() { return mLoadEventEnd; }

 private:
  friend class nsDocShell;
  nsDOMNavigationTiming(nsDocShell* aDocShell, nsDOMNavigationTiming* aOther);
  nsDOMNavigationTiming(const nsDOMNavigationTiming&) = delete;
  ~nsDOMNavigationTiming();

  void Clear();

  mozilla::TimeStamp GetUnloadEventStartTimeStamp() const;
  mozilla::TimeStamp GetUnloadEventEndTimeStamp() const;

  bool IsTopLevelContentDocumentInContentProcess() const;

  // Should those be amended, the IPC serializer should be updated
  // accordingly.
  mozilla::WeakPtr<nsDocShell> mDocShell;

  nsCOMPtr<nsIURI> mUnloadedURI;
  nsCOMPtr<nsIURI> mLoadedURI;
  nsCOMPtr<nsITimer> mTTITimer;

  Type mNavigationType;
  DOMHighResTimeStamp mNavigationStartHighRes;
  mozilla::TimeStamp mNavigationStart;
  mozilla::TimeStamp mNonBlankPaint;
  mozilla::TimeStamp mContentfulComposite;
  mozilla::TimeStamp mLargestContentfulRender;
  mozilla::TimeStamp mDOMContentFlushed;

  mozilla::TimeStamp mBeforeUnloadStart;
  mozilla::TimeStamp mUnloadStart;
  mozilla::TimeStamp mUnloadEnd;
  mozilla::TimeStamp mLoadEventStart;
  mozilla::TimeStamp mLoadEventEnd;

  mozilla::TimeStamp mDOMLoading;
  mozilla::TimeStamp mDOMInteractive;
  mozilla::TimeStamp mDOMContentLoadedEventStart;
  mozilla::TimeStamp mDOMContentLoadedEventEnd;
  mozilla::TimeStamp mDOMComplete;

  mozilla::TimeStamp mTTFI;

  bool mDocShellHasBeenActiveSinceNavigationStart;

  friend struct mozilla::ipc::IPDLParamTraits<nsDOMNavigationTiming*>;
};

// IPDL serializer. Please be aware of the caveats in sending across
// the information and the potential resulting data leakage.
// For now, this serializer is to only be used under a very narrowed scope
// so that only the starting times are ever set.
namespace mozilla::ipc {
template <>
struct IPDLParamTraits<nsDOMNavigationTiming*> {
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    nsDOMNavigationTiming* aParam);
  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   RefPtr<nsDOMNavigationTiming>* aResult);
};

}  // namespace mozilla::ipc

#endif /* nsDOMNavigationTiming_h___ */
