/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ObservedDocShell.h"

#include "TimelineMarker.h"
#include "LayerTimelineMarker.h"
#include "mozilla/Move.h"

namespace mozilla {

ObservedDocShell::ObservedDocShell(nsDocShell* aDocShell)
  : mDocShell(aDocShell)
{}

void
ObservedDocShell::AddMarker(UniquePtr<TimelineMarker>&& aMarker)
{
  mTimelineMarkers.AppendElement(Move(aMarker));
}

void
ObservedDocShell::ClearMarkers()
{
  mTimelineMarkers.Clear();
}

void
ObservedDocShell::PopMarkers(JSContext* aCx,
                             nsTArray<dom::ProfileTimelineMarker>& aStore)
{
  // If we see an unpaired START, we keep it around for the next call
  // to ObservedDocShell::PopMarkers. We store the kept START objects here.
  nsTArray<UniquePtr<TimelineMarker>> keptStartMarkers;

  for (uint32_t i = 0; i < mTimelineMarkers.Length(); ++i) {
    UniquePtr<TimelineMarker>& startPayload = mTimelineMarkers[i];

    // If this is a TRACING_TIMESTAMP marker, there's no corresponding END
    // as it's a single unit of time, not a duration.
    if (startPayload->GetMetaData() == TRACING_TIMESTAMP) {
      dom::ProfileTimelineMarker* marker = aStore.AppendElement();
      marker->mName = NS_ConvertUTF8toUTF16(startPayload->GetName());
      marker->mStart = startPayload->GetTime();
      marker->mEnd = startPayload->GetTime();
      marker->mStack = startPayload->GetStack();
      startPayload->AddDetails(aCx, *marker);
      continue;
    }

    // Whenever a START marker is found, look for the corresponding END
    // and build a {name,start,end} JS object.
    if (startPayload->GetMetaData() == TRACING_INTERVAL_START) {
      bool hasSeenEnd = false;

      // "Paint" markers are different because painting is handled at root
      // docshell level. The information that a paint was done is stored at
      // sub-docshell level, but we can only be sure that a paint did actually
      // happen in if a "Layer" marker was recorded too.
      bool startIsPaintType = strcmp(startPayload->GetName(), "Paint") == 0;
      bool hasSeenLayerType = false;

      // If we are processing a "Paint" marker, we append information from
      // all the embedded "Layer" markers to this array.
      dom::Sequence<dom::ProfileTimelineLayerRect> layerRectangles;

      // DOM events can be nested, so we must take care when searching
      // for the matching end. It doesn't hurt to apply this logic to
      // all event types.
      uint32_t markerDepth = 0;

      // The assumption is that the devtools timeline flushes markers frequently
      // enough for the amount of markers to always be small enough that the
      // nested for loop isn't going to be a performance problem.
      for (uint32_t j = i + 1; j < mTimelineMarkers.Length(); ++j) {
        UniquePtr<TimelineMarker>& endPayload = mTimelineMarkers[j];
        bool endIsLayerType = strcmp(endPayload->GetName(), "Layer") == 0;

        // Look for "Layer" markers to stream out "Paint" markers.
        if (startIsPaintType && endIsLayerType) {
          LayerTimelineMarker* layerPayload = static_cast<LayerTimelineMarker*>(endPayload.get());
          layerPayload->AddLayerRectangles(layerRectangles);
          hasSeenLayerType = true;
        }
        if (!startPayload->Equals(*endPayload)) {
          continue;
        }
        if (endPayload->GetMetaData() == TRACING_INTERVAL_START) {
          ++markerDepth;
          continue;
        }
        if (endPayload->GetMetaData() == TRACING_INTERVAL_END) {
          if (markerDepth > 0) {
            --markerDepth;
            continue;
          }
          if (!startIsPaintType || (startIsPaintType && hasSeenLayerType)) {
            dom::ProfileTimelineMarker* marker = aStore.AppendElement();
            marker->mName = NS_ConvertUTF8toUTF16(startPayload->GetName());
            marker->mStart = startPayload->GetTime();
            marker->mEnd = endPayload->GetTime();
            marker->mStack = startPayload->GetStack();
            if (hasSeenLayerType) {
              marker->mRectangles.Construct(layerRectangles);
            }
            startPayload->AddDetails(aCx, *marker);
            endPayload->AddDetails(aCx, *marker);
          }
          hasSeenEnd = true;
          break;
        }
      }

      // If we did not see the corresponding END, keep the START.
      if (!hasSeenEnd) {
        keptStartMarkers.AppendElement(Move(mTimelineMarkers[i]));
        mTimelineMarkers.RemoveElementAt(i);
        --i;
      }
    }
  }

  mTimelineMarkers.SwapElements(keptStartMarkers);
}

} // namespace mozilla
