/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZUtils_h
#define mozilla_layers_APZUtils_h

// This file is for APZ-related utilities that are used by code in gfx/layers
// only. For APZ-related utilities used by the Rest of the World (widget/,
// layout/, dom/, IPDL protocols, etc.), use APZPublicUtils.h.
// Do not include this header from source files outside of gfx/layers.

#include <stdint.h>  // for uint32_t
#include <type_traits>
#include "gfxTypes.h"
#include "FrameMetrics.h"
#include "LayersTypes.h"
#include "UnitTransforms.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/DefineEnum.h"
#include "mozilla/EnumSet.h"
#include "mozilla/FloatingPoint.h"

namespace mozilla {

namespace layers {

enum CancelAnimationFlags : uint32_t {
  Default = 0x0,             /* Cancel all animations */
  ExcludeOverscroll = 0x1,   /* Don't clear overscroll */
  ScrollSnap = 0x2,          /* Snap to snap points */
  ExcludeWheel = 0x4,        /* Don't stop wheel smooth-scroll animations */
  TriggeredExternally = 0x8, /* Cancellation was not triggered by APZ in
                                response to an input event */
};

inline CancelAnimationFlags operator|(CancelAnimationFlags a,
                                      CancelAnimationFlags b) {
  return static_cast<CancelAnimationFlags>(static_cast<int>(a) |
                                           static_cast<int>(b));
}

// clang-format off
enum class ScrollSource {
  // Touch-screen.
  Touchscreen,

  // Touchpad with gesture support.
  Touchpad,

  // Mouse wheel.
  Wheel,

  // Keyboard
  Keyboard,
};
// clang-format on

inline bool ScrollSourceRespectsDisregardedDirections(ScrollSource aSource) {
  return aSource == ScrollSource::Wheel || aSource == ScrollSource::Touchpad;
}

inline bool ScrollSourceAllowsOverscroll(ScrollSource aSource) {
  return aSource == ScrollSource::Touchpad ||
         aSource == ScrollSource::Touchscreen;
}

// Epsilon to be used when comparing 'float' coordinate values
// with FuzzyEqualsAdditive. The rationale is that 'float' has 7 decimal
// digits of precision, and coordinate values should be no larger than in the
// ten thousands. Note also that the smallest legitimate difference in page
// coordinates is 1 app unit, which is 1/60 of a (CSS pixel), so this epsilon
// isn't too large.
const CSSCoord COORDINATE_EPSILON = 0.01f;

inline bool IsZero(const CSSPoint& aPoint) {
  return FuzzyEqualsAdditive(aPoint.x, CSSCoord(), COORDINATE_EPSILON) &&
         FuzzyEqualsAdditive(aPoint.y, CSSCoord(), COORDINATE_EPSILON);
}

// Represents async transforms consisting of a scale and a translation.
struct AsyncTransform {
  explicit AsyncTransform(
      LayerToParentLayerScale aScale = LayerToParentLayerScale(),
      ParentLayerPoint aTranslation = ParentLayerPoint())
      : mScale(aScale), mTranslation(aTranslation) {}

  operator AsyncTransformComponentMatrix() const {
    return AsyncTransformComponentMatrix::Scaling(mScale.scale, mScale.scale, 1)
        .PostTranslate(mTranslation.x, mTranslation.y, 0);
  }

  bool operator==(const AsyncTransform& rhs) const {
    return mTranslation == rhs.mTranslation && mScale == rhs.mScale;
  }

  bool operator!=(const AsyncTransform& rhs) const { return !(*this == rhs); }

  LayerToParentLayerScale mScale;
  ParentLayerPoint mTranslation;
};

// Deem an AsyncTransformComponentMatrix (obtained by multiplying together
// one or more AsyncTransformComponentMatrix objects) as constituting a
// complete async transform.
inline AsyncTransformMatrix CompleteAsyncTransform(
    const AsyncTransformComponentMatrix& aMatrix) {
  return ViewAs<AsyncTransformMatrix>(
      aMatrix, PixelCastJustification::MultipleAsyncTransforms);
}

struct TargetConfirmationFlags final {
  explicit TargetConfirmationFlags(bool aTargetConfirmed)
      : mTargetConfirmed(aTargetConfirmed),
        mRequiresTargetConfirmation(false),
        mHitScrollbar(false),
        mHitScrollThumb(false),
        mDispatchToContent(false) {}

  explicit TargetConfirmationFlags(
      const gfx::CompositorHitTestInfo& aHitTestInfo)
      : mTargetConfirmed(
            (aHitTestInfo != gfx::CompositorHitTestInvisibleToHit) &&
            (aHitTestInfo & gfx::CompositorHitTestDispatchToContent).isEmpty()),
        mRequiresTargetConfirmation(aHitTestInfo.contains(
            gfx::CompositorHitTestFlags::eRequiresTargetConfirmation)),
        mHitScrollbar(
            aHitTestInfo.contains(gfx::CompositorHitTestFlags::eScrollbar)),
        mHitScrollThumb(aHitTestInfo.contains(
            gfx::CompositorHitTestFlags::eScrollbarThumb)),
        mDispatchToContent(
            !(aHitTestInfo & gfx::CompositorHitTestDispatchToContent)
                 .isEmpty()) {}

  bool mTargetConfirmed : 1;
  bool mRequiresTargetConfirmation : 1;
  bool mHitScrollbar : 1;
  bool mHitScrollThumb : 1;
  bool mDispatchToContent : 1;
};

enum class AsyncTransformComponent { eLayout, eVisual };

using AsyncTransformComponents = EnumSet<AsyncTransformComponent>;

constexpr AsyncTransformComponents LayoutAndVisual(
    AsyncTransformComponent::eLayout, AsyncTransformComponent::eVisual);

/**
 * Allows consumers of async transforms to specify for what purpose they are
 * using the async transform:
 *
 *   |eForEventHandling| is intended for event handling and other uses that
 *                       need the most up-to-date transform, reflecting all
 *                       events that have been processed so far, even if the
 *                       transform is not yet reflected visually.
 *   |eForCompositing| is intended for the transform that should be reflected
 *                     visually.
 *
 * For example, if an APZC has metrics with the mForceDisableApz flag set,
 * then the |eForCompositing| async transform will be empty, while the
 * |eForEventHandling| async transform will reflect processed input events
 * regardless of mForceDisableApz.
 */
enum class AsyncTransformConsumer {
  eForEventHandling,
  eForCompositing,
};

/**
 * Metrics that GeckoView wants to know at every composite.
 * These are the effective visual scroll offset and zoom level of
 * the root content APZC at composition time.
 */
struct GeckoViewMetrics {
  CSSPoint mVisualScrollOffset;
  CSSToParentLayerScale mZoom;
};

namespace apz {

/**
 * Is aAngle within the given threshold of the horizontal axis?
 * @param aAngle an angle in radians in the range [0, pi]
 * @param aThreshold an angle in radians in the range [0, pi/2]
 */
bool IsCloseToHorizontal(float aAngle, float aThreshold);

// As above, but for the vertical axis.
bool IsCloseToVertical(float aAngle, float aThreshold);

// Returns true if a sticky layer with async translation |aTranslation| is
// stuck with a bottom margin. The inner/outer ranges are produced by the main
// thread at the last paint, and so |aTranslation| only needs to be the
// async translation from the last paint.
bool IsStuckAtBottom(gfxFloat aTranslation,
                     const LayerRectAbsolute& aInnerRange,
                     const LayerRectAbsolute& aOuterRange);

// Returns true if a sticky layer with async translation |aTranslation| is
// stuck with a top margin.
bool IsStuckAtTop(gfxFloat aTranslation, const LayerRectAbsolute& aInnerRange,
                  const LayerRectAbsolute& aOuterRange);

/**
 * Compute the translation that should be applied to a layer that's fixed
 * at |eFixedSides|, to respect the fixed layer margins |aFixedMargins|.
 */
ScreenPoint ComputeFixedMarginsOffset(
    const ScreenMargin& aCompositorFixedLayerMargins, SideBits aFixedSides,
    const ScreenMargin& aGeckoFixedLayerMargins);

/**
 * Takes the visible rect from the compositor metrics, adds a pref-based
 * margin around it, and checks to see if it is contained inside the painted
 * rect from the painted metrics. Returns true if it is contained, or false
 * if not. Returning false means that a (relatively) small amount of async
 * scrolling/zooming can result in the visible area going outside the painted
 * area and resulting in visual checkerboarding.
 * Note that this may return false positives for cases where the scrollframe
 * in question is nested inside other scrollframes, as the composition bounds
 * used to determine the visible rect may in fact be clipped by enclosing
 * scrollframes, but that is not accounted for in this function.
 */
bool AboutToCheckerboard(const FrameMetrics& aPaintedMetrics,
                         const FrameMetrics& aCompositorMetrics);

/**
 * Returns SideBits where the given |aOverscrollAmount| overscrolls.
 */
SideBits GetOverscrollSideBits(const ParentLayerPoint& aOverscrollAmount);

}  // namespace apz

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZUtils_h
