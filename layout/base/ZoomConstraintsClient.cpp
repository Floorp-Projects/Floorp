/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ZoomConstraintsClient.h"

#include <inttypes.h>
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "mozilla/layers/ZoomConstraints.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsPoint.h"
#include "nsView.h"
#include "nsViewportInfo.h"
#include "Units.h"
#include "UnitTransforms.h"

static mozilla::LazyLogModule sApzZoomLog("apz.zoom");
#define ZCC_LOG(...) MOZ_LOG(sApzZoomLog, LogLevel::Debug, (__VA_ARGS__))

NS_IMPL_ISUPPORTS(ZoomConstraintsClient, nsIDOMEventListener, nsIObserver)

#define DOM_META_ADDED u"DOMMetaAdded"_ns
#define DOM_META_CHANGED u"DOMMetaChanged"_ns
#define FULLSCREEN_CHANGED u"fullscreenchange"_ns
#define BEFORE_FIRST_PAINT "before-first-paint"_ns
#define COMPOSITOR_REINITIALIZED "compositor-reinitialized"_ns
#define NS_PREF_CHANGED "nsPref:changed"_ns

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

ZoomConstraintsClient::ZoomConstraintsClient()
    : mDocument(nullptr),
      mPresShell(nullptr),
      mZoomConstraints(false, false, CSSToParentLayerScale(1.f),
                       CSSToParentLayerScale(1.f)) {}

ZoomConstraintsClient::~ZoomConstraintsClient() = default;

static nsIWidget* GetWidget(PresShell* aPresShell) {
  if (!aPresShell) {
    return nullptr;
  }
  if (nsIFrame* rootFrame = aPresShell->GetRootFrame()) {
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_UIKIT)
    return rootFrame->GetNearestWidget();
#else
    if (nsView* view = rootFrame->GetView()) {
      return view->GetWidget();
    }
#endif
  }
  return nullptr;
}

void ZoomConstraintsClient::Destroy() {
  if (!(mPresShell && mDocument)) {
    return;
  }

  ZCC_LOG("Destroying %p\n", this);

  if (mEventTarget) {
    mEventTarget->RemoveEventListener(DOM_META_ADDED, this, false);
    mEventTarget->RemoveEventListener(DOM_META_CHANGED, this, false);
    mEventTarget->RemoveSystemEventListener(FULLSCREEN_CHANGED, this, false);
    mEventTarget = nullptr;
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, BEFORE_FIRST_PAINT.Data());
    observerService->RemoveObserver(this, COMPOSITOR_REINITIALIZED.Data());
  }

  Preferences::RemoveObserver(this, "browser.ui.zoom.force-user-scalable");

  if (mGuid) {
    if (nsIWidget* widget = GetWidget(mPresShell)) {
      ZCC_LOG("Sending null constraints in %p for { %u, %" PRIu64 " }\n", this,
              mGuid->mPresShellId, mGuid->mScrollId);
      widget->UpdateZoomConstraints(mGuid->mPresShellId, mGuid->mScrollId,
                                    Nothing());
      mGuid = Nothing();
    }
  }

  mDocument = nullptr;
  mPresShell = nullptr;
}

void ZoomConstraintsClient::Init(PresShell* aPresShell, Document* aDocument) {
  if (!(aPresShell && aDocument)) {
    return;
  }

  mPresShell = aPresShell;
  mDocument = aDocument;

  if (nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow()) {
    mEventTarget = window->GetParentTarget();
  }
  if (mEventTarget) {
    mEventTarget->AddEventListener(DOM_META_ADDED, this, false);
    mEventTarget->AddEventListener(DOM_META_CHANGED, this, false);
    mEventTarget->AddSystemEventListener(FULLSCREEN_CHANGED, this, false);
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, BEFORE_FIRST_PAINT.Data(), false);
    observerService->AddObserver(this, COMPOSITOR_REINITIALIZED.Data(), false);
  }

  Preferences::AddStrongObserver(this, "browser.ui.zoom.force-user-scalable");
}

NS_IMETHODIMP
ZoomConstraintsClient::HandleEvent(dom::Event* event) {
  nsAutoString type;
  event->GetType(type);

  if (type.Equals(DOM_META_ADDED)) {
    ZCC_LOG("Got a dom-meta-added event in %p\n", this);
    RefreshZoomConstraints();
  } else if (type.Equals(DOM_META_CHANGED)) {
    ZCC_LOG("Got a dom-meta-changed event in %p\n", this);
    RefreshZoomConstraints();
  } else if (type.Equals(FULLSCREEN_CHANGED)) {
    ZCC_LOG("Got a fullscreen-change event in %p\n", this);
    RefreshZoomConstraints();
  }

  return NS_OK;
}

NS_IMETHODIMP
ZoomConstraintsClient::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  if (SameCOMIdentity(aSubject, ToSupports(mDocument)) &&
      BEFORE_FIRST_PAINT.EqualsASCII(aTopic)) {
    ZCC_LOG("Got a before-first-paint event in %p\n", this);
    RefreshZoomConstraints();
  } else if (COMPOSITOR_REINITIALIZED.EqualsASCII(aTopic)) {
    ZCC_LOG("Got a compositor-reinitialized notification in %p\n", this);
    RefreshZoomConstraints();
  } else if (NS_PREF_CHANGED.EqualsASCII(aTopic)) {
    ZCC_LOG("Got a pref-change event in %p\n", this);
    // We need to run this later because all the pref change listeners need
    // to execute before we can be guaranteed that
    // StaticPrefs::browser_ui_zoom_force_user_scalable() returns the updated
    // value.

    RefPtr<nsRunnableMethod<ZoomConstraintsClient>> event =
        NewRunnableMethod("ZoomConstraintsClient::RefreshZoomConstraints", this,
                          &ZoomConstraintsClient::RefreshZoomConstraints);
    mDocument->Dispatch(event.forget());
  }
  return NS_OK;
}

void ZoomConstraintsClient::ScreenSizeChanged() {
  ZCC_LOG("Got a screen-size change notification in %p\n", this);
  RefreshZoomConstraints();
}

static mozilla::layers::ZoomConstraints ComputeZoomConstraintsFromViewportInfo(
    const nsViewportInfo& aViewportInfo, Document* aDocument) {
  mozilla::layers::ZoomConstraints constraints;
  constraints.mAllowZoom = aViewportInfo.IsZoomAllowed() &&
                           nsLayoutUtils::AllowZoomingForDocument(aDocument);
  constraints.mAllowDoubleTapZoom =
      constraints.mAllowZoom && StaticPrefs::apz_allow_double_tap_zooming();
  if (constraints.mAllowZoom) {
    constraints.mMinZoom.scale = aViewportInfo.GetMinZoom().scale;
    constraints.mMaxZoom.scale = aViewportInfo.GetMaxZoom().scale;
  } else {
    constraints.mMinZoom.scale = aViewportInfo.GetDefaultZoom().scale;
    constraints.mMaxZoom.scale = aViewportInfo.GetDefaultZoom().scale;
  }
  return constraints;
}

void ZoomConstraintsClient::RefreshZoomConstraints() {
  mZoomConstraints = ZoomConstraints(false, false, CSSToParentLayerScale(1.f),
                                     CSSToParentLayerScale(1.f));

  nsIWidget* widget = GetWidget(mPresShell);
  if (!widget) {
    return;
  }

  uint32_t presShellId = 0;
  ScrollableLayerGuid::ViewID viewId = ScrollableLayerGuid::NULL_SCROLL_ID;
  bool scrollIdentifiersValid =
      APZCCallbackHelper::GetOrCreateScrollIdentifiers(
          mDocument->GetDocumentElement(), &presShellId, &viewId);
  if (!scrollIdentifiersValid) {
    return;
  }

  LayoutDeviceIntSize screenSize;
  if (!nsLayoutUtils::GetContentViewerSize(mPresShell->GetPresContext(),
                                           screenSize)) {
    return;
  }

  nsViewportInfo viewportInfo = mDocument->GetViewportInfo(ViewAs<ScreenPixel>(
      screenSize, PixelCastJustification::LayoutDeviceIsScreenForBounds));

  mZoomConstraints =
      ComputeZoomConstraintsFromViewportInfo(viewportInfo, mDocument);

  if (mDocument->Fullscreen()) {
    ZCC_LOG("%p is in fullscreen, disallowing zooming\n", this);
    mZoomConstraints.mAllowZoom = false;
    mZoomConstraints.mAllowDoubleTapZoom = false;
  }

  if (mDocument->IsStaticDocument()) {
    ZCC_LOG("%p is in print or print preview, disallowing double tap zooming\n",
            this);
    mZoomConstraints.mAllowDoubleTapZoom = false;
  }

  if (nsContentUtils::IsPDFJS(mDocument->GetPrincipal())) {
    ZCC_LOG("%p is pdf.js viewer, disallowing double tap zooming\n", this);
    mZoomConstraints.mAllowDoubleTapZoom = false;
  }

  // On macOS the OS can send us a double tap zoom event from the touchpad and
  // there are no touch screen macOS devices so we never wait to see if a second
  // tap is coming so we can always allow double tap zooming on mac. We need
  // this because otherwise the width check usually disables it.
  bool allow_double_tap_always = false;
#ifdef XP_MACOSX
  allow_double_tap_always =
      StaticPrefs::apz_mac_enable_double_tap_zoom_touchpad_gesture();
#endif
  if (!allow_double_tap_always && mZoomConstraints.mAllowDoubleTapZoom) {
    // If the CSS viewport is narrower than the screen (i.e. width <=
    // device-width) then we disable double-tap-to-zoom behaviour.
    CSSToLayoutDeviceScale scale =
        mPresShell->GetPresContext()->CSSToDevPixelScale();
    if ((viewportInfo.GetSize() * scale).width <= screenSize.width) {
      mZoomConstraints.mAllowDoubleTapZoom = false;
    }
  }

  // We only ever create a ZoomConstraintsClient for an RCD, so the RSF of
  // the presShell must be the RCD-RSF (if it exists).
  MOZ_ASSERT(mPresShell->GetPresContext()->IsRootContentDocumentCrossProcess());
  if (nsIScrollableFrame* rcdrsf =
          mPresShell->GetRootScrollFrameAsScrollable()) {
    ZCC_LOG("Notifying RCD-RSF that it is zoomable: %d\n",
            mZoomConstraints.mAllowZoom);
    rcdrsf->SetZoomableByAPZ(mZoomConstraints.mAllowZoom);
  }

  ScrollableLayerGuid newGuid(LayersId{0}, presShellId, viewId);
  if (mGuid && mGuid.value() != newGuid) {
    ZCC_LOG("Clearing old constraints in %p for { %u, %" PRIu64 " }\n", this,
            mGuid->mPresShellId, mGuid->mScrollId);
    // If the guid changes, send a message to clear the old one
    widget->UpdateZoomConstraints(mGuid->mPresShellId, mGuid->mScrollId,
                                  Nothing());
  }
  mGuid = Some(newGuid);
  ZCC_LOG("Sending constraints %s in %p for { %u, %" PRIu64 " }\n",
          ToString(mZoomConstraints).c_str(), this, presShellId, viewId);
  widget->UpdateZoomConstraints(presShellId, viewId, Some(mZoomConstraints));
}
