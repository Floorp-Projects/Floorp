/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PresShell.h"
#include "mozilla/ViewportUtils.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "nsIContent.h"
#include "nsLayoutUtils.h"

namespace mozilla {

using layers::APZCCallbackHelper;
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

}  // namespace mozilla