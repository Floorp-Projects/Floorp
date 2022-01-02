/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMWindowUtils.h"

#include "MobileViewportManager.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "nsPresContext.h"
#include "nsCaret.h"
#include "nsContentList.h"
#include "nsError.h"
#include "nsQueryContentEventResult.h"
#include "nsGlobalWindow.h"
#include "nsFocusManager.h"
#include "nsFrameManager.h"
#include "nsRefreshDriver.h"
#include "nsStyleUtil.h"
#include "mozilla/dom/Animation.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/DOMCollectedFramesBinding.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/SharedStyleSheetCache.h"
#include "mozilla/StaticPrefs_test.h"
#include "mozilla/InputTaskManager.h"
#include "nsIObjectLoadingContent.h"
#include "nsIFrame.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/PCompositorBridgeTypes.h"
#include "nsQueryObject.h"
#include "CubebDeviceEnumerator.h"

#include "nsIScrollableFrame.h"

#include "nsContentUtils.h"

#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsCharsetSource.h"
#include "nsJSEnvironment.h"
#include "nsJSUtils.h"
#include "js/experimental/PCCountProfiling.h"  // JS::{Start,Stop}PCCountProfiling, JS::PurgePCCounts, JS::GetPCCountScript{Count,Summary,Contents}
#include "js/Object.h"                         // JS::GetClass

#include "mozilla/ChaosMode.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/Span.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TouchEvents.h"

#include "nsViewManager.h"

#include "nsLayoutUtils.h"
#include "nsComputedDOMStyle.h"
#include "nsCSSProps.h"
#include "nsIDocShell.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/DOMRect.h"
#include <algorithm>

#if defined(MOZ_X11) && defined(MOZ_WIDGET_GTK)
#  include <gdk/gdk.h>
#  include <gdk/gdkx.h>
#endif

#include "Layers.h"

#include "mozilla/dom/AudioDeviceInfo.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/IDBMutableFileBinding.h"
#include "mozilla/dom/IDBMutableFile.h"
#include "mozilla/dom/IndexedDatabaseManager.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/Text.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/layers/FrameUniformityData.h"
#include "nsPrintfCString.h"
#include "nsViewportInfo.h"
#include "nsIFormControl.h"
//#include "nsWidgetsCID.h"
#include "nsDisplayList.h"
#include "nsROCSSPrimitiveValue.h"
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestorUtils.h"
#include "mozilla/Preferences.h"
#include "nsContentPermissionHelper.h"
#include "nsCSSPseudoElements.h"  // for PseudoStyleType
#include "nsNetUtil.h"
#include "HTMLImageElement.h"
#include "HTMLCanvasElement.h"
#include "mozilla/css/ImageLoader.h"
#include "mozilla/layers/IAPZCTreeManager.h"  // for layers::ZoomToRectBehavior
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/dom/TimeoutManager.h"
#include "mozilla/PreloadedStyleSheet.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/DisplayPortUtils.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ViewportUtils.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/IMEContentObserver.h"

#ifdef XP_WIN
#  undef GetClassName
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla::gfx;

class gfxContext;

class OldWindowSize : public LinkedListElement<OldWindowSize> {
 public:
  static void Set(nsIWeakReference* aWindowRef, const nsSize& aSize) {
    OldWindowSize* item = GetItem(aWindowRef);
    if (item) {
      item->mSize = aSize;
    } else {
      item = new OldWindowSize(aWindowRef, aSize);
      sList.insertBack(item);
    }
  }

  static nsSize GetAndRemove(nsIWeakReference* aWindowRef) {
    nsSize result;
    if (OldWindowSize* item = GetItem(aWindowRef)) {
      result = item->mSize;
      delete item;
    }
    return result;
  }

 private:
  explicit OldWindowSize(nsIWeakReference* aWindowRef, const nsSize& aSize)
      : mWindowRef(aWindowRef), mSize(aSize) {}
  ~OldWindowSize() = default;
  ;

  static OldWindowSize* GetItem(nsIWeakReference* aWindowRef) {
    OldWindowSize* item = sList.getFirst();
    while (item && item->mWindowRef != aWindowRef) {
      item = item->getNext();
    }
    return item;
  }

  static LinkedList<OldWindowSize> sList;
  nsWeakPtr mWindowRef;
  nsSize mSize;
};

namespace {

class NativeInputRunnable final : public PrioritizableRunnable {
  explicit NativeInputRunnable(already_AddRefed<nsIRunnable>&& aEvent);
  ~NativeInputRunnable() = default;

 public:
  static already_AddRefed<nsIRunnable> Create(
      already_AddRefed<nsIRunnable>&& aEvent);
};

NativeInputRunnable::NativeInputRunnable(already_AddRefed<nsIRunnable>&& aEvent)
    : PrioritizableRunnable(std::move(aEvent),
                            nsIRunnablePriority::PRIORITY_INPUT_HIGH) {}

/* static */
already_AddRefed<nsIRunnable> NativeInputRunnable::Create(
    already_AddRefed<nsIRunnable>&& aEvent) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> event(new NativeInputRunnable(std::move(aEvent)));
  return event.forget();
}

}  // unnamed namespace

LinkedList<OldWindowSize> OldWindowSize::sList;

NS_INTERFACE_MAP_BEGIN(nsDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMWindowUtils)
NS_IMPL_RELEASE(nsDOMWindowUtils)

nsDOMWindowUtils::nsDOMWindowUtils(nsGlobalWindowOuter* aWindow) {
  nsCOMPtr<nsISupports> supports = do_QueryObject(aWindow);
  mWindow = do_GetWeakReference(supports);
}

nsDOMWindowUtils::~nsDOMWindowUtils() { OldWindowSize::GetAndRemove(mWindow); }

nsIDocShell* nsDOMWindowUtils::GetDocShell() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  if (!window) {
    return nullptr;
  }
  return window->GetDocShell();
}

PresShell* nsDOMWindowUtils::GetPresShell() {
  nsIDocShell* docShell = GetDocShell();
  if (!docShell) {
    return nullptr;
  }
  return docShell->GetPresShell();
}

nsPresContext* nsDOMWindowUtils::GetPresContext() {
  nsIDocShell* docShell = GetDocShell();
  if (!docShell) {
    return nullptr;
  }
  return docShell->GetPresContext();
}

Document* nsDOMWindowUtils::GetDocument() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  if (!window) {
    return nullptr;
  }
  return window->GetExtantDoc();
}

WebRenderBridgeChild* nsDOMWindowUtils::GetWebRenderBridge() {
  if (nsIWidget* widget = GetWidget()) {
    if (WindowRenderer* renderer = widget->GetWindowRenderer()) {
      if (WebRenderLayerManager* wr = renderer->AsWebRender()) {
        return wr->WrBridge();
      }
    }
  }
  return nullptr;
}

CompositorBridgeChild* nsDOMWindowUtils::GetCompositorBridge() {
  if (nsIWidget* widget = GetWidget()) {
    if (WindowRenderer* renderer = widget->GetWindowRenderer()) {
      if (CompositorBridgeChild* cbc = renderer->GetCompositorBridgeChild()) {
        return cbc;
      }
    }
  }
  return nullptr;
}

NS_IMETHODIMP
nsDOMWindowUtils::SyncFlushCompositor() {
  if (nsIWidget* widget = GetWidget()) {
    if (WindowRenderer* renderer = widget->GetWindowRenderer()) {
      if (KnowsCompositor* kc = renderer->AsKnowsCompositor()) {
        kc->SyncWithCompositor();
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetImageAnimationMode(uint16_t* aMode) {
  NS_ENSURE_ARG_POINTER(aMode);
  *aMode = 0;
  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    *aMode = presContext->ImageAnimationMode();
    return NS_OK;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetImageAnimationMode(uint16_t aMode) {
  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    presContext->SetImageAnimationMode(aMode);
    return NS_OK;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetDocCharsetIsForced(bool* aIsForced) {
  *aIsForced = false;

  Document* doc = GetDocument();
  if (doc) {
    auto source = doc->GetDocumentCharacterSetSource();
    *aIsForced = source == kCharsetFromInitialUserForcedAutoDetection ||
                 source == kCharsetFromFinalUserForcedAutoDetection;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPhysicalMillimeterInCSSPixels(float* aPhysicalMillimeter) {
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aPhysicalMillimeter = nsPresContext::AppUnitsToFloatCSSPixels(
      presContext->PhysicalMillimetersToAppUnits(1));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetDocumentMetadata(const nsAString& aName,
                                      nsAString& aValue) {
  Document* doc = GetDocument();
  if (doc) {
    RefPtr<nsAtom> name = NS_Atomize(aName);
    doc->GetHeaderData(name, aValue);
    return NS_OK;
  }

  aValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::UpdateLayerTree() {
  if (RefPtr<PresShell> presShell = GetPresShell()) {
    // Don't flush throttled animations since it might fire MozAfterPaint event
    // (in WebRender it constantly does), thus the reftest harness can't take
    // any snapshot until the throttled animations finished.
    presShell->FlushPendingNotifications(
        ChangesToFlush(FlushType::Display, false /* flush animations */));
    RefPtr<nsViewManager> vm = presShell->GetViewManager();
    if (nsView* view = vm->GetRootView()) {
      nsAutoScriptBlocker scriptBlocker;
      presShell->PaintAndRequestComposite(view,
                                          PaintFlags::PaintSyncDecodeImages);
      presShell->GetWindowRenderer()->WaitOnTransactionProcessed();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetContentViewerSize(uint32_t* aDisplayWidth,
                                       uint32_t* aDisplayHeight) {
  PresShell* presShell = GetPresShell();
  LayoutDeviceIntSize displaySize;

  if (!presShell || !nsLayoutUtils::GetContentViewerSize(
                        presShell->GetPresContext(), displaySize)) {
    return NS_ERROR_FAILURE;
  }

  *aDisplayWidth = displaySize.width;
  *aDisplayHeight = displaySize.height;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetViewportInfo(uint32_t aDisplayWidth,
                                  uint32_t aDisplayHeight, double* aDefaultZoom,
                                  bool* aAllowZoom, double* aMinZoom,
                                  double* aMaxZoom, uint32_t* aWidth,
                                  uint32_t* aHeight, bool* aAutoSize) {
  Document* doc = GetDocument();
  NS_ENSURE_STATE(doc);

  nsViewportInfo info =
      doc->GetViewportInfo(ScreenIntSize(aDisplayWidth, aDisplayHeight));
  *aDefaultZoom = info.GetDefaultZoom().scale;
  *aAllowZoom = info.IsZoomAllowed();
  *aMinZoom = info.GetMinZoom().scale;
  *aMaxZoom = info.GetMaxZoom().scale;
  CSSIntSize size = gfx::RoundedToInt(info.GetSize());
  *aWidth = size.width;
  *aHeight = size.height;
  *aAutoSize = info.IsAutoSizeEnabled();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetViewportFitInfo(nsAString& aViewportFit) {
  Document* doc = GetDocument();
  NS_ENSURE_STATE(doc);

  ViewportMetaData metaData = doc->GetViewportMetaData();
  if (metaData.mViewportFit.EqualsLiteral("contain")) {
    aViewportFit.AssignLiteral("contain");
  } else if (metaData.mViewportFit.EqualsLiteral("cover")) {
    aViewportFit.AssignLiteral("cover");
  } else {
    aViewportFit.AssignLiteral("auto");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetDisplayPortForElement(float aXPx, float aYPx,
                                           float aWidthPx, float aHeightPx,
                                           Element* aElement,
                                           uint32_t aPriority) {
  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aElement->GetUncomposedDoc() != presShell->GetDocument()) {
    return NS_ERROR_INVALID_ARG;
  }

  bool hadDisplayPort = false;
  bool wasPainted = false;
  nsRect oldDisplayPort;
  {
    DisplayPortPropertyData* currentData =
        static_cast<DisplayPortPropertyData*>(
            aElement->GetProperty(nsGkAtoms::DisplayPort));
    if (currentData) {
      if (currentData->mPriority > aPriority) {
        return NS_OK;
      }
      hadDisplayPort = true;
      oldDisplayPort = currentData->mRect;
      wasPainted = currentData->mPainted;
    }
  }

  nsRect displayport(nsPresContext::CSSPixelsToAppUnits(aXPx),
                     nsPresContext::CSSPixelsToAppUnits(aYPx),
                     nsPresContext::CSSPixelsToAppUnits(aWidthPx),
                     nsPresContext::CSSPixelsToAppUnits(aHeightPx));

  aElement->RemoveProperty(nsGkAtoms::MinimalDisplayPort);
  aElement->SetProperty(
      nsGkAtoms::DisplayPort,
      new DisplayPortPropertyData(displayport, aPriority, wasPainted),
      nsINode::DeleteProperty<DisplayPortPropertyData>);

  DisplayPortUtils::InvalidateForDisplayPortChange(aElement, hadDisplayPort,
                                                   oldDisplayPort, displayport);

  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (rootFrame) {
    rootFrame->SchedulePaint();

    // If we are hiding something that is a display root then send empty paint
    // transaction in order to release retained layers because it won't get
    // any more paint requests when it is hidden.
    if (displayport.IsEmpty() &&
        rootFrame == nsLayoutUtils::GetDisplayRootFrame(rootFrame)) {
      nsCOMPtr<nsIWidget> widget = GetWidget();
      if (widget) {
        using PaintFrameFlags = nsLayoutUtils::PaintFrameFlags;
        nsLayoutUtils::PaintFrame(
            nullptr, rootFrame, nsRegion(), NS_RGB(255, 255, 255),
            nsDisplayListBuilderMode::Painting, PaintFrameFlags::WidgetLayers);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetDisplayPortMarginsForElement(
    float aLeftMargin, float aTopMargin, float aRightMargin,
    float aBottomMargin, Element* aElement, uint32_t aPriority) {
  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aElement->GetUncomposedDoc() != presShell->GetDocument()) {
    return NS_ERROR_INVALID_ARG;
  }

  // Note order change of arguments between our function signature and
  // ScreenMargin constructor.
  ScreenMargin displayportMargins(aTopMargin, aRightMargin, aBottomMargin,
                                  aLeftMargin);

  DisplayPortUtils::SetDisplayPortMargins(
      aElement, presShell,
      DisplayPortMargins::ForContent(aElement, displayportMargins),
      DisplayPortUtils::ClearMinimalDisplayPortProperty::Yes, aPriority);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetDisplayPortBaseForElement(int32_t aX, int32_t aY,
                                               int32_t aWidth, int32_t aHeight,
                                               Element* aElement) {
  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aElement->GetUncomposedDoc() != presShell->GetDocument()) {
    return NS_ERROR_INVALID_ARG;
  }

  DisplayPortUtils::SetDisplayPortBase(aElement,
                                       nsRect(aX, aY, aWidth, aHeight));

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScrollbarSizes(Element* aElement,
                                    uint32_t* aOutVerticalScrollbarWidth,
                                    uint32_t* aOutHorizontalScrollbarHeight) {
  nsIScrollableFrame* scrollFrame =
      nsLayoutUtils::FindScrollableFrameFor(aElement);
  if (!scrollFrame) {
    return NS_ERROR_INVALID_ARG;
  }

  CSSIntMargin scrollbarSizes =
      RoundedToInt(CSSMargin::FromAppUnits(scrollFrame->GetActualScrollbarSizes(
          nsIScrollableFrame::ScrollbarSizesOptions::
              INCLUDE_VISUAL_VIEWPORT_SCROLLBARS)));
  *aOutVerticalScrollbarWidth = scrollbarSizes.LeftRight();
  *aOutHorizontalScrollbarHeight = scrollbarSizes.TopBottom();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetResolutionAndScaleTo(float aResolution) {
  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  presShell->SetResolutionAndScaleTo(aResolution, ResolutionChangeOrigin::Test);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetRestoreResolution(float aResolution,
                                       uint32_t aDisplayWidth,
                                       uint32_t aDisplayHeight) {
  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  presShell->SetRestoreResolution(
      aResolution, LayoutDeviceIntSize(aDisplayWidth, aDisplayHeight));

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetResolution(float* aResolution) {
  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  *aResolution = presShell->GetResolution();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetIsFirstPaint(bool aIsFirstPaint) {
  if (PresShell* presShell = GetPresShell()) {
    presShell->SetIsFirstPaint(aIsFirstPaint);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsFirstPaint(bool* aIsFirstPaint) {
  if (PresShell* presShell = GetPresShell()) {
    *aIsFirstPaint = presShell->GetIsFirstPaint();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPresShellId(uint32_t* aPresShellId) {
  if (PresShell* presShell = GetPresShell()) {
    *aPresShellId = presShell->GetPresShellId();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendMouseEvent(
    const nsAString& aType, float aX, float aY, int32_t aButton,
    int32_t aClickCount, int32_t aModifiers, bool aIgnoreRootScrollFrame,
    float aPressure, unsigned short aInputSourceArg,
    bool aIsDOMEventSynthesized, bool aIsWidgetEventSynthesized,
    int32_t aButtons, uint32_t aIdentifier, uint8_t aOptionalArgCount,
    bool* aPreventDefault) {
  return SendMouseEventCommon(
      aType, aX, aY, aButton, aClickCount, aModifiers, aIgnoreRootScrollFrame,
      aPressure, aInputSourceArg,
      aOptionalArgCount >= 7 ? aIdentifier : DEFAULT_MOUSE_POINTER_ID, false,
      aPreventDefault, aOptionalArgCount >= 4 ? aIsDOMEventSynthesized : true,
      aOptionalArgCount >= 5 ? aIsWidgetEventSynthesized : false,
      aOptionalArgCount >= 6 ? aButtons : MOUSE_BUTTONS_NOT_SPECIFIED);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendMouseEventToWindow(
    const nsAString& aType, float aX, float aY, int32_t aButton,
    int32_t aClickCount, int32_t aModifiers, bool aIgnoreRootScrollFrame,
    float aPressure, unsigned short aInputSourceArg,
    bool aIsDOMEventSynthesized, bool aIsWidgetEventSynthesized,
    int32_t aButtons, uint32_t aIdentifier, uint8_t aOptionalArgCount) {
  AUTO_PROFILER_LABEL("nsDOMWindowUtils::SendMouseEventToWindow", OTHER);

  return SendMouseEventCommon(
      aType, aX, aY, aButton, aClickCount, aModifiers, aIgnoreRootScrollFrame,
      aPressure, aInputSourceArg,
      aOptionalArgCount >= 7 ? aIdentifier : DEFAULT_MOUSE_POINTER_ID, true,
      nullptr, aOptionalArgCount >= 4 ? aIsDOMEventSynthesized : true,
      aOptionalArgCount >= 5 ? aIsWidgetEventSynthesized : false,
      aOptionalArgCount >= 6 ? aButtons : MOUSE_BUTTONS_NOT_SPECIFIED);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendMouseEventCommon(
    const nsAString& aType, float aX, float aY, int32_t aButton,
    int32_t aClickCount, int32_t aModifiers, bool aIgnoreRootScrollFrame,
    float aPressure, unsigned short aInputSourceArg, uint32_t aPointerId,
    bool aToWindow, bool* aPreventDefault, bool aIsDOMEventSynthesized,
    bool aIsWidgetEventSynthesized, int32_t aButtons) {
  RefPtr<PresShell> presShell = GetPresShell();
  PreventDefaultResult preventDefaultResult;
  nsresult rv = nsContentUtils::SendMouseEvent(
      presShell, aType, aX, aY, aButton, aButtons, aClickCount, aModifiers,
      aIgnoreRootScrollFrame, aPressure, aInputSourceArg, aPointerId, aToWindow,
      &preventDefaultResult, aIsDOMEventSynthesized, aIsWidgetEventSynthesized);

  if (aPreventDefault) {
    *aPreventDefault = preventDefaultResult != PreventDefaultResult::No;
  }

  return rv;
}

NS_IMETHODIMP
nsDOMWindowUtils::IsCORSSafelistedRequestHeader(const nsACString& aName,
                                                const nsACString& aValue,
                                                bool* aRetVal) {
  NS_ENSURE_ARG_POINTER(aRetVal);
  *aRetVal = nsContentUtils::IsCORSSafelistedRequestHeader(aName, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendWheelEvent(float aX, float aY, double aDeltaX,
                                 double aDeltaY, double aDeltaZ,
                                 uint32_t aDeltaMode, int32_t aModifiers,
                                 int32_t aLineOrPageDeltaX,
                                 int32_t aLineOrPageDeltaY, uint32_t aOptions) {
  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget) {
    return NS_ERROR_NULL_POINTER;
  }

  WidgetWheelEvent wheelEvent(true, eWheel, widget);
  wheelEvent.mModifiers = nsContentUtils::GetWidgetModifiers(aModifiers);
  wheelEvent.mDeltaX = aDeltaX;
  wheelEvent.mDeltaY = aDeltaY;
  wheelEvent.mDeltaZ = aDeltaZ;
  wheelEvent.mDeltaMode = aDeltaMode;
  wheelEvent.mIsMomentum = (aOptions & WHEEL_EVENT_CAUSED_BY_MOMENTUM) != 0;
  wheelEvent.mIsNoLineOrPageDelta =
      (aOptions & WHEEL_EVENT_CAUSED_BY_NO_LINE_OR_PAGE_DELTA_DEVICE) != 0;
  wheelEvent.mCustomizedByUserPrefs =
      (aOptions & WHEEL_EVENT_CUSTOMIZED_BY_USER_PREFS) != 0;
  wheelEvent.mLineOrPageDeltaX = aLineOrPageDeltaX;
  wheelEvent.mLineOrPageDeltaY = aLineOrPageDeltaY;

  wheelEvent.mTime = PR_Now() / 1000;

  nsPresContext* presContext = GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  wheelEvent.mRefPoint =
      nsContentUtils::ToWidgetPoint(CSSPoint(aX, aY), offset, presContext);

  widget->DispatchInputEvent(&wheelEvent);

  if (widget->AsyncPanZoomEnabled()) {
    // Computing overflow deltas is not compatible with APZ, so if APZ is
    // enabled, we skip testing it.
    return NS_OK;
  }

  bool failedX = false;
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_ZERO) &&
      wheelEvent.mOverflowDeltaX != 0) {
    failedX = true;
  }
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_POSITIVE) &&
      wheelEvent.mOverflowDeltaX <= 0) {
    failedX = true;
  }
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_NEGATIVE) &&
      wheelEvent.mOverflowDeltaX >= 0) {
    failedX = true;
  }
  bool failedY = false;
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_ZERO) &&
      wheelEvent.mOverflowDeltaY != 0) {
    failedY = true;
  }
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_POSITIVE) &&
      wheelEvent.mOverflowDeltaY <= 0) {
    failedY = true;
  }
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_NEGATIVE) &&
      wheelEvent.mOverflowDeltaY >= 0) {
    failedY = true;
  }

#ifdef DEBUG
  if (failedX) {
    nsPrintfCString debugMsg("SendWheelEvent(): unexpected mOverflowDeltaX: %f",
                             wheelEvent.mOverflowDeltaX);
    NS_WARNING(debugMsg.get());
  }
  if (failedY) {
    nsPrintfCString debugMsg("SendWheelEvent(): unexpected mOverflowDeltaY: %f",
                             wheelEvent.mOverflowDeltaY);
    NS_WARNING(debugMsg.get());
  }
#endif

  return (!failedX && !failedY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendTouchEvent(
    const nsAString& aType, const nsTArray<uint32_t>& aIdentifiers,
    const nsTArray<int32_t>& aXs, const nsTArray<int32_t>& aYs,
    const nsTArray<uint32_t>& aRxs, const nsTArray<uint32_t>& aRys,
    const nsTArray<float>& aRotationAngles, const nsTArray<float>& aForces,
    int32_t aModifiers, bool aIgnoreRootScrollFrame, bool* aPreventDefault) {
  return SendTouchEventCommon(aType, aIdentifiers, aXs, aYs, aRxs, aRys,
                              aRotationAngles, aForces, aModifiers,
                              aIgnoreRootScrollFrame, false, aPreventDefault);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendTouchEventToWindow(
    const nsAString& aType, const nsTArray<uint32_t>& aIdentifiers,
    const nsTArray<int32_t>& aXs, const nsTArray<int32_t>& aYs,
    const nsTArray<uint32_t>& aRxs, const nsTArray<uint32_t>& aRys,
    const nsTArray<float>& aRotationAngles, const nsTArray<float>& aForces,
    int32_t aModifiers, bool aIgnoreRootScrollFrame, bool* aPreventDefault) {
  return SendTouchEventCommon(aType, aIdentifiers, aXs, aYs, aRxs, aRys,
                              aRotationAngles, aForces, aModifiers,
                              aIgnoreRootScrollFrame, true, aPreventDefault);
}

nsresult nsDOMWindowUtils::SendTouchEventCommon(
    const nsAString& aType, const nsTArray<uint32_t>& aIdentifiers,
    const nsTArray<int32_t>& aXs, const nsTArray<int32_t>& aYs,
    const nsTArray<uint32_t>& aRxs, const nsTArray<uint32_t>& aRys,
    const nsTArray<float>& aRotationAngles, const nsTArray<float>& aForces,
    int32_t aModifiers, bool aIgnoreRootScrollFrame, bool aToWindow,
    bool* aPreventDefault) {
  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget) {
    return NS_ERROR_NULL_POINTER;
  }
  EventMessage msg;
  if (aType.EqualsLiteral("touchstart")) {
    msg = eTouchStart;
  } else if (aType.EqualsLiteral("touchmove")) {
    msg = eTouchMove;
  } else if (aType.EqualsLiteral("touchend")) {
    msg = eTouchEnd;
  } else if (aType.EqualsLiteral("touchcancel")) {
    msg = eTouchCancel;
  } else {
    return NS_ERROR_UNEXPECTED;
  }
  WidgetTouchEvent event(true, msg, widget);
  event.mModifiers = nsContentUtils::GetWidgetModifiers(aModifiers);
  event.mTime = PR_Now();

  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_ERROR_FAILURE;
  }
  uint32_t count = aIdentifiers.Length();
  if (aXs.Length() != count || aYs.Length() != count ||
      aRxs.Length() != count || aRys.Length() != count ||
      aRotationAngles.Length() != count || aForces.Length() != count) {
    return NS_ERROR_INVALID_ARG;
  }
  event.mTouches.SetCapacity(count);
  for (uint32_t i = 0; i < count; ++i) {
    LayoutDeviceIntPoint pt = nsContentUtils::ToWidgetPoint(
        CSSPoint(aXs[i], aYs[i]), offset, presContext);
    LayoutDeviceIntPoint radius = LayoutDeviceIntPoint::FromAppUnitsRounded(
        CSSPoint::ToAppUnits(CSSPoint(aRxs[i], aRys[i])),
        presContext->AppUnitsPerDevPixel());

    RefPtr<Touch> t =
        new Touch(aIdentifiers[i], pt, radius, aRotationAngles[i], aForces[i]);

    event.mTouches.AppendElement(t);
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  if (aToWindow) {
    RefPtr<PresShell> presShell;
    nsView* view = nsContentUtils::GetViewToDispatchEvent(
        presContext, getter_AddRefs(presShell));
    if (!presShell || !view) {
      return NS_ERROR_FAILURE;
    }
    *aPreventDefault = (status == nsEventStatus_eConsumeNoDefault);
    return presShell->HandleEvent(view->GetFrame(), &event, false, &status);
  }

  if (StaticPrefs::test_events_async_enabled()) {
    status = widget->DispatchInputEvent(&event).mContentStatus;
  } else {
    nsresult rv = widget->DispatchEvent(&event, status);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aPreventDefault) {
    *aPreventDefault = (status == nsEventStatus_eConsumeNoDefault);
  }
  return NS_OK;
}

static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::CAPS_LOCK) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_CAPS_LOCK),
    "Need to sync CapsLock value between nsIWidget::Modifiers and "
    "nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::NUM_LOCK) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_NUM_LOCK),
    "Need to sync NumLock value between nsIWidget::Modifiers and "
    "nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::SHIFT_L) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_SHIFT_LEFT),
    "Need to sync ShiftLeft value between nsIWidget::Modifiers and "
    "nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::SHIFT_R) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_SHIFT_RIGHT),
    "Need to sync ShiftRight value between nsIWidget::Modifiers and "
    "nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::CTRL_L) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_CONTROL_LEFT),
    "Need to sync ControlLeft value between nsIWidget::Modifiers and "
    "nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::CTRL_R) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_CONTROL_RIGHT),
    "Need to sync ControlRight value between nsIWidget::Modifiers "
    "and nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::ALT_L) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_ALT_LEFT),
    "Need to sync AltLeft value between nsIWidget::Modifiers and "
    "nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::ALT_R) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_ALT_RIGHT),
    "Need to sync AltRight value between nsIWidget::Modifiers and "
    "nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::COMMAND_L) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_COMMAND_LEFT),
    "Need to sync CommandLeft value between nsIWidget::Modifiers and "
    "nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::COMMAND_R) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_COMMAND_RIGHT),
    "Need to sync CommandRight value between nsIWidget::Modifiers "
    "and nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::HELP) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_HELP),
    "Need to sync Help value between nsIWidget::Modifiers and "
    "nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::ALTGRAPH) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_ALT_GRAPH),
    "Need to sync AltGraph value between nsIWidget::Modifiers and "
    "nsIDOMWindowUtils");
static_assert(
    static_cast<uint32_t>(nsIWidget::Modifiers::FUNCTION) ==
        static_cast<uint32_t>(nsIDOMWindowUtils::NATIVE_MODIFIER_FUNCTION),
    "Need to sync Function value between nsIWidget::Modifiers and "
    "nsIDOMWindowUtils");
static_assert(static_cast<uint32_t>(nsIWidget::Modifiers::NUMERIC_KEY_PAD) ==
                  static_cast<uint32_t>(
                      nsIDOMWindowUtils::NATIVE_MODIFIER_NUMERIC_KEY_PAD),
              "Need to sync NumericKeyPad value between nsIWidget::Modifiers "
              "and nsIDOMWindowUtils");

static nsIWidget::Modifiers GetWidgetModifiers(uint32_t aNativeModifiers) {
  nsIWidget::Modifiers widgetModifiers = static_cast<nsIWidget::Modifiers>(
      aNativeModifiers &
      (nsIWidget::Modifiers::CAPS_LOCK | nsIWidget::Modifiers::NUM_LOCK |
       nsIWidget::Modifiers::SHIFT_L | nsIWidget::Modifiers::SHIFT_R |
       nsIWidget::Modifiers::CTRL_L | nsIWidget::Modifiers::CTRL_R |
       nsIWidget::Modifiers::ALT_L | nsIWidget::Modifiers::ALT_R |
       nsIWidget::Modifiers::COMMAND_L | nsIWidget::Modifiers::COMMAND_R |
       nsIWidget::Modifiers::HELP | nsIWidget::Modifiers::ALTGRAPH |
       nsIWidget::Modifiers::FUNCTION | nsIWidget::Modifiers::NUMERIC_KEY_PAD));
  NS_ASSERTION(static_cast<uint32_t>(widgetModifiers) == aNativeModifiers,
               "Invalid value is specified to the native modifiers");
  return widgetModifiers;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                     int32_t aNativeKeyCode,
                                     uint32_t aModifiers,
                                     const nsAString& aCharacters,
                                     const nsAString& aUnmodifiedCharacters,
                                     nsIObserver* aObserver) {
  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) return NS_ERROR_FAILURE;

  NS_DispatchToMainThread(NativeInputRunnable::Create(
      NewRunnableMethod<int32_t, int32_t, uint32_t, nsString, nsString,
                        nsIObserver*>(
          "nsIWidget::SynthesizeNativeKeyEvent", widget,
          &nsIWidget::SynthesizeNativeKeyEvent, aNativeKeyboardLayout,
          aNativeKeyCode, static_cast<uint32_t>(GetWidgetModifiers(aModifiers)),
          aCharacters, aUnmodifiedCharacters, aObserver)));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeMouseEvent(int32_t aScreenX, int32_t aScreenY,
                                       uint32_t aNativeMessage, int16_t aButton,
                                       uint32_t aModifierFlags,
                                       Element* aElementOnWidget,
                                       nsIObserver* aObserver) {
  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidgetForElement(aElementOnWidget);
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  nsIWidget::NativeMouseMessage message;
  switch (aNativeMessage) {
    case NATIVE_MOUSE_MESSAGE_BUTTON_DOWN:
      message = nsIWidget::NativeMouseMessage::ButtonDown;
      break;
    case NATIVE_MOUSE_MESSAGE_BUTTON_UP:
      message = nsIWidget::NativeMouseMessage::ButtonUp;
      break;
    case NATIVE_MOUSE_MESSAGE_MOVE:
      message = nsIWidget::NativeMouseMessage::Move;
      break;
    case NATIVE_MOUSE_MESSAGE_ENTER_WINDOW:
      message = nsIWidget::NativeMouseMessage::EnterWindow;
      break;
    case NATIVE_MOUSE_MESSAGE_LEAVE_WINDOW:
      message = nsIWidget::NativeMouseMessage::LeaveWindow;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  NS_DispatchToMainThread(NativeInputRunnable::Create(
      NewRunnableMethod<LayoutDeviceIntPoint, nsIWidget::NativeMouseMessage,
                        MouseButton, nsIWidget::Modifiers, nsIObserver*>(
          "nsIWidget::SynthesizeNativeMouseEvent", widget,
          &nsIWidget::SynthesizeNativeMouseEvent,
          LayoutDeviceIntPoint(aScreenX, aScreenY), message,
          static_cast<MouseButton>(aButton), GetWidgetModifiers(aModifierFlags),
          aObserver)));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeMouseScrollEvent(
    int32_t aScreenX, int32_t aScreenY, uint32_t aNativeMessage, double aDeltaX,
    double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
    uint32_t aAdditionalFlags, Element* aElement, nsIObserver* aObserver) {
  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidgetForElement(aElement);
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  NS_DispatchToMainThread(NativeInputRunnable::Create(
      NewRunnableMethod<mozilla::LayoutDeviceIntPoint, uint32_t, double, double,
                        double, uint32_t, uint32_t, nsIObserver*>(
          "nsIWidget::SynthesizeNativeMouseScrollEvent", widget,
          &nsIWidget::SynthesizeNativeMouseScrollEvent,
          LayoutDeviceIntPoint(aScreenX, aScreenY), aNativeMessage, aDeltaX,
          aDeltaY, aDeltaZ, aModifierFlags, aAdditionalFlags, aObserver)));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeTouchPoint(uint32_t aPointerId,
                                       uint32_t aTouchState, int32_t aScreenX,
                                       int32_t aScreenY, double aPressure,
                                       uint32_t aOrientation,
                                       nsIObserver* aObserver) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  if (aPressure < 0 || aPressure > 1 || aOrientation > 359) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_DispatchToMainThread(NativeInputRunnable::Create(
      NewRunnableMethod<uint32_t, nsIWidget::TouchPointerState,
                        LayoutDeviceIntPoint, double, uint32_t, nsIObserver*>(
          "nsIWidget::SynthesizeNativeTouchPoint", widget,
          &nsIWidget::SynthesizeNativeTouchPoint, aPointerId,
          (nsIWidget::TouchPointerState)aTouchState,
          LayoutDeviceIntPoint(aScreenX, aScreenY), aPressure, aOrientation,
          aObserver)));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeTouchpadPinch(uint32_t aEventPhase, float aScale,
                                          int32_t aScreenX, int32_t aScreenY,
                                          int32_t aModifierFlags) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }
  NS_DispatchToMainThread(NativeInputRunnable::Create(
      NewRunnableMethod<nsIWidget::TouchpadGesturePhase, float,
                        LayoutDeviceIntPoint, int32_t>(
          "nsIWidget::SynthesizeNativeTouchPadPinch", widget,
          &nsIWidget::SynthesizeNativeTouchPadPinch,
          (nsIWidget::TouchpadGesturePhase)aEventPhase, aScale,
          LayoutDeviceIntPoint(aScreenX, aScreenY), aModifierFlags)));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeTouchTap(int32_t aScreenX, int32_t aScreenY,
                                     bool aLongTap, nsIObserver* aObserver) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  NS_DispatchToMainThread(NativeInputRunnable::Create(
      NewRunnableMethod<LayoutDeviceIntPoint, bool, nsIObserver*>(
          "nsIWidget::SynthesizeNativeTouchTap", widget,
          &nsIWidget::SynthesizeNativeTouchTap,
          LayoutDeviceIntPoint(aScreenX, aScreenY), aLongTap, aObserver)));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativePenInput(uint32_t aPointerId,
                                     uint32_t aPointerState, int32_t aScreenX,
                                     int32_t aScreenY, double aPressure,
                                     uint32_t aRotation, int32_t aTiltX,
                                     int32_t aTiltY, int32_t aButton,
                                     nsIObserver* aObserver) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  if (aPressure < 0 || aPressure > 1 || aRotation > 359 || aTiltX < -90 ||
      aTiltX > 90 || aTiltY < -90 || aTiltY > 90) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_DispatchToMainThread(NativeInputRunnable::Create(
      NewRunnableMethod<uint32_t, nsIWidget::TouchPointerState,
                        LayoutDeviceIntPoint, double, uint32_t, int32_t,
                        int32_t, int32_t, nsIObserver*>(
          "nsIWidget::SynthesizeNativePenInput", widget,
          &nsIWidget::SynthesizeNativePenInput, aPointerId,
          (nsIWidget::TouchPointerState)aPointerState,
          LayoutDeviceIntPoint(aScreenX, aScreenY), aPressure, aRotation,
          aTiltX, aTiltY, aButton, aObserver)));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeTouchpadDoubleTap(int32_t aScreenX,
                                              int32_t aScreenY,
                                              int32_t aModifierFlags) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(aModifierFlags >= 0);
  NS_DispatchToMainThread(NativeInputRunnable::Create(
      NewRunnableMethod<LayoutDeviceIntPoint, uint32_t>(
          "nsIWidget::SynthesizeNativeTouchpadDoubleTap", widget,
          &nsIWidget::SynthesizeNativeTouchpadDoubleTap,
          LayoutDeviceIntPoint(aScreenX, aScreenY), aModifierFlags)));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeTouchpadPan(uint32_t aEventPhase, int32_t aScreenX,
                                        int32_t aScreenY, double aDeltaX,
                                        double aDeltaY,
                                        int32_t aModifierFlags) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(aModifierFlags >= 0);
  NS_DispatchToMainThread(NativeInputRunnable::Create(
      NewRunnableMethod<nsIWidget::TouchpadGesturePhase, LayoutDeviceIntPoint,
                        double, double, uint32_t>(
          "nsIWidget::SynthesizeNativeTouchpadPan", widget,
          &nsIWidget::SynthesizeNativeTouchpadPan,
          (nsIWidget::TouchpadGesturePhase)aEventPhase,
          LayoutDeviceIntPoint(aScreenX, aScreenY), aDeltaX, aDeltaY,
          aModifierFlags)));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SuppressAnimation(bool aSuppress) {
  nsIWidget* widget = GetWidget();
  if (widget) {
    widget->SuppressAnimation(aSuppress);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ClearSharedStyleSheetCache() {
  SharedStyleSheetCache::Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetParsedStyleSheets(uint32_t* aSheets) {
  RefPtr<Document> doc = GetDocument();
  if (!doc) {
    return NS_ERROR_UNEXPECTED;
  }

  *aSheets = doc->CSSLoader()->ParsedSheetCount();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ClearNativeTouchSequence(nsIObserver* aObserver) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  NS_DispatchToMainThread(
      NativeInputRunnable::Create(NewRunnableMethod<nsIObserver*>(
          "nsIWidget::ClearNativeTouchSequence", widget,
          &nsIWidget::ClearNativeTouchSequence, aObserver)));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ActivateNativeMenuItemAt(const nsAString& indexString) {
  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) return NS_ERROR_FAILURE;

  return widget->ActivateNativeMenuItemAt(indexString);
}

NS_IMETHODIMP
nsDOMWindowUtils::ForceUpdateNativeMenuAt(const nsAString& indexString) {
  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) return NS_ERROR_FAILURE;

  return widget->ForceUpdateNativeMenuAt(indexString);
}

NS_IMETHODIMP
nsDOMWindowUtils::GetSelectionAsPlaintext(nsAString& aResult) {
  // Get the widget to send the event to.
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  return widget->GetSelectionAsPlaintext(aResult);
}

nsIWidget* nsDOMWindowUtils::GetWidget(nsPoint* aOffset) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  if (window) {
    nsIDocShell* docShell = window->GetDocShell();
    if (docShell) {
      return nsContentUtils::GetWidget(docShell->GetPresShell(), aOffset);
    }
  }

  return nullptr;
}

nsIWidget* nsDOMWindowUtils::GetWidgetForElement(Element* aElement) {
  if (!aElement) {
    return GetWidget();
  }
  if (Document* doc = aElement->GetUncomposedDoc()) {
    if (PresShell* presShell = doc->GetPresShell()) {
      nsIFrame* frame = aElement->GetPrimaryFrame();
      if (!frame) {
        frame = presShell->GetRootFrame();
      }
      if (frame) {
        return frame->GetNearestWidget();
      }
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsDOMWindowUtils::GarbageCollect(nsICycleCollectorListener* aListener) {
  AUTO_PROFILER_LABEL("nsDOMWindowUtils::GarbageCollect", GCCC);

  nsJSContext::GarbageCollectNow(JS::GCReason::DOM_UTILS);
  nsJSContext::CycleCollectNow(CCReason::API, aListener);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::CycleCollect(nsICycleCollectorListener* aListener) {
  nsJSContext::CycleCollectNow(CCReason::API, aListener);
  return NS_OK;
}

static bool ParseGCReason(const nsACString& aStr, JS::GCReason* aReason,
                          JS::GCReason aDefault) {
  if (aStr.IsEmpty()) {
    *aReason = aDefault;
    return true;
  }
#define CHECK_REASON(name, _)         \
  if (aStr.EqualsIgnoreCase(#name)) { \
    *aReason = JS::GCReason::name;    \
    return true;                      \
  }
  GCREASONS(CHECK_REASON);
  return false;
}

NS_IMETHODIMP
nsDOMWindowUtils::RunNextCollectorTimer(const nsACString& aReason) {
  JS::GCReason reason;
  if (!ParseGCReason(aReason, &reason, JS::GCReason::DOM_WINDOW_UTILS)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsJSContext::RunNextCollectorTimer(reason);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::PokeGC(const nsACString& aReason) {
  JS::GCReason reason;
  if (!ParseGCReason(aReason, &reason, JS::GCReason::DOM_WINDOW_UTILS)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsJSContext::PokeGC(reason, nullptr);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendSimpleGestureEvent(const nsAString& aType, float aX,
                                         float aY, uint32_t aDirection,
                                         double aDelta, int32_t aModifiers,
                                         uint32_t aClickCount) {
  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget) return NS_ERROR_FAILURE;

  EventMessage msg;
  if (aType.EqualsLiteral("MozSwipeGestureMayStart")) {
    msg = eSwipeGestureMayStart;
  } else if (aType.EqualsLiteral("MozSwipeGestureStart")) {
    msg = eSwipeGestureStart;
  } else if (aType.EqualsLiteral("MozSwipeGestureUpdate")) {
    msg = eSwipeGestureUpdate;
  } else if (aType.EqualsLiteral("MozSwipeGestureEnd")) {
    msg = eSwipeGestureEnd;
  } else if (aType.EqualsLiteral("MozSwipeGesture")) {
    msg = eSwipeGesture;
  } else if (aType.EqualsLiteral("MozMagnifyGestureStart")) {
    msg = eMagnifyGestureStart;
  } else if (aType.EqualsLiteral("MozMagnifyGestureUpdate")) {
    msg = eMagnifyGestureUpdate;
  } else if (aType.EqualsLiteral("MozMagnifyGesture")) {
    msg = eMagnifyGesture;
  } else if (aType.EqualsLiteral("MozRotateGestureStart")) {
    msg = eRotateGestureStart;
  } else if (aType.EqualsLiteral("MozRotateGestureUpdate")) {
    msg = eRotateGestureUpdate;
  } else if (aType.EqualsLiteral("MozRotateGesture")) {
    msg = eRotateGesture;
  } else if (aType.EqualsLiteral("MozTapGesture")) {
    msg = eTapGesture;
  } else if (aType.EqualsLiteral("MozPressTapGesture")) {
    msg = ePressTapGesture;
  } else if (aType.EqualsLiteral("MozEdgeUIStarted")) {
    msg = eEdgeUIStarted;
  } else if (aType.EqualsLiteral("MozEdgeUICanceled")) {
    msg = eEdgeUICanceled;
  } else if (aType.EqualsLiteral("MozEdgeUICompleted")) {
    msg = eEdgeUICompleted;
  } else {
    return NS_ERROR_FAILURE;
  }

  WidgetSimpleGestureEvent event(true, msg, widget);
  event.mModifiers = nsContentUtils::GetWidgetModifiers(aModifiers);
  event.mDirection = aDirection;
  event.mDelta = aDelta;
  event.mClickCount = aClickCount;
  event.mTime = PR_IntervalNow();

  nsPresContext* presContext = GetPresContext();
  if (!presContext) return NS_ERROR_FAILURE;

  event.mRefPoint =
      nsContentUtils::ToWidgetPoint(CSSPoint(aX, aY), offset, presContext);

  nsEventStatus status;
  return widget->DispatchEvent(&event, status);
}

NS_IMETHODIMP
nsDOMWindowUtils::ElementFromPoint(float aX, float aY,
                                   bool aIgnoreRootScrollFrame,
                                   bool aFlushLayout, Element** aReturn) {
  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  RefPtr<Element> el = doc->ElementFromPointHelper(
      aX, aY, aIgnoreRootScrollFrame, aFlushLayout, ViewportType::Layout);
  el.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::NodesFromRect(float aX, float aY, float aTopSize,
                                float aRightSize, float aBottomSize,
                                float aLeftSize, bool aIgnoreRootScrollFrame,
                                bool aFlushLayout, bool aOnlyVisible,
                                float aVisibleThreshold,
                                nsINodeList** aReturn) {
  RefPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  auto list = MakeRefPtr<nsSimpleContentList>(doc);

  // The visible threshold was omitted or given a zero value (which makes no
  // sense), so give a reasonable default.
  if (aVisibleThreshold == 0.0f) {
    aVisibleThreshold = 1.0f;
  }

  AutoTArray<RefPtr<nsINode>, 8> nodes;
  doc->NodesFromRect(aX, aY, aTopSize, aRightSize, aBottomSize, aLeftSize,
                     aIgnoreRootScrollFrame, aFlushLayout, aOnlyVisible,
                     aVisibleThreshold, nodes);
  list->SetCapacity(nodes.Length());
  for (auto& node : nodes) {
    list->AppendElement(node->AsContent());
  }

  list.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetTranslationNodes(nsINode* aRoot,
                                      nsITranslationNodeList** aRetVal) {
  NS_ENSURE_ARG_POINTER(aRetVal);
  nsCOMPtr<nsIContent> root = do_QueryInterface(aRoot);
  NS_ENSURE_STATE(root);
  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  if (root->OwnerDoc() != doc) {
    return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
  }

  nsTHashSet<nsIContent*> translationNodesHash(500);
  RefPtr<nsTranslationNodeList> list = new nsTranslationNodeList;

  uint32_t limit = 15000;

  // We begin iteration with content->GetNextNode because we want to explictly
  // skip the root tag from being a translation node.
  nsIContent* content = root;
  while ((limit > 0) && (content = content->GetNextNode(root))) {
    if (!content->IsHTMLElement()) {
      continue;
    }

    // Skip elements that usually contain non-translatable text content.
    if (content->IsAnyOfHTMLElements(nsGkAtoms::script, nsGkAtoms::iframe,
                                     nsGkAtoms::frameset, nsGkAtoms::frame,
                                     nsGkAtoms::code, nsGkAtoms::noscript,
                                     nsGkAtoms::style)) {
      continue;
    }

    // An element is a translation node if it contains
    // at least one text node that has meaningful data
    // for translation
    for (nsIContent* child = content->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (child->IsText() && child->GetAsText()->HasTextForTranslation()) {
        translationNodesHash.Insert(content);

        nsIFrame* frame = content->GetPrimaryFrame();
        bool isTranslationRoot = frame && frame->IsBlockFrameOrSubclass();
        if (!isTranslationRoot) {
          // If an element is not a block element, it still
          // can be considered a translation root if the parent
          // of this element didn't make into the list of nodes
          // to be translated.
          bool parentInList = false;
          nsIContent* parent = content->GetParent();
          if (parent) {
            parentInList = translationNodesHash.Contains(parent);
          }
          isTranslationRoot = !parentInList;
        }

        list->AppendElement(content, isTranslationRoot);
        --limit;
        break;
      }
    }
  }

  *aRetVal = list.forget().take();
  return NS_OK;
}

static already_AddRefed<DataSourceSurface> CanvasToDataSourceSurface(
    HTMLCanvasElement* aCanvas) {
  MOZ_ASSERT(aCanvas);
  SurfaceFromElementResult result = nsLayoutUtils::SurfaceFromElement(aCanvas);

  MOZ_ASSERT(result.GetSourceSurface());
  return result.GetSourceSurface()->GetDataSurface();
}

NS_IMETHODIMP
nsDOMWindowUtils::CompareCanvases(nsISupports* aCanvas1, nsISupports* aCanvas2,
                                  uint32_t* aMaxDifference, uint32_t* retVal) {
  nsCOMPtr<nsIContent> contentCanvas1 = do_QueryInterface(aCanvas1);
  nsCOMPtr<nsIContent> contentCanvas2 = do_QueryInterface(aCanvas2);
  auto* canvas1 = HTMLCanvasElement::FromNodeOrNull(contentCanvas1);
  auto* canvas2 = HTMLCanvasElement::FromNodeOrNull(contentCanvas2);

  if (NS_WARN_IF(!canvas1) || NS_WARN_IF(!canvas2)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<DataSourceSurface> img1 = CanvasToDataSourceSurface(canvas1);
  RefPtr<DataSourceSurface> img2 = CanvasToDataSourceSurface(canvas2);

  if (NS_WARN_IF(!img1) || NS_WARN_IF(!img2) ||
      NS_WARN_IF(img1->GetSize() != img2->GetSize())) {
    return NS_ERROR_FAILURE;
  }

  if (img1->Equals(img2)) {
    // They point to the same underlying content.
    return NS_OK;
  }

  DataSourceSurface::ScopedMap map1(img1, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap map2(img2, DataSourceSurface::READ);

  if (NS_WARN_IF(!map1.IsMapped()) || NS_WARN_IF(!map2.IsMapped()) ||
      NS_WARN_IF(map1.GetStride() != map2.GetStride())) {
    return NS_ERROR_FAILURE;
  }

  int v;
  IntSize size = img1->GetSize();
  int32_t stride = map1.GetStride();

  // we can optimize for the common all-pass case
  if (stride == size.width * 4) {
    v = memcmp(map1.GetData(), map2.GetData(), size.width * size.height * 4);
    if (v == 0) {
      if (aMaxDifference) *aMaxDifference = 0;
      *retVal = 0;
      return NS_OK;
    }
  }

  uint32_t dc = 0;
  uint32_t different = 0;

  for (int j = 0; j < size.height; j++) {
    unsigned char* p1 = map1.GetData() + j * stride;
    unsigned char* p2 = map2.GetData() + j * stride;
    v = memcmp(p1, p2, stride);

    if (v) {
      for (int i = 0; i < size.width; i++) {
        if (*(uint32_t*)p1 != *(uint32_t*)p2) {
          different++;

          dc = std::max((uint32_t)abs(p1[0] - p2[0]), dc);
          dc = std::max((uint32_t)abs(p1[1] - p2[1]), dc);
          dc = std::max((uint32_t)abs(p1[2] - p2[2]), dc);
          dc = std::max((uint32_t)abs(p1[3] - p2[3]), dc);
        }

        p1 += 4;
        p2 += 4;
      }
    }
  }

  if (aMaxDifference) *aMaxDifference = dc;

  *retVal = different;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsMozAfterPaintPending(bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;
  nsPresContext* presContext = GetPresContext();
  if (!presContext) return NS_OK;
  *aResult = presContext->IsDOMPaintEventPending();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsInputTaskManagerSuspended(bool* aResult) {
  *aResult = InputTaskManager::Get()->IsSuspended();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::DisableNonTestMouseEvents(bool aDisable) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsIDocShell* docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  PresShell* presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  presShell->DisableNonTestMouseEvents(aDisable);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SuppressEventHandling(bool aSuppress) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  if (aSuppress) {
    window->SuppressEventHandling();
  } else {
    window->UnsuppressEventHandling();
  }

  return NS_OK;
}

static nsresult getScrollXYAppUnits(const nsWeakPtr& aWindow, bool aFlushLayout,
                                    nsPoint& aScrollPos) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(aWindow);
  nsCOMPtr<Document> doc = window ? window->GetExtantDoc() : nullptr;
  NS_ENSURE_STATE(doc);

  if (aFlushLayout) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  if (PresShell* presShell = doc->GetPresShell()) {
    nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
    if (sf) {
      aScrollPos = sf->GetScrollPosition();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScrollXY(bool aFlushLayout, int32_t* aScrollX,
                              int32_t* aScrollY) {
  nsPoint scrollPos(0, 0);
  nsresult rv = getScrollXYAppUnits(mWindow, aFlushLayout, scrollPos);
  NS_ENSURE_SUCCESS(rv, rv);
  *aScrollX = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x);
  *aScrollY = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScrollXYFloat(bool aFlushLayout, float* aScrollX,
                                   float* aScrollY) {
  nsPoint scrollPos(0, 0);
  nsresult rv = getScrollXYAppUnits(mWindow, aFlushLayout, scrollPos);
  NS_ENSURE_SUCCESS(rv, rv);
  *aScrollX = nsPresContext::AppUnitsToFloatCSSPixels(scrollPos.x);
  *aScrollY = nsPresContext::AppUnitsToFloatCSSPixels(scrollPos.y);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ScrollToVisual(float aOffsetX, float aOffsetY,
                                 int32_t aUpdateType, int32_t aScrollMode) {
  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  nsPresContext* presContext = doc->GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_NOT_AVAILABLE);

  // This should only be called on the root content document.
  NS_ENSURE_TRUE(presContext->IsRootContentDocumentCrossProcess(),
                 NS_ERROR_INVALID_ARG);

  FrameMetrics::ScrollOffsetUpdateType updateType;
  switch (aUpdateType) {
    case UPDATE_TYPE_RESTORE:
      updateType = FrameMetrics::eRestore;
      break;
    case UPDATE_TYPE_MAIN_THREAD:
      updateType = FrameMetrics::eMainThread;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  ScrollMode scrollMode;
  switch (aScrollMode) {
    case SCROLL_MODE_INSTANT:
      scrollMode = ScrollMode::Instant;
      break;
    case SCROLL_MODE_SMOOTH:
      scrollMode = ScrollMode::SmoothMsd;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  presContext->PresShell()->ScrollToVisual(
      CSSPoint::ToAppUnits(CSSPoint(aOffsetX, aOffsetY)), updateType,
      scrollMode);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetVisualViewportOffsetRelativeToLayoutViewport(
    float* aOffsetX, float* aOffsetY) {
  *aOffsetX = 0;
  *aOffsetY = 0;

  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  PresShell* presShell = doc->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_AVAILABLE);

  nsPoint offset = presShell->GetVisualViewportOffsetRelativeToLayoutViewport();
  *aOffsetX = nsPresContext::AppUnitsToFloatCSSPixels(offset.x);
  *aOffsetY = nsPresContext::AppUnitsToFloatCSSPixels(offset.y);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetVisualViewportOffset(int32_t* aOffsetX,
                                          int32_t* aOffsetY) {
  *aOffsetX = 0;
  *aOffsetY = 0;

  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  PresShell* presShell = doc->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_AVAILABLE);

  nsPoint offset = presShell->GetVisualViewportOffset();
  *aOffsetX = nsPresContext::AppUnitsToIntCSSPixels(offset.x);
  *aOffsetY = nsPresContext::AppUnitsToIntCSSPixels(offset.y);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::TransformRectLayoutToVisual(float aX, float aY, float aWidth,
                                              float aHeight,
                                              DOMRect** aResult) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  PresShell* presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_AVAILABLE);

  CSSRect rect(aX, aY, aWidth, aHeight);
  rect = ViewportUtils::DocumentRelativeLayoutToVisual(rect, presShell);

  RefPtr<DOMRect> outRect = new DOMRect(window);
  outRect->SetRect(rect.x, rect.y, rect.width, rect.height);
  outRect.forget(aResult);
  return NS_OK;
}

Result<mozilla::ScreenRect, nsresult> nsDOMWindowUtils::ConvertToScreenRect(
    float aX, float aY, float aWidth, float aHeight) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  if (!window) {
    return Err(NS_ERROR_NOT_AVAILABLE);
  }

  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return Err(NS_ERROR_NOT_AVAILABLE);
  }

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return Err(NS_ERROR_NOT_AVAILABLE);
  }

  // Note that if the document is NOT in OOP iframes, i.e. it's in the top level
  // content subtree in the same process,
  // nsIWidget::WidgetToTopLevelWidgetTransform() doesn't include the desktop
  // zoom value, so for documents in the top level content document subtree,
  // this ViewportUtils::DocumentRelativeLayoutToVisual call applies the desktop
  // zoom value via PresShell::GetResolution() in the function.
  CSSRect rect(aX, aY, aWidth, aHeight);
  rect = ViewportUtils::DocumentRelativeLayoutToVisual(rect, presShell);

  nsPresContext* presContext = presShell->GetPresContext();
  MOZ_ASSERT(presContext);

  // For OOP iframe documents, we don't have desktop zoom value specifically in
  // each iframe documents (i.e. the in-process root presshell's resolution is
  // 1.0), instead nsIWidget::WidgetToTopLevelWidgetTransform() includes the
  // desktop zoom scale value along with translations by ancestor scroll
  // containers, ancestor CSS transforms, etc.
  nsRect appUnitsRect = CSSPixel::ToAppUnits(rect);
  LayoutDeviceRect devPixelsRect = LayoutDeviceRect::FromAppUnits(
      appUnitsRect, presContext->AppUnitsPerDevPixel());
  devPixelsRect =
      widget->WidgetToTopLevelWidgetTransform().TransformBounds(devPixelsRect) +
      widget->TopLevelWidgetToScreenOffset();

  return ViewAs<ScreenPixel>(
      devPixelsRect, PixelCastJustification::ScreenIsParentLayerForRoot);
}

NS_IMETHODIMP
nsDOMWindowUtils::ToScreenRectInCSSUnits(float aX, float aY, float aWidth,
                                         float aHeight, DOMRect** aResult) {
  ScreenRect rect;
  MOZ_TRY_VAR(rect, ConvertToScreenRect(aX, aY, aWidth, aHeight));

  nsPresContext* presContext = GetPresContext();
  MOZ_ASSERT(presContext);

  nsRect appUnitsRect = LayoutDeviceRect::ToAppUnits(
      ViewAs<LayoutDevicePixel>(
          rect, PixelCastJustification::ScreenIsParentLayerForRoot),
      presContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom());
  CSSRect cssUnitsRect = CSSRect::FromAppUnits(appUnitsRect);

  RefPtr<DOMRect> outRect = new DOMRect(mWindow);
  outRect->SetRect(cssUnitsRect.x, cssUnitsRect.y, cssUnitsRect.width,
                   cssUnitsRect.height);
  outRect.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ToScreenRect(float aX, float aY, float aWidth, float aHeight,
                               DOMRect** aResult) {
  ScreenRect rect;
  MOZ_TRY_VAR(rect, ConvertToScreenRect(aX, aY, aWidth, aHeight));

  RefPtr<DOMRect> outRect = new DOMRect(mWindow);
  outRect->SetRect(rect.x, rect.y, rect.width, rect.height);
  outRect.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ConvertFromParentProcessWidgetToLocal(float aX, float aY,
                                                        float aWidth,
                                                        float aHeight,
                                                        DOMRect** aResult) {
  if (!XRE_IsContentProcess()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  if (!window) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  LayoutDeviceRect devPixelsRect = LayoutDeviceRect(aX, aY, aWidth, aHeight);

  Maybe<LayoutDeviceToLayoutDeviceMatrix4x4> inverse =
      widget->WidgetToTopLevelWidgetTransform().MaybeInverse();
  if (inverse) {
    Maybe<LayoutDeviceRect> rect =
        UntransformBy(*inverse, devPixelsRect, LayoutDeviceRect::MaxIntRect());
    if (rect) {
      RefPtr<DOMRect> outRect = new DOMRect(mWindow);
      outRect->SetRect(rect->x, rect->y, rect->width, rect->height);
      outRect.forget(aResult);
      return NS_OK;
    }
  }

  RefPtr<DOMRect> outRect = new DOMRect(mWindow);
  outRect->SetRect(0, 0, 0, 0);
  outRect.forget(aResult);
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetDynamicToolbarMaxHeight(uint32_t aHeightInScreen) {
  if (aHeightInScreen > INT32_MAX) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<nsPresContext> presContext = GetPresContext();
  if (!presContext) {
    return NS_OK;
  }

  MOZ_ASSERT(presContext->IsRootContentDocumentCrossProcess());

  presContext->SetDynamicToolbarMaxHeight(ScreenIntCoord(aHeightInScreen));

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScrollbarSize(bool aFlushLayout, int32_t* aWidth,
                                   int32_t* aHeight) {
  *aWidth = 0;
  *aHeight = 0;

  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  if (aFlushLayout) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  PresShell* presShell = doc->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_AVAILABLE);

  nsIScrollableFrame* scrollFrame = presShell->GetRootScrollFrameAsScrollable();
  NS_ENSURE_TRUE(scrollFrame, NS_OK);

  nsMargin sizes = scrollFrame->GetActualScrollbarSizes();
  *aWidth = nsPresContext::AppUnitsToIntCSSPixels(sizes.LeftRight());
  *aHeight = nsPresContext::AppUnitsToIntCSSPixels(sizes.TopBottom());

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetBoundsWithoutFlushing(Element* aElement,
                                           DOMRect** aResult) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  NS_ENSURE_ARG_POINTER(aElement);

  RefPtr<DOMRect> rect = new DOMRect(window);
  nsIFrame* frame = aElement->GetPrimaryFrame();

  if (frame) {
    nsRect r = nsLayoutUtils::GetAllInFlowRectsUnion(
        frame, nsLayoutUtils::GetContainingBlockForClientRect(frame),
        nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS);
    rect->SetLayoutRect(r);
  }

  rect.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::NeedsFlush(int32_t aFlushType, bool* aResult) {
  MOZ_ASSERT(aResult);

  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  PresShell* presShell = doc->GetPresShell();
  NS_ENSURE_STATE(presShell);

  FlushType flushType;
  switch (aFlushType) {
    case FLUSH_STYLE:
      flushType = FlushType::Style;
      break;

    case FLUSH_LAYOUT:
      flushType = FlushType::Layout;
      break;

    case FLUSH_DISPLAY:
      flushType = FlushType::Display;
      break;

    default:
      return NS_ERROR_INVALID_ARG;
  }

  *aResult = presShell->NeedFlush(flushType);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::FlushLayoutWithoutThrottledAnimations() {
  nsCOMPtr<Document> doc = GetDocument();
  if (doc) {
    doc->FlushPendingNotifications(
        ChangesToFlush(FlushType::Layout, false /* flush animations */));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetRootBounds(DOMRect** aResult) {
  Document* doc = GetDocument();
  NS_ENSURE_STATE(doc);

  nsRect bounds(0, 0, 0, 0);
  PresShell* presShell = doc->GetPresShell();
  if (presShell) {
    nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
    if (sf) {
      bounds = sf->GetScrollRange();
      bounds.SetWidth(bounds.Width() + sf->GetScrollPortRect().Width());
      bounds.SetHeight(bounds.Height() + sf->GetScrollPortRect().Height());
    } else if (presShell->GetRootFrame()) {
      bounds = presShell->GetRootFrame()->GetRect();
    }
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  RefPtr<DOMRect> rect = new DOMRect(window);
  rect->SetRect(nsPresContext::AppUnitsToFloatCSSPixels(bounds.x),
                nsPresContext::AppUnitsToFloatCSSPixels(bounds.y),
                nsPresContext::AppUnitsToFloatCSSPixels(bounds.Width()),
                nsPresContext::AppUnitsToFloatCSSPixels(bounds.Height()));
  rect.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIMEIsOpen(bool* aState) {
  NS_ENSURE_ARG_POINTER(aState);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) return NS_ERROR_FAILURE;

  // Open state should not be available when IME is not enabled.
  InputContext context = widget->GetInputContext();
  if (context.mIMEState.mEnabled != IMEEnabled::Enabled) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (context.mIMEState.mOpen == IMEState::OPEN_STATE_NOT_SUPPORTED) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  *aState = (context.mIMEState.mOpen == IMEState::OPEN);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIMEStatus(uint32_t* aState) {
  NS_ENSURE_ARG_POINTER(aState);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) return NS_ERROR_FAILURE;

  InputContext context = widget->GetInputContext();
  *aState = static_cast<uint32_t>(context.mIMEState.mEnabled);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetInputContextOrigin(uint32_t* aOrigin) {
  NS_ENSURE_ARG_POINTER(aOrigin);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  InputContext context = widget->GetInputContext();
  static_assert(static_cast<uint32_t>(InputContext::Origin::ORIGIN_MAIN) ==
                INPUT_CONTEXT_ORIGIN_MAIN);
  static_assert(static_cast<uint32_t>(InputContext::Origin::ORIGIN_CONTENT) ==
                INPUT_CONTEXT_ORIGIN_CONTENT);
  MOZ_ASSERT(context.mOrigin == InputContext::Origin::ORIGIN_MAIN ||
             context.mOrigin == InputContext::Origin::ORIGIN_CONTENT);
  *aOrigin = static_cast<uint32_t>(context.mOrigin);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetNodeObservedByIMEContentObserver(nsINode** aNode) {
  NS_ENSURE_ARG_POINTER(aNode);

  IMEContentObserver* observer = IMEStateManager::GetActiveContentObserver();
  if (!observer) {
    *aNode = nullptr;
    return NS_OK;
  }
  *aNode = do_AddRef(observer->GetObservingContent()).take();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetCanvasBackgroundColor(nsAString& aColor) {
  if (RefPtr<Document> doc = GetDocument()) {
    doc->FlushPendingNotifications(FlushType::Frames);
  }
  nscolor color = NS_RGB(255, 255, 255);
  if (PresShell* presShell = GetPresShell()) {
    color = presShell->ComputeCanvasBackground().mColor;
  }
  nsStyleUtil::GetSerializedColorValue(color, aColor);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFocusedInputType(nsAString& aType) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  aType = widget->GetInputContext().mHTMLInputType;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFocusedActionHint(nsAString& aType) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  aType = widget->GetInputContext().mActionHint;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFocusedInputMode(nsAString& aInputMode) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }
  aInputMode = widget->GetInputContext().mHTMLInputInputmode;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFocusedAutocapitalize(nsAString& aAutocapitalize) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }
  aAutocapitalize = widget->GetInputContext().mAutocapitalize;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetViewId(Element* aElement, nsViewID* aResult) {
  if (aElement && nsLayoutUtils::FindIDFor(aElement, aResult)) {
    return NS_OK;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScreenPixelsPerCSSPixel(float* aScreenPixels) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  *aScreenPixels = window->GetDevicePixelRatio(CallerType::System);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScreenPixelsPerCSSPixelNoOverride(float* aScreenPixels) {
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    *aScreenPixels = 1.0;
    return NS_OK;
  }

  *aScreenPixels =
      float(AppUnitsPerCSSPixel()) / float(presContext->AppUnitsPerDevPixel());
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFullZoom(float* aFullZoom) {
  *aFullZoom = 1.0f;

  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_OK;
  }

  *aFullZoom = presContext->DeviceContext()->GetFullZoom();

  return NS_OK;
}

NS_IMETHODIMP nsDOMWindowUtils::DispatchDOMEventViaPresShellForTesting(
    nsINode* aTarget, Event* aEvent, bool* aRetVal) {
  NS_ENSURE_STATE(aEvent);
  aEvent->SetTrusted(true);
  WidgetEvent* internalEvent = aEvent->WidgetEventPtr();
  NS_ENSURE_STATE(internalEvent);
  // This API is currently used only by EventUtils.js.  Thus we should always
  // set mIsSynthesizedForTests to true.
  internalEvent->mFlags.mIsSynthesizedForTests = true;
  nsCOMPtr<nsIContent> content = nsIContent::FromNodeOrNull(aTarget);
  NS_ENSURE_STATE(content);
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  if (content->OwnerDoc()->GetWindow() != window) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }
  nsCOMPtr<Document> targetDoc = content->GetUncomposedDoc();
  NS_ENSURE_STATE(targetDoc);
  RefPtr<PresShell> targetPresShell = targetDoc->GetPresShell();
  NS_ENSURE_STATE(targetPresShell);

  targetDoc->FlushPendingNotifications(FlushType::Layout);

  nsEventStatus status = nsEventStatus_eIgnore;
  targetPresShell->HandleEventWithTarget(internalEvent, nullptr, content,
                                         &status);
  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return NS_OK;
}

static void InitEvent(WidgetGUIEvent& aEvent,
                      LayoutDeviceIntPoint* aPt = nullptr) {
  if (aPt) {
    aEvent.mRefPoint = *aPt;
  }
  aEvent.mTime = PR_IntervalNow();
}

NS_IMETHODIMP
nsDOMWindowUtils::SendQueryContentEvent(uint32_t aType, int64_t aOffset,
                                        uint32_t aLength, int32_t aX,
                                        int32_t aY, uint32_t aAdditionalFlags,
                                        nsIQueryContentEventResult** aResult) {
  *aResult = nullptr;

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsIDocShell* docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  PresShell* presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsPresContext* presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  EventMessage message;
  switch (aType) {
    case QUERY_SELECTED_TEXT:
      message = eQuerySelectedText;
      break;
    case QUERY_TEXT_CONTENT:
      message = eQueryTextContent;
      break;
    case QUERY_CARET_RECT:
      message = eQueryCaretRect;
      break;
    case QUERY_TEXT_RECT:
      message = eQueryTextRect;
      break;
    case QUERY_EDITOR_RECT:
      message = eQueryEditorRect;
      break;
    case QUERY_CHARACTER_AT_POINT:
      message = eQueryCharacterAtPoint;
      break;
    case QUERY_TEXT_RECT_ARRAY:
      message = eQueryTextRectArray;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  SelectionType selectionType = SelectionType::eNormal;
  static const uint32_t kSelectionFlags =
      QUERY_CONTENT_FLAG_SELECTION_SPELLCHECK |
      QUERY_CONTENT_FLAG_SELECTION_IME_RAWINPUT |
      QUERY_CONTENT_FLAG_SELECTION_IME_SELECTEDRAWTEXT |
      QUERY_CONTENT_FLAG_SELECTION_IME_CONVERTEDTEXT |
      QUERY_CONTENT_FLAG_SELECTION_IME_SELECTEDCONVERTEDTEXT |
      QUERY_CONTENT_FLAG_SELECTION_ACCESSIBILITY |
      QUERY_CONTENT_FLAG_SELECTION_FIND |
      QUERY_CONTENT_FLAG_SELECTION_URLSECONDARY |
      QUERY_CONTENT_FLAG_SELECTION_URLSTRIKEOUT;
  switch (aAdditionalFlags & kSelectionFlags) {
    case QUERY_CONTENT_FLAG_SELECTION_SPELLCHECK:
      selectionType = SelectionType::eSpellCheck;
      break;
    case QUERY_CONTENT_FLAG_SELECTION_IME_RAWINPUT:
      selectionType = SelectionType::eIMERawClause;
      break;
    case QUERY_CONTENT_FLAG_SELECTION_IME_SELECTEDRAWTEXT:
      selectionType = SelectionType::eIMESelectedRawClause;
      break;
    case QUERY_CONTENT_FLAG_SELECTION_IME_CONVERTEDTEXT:
      selectionType = SelectionType::eIMEConvertedClause;
      break;
    case QUERY_CONTENT_FLAG_SELECTION_IME_SELECTEDCONVERTEDTEXT:
      selectionType = SelectionType::eIMESelectedClause;
      break;
    case QUERY_CONTENT_FLAG_SELECTION_ACCESSIBILITY:
      selectionType = SelectionType::eAccessibility;
      break;
    case QUERY_CONTENT_FLAG_SELECTION_FIND:
      selectionType = SelectionType::eFind;
      break;
    case QUERY_CONTENT_FLAG_SELECTION_URLSECONDARY:
      selectionType = SelectionType::eURLSecondary;
      break;
    case QUERY_CONTENT_FLAG_SELECTION_URLSTRIKEOUT:
      selectionType = SelectionType::eURLStrikeout;
      break;
    case 0:
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  if (selectionType != SelectionType::eNormal &&
      message != eQuerySelectedText) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIWidget> targetWidget = widget;
  LayoutDeviceIntPoint pt(aX, aY);

  WidgetQueryContentEvent::Options options;
  options.mUseNativeLineBreak =
      !(aAdditionalFlags & QUERY_CONTENT_FLAG_USE_XP_LINE_BREAK);
  options.mRelativeToInsertionPoint =
      (aAdditionalFlags &
       QUERY_CONTENT_FLAG_OFFSET_RELATIVE_TO_INSERTION_POINT) != 0;
  if (options.mRelativeToInsertionPoint) {
    switch (message) {
      case eQueryTextContent:
      case eQueryCaretRect:
      case eQueryTextRect:
        break;
      default:
        return NS_ERROR_INVALID_ARG;
    }
  } else if (aOffset < 0) {
    return NS_ERROR_INVALID_ARG;
  }

  if (message == eQueryCharacterAtPoint) {
    // Looking for the widget at the point.
    WidgetQueryContentEvent dummyEvent(true, eQueryContentState, widget);
    dummyEvent.Init(options);
    InitEvent(dummyEvent, &pt);
    nsIFrame* popupFrame = nsLayoutUtils::GetPopupFrameForEventCoordinates(
        presContext->GetRootPresContext(), &dummyEvent);

    LayoutDeviceIntRect widgetBounds = widget->GetClientBounds();
    widgetBounds.MoveTo(0, 0);

    // There is no popup frame at the point and the point isn't in our widget,
    // we cannot process this request.
    NS_ENSURE_TRUE(popupFrame || widgetBounds.Contains(pt), NS_ERROR_FAILURE);

    // Fire the event on the widget at the point
    if (popupFrame) {
      targetWidget = popupFrame->GetNearestWidget();
    }
  }

  pt += widget->WidgetToScreenOffset() - targetWidget->WidgetToScreenOffset();

  WidgetQueryContentEvent queryEvent(true, message, targetWidget);
  InitEvent(queryEvent, &pt);

  switch (message) {
    case eQueryTextContent:
      queryEvent.InitForQueryTextContent(aOffset, aLength, options);
      break;
    case eQueryCaretRect:
      queryEvent.InitForQueryCaretRect(aOffset, options);
      break;
    case eQueryTextRect:
      queryEvent.InitForQueryTextRect(aOffset, aLength, options);
      break;
    case eQuerySelectedText:
      queryEvent.InitForQuerySelectedText(selectionType, options);
      break;
    case eQueryTextRectArray:
      queryEvent.InitForQueryTextRectArray(aOffset, aLength, options);
      break;
    default:
      queryEvent.Init(options);
      break;
  }

  nsEventStatus status;
  nsresult rv = targetWidget->DispatchEvent(&queryEvent, status);
  NS_ENSURE_SUCCESS(rv, rv);

  auto* result = new nsQueryContentEventResult(std::move(queryEvent));
  result->SetEventResult(widget);
  NS_ADDREF(*aResult = result);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendSelectionSetEvent(uint32_t aOffset, uint32_t aLength,
                                        uint32_t aAdditionalFlags,
                                        bool* aResult) {
  *aResult = false;

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  WidgetSelectionEvent selectionEvent(true, eSetSelection, widget);
  InitEvent(selectionEvent);

  selectionEvent.mOffset = aOffset;
  selectionEvent.mLength = aLength;
  selectionEvent.mReversed = (aAdditionalFlags & SELECTION_SET_FLAG_REVERSE);
  selectionEvent.mUseNativeLineBreak =
      !(aAdditionalFlags & SELECTION_SET_FLAG_USE_XP_LINE_BREAK);

  nsEventStatus status;
  nsresult rv = widget->DispatchEvent(&selectionEvent, status);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = selectionEvent.mSucceeded;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendContentCommandEvent(const nsAString& aType,
                                          nsITransferable* aTransferable,
                                          const nsAString& aString) {
  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) return NS_ERROR_FAILURE;

  EventMessage msg;
  if (aType.EqualsLiteral("cut")) {
    msg = eContentCommandCut;
  } else if (aType.EqualsLiteral("copy")) {
    msg = eContentCommandCopy;
  } else if (aType.EqualsLiteral("paste")) {
    msg = eContentCommandPaste;
  } else if (aType.EqualsLiteral("delete")) {
    msg = eContentCommandDelete;
  } else if (aType.EqualsLiteral("undo")) {
    msg = eContentCommandUndo;
  } else if (aType.EqualsLiteral("redo")) {
    msg = eContentCommandRedo;
  } else if (aType.EqualsLiteral("insertText")) {
    msg = eContentCommandInsertText;
  } else if (aType.EqualsLiteral("pasteTransferable")) {
    msg = eContentCommandPasteTransferable;
  } else {
    return NS_ERROR_FAILURE;
  }

  WidgetContentCommandEvent event(true, msg, widget);
  if (msg == eContentCommandInsertText) {
    event.mString.emplace(aString);
  }
  if (msg == eContentCommandPasteTransferable) {
    event.mTransferable = aTransferable;
  }

  nsEventStatus status;
  return widget->DispatchEvent(&event, status);
}

NS_IMETHODIMP
nsDOMWindowUtils::GetClassName(JS::Handle<JS::Value> aObject, JSContext* aCx,
                               char** aName) {
  // Our argument must be a non-null object.
  if (aObject.isPrimitive()) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  *aName = NS_xstrdup(JS::GetClass(aObject.toObjectOrNull())->name);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetVisitedDependentComputedStyle(
    Element* aElement, const nsAString& aPseudoElement,
    const nsAString& aPropertyName, nsAString& aResult) {
  aResult.Truncate();

  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window && aElement);
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = window->GetCurrentInnerWindow();
  NS_ENSURE_STATE(window);

  nsCOMPtr<nsICSSDeclaration> decl;
  {
    ErrorResult rv;
    decl = innerWindow->GetComputedStyle(*aElement, aPseudoElement, rv);
    ENSURE_SUCCESS(rv, rv.StealNSResult());
  }

  nsAutoCString result;

  static_cast<nsComputedDOMStyle*>(decl.get())->SetExposeVisitedStyle(true);
  nsresult rv =
      decl->GetPropertyValue(NS_ConvertUTF16toUTF8(aPropertyName), result);
  static_cast<nsComputedDOMStyle*>(decl.get())->SetExposeVisitedStyle(false);

  CopyUTF8toUTF16(result, aResult);
  return rv;
}

NS_IMETHODIMP
nsDOMWindowUtils::EnterModalState() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  window->EnterModalState();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::LeaveModalState() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  window->LeaveModalState();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::IsInModalState(bool* retval) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  *retval = nsGlobalWindowOuter::Cast(window)->IsInModalState();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetDesktopModeViewport(bool* retval) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  *retval = window && window->IsDesktopModeViewport();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetDesktopModeViewport(bool aDesktopMode) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  window->SetDesktopModeViewport(aDesktopMode);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SuspendTimeouts() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindowInner> inner = window->GetCurrentInnerWindow();
  NS_ENSURE_TRUE(inner, NS_ERROR_FAILURE);

  inner->Suspend();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ResumeTimeouts() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindowInner> inner = window->GetCurrentInnerWindow();
  NS_ENSURE_TRUE(inner, NS_ERROR_FAILURE);

  inner->Resume();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetLayerManagerType(nsAString& aType) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) return NS_ERROR_FAILURE;

  WindowRenderer* renderer = widget->GetWindowRenderer();
  if (!renderer) return NS_ERROR_FAILURE;

  renderer->GetBackendName(aType);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetLayerManagerRemote(bool* retval) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) return NS_ERROR_FAILURE;

  WindowRenderer* renderer = widget->GetWindowRenderer();
  if (!renderer) return NS_ERROR_FAILURE;

  *retval = !!renderer->AsKnowsCompositor();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsWebRenderRequested(bool* retval) {
  *retval = gfxPlatform::WebRenderPrefEnabled() ||
            gfxPlatform::WebRenderEnvvarEnabled();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetCurrentAudioBackend(nsAString& aBackend) {
  CubebUtils::GetCurrentBackend(aBackend);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetCurrentMaxAudioChannels(uint32_t* aChannels) {
  *aChannels = CubebUtils::MaxNumberOfChannels();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetCurrentPreferredSampleRate(uint32_t* aRate) {
  *aRate = CubebUtils::PreferredSampleRate();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::DefaultDevicesRoundTripLatency(Promise** aOutPromise) {
  NS_ENSURE_ARG_POINTER(aOutPromise);
  *aOutPromise = nullptr;

  nsCOMPtr<nsPIDOMWindowOuter> outer = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(outer);
  nsCOMPtr<nsPIDOMWindowInner> inner = outer->GetCurrentInnerWindow();
  NS_ENSURE_STATE(inner);

  ErrorResult err;
  RefPtr<Promise> promise = Promise::Create(inner->AsGlobal(), err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  NS_ADDREF(promise.get());
  void* p = reinterpret_cast<void*>(promise.get());
  NS_DispatchBackgroundTask(
      NS_NewRunnableFunction("DefaultDevicesRoundTripLatency", [p]() {
        double mean, stddev;
        bool success =
            CubebUtils::EstimatedRoundTripLatencyDefaultDevices(&mean, &stddev);

        NS_DispatchToMainThread(NS_NewRunnableFunction(
            "DefaultDevicesRoundTripLatency", [p, success, mean, stddev]() {
              Promise* promise = reinterpret_cast<Promise*>(p);
              if (!success) {
                promise->MaybeReject(NS_ERROR_FAILURE);
                NS_RELEASE(promise);
                return;
              }
              nsTArray<double> a;
              a.AppendElement(mean);
              a.AppendElement(stddev);
              promise->MaybeResolve(a);
              NS_RELEASE(promise);
            }));
      }));

  promise.forget(aOutPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::AudioDevices(uint16_t aSide, nsIArray** aDevices) {
  NS_ENSURE_ARG_POINTER(aDevices);
  NS_ENSURE_ARG((aSide == AUDIO_INPUT) || (aSide == AUDIO_OUTPUT));
  *aDevices = nullptr;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> devices =
      do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<CubebDeviceEnumerator> enumerator = Enumerator::GetInstance();
  nsTArray<RefPtr<AudioDeviceInfo>> collection;
  if (aSide == AUDIO_INPUT) {
    enumerator->EnumerateAudioInputDevices(collection);
  } else {
    enumerator->EnumerateAudioOutputDevices(collection);
  }

  for (const auto& device : collection) {
    devices->AppendElement(device);
  }

  devices.forget(aDevices);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StartFrameTimeRecording(uint32_t* startIndex) {
  NS_ENSURE_ARG_POINTER(startIndex);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) return NS_ERROR_FAILURE;

  WindowRenderer* renderer = widget->GetWindowRenderer();
  if (!renderer) return NS_ERROR_FAILURE;

  const uint32_t kRecordingMinSize = 60 * 10;       // 10 seconds @60 fps.
  const uint32_t kRecordingMaxSize = 60 * 60 * 60;  // One hour
  uint32_t bufferSize =
      Preferences::GetUint("toolkit.framesRecording.bufferSize", uint32_t(0));
  bufferSize = std::min(bufferSize, kRecordingMaxSize);
  bufferSize = std::max(bufferSize, kRecordingMinSize);
  *startIndex = renderer->StartFrameTimeRecording(bufferSize);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StopFrameTimeRecording(uint32_t startIndex,
                                         nsTArray<float>& frameIntervals) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) return NS_ERROR_FAILURE;

  WindowRenderer* renderer = widget->GetWindowRenderer();
  if (!renderer) return NS_ERROR_FAILURE;

  renderer->StopFrameTimeRecording(startIndex, frameIntervals);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::AdvanceTimeAndRefresh(int64_t aMilliseconds) {
  // Before we advance the time, we should trigger any animations that are
  // waiting to start. This is because there are many tests that call this
  // which expect animations to start immediately. Ideally, we should make
  // all these tests do an asynchronous wait on the corresponding animation's
  // 'ready' promise before continuing. Then we could remove the special
  // handling here and the code path followed when testing would more closely
  // match the code path during regular operation. Filed as bug 1112957.
  nsCOMPtr<Document> doc = GetDocument();
  if (doc) {
    PendingAnimationTracker* tracker = doc->GetPendingAnimationTracker();
    if (tracker) {
      tracker->TriggerPendingAnimationsNow();
    }
  }

  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    nsRefreshDriver* driver = presContext->RefreshDriver();
    driver->AdvanceTimeAndRefresh(aMilliseconds);

    if (WebRenderBridgeChild* wrbc = GetWebRenderBridge()) {
      wrbc->SendSetTestSampleTime(driver->MostRecentRefresh());
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetLastTransactionId(uint64_t* aLastTransactionId) {
  nsCOMPtr<nsIDocShell> docShell = GetDocShell();
  if (!docShell) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
  docShell->GetInProcessRootTreeItem(getter_AddRefs(rootTreeItem));
  docShell = do_QueryInterface(rootTreeItem);
  if (!docShell) {
    return NS_ERROR_UNEXPECTED;
  }

  nsPresContext* presContext = docShell->GetPresContext();
  if (!presContext) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefreshDriver* driver = presContext->RefreshDriver();
  *aLastTransactionId = uint64_t(driver->LastTransactionId());
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::RestoreNormalRefresh() {
  // Kick the compositor out of test mode before the refresh driver, so that
  // the refresh driver doesn't send an update that gets ignored by the
  // compositor.
  if (WebRenderBridgeChild* wrbc = GetWebRenderBridge()) {
    wrbc->SendLeaveTestMode();
  }

  if (nsPresContext* pc = GetPresContext()) {
    nsRefreshDriver* driver = pc->RefreshDriver();
    driver->RestoreNormalRefresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsTestControllingRefreshes(bool* aResult) {
  nsPresContext* pc = GetPresContext();
  *aResult =
      pc ? pc->RefreshDriver()->IsTestControllingRefreshesEnabled() : false;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetAsyncPanZoomEnabled(bool* aResult) {
  nsIWidget* widget = GetWidget();
  if (widget) {
    *aResult = widget->AsyncPanZoomEnabled();
  } else {
    *aResult = gfxPlatform::AsyncPanZoomEnabled();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetAsyncScrollOffset(Element* aElement, float aX, float aY) {
  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }
  ScrollableLayerGuid::ViewID viewId;
  if (!nsLayoutUtils::FindIDFor(aElement, &viewId)) {
    return NS_ERROR_UNEXPECTED;
  }
  nsIWidget* widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }
  WindowRenderer* renderer = widget->GetWindowRenderer();
  if (!renderer) {
    return NS_ERROR_FAILURE;
  }
  if (WebRenderLayerManager* wr = renderer->AsWebRender()) {
    WebRenderBridgeChild* wrbc = wr->WrBridge();
    if (!wrbc) {
      return NS_ERROR_UNEXPECTED;
    }
    wrbc->SendSetAsyncScrollOffset(viewId, aX, aY);
    return NS_OK;
  }
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetAsyncZoom(Element* aRootElement, float aValue) {
  if (!aRootElement) {
    return NS_ERROR_INVALID_ARG;
  }
  ScrollableLayerGuid::ViewID viewId;
  if (!nsLayoutUtils::FindIDFor(aRootElement, &viewId)) {
    return NS_ERROR_UNEXPECTED;
  }
  nsIWidget* widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }
  WindowRenderer* renderer = widget->GetWindowRenderer();
  if (!renderer) {
    return NS_ERROR_FAILURE;
  }
  if (WebRenderLayerManager* wr = renderer->AsWebRender()) {
    WebRenderBridgeChild* wrbc = wr->WrBridge();
    if (!wrbc) {
      return NS_ERROR_UNEXPECTED;
    }
    wrbc->SendSetAsyncZoom(viewId, aValue);
    return NS_OK;
  }
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMWindowUtils::FlushApzRepaints(bool* aOutResult) {
  nsIWidget* widget = GetWidget();
  if (!widget) {
    *aOutResult = false;
    return NS_OK;
  }
  // If APZ is not enabled, this function is a no-op.
  if (!widget->AsyncPanZoomEnabled()) {
    *aOutResult = false;
    return NS_OK;
  }
  WindowRenderer* renderer = widget->GetWindowRenderer();
  if (!renderer) {
    *aOutResult = false;
    return NS_OK;
  }
  if (WebRenderLayerManager* wr = renderer->AsWebRender()) {
    WebRenderBridgeChild* wrbc = wr->WrBridge();
    if (!wrbc) {
      return NS_ERROR_UNEXPECTED;
    }
    wrbc->SendFlushApzRepaints();
    *aOutResult = true;
    return NS_OK;
  }
  *aOutResult = false;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::DisableApzForElement(Element* aElement) {
  aElement->SetProperty(nsGkAtoms::apzDisabled, reinterpret_cast<void*>(true));
  nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(aElement);
  if (!sf) {
    return NS_OK;
  }
  nsIFrame* frame = do_QueryFrame(sf);
  if (!frame) {
    return NS_OK;
  }
  frame->SchedulePaint();
  return NS_OK;
}

static nsTArray<nsIScrollableFrame*> CollectScrollableAncestors(
    nsIFrame* aStart) {
  nsTArray<nsIScrollableFrame*> result;
  nsIFrame* frame = aStart;
  while (frame) {
    frame = nsLayoutUtils::GetCrossDocParentFrame(frame);
    if (!frame) {
      break;
    }
    nsIScrollableFrame* scrollAncestor =
        nsLayoutUtils::GetAsyncScrollableAncestorFrame(frame);
    if (!scrollAncestor) {
      break;
    }
    result.AppendElement(scrollAncestor);
    frame = do_QueryFrame(scrollAncestor);
  }
  return result;
}

NS_IMETHODIMP
nsDOMWindowUtils::ZoomToFocusedInput() {
  if (!Preferences::GetBool("apz.zoom-to-focused-input.enabled")) {
    return NS_OK;
  }

  nsIWidget* widget = GetWidget();
  if (!widget) {
    return NS_OK;
  }

  // If APZ is not enabled, this function is a no-op.
  //
  // FIXME(emilio): This is not quite true anymore now that we also
  // ScrollIntoView() too...
  if (!widget->AsyncPanZoomEnabled()) {
    return NS_OK;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return NS_OK;
  }

  RefPtr<Element> element = fm->GetFocusedElement();
  if (!element) {
    return NS_OK;
  }

  RefPtr<PresShell> presShell =
      APZCCallbackHelper::GetRootContentDocumentPresShellForContent(element);
  if (!presShell) {
    return NS_OK;
  }

  bool shouldSkip = [&] {
    nsIFrame* frame = element->GetPrimaryFrame();
    if (!frame) {
      return true;
    }

    // Skip zooming to focused inputs in fixed subtrees, as we'd scroll to the
    // top unnecessarily, see bug 1627734.
    //
    // We could try to teach apz to zoom to a rect only without panning, or
    // maybe we could give it a rect offsetted by the root scroll position, if
    // we wanted to do this.
    for (; frame; frame = nsLayoutUtils::GetCrossDocParentFrame(frame)) {
      if (frame->PresShell() == presShell) {
        // Note that we only do this if the frame belongs to `presShell` (that
        // is, we still zoom in fixed elements in subdocuments, as they're not
        // fixed to the root content document).
        return nsLayoutUtils::IsInPositionFixedSubtree(frame);
      }
      frame = frame->PresShell()->GetRootFrame();
    }

    return false;
  }();

  // The content may be inside a scrollable subframe inside a non-scrollable
  // root content document. In this scenario, we want to ensure that the
  // main-thread side knows to scroll the content into view before we get
  // the bounding content rect and ask APZ to adjust the visual viewport.
  presShell->ScrollContentIntoView(
      element, ScrollAxis(kScrollMinimum, WhenToScroll::IfNotVisible),
      ScrollAxis(kScrollMinimum, WhenToScroll::IfNotVisible),
      ScrollFlags::ScrollOverflowHidden);

  if (shouldSkip) {
    return NS_OK;
  }

  RefPtr<Document> document = presShell->GetDocument();
  if (!document) {
    return NS_OK;
  }

  uint32_t presShellId;
  ScrollableLayerGuid::ViewID viewId;
  if (!APZCCallbackHelper::GetOrCreateScrollIdentifiers(
          document->GetDocumentElement(), &presShellId, &viewId)) {
    return NS_OK;
  }

  uint32_t flags = layers::DISABLE_ZOOM_OUT;
  if (!Preferences::GetBool("formhelper.autozoom") ||
      Preferences::GetBool("formhelper.autozoom.force-disable.test-only",
                           /* aFallback = */ false)) {
    flags |= layers::PAN_INTO_VIEW_ONLY;
  } else {
    flags |= layers::ONLY_ZOOM_TO_DEFAULT_SCALE;
  }

  nsIScrollableFrame* rootScrollFrame =
      presShell->GetRootScrollFrameAsScrollable();
  if (!rootScrollFrame) {
    return NS_OK;
  }

  CSSRect bounds;
  if (element->IsHTMLElement(nsGkAtoms::input)) {
    bounds = nsLayoutUtils::GetBoundingContentRect(element, rootScrollFrame);
  } else {
    // When focused elment is content editable or <textarea> element,
    // focused element will have multi-line content.
    nsIFrame* frame = element->GetPrimaryFrame();
    if (frame) {
      RefPtr<nsCaret> caret = frame->PresShell()->GetCaret();
      if (caret && caret->IsVisible()) {
        nsRect rect;
        if (nsIFrame* frame = caret->GetGeometry(&rect)) {
          bounds = nsLayoutUtils::GetBoundingFrameRect(frame, rootScrollFrame);
        }
      }
    }
    if (bounds.IsEmpty()) {
      // Fallback if no caret frame.
      bounds = nsLayoutUtils::GetBoundingContentRect(element, rootScrollFrame);
    }
  }

  if (bounds.IsEmpty()) {
    // Do not zoom on empty bounds. Bail out.
    return NS_OK;
  }

  bounds.Inflate(15.0f, 0.0f);

  bool waitForRefresh = false;
  for (nsIScrollableFrame* scrollAncestor :
       CollectScrollableAncestors(element->GetPrimaryFrame())) {
    if (scrollAncestor->HasScrollUpdates()) {
      waitForRefresh = true;
      break;
    }
  }
  if (waitForRefresh) {
    waitForRefresh = false;
    if (nsPresContext* presContext = presShell->GetPresContext()) {
      waitForRefresh = true;
      presContext->RegisterManagedPostRefreshObserver(
          new ManagedPostRefreshObserver(
              presContext, [widget = RefPtr<nsIWidget>(widget), presShellId,
                            viewId, bounds, flags](bool aWasCanceled) {
                if (!aWasCanceled) {
                  widget->ZoomToRect(presShellId, viewId, bounds, flags);
                }
                return ManagedPostRefreshObserver::Unregister::Yes;
              }));
    }
  }
  if (!waitForRefresh) {
    widget->ZoomToRect(presShellId, viewId, bounds, flags);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ComputeAnimationDistance(Element* aElement,
                                           const nsAString& aProperty,
                                           const nsAString& aValue1,
                                           const nsAString& aValue2,
                                           double* aResult) {
  NS_ENSURE_ARG_POINTER(aElement);

  nsCSSPropertyID property =
      nsCSSProps::LookupProperty(NS_ConvertUTF16toUTF8(aProperty));
  if (property == eCSSProperty_UNKNOWN || nsCSSProps::IsShorthand(property)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  AnimationValue v1 = AnimationValue::FromString(
      property, NS_ConvertUTF16toUTF8(aValue1), aElement);
  AnimationValue v2 = AnimationValue::FromString(
      property, NS_ConvertUTF16toUTF8(aValue2), aElement);
  if (v1.IsNull() || v2.IsNull()) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  *aResult = v1.ComputeDistance(property, v2);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetUnanimatedComputedStyle(Element* aElement,
                                             const nsAString& aPseudoElement,
                                             const nsAString& aProperty,
                                             int32_t aFlushType,
                                             nsAString& aResult) {
  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCSSPropertyID propertyID =
      nsCSSProps::LookupProperty(NS_ConvertUTF16toUTF8(aProperty));
  if (propertyID == eCSSProperty_UNKNOWN ||
      nsCSSProps::IsShorthand(propertyID)) {
    return NS_ERROR_INVALID_ARG;
  }

  switch (aFlushType) {
    case FLUSH_NONE:
      break;
    case FLUSH_STYLE: {
      if (Document* doc = aElement->GetComposedDoc()) {
        doc->FlushPendingNotifications(FlushType::Style);
      }
      break;
    }
    default:
      return NS_ERROR_INVALID_ARG;
  }

  RefPtr<PresShell> presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  Maybe<PseudoStyleType> pseudo =
      nsCSSPseudoElements::GetPseudoType(aPseudoElement);
  if (!pseudo) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<ComputedStyle> computedStyle =
      nsComputedDOMStyle::GetUnanimatedComputedStyleNoFlush(aElement, *pseudo);
  if (!computedStyle) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<RawServoAnimationValue> value =
      Servo_ComputedValues_ExtractAnimationValue(computedStyle, propertyID)
          .Consume();
  if (!value) {
    return NS_ERROR_FAILURE;
  }
  if (!aElement->GetComposedDoc()) {
    return NS_ERROR_FAILURE;
  }
  nsAutoCString result;
  Servo_AnimationValue_Serialize(value, propertyID,
                                 presShell->StyleSet()->RawSet(), &result);
  CopyUTF8toUTF16(result, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetDisplayDPI(float* aDPI) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) return NS_ERROR_FAILURE;

  *aDPI = widget->GetDPI();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::CheckAndClearPaintedState(Element* aElement, bool* aResult) {
  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  nsIFrame* frame = aElement->GetPrimaryFrame();

  if (!frame) {
    *aResult = false;
    return NS_OK;
  }

  // Get the outermost frame for the content node, so that we can test
  // canvasframe invalidations by observing the documentElement.
  for (;;) {
    nsIFrame* parentFrame = frame->GetParent();
    if (parentFrame && parentFrame->GetContent() == aElement) {
      frame = parentFrame;
    } else {
      break;
    }
  }

  while (frame) {
    if (!frame->CheckAndClearPaintedState()) {
      *aResult = false;
      return NS_OK;
    }
    frame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(frame);
  }
  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::CheckAndClearDisplayListState(Element* aElement,
                                                bool* aResult) {
  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  nsIFrame* frame = aElement->GetPrimaryFrame();

  if (!frame) {
    *aResult = false;
    return NS_OK;
  }

  // Get the outermost frame for the content node, so that we can test
  // canvasframe invalidations by observing the documentElement.
  for (;;) {
    nsIFrame* parentFrame = frame->GetParent();
    if (parentFrame && parentFrame->GetContent() == aElement) {
      frame = parentFrame;
    } else {
      break;
    }
  }

  while (frame) {
    if (!frame->CheckAndClearDisplayListState()) {
      *aResult = false;
      return NS_OK;
    }
    frame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(frame);
  }
  *aResult = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::IsPartOfOpaqueLayer(Element* aElement, bool* aResult) {
  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  nsIFrame* frame = aElement->GetPrimaryFrame();
  if (!frame) {
    return NS_ERROR_FAILURE;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::NumberOfAssignedPaintedLayers(
    const nsTArray<RefPtr<Element>>& aElements, uint32_t* aResult) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::EnableDialogs() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsGlobalWindowOuter::Cast(window)->EnableDialogs();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::DisableDialogs() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsGlobalWindowOuter::Cast(window)->DisableDialogs();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::AreDialogsEnabled(bool* aResult) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  *aResult = nsGlobalWindowOuter::Cast(window)->AreDialogsEnabled();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ResetDialogAbuseState() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsGlobalWindowOuter::Cast(window)
      ->GetBrowsingContextGroup()
      ->ResetDialogAbuseState();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFileId(JS::Handle<JS::Value> aFile, JSContext* aCx,
                            int64_t* _retval) {
  if (aFile.isPrimitive()) {
    *_retval = -1;
    return NS_OK;
  }

  JS::Rooted<JSObject*> obj(aCx, aFile.toObjectOrNull());

  IDBMutableFile* mutableFile = nullptr;
  if (NS_SUCCEEDED(UNWRAP_OBJECT(IDBMutableFile, &obj, mutableFile))) {
    *_retval = mutableFile->GetFileId();
    return NS_OK;
  }

  Blob* blob = nullptr;
  if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, &obj, blob))) {
    *_retval = blob->GetFileId();
    return NS_OK;
  }

  *_retval = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFilePath(JS::HandleValue aFile, JSContext* aCx,
                              nsAString& _retval) {
  if (aFile.isPrimitive()) {
    _retval.Truncate();
    return NS_OK;
  }

  JS::Rooted<JSObject*> obj(aCx, aFile.toObjectOrNull());

  File* file = nullptr;
  if (NS_SUCCEEDED(UNWRAP_OBJECT(File, &obj, file))) {
    nsString filePath;
    ErrorResult rv;
    file->GetMozFullPathInternal(filePath, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }

    _retval = filePath;
    return NS_OK;
  }

  _retval.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFileReferences(const nsAString& aDatabaseName, int64_t aId,
                                    JS::Handle<JS::Value> aOptions,
                                    int32_t* aRefCnt, int32_t* aDBRefCnt,
                                    JSContext* aCx, bool* aResult) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCString origin;
  MOZ_TRY_VAR(origin, quota::QuotaManager::GetOriginFromWindow(window));

  IDBOpenDBOptions options;
  JS::Rooted<JS::Value> optionsVal(aCx, aOptions);
  if (!options.Init(aCx, optionsVal)) {
    return NS_ERROR_UNEXPECTED;
  }

  const quota::PersistenceType persistenceType =
      options.mStorage.WasPassed()
          ? quota::PersistenceTypeFromStorageType(options.mStorage.Value())
          : quota::PERSISTENCE_TYPE_DEFAULT;

  RefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::Get();

  if (mgr) {
    nsresult rv =
        mgr->BlockAndGetFileReferences(persistenceType, origin, aDatabaseName,
                                       aId, aRefCnt, aDBRefCnt, aResult);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    *aRefCnt = *aDBRefCnt = -1;
    *aResult = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::FlushPendingFileDeletions() {
  RefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::Get();
  if (mgr) {
    nsresult rv = mgr->FlushPendingFileDeletions();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StartPCCountProfiling(JSContext* cx) {
  JS::StartPCCountProfiling(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StopPCCountProfiling(JSContext* cx) {
  JS::StopPCCountProfiling(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::PurgePCCounts(JSContext* cx) {
  JS::PurgePCCounts(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPCCountScriptCount(JSContext* cx, int32_t* result) {
  *result = JS::GetPCCountScriptCount(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPCCountScriptSummary(int32_t script, JSContext* cx,
                                          nsAString& result) {
  JSString* text = JS::GetPCCountScriptSummary(cx, script);
  if (!text) return NS_ERROR_FAILURE;

  if (!AssignJSString(cx, result, text)) return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPCCountScriptContents(int32_t script, JSContext* cx,
                                           nsAString& result) {
  JSString* text = JS::GetPCCountScriptContents(cx, script);
  if (!text) return NS_ERROR_FAILURE;

  if (!AssignJSString(cx, result, text)) return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPaintingSuppressed(bool* aPaintingSuppressed) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsIDocShell* docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  PresShell* presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  *aPaintingSuppressed = presShell->IsPaintingSuppressed();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetVisualViewportSize(float aWidth, float aHeight) {
  if (!(aWidth >= 0.0 && aHeight >= 0.0)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  presShell->SetVisualViewportSize(nsPresContext::CSSPixelsToAppUnits(aWidth),
                                   nsPresContext::CSSPixelsToAppUnits(aHeight));

  return NS_OK;
}

nsresult nsDOMWindowUtils::RemoteFrameFullscreenChanged(
    Element* aFrameElement) {
  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  doc->RemoteFrameFullscreenChanged(aFrameElement);
  return NS_OK;
}

nsresult nsDOMWindowUtils::RemoteFrameFullscreenReverted() {
  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  doc->RemoteFrameFullscreenReverted();
  return NS_OK;
}

static void PrepareForFullscreenChange(nsIDocShell* aDocShell,
                                       const nsSize& aSize,
                                       nsSize* aOldSize = nullptr) {
  if (!aDocShell) {
    return;
  }
  PresShell* presShell = aDocShell->GetPresShell();
  if (!presShell) {
    return;
  }
  if (nsRefreshDriver* rd = presShell->GetRefreshDriver()) {
    rd->SetIsResizeSuppressed();
    // Since we are suppressing the resize reflow which would originally
    // be triggered by view manager, we need to ensure that the refresh
    // driver actually schedules a flush, otherwise it may get stuck.
    rd->ScheduleViewManagerFlush();
  }
  if (!aSize.IsEmpty()) {
    nsCOMPtr<nsIContentViewer> cv;
    aDocShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      nsIntRect cvBounds;
      cv->GetBounds(cvBounds);
      nscoord auPerDev = presShell->GetPresContext()->AppUnitsPerDevPixel();
      if (aOldSize) {
        *aOldSize = LayoutDeviceIntSize::ToAppUnits(
            LayoutDeviceIntSize::FromUnknownSize(cvBounds.Size()), auPerDev);
      }
      LayoutDeviceIntSize newSize =
          LayoutDeviceIntSize::FromAppUnitsRounded(aSize, auPerDev);

      cvBounds.SizeTo(newSize.width, newSize.height);
      cv->SetBounds(cvBounds);
    }
  }
}

NS_IMETHODIMP
nsDOMWindowUtils::HandleFullscreenRequests(bool* aRetVal) {
  PROFILER_MARKER_UNTYPED("Enter fullscreen", DOM);
  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  // Notify the pres shell that we are starting fullscreen change, and
  // set the window dimensions in advance. Since the resize message
  // comes after the fullscreen change call, doing so could avoid an
  // extra resize reflow after this point.
  nsRect screenRect;
  if (nsPresContext* presContext = GetPresContext()) {
    presContext->DeviceContext()->GetRect(screenRect);
  }
  nsSize oldSize;
  PrepareForFullscreenChange(GetDocShell(), screenRect.Size(), &oldSize);
  OldWindowSize::Set(mWindow, oldSize);

  *aRetVal = Document::HandlePendingFullscreenRequests(doc);
  return NS_OK;
}

nsresult nsDOMWindowUtils::ExitFullscreen() {
  PROFILER_MARKER_UNTYPED("Exit fullscreen", DOM);
  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  // Although we would not use the old size if we have already exited
  // fullscreen, we still want to cleanup in case we haven't.
  nsSize oldSize = OldWindowSize::GetAndRemove(mWindow);
  if (!doc->GetFullscreenElement()) {
    return NS_OK;
  }

  // Notify the pres shell that we are starting fullscreen change, and
  // set the window dimensions in advance. Since the resize message
  // comes after the fullscreen change call, doing so could avoid an
  // extra resize reflow after this point.
  PrepareForFullscreenChange(GetDocShell(), oldSize);
  Document::ExitFullscreenInDocTree(doc);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SelectAtPoint(float aX, float aY, uint32_t aSelectBehavior,
                                bool* _retval) {
  *_retval = false;

  nsSelectionAmount amount;
  switch (aSelectBehavior) {
    case nsIDOMWindowUtils::SELECT_CHARACTER:
      amount = eSelectCharacter;
      break;
    case nsIDOMWindowUtils::SELECT_CLUSTER:
      amount = eSelectCluster;
      break;
    case nsIDOMWindowUtils::SELECT_WORD:
      amount = eSelectWord;
      break;
    case nsIDOMWindowUtils::SELECT_LINE:
      amount = eSelectLine;
      break;
    case nsIDOMWindowUtils::SELECT_BEGINLINE:
      amount = eSelectBeginLine;
      break;
    case nsIDOMWindowUtils::SELECT_ENDLINE:
      amount = eSelectEndLine;
      break;
    case nsIDOMWindowUtils::SELECT_PARAGRAPH:
      amount = eSelectParagraph;
      break;
    case nsIDOMWindowUtils::SELECT_WORDNOSPACE:
      amount = eSelectWordNoSpace;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }

  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_UNEXPECTED;
  }

  // The root frame for this content window
  nsIFrame* rootFrame = presShell->GetRootFrame();
  if (!rootFrame) {
    return NS_ERROR_UNEXPECTED;
  }

  // Get the target frame at the client coordinates passed to us
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  LayoutDeviceIntPoint pt =
      nsContentUtils::ToWidgetPoint(CSSPoint(aX, aY), offset, GetPresContext());
  nsPoint ptInRoot = nsLayoutUtils::GetEventCoordinatesRelativeTo(
      widget, pt, RelativeTo{rootFrame});
  nsIFrame* targetFrame =
      nsLayoutUtils::GetFrameForPoint(RelativeTo{rootFrame}, ptInRoot);
  // This can happen if the page hasn't loaded yet or if the point
  // is outside the frame.
  if (!targetFrame) {
    return NS_ERROR_INVALID_ARG;
  }

  // Convert point to coordinates relative to the target frame, which is
  // what targetFrame's SelectByTypeAtPoint expects.
  nsPoint relPoint = nsLayoutUtils::GetEventCoordinatesRelativeTo(
      widget, pt, RelativeTo{targetFrame});

  const RefPtr<nsPresContext> pinnedPresContext{GetPresContext()};
  nsresult rv = targetFrame->SelectByTypeAtPoint(
      pinnedPresContext, relPoint, amount, amount, nsIFrame::SELECT_ACCUMULATE);
  *_retval = !NS_FAILED(rv);
  return NS_OK;
}

static Document::additionalSheetType convertSheetType(uint32_t aSheetType) {
  switch (aSheetType) {
    case nsDOMWindowUtils::AGENT_SHEET:
      return Document::eAgentSheet;
    case nsDOMWindowUtils::USER_SHEET:
      return Document::eUserSheet;
    case nsDOMWindowUtils::AUTHOR_SHEET:
      return Document::eAuthorSheet;
    default:
      NS_ASSERTION(false, "wrong type");
      // we must return something although this should never happen
      return Document::AdditionalSheetTypeCount;
  }
}

NS_IMETHODIMP
nsDOMWindowUtils::LoadSheet(nsIURI* aSheetURI, uint32_t aSheetType) {
  NS_ENSURE_ARG_POINTER(aSheetURI);
  NS_ENSURE_ARG(aSheetType == AGENT_SHEET || aSheetType == USER_SHEET ||
                aSheetType == AUTHOR_SHEET);

  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  Document::additionalSheetType type = convertSheetType(aSheetType);

  return doc->LoadAdditionalStyleSheet(type, aSheetURI);
}

NS_IMETHODIMP
nsDOMWindowUtils::LoadSheetUsingURIString(const nsACString& aSheetURI,
                                          uint32_t aSheetType) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aSheetURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return LoadSheet(uri, aSheetType);
}

NS_IMETHODIMP
nsDOMWindowUtils::AddSheet(nsIPreloadedStyleSheet* aSheet,
                           uint32_t aSheetType) {
  NS_ENSURE_ARG_POINTER(aSheet);
  NS_ENSURE_ARG(aSheetType == AGENT_SHEET || aSheetType == USER_SHEET ||
                aSheetType == AUTHOR_SHEET);

  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  StyleSheet* sheet = nullptr;
  auto* preloadedSheet = static_cast<PreloadedStyleSheet*>(aSheet);
  nsresult rv = preloadedSheet->GetSheet(&sheet);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(sheet, NS_ERROR_FAILURE);

  if (sheet->GetAssociatedDocumentOrShadowRoot()) {
    return NS_ERROR_INVALID_ARG;
  }

  Document::additionalSheetType type = convertSheetType(aSheetType);
  return doc->AddAdditionalStyleSheet(type, sheet);
}

NS_IMETHODIMP
nsDOMWindowUtils::RemoveSheet(nsIURI* aSheetURI, uint32_t aSheetType) {
  NS_ENSURE_ARG_POINTER(aSheetURI);
  NS_ENSURE_ARG(aSheetType == AGENT_SHEET || aSheetType == USER_SHEET ||
                aSheetType == AUTHOR_SHEET);

  nsCOMPtr<Document> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  Document::additionalSheetType type = convertSheetType(aSheetType);

  doc->RemoveAdditionalStyleSheet(type, aSheetURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::RemoveSheetUsingURIString(const nsACString& aSheetURI,
                                            uint32_t aSheetType) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aSheetURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return RemoveSheet(uri, aSheetType);
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsHandlingUserInput(bool* aHandlingUserInput) {
  *aHandlingUserInput = UserActivation::IsHandlingUserInput();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetMillisSinceLastUserInput(
    double* aMillisSinceLastUserInput) {
  TimeStamp lastInput = UserActivation::LatestUserInputStart();
  if (lastInput.IsNull()) {
    *aMillisSinceLastUserInput = -1.0f;
    return NS_OK;
  }

  *aMillisSinceLastUserInput = (TimeStamp::Now() - lastInput).ToMilliseconds();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::AllowScriptsToClose() {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);
  nsGlobalWindowOuter::Cast(window)->AllowScriptsToClose();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsParentWindowMainWidgetVisible(bool* aIsVisible) {
  if (!XRE_IsParentProcess()) {
    MOZ_CRASH(
        "IsParentWindowMainWidgetVisible is only available in the parent "
        "process");
  }

  // this should reflect the "is parent window visible" logic in
  // nsWindowWatcher::OpenWindowInternal()
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  nsCOMPtr<nsIWidget> parentWidget;
  nsIDocShell* docShell = window->GetDocShell();
  if (docShell) {
    nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
    docShell->GetTreeOwner(getter_AddRefs(parentTreeOwner));
    nsCOMPtr<nsIBaseWindow> parentWindow(do_GetInterface(parentTreeOwner));
    if (parentWindow) {
      parentWindow->GetMainWidget(getter_AddRefs(parentWidget));
    }
  }
  if (!parentWidget) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aIsVisible = parentWidget->IsVisible();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::IsNodeDisabledForEvents(nsINode* aNode, bool* aRetVal) {
  *aRetVal = false;
  nsINode* node = aNode;
  while (node) {
    if (node->IsNodeOfType(nsINode::eHTML_FORM_CONTROL)) {
      nsGenericHTMLElement* element = nsGenericHTMLElement::FromNode(node);
      WidgetEvent event(true, eVoidEvent);
      if (element && element->IsDisabledForEvents(&event)) {
        *aRetVal = true;
        break;
      }
    }
    node = node->GetParentNode();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::DispatchEventToChromeOnly(EventTarget* aTarget, Event* aEvent,
                                            bool* aRetVal) {
  *aRetVal = false;
  NS_ENSURE_STATE(aTarget && aEvent);
  aEvent->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;
  *aRetVal =
      aTarget->DispatchEvent(*aEvent, CallerType::System, IgnoreErrors());
  return NS_OK;
}

static Result<nsIFrame*, nsresult> GetTargetFrame(
    const Element* aElement, const nsAString& aPseudoElement) {
  nsIFrame* frame = aElement->GetPrimaryFrame();
  if (!aPseudoElement.IsEmpty()) {
    if (aPseudoElement.EqualsLiteral("::before")) {
      frame = nsLayoutUtils::GetBeforeFrame(aElement);
    } else if (aPseudoElement.EqualsLiteral("::after")) {
      frame = nsLayoutUtils::GetAfterFrame(aElement);
    } else {
      return Err(NS_ERROR_INVALID_ARG);
    }
  }
  return frame;
}

static OMTAValue GetOMTAValue(nsIFrame* aFrame, DisplayItemType aDisplayItemKey,
                              WebRenderBridgeChild* aWebRenderBridgeChild) {
  OMTAValue value = mozilla::null_t();

  if (aWebRenderBridgeChild) {
    RefPtr<WebRenderAnimationData> animationData =
        GetWebRenderUserData<WebRenderAnimationData>(aFrame,
                                                     (uint32_t)aDisplayItemKey);
    if (animationData) {
      aWebRenderBridgeChild->SendGetAnimationValue(
          animationData->GetAnimationInfo().GetCompositorAnimationsId(),
          &value);
    }
  }
  return value;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetOMTAStyle(Element* aElement, const nsAString& aProperty,
                               const nsAString& aPseudoElement,
                               nsAString& aResult) {
  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  auto frameOrError = GetTargetFrame(aElement, aPseudoElement);
  if (frameOrError.isErr()) {
    return frameOrError.unwrapErr();
  }
  nsIFrame* frame = frameOrError.unwrap();

  RefPtr<nsROCSSPrimitiveValue> cssValue = nullptr;
  if (frame && nsLayoutUtils::AreAsyncAnimationsEnabled()) {
    if (aProperty.EqualsLiteral("opacity")) {
      OMTAValue value = GetOMTAValue(frame, DisplayItemType::TYPE_OPACITY,
                                     GetWebRenderBridge());
      if (value.type() == OMTAValue::Tfloat) {
        cssValue = new nsROCSSPrimitiveValue;
        cssValue->SetNumber(value.get_float());
      }
    } else if (aProperty.EqualsLiteral("transform") ||
               aProperty.EqualsLiteral("translate") ||
               aProperty.EqualsLiteral("rotate") ||
               aProperty.EqualsLiteral("scale") ||
               aProperty.EqualsLiteral("offset-path") ||
               aProperty.EqualsLiteral("offset-distance") ||
               aProperty.EqualsLiteral("offset-rotate") ||
               aProperty.EqualsLiteral("offset-anchor")) {
      OMTAValue value = GetOMTAValue(frame, DisplayItemType::TYPE_TRANSFORM,
                                     GetWebRenderBridge());
      if (value.type() == OMTAValue::TMatrix4x4) {
        cssValue = nsComputedDOMStyle::MatrixToCSSValue(value.get_Matrix4x4());
      }
    } else if (aProperty.EqualsLiteral("background-color")) {
      OMTAValue value = GetOMTAValue(
          frame, DisplayItemType::TYPE_BACKGROUND_COLOR, GetWebRenderBridge());
      if (value.type() == OMTAValue::Tnscolor) {
        nsStyleUtil::GetSerializedColorValue(value.get_nscolor(), aResult);
        return NS_OK;
      }
    }
  }

  if (cssValue) {
    nsString text;
    ErrorResult rv;
    cssValue->GetCssText(text, rv);
    aResult.Assign(text);
    return rv.StealNSResult();
  }
  aResult.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::IsAnimationInPendingTracker(dom::Animation* aAnimation,
                                              bool* aRetVal) {
  MOZ_ASSERT(aRetVal);

  if (!aAnimation) {
    return NS_ERROR_INVALID_ARG;
  }

  Document* doc = GetDocument();
  if (!doc) {
    *aRetVal = false;
    return NS_OK;
  }

  PendingAnimationTracker* tracker = doc->GetPendingAnimationTracker();
  if (!tracker) {
    *aRetVal = false;
    return NS_OK;
  }

  *aRetVal = tracker->IsWaitingToPlay(*aAnimation) ||
             tracker->IsWaitingToPause(*aAnimation);
  return NS_OK;
}

namespace {

class HandlingUserInputHelper final : public nsIJSRAIIHelper {
 public:
  explicit HandlingUserInputHelper(bool aHandlingUserInput);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIJSRAIIHELPER

 private:
  ~HandlingUserInputHelper();

  bool mHandlingUserInput;
  bool mDestructCalled;
};

NS_IMPL_ISUPPORTS(HandlingUserInputHelper, nsIJSRAIIHelper)

HandlingUserInputHelper::HandlingUserInputHelper(bool aHandlingUserInput)
    : mHandlingUserInput(aHandlingUserInput), mDestructCalled(false) {
  if (aHandlingUserInput) {
    UserActivation::StartHandlingUserInput(eVoidEvent);
  }
}

HandlingUserInputHelper::~HandlingUserInputHelper() {
  // We assert, but just in case, make sure we notify the ESM.
  MOZ_ASSERT(mDestructCalled);
  if (!mDestructCalled) {
    Destruct();
  }
}

NS_IMETHODIMP
HandlingUserInputHelper::Destruct() {
  if (NS_WARN_IF(mDestructCalled)) {
    return NS_ERROR_FAILURE;
  }

  mDestructCalled = true;
  if (mHandlingUserInput) {
    UserActivation::StopHandlingUserInput(eVoidEvent);
  }

  return NS_OK;
}

}  // unnamed namespace

NS_IMETHODIMP
nsDOMWindowUtils::SetHandlingUserInput(bool aHandlingUserInput,
                                       nsIJSRAIIHelper** aHelper) {
  RefPtr<HandlingUserInputHelper> helper(
      new HandlingUserInputHelper(aHandlingUserInput));
  helper.forget(aHelper);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetContentAPZTestData(
    JSContext* aContext, JS::MutableHandleValue aOutContentTestData) {
  if (nsIWidget* widget = GetWidget()) {
    WindowRenderer* renderer = widget->GetWindowRenderer();
    if (!renderer) {
      return NS_OK;
    }
    if (WebRenderLayerManager* wr = renderer->AsWebRender()) {
      if (!wr->GetAPZTestData().ToJS(aOutContentTestData, aContext)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetCompositorAPZTestData(
    JSContext* aContext, JS::MutableHandleValue aOutCompositorTestData) {
  if (nsIWidget* widget = GetWidget()) {
    WindowRenderer* renderer = widget->GetWindowRenderer();
    if (!renderer) {
      return NS_OK;
    }
    APZTestData compositorSideData;
    if (WebRenderLayerManager* wr = renderer->AsWebRender()) {
      if (!wr->WrBridge()) {
        return NS_ERROR_UNEXPECTED;
      }
      if (!wr->WrBridge()->SendGetAPZTestData(&compositorSideData)) {
        return NS_ERROR_FAILURE;
      }
    }
    if (!compositorSideData.ToJS(aOutCompositorTestData, aContext)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::PostRestyleSelfEvent(Element* aElement) {
  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  nsLayoutUtils::PostRestyleEvent(aElement, RestyleHint::RESTYLE_SELF,
                                  nsChangeHint(0));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetChromeMargin(int32_t aTop, int32_t aRight, int32_t aBottom,
                                  int32_t aLeft) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  if (window) {
    nsCOMPtr<nsIBaseWindow> baseWindow =
        do_QueryInterface(window->GetDocShell());
    if (baseWindow) {
      nsCOMPtr<nsIWidget> widget;
      baseWindow->GetMainWidget(getter_AddRefs(widget));
      if (widget) {
        LayoutDeviceIntMargin margins(aTop, aRight, aBottom, aLeft);
        return widget->SetNonClientMargins(margins);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFrameUniformityTestData(
    JSContext* aContext, JS::MutableHandleValue aOutFrameUniformity) {
  nsIWidget* widget = GetWidget();
  if (!widget) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  WindowRenderer* renderer = widget->GetWindowRenderer();
  if (!renderer) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  FrameUniformityData outData;
  renderer->GetFrameUniformity(&outData);
  outData.ToJS(aOutFrameUniformity, aContext);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::XpconnectArgument(nsISupports* aObj) {
  // Do nothing.
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::AskPermission(nsIContentPermissionRequest* aRequest) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  return nsContentPermissionUtils::AskPermission(
      aRequest, window->GetCurrentInnerWindow());
}

NS_IMETHODIMP
nsDOMWindowUtils::GetRestyleGeneration(uint64_t* aResult) {
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aResult = presContext->GetRestyleGeneration();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFramesConstructed(uint64_t* aResult) {
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aResult = presContext->FramesConstructedCount();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFramesReflowed(uint64_t* aResult) {
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aResult = presContext->FramesReflowedCount();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::EnterChaosMode() {
  ChaosMode::enterChaosMode();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::LeaveChaosMode() {
  ChaosMode::leaveChaosMode();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::TriggerDeviceReset() {
  ContentChild* cc = ContentChild::GetSingleton();
  if (cc) {
    cc->SendDeviceReset();
    return NS_OK;
  }

  GPUProcessManager* pm = GPUProcessManager::Get();
  if (pm) {
    pm->SimulateDeviceReset();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::HasRuleProcessorUsedByMultipleStyleSets(uint32_t aSheetType,
                                                          bool* aRetVal) {
  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  return presShell->HasRuleProcessorUsedByMultipleStyleSets(aSheetType,
                                                            aRetVal);
}

NS_IMETHODIMP
nsDOMWindowUtils::RespectDisplayPortSuppression(bool aEnabled) {
  RefPtr<PresShell> presShell = GetPresShell();
  presShell->RespectDisplayportSuppression(aEnabled);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ForceReflowInterrupt() {
  nsPresContext* pc = GetPresContext();
  if (!pc) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  pc->SetPendingInterruptFromTest();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::TerminateGPUProcess() {
  GPUProcessManager* pm = GPUProcessManager::Get();
  if (pm) {
    pm->KillProcess();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetGpuProcessPid(int32_t* aPid) {
  GPUProcessManager* pm = GPUProcessManager::Get();
  if (pm) {
    *aPid = pm->GPUProcessPid();
  } else {
    *aPid = -1;
  }

  return NS_OK;
}

struct StateTableEntry {
  const char* mStateString;
  EventStates mState;
};

static constexpr StateTableEntry kManuallyManagedStates[] = {
    {"autofill", NS_EVENT_STATE_AUTOFILL},
    // :-moz-autofill-preview implies :autofill.
    {"-moz-autofill-preview",
     NS_EVENT_STATE_AUTOFILL_PREVIEW | NS_EVENT_STATE_AUTOFILL},
    {nullptr, EventStates()},
};

static_assert(!kManuallyManagedStates[ArrayLength(kManuallyManagedStates) - 1]
                   .mStateString,
              "last kManuallyManagedStates entry must be a sentinel with "
              "mStateString == nullptr");

static EventStates GetEventStateForString(const nsAString& aStateString) {
  for (const StateTableEntry* entry = kManuallyManagedStates;
       entry->mStateString; ++entry) {
    if (aStateString.EqualsASCII(entry->mStateString)) {
      return entry->mState;
    }
  }
  return EventStates();
}

NS_IMETHODIMP
nsDOMWindowUtils::AddManuallyManagedState(Element* aElement,
                                          const nsAString& aStateString) {
  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  EventStates state = GetEventStateForString(aStateString);
  if (state.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  aElement->AddManuallyManagedStates(state);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::RemoveManuallyManagedState(Element* aElement,
                                             const nsAString& aStateString) {
  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  EventStates state = GetEventStateForString(aStateString);
  if (state.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  aElement->RemoveManuallyManagedStates(state);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetStorageUsage(Storage* aStorage, int64_t* aRetval) {
  if (!aStorage) {
    return NS_ERROR_UNEXPECTED;
  }

  *aRetval = aStorage->GetOriginQuotaUsage();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetDirectionFromText(const nsAString& aString,
                                       int32_t* aRetval) {
  Directionality dir =
      ::GetDirectionFromText(aString.BeginReading(), aString.Length(), nullptr);
  switch (dir) {
    case eDir_NotSet:
      *aRetval = nsIDOMWindowUtils::DIRECTION_NOT_SET;
      break;
    case eDir_RTL:
      *aRetval = nsIDOMWindowUtils::DIRECTION_RTL;
      break;
    case eDir_LTR:
      *aRetval = nsIDOMWindowUtils::DIRECTION_LTR;
      break;
    case eDir_Auto:
      MOZ_ASSERT_UNREACHABLE(
          "GetDirectionFromText should never return this value");
      return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::EnsureDirtyRootFrame() {
  Document* doc = GetDocument();
  PresShell* presShell = doc ? doc->GetPresShell() : nullptr;

  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame* frame = presShell->GetRootFrame();
  if (!frame) {
    return NS_ERROR_FAILURE;
  }

  presShell->FrameNeedsReflow(frame, IntrinsicDirty::StyleChange,
                              NS_FRAME_IS_DIRTY);
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsTranslationNodeList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsITranslationNodeList)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsTranslationNodeList)
NS_IMPL_RELEASE(nsTranslationNodeList)

NS_IMETHODIMP
nsTranslationNodeList::Item(uint32_t aIndex, nsINode** aRetVal) {
  NS_ENSURE_ARG_POINTER(aRetVal);
  NS_IF_ADDREF(*aRetVal = mNodes.SafeElementAt(aIndex));
  return NS_OK;
}

NS_IMETHODIMP
nsTranslationNodeList::IsTranslationRootAtIndex(uint32_t aIndex,
                                                bool* aRetVal) {
  NS_ENSURE_ARG_POINTER(aRetVal);
  if (aIndex >= mLength) {
    *aRetVal = false;
    return NS_OK;
  }

  *aRetVal = mNodeIsRoot.ElementAt(aIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsTranslationNodeList::GetLength(uint32_t* aRetVal) {
  NS_ENSURE_ARG_POINTER(aRetVal);
  *aRetVal = mLength;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::WrCapture() {
  if (WebRenderBridgeChild* wrbc = GetWebRenderBridge()) {
    wrbc->Capture();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::WrStartCaptureSequence(const nsACString& aPath,
                                         uint32_t aFlags) {
  if (WebRenderBridgeChild* wrbc = GetWebRenderBridge()) {
    wrbc->StartCaptureSequence(nsCString(aPath), aFlags);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::WrStopCaptureSequence() {
  if (WebRenderBridgeChild* wrbc = GetWebRenderBridge()) {
    wrbc->StopCaptureSequence();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetCompositionRecording(bool aValue, Promise** aOutPromise) {
  return aValue ? StartCompositionRecording(aOutPromise)
                : StopCompositionRecording(true, aOutPromise);
}

NS_IMETHODIMP
nsDOMWindowUtils::StartCompositionRecording(Promise** aOutPromise) {
  NS_ENSURE_ARG(aOutPromise);
  *aOutPromise = nullptr;

  nsCOMPtr<nsPIDOMWindowOuter> outer = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(outer);
  nsCOMPtr<nsPIDOMWindowInner> inner = outer->GetCurrentInnerWindow();
  NS_ENSURE_STATE(inner);

  ErrorResult err;
  RefPtr<Promise> promise = Promise::Create(inner->AsGlobal(), err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  CompositorBridgeChild* cbc = GetCompositorBridge();
  if (NS_WARN_IF(!cbc)) {
    promise->MaybeReject(NS_ERROR_UNEXPECTED);
  } else {
    cbc->SendBeginRecording(TimeStamp::Now())
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [promise](const bool& aSuccess) {
              if (aSuccess) {
                promise->MaybeResolve(true);
              } else {
                promise->MaybeRejectWithInvalidStateError(
                    "The composition recorder is already running.");
              }
            },
            [promise](const mozilla::ipc::ResponseRejectReason&) {
              promise->MaybeRejectWithInvalidStateError(
                  "Could not start the composition recorder.");
            });
  }

  promise.forget(aOutPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StopCompositionRecording(bool aWriteToDisk,
                                           Promise** aOutPromise) {
  NS_ENSURE_ARG_POINTER(aOutPromise);
  *aOutPromise = nullptr;

  nsCOMPtr<nsPIDOMWindowOuter> outer = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(outer);
  nsCOMPtr<nsPIDOMWindowInner> inner = outer->GetCurrentInnerWindow();
  NS_ENSURE_STATE(inner);

  ErrorResult err;
  RefPtr<Promise> promise = Promise::Create(inner->AsGlobal(), err);
  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }

  CompositorBridgeChild* cbc = GetCompositorBridge();
  if (NS_WARN_IF(!cbc)) {
    promise->MaybeReject(NS_ERROR_UNEXPECTED);
  } else if (aWriteToDisk) {
    cbc->SendEndRecordingToDisk()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [promise](const bool& aSuccess) {
          if (aSuccess) {
            promise->MaybeResolveWithUndefined();
          } else {
            promise->MaybeRejectWithInvalidStateError(
                "The composition recorder is not running.");
          }
        },
        [promise](const mozilla::ipc::ResponseRejectReason&) {
          promise->MaybeRejectWithUnknownError(
              "Could not stop the composition recorder.");
        });
  } else {
    cbc->SendEndRecordingToMemory()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [promise](Maybe<CollectedFramesParams>&& aFrames) {
          if (!aFrames) {
            promise->MaybeRejectWithUnknownError(
                "Could not stop the composition recorder.");
            return;
          }

          DOMCollectedFrames domFrames;
          if (!domFrames.mFrames.SetCapacity(aFrames->frames().Length(),
                                             fallible)) {
            promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
            return;
          }

          domFrames.mRecordingStart = aFrames->recordingStart();

          CheckedInt<size_t> totalSize(0);
          for (const CollectedFrameParams& frame : aFrames->frames()) {
            totalSize += frame.length();
          }

          if (totalSize.isValid() &&
              totalSize.value() > aFrames->buffer().Size<char>()) {
            promise->MaybeRejectWithUnknownError(
                "Could not interpret returned frames.");
            return;
          }

          Span<const char> buffer(aFrames->buffer().get<char>(),
                                  aFrames->buffer().Size<char>());

          for (const CollectedFrameParams& frame : aFrames->frames()) {
            size_t length = frame.length();

            Span<const char> dataUri = buffer.First(length);

            // We have to do a fallible AppendElement() because WebIDL
            // dictionaries use FallibleTArray. However, this cannot fail due to
            // the pre-allocation earlier.
            DOMCollectedFrame* domFrame =
                domFrames.mFrames.AppendElement(fallible);
            MOZ_ASSERT(domFrame);

            domFrame->mTimeOffset = frame.timeOffset();
            domFrame->mDataUri = nsCString(dataUri);

            buffer = buffer.Subspan(length);
          }

          promise->MaybeResolve(domFrames);
        },
        [promise](const mozilla::ipc::ResponseRejectReason&) {
          promise->MaybeRejectWithUnknownError(
              "Could not stop the composition recorder.");
        });
  }

  promise.forget(aOutPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetSystemFont(const nsACString& aFontName) {
  nsIWidget* widget = GetWidget();
  if (!widget) {
    return NS_OK;
  }

  nsAutoCString fontName(aFontName);
  return widget->SetSystemFont(fontName);
}

NS_IMETHODIMP
nsDOMWindowUtils::GetSystemFont(nsACString& aFontName) {
  nsIWidget* widget = GetWidget();
  if (!widget) {
    return NS_OK;
  }

  nsAutoCString fontName;
  widget->GetSystemFont(fontName);
  aFontName.Assign(fontName);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::IsCssPropertyRecordedInUseCounter(const nsACString& aPropName,
                                                    bool* aRecorded) {
  *aRecorded = false;

  Document* doc = GetDocument();
  if (!doc || !doc->GetStyleUseCounters()) {
    return NS_ERROR_FAILURE;
  }

  bool knownProp = false;
  *aRecorded = Servo_IsCssPropertyRecordedInUseCounter(
      doc->GetStyleUseCounters(), &aPropName, &knownProp);
  return knownProp ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetLayersId(uint64_t* aOutLayersId) {
  nsIWidget* widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }
  BrowserChild* child = widget->GetOwningBrowserChild();
  if (!child) {
    return NS_ERROR_FAILURE;
  }
  *aOutLayersId = (uint64_t)child->GetLayersId();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPaintCount(uint64_t* aPaintCount) {
  auto* presShell = GetPresShell();
  *aPaintCount = presShell ? presShell->GetPaintCount() : 0;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetWebrtcRawDeviceId(nsAString& aRawDeviceId) {
  if (!XRE_IsParentProcess()) {
    MOZ_CRASH(
        "GetWebrtcRawDeviceId is only available in the parent "
        "process");
  }

  nsIWidget* widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  int64_t rawDeviceId =
      (int64_t)(widget->GetNativeData(NS_NATIVE_WINDOW_WEBRTC_DEVICE_ID));
  if (!rawDeviceId) {
    return NS_ERROR_FAILURE;
  }

  aRawDeviceId.AppendInt(rawDeviceId);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetEffectivelyThrottlesFrameRequests(bool* aResult) {
  Document* doc = GetDocument();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }
  *aResult = !doc->WouldScheduleFrameRequestCallbacks() ||
             doc->ShouldThrottleFrameRequests();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ResetMobileViewportManager() {
  if (RefPtr<PresShell> presShell = GetPresShell()) {
    if (auto mvm = presShell->GetMobileViewportManager()) {
      mvm->SetInitialViewport();
      return NS_OK;
    }
  }
  // Unable to reset, so let's error out
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetSuspendedByBrowsingContextGroup(bool* aResult) {
  nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindowInner> inner = window->GetCurrentInnerWindow();
  NS_ENSURE_TRUE(inner, NS_ERROR_FAILURE);

  *aResult = inner->GetWasSuspendedByGroup();
  return NS_OK;
}
