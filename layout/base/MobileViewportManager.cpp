/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileViewportManager.h"

#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsViewManager.h"
#include "nsViewportInfo.h"
#include "UnitTransforms.h"
#include "nsIDocument.h"

#define MVM_LOG(...)
// #define MVM_LOG(...) printf_stderr("MVM: " __VA_ARGS__)

NS_IMPL_ISUPPORTS(MobileViewportManager, nsIDOMEventListener, nsIObserver)

#define DOM_META_ADDED NS_LITERAL_STRING("DOMMetaAdded")
#define DOM_META_CHANGED NS_LITERAL_STRING("DOMMetaChanged")
#define FULL_ZOOM_CHANGE NS_LITERAL_STRING("FullZoomChange")
#define LOAD NS_LITERAL_STRING("load")
#define BEFORE_FIRST_PAINT NS_LITERAL_CSTRING("before-first-paint")

using namespace mozilla;
using namespace mozilla::layers;

MobileViewportManager::MobileViewportManager(nsIPresShell* aPresShell,
                                             nsIDocument* aDocument)
    : mDocument(aDocument),
      mPresShell(aPresShell),
      mIsFirstPaint(false),
      mPainted(false) {
  MOZ_ASSERT(mPresShell);
  MOZ_ASSERT(mDocument);

  MVM_LOG("%p: creating with presShell %p document %p\n", this, mPresShell,
          aDocument);

  if (nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow()) {
    mEventTarget = window->GetChromeEventHandler();
  }
  if (mEventTarget) {
    mEventTarget->AddEventListener(DOM_META_ADDED, this, false);
    mEventTarget->AddEventListener(DOM_META_CHANGED, this, false);
    mEventTarget->AddEventListener(FULL_ZOOM_CHANGE, this, false);
    mEventTarget->AddEventListener(LOAD, this, true);
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, BEFORE_FIRST_PAINT.Data(), false);
  }
}

MobileViewportManager::~MobileViewportManager() {}

void MobileViewportManager::Destroy() {
  MVM_LOG("%p: destroying\n", this);

  if (mEventTarget) {
    mEventTarget->RemoveEventListener(DOM_META_ADDED, this, false);
    mEventTarget->RemoveEventListener(DOM_META_CHANGED, this, false);
    mEventTarget->RemoveEventListener(FULL_ZOOM_CHANGE, this, false);
    mEventTarget->RemoveEventListener(LOAD, this, true);
    mEventTarget = nullptr;
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, BEFORE_FIRST_PAINT.Data());
  }

  mDocument = nullptr;
  mPresShell = nullptr;
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
  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
  CSSToScreenScale intrinsicScale =
      ComputeIntrinsicScale(mDocument->GetViewportInfo(displaySize),
                            displaySize, mMobileViewportSize);
  CSSToLayoutDeviceScale cssToDev =
      mPresShell->GetPresContext()->CSSToDevPixelScale();
  return (intrinsicScale / cssToDev).scale;
}

mozilla::CSSToScreenScale MobileViewportManager::ComputeIntrinsicScale(
    const nsViewportInfo& aViewportInfo,
    const mozilla::ScreenIntSize& aDisplaySize,
    const mozilla::CSSSize& aViewportSize) const {
  CSSToScreenScale intrinsicScale =
      MaxScaleRatio(ScreenSize(aDisplaySize), aViewportSize);
  MVM_LOG("%p: Intrinsic computed zoom is %f\n", this, intrinsicScale.scale);
  return ClampZoom(intrinsicScale, aViewportInfo);
}

void MobileViewportManager::RequestReflow() {
  MVM_LOG("%p: got a reflow request\n", this);
  RefreshViewportSize(false);
}

void MobileViewportManager::ResolutionUpdated() {
  MVM_LOG("%p: resolution updated\n", this);
  if (!mPainted) {
    // Save the value, so our default zoom calculation
    // can take it into account later on.
    SetRestoreResolution(mPresShell->GetResolution());
  }
  RefreshVisualViewportSize();
}

NS_IMETHODIMP
MobileViewportManager::HandleEvent(dom::Event* event) {
  nsAutoString type;
  event->GetType(type);

  if (type.Equals(DOM_META_ADDED)) {
    MVM_LOG("%p: got a dom-meta-added event\n", this);
    RefreshViewportSize(mPainted);
  } else if (type.Equals(DOM_META_CHANGED)) {
    MVM_LOG("%p: got a dom-meta-changed event\n", this);
    RefreshViewportSize(mPainted);
  } else if (type.Equals(FULL_ZOOM_CHANGE)) {
    MVM_LOG("%p: got a full-zoom-change event\n", this);
    RefreshViewportSize(false);
  } else if (type.Equals(LOAD)) {
    MVM_LOG("%p: got a load event\n", this);
    if (!mPainted) {
      // Load event got fired before the before-first-paint message
      SetInitialViewport();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
MobileViewportManager::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  if (SameCOMIdentity(aSubject, mDocument) &&
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
  if (zoom < aViewportInfo.GetMinZoom()) {
    zoom = aViewportInfo.GetMinZoom();
    MVM_LOG("%p: Clamped to %f\n", this, zoom.scale);
  }
  if (zoom > aViewportInfo.GetMaxZoom()) {
    zoom = aViewportInfo.GetMaxZoom();
    MVM_LOG("%p: Clamped to %f\n", this, zoom.scale);
  }
  return zoom;
}

CSSToScreenScale MobileViewportManager::ScaleZoomWithDisplayWidth(
    const CSSToScreenScale& aZoom, const float& aDisplayWidthChangeRatio,
    const CSSSize& aNewViewport, const CSSSize& aOldViewport) {
  float cssViewportChangeRatio = (aOldViewport.width == 0)
                                     ? 1.0f
                                     : aNewViewport.width / aOldViewport.width;
  CSSToScreenScale newZoom(aZoom.scale * aDisplayWidthChangeRatio /
                           cssViewportChangeRatio);
  MVM_LOG("%p: Old zoom was %f, changed by %f/%f to %f\n", this, aZoom.scale,
          aDisplayWidthChangeRatio, cssViewportChangeRatio, newZoom.scale);
  return newZoom;
}

static CSSToScreenScale ResolutionToZoom(LayoutDeviceToLayerScale aResolution,
                                         CSSToLayoutDeviceScale aCssToDev) {
  return ViewTargetAs<ScreenPixel>(
      aCssToDev * aResolution / ParentLayerToLayerScale(1),
      PixelCastJustification::ScreenIsParentLayerForRoot);
}

static LayoutDeviceToLayerScale ZoomToResolution(
    CSSToScreenScale aZoom, CSSToLayoutDeviceScale aCssToDev) {
  return ViewTargetAs<ParentLayerPixel>(
             aZoom, PixelCastJustification::ScreenIsParentLayerForRoot) /
         aCssToDev * ParentLayerToLayerScale(1);
}

void MobileViewportManager::UpdateResolution(
    const nsViewportInfo& aViewportInfo, const ScreenIntSize& aDisplaySize,
    const CSSSize& aViewportOrContentSize,
    const Maybe<float>& aDisplayWidthChangeRatio, UpdateType aType) {
  CSSToLayoutDeviceScale cssToDev =
      mPresShell->GetPresContext()->CSSToDevPixelScale();
  LayoutDeviceToLayerScale res(mPresShell->GetResolution());
  CSSToScreenScale zoom = ResolutionToZoom(res, cssToDev);
  Maybe<CSSToScreenScale> newZoom;

  ScreenIntSize compositionSize = GetCompositionSize(aDisplaySize);
  CSSToScreenScale intrinsicScale = ComputeIntrinsicScale(
      aViewportInfo, compositionSize, aViewportOrContentSize);

  if (aType == UpdateType::ViewportSize) {
    const CSSSize& viewportSize = aViewportOrContentSize;
    if (mIsFirstPaint) {
      CSSToScreenScale defaultZoom;
      if (mRestoreResolution) {
        LayoutDeviceToLayerScale restoreResolution(mRestoreResolution.value());
        CSSToScreenScale restoreZoom =
            ResolutionToZoom(restoreResolution, cssToDev);
        if (mRestoreDisplaySize) {
          CSSSize prevViewport =
              mDocument->GetViewportInfo(mRestoreDisplaySize.value()).GetSize();
          float restoreDisplayWidthChangeRatio =
              (mRestoreDisplaySize.value().width > 0)
                  ? (float)compositionSize.width /
                        (float)mRestoreDisplaySize.value().width
                  : 1.0f;

          restoreZoom = ScaleZoomWithDisplayWidth(
              restoreZoom, restoreDisplayWidthChangeRatio, viewportSize,
              prevViewport);
        }
        defaultZoom = restoreZoom;
        MVM_LOG("%p: restored zoom is %f\n", this, defaultZoom.scale);
        defaultZoom = ClampZoom(defaultZoom, aViewportInfo);
      } else {
        defaultZoom = aViewportInfo.GetDefaultZoom();
        MVM_LOG("%p: default zoom from viewport is %f\n", this,
                defaultZoom.scale);
        if (!aViewportInfo.IsDefaultZoomValid()) {
          defaultZoom = intrinsicScale;
        }
      }
      MOZ_ASSERT(aViewportInfo.GetMinZoom() <= defaultZoom &&
                 defaultZoom <= aViewportInfo.GetMaxZoom());

      // On first paint, we always consider the zoom to have changed.
      newZoom = Some(defaultZoom);
    } else {
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
      // want to compute a new CSS viewport, but we don't want to update the
      // resolution.
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
      if (aDisplayWidthChangeRatio) {
        newZoom = Some(
            ScaleZoomWithDisplayWidth(zoom, aDisplayWidthChangeRatio.value(),
                                      viewportSize, mMobileViewportSize));
      }
    }
  } else {  // aType == UpdateType::ContentSize
    MOZ_ASSERT(aType == UpdateType::ContentSize);
    MOZ_ASSERT(aDisplayWidthChangeRatio.isNothing());

    // We try to scale down the contents only IF the document has no
    // initial-scale AND IF it's the initial paint AND IF it's not restored
    // documents.
    if (mIsFirstPaint && !mRestoreResolution &&
        !aViewportInfo.IsDefaultZoomValid()) {
      if (zoom != intrinsicScale) {
        newZoom = Some(intrinsicScale);
      }
    } else {
      // Even in other scenarios, we want to ensure that zoom level is
      // not _smaller_ than the intrinsic scale, otherwise we might be
      // trying to show regions where there is no content to show.
      if (zoom < intrinsicScale) {
        newZoom = Some(intrinsicScale);
      }
    }
  }

  // If the zoom has changed, update the pres shell resolution accordingly.
  if (newZoom) {
    LayoutDeviceToLayerScale resolution = ZoomToResolution(*newZoom, cssToDev);
    MVM_LOG("%p: setting resolution %f\n", this, resolution.scale);
    mPresShell->SetResolutionAndScaleTo(resolution.scale, nsGkAtoms::other);

    MVM_LOG("%p: New zoom is %f\n", this, newZoom->scale);
  }

  // The visual viewport size depends on both the zoom and the display size,
  // and needs to be updated if either might have changed.
  if (newZoom || aType == UpdateType::ViewportSize) {
    UpdateVisualViewportSize(aDisplaySize, newZoom ? *newZoom : zoom);
  }
}

ScreenIntSize MobileViewportManager::GetCompositionSize(
    const ScreenIntSize& aDisplaySize) const {
  ScreenIntSize compositionSize(aDisplaySize);
  ScreenMargin scrollbars =
      LayoutDeviceMargin::FromAppUnits(
          nsLayoutUtils::ScrollbarAreaToExcludeFromCompositionBoundsFor(
              mPresShell->GetRootScrollFrame()),
          mPresShell->GetPresContext()->AppUnitsPerDevPixel())
      // Scrollbars are not subject to resolution scaling, so LD pixels =
      // Screen pixels for them.
      * LayoutDeviceToScreenScale(1.0f);

  compositionSize.width -= scrollbars.LeftRight();
  compositionSize.height -= scrollbars.TopBottom();

  return compositionSize;
}

void MobileViewportManager::UpdateVisualViewportSize(
    const ScreenIntSize& aDisplaySize, const CSSToScreenScale& aZoom) {
  ScreenSize compositionSize = ScreenSize(GetCompositionSize(aDisplaySize));

  CSSSize compSize = compositionSize / aZoom;
  MVM_LOG("%p: Setting VVPS %s\n", this, Stringify(compSize).c_str());
  nsLayoutUtils::SetVisualViewportSize(mPresShell, compSize);
}

void MobileViewportManager::UpdateDisplayPortMargins() {
  if (nsIFrame* root = mPresShell->GetRootScrollFrame()) {
    bool hasDisplayPort = nsLayoutUtils::HasDisplayPort(root->GetContent());
    bool hasResolution =
        mPresShell->ScaleToResolution() && mPresShell->GetResolution() != 1.0f;
    if (!hasDisplayPort && !hasResolution) {
      // We only want to update the displayport if there is one already, or
      // add one if there's a resolution on the document (see bug 1225508
      // comment 1).
      return;
    }
    nsRect displayportBase = nsRect(
        nsPoint(0, 0), nsLayoutUtils::CalculateCompositionSizeForFrame(root));
    // We only create MobileViewportManager for root content documents. If that
    // ever changes we'd need to limit the size of this displayport base rect
    // because non-toplevel documents have no limit on their size.
    MOZ_ASSERT(mPresShell->GetPresContext()->IsRootContentDocument());
    nsLayoutUtils::SetDisplayPortBaseIfNotSet(root->GetContent(),
                                              displayportBase);
    nsIScrollableFrame* scrollable = do_QueryFrame(root);
    nsLayoutUtils::CalculateAndSetDisplayPortMargins(
        scrollable, nsLayoutUtils::RepaintMode::DoNotRepaint);
  }
}

void MobileViewportManager::RefreshVisualViewportSize() {
  // This function is a subset of RefreshViewportSize, and only updates the
  // visual viewport size.

  if (!gfxPrefs::APZAllowZooming()) {
    return;
  }

  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);

  CSSToLayoutDeviceScale cssToDev =
      mPresShell->GetPresContext()->CSSToDevPixelScale();
  LayoutDeviceToLayerScale res(mPresShell->GetResolution());
  CSSToScreenScale zoom = ViewTargetAs<ScreenPixel>(
      cssToDev * res / ParentLayerToLayerScale(1),
      PixelCastJustification::ScreenIsParentLayerForRoot);

  UpdateVisualViewportSize(displaySize, zoom);
}

void MobileViewportManager::RefreshViewportSize(bool aForceAdjustResolution) {
  // This function gets called by the various triggers that may result in a
  // change of the CSS viewport. In some of these cases (e.g. the meta-viewport
  // tag changes) we want to update the resolution and in others (e.g. the full
  // zoom changing) we don't want to update the resolution. See the comment in
  // UpdateResolution for some more detail on this. An important assumption we
  // make here is that this RefreshViewportSize function will be called
  // separately for each trigger that changes. For instance it should never get
  // called such that both the full zoom and the meta-viewport tag have changed;
  // instead it would get called twice - once after each trigger changes. This
  // assumption is what allows the aForceAdjustResolution parameter to work as
  // intended; if this assumption is violated then we will need to add extra
  // complicated logic in UpdateResolution to ensure we only do the resolution
  // update in the right scenarios.

  Maybe<float> displayWidthChangeRatio;
  LayoutDeviceIntSize newDisplaySize;
  if (nsLayoutUtils::GetContentViewerSize(mPresShell->GetPresContext(),
                                          newDisplaySize)) {
    // See the comment in UpdateResolution for why we're doing this.
    if (mDisplaySize.width > 0) {
      if (aForceAdjustResolution ||
          mDisplaySize.width != newDisplaySize.width) {
        displayWidthChangeRatio =
            Some((float)newDisplaySize.width / (float)mDisplaySize.width);
      }
    } else if (aForceAdjustResolution) {
      displayWidthChangeRatio = Some(1.0f);
    }

    MVM_LOG("%p: Display width change ratio is %f\n", this,
            displayWidthChangeRatio.valueOr(0.0f));
    mDisplaySize = newDisplaySize;
  }

  MVM_LOG("%p: Computing CSS viewport using %d,%d\n", this, mDisplaySize.width,
          mDisplaySize.height);
  if (mDisplaySize.width == 0 || mDisplaySize.height == 0) {
    // We can't do anything useful here, we should just bail out
    return;
  }

  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
      mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);
  nsViewportInfo viewportInfo = mDocument->GetViewportInfo(displaySize);

  CSSSize viewport = viewportInfo.GetSize();
  MVM_LOG("%p: Computed CSS viewport %s\n", this, Stringify(viewport).c_str());

  if (!mIsFirstPaint && mMobileViewportSize == viewport) {
    // Nothing changed, so no need to do a reflow
    return;
  }

  // If it's the first-paint or the viewport changed, we need to update
  // various APZ properties (the zoom and some things that might depend on it)
  MVM_LOG("%p: Updating properties because %d || %d\n", this, mIsFirstPaint,
          mMobileViewportSize != viewport);

  if (gfxPrefs::APZAllowZooming()) {
    UpdateResolution(viewportInfo, displaySize, viewport,
                     displayWidthChangeRatio, UpdateType::ViewportSize);
  }
  if (gfxPlatform::AsyncPanZoomEnabled()) {
    UpdateDisplayPortMargins();
  }

  CSSSize oldSize = mMobileViewportSize;

  // Update internal state.
  mMobileViewportSize = viewport;

  // Kick off a reflow.
  mPresShell->ResizeReflowIgnoreOverride(
      nsPresContext::CSSPixelsToAppUnits(viewport.width),
      nsPresContext::CSSPixelsToAppUnits(viewport.height),
      nsPresContext::CSSPixelsToAppUnits(oldSize.width),
      nsPresContext::CSSPixelsToAppUnits(oldSize.height));

  // We are going to fit the content to the display width if the initial-scale
  // is not specied and if the content is still wider than the display width.
  ShrinkToDisplaySizeIfNeeded(viewportInfo, displaySize);

  mIsFirstPaint = false;
}

void MobileViewportManager::ShrinkToDisplaySizeIfNeeded(
    nsViewportInfo& aViewportInfo, const ScreenIntSize& aDisplaySize) {
  if (!gfxPrefs::APZAllowZooming()) {
    // If the APZ is disabled, we don't scale down wider contents to fit them
    // into device screen because users won't be able to zoom out the tiny
    // contents.
    return;
  }

  nsIScrollableFrame* rootScrollableFrame =
      mPresShell->GetRootScrollFrameAsScrollable();
  if (rootScrollableFrame) {
    nsRect scrollableRect = nsLayoutUtils::CalculateScrollableRectForFrame(
        rootScrollableFrame, nullptr);
    CSSSize contentSize = CSSSize::FromAppUnits(scrollableRect.Size());
    UpdateResolution(aViewportInfo, aDisplaySize, contentSize, Nothing(),
                     UpdateType::ContentSize);
  }
}
