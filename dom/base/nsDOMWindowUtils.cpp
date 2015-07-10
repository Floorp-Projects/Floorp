/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMWindowUtils.h"

#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "nsPresContext.h"
#include "nsDOMClassInfoID.h"
#include "nsError.h"
#include "nsIDOMEvent.h"
#include "nsQueryContentEventResult.h"
#include "nsGlobalWindow.h"
#include "nsIDocument.h"
#include "nsFocusManager.h"
#include "nsFrameManager.h"
#include "nsRefreshDriver.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/PendingAnimationTracker.h"
#include "nsIObjectLoadingContent.h"
#include "nsFrame.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "ClientLayerManager.h"
#include "nsQueryObject.h"
#ifdef MOZ_FMP4
#include "MP4Decoder.h"
#endif

#include "nsIScrollableFrame.h"

#include "nsContentUtils.h"

#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsCharsetSource.h"
#include "nsJSEnvironment.h"
#include "nsJSUtils.h"

#include "mozilla/ChaosMode.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TouchEvents.h"

#include "nsViewManager.h"

#include "nsIDOMHTMLCanvasElement.h"
#include "nsLayoutUtils.h"
#include "nsComputedDOMStyle.h"
#include "nsIPresShell.h"
#include "nsCSSProps.h"
#include "nsTArrayHelpers.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/DOMRect.h"
#include <algorithm>

#if defined(MOZ_X11) && defined(MOZ_WIDGET_GTK)
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#endif

#include "Layers.h"
#include "gfxPrefs.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/IDBMutableFileBinding.h"
#include "mozilla/dom/indexedDB/IDBMutableFile.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/layers/FrameUniformityData.h"
#include "mozilla/layers/ShadowLayers.h"
#include "nsPrintfCString.h"
#include "nsViewportInfo.h"
#include "nsIFormControl.h"
#include "nsIScriptError.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"
#include "FrameLayerBuilder.h"
#include "nsDisplayList.h"
#include "nsROCSSPrimitiveValue.h"
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestorUtils.h"
#include "GeckoProfiler.h"
#include "mozilla/Preferences.h"
#include "nsIContentIterator.h"
#include "nsIDOMStyleSheet.h"
#include "nsIStyleSheet.h"
#include "nsIStyleSheetService.h"
#include "nsContentPermissionHelper.h"
#include "nsNetUtil.h"

#ifdef XP_WIN
#undef GetClassName
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla::gfx;

class gfxContext;

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

NS_INTERFACE_MAP_BEGIN(nsDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMWindowUtils)
NS_IMPL_RELEASE(nsDOMWindowUtils)

nsDOMWindowUtils::nsDOMWindowUtils(nsGlobalWindow *aWindow)
{
  nsCOMPtr<nsISupports> supports = do_QueryObject(aWindow);
  mWindow = do_GetWeakReference(supports);
  NS_ASSERTION(aWindow->IsOuterWindow(), "How did that happen?");
}

nsDOMWindowUtils::~nsDOMWindowUtils()
{
}

nsIPresShell*
nsDOMWindowUtils::GetPresShell()
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  if (!window)
    return nullptr;

  nsIDocShell *docShell = window->GetDocShell();
  if (!docShell)
    return nullptr;

  return docShell->GetPresShell();
}

nsPresContext*
nsDOMWindowUtils::GetPresContext()
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  if (!window)
    return nullptr;
  nsIDocShell *docShell = window->GetDocShell();
  if (!docShell)
    return nullptr;
  nsRefPtr<nsPresContext> presContext;
  docShell->GetPresContext(getter_AddRefs(presContext));
  return presContext;
}

nsIDocument*
nsDOMWindowUtils::GetDocument()
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  if (!window) {
    return nullptr;
  }
  return window->GetExtantDoc();
}

LayerTransactionChild*
nsDOMWindowUtils::GetLayerTransaction()
{
  nsIWidget* widget = GetWidget();
  if (!widget)
    return nullptr;

  LayerManager* manager = widget->GetLayerManager();
  if (!manager)
    return nullptr;

  ShadowLayerForwarder* forwarder = manager->AsShadowForwarder();
  return forwarder && forwarder->HasShadowManager() ?
         forwarder->GetShadowManager() :
         nullptr;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetImageAnimationMode(uint16_t *aMode)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

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
nsDOMWindowUtils::SetImageAnimationMode(uint16_t aMode)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    presContext->SetImageAnimationMode(aMode);
    return NS_OK;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetDocCharsetIsForced(bool *aIsForced)
{
  *aIsForced = false;

  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsIDocument* doc = GetDocument();
  *aIsForced = doc &&
    doc->GetDocumentCharacterSetSource() >= kCharsetFromParentForced;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetDocumentMetadata(const nsAString& aName,
                                      nsAString& aValue)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsIDocument* doc = GetDocument();
  if (doc) {
    nsCOMPtr<nsIAtom> name = do_GetAtom(aName);
    doc->GetHeaderData(name, aValue);
    return NS_OK;
  }

  aValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::Redraw(uint32_t aCount, uint32_t *aDurationOut)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (aCount == 0)
    aCount = 1;

  if (nsIPresShell* presShell = GetPresShell()) {
    nsIFrame *rootFrame = presShell->GetRootFrame();

    if (rootFrame) {
      PRIntervalTime iStart = PR_IntervalNow();

      for (uint32_t i = 0; i < aCount; i++)
        rootFrame->InvalidateFrame();

#if defined(MOZ_X11) && defined(MOZ_WIDGET_GTK)
      XSync(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), False);
#endif

      *aDurationOut = PR_IntervalToMilliseconds(PR_IntervalNow() - iStart);

      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::UpdateLayerTree()
{
  if (nsIPresShell* presShell = GetPresShell()) {
    presShell->FlushPendingNotifications(Flush_Display);
    nsRefPtr<nsViewManager> vm = presShell->GetViewManager();
    nsView* view = vm->GetRootView();
    if (view) {
      presShell->Paint(view, view->GetBounds(),
          nsIPresShell::PAINT_LAYERS | nsIPresShell::PAINT_SYNC_DECODE_IMAGES);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetCSSViewport(float aWidthPx, float aHeightPx)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (!(aWidthPx >= 0.0 && aHeightPx >= 0.0)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  nsLayoutUtils::SetCSSViewport(presShell, CSSSize(aWidthPx, aHeightPx));

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetViewportInfo(uint32_t aDisplayWidth,
                                  uint32_t aDisplayHeight,
                                  double *aDefaultZoom, bool *aAllowZoom,
                                  double *aMinZoom, double *aMaxZoom,
                                  uint32_t *aWidth, uint32_t *aHeight,
                                  bool *aAutoSize)
{
  nsIDocument* doc = GetDocument();
  NS_ENSURE_STATE(doc);

  nsViewportInfo info = nsContentUtils::GetViewportInfo(doc, ScreenIntSize(aDisplayWidth, aDisplayHeight));
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
nsDOMWindowUtils::SetDisplayPortForElement(float aXPx, float aYPx,
                                           float aWidthPx, float aHeightPx,
                                           nsIDOMElement* aElement,
                                           uint32_t aPriority)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);

  if (!content) {
    return NS_ERROR_INVALID_ARG;
  }

  if (content->GetCurrentDoc() != presShell->GetDocument()) {
    return NS_ERROR_INVALID_ARG;
  }

  DisplayPortPropertyData* currentData =
    static_cast<DisplayPortPropertyData*>(content->GetProperty(nsGkAtoms::DisplayPort));
  if (currentData && currentData->mPriority > aPriority) {
    return NS_OK;
  }

  nsRect displayport(nsPresContext::CSSPixelsToAppUnits(aXPx),
                     nsPresContext::CSSPixelsToAppUnits(aYPx),
                     nsPresContext::CSSPixelsToAppUnits(aWidthPx),
                     nsPresContext::CSSPixelsToAppUnits(aHeightPx));

  content->SetProperty(nsGkAtoms::DisplayPort,
                       new DisplayPortPropertyData(displayport, aPriority),
                       nsINode::DeleteProperty<DisplayPortPropertyData>);

  if (gfxPrefs::LayoutUseContainersForRootFrames()) {
    nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
    if (rootScrollFrame &&
        content == rootScrollFrame->GetContent() &&
        nsLayoutUtils::UsesAsyncScrolling(rootScrollFrame))
    {
      // We are setting a root displayport for a document.
      // The pres shell needs a special flag set.
      presShell->SetIgnoreViewportScrolling(true);
    }
  }

  nsIFrame* rootFrame = presShell->FrameManager()->GetRootFrame();
  if (rootFrame) {
    rootFrame->SchedulePaint();

    // If we are hiding something that is a display root then send empty paint
    // transaction in order to release retained layers because it won't get
    // any more paint requests when it is hidden.
    if (displayport.IsEmpty() &&
        rootFrame == nsLayoutUtils::GetDisplayRootFrame(rootFrame)) {
      nsCOMPtr<nsIWidget> widget = GetWidget();
      if (widget) {
        bool isRetainingManager;
        LayerManager* manager = widget->GetLayerManager(&isRetainingManager);
        if (isRetainingManager) {
          manager->BeginTransaction();
          nsLayoutUtils::PaintFrame(nullptr, rootFrame, nsRegion(), NS_RGB(255, 255, 255),
                                    nsLayoutUtils::PAINT_WIDGET_LAYERS |
                                    nsLayoutUtils::PAINT_EXISTING_TRANSACTION);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetDisplayPortMarginsForElement(float aLeftMargin,
                                                  float aTopMargin,
                                                  float aRightMargin,
                                                  float aBottomMargin,
                                                  nsIDOMElement* aElement,
                                                  uint32_t aPriority)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);

  if (!content) {
    return NS_ERROR_INVALID_ARG;
  }

  if (content->GetCurrentDoc() != presShell->GetDocument()) {
    return NS_ERROR_INVALID_ARG;
  }

  // Note order change of arguments between our function signature and
  // ScreenMargin constructor.
  ScreenMargin displayportMargins(aTopMargin,
                                  aRightMargin,
                                  aBottomMargin,
                                  aLeftMargin);

  nsLayoutUtils::SetDisplayPortMargins(content, presShell, displayportMargins,
                                       aPriority);

  return NS_OK;
}


NS_IMETHODIMP
nsDOMWindowUtils::SetDisplayPortBaseForElement(int32_t aX,
                                               int32_t aY,
                                               int32_t aWidth,
                                               int32_t aHeight,
                                               nsIDOMElement* aElement)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);

  if (!content) {
    return NS_ERROR_INVALID_ARG;
  }

  if (content->GetCurrentDoc() != presShell->GetDocument()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsLayoutUtils::SetDisplayPortBase(content, nsRect(aX, aY, aWidth, aHeight));

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetResolution(float aResolution)
{
  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
  if (sf) {
    sf->SetResolution(aResolution);
    presShell->SetResolution(aResolution);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetResolutionAndScaleTo(float aResolution)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  nsLayoutUtils::SetResolutionAndScaleTo(presShell, aResolution);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetResolution(float* aResolution)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  *aResolution = nsLayoutUtils::GetResolution(presShell);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsResolutionSet(bool* aIsResolutionSet) {
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  const nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
  *aIsResolutionSet = sf && sf->IsResolutionSet();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetIsFirstPaint(bool aIsFirstPaint)
{
  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    presShell->SetIsFirstPaint(aIsFirstPaint);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsFirstPaint(bool *aIsFirstPaint)
{
  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    *aIsFirstPaint = presShell->GetIsFirstPaint();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPresShellId(uint32_t *aPresShellId)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    *aPresShellId = presShell->GetPresShellId();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendMouseEvent(const nsAString& aType,
                                 float aX,
                                 float aY,
                                 int32_t aButton,
                                 int32_t aClickCount,
                                 int32_t aModifiers,
                                 bool aIgnoreRootScrollFrame,
                                 float aPressure,
                                 unsigned short aInputSourceArg,
                                 bool aIsSynthesized,
                                 uint8_t aOptionalArgCount,
                                 bool *aPreventDefault)
{
  return SendMouseEventCommon(aType, aX, aY, aButton, aClickCount, aModifiers,
                              aIgnoreRootScrollFrame, aPressure,
                              aInputSourceArg, false, aPreventDefault,
                              aOptionalArgCount >= 4 ? aIsSynthesized : true);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendMouseEventToWindow(const nsAString& aType,
                                         float aX,
                                         float aY,
                                         int32_t aButton,
                                         int32_t aClickCount,
                                         int32_t aModifiers,
                                         bool aIgnoreRootScrollFrame,
                                         float aPressure,
                                         unsigned short aInputSourceArg,
                                         bool aIsSynthesized,
                                         uint8_t aOptionalArgCount)
{
  PROFILER_LABEL("nsDOMWindowUtils", "SendMouseEventToWindow",
    js::ProfileEntry::Category::EVENTS);

  return SendMouseEventCommon(aType, aX, aY, aButton, aClickCount, aModifiers,
                              aIgnoreRootScrollFrame, aPressure,
                              aInputSourceArg, true, nullptr,
                              aOptionalArgCount >= 4 ? aIsSynthesized : true);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendMouseEventCommon(const nsAString& aType,
                                       float aX,
                                       float aY,
                                       int32_t aButton,
                                       int32_t aClickCount,
                                       int32_t aModifiers,
                                       bool aIgnoreRootScrollFrame,
                                       float aPressure,
                                       unsigned short aInputSourceArg,
                                       bool aToWindow,
                                       bool *aPreventDefault,
                                       bool aIsSynthesized)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  return nsContentUtils::SendMouseEvent(presShell, aType, aX, aY, aButton,
      aClickCount, aModifiers, aIgnoreRootScrollFrame, aPressure,
      aInputSourceArg, aToWindow, aPreventDefault, aIsSynthesized);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendPointerEventCommon(const nsAString& aType,
                                         float aX,
                                         float aY,
                                         int32_t aButton,
                                         int32_t aClickCount,
                                         int32_t aModifiers,
                                         bool aIgnoreRootScrollFrame,
                                         float aPressure,
                                         unsigned short aInputSourceArg,
                                         int32_t aPointerId,
                                         int32_t aWidth,
                                         int32_t aHeight,
                                         int32_t aTiltX,
                                         int32_t aTiltY,
                                         bool aIsPrimary,
                                         bool aIsSynthesized,
                                         uint8_t aOptionalArgCount,
                                         bool aToWindow,
                                         bool* aPreventDefault)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  int32_t msg;
  if (aType.EqualsLiteral("pointerdown")) {
    msg = NS_POINTER_DOWN;
  } else if (aType.EqualsLiteral("pointerup")) {
    msg = NS_POINTER_UP;
  } else if (aType.EqualsLiteral("pointermove")) {
    msg = NS_POINTER_MOVE;
  } else if (aType.EqualsLiteral("pointerover")) {
    msg = NS_POINTER_OVER;
  } else if (aType.EqualsLiteral("pointerout")) {
    msg = NS_POINTER_OUT;
  } else {
    return NS_ERROR_FAILURE;
  }

  if (aInputSourceArg == nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN) {
    aInputSourceArg = nsIDOMMouseEvent::MOZ_SOURCE_MOUSE;
  }

  WidgetPointerEvent event(true, msg, widget);
  event.modifiers = nsContentUtils::GetWidgetModifiers(aModifiers);
  event.button = aButton;
  event.buttons = nsContentUtils::GetButtonsFlagForButton(aButton);
  event.widget = widget;
  event.pressure = aPressure;
  event.inputSource = aInputSourceArg;
  event.pointerId = aPointerId;
  event.width = aWidth;
  event.height = aHeight;
  event.tiltX = aTiltX;
  event.tiltY = aTiltY;
  event.isPrimary = (nsIDOMMouseEvent::MOZ_SOURCE_MOUSE == aInputSourceArg) ? true : aIsPrimary;
  event.clickCount = aClickCount;
  event.time = PR_IntervalNow();
  event.mFlags.mIsSynthesizedForTests = aOptionalArgCount >= 10 ? aIsSynthesized : true;

  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_ERROR_FAILURE;
  }

  event.refPoint = nsContentUtils::ToWidgetPoint(CSSPoint(aX, aY), offset, presContext);
  event.ignoreRootScrollFrame = aIgnoreRootScrollFrame;

  nsEventStatus status;
  if (aToWindow) {
    nsCOMPtr<nsIPresShell> presShell;
    nsView* view = nsContentUtils::GetViewToDispatchEvent(presContext, getter_AddRefs(presShell));
    if (!presShell || !view) {
      return NS_ERROR_FAILURE;
    }
    status = nsEventStatus_eIgnore;
    return presShell->HandleEvent(view->GetFrame(), &event, false, &status);
  }
  nsresult rv = widget->DispatchEvent(&event, status);
  if (aPreventDefault) {
    *aPreventDefault = (status == nsEventStatus_eConsumeNoDefault);
  }

  return rv;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendPointerEvent(const nsAString& aType,
                                   float aX,
                                   float aY,
                                   int32_t aButton,
                                   int32_t aClickCount,
                                   int32_t aModifiers,
                                   bool aIgnoreRootScrollFrame,
                                   float aPressure,
                                   unsigned short aInputSourceArg,
                                   int32_t aPointerId,
                                   int32_t aWidth,
                                   int32_t aHeight,
                                   int32_t aTiltX,
                                   int32_t aTiltY,
                                   bool aIsPrimary,
                                   bool aIsSynthesized,
                                   uint8_t aOptionalArgCount,
                                   bool* aPreventDefault)
{
  PROFILER_LABEL("nsDOMWindowUtils", "SendPointerEvent",
                 js::ProfileEntry::Category::EVENTS);

  return SendPointerEventCommon(aType, aX, aY, aButton, aClickCount,
                                aModifiers, aIgnoreRootScrollFrame,
                                aPressure, aInputSourceArg, aPointerId,
                                aWidth, aHeight, aTiltX, aTiltY,
                                aIsPrimary, aIsSynthesized,
                                aOptionalArgCount, false, aPreventDefault);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendPointerEventToWindow(const nsAString& aType,
                                           float aX,
                                           float aY,
                                           int32_t aButton,
                                           int32_t aClickCount,
                                           int32_t aModifiers,
                                           bool aIgnoreRootScrollFrame,
                                           float aPressure,
                                           unsigned short aInputSourceArg,
                                           int32_t aPointerId,
                                           int32_t aWidth,
                                           int32_t aHeight,
                                           int32_t aTiltX,
                                           int32_t aTiltY,
                                           bool aIsPrimary,
                                           bool aIsSynthesized,
                                           uint8_t aOptionalArgCount)
{
  PROFILER_LABEL("nsDOMWindowUtils", "SendPointerEventToWindow",
                 js::ProfileEntry::Category::EVENTS);

  return SendPointerEventCommon(aType, aX, aY, aButton, aClickCount,
                                aModifiers, aIgnoreRootScrollFrame,
                                aPressure, aInputSourceArg, aPointerId,
                                aWidth, aHeight, aTiltX, aTiltY,
                                aIsPrimary, aIsSynthesized,
                                aOptionalArgCount, true, nullptr);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendWheelEvent(float aX,
                                 float aY,
                                 double aDeltaX,
                                 double aDeltaY,
                                 double aDeltaZ,
                                 uint32_t aDeltaMode,
                                 int32_t aModifiers,
                                 int32_t aLineOrPageDeltaX,
                                 int32_t aLineOrPageDeltaY,
                                 uint32_t aOptions)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget) {
    return NS_ERROR_NULL_POINTER;
  }

  WidgetWheelEvent wheelEvent(true, NS_WHEEL_WHEEL, widget);
  wheelEvent.modifiers = nsContentUtils::GetWidgetModifiers(aModifiers);
  wheelEvent.deltaX = aDeltaX;
  wheelEvent.deltaY = aDeltaY;
  wheelEvent.deltaZ = aDeltaZ;
  wheelEvent.deltaMode = aDeltaMode;
  wheelEvent.isMomentum =
    (aOptions & WHEEL_EVENT_CAUSED_BY_MOMENTUM) != 0;
  wheelEvent.mIsNoLineOrPageDelta =
    (aOptions & WHEEL_EVENT_CAUSED_BY_NO_LINE_OR_PAGE_DELTA_DEVICE) != 0;
  wheelEvent.customizedByUserPrefs =
    (aOptions & WHEEL_EVENT_CUSTOMIZED_BY_USER_PREFS) != 0;
  wheelEvent.lineOrPageDeltaX = aLineOrPageDeltaX;
  wheelEvent.lineOrPageDeltaY = aLineOrPageDeltaY;
  wheelEvent.widget = widget;

  wheelEvent.time = PR_Now() / 1000;

  nsPresContext* presContext = GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  wheelEvent.refPoint = nsContentUtils::ToWidgetPoint(CSSPoint(aX, aY), offset, presContext);

  widget->DispatchAPZAwareEvent(&wheelEvent);

  if (widget->AsyncPanZoomEnabled()) {
    // Computing overflow deltas is not compatible with APZ, so if APZ is
    // enabled, we skip testing it.
    return NS_OK;
  }

  bool failedX = false;
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_ZERO) &&
      wheelEvent.overflowDeltaX != 0) {
    failedX = true;
  }
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_POSITIVE) &&
      wheelEvent.overflowDeltaX <= 0) {
    failedX = true;
  }
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_X_NEGATIVE) &&
      wheelEvent.overflowDeltaX >= 0) {
    failedX = true;
  }
  bool failedY = false;
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_ZERO) &&
      wheelEvent.overflowDeltaY != 0) {
    failedY = true;
  }
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_POSITIVE) &&
      wheelEvent.overflowDeltaY <= 0) {
    failedY = true;
  }
  if ((aOptions & WHEEL_EVENT_EXPECTED_OVERFLOW_DELTA_Y_NEGATIVE) &&
      wheelEvent.overflowDeltaY >= 0) {
    failedY = true;
  }

#ifdef DEBUG
  if (failedX) {
    nsPrintfCString debugMsg("SendWheelEvent(): unexpected overflowDeltaX: %f",
                             wheelEvent.overflowDeltaX);
    NS_WARNING(debugMsg.get());
  }
  if (failedY) {
    nsPrintfCString debugMsg("SendWheelEvent(): unexpected overflowDeltaY: %f",
                             wheelEvent.overflowDeltaY);
    NS_WARNING(debugMsg.get());
  }
#endif

  return (!failedX && !failedY) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendTouchEvent(const nsAString& aType,
                                 uint32_t *aIdentifiers,
                                 int32_t *aXs,
                                 int32_t *aYs,
                                 uint32_t *aRxs,
                                 uint32_t *aRys,
                                 float *aRotationAngles,
                                 float *aForces,
                                 uint32_t aCount,
                                 int32_t aModifiers,
                                 bool aIgnoreRootScrollFrame,
                                 bool *aPreventDefault)
{
  return SendTouchEventCommon(aType, aIdentifiers, aXs, aYs, aRxs, aRys,
                              aRotationAngles, aForces, aCount, aModifiers,
                              aIgnoreRootScrollFrame, false, aPreventDefault);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendTouchEventToWindow(const nsAString& aType,
                                         uint32_t* aIdentifiers,
                                         int32_t* aXs,
                                         int32_t* aYs,
                                         uint32_t* aRxs,
                                         uint32_t* aRys,
                                         float* aRotationAngles,
                                         float* aForces,
                                         uint32_t aCount,
                                         int32_t aModifiers,
                                         bool aIgnoreRootScrollFrame,
                                         bool* aPreventDefault)
{
  return SendTouchEventCommon(aType, aIdentifiers, aXs, aYs, aRxs, aRys,
                              aRotationAngles, aForces, aCount, aModifiers,
                              aIgnoreRootScrollFrame, true, aPreventDefault);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendTouchEventCommon(const nsAString& aType,
                                       uint32_t* aIdentifiers,
                                       int32_t* aXs,
                                       int32_t* aYs,
                                       uint32_t* aRxs,
                                       uint32_t* aRys,
                                       float* aRotationAngles,
                                       float* aForces,
                                       uint32_t aCount,
                                       int32_t aModifiers,
                                       bool aIgnoreRootScrollFrame,
                                       bool aToWindow,
                                       bool* aPreventDefault)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget) {
    return NS_ERROR_NULL_POINTER;
  }
  int32_t msg;
  if (aType.EqualsLiteral("touchstart")) {
    msg = NS_TOUCH_START;
  } else if (aType.EqualsLiteral("touchmove")) {
    msg = NS_TOUCH_MOVE;
  } else if (aType.EqualsLiteral("touchend")) {
    msg = NS_TOUCH_END;
  } else if (aType.EqualsLiteral("touchcancel")) {
    msg = NS_TOUCH_CANCEL;
  } else {
    return NS_ERROR_UNEXPECTED;
  }
  WidgetTouchEvent event(true, msg, widget);
  event.modifiers = nsContentUtils::GetWidgetModifiers(aModifiers);
  event.widget = widget;
  event.time = PR_Now();

  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_ERROR_FAILURE;
  }
  event.touches.SetCapacity(aCount);
  for (uint32_t i = 0; i < aCount; ++i) {
    LayoutDeviceIntPoint pt =
      nsContentUtils::ToWidgetPoint(CSSPoint(aXs[i], aYs[i]), offset, presContext);
    nsRefPtr<Touch> t = new Touch(aIdentifiers[i],
                                  pt,
                                  nsIntPoint(aRxs[i], aRys[i]),
                                  aRotationAngles[i],
                                  aForces[i]);
    event.touches.AppendElement(t);
  }

  nsEventStatus status;
  if (aToWindow) {
    nsCOMPtr<nsIPresShell> presShell;
    nsView* view = nsContentUtils::GetViewToDispatchEvent(presContext, getter_AddRefs(presShell));
    if (!presShell || !view) {
      return NS_ERROR_FAILURE;
    }
    status = nsEventStatus_eIgnore;
    *aPreventDefault = (status == nsEventStatus_eConsumeNoDefault);
    return presShell->HandleEvent(view->GetFrame(), &event, false, &status);
  }

  nsresult rv = widget->DispatchEvent(&event, status);
  *aPreventDefault = (status == nsEventStatus_eConsumeNoDefault);
  return rv;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendKeyEvent(const nsAString& aType,
                               int32_t aKeyCode,
                               int32_t aCharCode,
                               int32_t aModifiers,
                               uint32_t aAdditionalFlags,
                               bool* aDefaultActionTaken)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  
  return nsContentUtils::SendKeyEvent(widget, aType, aKeyCode, aCharCode,
                                      aModifiers, aAdditionalFlags,
                                      aDefaultActionTaken);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                     int32_t aNativeKeyCode,
                                     int32_t aModifiers,
                                     const nsAString& aCharacters,
                                     const nsAString& aUnmodifiedCharacters,
                                     nsIObserver* aObserver)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  NS_DispatchToMainThread(NS_NewRunnableMethodWithArgs
    <int32_t, int32_t, uint32_t, nsString, nsString, nsIObserver*>
    (widget, &nsIWidget::SynthesizeNativeKeyEvent, aNativeKeyboardLayout,
    aNativeKeyCode, aModifiers, aCharacters, aUnmodifiedCharacters, aObserver));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeMouseEvent(int32_t aScreenX,
                                       int32_t aScreenY,
                                       int32_t aNativeMessage,
                                       int32_t aModifierFlags,
                                       nsIDOMElement* aElement,
                                       nsIObserver* aObserver)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidgetForElement(aElement);
  if (!widget)
    return NS_ERROR_FAILURE;

  NS_DispatchToMainThread(NS_NewRunnableMethodWithArgs
    <LayoutDeviceIntPoint, int32_t, int32_t, nsIObserver*>
    (widget, &nsIWidget::SynthesizeNativeMouseEvent,
    LayoutDeviceIntPoint(aScreenX, aScreenY), aNativeMessage, aModifierFlags,
    aObserver));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeMouseScrollEvent(int32_t aScreenX,
                                             int32_t aScreenY,
                                             uint32_t aNativeMessage,
                                             double aDeltaX,
                                             double aDeltaY,
                                             double aDeltaZ,
                                             uint32_t aModifierFlags,
                                             uint32_t aAdditionalFlags,
                                             nsIDOMElement* aElement,
                                             nsIObserver* aObserver)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidgetForElement(aElement);
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  NS_DispatchToMainThread(NS_NewRunnableMethodWithArgs
    <mozilla::LayoutDeviceIntPoint, uint32_t, double, double, double, uint32_t, uint32_t, nsIObserver*>
    (widget, &nsIWidget::SynthesizeNativeMouseScrollEvent,
    LayoutDeviceIntPoint(aScreenX, aScreenY), aNativeMessage, aDeltaX, aDeltaY,
    aDeltaZ, aModifierFlags, aAdditionalFlags, aObserver));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeTouchPoint(uint32_t aPointerId,
                                       uint32_t aTouchState,
                                       int32_t aScreenX,
                                       int32_t aScreenY,
                                       double aPressure,
                                       uint32_t aOrientation,
                                       nsIObserver* aObserver)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  if (aPressure < 0 || aPressure > 1 || aOrientation > 359) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_DispatchToMainThread(NS_NewRunnableMethodWithArgs
    <uint32_t, nsIWidget::TouchPointerState, nsIntPoint, double, uint32_t, nsIObserver*>
    (widget, &nsIWidget::SynthesizeNativeTouchPoint, aPointerId,
    (nsIWidget::TouchPointerState)aTouchState, nsIntPoint(aScreenX, aScreenY),
    aPressure, aOrientation, aObserver));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeTouchTap(int32_t aScreenX,
                                     int32_t aScreenY,
                                     bool aLongTap,
                                     nsIObserver* aObserver)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  NS_DispatchToMainThread(NS_NewRunnableMethodWithArgs
    <nsIntPoint, bool, nsIObserver*>
    (widget, &nsIWidget::SynthesizeNativeTouchTap,
    nsIntPoint(aScreenX, aScreenY), aLongTap, aObserver));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ClearNativeTouchSequence(nsIObserver* aObserver)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  NS_DispatchToMainThread(NS_NewRunnableMethodWithArgs<nsIObserver*>
    (widget, &nsIWidget::ClearNativeTouchSequence, aObserver));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ActivateNativeMenuItemAt(const nsAString& indexString)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  return widget->ActivateNativeMenuItemAt(indexString);
}

NS_IMETHODIMP
nsDOMWindowUtils::ForceUpdateNativeMenuAt(const nsAString& indexString)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  return widget->ForceUpdateNativeMenuAt(indexString);
}

nsIWidget*
nsDOMWindowUtils::GetWidget(nsPoint* aOffset)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  if (window) {
    nsIDocShell *docShell = window->GetDocShell();
    if (docShell) {
      nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
      return nsContentUtils::GetWidget(presShell, aOffset);
    }
  }

  return nullptr;
}

nsIWidget*
nsDOMWindowUtils::GetWidgetForElement(nsIDOMElement* aElement)
{
  if (!aElement)
    return GetWidget();

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  nsIDocument* doc = content->GetCurrentDoc();
  nsIPresShell* presShell = doc ? doc->GetShell() : nullptr;

  if (presShell) {
    nsIFrame* frame = content->GetPrimaryFrame();
    if (!frame) {
      frame = presShell->GetRootFrame();
    }
    if (frame)
      return frame->GetNearestWidget();
  }

  return nullptr;
}

NS_IMETHODIMP
nsDOMWindowUtils::Focus(nsIDOMElement* aElement)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIDOMWindow> window = do_QueryReferent(mWindow);
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    if (aElement)
      fm->SetFocus(aElement, 0);
    else
      fm->ClearFocus(window);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GarbageCollect(nsICycleCollectorListener *aListener,
                                 int32_t aExtraForgetSkippableCalls)
{
  PROFILER_LABEL("nsDOMWindowUtils", "GarbageCollect",
    js::ProfileEntry::Category::GC);
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsJSContext::GarbageCollectNow(JS::gcreason::DOM_UTILS);
  nsJSContext::CycleCollectNow(aListener, aExtraForgetSkippableCalls);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::CycleCollect(nsICycleCollectorListener *aListener,
                               int32_t aExtraForgetSkippableCalls)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsJSContext::CycleCollectNow(aListener, aExtraForgetSkippableCalls);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::RunNextCollectorTimer()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsJSContext::RunNextCollectorTimer();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendSimpleGestureEvent(const nsAString& aType,
                                         float aX,
                                         float aY,
                                         uint32_t aDirection,
                                         double aDelta,
                                         int32_t aModifiers,
                                         uint32_t aClickCount)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget)
    return NS_ERROR_FAILURE;

  int32_t msg;
  if (aType.EqualsLiteral("MozSwipeGestureStart"))
    msg = NS_SIMPLE_GESTURE_SWIPE_START;
  else if (aType.EqualsLiteral("MozSwipeGestureUpdate"))
    msg = NS_SIMPLE_GESTURE_SWIPE_UPDATE;
  else if (aType.EqualsLiteral("MozSwipeGestureEnd"))
    msg = NS_SIMPLE_GESTURE_SWIPE_END;
  else if (aType.EqualsLiteral("MozSwipeGesture"))
    msg = NS_SIMPLE_GESTURE_SWIPE;
  else if (aType.EqualsLiteral("MozMagnifyGestureStart"))
    msg = NS_SIMPLE_GESTURE_MAGNIFY_START;
  else if (aType.EqualsLiteral("MozMagnifyGestureUpdate"))
    msg = NS_SIMPLE_GESTURE_MAGNIFY_UPDATE;
  else if (aType.EqualsLiteral("MozMagnifyGesture"))
    msg = NS_SIMPLE_GESTURE_MAGNIFY;
  else if (aType.EqualsLiteral("MozRotateGestureStart"))
    msg = NS_SIMPLE_GESTURE_ROTATE_START;
  else if (aType.EqualsLiteral("MozRotateGestureUpdate"))
    msg = NS_SIMPLE_GESTURE_ROTATE_UPDATE;
  else if (aType.EqualsLiteral("MozRotateGesture"))
    msg = NS_SIMPLE_GESTURE_ROTATE;
  else if (aType.EqualsLiteral("MozTapGesture"))
    msg = NS_SIMPLE_GESTURE_TAP;
  else if (aType.EqualsLiteral("MozPressTapGesture"))
    msg = NS_SIMPLE_GESTURE_PRESSTAP;
  else if (aType.EqualsLiteral("MozEdgeUIStarted"))
    msg = NS_SIMPLE_GESTURE_EDGE_STARTED;
  else if (aType.EqualsLiteral("MozEdgeUICanceled"))
    msg = NS_SIMPLE_GESTURE_EDGE_CANCELED;
  else if (aType.EqualsLiteral("MozEdgeUICompleted"))
    msg = NS_SIMPLE_GESTURE_EDGE_COMPLETED;
  else
    return NS_ERROR_FAILURE;
 
  WidgetSimpleGestureEvent event(true, msg, widget);
  event.modifiers = nsContentUtils::GetWidgetModifiers(aModifiers);
  event.direction = aDirection;
  event.delta = aDelta;
  event.clickCount = aClickCount;
  event.time = PR_IntervalNow();

  nsPresContext* presContext = GetPresContext();
  if (!presContext)
    return NS_ERROR_FAILURE;

  event.refPoint = nsContentUtils::ToWidgetPoint(CSSPoint(aX, aY), offset, presContext);

  nsEventStatus status;
  return widget->DispatchEvent(&event, status);
}

NS_IMETHODIMP
nsDOMWindowUtils::ElementFromPoint(float aX, float aY,
                                   bool aIgnoreRootScrollFrame,
                                   bool aFlushLayout,
                                   nsIDOMElement** aReturn)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  Element* el =
    doc->ElementFromPointHelper(aX, aY, aIgnoreRootScrollFrame, aFlushLayout);
  nsCOMPtr<nsIDOMElement> retval = do_QueryInterface(el);
  retval.forget(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::NodesFromRect(float aX, float aY,
                                float aTopSize, float aRightSize,
                                float aBottomSize, float aLeftSize,
                                bool aIgnoreRootScrollFrame,
                                bool aFlushLayout,
                                nsIDOMNodeList** aReturn)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  return doc->NodesFromRectHelper(aX, aY, aTopSize, aRightSize, aBottomSize, aLeftSize, 
                                  aIgnoreRootScrollFrame, aFlushLayout, aReturn);
}

NS_IMETHODIMP
nsDOMWindowUtils::GetTranslationNodes(nsIDOMNode* aRoot,
                                      nsITranslationNodeList** aRetVal)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_ARG_POINTER(aRetVal);
  nsCOMPtr<nsIContent> root = do_QueryInterface(aRoot);
  NS_ENSURE_STATE(root);
  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  if (root->OwnerDoc() != doc) {
    return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
  }

  nsTHashtable<nsPtrHashKey<nsIContent>> translationNodesHash(500);
  nsRefPtr<nsTranslationNodeList> list = new nsTranslationNodeList;

  uint32_t limit = 15000;

  // We begin iteration with content->GetNextNode because we want to explictly
  // skip the root tag from being a translation node.
  nsIContent* content = root;
  while ((limit > 0) && (content = content->GetNextNode(root))) {
    if (!content->IsHTMLElement()) {
      continue;
    }

    // Skip elements that usually contain non-translatable text content.
    if (content->IsAnyOfHTMLElements(nsGkAtoms::script,
                                     nsGkAtoms::iframe,
                                     nsGkAtoms::frameset,
                                     nsGkAtoms::frame,
                                     nsGkAtoms::code,
                                     nsGkAtoms::noscript,
                                     nsGkAtoms::style)) {
      continue;
    }

    // An element is a translation node if it contains
    // at least one text node that has meaningful data
    // for translation
    for (nsIContent* child = content->GetFirstChild();
         child;
         child = child->GetNextSibling()) {

      if (child->HasTextForTranslation()) {
        translationNodesHash.PutEntry(content);

        bool isBlockFrame = false;
        nsIFrame* frame = content->GetPrimaryFrame();
        if (frame) {
          isBlockFrame = frame->IsFrameOfType(nsIFrame::eBlockFrame);
        }

        bool isTranslationRoot = isBlockFrame;
        if (!isBlockFrame) {
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

        list->AppendElement(content->AsDOMNode(), isTranslationRoot);
        --limit;
        break;
      }
    }
  }

  *aRetVal = list.forget().take();
  return NS_OK;
}

static already_AddRefed<DataSourceSurface>
CanvasToDataSourceSurface(nsIDOMHTMLCanvasElement* aCanvas)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aCanvas);
  if (!node) {
    return nullptr;
  }

  MOZ_ASSERT(node->IsElement(),
             "An nsINode that implements nsIDOMHTMLCanvasElement should "
             "be an element.");
  nsLayoutUtils::SurfaceFromElementResult result =
    nsLayoutUtils::SurfaceFromElement(node->AsElement());
  return result.mSourceSurface->GetDataSurface();
}

NS_IMETHODIMP
nsDOMWindowUtils::CompareCanvases(nsIDOMHTMLCanvasElement *aCanvas1,
                                  nsIDOMHTMLCanvasElement *aCanvas2,
                                  uint32_t* aMaxDifference,
                                  uint32_t* retVal)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (aCanvas1 == nullptr ||
      aCanvas2 == nullptr ||
      retVal == nullptr)
    return NS_ERROR_FAILURE;

  RefPtr<DataSourceSurface> img1 = CanvasToDataSourceSurface(aCanvas1);
  RefPtr<DataSourceSurface> img2 = CanvasToDataSourceSurface(aCanvas2);

  DataSourceSurface::ScopedMap map1(img1, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap map2(img2, DataSourceSurface::READ);

  if (img1 == nullptr || img2 == nullptr ||
      !map1.IsMapped() || !map2.IsMapped() ||
      img1->GetSize() != img2->GetSize() ||
      map1.GetStride() != map2.GetStride()) {
    return NS_ERROR_FAILURE;
  }

  int v;
  IntSize size = img1->GetSize();
  int32_t stride = map1.GetStride();

  // we can optimize for the common all-pass case
  if (stride == size.width * 4) {
    v = memcmp(map1.GetData(), map2.GetData(), size.width * size.height * 4);
    if (v == 0) {
      if (aMaxDifference)
        *aMaxDifference = 0;
      *retVal = 0;
      return NS_OK;
    }
  }

  uint32_t dc = 0;
  uint32_t different = 0;

  for (int j = 0; j < size.height; j++) {
    unsigned char *p1 = map1.GetData() + j*stride;
    unsigned char *p2 = map2.GetData() + j*stride;
    v = memcmp(p1, p2, stride);

    if (v) {
      for (int i = 0; i < size.width; i++) {
        if (*(uint32_t*) p1 != *(uint32_t*) p2) {

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

  if (aMaxDifference)
    *aMaxDifference = dc;

  *retVal = different;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsMozAfterPaintPending(bool *aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;
  nsPresContext* presContext = GetPresContext();
  if (!presContext)
    return NS_OK;
  *aResult = presContext->IsDOMPaintEventPending();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ClearMozAfterPaintEvents()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsPresContext* presContext = GetPresContext();
  if (!presContext)
    return NS_OK;
  presContext->ClearMozAfterPaintEvents();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::DisableNonTestMouseEvents(bool aDisable)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsIDocShell *docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  presShell->DisableNonTestMouseEvents(aDisable);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SuppressEventHandling(bool aSuppress)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  if (aSuppress) {
    doc->SuppressEventHandling(nsIDocument::eEvents);
  } else {
    doc->UnsuppressEventHandlingAndFireEvents(nsIDocument::eEvents, true);
  }

  return NS_OK;
}

static nsresult
getScrollXYAppUnits(nsWeakPtr aWindow, bool aFlushLayout, nsPoint& aScrollPos) {
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(aWindow);
  nsCOMPtr<nsIDocument> doc = window ? window->GetExtantDoc() : nullptr;
  NS_ENSURE_STATE(doc);

  if (aFlushLayout) {
    doc->FlushPendingNotifications(Flush_Layout);
  }

  nsIPresShell *presShell = doc->GetShell();
  if (presShell) {
    nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
    if (sf) {
      aScrollPos = sf->GetScrollPosition();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScrollXY(bool aFlushLayout, int32_t* aScrollX, int32_t* aScrollY)
{
  nsPoint scrollPos(0,0);
  nsresult rv = getScrollXYAppUnits(mWindow, aFlushLayout, scrollPos);
  NS_ENSURE_SUCCESS(rv, rv);
  *aScrollX = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x);
  *aScrollY = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScrollXYFloat(bool aFlushLayout, float* aScrollX, float* aScrollY)
{
  nsPoint scrollPos(0,0);
  nsresult rv = getScrollXYAppUnits(mWindow, aFlushLayout, scrollPos);
  NS_ENSURE_SUCCESS(rv, rv);
  *aScrollX = nsPresContext::AppUnitsToFloatCSSPixels(scrollPos.x);
  *aScrollY = nsPresContext::AppUnitsToFloatCSSPixels(scrollPos.y);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScrollbarSize(bool aFlushLayout, int32_t* aWidth,
                                                      int32_t* aHeight)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  *aWidth = 0;
  *aHeight = 0;

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  if (aFlushLayout) {
    doc->FlushPendingNotifications(Flush_Layout);
  }

  nsIPresShell* presShell = doc->GetShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_AVAILABLE);

  nsIScrollableFrame* scrollFrame = presShell->GetRootScrollFrameAsScrollable();
  NS_ENSURE_TRUE(scrollFrame, NS_OK);

  nsMargin sizes = scrollFrame->GetActualScrollbarSizes();
  *aWidth = nsPresContext::AppUnitsToIntCSSPixels(sizes.LeftRight());
  *aHeight = nsPresContext::AppUnitsToIntCSSPixels(sizes.TopBottom());

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetBoundsWithoutFlushing(nsIDOMElement *aElement,
                                           nsIDOMClientRect** aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  nsresult rv;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<DOMRect> rect = new DOMRect(window);
  nsIFrame* frame = content->GetPrimaryFrame();

  if (frame) {
    nsRect r = nsLayoutUtils::GetAllInFlowRectsUnion(frame,
               nsLayoutUtils::GetContainingBlockForClientRect(frame),
               nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS);
    rect->SetLayoutRect(r);
  }

  rect.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetRootBounds(nsIDOMClientRect** aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsIDocument* doc = GetDocument();
  NS_ENSURE_STATE(doc);

  nsRect bounds(0, 0, 0, 0);
  nsIPresShell* presShell = doc->GetShell();
  if (presShell) {
    nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
    if (sf) {
      bounds = sf->GetScrollRange();
      bounds.width += sf->GetScrollPortRect().width;
      bounds.height += sf->GetScrollPortRect().height;
    } else if (presShell->GetRootFrame()) {
      bounds = presShell->GetRootFrame()->GetRect();
    }
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  nsRefPtr<DOMRect> rect = new DOMRect(window);
  rect->SetRect(nsPresContext::AppUnitsToFloatCSSPixels(bounds.x),
                nsPresContext::AppUnitsToFloatCSSPixels(bounds.y),
                nsPresContext::AppUnitsToFloatCSSPixels(bounds.width),
                nsPresContext::AppUnitsToFloatCSSPixels(bounds.height));
  rect.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIMEIsOpen(bool *aState)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_ARG_POINTER(aState);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  // Open state should not be available when IME is not enabled.
  InputContext context = widget->GetInputContext();
  if (context.mIMEState.mEnabled != IMEState::ENABLED) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (context.mIMEState.mOpen == IMEState::OPEN_STATE_NOT_SUPPORTED) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  *aState = (context.mIMEState.mOpen == IMEState::OPEN);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIMEStatus(uint32_t *aState)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_ARG_POINTER(aState);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  InputContext context = widget->GetInputContext();
  *aState = static_cast<uint32_t>(context.mIMEState.mEnabled);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFocusedInputType(char** aType)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_ARG_POINTER(aType);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  InputContext context = widget->GetInputContext();
  *aType = ToNewCString(context.mHTMLInputType);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::FindElementWithViewId(nsViewID aID,
                                        nsIDOMElement** aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsRefPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aID);
  return content ? CallQueryInterface(content, aResult) : NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetViewId(nsIDOMElement* aElement, nsViewID* aResult)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  if (content && nsLayoutUtils::FindIDFor(content, aResult)) {
    return NS_OK;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScreenPixelsPerCSSPixel(float* aScreenPixels)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  return window->GetDevicePixelRatio(aScreenPixels);
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFullZoom(float* aFullZoom)
{
  *aFullZoom = 1.0f;

  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_OK;
  }

  *aFullZoom = presContext->DeviceContext()->GetFullZoom();

  return NS_OK;
}
 
NS_IMETHODIMP
nsDOMWindowUtils::DispatchDOMEventViaPresShell(nsIDOMNode* aTarget,
                                               nsIDOMEvent* aEvent,
                                               bool aTrusted,
                                               bool* aRetVal)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_STATE(aEvent);
  aEvent->SetTrusted(aTrusted);
  WidgetEvent* internalEvent = aEvent->GetInternalNSEvent();
  NS_ENSURE_STATE(internalEvent);
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTarget);
  NS_ENSURE_STATE(content);
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  if (content->OwnerDoc()->GetWindow() != window) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }
  nsCOMPtr<nsIDocument> targetDoc = content->GetCurrentDoc();
  NS_ENSURE_STATE(targetDoc);
  nsRefPtr<nsIPresShell> targetShell = targetDoc->GetShell();
  NS_ENSURE_STATE(targetShell);

  targetDoc->FlushPendingNotifications(Flush_Layout);

  nsEventStatus status = nsEventStatus_eIgnore;
  targetShell->HandleEventWithTarget(internalEvent, nullptr, content, &status);
  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return NS_OK;
}

static void
InitEvent(WidgetGUIEvent& aEvent, LayoutDeviceIntPoint* aPt = nullptr)
{
  if (aPt) {
    aEvent.refPoint = *aPt;
  }
  aEvent.time = PR_IntervalNow();
}

NS_IMETHODIMP
nsDOMWindowUtils::SendQueryContentEvent(uint32_t aType,
                                        uint32_t aOffset, uint32_t aLength,
                                        int32_t aX, int32_t aY,
                                        uint32_t aAdditionalFlags,
                                        nsIQueryContentEventResult **aResult)
{
  *aResult = nullptr;

  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsIDocShell *docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsPresContext* presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  if (aType != NS_QUERY_SELECTED_TEXT &&
      aType != NS_QUERY_TEXT_CONTENT &&
      aType != NS_QUERY_CARET_RECT &&
      aType != NS_QUERY_TEXT_RECT &&
      aType != NS_QUERY_EDITOR_RECT &&
      aType != NS_QUERY_CHARACTER_AT_POINT) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIWidget> targetWidget = widget;
  LayoutDeviceIntPoint pt(aX, aY);

  bool useNativeLineBreak =
    !(aAdditionalFlags & QUERY_CONTENT_FLAG_USE_XP_LINE_BREAK);

  if (aType == QUERY_CHARACTER_AT_POINT) {
    // Looking for the widget at the point.
    WidgetQueryContentEvent dummyEvent(true, NS_QUERY_CONTENT_STATE, widget);
    dummyEvent.mUseNativeLineBreak = useNativeLineBreak;
    InitEvent(dummyEvent, &pt);
    nsIFrame* popupFrame =
      nsLayoutUtils::GetPopupFrameForEventCoordinates(presContext->GetRootPresContext(), &dummyEvent);

    nsIntRect widgetBounds;
    nsresult rv = widget->GetClientBounds(widgetBounds);
    NS_ENSURE_SUCCESS(rv, rv);
    widgetBounds.MoveTo(0, 0);

    // There is no popup frame at the point and the point isn't in our widget,
    // we cannot process this request.
    NS_ENSURE_TRUE(popupFrame ||
                   widgetBounds.Contains(LayoutDeviceIntPoint::ToUntyped(pt)),
                   NS_ERROR_FAILURE);

    // Fire the event on the widget at the point
    if (popupFrame) {
      targetWidget = popupFrame->GetNearestWidget();
    }
  }

  pt += widget->WidgetToScreenOffset() - targetWidget->WidgetToScreenOffset();

  WidgetQueryContentEvent queryEvent(true, aType, targetWidget);
  InitEvent(queryEvent, &pt);

  switch (aType) {
    case NS_QUERY_TEXT_CONTENT:
      queryEvent.InitForQueryTextContent(aOffset, aLength, useNativeLineBreak);
      break;
    case NS_QUERY_CARET_RECT:
      queryEvent.InitForQueryCaretRect(aOffset, useNativeLineBreak);
      break;
    case NS_QUERY_TEXT_RECT:
      queryEvent.InitForQueryTextRect(aOffset, aLength, useNativeLineBreak);
      break;
    default:
      queryEvent.mUseNativeLineBreak = useNativeLineBreak;
      break;
  }

  nsEventStatus status;
  nsresult rv = targetWidget->DispatchEvent(&queryEvent, status);
  NS_ENSURE_SUCCESS(rv, rv);

  nsQueryContentEventResult* result = new nsQueryContentEventResult();
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);
  result->SetEventResult(widget, queryEvent);
  NS_ADDREF(*aResult = result);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendSelectionSetEvent(uint32_t aOffset,
                                        uint32_t aLength,
                                        uint32_t aAdditionalFlags,
                                        bool *aResult)
{
  *aResult = false;

  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  WidgetSelectionEvent selectionEvent(true, NS_SELECTION_SET, widget);
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
                                          nsITransferable * aTransferable)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  int32_t msg;
  if (aType.EqualsLiteral("cut"))
    msg = NS_CONTENT_COMMAND_CUT;
  else if (aType.EqualsLiteral("copy"))
    msg = NS_CONTENT_COMMAND_COPY;
  else if (aType.EqualsLiteral("paste"))
    msg = NS_CONTENT_COMMAND_PASTE;
  else if (aType.EqualsLiteral("delete"))
    msg = NS_CONTENT_COMMAND_DELETE;
  else if (aType.EqualsLiteral("undo"))
    msg = NS_CONTENT_COMMAND_UNDO;
  else if (aType.EqualsLiteral("redo"))
    msg = NS_CONTENT_COMMAND_REDO;
  else if (aType.EqualsLiteral("pasteTransferable"))
    msg = NS_CONTENT_COMMAND_PASTE_TRANSFERABLE;
  else
    return NS_ERROR_FAILURE;

  WidgetContentCommandEvent event(true, msg, widget);
  if (msg == NS_CONTENT_COMMAND_PASTE_TRANSFERABLE) {
    event.mTransferable = aTransferable;
  }

  nsEventStatus status;
  return widget->DispatchEvent(&event, status);
}

NS_IMETHODIMP
nsDOMWindowUtils::GetClassName(JS::Handle<JS::Value> aObject, JSContext* aCx,
                               char** aName)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // Our argument must be a non-null object.
  if (aObject.isPrimitive()) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  *aName = NS_strdup(JS_GetClass(aObject.toObjectOrNull())->name);
  MOZ_ASSERT(*aName, "NS_strdup should be infallible.");
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetVisitedDependentComputedStyle(
                    nsIDOMElement *aElement, const nsAString& aPseudoElement,
                    const nsAString& aPropertyName, nsAString& aResult)
{
  aResult.Truncate();

  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  nsCOMPtr<nsIDOMCSSStyleDeclaration> decl;
  nsresult rv =
    window->GetComputedStyle(aElement, aPseudoElement, getter_AddRefs(decl));
  NS_ENSURE_SUCCESS(rv, rv);

  static_cast<nsComputedDOMStyle*>(decl.get())->SetExposeVisitedStyle(true);
  rv = decl->GetPropertyValue(aPropertyName, aResult);
  static_cast<nsComputedDOMStyle*>(decl.get())->SetExposeVisitedStyle(false);

  return rv;
}

NS_IMETHODIMP
nsDOMWindowUtils::EnterModalState()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  window->EnterModalState();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::LeaveModalState()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  window->LeaveModalState();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::IsInModalState(bool *retval)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  *retval = static_cast<nsGlobalWindow*>(window.get())->IsInModalState();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetDesktopModeViewport(bool aDesktopMode)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  static_cast<nsGlobalWindow*>(window.get())->SetDesktopModeViewport(aDesktopMode);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetOuterWindowID(uint64_t *aWindowID)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  NS_ASSERTION(window->IsOuterWindow(), "How did that happen?");
  *aWindowID = window->WindowID();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetCurrentInnerWindowID(uint64_t *aWindowID)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_NOT_AVAILABLE);

  NS_ASSERTION(window->IsOuterWindow(), "How did that happen?");
  nsGlobalWindow* inner =
    static_cast<nsGlobalWindow*>(window.get())->GetCurrentInnerWindowInternal();
  if (!inner) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aWindowID = inner->WindowID();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SuspendTimeouts()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  window->SuspendTimeouts();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ResumeTimeouts()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  window->ResumeTimeouts();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetLayerManagerType(nsAString& aType)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  LayerManager *mgr = widget->GetLayerManager(nsIWidget::LAYER_MANAGER_PERSISTENT);
  if (!mgr)
    return NS_ERROR_FAILURE;

  mgr->GetBackendName(aType);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetLayerManagerRemote(bool* retval)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  LayerManager *mgr = widget->GetLayerManager();
  if (!mgr)
    return NS_ERROR_FAILURE;

  *retval = !!mgr->AsShadowForwarder();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetSupportsHardwareH264Decoding(bool* retval)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

#ifdef MOZ_FMP4
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  LayerManager *mgr = widget->GetLayerManager();
  if (!mgr)
    return NS_ERROR_FAILURE;

  *retval = MP4Decoder::IsVideoAccelerated(mgr->GetCompositorBackendType());
#else
  *retval = false;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StartFrameTimeRecording(uint32_t *startIndex)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_ARG_POINTER(startIndex);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  LayerManager *mgr = widget->GetLayerManager();
  if (!mgr)
    return NS_ERROR_FAILURE;

  const uint32_t kRecordingMinSize = 60 * 10; // 10 seconds @60 fps.
  const uint32_t kRecordingMaxSize = 60 * 60 * 60; // One hour
  uint32_t bufferSize = Preferences::GetUint("toolkit.framesRecording.bufferSize", uint32_t(0));
  bufferSize = std::min(bufferSize, kRecordingMaxSize);
  bufferSize = std::max(bufferSize, kRecordingMinSize);
  *startIndex = mgr->StartFrameTimeRecording(bufferSize);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StopFrameTimeRecording(uint32_t   startIndex,
                                         uint32_t  *frameCount,
                                         float    **frameIntervals)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_ARG_POINTER(frameCount);
  NS_ENSURE_ARG_POINTER(frameIntervals);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  LayerManager *mgr = widget->GetLayerManager();
  if (!mgr)
    return NS_ERROR_FAILURE;

  nsTArray<float> tmpFrameIntervals;
  mgr->StopFrameTimeRecording(startIndex, tmpFrameIntervals);
  *frameCount = tmpFrameIntervals.Length();

  *frameIntervals = (float*)moz_xmalloc(*frameCount * sizeof(float));

  /* copy over the frame intervals and paint times into the arrays we just allocated */
  for (uint32_t i = 0; i < *frameCount; i++) {
    (*frameIntervals)[i] = tmpFrameIntervals[i];
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::BeginTabSwitch()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  LayerManager *mgr = widget->GetLayerManager();
  if (!mgr)
    return NS_ERROR_FAILURE;

  mgr->BeginTabSwitch();

  return NS_OK;
}

static bool
ComputeAnimationValue(nsCSSProperty aProperty,
                      Element* aElement,
                      const nsAString& aInput,
                      StyleAnimationValue& aOutput)
{

  if (!StyleAnimationValue::ComputeValue(aProperty, aElement, aInput,
                                         false, aOutput)) {
    return false;
  }

  // This matches TransExtractComputedValue in nsTransitionManager.cpp.
  if (aProperty == eCSSProperty_visibility) {
    MOZ_ASSERT(aOutput.GetUnit() == StyleAnimationValue::eUnit_Enumerated,
               "unexpected unit");
    aOutput.SetIntValue(aOutput.GetIntValue(),
                        StyleAnimationValue::eUnit_Visibility);
  }

  return true;
}

NS_IMETHODIMP
nsDOMWindowUtils::AdvanceTimeAndRefresh(int64_t aMilliseconds)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // Before we advance the time, we should trigger any animations that are
  // waiting to start. This is because there are many tests that call this
  // which expect animations to start immediately. Ideally, we should make
  // all these tests do an asynchronous wait on the corresponding animation's
  // 'ready' promise before continuing. Then we could remove the special
  // handling here and the code path followed when testing would more closely
  // match the code path during regular operation. Filed as bug 1112957.
  nsCOMPtr<nsIDocument> doc = GetDocument();
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

    RefPtr<LayerTransactionChild> transaction = GetLayerTransaction();
    if (transaction && transaction->IPCOpen()) {
      transaction->SendSetTestSampleTime(driver->MostRecentRefresh());
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::RestoreNormalRefresh()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // Kick the compositor out of test mode before the refresh driver, so that
  // the refresh driver doesn't send an update that gets ignored by the
  // compositor.
  RefPtr<LayerTransactionChild> transaction = GetLayerTransaction();
  if (transaction && transaction->IPCOpen()) {
    transaction->SendLeaveTestMode();
  }

  if (nsPresContext* pc = GetPresContext()) {
    nsRefreshDriver* driver = pc->RefreshDriver();
    driver->RestoreNormalRefresh();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsTestControllingRefreshes(bool *aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsPresContext* pc = GetPresContext();
  *aResult =
    pc ? pc->RefreshDriver()->IsTestControllingRefreshesEnabled() : false;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetAsyncPanZoomEnabled(bool *aResult)
{
  nsIWidget* widget = GetWidget();
  if (widget) {
    *aResult = widget->AsyncPanZoomEnabled();
  } else {
    *aResult = gfxPlatform::AsyncPanZoomEnabled();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetAsyncScrollOffset(nsIDOMNode* aNode,
                                       int32_t aX, int32_t aY)
{
  nsCOMPtr<Element> element = do_QueryInterface(aNode);
  if (!element) {
    return NS_ERROR_INVALID_ARG;
  }
  FrameMetrics::ViewID viewId;
  if (!nsLayoutUtils::FindIDFor(element, &viewId)) {
    return NS_ERROR_UNEXPECTED;
  }
  nsIWidget* widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }
  LayerManager* manager = widget->GetLayerManager();
  if (!manager) {
    return NS_ERROR_FAILURE;
  }
  ShadowLayerForwarder* forwarder = manager->AsShadowForwarder();
  if (!forwarder || !forwarder->HasShadowManager()) {
    return NS_ERROR_UNEXPECTED;
  }
  forwarder->GetShadowManager()->SendSetAsyncScrollOffset(viewId, aX, aY);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetAsyncZoom(nsIDOMNode* aRootElement, float aValue)
{
  nsCOMPtr<Element> element = do_QueryInterface(aRootElement);
  if (!element) {
    return NS_ERROR_INVALID_ARG;
  }
  FrameMetrics::ViewID viewId;
  if (!nsLayoutUtils::FindIDFor(element, &viewId)) {
    return NS_ERROR_UNEXPECTED;
  }
  nsIWidget* widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }
  LayerManager* manager = widget->GetLayerManager();
  if (!manager) {
    return NS_ERROR_FAILURE;
  }
  ShadowLayerForwarder* forwarder = manager->AsShadowForwarder();
  if (!forwarder || !forwarder->HasShadowManager()) {
    return NS_ERROR_UNEXPECTED;
  }
  forwarder->GetShadowManager()->SendSetAsyncZoom(viewId, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::FlushApzRepaints(bool* aOutResult)
{
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
  LayerManager* manager = widget->GetLayerManager();
  if (!manager) {
    *aOutResult = false;
    return NS_OK;
  }
  ShadowLayerForwarder* forwarder = manager->AsShadowForwarder();
  if (!forwarder || !forwarder->HasShadowManager()) {
    *aOutResult = false;
    return NS_OK;
  }
  forwarder->GetShadowManager()->SendFlushApzRepaints();
  *aOutResult = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ComputeAnimationDistance(nsIDOMElement* aElement,
                                           const nsAString& aProperty,
                                           const nsAString& aValue1,
                                           const nsAString& aValue2,
                                           double* aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsresult rv;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCSSProperty property =
    nsCSSProps::LookupProperty(aProperty, nsCSSProps::eIgnoreEnabledState);
  if (property != eCSSProperty_UNKNOWN && nsCSSProps::IsShorthand(property)) {
    property = eCSSProperty_UNKNOWN;
  }

  MOZ_ASSERT(property == eCSSProperty_UNKNOWN ||
             !nsCSSProps::IsShorthand(property),
             "should not have shorthand");

  StyleAnimationValue v1, v2;
  if (property == eCSSProperty_UNKNOWN ||
      !ComputeAnimationValue(property, content->AsElement(), aValue1, v1) ||
      !ComputeAnimationValue(property, content->AsElement(), aValue2, v2)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (!StyleAnimationValue::ComputeDistance(property, v1, v2, *aResult)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsDOMWindowUtils::RenderDocument(const nsRect& aRect,
                                 uint32_t aFlags,
                                 nscolor aBackgroundColor,
                                 gfxContext* aThebesContext)
{
    MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

    nsCOMPtr<nsIDocument> doc = GetDocument();
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

    // Get Primary Shell
    nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    // Render Document
    return presShell->RenderDocument(aRect, aFlags, aBackgroundColor, aThebesContext);
}

NS_IMETHODIMP 
nsDOMWindowUtils::GetCursorType(int16_t *aCursor)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_ARG_POINTER(aCursor);

  nsIDocument* doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  bool isSameDoc = false;
  do {
    if (EventStateManager::sMouseOverDocument == doc) {
      isSameDoc = true;
      break;
    }
  } while ((doc = doc->GetParentDocument()));

  if (!isSameDoc) {
    *aCursor = eCursor_none;
    return NS_OK;
  }

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  // fetch cursor value from window's widget
  *aCursor = widget->GetCursor();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetDisplayDPI(float *aDPI)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  *aDPI = widget->GetDPI();

  return NS_OK;
}


NS_IMETHODIMP
nsDOMWindowUtils::GetOuterWindowWithId(uint64_t aWindowID,
                                       nsIDOMWindow** aWindow)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // XXX This method is deprecated.  See bug 865664.
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("DOM"),
                                  nsContentUtils::GetDocumentFromCaller(),
                                  nsContentUtils::eDOM_PROPERTIES,
                                  "GetWindowWithOuterIdWarning");

  *aWindow = nsGlobalWindow::GetOuterWindowWithId(aWindowID);
  NS_IF_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetContainerElement(nsIDOMElement** aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  nsCOMPtr<nsIDOMElement> element =
    do_QueryInterface(window->GetFrameElementInternal());

  element.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::WrapDOMFile(nsIFile *aFile,
                              nsISupports **aDOMFile)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (!aFile) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  nsPIDOMWindow* innerWindow = window->GetCurrentInnerWindow();
  if (!innerWindow) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMBlob> blob = File::CreateFromFile(innerWindow, aFile);
  blob.forget(aDOMFile);
  return NS_OK;
}

#ifdef DEBUG
static bool
CheckLeafLayers(Layer* aLayer, const nsIntPoint& aOffset, nsIntRegion* aCoveredRegion)
{
  gfx::Matrix transform;
  if (!aLayer->GetTransform().Is2D(&transform) ||
      transform.HasNonIntegerTranslation())
    return false;
  transform.NudgeToIntegers();
  nsIntPoint offset = aOffset + nsIntPoint(transform._31, transform._32);

  Layer* child = aLayer->GetFirstChild();
  if (child) {
    while (child) {
      if (!CheckLeafLayers(child, offset, aCoveredRegion))
        return false;
      child = child->GetNextSibling();
    }
  } else {
    nsIntRegion rgn = aLayer->GetVisibleRegion();
    rgn.MoveBy(offset);
    nsIntRegion tmp;
    tmp.And(rgn, *aCoveredRegion);
    if (!tmp.IsEmpty())
      return false;
    aCoveredRegion->Or(*aCoveredRegion, rgn);
  }

  return true;
}
#endif

NS_IMETHODIMP
nsDOMWindowUtils::LeafLayersPartitionWindow(bool* aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  *aResult = true;
#ifdef DEBUG
  nsIWidget* widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;
  LayerManager* manager = widget->GetLayerManager();
  if (!manager)
    return NS_ERROR_FAILURE;
  nsPresContext* presContext = GetPresContext();
  if (!presContext)
    return NS_ERROR_FAILURE;
  Layer* root = manager->GetRoot();
  if (!root)
    return NS_ERROR_FAILURE;

  nsIntPoint offset(0, 0);
  nsIntRegion coveredRegion;
  if (!CheckLeafLayers(root, offset, &coveredRegion)) {
    *aResult = false;
  }
  if (!coveredRegion.IsEqual(root->GetVisibleRegion())) {
    *aResult = false;
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::CheckAndClearPaintedState(nsIDOMElement* aElement, bool* aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIFrame* frame = content->GetPrimaryFrame();

  if (!frame) {
    *aResult = false;
    return NS_OK;
  }

  // Get the outermost frame for the content node, so that we can test
  // canvasframe invalidations by observing the documentElement.
  for (;;) {
    nsIFrame* parentFrame = frame->GetParent();
    if (parentFrame && parentFrame->GetContent() == content) {
      frame = parentFrame;
    } else {
      break;
    }
  }

  *aResult = frame->CheckAndClearPaintedState();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::IsPartOfOpaqueLayer(nsIDOMElement* aElement, bool* aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (!aElement) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIFrame* frame = content->GetPrimaryFrame();
  if (!frame) {
    return NS_ERROR_FAILURE;
  }

  Layer* layer = FrameLayerBuilder::GetDebugSingleOldLayerForFrame(frame);
  if (!layer || !layer->AsPaintedLayer()) {
    return NS_ERROR_FAILURE;
  }

  *aResult = (layer->GetContentFlags() & Layer::CONTENT_OPAQUE);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::NumberOfAssignedPaintedLayers(nsIDOMElement** aElements,
                                                uint32_t aCount,
                                                uint32_t* aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (!aElements) {
    return NS_ERROR_INVALID_ARG;
  }

  nsTHashtable<nsPtrHashKey<PaintedLayer>> layers;
  nsresult rv;
  for (uint32_t i = 0; i < aCount; i++) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElements[i], &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIFrame* frame = content->GetPrimaryFrame();
    if (!frame) {
      return NS_ERROR_FAILURE;
    }

    Layer* layer = FrameLayerBuilder::GetDebugSingleOldLayerForFrame(frame);
    if (!layer || !layer->AsPaintedLayer()) {
      return NS_ERROR_FAILURE;
    }

    layers.PutEntry(layer->AsPaintedLayer());
  }

  *aResult = layers.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::EnableDialogs()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  static_cast<nsGlobalWindow*>(window.get())->EnableDialogs();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::DisableDialogs()
{
  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  static_cast<nsGlobalWindow*>(window.get())->DisableDialogs();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::AreDialogsEnabled(bool* aResult)
{
  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  *aResult = static_cast<nsGlobalWindow*>(window.get())->AreDialogsEnabled();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFileId(JS::Handle<JS::Value> aFile, JSContext* aCx,
                            int64_t* _retval)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (aFile.isPrimitive()) {
    *_retval = -1;
    return NS_OK;
  }

  JSObject* obj = aFile.toObjectOrNull();

  indexedDB::IDBMutableFile* mutableFile = nullptr;
  if (NS_SUCCEEDED(UNWRAP_OBJECT(IDBMutableFile, obj, mutableFile))) {
    *_retval = mutableFile->GetFileId();
    return NS_OK;
  }

  Blob* blob = nullptr;
  if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, obj, blob))) {
    *_retval = blob->GetFileId();
    return NS_OK;
  }

  *_retval = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFilePath(JS::HandleValue aFile, JSContext* aCx,
                              nsAString& _retval)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (aFile.isPrimitive()) {
    _retval.Truncate();
    return NS_OK;
  }

  JSObject* obj = aFile.toObjectOrNull();

  File* file = nullptr;
  if (NS_SUCCEEDED(UNWRAP_OBJECT(File, obj, file))) {
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
                                    int32_t* aSliceRefCnt, JSContext* aCx,
                                    bool* aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCString origin;
  nsresult rv =
    quota::QuotaManager::GetInfoFromWindow(window, nullptr, &origin, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  IDBOpenDBOptions options;
  JS::Rooted<JS::Value> optionsVal(aCx, aOptions);
  if (!options.Init(aCx, optionsVal)) {
    return NS_ERROR_TYPE_ERR;
  }

  quota::PersistenceType persistenceType =
    quota::PersistenceTypeFromStorage(options.mStorage);

  nsRefPtr<indexedDB::IndexedDatabaseManager> mgr =
    indexedDB::IndexedDatabaseManager::Get();

  if (mgr) {
    rv = mgr->BlockAndGetFileReferences(persistenceType, origin, aDatabaseName,
                                        aId, aRefCnt, aDBRefCnt, aSliceRefCnt,
                                        aResult);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    *aRefCnt = *aDBRefCnt = *aSliceRefCnt = -1;
    *aResult = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::FlushPendingFileDeletions()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  using mozilla::dom::indexedDB::IndexedDatabaseManager;

  nsRefPtr<IndexedDatabaseManager> mgr = IndexedDatabaseManager::Get();
  if (mgr) {
    nsresult rv = mgr->FlushPendingFileDeletions();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::IsIncrementalGCEnabled(JSContext* cx, bool* aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  *aResult = JS::IsIncrementalGCEnabled(JS_GetRuntime(cx));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StartPCCountProfiling(JSContext* cx)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  js::StartPCCountProfiling(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StopPCCountProfiling(JSContext* cx)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  js::StopPCCountProfiling(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::PurgePCCounts(JSContext* cx)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  js::PurgePCCounts(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPCCountScriptCount(JSContext* cx, int32_t *result)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  *result = js::GetPCCountScriptCount(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPCCountScriptSummary(int32_t script, JSContext* cx, nsAString& result)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  JSString *text = js::GetPCCountScriptSummary(cx, script);
  if (!text)
    return NS_ERROR_FAILURE;

  if (!AssignJSString(cx, result, text))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPCCountScriptContents(int32_t script, JSContext* cx, nsAString& result)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  JSString *text = js::GetPCCountScriptContents(cx, script);
  if (!text)
    return NS_ERROR_FAILURE;

  if (!AssignJSString(cx, result, text))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPaintingSuppressed(bool *aPaintingSuppressed)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsIDocShell *docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  *aPaintingSuppressed = presShell->IsPaintingSuppressed();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPlugins(JSContext* cx, JS::MutableHandle<JS::Value> aPlugins)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  nsTArray<nsIObjectLoadingContent*> plugins;
  doc->GetPlugins(plugins);

  JS::Rooted<JSObject*> jsPlugins(cx);
  nsresult rv = nsTArrayToJSArray(cx, plugins, &jsPlugins);
  NS_ENSURE_SUCCESS(rv, rv);

  aPlugins.setObject(*jsPlugins);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetScrollPositionClampingScrollPortSize(float aWidth, float aHeight)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (!(aWidth >= 0.0 && aHeight >= 0.0)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  nsLayoutUtils::SetScrollPositionClampingScrollPortSize(presShell, CSSSize(aWidth, aHeight));

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetContentDocumentFixedPositionMargins(float aTop, float aRight,
                                                         float aBottom, float aLeft)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (!(aTop >= 0.0f && aRight >= 0.0f && aBottom >= 0.0f && aLeft >= 0.0f)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  nsMargin margins(nsPresContext::CSSPixelsToAppUnits(aTop),
                   nsPresContext::CSSPixelsToAppUnits(aRight),
                   nsPresContext::CSSPixelsToAppUnits(aBottom),
                   nsPresContext::CSSPixelsToAppUnits(aLeft));
  presShell->SetContentDocumentFixedPositionMargins(margins);

  return NS_OK;
}

nsresult
nsDOMWindowUtils::RemoteFrameFullscreenChanged(nsIDOMElement* aFrameElement)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  doc->RemoteFrameFullscreenChanged(aFrameElement);
  return NS_OK;
}

nsresult
nsDOMWindowUtils::RemoteFrameFullscreenReverted()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  doc->RemoteFrameFullscreenReverted();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::HandleFullscreenRequests(bool* aRetVal)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  *aRetVal = nsIDocument::HandlePendingFullscreenRequests(doc);
  return NS_OK;
}

nsresult
nsDOMWindowUtils::ExitFullscreen()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_STATE(doc);

  nsIDocument::ExitFullscreen(doc, /* async */ false);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SelectAtPoint(float aX, float aY, uint32_t aSelectBehavior,
                                bool *_retval)
{
  *_retval = false;
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

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

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_UNEXPECTED;
  }

  // The root frame for this content window
  nsIFrame* rootFrame = presShell->FrameManager()->GetRootFrame();
  if (!rootFrame) {
    return NS_ERROR_UNEXPECTED;
  }

  // Get the target frame at the client coordinates passed to us
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  LayoutDeviceIntPoint pt =
    nsContentUtils::ToWidgetPoint(CSSPoint(aX, aY), offset, GetPresContext());
  nsPoint ptInRoot =
    nsLayoutUtils::GetEventCoordinatesRelativeTo(widget, pt, rootFrame);
  nsIFrame* targetFrame = nsLayoutUtils::GetFrameForPoint(rootFrame, ptInRoot);
  // This can happen if the page hasn't loaded yet or if the point
  // is outside the frame.
  if (!targetFrame) {
    return NS_ERROR_INVALID_ARG;
  }

  // Convert point to coordinates relative to the target frame, which is
  // what targetFrame's SelectByTypeAtPoint expects.
  nsPoint relPoint =
    nsLayoutUtils::GetEventCoordinatesRelativeTo(widget, pt, targetFrame);

  nsresult rv =
    static_cast<nsFrame*>(targetFrame)->
      SelectByTypeAtPoint(GetPresContext(), relPoint, amount, amount,
                          nsFrame::SELECT_ACCUMULATE);
  *_retval = !NS_FAILED(rv);
  return NS_OK;
}

static nsIDocument::additionalSheetType
convertSheetType(uint32_t aSheetType)
{
  switch(aSheetType) {
    case nsDOMWindowUtils::AGENT_SHEET:
      return nsIDocument::eAgentSheet;
    case nsDOMWindowUtils::USER_SHEET:
      return nsIDocument::eUserSheet;
    case nsDOMWindowUtils::AUTHOR_SHEET:
      return nsIDocument::eAuthorSheet;
    default:
      NS_ASSERTION(false, "wrong type");
      // we must return something although this should never happen
      return nsIDocument::SheetTypeCount;
  }
}

NS_IMETHODIMP
nsDOMWindowUtils::LoadSheet(nsIURI *aSheetURI, uint32_t aSheetType)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_ARG_POINTER(aSheetURI);
  NS_ENSURE_ARG(aSheetType == AGENT_SHEET ||
                aSheetType == USER_SHEET ||
                aSheetType == AUTHOR_SHEET);

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsIDocument::additionalSheetType type = convertSheetType(aSheetType);

  return doc->LoadAdditionalStyleSheet(type, aSheetURI);
}

NS_IMETHODIMP
nsDOMWindowUtils::LoadSheetUsingURIString(const nsACString& aSheetURI, uint32_t aSheetType)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aSheetURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return LoadSheet(uri, aSheetType);
}

NS_IMETHODIMP
nsDOMWindowUtils::AddSheet(nsIDOMStyleSheet *aSheet, uint32_t aSheetType)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_ARG_POINTER(aSheet);
  NS_ENSURE_ARG(aSheetType == AGENT_SHEET ||
                aSheetType == USER_SHEET ||
                aSheetType == AUTHOR_SHEET);

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsIDocument::additionalSheetType type = convertSheetType(aSheetType);
  nsCOMPtr<nsIStyleSheet> sheet = do_QueryInterface(aSheet);
  NS_ENSURE_TRUE(sheet, NS_ERROR_FAILURE);
  if (sheet->GetOwningDocument()) {
    return NS_ERROR_INVALID_ARG;
  }
  return doc->AddAdditionalStyleSheet(type, sheet);
}

NS_IMETHODIMP
nsDOMWindowUtils::RemoveSheet(nsIURI *aSheetURI, uint32_t aSheetType)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  NS_ENSURE_ARG_POINTER(aSheetURI);
  NS_ENSURE_ARG(aSheetType == AGENT_SHEET ||
                aSheetType == USER_SHEET ||
                aSheetType == AUTHOR_SHEET);

  nsCOMPtr<nsIDocument> doc = GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsIDocument::additionalSheetType type = convertSheetType(aSheetType);

  doc->RemoveAdditionalStyleSheet(type, aSheetURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::RemoveSheetUsingURIString(const nsACString& aSheetURI, uint32_t aSheetType)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aSheetURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return RemoveSheet(uri, aSheetType);
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsHandlingUserInput(bool* aHandlingUserInput)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  *aHandlingUserInput = EventStateManager::IsHandlingUserInput();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::AllowScriptsToClose()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);
  static_cast<nsGlobalWindow*>(window.get())->AllowScriptsToClose();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIsParentWindowMainWidgetVisible(bool* aIsVisible)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  // this should reflect the "is parent window visible" logic in
  // nsWindowWatcher::OpenWindowInternal()
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  nsCOMPtr<nsIWidget> parentWidget;
  nsIDocShell *docShell = window->GetDocShell();
  if (docShell) {
    if (TabChild *tabChild = TabChild::GetFrom(docShell)) {
      if (!tabChild->SendIsParentWindowMainWidgetVisible(aIsVisible))
        return NS_ERROR_FAILURE;
      return NS_OK;
    }

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
nsDOMWindowUtils::IsNodeDisabledForEvents(nsIDOMNode* aNode, bool* aRetVal)
{
  *aRetVal = false;
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsCOMPtr<nsINode> n = do_QueryInterface(aNode);
  nsINode* node = n;
  while (node) {
    if (node->IsNodeOfType(nsINode::eHTML_FORM_CONTROL)) {
      nsCOMPtr<nsIFormControl> fc = do_QueryInterface(node);
      if (fc && fc->IsDisabledForEvents(NS_EVENT_NULL)) {
        *aRetVal = true;
        break;
      }
    }
    node = node->GetParentNode();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetPaintFlashing(bool aPaintFlashing)
{
  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    presContext->SetPaintFlashing(aPaintFlashing);
    // Clear paint flashing colors
    nsIPresShell* presShell = GetPresShell();
    if (!aPaintFlashing && presShell) {
      nsIFrame* rootFrame = presShell->GetRootFrame();
      if (rootFrame) {
        rootFrame->InvalidateFrameSubtree();
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPaintFlashing(bool* aRetVal)
{
  *aRetVal = false;
  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    *aRetVal = presContext->GetPaintFlashing();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::DispatchEventToChromeOnly(nsIDOMEventTarget* aTarget,
                                            nsIDOMEvent* aEvent,
                                            bool* aRetVal)
{
  *aRetVal = false;
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  NS_ENSURE_STATE(aTarget && aEvent);
  aEvent->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;
  aTarget->DispatchEvent(aEvent, aRetVal);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::RunInStableState(nsIRunnable *aRunnable)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIRunnable> runnable = aRunnable;
  nsContentUtils::RunInStableState(runnable.forget());

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::RunBeforeNextEvent(nsIRunnable *runnable)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsIAppShell> appShell(do_GetService(kAppShellCID));
  if (!appShell) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return appShell->RunBeforeNextEvent(runnable);
}

NS_IMETHODIMP
nsDOMWindowUtils::RequestCompositorProperty(const nsAString& property,
                                            float* aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (nsIWidget* widget = GetWidget()) {
    mozilla::layers::LayerManager* manager = widget->GetLayerManager();
    if (manager) {
      *aResult = manager->RequestProperty(property);
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetOMTAStyle(nsIDOMElement* aElement,
                               const nsAString& aProperty,
                               const nsAString& aPseudoElement,
                               nsAString& aResult)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  if (!element) {
    return NS_ERROR_INVALID_ARG;
  }

  nsRefPtr<nsROCSSPrimitiveValue> cssValue = nullptr;
  nsIFrame* frame = element->GetPrimaryFrame();
  if (frame && !aPseudoElement.IsEmpty()) {
    if (aPseudoElement.EqualsLiteral("::before")) {
      frame = nsLayoutUtils::GetBeforeFrame(frame);
    } else if (aPseudoElement.EqualsLiteral("::after")) {
      frame = nsLayoutUtils::GetAfterFrame(frame);
    } else {
      return NS_ERROR_INVALID_ARG;
    }
  }
  if (frame && nsLayoutUtils::AreAsyncAnimationsEnabled()) {
    if (aProperty.EqualsLiteral("opacity")) {
      Layer* layer =
        FrameLayerBuilder::GetDedicatedLayer(frame,
                                             nsDisplayItem::TYPE_OPACITY);
      if (layer) {
        float value;
        ShadowLayerForwarder* forwarder = layer->Manager()->AsShadowForwarder();
        if (forwarder && forwarder->HasShadowManager()) {
          forwarder->GetShadowManager()->SendGetOpacity(
            layer->AsShadowableLayer()->GetShadow(), &value);
          cssValue = new nsROCSSPrimitiveValue;
          cssValue->SetNumber(value);
        }
      }
    } else if (aProperty.EqualsLiteral("transform")) {
      Layer* layer =
        FrameLayerBuilder::GetDedicatedLayer(frame,
                                             nsDisplayItem::TYPE_TRANSFORM);
      if (layer) {
        ShadowLayerForwarder* forwarder = layer->Manager()->AsShadowForwarder();
        if (forwarder && forwarder->HasShadowManager()) {
          MaybeTransform transform;
          forwarder->GetShadowManager()->SendGetAnimationTransform(
            layer->AsShadowableLayer()->GetShadow(), &transform);
          if (transform.type() == MaybeTransform::TMatrix4x4) {
            gfx3DMatrix matrix = To3DMatrix(transform.get_Matrix4x4());
            cssValue = nsComputedDOMStyle::MatrixToCSSValue(matrix);
          }
        }
      }
    }
  }

  if (cssValue) {
    nsString text;
    ErrorResult rv;
    cssValue->GetCssText(text, rv);
    aResult.Assign(text);
    return rv.StealNSResult();
  } else {
    aResult.Truncate();
  }

  return NS_OK;
}

namespace {

class HandlingUserInputHelper final : public nsIJSRAIIHelper
{
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
  : mHandlingUserInput(aHandlingUserInput),
    mDestructCalled(false)
{
  if (aHandlingUserInput) {
    EventStateManager::StartHandlingUserInput();
  }
}

HandlingUserInputHelper::~HandlingUserInputHelper()
{
  // We assert, but just in case, make sure we notify the ESM.
  MOZ_ASSERT(mDestructCalled);
  if (!mDestructCalled) {
    Destruct();
  }
}

NS_IMETHODIMP
HandlingUserInputHelper::Destruct()
{
  if (NS_WARN_IF(mDestructCalled)) {
    return NS_ERROR_FAILURE;
  }

  mDestructCalled = true;
  if (mHandlingUserInput) {
    EventStateManager::StopHandlingUserInput();
  }

  return NS_OK;
}

} // unnamed namespace

NS_IMETHODIMP
nsDOMWindowUtils::SetHandlingUserInput(bool aHandlingUserInput,
                                       nsIJSRAIIHelper** aHelper)
{
  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsRefPtr<HandlingUserInputHelper> helper(
    new HandlingUserInputHelper(aHandlingUserInput));
  helper.forget(aHelper);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetContentAPZTestData(JSContext* aContext,
                                        JS::MutableHandleValue aOutContentTestData)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (nsIWidget* widget = GetWidget()) {
    nsRefPtr<LayerManager> lm = widget->GetLayerManager();
    if (lm && lm->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
      ClientLayerManager* clm = static_cast<ClientLayerManager*>(lm.get());
      if (!clm->GetAPZTestData().ToJS(aOutContentTestData, aContext)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetCompositorAPZTestData(JSContext* aContext,
                                           JS::MutableHandleValue aOutCompositorTestData)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  if (nsIWidget* widget = GetWidget()) {
    nsRefPtr<LayerManager> lm = widget->GetLayerManager();
    if (lm && lm->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
      ClientLayerManager* clm = static_cast<ClientLayerManager*>(lm.get());
      APZTestData compositorSideData;
      clm->GetCompositorSideAPZTestData(&compositorSideData);
      if (!compositorSideData.ToJS(aOutCompositorTestData, aContext)) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::PostRestyleSelfEvent(nsIDOMElement* aElement)
{
  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  if (!element) {
    return NS_ERROR_INVALID_ARG;
  }

  nsLayoutUtils::PostRestyleEvent(element, eRestyle_Self, nsChangeHint(0));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetAudioMuted(bool* aMuted)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  *aMuted = window->GetAudioMuted();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetAudioMuted(bool aMuted)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  window->SetAudioMuted(aMuted);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetAudioVolume(float* aVolume)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  *aVolume = window->GetAudioVolume();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetAudioVolume(float aVolume)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  return window->SetAudioVolume(aVolume);
}

NS_IMETHODIMP
nsDOMWindowUtils::SetChromeMargin(int32_t aTop,
                                  int32_t aRight,
                                  int32_t aBottom,
                                  int32_t aLeft)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  if (window) {
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(window->GetDocShell());
    if (baseWindow) {
      nsCOMPtr<nsIWidget> widget;
      baseWindow->GetMainWidget(getter_AddRefs(widget));
      if (widget) {
        nsIntMargin margins(aTop, aRight, aBottom, aLeft);
        return widget->SetNonClientMargins(margins);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFrameUniformityTestData(JSContext* aContext,
                                             JS::MutableHandleValue aOutFrameUniformity)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  nsIWidget* widget = GetWidget();
  if (!widget) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<LayerManager> manager = widget->GetLayerManager();
  if (!manager) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  FrameUniformityData outData;
  manager->GetFrameUniformity(&outData);
  outData.ToJS(aOutFrameUniformity, aContext);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::XpconnectArgument(nsIDOMWindowUtils* aThis)
{
  // Do nothing.
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::AskPermission(nsIContentPermissionRequest* aRequest)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  return nsContentPermissionUtils::AskPermission(aRequest, window->GetCurrentInnerWindow());
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFramesConstructed(uint64_t* aResult)
{
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aResult = presContext->FramesConstructedCount();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFramesReflowed(uint64_t* aResult)
{
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aResult = presContext->FramesReflowedCount();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetServiceWorkersTestingEnabled(bool aEnabled)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  window->SetServiceWorkersTestingEnabled(aEnabled);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetServiceWorkersTestingEnabled(bool *aEnabled)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  *aEnabled = window->GetServiceWorkersTestingEnabled();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::EnterChaosMode()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  ChaosMode::enterChaosMode();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::LeaveChaosMode()
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());
  ChaosMode::leaveChaosMode();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::HasRuleProcessorUsedByMultipleStyleSets(uint32_t aSheetType,
                                                          bool* aRetVal)
{
  MOZ_RELEASE_ASSERT(nsContentUtils::IsCallerChrome());

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  return presShell->HasRuleProcessorUsedByMultipleStyleSets(aSheetType,
                                                            aRetVal);
}

NS_INTERFACE_MAP_BEGIN(nsTranslationNodeList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsITranslationNodeList)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsTranslationNodeList)
NS_IMPL_RELEASE(nsTranslationNodeList)

NS_IMETHODIMP
nsTranslationNodeList::Item(uint32_t aIndex, nsIDOMNode** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  NS_IF_ADDREF(*aRetVal = mNodes.SafeElementAt(aIndex));
  return NS_OK;
}

NS_IMETHODIMP
nsTranslationNodeList::IsTranslationRootAtIndex(uint32_t aIndex, bool* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  if (aIndex >= mLength) {
    *aRetVal = false;
    return NS_OK;
  }

  *aRetVal = mNodeIsRoot.ElementAt(aIndex);
  return NS_OK;
}

NS_IMETHODIMP
nsTranslationNodeList::GetLength(uint32_t* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  *aRetVal = mLength;
  return NS_OK;
}
