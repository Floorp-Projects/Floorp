/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AutoTimelineMarker_h_
#define mozilla_AutoTimelineMarker_h_

#include "mozilla/GuardObjects.h"
#include "mozilla/nsRefPtr.h"

class nsIDocShell;
class nsDocShell;

namespace mozilla {

// # AutoTimelineMarker
//
// An RAII class to trace some task in the platform by adding a start and end
// timeline marker pair. These markers are then rendered in the devtools'
// performance tool's waterfall graph.
//
// Example usage:
//
//     {
//       AutoTimelineMarker marker(mDocShell, "Parse CSS");
//       nsresult rv = ParseTheCSSFile(mFile);
//       ...
//     }
class MOZ_RAII AutoTimelineMarker
{
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;

  // The name of the marker we are adding.
  const char* mName;

  // The docshell that is associated with this marker.
  nsRefPtr<nsDocShell> mDocShell;

public:
  explicit AutoTimelineMarker(nsIDocShell* aDocShell, const char* aName
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
  ~AutoTimelineMarker();

  AutoTimelineMarker(const AutoTimelineMarker& aOther) = delete;
  void operator=(const AutoTimelineMarker& aOther) = delete;
};

} // namespace mozilla

#endif /* mozilla_AutoTimelineMarker_h_ */
