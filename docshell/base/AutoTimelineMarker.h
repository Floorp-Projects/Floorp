/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AutoTimelineMarker_h__
#define AutoTimelineMarker_h__

#include "mozilla/GuardObjects.h"
#include "mozilla/Vector.h"

#include "nsRefPtr.h"

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
class MOZ_STACK_CLASS AutoTimelineMarker
{
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;

  nsRefPtr<nsDocShell> mDocShell;
  const char* mName;

  bool DocShellIsRecording(nsDocShell& aDocShell);

public:
  explicit AutoTimelineMarker(nsIDocShell* aDocShell, const char* aName
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
  ~AutoTimelineMarker();

  AutoTimelineMarker(const AutoTimelineMarker& aOther) = delete;
  void operator=(const AutoTimelineMarker& aOther) = delete;
};

// # AutoGlobalTimelineMarker
//
// Similar to `AutoTimelineMarker`, but adds its traced marker to all docshells,
// not a single particular one. This is useful for operations that aren't
// associated with any one particular doc shell, or when it isn't clear which
// doc shell triggered the operation.
//
// Example usage:
//
//     {
//       AutoGlobalTimelineMarker marker("Cycle Collection");
//       nsCycleCollector* cc = GetCycleCollector();
//       cc->Collect();
//       ...
//     }
class MOZ_STACK_CLASS AutoGlobalTimelineMarker
{
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;

  // True as long as no operation has failed, eg due to OOM.
  bool mOk;

  // The set of docshells that are being observed and will get markers.
  mozilla::Vector<nsRefPtr<nsDocShell>> mDocShells;

  // The name of the marker we are adding.
  const char* mName;

  void PopulateDocShells();

public:
  explicit AutoGlobalTimelineMarker(const char* aName
                                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

  ~AutoGlobalTimelineMarker();

  AutoGlobalTimelineMarker(const AutoGlobalTimelineMarker& aOther) = delete;
  void operator=(const AutoGlobalTimelineMarker& aOther) = delete;
};

} // namespace mozilla

#endif /* AutoTimelineMarker_h__ */
