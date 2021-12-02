/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Layers.h"

#include <inttypes.h>          // for PRIu64
#include <stdio.h>             // for stderr
#include <algorithm>           // for max, min
#include <list>                // for list
#include <set>                 // for set
#include <string>              // for char_traits, string, basic_string
#include <type_traits>         // for remove_reference<>::type
#include "CompositableHost.h"  // for CompositableHost
#include "LayerUserData.h"     // for LayerUserData
#include "TreeTraversal.h"  // for ForwardIterator, ForEachNode, DepthFirstSearch, TraversalFlag, TraversalFl...
#include "UnitTransforms.h"  // for ViewAs, PixelCastJustification, PixelCastJustification::RenderTargetIsPare...
#include "apz/src/AsyncPanZoomController.h"  // for AsyncPanZoomController
#include "gfx2DGlue.h"              // for ThebesMatrix, ToPoint, ThebesRect
#include "gfxEnv.h"                 // for gfxEnv
#include "gfxMatrix.h"              // for gfxMatrix
#include "gfxUtils.h"               // for gfxUtils, gfxUtils::sDumpPaintFile
#include "mozilla/ArrayIterator.h"  // for ArrayIterator
#include "mozilla/DebugOnly.h"      // for DebugOnly
#include "mozilla/Logging.h"  // for LogLevel, LogLevel::Debug, MOZ_LOG_TEST
#include "mozilla/ProfilerMarkers.h"  // for profiler_thread_is_being_profiled_for_markers, PROFILER_MARKER_TEXT
#include "mozilla/ScrollPositionUpdate.h"  // for ScrollPositionUpdate
#include "mozilla/Telemetry.h"             // for AccumulateTimeDelta
#include "mozilla/TelemetryHistogramEnums.h"  // for KEYPRESS_PRESENT_LATENCY, SCROLL_PRESENT_LATENCY
#include "mozilla/ToString.h"  // for ToString
#include "mozilla/gfx/2D.h"  // for SourceSurface, DrawTarget, DataSourceSurface
#include "mozilla/gfx/BasePoint3D.h"  // for BasePoint3D<>::(anonymous union)::(anonymous), BasePoint3D<>::(anonymous)
#include "mozilla/gfx/BaseRect.h"  // for operator<<, BaseRect (ptr only)
#include "mozilla/gfx/BaseSize.h"  // for operator<<, BaseSize<>::(anonymous union)::(anonymous), BaseSize<>::(anony...
#include "mozilla/gfx/Matrix.h"  // for Matrix4x4, Matrix, Matrix4x4Typed<>::(anonymous union)::(anonymous), Matri...
#include "mozilla/gfx/MatrixFwd.h"              // for Float
#include "mozilla/gfx/Polygon.h"                // for Polygon, PolygonTyped
#include "mozilla/layers/BSPTree.h"             // for LayerPolygon, BSPTree
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/Compositor.h"          // for Compositor
#include "mozilla/layers/LayersMessages.h"  // for SpecificLayerAttributes, CompositorAnimations (ptr only), ContainerLayerAt...
#include "mozilla/layers/LayersTypes.h"  // for EventRegions, operator<<, CompositionPayload, CSSTransformMatrix, MOZ_LAYE...
#include "nsBaseHashtable.h"  // for nsBaseHashtable<>::Iterator, nsBaseHashtable<>::LookupResult
#include "nsISupportsUtils.h"  // for NS_ADDREF, NS_RELEASE
#include "nsPrintfCString.h"   // for nsPrintfCString
#include "nsRegionFwd.h"       // for IntRegion
#include "nsString.h"          // for nsTSubstring

// Undo the damage done by mozzconf.h
#undef compress
#include "mozilla/Compression.h"

namespace mozilla {
namespace layers {

typedef ScrollableLayerGuid::ViewID ViewID;

using namespace mozilla::gfx;
using namespace mozilla::Compression;

#ifdef MOZ_DUMP_PAINTING
template <typename T>
void WriteSnapshotToDumpFile_internal(T* aObj, DataSourceSurface* aSurf) {
  nsCString string(aObj->Name());
  string.Append('-');
  string.AppendInt((uint64_t)aObj);
  if (gfxUtils::sDumpPaintFile != stderr) {
    fprintf_stderr(gfxUtils::sDumpPaintFile, R"(array["%s"]=")",
                   string.BeginReading());
  }
  gfxUtils::DumpAsDataURI(aSurf, gfxUtils::sDumpPaintFile);
  if (gfxUtils::sDumpPaintFile != stderr) {
    fprintf_stderr(gfxUtils::sDumpPaintFile, R"(";)");
  }
}

void WriteSnapshotToDumpFile(Compositor* aCompositor, DrawTarget* aTarget) {
  RefPtr<SourceSurface> surf = aTarget->Snapshot();
  RefPtr<DataSourceSurface> dSurf = surf->GetDataSurface();
  WriteSnapshotToDumpFile_internal(aCompositor, dSurf);
}
#endif

IntRect ToOutsideIntRect(const gfxRect& aRect) {
  return IntRect::RoundOut(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height());
}

void RecordCompositionPayloadsPresented(
    const TimeStamp& aCompositionEndTime,
    const nsTArray<CompositionPayload>& aPayloads) {
  if (aPayloads.Length()) {
    TimeStamp presented = aCompositionEndTime;
    for (const CompositionPayload& payload : aPayloads) {
      if (profiler_thread_is_being_profiled_for_markers()) {
        MOZ_RELEASE_ASSERT(payload.mType <= kHighestCompositionPayloadType);
        nsAutoCString name(
            kCompositionPayloadTypeNames[uint8_t(payload.mType)]);
        name.AppendLiteral(" Payload Presented");
        // This doesn't really need to be a text marker. Once we have a version
        // of profiler_add_marker that accepts both a start time and an end
        // time, we could use that here.
        nsPrintfCString text(
            "Latency: %dms",
            int32_t((presented - payload.mTimeStamp).ToMilliseconds()));
        PROFILER_MARKER_TEXT(
            name, GRAPHICS,
            MarkerTiming::Interval(payload.mTimeStamp, presented), text);
      }

      if (payload.mType == CompositionPayloadType::eKeyPress) {
        Telemetry::AccumulateTimeDelta(
            mozilla::Telemetry::KEYPRESS_PRESENT_LATENCY, payload.mTimeStamp,
            presented);
      } else if (payload.mType == CompositionPayloadType::eAPZScroll) {
        Telemetry::AccumulateTimeDelta(
            mozilla::Telemetry::SCROLL_PRESENT_LATENCY, payload.mTimeStamp,
            presented);
      } else if (payload.mType ==
                 CompositionPayloadType::eMouseUpFollowedByClick) {
        Telemetry::AccumulateTimeDelta(
            mozilla::Telemetry::MOUSEUP_FOLLOWED_BY_CLICK_PRESENT_LATENCY,
            payload.mTimeStamp, presented);
      }
    }
  }
}

}  // namespace layers
}  // namespace mozilla
