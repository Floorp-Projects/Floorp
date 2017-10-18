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
#include "mozilla/TimeStamp.h"

class nsDocShell;
class nsIURI;

typedef unsigned long long DOMTimeMilliSec;
typedef double DOMHighResTimeStamp;

class nsDOMNavigationTiming final
{
public:
  enum Type {
    TYPE_NAVIGATE = 0,
    TYPE_RELOAD = 1,
    TYPE_BACK_FORWARD = 2,
    TYPE_RESERVED = 255,
  };

  explicit nsDOMNavigationTiming(nsDocShell* aDocShell);

  NS_INLINE_DECL_REFCOUNTING(nsDOMNavigationTiming)

  Type GetType() const
  {
    return mNavigationType;
  }

  inline DOMHighResTimeStamp GetNavigationStartHighRes() const
  {
    return mNavigationStartHighRes;
  }

  DOMTimeMilliSec GetNavigationStart() const
  {
    return static_cast<int64_t>(GetNavigationStartHighRes());
  }

  mozilla::TimeStamp GetNavigationStartTimeStamp() const
  {
    return mNavigationStart;
  }

  DOMTimeMilliSec GetUnloadEventStart()
  {
    return TimeStampToDOM(GetUnloadEventStartTimeStamp());
  }

  DOMTimeMilliSec GetUnloadEventEnd()
  {
    return TimeStampToDOM(GetUnloadEventEndTimeStamp());
  }

  DOMTimeMilliSec GetDomLoading() const
  {
    return TimeStampToDOM(mDOMLoading);
  }
  DOMTimeMilliSec GetDomInteractive() const
  {
    return TimeStampToDOM(mDOMInteractive);
  }
  DOMTimeMilliSec GetDomContentLoadedEventStart() const
  {
    return TimeStampToDOM(mDOMContentLoadedEventStart);
  }
  DOMTimeMilliSec GetDomContentLoadedEventEnd() const
  {
    return TimeStampToDOM(mDOMContentLoadedEventEnd);
  }
  DOMTimeMilliSec GetDomComplete() const
  {
    return TimeStampToDOM(mDOMComplete);
  }
  DOMTimeMilliSec GetLoadEventStart() const
  {
    return TimeStampToDOM(mLoadEventStart);
  }
  DOMTimeMilliSec GetLoadEventEnd() const
  {
    return TimeStampToDOM(mLoadEventEnd);
  }
  DOMTimeMilliSec GetTimeToNonBlankPaint() const
  {
    return TimeStampToDOM(mNonBlankPaint);
  }

  DOMHighResTimeStamp GetUnloadEventStartHighRes()
  {
    mozilla::TimeStamp stamp = GetUnloadEventStartTimeStamp();
    if (stamp.IsNull()) {
      return 0;
    }
    return TimeStampToDOMHighRes(stamp);
  }
  DOMHighResTimeStamp GetUnloadEventEndHighRes()
  {
    mozilla::TimeStamp stamp = GetUnloadEventEndTimeStamp();
    if (stamp.IsNull()) {
      return 0;
    }
    return TimeStampToDOMHighRes(stamp);
  }
  DOMHighResTimeStamp GetDomInteractiveHighRes() const
  {
    return TimeStampToDOMHighRes(mDOMInteractive);
  }
  DOMHighResTimeStamp GetDomContentLoadedEventStartHighRes() const
  {
    return TimeStampToDOMHighRes(mDOMContentLoadedEventStart);
  }
  DOMHighResTimeStamp GetDomContentLoadedEventEndHighRes() const
  {
    return TimeStampToDOMHighRes(mDOMContentLoadedEventEnd);
  }
  DOMHighResTimeStamp GetDomCompleteHighRes() const
  {
    return TimeStampToDOMHighRes(mDOMComplete);
  }
  DOMHighResTimeStamp GetLoadEventStartHighRes() const
  {
    return TimeStampToDOMHighRes(mLoadEventStart);
  }
  DOMHighResTimeStamp GetLoadEventEndHighRes() const
  {
    return TimeStampToDOMHighRes(mLoadEventEnd);
  }

  enum class DocShellState : uint8_t {
    eActive,
    eInactive
  };

  void NotifyNavigationStart(DocShellState aDocShellState);
  void NotifyFetchStart(nsIURI* aURI, Type aNavigationType);
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

  void NotifyNonBlankPaintForRootContentDocument();
  void NotifyDocShellStateChanged(DocShellState aDocShellState);

  DOMTimeMilliSec TimeStampToDOM(mozilla::TimeStamp aStamp) const;

  inline DOMHighResTimeStamp TimeStampToDOMHighRes(mozilla::TimeStamp aStamp) const
  {
    if (aStamp.IsNull()) {
      return 0;
    }
    mozilla::TimeDuration duration = aStamp - mNavigationStart;
    return duration.ToMilliseconds();
  }

private:
  nsDOMNavigationTiming(const nsDOMNavigationTiming &) = delete;
  ~nsDOMNavigationTiming();

  void Clear();

  mozilla::TimeStamp GetUnloadEventStartTimeStamp() const;
  mozilla::TimeStamp GetUnloadEventEndTimeStamp() const;

  bool IsTopLevelContentDocumentInContentProcess() const;

  mozilla::WeakPtr<nsDocShell> mDocShell;

  nsCOMPtr<nsIURI> mUnloadedURI;
  nsCOMPtr<nsIURI> mLoadedURI;

  Type mNavigationType;
  DOMHighResTimeStamp mNavigationStartHighRes;
  mozilla::TimeStamp mNavigationStart;
  mozilla::TimeStamp mNonBlankPaint;

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

  bool mDocShellHasBeenActiveSinceNavigationStart : 1;
};

#endif /* nsDOMNavigationTiming_h___ */
