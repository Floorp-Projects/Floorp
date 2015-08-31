/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TimelineMarker_h_
#define mozilla_TimelineMarker_h_

#include "nsString.h"
#include "nsContentUtils.h"
#include "GeckoProfiler.h"

class nsDocShell;

namespace mozilla {
namespace dom {
struct ProfileTimelineMarker;
}

// Objects of this type can be added to the timeline if there is an interested
// consumer. The class can also be subclassed to let a given marker creator
// provide custom details.
class TimelineMarker
{
public:
  enum TimelineStackRequest { STACK, NO_STACK };

  TimelineMarker(const char* aName,
                 TracingMetadata aMetaData,
                 TimelineStackRequest aStackRequest = STACK);

  TimelineMarker(const char* aName,
                 const TimeStamp& aTime,
                 TracingMetadata aMetaData,
                 TimelineStackRequest aStackRequest = STACK);

  virtual ~TimelineMarker();

  // Check whether two markers should be considered the same, for the purpose
  // of pairing start and end markers. Normally this definition suffices.
  virtual bool Equals(const TimelineMarker& aOther)
  {
    return strcmp(mName, aOther.mName) == 0;
  }

  // Add details specific to this marker type to aMarker. The standard elements
  // have already been set. This method is called on both the starting and
  // ending markers of a pair. Ordinarily the ending marker doesn't need to do
  // anything here.
  virtual void AddDetails(JSContext* aCx, dom::ProfileTimelineMarker& aMarker)
  {}

  const char* GetName() const { return mName; }
  DOMHighResTimeStamp GetTime() const { return mTime; }
  TracingMetadata GetMetaData() const { return mMetaData; }

  JSObject* GetStack()
  {
    if (mStackTrace.initialized()) {
      return mStackTrace;
    }
    return nullptr;
  }

protected:
  void CaptureStack()
  {
    JSContext* ctx = nsContentUtils::GetCurrentJSContext();
    if (ctx) {
      JS::RootedObject stack(ctx);
      if (JS::CaptureCurrentStack(ctx, &stack)) {
        mStackTrace.init(ctx, stack.get());
      } else {
        JS_ClearPendingException(ctx);
      }
    }
  }

private:
  const char* mName;
  DOMHighResTimeStamp mTime;
  TracingMetadata mMetaData;

  // While normally it is not a good idea to make a persistent root,
  // in this case changing nsDocShell to participate in cycle
  // collection was deemed too invasive, and the markers are only held
  // here temporarily to boot.
  JS::PersistentRooted<JSObject*> mStackTrace;

  void SetCurrentTime();
  void SetCustomTime(const TimeStamp& aTime);
  void CaptureStackIfNecessary(TracingMetadata aMetaData,
                               TimelineStackRequest aStackRequest);
};

} // namespace mozilla

#endif /* mozilla_TimelineMarker_h_ */
