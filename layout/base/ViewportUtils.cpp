/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Units.h"
#include "mozilla/PresShell.h"
#include "mozilla/ViewportFrame.h"
#include "mozilla/ViewportUtils.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsQueryFrame.h"
#include "nsStyleStruct.h"

namespace mozilla {

using layers::APZCCallbackHelper;
using layers::InputAPZContext;
using layers::ScrollableLayerGuid;

template <typename Units>
gfx::Matrix4x4Typed<Units, Units> ViewportUtils::GetVisualToLayoutTransform(
    ScrollableLayerGuid::ViewID aScrollId) {
  static_assert(
      std::is_same_v<Units, CSSPixel> ||
          std::is_same_v<Units, LayoutDevicePixel>,
      "GetCallbackTransform() may only be used with CSS or LayoutDevice units");

  if (aScrollId == ScrollableLayerGuid::NULL_SCROLL_ID) {
    return {};
  }
  nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aScrollId);
  if (!content || !content->GetPrimaryFrame()) {
    return {};
  }

  // First, scale inversely by the root content document's pres shell
  // resolution to cancel the scale-to-resolution transform that the
  // compositor adds to the layer with the pres shell resolution. The points
  // sent to Gecko by APZ don't have this transform unapplied (unlike other
  // compositor-side transforms) because Gecko needs it applied when hit
  // testing against content that's conceptually outside the resolution,
  // such as scrollbars.
  float resolution = 1.0f;
  if (PresShell* presShell =
          APZCCallbackHelper::GetRootContentDocumentPresShellForContent(
              content)) {
    resolution = presShell->GetResolution();
  }

  // Now apply the callback-transform. This is only approximately correct,
  // see the comment on GetCumulativeApzCallbackTransform for details.
  gfx::PointTyped<Units> transform;
  CSSPoint transformCSS = nsLayoutUtils::GetCumulativeApzCallbackTransform(
      content->GetPrimaryFrame());
  if constexpr (std::is_same_v<Units, CSSPixel>) {
    transform = transformCSS;
  } else {  // Units == LayoutDevicePixel
    transform = transformCSS *
                content->GetPrimaryFrame()->PresContext()->CSSToDevPixelScale();
  }

  return gfx::Matrix4x4Typed<Units, Units>::Scaling(1 / resolution,
                                                    1 / resolution, 1)
      .PostTranslate(transform.x, transform.y, 0);
}

CSSToCSSMatrix4x4 GetVisualToLayoutTransform(PresShell* aContext) {
  ScrollableLayerGuid::ViewID targetScrollId =
      InputAPZContext::GetTargetLayerGuid().mScrollId;
  if (targetScrollId == ScrollableLayerGuid::NULL_SCROLL_ID) {
    if (nsIFrame* rootScrollFrame = aContext->GetRootScrollFrame()) {
      targetScrollId =
          nsLayoutUtils::FindOrCreateIDFor(rootScrollFrame->GetContent());
    }
  }
  return ViewportUtils::GetVisualToLayoutTransform(targetScrollId);
}

nsPoint ViewportUtils::VisualToLayout(const nsPoint& aPt, PresShell* aContext) {
  auto visualToLayout = mozilla::GetVisualToLayoutTransform(aContext);
  CSSPoint cssPt = CSSPoint::FromAppUnits(aPt);
  cssPt = visualToLayout.TransformPoint(cssPt);
  return CSSPoint::ToAppUnits(cssPt);
}

nsRect ViewportUtils::VisualToLayout(const nsRect& aRect, PresShell* aContext) {
  auto visualToLayout = mozilla::GetVisualToLayoutTransform(aContext);
  CSSRect cssRect = CSSRect::FromAppUnits(aRect);
  cssRect = visualToLayout.TransformBounds(cssRect);
  nsRect result = CSSRect::ToAppUnits(cssRect);

  // In hit testing codepaths, the input rect often has dimensions of one app
  // units. If we are zoomed in enough, the rounded size of the output rect
  // can be zero app units, which will fail to Intersect() with anything, and
  // therefore cause hit testing to fail. To avoid this, we expand the output
  // rect to one app units.
  if (!aRect.IsEmpty() && result.IsEmpty()) {
    result.width = 1;
    result.height = 1;
  }

  return result;
}

nsPoint ViewportUtils::LayoutToVisual(const nsPoint& aPt, PresShell* aContext) {
  auto visualToLayout = mozilla::GetVisualToLayoutTransform(aContext);
  CSSPoint cssPt = CSSPoint::FromAppUnits(aPt);
  auto transformed = visualToLayout.Inverse().TransformPoint(cssPt);
  return CSSPoint::ToAppUnits(transformed);
}

LayoutDevicePoint ViewportUtils::DocumentRelativeLayoutToVisual(
    const LayoutDevicePoint& aPoint, PresShell* aShell) {
  ScrollableLayerGuid::ViewID targetScrollId =
      nsLayoutUtils::ScrollIdForRootScrollFrame(aShell->GetPresContext());
  auto visualToLayout =
      ViewportUtils::GetVisualToLayoutTransform<LayoutDevicePixel>(
          targetScrollId);
  return visualToLayout.Inverse().TransformPoint(aPoint);
}

LayoutDeviceRect ViewportUtils::DocumentRelativeLayoutToVisual(
    const LayoutDeviceRect& aRect, PresShell* aShell) {
  ScrollableLayerGuid::ViewID targetScrollId =
      nsLayoutUtils::ScrollIdForRootScrollFrame(aShell->GetPresContext());
  auto visualToLayout =
      ViewportUtils::GetVisualToLayoutTransform<LayoutDevicePixel>(
          targetScrollId);
  return visualToLayout.Inverse().TransformBounds(aRect);
}

LayoutDeviceRect ViewportUtils::DocumentRelativeLayoutToVisual(
    const LayoutDeviceIntRect& aRect, PresShell* aShell) {
  return DocumentRelativeLayoutToVisual(IntRectToRect(aRect), aShell);
}

CSSRect ViewportUtils::DocumentRelativeLayoutToVisual(const CSSRect& aRect,
                                                      PresShell* aShell) {
  ScrollableLayerGuid::ViewID targetScrollId =
      nsLayoutUtils::ScrollIdForRootScrollFrame(aShell->GetPresContext());
  auto visualToLayout =
      ViewportUtils::GetVisualToLayoutTransform(targetScrollId);
  return visualToLayout.Inverse().TransformBounds(aRect);
}

template <class SourceUnits, class DestUnits>
gfx::PointTyped<DestUnits> TransformPointOrRect(
    const gfx::Matrix4x4Typed<SourceUnits, DestUnits>& aMatrix,
    const gfx::PointTyped<SourceUnits>& aPoint) {
  return aMatrix.TransformPoint(aPoint);
}

template <class SourceUnits, class DestUnits>
gfx::RectTyped<DestUnits> TransformPointOrRect(
    const gfx::Matrix4x4Typed<SourceUnits, DestUnits>& aMatrix,
    const gfx::RectTyped<SourceUnits>& aRect) {
  return aMatrix.TransformBounds(aRect);
}

template <class LDPointOrRect>
LDPointOrRect ConvertToScreenRelativeVisual(const LDPointOrRect& aInput,
                                            nsPresContext* aCtx) {
  MOZ_ASSERT(aCtx);

  LDPointOrRect layoutToVisual(aInput);
  nsIFrame* prevRootFrame = nullptr;
  nsPresContext* prevCtx = nullptr;

  // Walk up to the rootmost prescontext, transforming as we go.
  for (nsPresContext* ctx = aCtx; ctx; ctx = ctx->GetParentPresContext()) {
    PresShell* shell = ctx->PresShell();
    nsIFrame* rootFrame = shell->GetRootFrame();
    if (prevRootFrame) {
      // Convert layoutToVisual from being relative to `prevRootFrame`
      // to being relative to `rootFrame` (layout space).
      nscoord apd = prevCtx->AppUnitsPerDevPixel();
      nsPoint offset = prevRootFrame->GetOffsetToCrossDoc(rootFrame, apd);
      layoutToVisual += LayoutDevicePoint::FromAppUnits(offset, apd);
    }
    if (shell->GetResolution() != 1.0) {
      // Found the APZ zoom root, so do the layout -> visual conversion.
      layoutToVisual =
          ViewportUtils::DocumentRelativeLayoutToVisual(layoutToVisual, shell);
    }

    prevRootFrame = rootFrame;
    prevCtx = ctx;
  }

  // If we're in a nested content process, the above traversal will not have
  // encountered the APZ zoom root. The translation part of the layout-to-visual
  // transform will be included in |rootScreenRect.TopLeft()|, added below
  // (that ultimately comes from nsIWidget::WidgetToScreenOffset(), which for an
  // OOP iframe's widget includes this translation), but the scale part needs to
  // be computed and added separately.
  Scale2D enclosingResolution =
      ViewportUtils::TryInferEnclosingResolution(prevCtx->GetPresShell());
  if (enclosingResolution != Scale2D{1.0f, 1.0f}) {
    layoutToVisual = TransformPointOrRect(
        LayoutDeviceToLayoutDeviceMatrix4x4::Scaling(
            enclosingResolution.xScale, enclosingResolution.yScale, 1.0f),
        layoutToVisual);
  }

  // Then we do the conversion from the rootmost presContext's root frame (in
  // visual space) to screen space.
  LayoutDeviceIntRect rootScreenRect =
      LayoutDeviceIntRect::FromAppUnitsToNearest(
          prevRootFrame->GetScreenRectInAppUnits(),
          prevCtx->AppUnitsPerDevPixel());

  return layoutToVisual + rootScreenRect.TopLeft();
}

LayoutDevicePoint ViewportUtils::ToScreenRelativeVisual(
    const LayoutDevicePoint& aPt, nsPresContext* aCtx) {
  return ConvertToScreenRelativeVisual(aPt, aCtx);
}

LayoutDeviceRect ViewportUtils::ToScreenRelativeVisual(
    const LayoutDeviceRect& aRect, nsPresContext* aCtx) {
  return ConvertToScreenRelativeVisual(aRect, aCtx);
}

// Definitions of the two explicit instantiations forward declared in the header
// file. This causes code for these instantiations to be emitted into the object
// file for ViewportUtils.cpp.
template CSSToCSSMatrix4x4 ViewportUtils::GetVisualToLayoutTransform<CSSPixel>(
    ScrollableLayerGuid::ViewID);
template LayoutDeviceToLayoutDeviceMatrix4x4
    ViewportUtils::GetVisualToLayoutTransform<LayoutDevicePixel>(
        ScrollableLayerGuid::ViewID);

const nsIFrame* ViewportUtils::IsZoomedContentRoot(const nsIFrame* aFrame) {
  if (!aFrame) {
    return nullptr;
  }
  if (aFrame->Type() == LayoutFrameType::Canvas ||
      aFrame->Type() == LayoutFrameType::PageSequence) {
    nsIScrollableFrame* sf = do_QueryFrame(aFrame->GetParent());
    if (sf && sf->IsRootScrollFrameOfDocument() &&
        aFrame->PresContext()->IsRootContentDocumentCrossProcess()) {
      return aFrame->GetParent();
    }
  } else if (aFrame->StyleDisplay()->mPosition ==
             StylePositionProperty::Fixed) {
    if (ViewportFrame* viewportFrame = do_QueryFrame(aFrame->GetParent())) {
      if (viewportFrame->PresContext()->IsRootContentDocumentCrossProcess()) {
        return viewportFrame->PresShell()->GetRootScrollFrame();
      }
    }
  }
  return nullptr;
}

Scale2D ViewportUtils::TryInferEnclosingResolution(PresShell* aShell) {
  MOZ_ASSERT(aShell && aShell->GetPresContext());
  MOZ_ASSERT(!aShell->GetPresContext()->GetParentPresContext(),
             "TryInferEnclosingResolution can only be called for a root pres "
             "shell within a process");
  if (dom::BrowserChild* bc = dom::BrowserChild::GetFrom(aShell)) {
    if (!bc->IsTopLevel()) {
      // The enclosing resolution is not directly available in the BrowserChild.
      // The closest thing available is GetChildToParentConversionMatrix(),
      // which also includes any enclosing CSS transforms.
      // The behaviour implemented here will not provide an accurate answer
      // in the presence of CSS transforms, but it tries to do something
      // reasonable:
      //  - If there are no enclosing CSS transforms, it will return the
      //    resolution.
      //  - If the enclosing transforms contain scales and translations only,
      //    it will return the resolution times the CSS transform scale
      //    (choosing the x-scale if they are different).
      //  - Otherwise, it will return the resolution times a scale component
      //    of the transform as returned by Matrix4x4Typed::Decompose().
      //  - If the enclosing transform is sufficiently complex that
      //    Decompose() returns false, give up and return 1.0.
      gfx::Point3DTyped<gfx::UnknownUnits> translation;
      gfx::Quaternion rotation;
      gfx::Point3DTyped<gfx::UnknownUnits> scale;
      if (bc->GetChildToParentConversionMatrix().Decompose(translation,
                                                           rotation, scale)) {
        return {scale.x, scale.y};
      }
    }
  }
  return {1.0f, 1.0f};
}

}  // namespace mozilla
