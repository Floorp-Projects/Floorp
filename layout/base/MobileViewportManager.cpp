/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileViewportManager.h"

#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/PresShell.h"
#include "nsIDOMEvent.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsViewManager.h"
#include "nsViewportInfo.h"
#include "UnitTransforms.h"
#include "nsIDocument.h"

#define MVM_LOG(...)
// #define MVM_LOG(...) printf_stderr("MVM: " __VA_ARGS__)

NS_IMPL_ISUPPORTS(MobileViewportManager, nsIDOMEventListener, nsIObserver)

static const nsLiteralString DOM_META_ADDED = NS_LITERAL_STRING("DOMMetaAdded");
static const nsLiteralString DOM_META_CHANGED = NS_LITERAL_STRING("DOMMetaChanged");
static const nsLiteralString FULL_ZOOM_CHANGE = NS_LITERAL_STRING("FullZoomChange");
static const nsLiteralString LOAD = NS_LITERAL_STRING("load");
static const nsLiteralCString BEFORE_FIRST_PAINT = NS_LITERAL_CSTRING("before-first-paint");

using namespace mozilla;
using namespace mozilla::layers;

MobileViewportManager::MobileViewportManager(nsIPresShell* aPresShell,
                                             nsIDocument* aDocument)
  : mDocument(aDocument)
  , mPresShell(aPresShell)
  , mIsFirstPaint(false)
  , mPainted(false)
{
  MOZ_ASSERT(mPresShell);
  MOZ_ASSERT(mDocument);

  MVM_LOG("%p: creating with presShell %p document %p\n", this, mPresShell, aDocument);

  if (nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow()) {
    mEventTarget = window->GetChromeEventHandler();
  }
  if (mEventTarget) {
    mEventTarget->AddEventListener(DOM_META_ADDED, this, false);
    mEventTarget->AddEventListener(DOM_META_CHANGED, this, false);
    mEventTarget->AddEventListener(FULL_ZOOM_CHANGE, this, false);
    mEventTarget->AddEventListener(LOAD, this, true);
  }

  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, BEFORE_FIRST_PAINT.Data(), false);
  }
}

MobileViewportManager::~MobileViewportManager()
{
}

void
MobileViewportManager::Destroy()
{
  MVM_LOG("%p: destroying\n", this);

  if (mEventTarget) {
    mEventTarget->RemoveEventListener(DOM_META_ADDED, this, false);
    mEventTarget->RemoveEventListener(DOM_META_CHANGED, this, false);
    mEventTarget->RemoveEventListener(FULL_ZOOM_CHANGE, this, false);
    mEventTarget->RemoveEventListener(LOAD, this, true);
    mEventTarget = nullptr;
  }

  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, BEFORE_FIRST_PAINT.Data());
  }

  mDocument = nullptr;
  mPresShell = nullptr;
}

void
MobileViewportManager::SetRestoreResolution(float aResolution,
                                            LayoutDeviceIntSize aDisplaySize)
{
  mRestoreResolution = Some(aResolution);
  ScreenIntSize restoreDisplaySize = ViewAs<ScreenPixel>(aDisplaySize,
    PixelCastJustification::LayoutDeviceIsScreenForBounds);
  mRestoreDisplaySize = Some(restoreDisplaySize);
}

void
MobileViewportManager::RequestReflow()
{
  MVM_LOG("%p: got a reflow request\n", this);
  RefreshViewportSize(false);
}

void
MobileViewportManager::ResolutionUpdated()
{
  MVM_LOG("%p: resolution updated\n", this);
  RefreshSPCSPS();
}

NS_IMETHODIMP
MobileViewportManager::HandleEvent(nsIDOMEvent* event)
{
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
MobileViewportManager::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  if (SameCOMIdentity(aSubject, mDocument) && BEFORE_FIRST_PAINT.EqualsASCII(aTopic)) {
    MVM_LOG("%p: got a before-first-paint event\n", this);
    if (!mPainted) {
      // before-first-paint message arrived before load event
      SetInitialViewport();
    }
  }
  return NS_OK;
}

void
MobileViewportManager::SetInitialViewport()
{
  MVM_LOG("%p: setting initial viewport\n", this);
  mIsFirstPaint = true;
  mPainted = true;
  RefreshViewportSize(false);
}

CSSToScreenScale
MobileViewportManager::ClampZoom(const CSSToScreenScale& aZoom,
                                 const nsViewportInfo& aViewportInfo)
{
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

LayoutDeviceToLayerScale
MobileViewportManager::ScaleResolutionWithDisplayWidth(const LayoutDeviceToLayerScale& aRes,
                                                       const float& aDisplayWidthChangeRatio,
                                                       const CSSSize& aNewViewport,
                                                       const CSSSize& aOldViewport)
{
  float cssViewportChangeRatio = (aOldViewport.width == 0)
     ? 1.0f : aNewViewport.width / aOldViewport.width;
  LayoutDeviceToLayerScale newRes(aRes.scale * aDisplayWidthChangeRatio
    / cssViewportChangeRatio);
  MVM_LOG("%p: Old resolution was %f, changed by %f/%f to %f\n", this, aRes.scale,
    aDisplayWidthChangeRatio, cssViewportChangeRatio, newRes.scale);
  return newRes;
}

CSSToScreenScale
MobileViewportManager::UpdateResolution(const nsViewportInfo& aViewportInfo,
                                        const ScreenIntSize& aDisplaySize,
                                        const CSSSize& aViewport,
                                        const Maybe<float>& aDisplayWidthChangeRatio)
{
  CSSToLayoutDeviceScale cssToDev =
      mPresShell->GetPresContext()->CSSToDevPixelScale();
  LayoutDeviceToLayerScale res(mPresShell->GetResolution());

  if (mIsFirstPaint) {
    CSSToScreenScale defaultZoom;
    if (mRestoreResolution) {
      LayoutDeviceToLayerScale restoreResolution(mRestoreResolution.value());
      if (mRestoreDisplaySize) {
        CSSSize prevViewport = mDocument->GetViewportInfo(mRestoreDisplaySize.value()).GetSize();
        float restoreDisplayWidthChangeRatio = (mRestoreDisplaySize.value().width > 0)
          ? (float)aDisplaySize.width / (float)mRestoreDisplaySize.value().width : 1.0f;

        restoreResolution =
          ScaleResolutionWithDisplayWidth(restoreResolution,
                                          restoreDisplayWidthChangeRatio,
                                          aViewport,
                                          prevViewport);
      }
      defaultZoom = CSSToScreenScale(restoreResolution.scale * cssToDev.scale);
      MVM_LOG("%p: restored zoom is %f\n", this, defaultZoom.scale);
      defaultZoom = ClampZoom(defaultZoom, aViewportInfo);
    } else {
      defaultZoom = aViewportInfo.GetDefaultZoom();
      MVM_LOG("%p: default zoom from viewport is %f\n", this, defaultZoom.scale);
      if (!aViewportInfo.IsDefaultZoomValid()) {
        defaultZoom = MaxScaleRatio(ScreenSize(aDisplaySize), aViewport);
        MVM_LOG("%p: Intrinsic computed zoom is %f\n", this, defaultZoom.scale);
        defaultZoom = ClampZoom(defaultZoom, aViewportInfo);
      }
    }
    MOZ_ASSERT(aViewportInfo.GetMinZoom() <= defaultZoom &&
      defaultZoom <= aViewportInfo.GetMaxZoom());

    CSSToParentLayerScale zoom = ViewTargetAs<ParentLayerPixel>(defaultZoom,
      PixelCastJustification::ScreenIsParentLayerForRoot);

    LayoutDeviceToLayerScale resolution = zoom / cssToDev * ParentLayerToLayerScale(1);
    MVM_LOG("%p: setting resolution %f\n", this, resolution.scale);
    mPresShell->SetResolutionAndScaleTo(resolution.scale);

    return defaultZoom;
  }

  // If this is not a first paint, then in some cases we want to update the pre-
  // existing resolution so as to maintain how much actual content is visible
  // within the display width. Note that "actual content" may be different with
  // respect to CSS pixels because of the CSS viewport size changing.
  //
  // aDisplayWidthChangeRatio is non-empty if:
  // (a) The meta-viewport tag information changes, and so the CSS viewport
  //     might change as a result. If this happens after the content has been
  //     painted, we want to adjust the zoom to compensate. OR
  // (b) The display size changed from a nonzero value to another nonzero value.
  //     This covers the case where e.g. the device was rotated, and again we
  //     want to adjust the zoom to compensate.
  // Note in particular that aDisplayWidthChangeRatio will be None if all that
  // happened was a change in the full-zoom. In this case, we still want to
  // compute a new CSS viewport, but we don't want to update the resolution.
  //
  // Given the above, the algorithm below accounts for all types of changes I
  // can conceive of:
  // 1. screen size changes, CSS viewport does not (pages with no meta viewport
  //    or a fixed size viewport)
  // 2. screen size changes, CSS viewport also does (pages with a device-width
  //    viewport)
  // 3. screen size remains constant, but CSS viewport changes (meta viewport
  //    tag is added or removed)
  // 4. neither screen size nor CSS viewport changes
  if (aDisplayWidthChangeRatio) {
    res = ScaleResolutionWithDisplayWidth(res, aDisplayWidthChangeRatio.value(),
      aViewport, mMobileViewportSize);
    mPresShell->SetResolutionAndScaleTo(res.scale);
  }

  return ViewTargetAs<ScreenPixel>(cssToDev * res / ParentLayerToLayerScale(1),
    PixelCastJustification::ScreenIsParentLayerForRoot);
}

void
MobileViewportManager::UpdateSPCSPS(const ScreenIntSize& aDisplaySize,
                                    const CSSToScreenScale& aZoom)
{
  ScreenSize compositionSize(aDisplaySize);
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
  CSSSize compSize = compositionSize / aZoom;
  MVM_LOG("%p: Setting SPCSPS %s\n", this, Stringify(compSize).c_str());
  nsLayoutUtils::SetScrollPositionClampingScrollPortSize(mPresShell, compSize);
}

void
MobileViewportManager::UpdateDisplayPortMargins()
{
  if (nsIFrame* root = mPresShell->GetRootScrollFrame()) {
    bool hasDisplayPort = nsLayoutUtils::HasDisplayPort(root->GetContent());
    bool hasResolution = mPresShell->ScaleToResolution() &&
        mPresShell->GetResolution() != 1.0f;
    if (!hasDisplayPort && !hasResolution) {
      // We only want to update the displayport if there is one already, or
      // add one if there's a resolution on the document (see bug 1225508
      // comment 1).
      return;
    }
    nsRect displayportBase =
      nsRect(nsPoint(0, 0), nsLayoutUtils::CalculateCompositionSizeForFrame(root));
    // We only create MobileViewportManager for root content documents. If that ever changes
    // we'd need to limit the size of this displayport base rect because non-toplevel documents
    // have no limit on their size.
    MOZ_ASSERT(mPresShell->GetPresContext()->IsRootContentDocument());
    nsLayoutUtils::SetDisplayPortBaseIfNotSet(root->GetContent(), displayportBase);
    nsIScrollableFrame* scrollable = do_QueryFrame(root);
    nsLayoutUtils::CalculateAndSetDisplayPortMargins(scrollable,
      nsLayoutUtils::RepaintMode::DoNotRepaint);
  }
}

void
MobileViewportManager::RefreshSPCSPS()
{
  // This function is a subset of RefreshViewportSize, and only updates the
  // SPCSPS.

  if (!gfxPrefs::APZAllowZooming()) {
    return;
  }

  ScreenIntSize displaySize = ViewAs<ScreenPixel>(
    mDisplaySize, PixelCastJustification::LayoutDeviceIsScreenForBounds);

  CSSToLayoutDeviceScale cssToDev =
      mPresShell->GetPresContext()->CSSToDevPixelScale();
  LayoutDeviceToLayerScale res(mPresShell->GetResolution());
  CSSToScreenScale zoom = ViewTargetAs<ScreenPixel>(cssToDev * res / ParentLayerToLayerScale(1),
    PixelCastJustification::ScreenIsParentLayerForRoot);

  UpdateSPCSPS(displaySize, zoom);
}

void
MobileViewportManager::RefreshViewportSize(bool aForceAdjustResolution)
{
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
  if (nsLayoutUtils::GetContentViewerSize(mPresShell->GetPresContext(), newDisplaySize)) {
    // See the comment in UpdateResolution for why we're doing this.
    if (mDisplaySize.width > 0) {
      if (aForceAdjustResolution || mDisplaySize.width != newDisplaySize.width) {
        displayWidthChangeRatio = Some((float)newDisplaySize.width / (float)mDisplaySize.width);
      }
    } else if (aForceAdjustResolution) {
      displayWidthChangeRatio = Some(1.0f);
    }

    MVM_LOG("%p: Display width change ratio is %f\n", this, displayWidthChangeRatio.valueOr(0.0f));
    mDisplaySize = newDisplaySize;
  }

  MVM_LOG("%p: Computing CSS viewport using %d,%d\n", this,
    mDisplaySize.width, mDisplaySize.height);
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
  MVM_LOG("%p: Updating properties because %d || %d\n", this,
    mIsFirstPaint, mMobileViewportSize != viewport);

  if (gfxPrefs::APZAllowZooming()) {
    CSSToScreenScale zoom = UpdateResolution(viewportInfo, displaySize, viewport,
      displayWidthChangeRatio);
    MVM_LOG("%p: New zoom is %f\n", this, zoom.scale);
    UpdateSPCSPS(displaySize, zoom);
  }
  if (gfxPlatform::AsyncPanZoomEnabled()) {
    UpdateDisplayPortMargins();
  }

  CSSSize oldSize = mMobileViewportSize;

  // Update internal state.
  mIsFirstPaint = false;
  mMobileViewportSize = viewport;

  // Kick off a reflow.
  mPresShell->ResizeReflowIgnoreOverride(
    nsPresContext::CSSPixelsToAppUnits(viewport.width),
    nsPresContext::CSSPixelsToAppUnits(viewport.height),
    nsPresContext::CSSPixelsToAppUnits(oldSize.width),
    nsPresContext::CSSPixelsToAppUnits(oldSize.height));
}
