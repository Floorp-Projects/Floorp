/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileViewportManager.h"

#include "mozilla/PresShell.h"
#include "mozilla/ToString.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsViewManager.h"
#include "nsViewportInfo.h"
#include "UnitTransforms.h"

static mozilla::LazyLogModule sApzMvmLog("apz.mobileviewport");
#define MVM_LOG(...) MOZ_LOG(sApzMvmLog, LogLevel::Debug, (__VA_ARGS__))

NS_IMPL_ISUPPORTS(MobileViewportManager, nsIDOMEventListener, nsIObserver)

#define DOM_META_ADDED u"DOMMetaAdded"_ns
#define DOM_META_CHANGED u"DOMMetaChanged"_ns
#define FULLSCREEN_CHANGED u"fullscreenchange"_ns
#define LOAD u"load"_ns
#define BEFORE_FIRST_PAINT "before-first-paint"_ns

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

MobileViewportManager::MobileViewportManager(MVMContext* aContext,
                                             ManagerType aType)
    : mContext(aContext),
      mManagerType(aType),
      mIsFirstPaint(false),
      mPainted(false) {
  MOZ_ASSERT(mContext);

  MVM_LOG("%p: creating with context %p\n", this, mContext.get());

  mContext->AddEventListener(DOM_META_ADDED, this, false);
  mContext->AddEventListener(DOM_META_CHANGED, this, false);
  mContext->AddEventListener(FULLSCREEN_CHANGED, this, false);
  mContext->AddEventListener(LOAD, this, true);

  mContext->AddObserver(this, BEFORE_FIRST_PAINT.Data(), false);
}

MobileViewportManager::~MobileViewportManager() = default;

void MobileViewportManager::Destroy() {
  MVM_LOG("%p: destroying\n", this);

  mContext->RemoveEventListener(DOM_META_ADDED, this, false);
  mContext->RemoveEventListener(DOM_META_CHANGED, this, false);
  mContext->RemoveEventListener(FULLSCREEN_CHANGED, this, false);
  mContext->RemoveEventListener(LOAD, this, true);

  mContext->RemoveObserver(this, BEFORE_FIRST_PAINT.Data());

  mContext->Destroy();
  mContext = nullptr;
}

void MobileViewportManager::SetRestoreResolution(
    float aResolution, LayoutDeviceIntSize aDisplaySize) {
  SetRestoreResolution(aResolution);
  ScreenIntSize restoreDisplaySize = ViewAs<ScreenPixel>(
      aDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
  mRestoreDisplaySize = Some(restoreDisplaySize);
}

void MobileViewportManager::SetRestoreResolution(float aResolution) {
  mRestoreResolution = Some(aResolution);
}

float MobileViewportManager::ComputeIntrinsicResolution() const {
  if (!mContext) {
    return 1.f;
  }

  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
  CSSToScreenScale intrinsicScale = ComputeIntrinsicScale(
      mContext->GetViewportInfo(displaySize), displaySize, mMobileViewportSize);
  CSSToLayoutDeviceScale cssToDev = mContext->CSSToDevPixelScale();
  return (intrinsicScale / cssToDev).scale;
}

mozilla::CSSToScreenScale MobileViewportManager::ComputeIntrinsicScale(
    const nsViewportInfo& aViewportInfo,
    const mozilla::ScreenIntSize& aDisplaySize,
    const mozilla::CSSSize& aViewportOrContentSize) const {
  CSSToScreenScale intrinsicScale =
      aViewportOrContentSize.IsEmpty()
          ? CSSToScreenScale(1.0)
          : MaxScaleRatio(ScreenSize(aDisplaySize), aViewportOrContentSize);
  MVM_LOG("%p: Intrinsic computed zoom is %f\n", this, intrinsicScale.scale);
  return ClampZoom(intrinsicScale, aViewportInfo);
}

void MobileViewportManager::RequestReflow(bool aForceAdjustResolution) {
  MVM_LOG("%p: got a reflow request with force resolution: %d\n", this,
          aForceAdjustResolution);
  RefreshViewportSize(aForceAdjustResolution);
}

void MobileViewportManager::ResolutionUpdated(
    mozilla::ResolutionChangeOrigin aOrigin) {
  MVM_LOG("%p: resolution updated\n", this);

  if (!mContext) {
    return;
  }

  if ((!mPainted &&
       aOrigin == mozilla::ResolutionChangeOrigin::MainThreadRestore) ||
      aOrigin == mozilla::ResolutionChangeOrigin::Test) {
    // Save the value, so our default zoom calculation
    // can take it into account later on.
    SetRestoreResolution(mContext->GetResolution());
  }
  RefreshVisualViewportSize();
}

NS_IMETHODIMP
MobileViewportManager::HandleEvent(dom::Event* event) {
  nsAutoString type;
  event->GetType(type);

  if (type.Equals(DOM_META_ADDED)) {
    HandleDOMMetaAdded();
  } else if (type.Equals(DOM_META_CHANGED)) {
    MVM_LOG("%p: got a dom-meta-changed event\n", this);
    RefreshViewportSize(mPainted);
  } else if (type.Equals(FULLSCREEN_CHANGED)) {
    MVM_LOG("%p: got a fullscreenchange event\n", this);
    RefreshViewportSize(mPainted);
  } else if (type.Equals(LOAD)) {
    MVM_LOG("%p: got a load event\n", this);
    if (!mPainted) {
      // Load event got fired before the before-first-paint message
      SetInitialViewport();
    }
  }
  return NS_OK;
}

void MobileViewportManager::HandleDOMMetaAdded() {
  MVM_LOG("%p: got a dom-meta-added event\n", this);
  if (mPainted && mContext->IsDocumentLoading()) {
    // It's possible that we get a DOMMetaAdded event after the page
    // has already been painted, but before the document finishes loading.
    // In such a case, we've already run SetInitialViewport() on
    // "before-first-paint", and won't run it again on "load" (because
    // mPainted=true). But that SetInitialViewport() call didn't know the
    // "initial-scale" from this meta viewport tag. To ensure we respect
    // the "initial-scale", call SetInitialViewport() again.
    // Note: It's important that we only do this if mPainted=true. In the
    // usual case, we get the DOMMetaAdded before the first paint, sometimes
    // even before we have a frame tree, and calling SetInitialViewport()
    // before we have a frame tree will skip some important steps (e.g.
    // updating display port margins).
    SetInitialViewport();
  } else {
    RefreshViewportSize(mPainted);
  }
}

NS_IMETHODIMP
MobileViewportManager::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  if (!mContext) {
    return NS_OK;
  }

  if (mContext->SubjectMatchesDocument(aSubject) &&
      BEFORE_FIRST_PAINT.EqualsASCII(aTopic)) {
    MVM_LOG("%p: got a before-first-paint event\n", this);
    if (!mPainted) {
      // before-first-paint message arrived before load event
      SetInitialViewport();
    }
  }
  return NS_OK;
}

void MobileViewportManager::SetInitialViewport() {
  MVM_LOG("%p: setting initial viewport\n", this);
  mIsFirstPaint = true;
  mPainted = true;
  RefreshViewportSize(false);
}

CSSToScreenScale MobileViewportManager::ClampZoom(
    const CSSToScreenScale& aZoom, const nsViewportInfo& aViewportInfo) const {
  CSSToScreenScale zoom = aZoom;
  if (IsNaN(zoom.scale)) {
    NS_ERROR("Don't pass NaN to ClampZoom; check caller for 0/0 division");
    zoom = CSSToScreenScale(1.0);
  }

  if (zoom < aViewportInfo.GetMinZoom()) {
    zoom = aViewportInfo.GetMinZoom();
    MVM_LOG("%p: Clamped to %f\n", this, zoom.scale);
  }
  if (zoom > aViewportInfo.GetMaxZoom()) {
    zoom = aViewportInfo.GetMaxZoom();
    MVM_LOG("%p: Clamped to %f\n", this, zoom.scale);
  }

  // Non-positive zoom factors can produce NaN or negative viewport sizes,
  // so we better be sure we've got a positive zoom factor. Just for good
  // measure, we check our min/max as well as the final clamped value.
  MOZ_ASSERT(aViewportInfo.GetMinZoom() > CSSToScreenScale(0.0f),
             "zoom factor must be positive");
  MOZ_ASSERT(aViewportInfo.GetMaxZoom() > CSSToScreenScale(0.0f),
             "zoom factor must be positive");
  MOZ_ASSERT(zoom > CSSToScreenScale(0.0f), "zoom factor must be positive");
  return zoom;
}

CSSToScreenScale MobileViewportManager::ScaleZoomWithDisplayWidth(
    const CSSToScreenScale& aZoom, const float& aDisplayWidthChangeRatio,
    const CSSSize& aNewViewport, const CSSSize& aOldViewport) {
  float inverseCssWidthChangeRatio =
      (aNewViewport.width == 0) ? 1.0f
                                : aOldViewport.width / aNewViewport.width;
  CSSToScreenScale newZoom(aZoom.scale * aDisplayWidthChangeRatio *
                           inverseCssWidthChangeRatio);
  MVM_LOG("%p: Old zoom was %f, changed by %f * %f to %f\n", this, aZoom.scale,
          aDisplayWidthChangeRatio, inverseCssWidthChangeRatio, newZoom.scale);
  return newZoom;
}

CSSToScreenScale MobileViewportManager::ResolutionToZoom(
    const LayoutDeviceToLayerScale& aResolution) const {
  return ViewTargetAs<ScreenPixel>(
      mContext->CSSToDevPixelScale() * aResolution / ParentLayerToLayerScale(1),
      PixelCastJustification::ScreenIsParentLayerForRoot);
}

LayoutDeviceToLayerScale MobileViewportManager::ZoomToResolution(
    const CSSToScreenScale& aZoom) const {
  return ViewTargetAs<ParentLayerPixel>(
             aZoom, PixelCastJustification::ScreenIsParentLayerForRoot) /
         mContext->CSSToDevPixelScale() * ParentLayerToLayerScale(1);
}

void MobileViewportManager::UpdateResolutionForFirstPaint(
    const CSSSize& aViewportSize) {
  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
  nsViewportInfo viewportInfo = mContext->GetViewportInfo(displaySize);
  ScreenIntSize compositionSize = GetCompositionSize(displaySize);

  if (mRestoreResolution) {
    LayoutDeviceToLayerScale restoreResolution(*mRestoreResolution);
    CSSToScreenScale restoreZoom = ResolutionToZoom(restoreResolution);
    if (mRestoreDisplaySize) {
      CSSSize prevViewport =
          mContext->GetViewportInfo(*mRestoreDisplaySize).GetSize();
      float restoreDisplayWidthChangeRatio =
          (mRestoreDisplaySize->width > 0)
              ? (float)compositionSize.width / (float)mRestoreDisplaySize->width
              : 1.0f;

      restoreZoom =
          ScaleZoomWithDisplayWidth(restoreZoom, restoreDisplayWidthChangeRatio,
                                    aViewportSize, prevViewport);
    }
    MVM_LOG("%p: restored zoom is %f\n", this, restoreZoom.scale);
    restoreZoom = ClampZoom(restoreZoom, viewportInfo);

    ApplyNewZoom(displaySize, restoreZoom);
    return;
  }

  CSSToScreenScale defaultZoom = viewportInfo.GetDefaultZoom();
  MVM_LOG("%p: default zoom from viewport is %f\n", this, defaultZoom.scale);
  if (!viewportInfo.IsDefaultZoomValid()) {
    CSSSize contentSize = aViewportSize;
    if (Maybe<CSSRect> scrollableRect =
            mContext->CalculateScrollableRectForRSF()) {
      contentSize = scrollableRect->Size();
    }
    defaultZoom =
        ComputeIntrinsicScale(viewportInfo, compositionSize, contentSize);
  }
  MOZ_ASSERT(viewportInfo.GetMinZoom() <= defaultZoom &&
             defaultZoom <= viewportInfo.GetMaxZoom());

  ApplyNewZoom(displaySize, defaultZoom);
}

void MobileViewportManager::UpdateResolutionForViewportSizeChange(
    const CSSSize& aViewportSize,
    const Maybe<float>& aDisplayWidthChangeRatio) {
  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
  nsViewportInfo viewportInfo = mContext->GetViewportInfo(displaySize);

  CSSToScreenScale zoom = GetZoom();
  // Non-positive zoom factors can produce NaN or negative viewport sizes,
  // so we better be sure we've got a positive zoom factor.
  MOZ_ASSERT(zoom > CSSToScreenScale(0.0f), "zoom factor must be positive");

  MOZ_ASSERT(!mIsFirstPaint);

  // If this is not a first paint, then in some cases we want to update the
  // pre- existing resolution so as to maintain how much actual content is
  // visible within the display width. Note that "actual content" may be
  // different with respect to CSS pixels because of the CSS viewport size
  // changing.
  //
  // aDisplayWidthChangeRatio is non-empty if:
  // (a) The meta-viewport tag information changes, and so the CSS viewport
  //     might change as a result. If this happens after the content has
  //     been painted, we want to adjust the zoom to compensate. OR
  // (b) The display size changed from a nonzero value to another
  //     nonzero value. This covers the case where e.g. the device was
  //     rotated, and again we want to adjust the zoom to compensate.
  // Note in particular that aDisplayWidthChangeRatio will be None if all
  // that happened was a change in the full-zoom. In this case, we still
  // want to compute a new CSS and visual viewport, but we don't want to update
  // the resolution.
  //
  // Given the above, the algorithm below accounts for all types of changes
  // I can conceive of:
  // 1. screen size changes, CSS viewport does not (pages with no meta
  //    viewport or a fixed size viewport)
  // 2. screen size changes, CSS viewport also does (pages with a
  //    device-width viewport)
  // 3. screen size remains constant, but CSS viewport changes (meta
  //    viewport tag is added or removed)
  // 4. neither screen size nor CSS viewport changes

  if (!aDisplayWidthChangeRatio) {
    UpdateVisualViewportSize(displaySize, zoom);
    return;
  }

  // One more complication is that our current zoom level may be the
  // result of clamping to either the minimum or maximum zoom level
  // allowed by the viewport. If we naively scale the zoom level with
  // the change in the display width, we might be scaling one of these
  // previously clamped values. What we really want to do is to make
  // scaling of the zoom aware of these minimum and maximum clamping
  // points for the existing content size, so that we keep display
  // width changes completely reversible.

  // We don't consider here if we are scaling to a zoom value outside
  // of our viewport limits, because we'll clamp to the viewport limits
  // as a final step.

  // Because of the behavior of ShrinkToDisplaySizeIfNeeded, we are
  // choosing zoom clamping points based on the content size of the
  // scrollable rect, which might different from aViewportSize.
  CSSSize contentSize = aViewportSize;
  if (Maybe<CSSRect> scrollableRect =
          mContext->CalculateScrollableRectForRSF()) {
    contentSize = scrollableRect->Size();
  }

  // We scale the sizes, though we only care about the scaled widths.
  ScreenSize minZoomDisplaySize = contentSize * viewportInfo.GetMinZoom();
  ScreenSize maxZoomDisplaySize = contentSize * viewportInfo.GetMaxZoom();

  ScreenSize newDisplaySize(displaySize);
  ScreenSize oldDisplaySize = newDisplaySize / *aDisplayWidthChangeRatio;

  // To calculate an adjusted ratio, we use some combination of these
  // four values:
  float a(minZoomDisplaySize.width);
  float b(maxZoomDisplaySize.width);
  float c(oldDisplaySize.width);
  float d(newDisplaySize.width);

  // The oldDisplaySize value is in one of three "zones":
  // 1) Less than or equal to minZoomDisplaySize.
  // 2) Between minZoomDisplaySize and maxZoomDisplaySize.
  // 3) Greater than or equal to maxZoomDisplaySize.

  // Depending on which zone each are in, the adjusted ratio is shown in
  // the table below (using the a-b-c-d coding from above):

  // c   +---+
  //     | d |
  // 1   | a |
  //     +---+
  //     | d |
  // 2   | c |
  //     +---+
  //     | d |
  // 3   | b |
  //     +---+

  // Conveniently, the denominator is c clamped to a..b.
  float denominator = clamped(c, a, b);

  float adjustedRatio = d / denominator;
  CSSToScreenScale adjustedZoom = ScaleZoomWithDisplayWidth(
      zoom, adjustedRatio, aViewportSize, mMobileViewportSize);
  CSSToScreenScale newZoom = ClampZoom(adjustedZoom, viewportInfo);

  ApplyNewZoom(displaySize, newZoom);
}

void MobileViewportManager::UpdateResolutionForContentSizeChange(
    const CSSSize& aContentSize) {
  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
  nsViewportInfo viewportInfo = mContext->GetViewportInfo(displaySize);

  CSSToScreenScale zoom = GetZoom();
  // Non-positive zoom factors can produce NaN or negative viewport sizes,
  // so we better be sure we've got a positive zoom factor.
  MOZ_ASSERT(zoom > CSSToScreenScale(0.0f), "zoom factor must be positive");

  ScreenIntSize compositionSize = GetCompositionSize(displaySize);
  CSSToScreenScale intrinsicScale =
      ComputeIntrinsicScale(viewportInfo, compositionSize, aContentSize);

  // We try to scale down the contents only IF the document has no
  // initial-scale AND IF it's not restored documents AND IF the resolution
  // has never been changed by APZ.
  if (!mRestoreResolution && !mContext->IsResolutionUpdatedByApz() &&
      !viewportInfo.IsDefaultZoomValid()) {
    if (zoom != intrinsicScale) {
      ApplyNewZoom(displaySize, intrinsicScale);
    }
    return;
  }

  // Even in other scenarios, we want to ensure that zoom level is
  // not _smaller_ than the intrinsic scale, otherwise we might be
  // trying to show regions where there is no content to show.
  CSSToScreenScale clampedZoom = zoom;

  if (clampedZoom < intrinsicScale) {
    clampedZoom = intrinsicScale;
  }

  // Also clamp to the restrictions imposed by viewportInfo.
  clampedZoom = ClampZoom(clampedZoom, viewportInfo);

  if (clampedZoom != zoom) {
    ApplyNewZoom(displaySize, clampedZoom);
  }
}

void MobileViewportManager::ApplyNewZoom(const ScreenIntSize& aDisplaySize,
                                         const CSSToScreenScale& aNewZoom) {
  // If the zoom has changed, update the pres shell resolution accordingly.
  // We characterize this as MainThreadAdjustment, because we don't want our
  // change here to be remembered as a restore resolution.

  // Non-positive zoom factors can produce NaN or negative viewport sizes,
  // so we better be sure we've got a positive zoom factor.
  MOZ_ASSERT(aNewZoom > CSSToScreenScale(0.0f), "zoom factor must be positive");

  LayoutDeviceToLayerScale resolution = ZoomToResolution(aNewZoom);
  MVM_LOG("%p: setting resolution %f\n", this, resolution.scale);
  mContext->SetResolutionAndScaleTo(
      resolution.scale, ResolutionChangeOrigin::MainThreadAdjustment);

  MVM_LOG("%p: New zoom is %f\n", this, aNewZoom.scale);

  UpdateVisualViewportSize(aDisplaySize, aNewZoom);
}

ScreenIntSize MobileViewportManager::GetCompositionSize(
    const ScreenIntSize& aDisplaySize) const {
  if (!mContext) {
    return ScreenIntSize();
  }

  // FIXME: Bug 1586986 - To update VisualViewport in response to the dynamic
  // toolbar transition we probably need to include the dynamic toolbar
  // _current_ height.
  ScreenIntSize compositionSize(aDisplaySize);
  ScreenMargin scrollbars =
      mContext->ScrollbarAreaToExcludeFromCompositionBounds()
      // Scrollbars are not subject to resolution scaling, so LD pixels =
      // Screen pixels for them.
      * LayoutDeviceToScreenScale(1.0f);

  compositionSize.width =
      std::max(0.0f, compositionSize.width - scrollbars.LeftRight());
  compositionSize.height =
      std::max(0.0f, compositionSize.height - scrollbars.TopBottom());

  return compositionSize;
}

void MobileViewportManager::UpdateVisualViewportSize(
    const ScreenIntSize& aDisplaySize, const CSSToScreenScale& aZoom) {
  if (!mContext) {
    return;
  }

  ScreenSize compositionSize = ScreenSize(GetCompositionSize(aDisplaySize));

  CSSSize compSize = compositionSize / aZoom;
  MVM_LOG("%p: Setting VVPS %s\n", this, ToString(compSize).c_str());
  mContext->SetVisualViewportSize(compSize);
}

CSSToScreenScale MobileViewportManager::GetZoom() const {
  LayoutDeviceToLayerScale res(mContext->GetResolution());
  return ResolutionToZoom(res);
}

void MobileViewportManager::UpdateVisualViewportSizeByDynamicToolbar(
    ScreenIntCoord aToolbarHeight) {
  if (!mContext) {
    return;
  }

  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
  displaySize.height += aToolbarHeight;
  CSSSize compSize = ScreenSize(GetCompositionSize(displaySize)) / GetZoom();

  mVisualViewportSizeUpdatedByDynamicToolbar =
      nsSize(nsPresContext::CSSPixelsToAppUnits(compSize.width),
             nsPresContext::CSSPixelsToAppUnits(compSize.height));

  mContext->PostVisualViewportResizeEventByDynamicToolbar();
}

void MobileViewportManager::
    UpdateVisualViewportSizeForPotentialScrollbarChange() {
  RefreshVisualViewportSize();
}

void MobileViewportManager::UpdateDisplayPortMargins() {
  if (!mContext) {
    return;
  }
  mContext->UpdateDisplayPortMargins();
}

void MobileViewportManager::RefreshVisualViewportSize() {
  // This function is a subset of RefreshViewportSize, and only updates the
  // visual viewport size.

  if (!mContext) {
    return;
  }

  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);

  if (displaySize.width == 0 || displaySize.height == 0) {
    return;
  }

  UpdateVisualViewportSize(displaySize, GetZoom());
}

void MobileViewportManager::UpdateSizesBeforeReflow() {
  if (Maybe<LayoutDeviceIntSize> newDisplaySize =
          mContext->GetContentViewerSize()) {
    if (mDisplaySize == *newDisplaySize) {
      return;
    }

    mDisplaySize = *newDisplaySize;
    MVM_LOG("%p: Reflow starting, display size updated to %s\n", this,
            ToString(mDisplaySize).c_str());

    if (mDisplaySize.width == 0 || mDisplaySize.height == 0) {
      return;
    }

    ScreenIntSize displaySize = ViewAs<ScreenPixel>(
        mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
    nsViewportInfo viewportInfo = mContext->GetViewportInfo(displaySize);
    mMobileViewportSize = viewportInfo.GetSize();
    MVM_LOG("%p: MVSize updated to %s\n", this,
            ToString(mMobileViewportSize).c_str());
  }
}

void MobileViewportManager::RefreshViewportSize(bool aForceAdjustResolution) {
  // This function gets called by the various triggers that may result in a
  // change of the CSS viewport. In some of these cases (e.g. the meta-viewport
  // tag changes) we want to update the resolution and in others (e.g. the full
  // zoom changing) we don't want to update the resolution. See the comment in
  // UpdateResolutionForViewportSizeChange for some more detail on this.
  // An important assumption we
  // make here is that this RefreshViewportSize function will be called
  // separately for each trigger that changes. For instance it should never get
  // called such that both the full zoom and the meta-viewport tag have changed;
  // instead it would get called twice - once after each trigger changes. This
  // assumption is what allows the aForceAdjustResolution parameter to work as
  // intended; if this assumption is violated then we will need to add extra
  // complicated logic in UpdateResolutionForViewportSizeChange to ensure we
  // only do the resolution update in the right scenarios.

  if (!mContext) {
    return;
  }

  Maybe<float> displayWidthChangeRatio;
  if (Maybe<LayoutDeviceIntSize> newDisplaySize =
          mContext->GetContentViewerSize()) {
    // See the comment in UpdateResolutionForViewportSizeChange for why we're
    // doing this.
    if (mDisplaySize.width > 0) {
      if (aForceAdjustResolution ||
          mDisplaySize.width != newDisplaySize->width) {
        displayWidthChangeRatio =
            Some((float)newDisplaySize->width / (float)mDisplaySize.width);
      }
    } else if (aForceAdjustResolution) {
      displayWidthChangeRatio = Some(1.0f);
    }

    MVM_LOG("%p: Display width change ratio is %f\n", this,
            displayWidthChangeRatio.valueOr(0.0f));
    mDisplaySize = *newDisplaySize;
  }

  MVM_LOG("%p: Computing CSS viewport using %d,%d\n", this, mDisplaySize.width,
          mDisplaySize.height);
  if (mDisplaySize.width == 0 || mDisplaySize.height == 0) {
    // We can't do anything useful here, we should just bail out
    return;
  }

  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
  nsViewportInfo viewportInfo = mContext->GetViewportInfo(displaySize);
  MVM_LOG("%p: viewport info has zooms min=%f max=%f default=%f,valid=%d\n",
          this, viewportInfo.GetMinZoom().scale,
          viewportInfo.GetMaxZoom().scale, viewportInfo.GetDefaultZoom().scale,
          viewportInfo.IsDefaultZoomValid());

  CSSSize viewport = viewportInfo.GetSize();
  MVM_LOG("%p: Computed CSS viewport %s\n", this, ToString(viewport).c_str());

  if (!mIsFirstPaint && mMobileViewportSize == viewport) {
    // Nothing changed, so no need to do a reflow
    return;
  }

  // If it's the first-paint or the viewport changed, we need to update
  // various APZ properties (the zoom and some things that might depend on it)
  MVM_LOG("%p: Updating properties because %d || %d\n", this, mIsFirstPaint,
          mMobileViewportSize != viewport);

  if (mManagerType == ManagerType::VisualAndMetaViewport &&
      (aForceAdjustResolution || mContext->AllowZoomingForDocument())) {
    MVM_LOG("%p: Updating resolution because %d || %d\n", this,
            aForceAdjustResolution, mContext->AllowZoomingForDocument());
    if (mIsFirstPaint) {
      UpdateResolutionForFirstPaint(viewport);
    } else {
      UpdateResolutionForViewportSizeChange(viewport, displayWidthChangeRatio);
    }
  } else {
    // Even without zoom, we need to update that the visual viewport size
    // has changed.
    MVM_LOG("%p: Updating VV size\n", this);
    RefreshVisualViewportSize();
  }
  if (gfxPlatform::AsyncPanZoomEnabled()) {
    UpdateDisplayPortMargins();
  }

  // Update internal state.
  mMobileViewportSize = viewport;

  if (mManagerType == ManagerType::VisualViewportOnly) {
    MVM_LOG("%p: Visual-only, so aborting before reflow\n", this);
    mIsFirstPaint = false;
    return;
  }

  RefPtr<MobileViewportManager> strongThis(this);

  // Kick off a reflow.
  MVM_LOG("%p: Triggering reflow with viewport %s\n", this,
          ToString(viewport).c_str());
  mContext->Reflow(viewport);

  // We are going to fit the content to the display width if the initial-scale
  // is not specied and if the content is still wider than the display width.
  ShrinkToDisplaySizeIfNeeded();

  mIsFirstPaint = false;
}

void MobileViewportManager::ShrinkToDisplaySizeIfNeeded() {
  if (!mContext) {
    return;
  }

  if (mManagerType == ManagerType::VisualViewportOnly) {
    MVM_LOG("%p: Visual-only, so aborting ShrinkToDisplaySizeIfNeeded\n", this);
    return;
  }

  if (!mContext->AllowZoomingForDocument() || mContext->IsInReaderMode()) {
    // If zoom is disabled, we don't scale down wider contents to fit them
    // into device screen because users won't be able to zoom out the tiny
    // contents.
    // We special-case reader mode, because it doesn't allow zooming, but
    // the restriction is often not yet in place at the time this logic
    // runs.
    return;
  }

  if (Maybe<CSSRect> scrollableRect =
          mContext->CalculateScrollableRectForRSF()) {
    MVM_LOG("%p: ShrinkToDisplaySize using scrollableRect %s\n", this,
            ToString(scrollableRect->Size()).c_str());
    UpdateResolutionForContentSizeChange(scrollableRect->Size());
  }
}

CSSSize MobileViewportManager::GetIntrinsicCompositionSize() const {
  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
  ScreenIntSize compositionSize = GetCompositionSize(displaySize);
  CSSToScreenScale intrinsicScale =
      ComputeIntrinsicScale(mContext->GetViewportInfo(displaySize),
                            compositionSize, mMobileViewportSize);

  return ScreenSize(compositionSize) / intrinsicScale;
}

ParentLayerSize MobileViewportManager::GetCompositionSizeWithoutDynamicToolbar()
    const {
  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
  return ViewAs<ParentLayerPixel>(
      ScreenSize(GetCompositionSize(displaySize)),
      PixelCastJustification::ScreenIsParentLayerForRoot);
}
