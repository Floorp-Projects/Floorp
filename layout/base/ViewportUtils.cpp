/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PresShell.h"
#include "mozilla/ViewportUtils.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "nsIContent.h"
#include "nsLayoutUtils.h"

namespace mozilla {

using layers::APZCCallbackHelper;
using layers::InputAPZContext;
using layers::ScrollableLayerGuid;

CSSToCSSMatrix4x4 ViewportUtils::GetVisualToLayoutTransform(
    ScrollableLayerGuid::ViewID aScrollId) {
  if (aScrollId == ScrollableLayerGuid::NULL_SCROLL_ID) {
    return {};
  }
  nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aScrollId);
  if (!content) {
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
  CSSPoint transform = nsLayoutUtils::GetCumulativeApzCallbackTransform(
      content->GetPrimaryFrame());

  return mozilla::CSSToCSSMatrix4x4::Scaling(1 / resolution, 1 / resolution, 1)
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

}  // namespace mozilla