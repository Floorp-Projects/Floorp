/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TimelineMarker_h__
#define TimelineMarker_h__

#include "nsString.h"
#include "GeckoProfiler.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "nsContentUtils.h"
#include "jsapi.h"

class nsDocShell;

// Objects of this type can be added to the timeline.  The class can
// also be subclassed to let a given marker creator provide custom
// details.
class TimelineMarker
{
public:
  TimelineMarker(nsDocShell* aDocShell, const char* aName,
                 TracingMetadata aMetaData);

  TimelineMarker(nsDocShell* aDocShell, const char* aName,
                 TracingMetadata aMetaData,
                 const nsAString& aCause);

  virtual ~TimelineMarker();

  // Check whether two markers should be considered the same,
  // for the purpose of pairing start and end markers.  Normally
  // this definition suffices.
  virtual bool Equals(const TimelineMarker* aOther)
  {
    return strcmp(mName, aOther->mName) == 0;
  }

  // Add details specific to this marker type to aMarker.  The
  // standard elements have already been set.  This method is
  // called on both the starting and ending markers of a pair.
  // Ordinarily the ending marker doesn't need to do anything
  // here.
  virtual void AddDetails(mozilla::dom::ProfileTimelineMarker& aMarker) {}

  virtual void AddLayerRectangles(
      mozilla::dom::Sequence<mozilla::dom::ProfileTimelineLayerRect>&)
  {
    MOZ_ASSERT_UNREACHABLE("can only be called on layer markers");
  }

  const char* GetName() const { return mName; }
  TracingMetadata GetMetaData() const { return mMetaData; }
  DOMHighResTimeStamp GetTime() const { return mTime; }
  const nsString& GetCause() const { return mCause; }

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
  TracingMetadata mMetaData;
  DOMHighResTimeStamp mTime;
  nsString mCause;

  // While normally it is not a good idea to make a persistent root,
  // in this case changing nsDocShell to participate in cycle
  // collection was deemed too invasive, and the markers are only held
  // here temporarily to boot.
  JS::PersistentRooted<JSObject*> mStackTrace;
};

#endif /* TimelineMarker_h__ */
