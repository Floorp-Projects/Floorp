/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ScrollThumbUtils_h
#define mozilla_layers_ScrollThumbUtils_h

#include "LayersTypes.h"
#include "Units.h"

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

struct FrameMetrics;
struct ScrollbarData;

namespace apz {
/**
 * Compute the updated shadow transform for a scroll thumb layer that
 * reflects async scrolling of the associated scroll frame.
 *
 * @param aCurrentTransform The current shadow transform on the scroll thumb
 *    layer, as returned by Layer::GetLocalTransform() or similar.
 * @param aScrollableContentTransform The current content transform on the
 *    scrollable content, as returned by Layer::GetTransform().
 * @param aApzc The APZC that scrolls the scroll frame.
 * @param aMetrics The metrics associated with the scroll frame, reflecting
 *    the last paint of the associated content. Note: this metrics should
 *    NOT reflect async scrolling, i.e. they should be the layer tree's
 *    copy of the metrics, or APZC's last-content-paint metrics.
 * @param aScrollbarData The scrollbar data for the the scroll thumb layer.
 * @param aScrollbarIsDescendant True iff. the scroll thumb layer is a
 *    descendant of the layer bearing the scroll frame's metrics.
 * @param aOutClipTransform If not null, and |aScrollbarIsDescendant| is true,
 *    this will be populated with a transform that should be applied to the
 *    clip rects of all layers between the scroll thumb layer and the ancestor
 *    layer for the scrollable content.
 * @return The new shadow transform for the scroll thumb layer, including
 *    any pre- or post-scales.
 */
LayerToParentLayerMatrix4x4 ComputeTransformForScrollThumb(
    const LayerToParentLayerMatrix4x4& aCurrentTransform,
    const gfx::Matrix4x4& aScrollableContentTransform,
    AsyncPanZoomController* aApzc, const FrameMetrics& aMetrics,
    const ScrollbarData& aScrollbarData, bool aScrollbarIsDescendant,
    AsyncTransformComponentMatrix* aOutClipTransform);

}  // namespace apz
}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_ScrollThumbUtils_h
