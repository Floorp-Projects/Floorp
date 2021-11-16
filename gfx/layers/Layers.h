/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERS_H
#define GFX_LAYERS_H

#include <cstdint>        // for uint32_t, uint64_t, int32_t, uint8_t
#include <cstring>        // for memcpy, size_t
#include <iosfwd>         // for stringstream
#include <new>            // for operator new
#include <unordered_set>  // for unordered_set
#include <utility>        // for forward, move
#include "FrameMetrics.h"  // for ScrollMetadata, FrameMetrics::ViewID, FrameMetrics
#include "Units.h"  // for LayerIntRegion, ParentLayerIntRect, LayerIntSize, Lay...
#include "gfxPoint.h"           // for gfxPoint
#include "gfxRect.h"            // for gfxRect
#include "mozilla/Maybe.h"      // for Maybe, Nothing (ptr only)
#include "mozilla/Poison.h"     // for CorruptionCanary
#include "mozilla/RefPtr.h"     // for RefPtr, operator!=
#include "mozilla/TimeStamp.h"  // for TimeStamp
#include "mozilla/UniquePtr.h"  // for UniquePtr, MakeUnique
#include "mozilla/gfx/BasePoint.h"  // for BasePoint<>::(anonymous union)::(anonymous), BasePoin...
#include "mozilla/gfx/BaseSize.h"  // for BaseSize
#include "mozilla/gfx/Matrix.h"    // for Matrix4x4, Matrix, Matrix4x4Typed
#include "mozilla/gfx/Point.h"     // for Point, PointTyped
#include "mozilla/gfx/Polygon.h"   // for Polygon
#include "mozilla/gfx/Rect.h"      // for IntRectTyped, IntRect
#include "mozilla/gfx/Types.h"  // for CompositionOp, DeviceColor, SamplingFilter, SideBits
#include "mozilla/gfx/UserData.h"  // for UserData, UserDataKey (ptr only)
#include "mozilla/layers/AnimationInfo.h"  // for AnimationInfo
#include "mozilla/layers/LayerAttributes.h"  // for SimpleLayerAttributes, ScrollbarData (ptr only)
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid, ScrollableLayerGuid::ViewID
#include "mozilla/layers/BSPTree.h"
#include "nsISupports.h"    // for NS_INLINE_DECL_REFCOUNTING
#include "nsPoint.h"        // for nsIntPoint
#include "nsRect.h"         // for nsIntRect
#include "nsRegion.h"       // for nsIntRegion
#include "nsStringFlags.h"  // for operator&
#include "nsStringFwd.h"    // for nsCString, nsACString
#include "nsTArray.h"  // for nsTArray, nsTArray_Impl, nsTArray_Impl<>::elem_type

// XXX These includes could be avoided by moving function implementations to the
// cpp file
#include "gfx2DGlue.h"           // for ThebesPoint
#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT, MOZ_A...
#include "mozilla/DebugOnly.h"  // for DebugOnly
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG_IF_SHADOWABLE, LayersId, EventRegionsO...
#include "nsDebug.h"  // for NS_ASSERTION, NS_WARNING

namespace mozilla {

namespace gfx {
class DataSourceSurface;
class DrawTarget;
class Path;
}  // namespace gfx

namespace layers {

class Animation;
class AsyncPanZoomController;
class CompositorAnimations;
class SpecificLayerAttributes;
class Compositor;
class TransformData;
struct PropertyAnimationGroup;

class Layer {
 public:
  enum {
    /**
     * If this is set, the caller is promising that by the end of this
     * transaction the entire visible region (as specified by
     * SetVisibleRegion) will be filled with opaque content.
     */
    CONTENT_OPAQUE = 0x01,
    /**
     * If this is set, the caller is notifying that the contents of this layer
     * require per-component alpha for optimal fidelity. However, there is no
     * guarantee that component alpha will be supported for this layer at
     * paint time.
     * This should never be set at the same time as CONTENT_OPAQUE.
     */
    CONTENT_COMPONENT_ALPHA = 0x02,

    /**
     * If this is set then this layer is part of a preserve-3d group, and should
     * be sorted with sibling layers that are also part of the same group.
     */
    CONTENT_EXTEND_3D_CONTEXT = 0x08,

    /**
     * Disable subpixel AA for this layer. This is used if the display isn't
     * suited for subpixel AA like hidpi or rotated content.
     */
    CONTENT_DISABLE_SUBPIXEL_AA = 0x20,

    /**
     * If this is set then the layer contains content that may look
     * objectionable if not handled as an active layer (such as text with an
     * animated transform). This is for internal layout/FrameLayerBuilder usage
     * only until flattening code is obsoleted. See bug 633097
     */
    CONTENT_DISABLE_FLATTENING = 0x40,

    /**
     * This layer is hidden if the backface of the layer is visible
     * to user.
     */
    CONTENT_BACKFACE_HIDDEN = 0x80,

    /**
     * This layer should be snapped to the pixel grid.
     */
    CONTENT_SNAP_TO_GRID = 0x100
  };
};

void SetAntialiasingFlags(Layer* aLayer, gfx::DrawTarget* aTarget);

#ifdef MOZ_DUMP_PAINTING
void WriteSnapshotToDumpFile(Layer* aLayer, gfx::DataSourceSurface* aSurf);
void WriteSnapshotToDumpFile(Compositor* aCompositor, gfx::DrawTarget* aTarget);
#endif

// A utility function used by different LayerManager implementations.
gfx::IntRect ToOutsideIntRect(const gfxRect& aRect);

void RecordCompositionPayloadsPresented(
    const TimeStamp& aCompositionEndTime,
    const nsTArray<CompositionPayload>& aPayloads);

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_LAYERS_H */
