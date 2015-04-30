/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AutoTimelineMarker_h__
#define AutoTimelineMarker_h__

#include "mozilla/GuardObjects.h"
#include "mozilla/Move.h"
#include "nsDocShell.h"
#include "nsRefPtr.h"

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

  bool
  DocShellIsRecording(nsDocShell& aDocShell)
  {
    bool isRecording = false;
    if (nsDocShell::gProfileTimelineRecordingsCount > 0) {
      aDocShell.GetRecordProfileTimelineMarkers(&isRecording);
    }
    return isRecording;
  }

public:
  explicit AutoTimelineMarker(nsIDocShell* aDocShell, const char* aName
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mDocShell(nullptr)
    , mName(aName)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    nsDocShell* docShell = static_cast<nsDocShell*>(aDocShell);
    if (docShell && DocShellIsRecording(*docShell)) {
      mDocShell = docShell;
      mDocShell->AddProfileTimelineMarker(mName, TRACING_INTERVAL_START);
    }
  }

  ~AutoTimelineMarker()
  {
    if (mDocShell) {
      mDocShell->AddProfileTimelineMarker(mName, TRACING_INTERVAL_END);
    }
  }

  AutoTimelineMarker(const AutoTimelineMarker& aOther) = delete;
  void operator=(const AutoTimelineMarker& aOther) = delete;
};

} // namespace mozilla

#endif /* AutoTimelineMarker_h__ */
