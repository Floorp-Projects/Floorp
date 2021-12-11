/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AutoGlobalTimelineMarker_h_
#define mozilla_AutoGlobalTimelineMarker_h_

#include "mozilla/Attributes.h"
#include "TimelineMarkerEnums.h"

namespace mozilla {

// # AutoGlobalTimelineMarker
//
// Similar to `AutoTimelineMarker`, but adds its traced marker to all docshells,
// not a single particular one. This is useful for operations that aren't
// associated with any one particular doc shell, or when it isn't clear which
// docshell triggered the operation.
//
// Example usage:
//
//     {
//       AutoGlobalTimelineMarker marker("Cycle Collection");
//       nsCycleCollector* cc = GetCycleCollector();
//       cc->Collect();
//       ...
//     }
class MOZ_RAII AutoGlobalTimelineMarker {
  // The name of the marker we are adding.
  const char* mName;
  // Whether to capture the JS stack or not.
  MarkerStackRequest mStackRequest;

 public:
  explicit AutoGlobalTimelineMarker(
      const char* aName,
      MarkerStackRequest aStackRequest = MarkerStackRequest::STACK);
  ~AutoGlobalTimelineMarker();

  AutoGlobalTimelineMarker(const AutoGlobalTimelineMarker& aOther) = delete;
  void operator=(const AutoGlobalTimelineMarker& aOther) = delete;
};

}  // namespace mozilla

#endif /* mozilla_AutoGlobalTimelineMarker_h_ */
