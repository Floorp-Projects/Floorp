/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMError.h"
#include "nsIDOMNSEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsDOMWindowUtils.h"
#include "nsQueryContentEventResult.h"
#include "nsGlobalWindow.h"
#include "nsIDocument.h"
#include "nsFocusManager.h"
#include "nsEventStateManager.h"
#include "nsFrameManager.h"
#include "nsRefreshDriver.h"
#include "nsDOMTouchEvent.h"
#include "nsIDOMTouchEvent.h"
#include "nsObjectLoadingContent.h"

#include "nsIScrollableFrame.h"

#include "nsContentUtils.h"
#include "nsLayoutUtils.h"

#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsGUIEvent.h"
#include "nsCharsetSource.h"
#include "nsJSEnvironment.h"
#include "nsJSUtils.h"

#include "nsIViewManager.h"

#include "nsIDOMHTMLCanvasElement.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsLayoutUtils.h"
#include "nsComputedDOMStyle.h"
#include "nsIPresShell.h"
#include "nsStyleAnimation.h"
#include "nsCSSProps.h"
#include "nsDOMFile.h"
#include "BasicLayers.h"
#include "nsTArrayHelpers.h"

#if defined(MOZ_X11) && defined(MOZ_WIDGET_GTK2)
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#endif

#include "Layers.h"
#include "nsIIOService.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/indexedDB/FileInfo.h"
#include "mozilla/dom/indexedDB/IndexedDatabaseManager.h"
#include "sampler.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::widget;

static bool IsUniversalXPConnectCapable()
{
  bool hasCap = false;
  nsresult rv = nsContentUtils::GetSecurityManager()->
                  IsCapabilityEnabled("UniversalXPConnect", &hasCap);
  NS_ENSURE_SUCCESS(rv, false);
  return hasCap;
}

DOMCI_DATA(WindowUtils, nsDOMWindowUtils)

NS_INTERFACE_MAP_BEGIN(nsDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WindowUtils)
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
    return nsnull;

  nsIDocShell *docShell = window->GetDocShell();
  if (!docShell)
    return nsnull;

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  return presShell;
}

nsPresContext*
nsDOMWindowUtils::GetPresContext()
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  if (!window)
    return nsnull;
  nsIDocShell *docShell = window->GetDocShell();
  if (!docShell)
    return nsnull;
  nsRefPtr<nsPresContext> presContext;
  docShell->GetPresContext(getter_AddRefs(presContext));
  return presContext;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetImageAnimationMode(PRUint16 *aMode)
{
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
nsDOMWindowUtils::SetImageAnimationMode(PRUint16 aMode)
{
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

  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  if (window) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(window->GetExtantDocument()));
    *aIsForced = doc &&
      doc->GetDocumentCharacterSetSource() >= kCharsetFromParentForced;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetDocumentMetadata(const nsAString& aName,
                                      nsAString& aValue)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  if (window) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(window->GetExtantDocument()));
    if (doc) {
      nsCOMPtr<nsIAtom> name = do_GetAtom(aName);
      doc->GetHeaderData(name, aValue);
      return NS_OK;
    }
  }
  
  aValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::Redraw(PRUint32 aCount, PRUint32 *aDurationOut)
{
  if (aCount == 0)
    aCount = 1;

  if (nsIPresShell* presShell = GetPresShell()) {
    nsIFrame *rootFrame = presShell->GetRootFrame();

    if (rootFrame) {
      nsRect r(nsPoint(0, 0), rootFrame->GetSize());

      PRIntervalTime iStart = PR_IntervalNow();

      for (PRUint32 i = 0; i < aCount; i++)
        rootFrame->InvalidateWithFlags(r, nsIFrame::INVALIDATE_IMMEDIATE);

#if defined(MOZ_X11) && defined(MOZ_WIDGET_GTK2)
      XSync(GDK_DISPLAY(), False);
#endif

      *aDurationOut = PR_IntervalToMilliseconds(PR_IntervalNow() - iStart);

      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetCSSViewport(float aWidthPx, float aHeightPx)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (!(aWidthPx >= 0.0 && aHeightPx >= 0.0)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  nscoord width = nsPresContext::CSSPixelsToAppUnits(aWidthPx);
  nscoord height = nsPresContext::CSSPixelsToAppUnits(aHeightPx);

  presShell->ResizeReflowOverride(width, height);

  return NS_OK;
}

static void DestroyNsRect(void* aObject, nsIAtom* aPropertyName,
                          void* aPropertyValue, void* aData)
{
  nsRect* rect = static_cast<nsRect*>(aPropertyValue);
  delete rect;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetDisplayPortForElement(float aXPx, float aYPx,
                                           float aWidthPx, float aHeightPx,
                                           nsIDOMElement* aElement)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  } 

  nsRect displayport(nsPresContext::CSSPixelsToAppUnits(aXPx),
                     nsPresContext::CSSPixelsToAppUnits(aYPx),
                     nsPresContext::CSSPixelsToAppUnits(aWidthPx),
                     nsPresContext::CSSPixelsToAppUnits(aHeightPx));

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

  nsRect lastDisplayPort;
  if (nsLayoutUtils::GetDisplayPort(content, &lastDisplayPort) &&
      displayport.IsEqualInterior(lastDisplayPort)) {
    return NS_OK;
  }

  content->SetProperty(nsGkAtoms::DisplayPort, new nsRect(displayport),
                       DestroyNsRect);

  nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
  if (rootScrollFrame) {
    if (content == rootScrollFrame->GetContent()) {
      // We are setting a root displayport for a document.
      // The pres shell needs a special flag set.
      presShell->SetIgnoreViewportScrolling(true);

      // The root document currently has a widget, but we might end up
      // painting content inside the displayport but outside the widget
      // bounds. This ensures the document's view honors invalidations
      // within the displayport.
      nsPresContext* presContext = GetPresContext();
      if (presContext && presContext->IsRoot()) {
        nsIFrame* rootFrame = presShell->GetRootFrame();
        nsIView* view = rootFrame->GetView();
        if (view) {
          view->SetInvalidationDimensions(&displayport);
        }
      }
    }
  }

  if (presShell) {
    nsIFrame* rootFrame = presShell->FrameManager()->GetRootFrame();
    if (rootFrame) {
      nsIContent* rootContent =
        rootScrollFrame ? rootScrollFrame->GetContent() : nsnull;
      nsRect rootDisplayport;
      bool usingDisplayport = rootContent &&
        nsLayoutUtils::GetDisplayPort(rootContent, &rootDisplayport);
      rootFrame->InvalidateWithFlags(
        usingDisplayport ? rootDisplayport : rootFrame->GetVisualOverflowRect(),
        nsIFrame::INVALIDATE_NO_THEBES_LAYERS);

      // Send empty paint transaction in order to release retained layers
      if (displayport.IsEmpty()) {
        nsCOMPtr<nsIWidget> widget = GetWidget();
        if (widget) {
          bool isRetainingManager;
          LayerManager* manager = widget->GetLayerManager(&isRetainingManager);
          if (isRetainingManager) {
            manager->BeginTransaction();
            nsLayoutUtils::PaintFrame(nsnull, rootFrame, nsRegion(), NS_RGB(255, 255, 255),
                                      nsLayoutUtils::PAINT_WIDGET_LAYERS |
                                      nsLayoutUtils::PAINT_EXISTING_TRANSACTION);
          }
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetResolution(float aXResolution, float aYResolution)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsIPresShell* presShell = GetPresShell();
  return presShell ? presShell->SetResolution(aXResolution, aYResolution)
                   : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetIsFirstPaint(bool aIsFirstPaint)
{
  if (!IsUniversalXPConnectCapable()) {
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
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsIPresShell* presShell = GetPresShell();
  if (presShell) {
    *aIsFirstPaint = presShell->GetIsFirstPaint();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/* static */
mozilla::widget::Modifiers
nsDOMWindowUtils::GetWidgetModifiers(PRInt32 aModifiers)
{
  widget::Modifiers result = 0;
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_SHIFT) {
    result |= widget::MODIFIER_SHIFT;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_CONTROL) {
    result |= widget::MODIFIER_CONTROL;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_ALT) {
    result |= widget::MODIFIER_ALT;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_META) {
    result |= widget::MODIFIER_META;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_ALTGRAPH) {
    result |= widget::MODIFIER_ALTGRAPH;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_CAPSLOCK) {
    result |= widget::MODIFIER_CAPSLOCK;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_FN) {
    result |= widget::MODIFIER_FN;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_NUMLOCK) {
    result |= widget::MODIFIER_NUMLOCK;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_SCROLL) {
    result |= widget::MODIFIER_SCROLL;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_SYMBOLLOCK) {
    result |= widget::MODIFIER_SYMBOLLOCK;
  }
  if (aModifiers & nsIDOMWindowUtils::MODIFIER_WIN) {
    result |= widget::MODIFIER_WIN;
  }
  return result;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendMouseEvent(const nsAString& aType,
                                 float aX,
                                 float aY,
                                 PRInt32 aButton,
                                 PRInt32 aClickCount,
                                 PRInt32 aModifiers,
                                 bool aIgnoreRootScrollFrame)
{
  return SendMouseEventCommon(aType, aX, aY, aButton, aClickCount, aModifiers,
                              aIgnoreRootScrollFrame, false);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendMouseEventToWindow(const nsAString& aType,
                                         float aX,
                                         float aY,
                                         PRInt32 aButton,
                                         PRInt32 aClickCount,
                                         PRInt32 aModifiers,
                                         bool aIgnoreRootScrollFrame)
{
  return SendMouseEventCommon(aType, aX, aY, aButton, aClickCount, aModifiers,
                              aIgnoreRootScrollFrame, true);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendMouseEventCommon(const nsAString& aType,
                                       float aX,
                                       float aY,
                                       PRInt32 aButton,
                                       PRInt32 aClickCount,
                                       PRInt32 aModifiers,
                                       bool aIgnoreRootScrollFrame,
                                       bool aToWindow)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget)
    return NS_ERROR_FAILURE;

  PRInt32 msg;
  bool contextMenuKey = false;
  if (aType.EqualsLiteral("mousedown"))
    msg = NS_MOUSE_BUTTON_DOWN;
  else if (aType.EqualsLiteral("mouseup"))
    msg = NS_MOUSE_BUTTON_UP;
  else if (aType.EqualsLiteral("mousemove"))
    msg = NS_MOUSE_MOVE;
  else if (aType.EqualsLiteral("mouseover"))
    msg = NS_MOUSE_ENTER;
  else if (aType.EqualsLiteral("mouseout"))
    msg = NS_MOUSE_EXIT;
  else if (aType.EqualsLiteral("contextmenu")) {
    msg = NS_CONTEXTMENU;
    contextMenuKey = (aButton == 0);
  } else
    return NS_ERROR_FAILURE;

  nsMouseEvent event(true, msg, widget, nsMouseEvent::eReal,
                     contextMenuKey ?
                       nsMouseEvent::eContextMenuKey : nsMouseEvent::eNormal);
  event.modifiers = GetWidgetModifiers(aModifiers);
  event.button = aButton;
  event.widget = widget;

  event.clickCount = aClickCount;
  event.time = PR_IntervalNow();
  event.flags |= NS_EVENT_FLAG_SYNTHETIC_TEST_EVENT;

  nsPresContext* presContext = GetPresContext();
  if (!presContext)
    return NS_ERROR_FAILURE;

  PRInt32 appPerDev = presContext->AppUnitsPerDevPixel();
  event.refPoint.x =
    NSAppUnitsToIntPixels(nsPresContext::CSSPixelsToAppUnits(aX) + offset.x,
                          appPerDev);
  event.refPoint.y =
    NSAppUnitsToIntPixels(nsPresContext::CSSPixelsToAppUnits(aY) + offset.y,
                          appPerDev);
  event.ignoreRootScrollFrame = aIgnoreRootScrollFrame;

  nsEventStatus status;
  if (aToWindow) {
    nsCOMPtr<nsIPresShell> presShell = presContext->PresShell();
    if (!presShell)
      return NS_ERROR_FAILURE;
    nsIViewManager* viewManager = presShell->GetViewManager();
    if (!viewManager)
      return NS_ERROR_FAILURE;
    nsIView* view = viewManager->GetRootView();
    if (!view)
      return NS_ERROR_FAILURE;

    status = nsEventStatus_eIgnore;
    return presShell->HandleEvent(view->GetFrame(), &event, false, &status);
  }
  return widget->DispatchEvent(&event, status);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendMouseScrollEvent(const nsAString& aType,
                                       float aX,
                                       float aY,
                                       PRInt32 aButton,
                                       PRInt32 aScrollFlags,
                                       PRInt32 aDelta,
                                       PRInt32 aModifiers)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget)
    return NS_ERROR_NULL_POINTER;

  PRInt32 msg;
  if (aType.EqualsLiteral("DOMMouseScroll"))
    msg = NS_MOUSE_SCROLL;
  else if (aType.EqualsLiteral("MozMousePixelScroll"))
    msg = NS_MOUSE_PIXEL_SCROLL;
  else
    return NS_ERROR_UNEXPECTED;

  nsMouseScrollEvent event(true, msg, widget);
  event.modifiers = GetWidgetModifiers(aModifiers);
  event.button = aButton;
  event.widget = widget;
  event.delta = aDelta;
  event.scrollFlags = aScrollFlags;

  event.time = PR_IntervalNow();

  nsPresContext* presContext = GetPresContext();
  if (!presContext)
    return NS_ERROR_FAILURE;

  PRInt32 appPerDev = presContext->AppUnitsPerDevPixel();
  event.refPoint.x =
    NSAppUnitsToIntPixels(nsPresContext::CSSPixelsToAppUnits(aX) + offset.x,
                          appPerDev);
  event.refPoint.y =
    NSAppUnitsToIntPixels(nsPresContext::CSSPixelsToAppUnits(aY) + offset.y,
                          appPerDev);

  nsEventStatus status;
  return widget->DispatchEvent(&event, status);
}


NS_IMETHODIMP
nsDOMWindowUtils::SendTouchEvent(const nsAString& aType,
                                 PRUint32 *aIdentifiers,
                                 PRInt32 *aXs,
                                 PRInt32 *aYs,
                                 PRUint32 *aRxs,
                                 PRUint32 *aRys,
                                 float *aRotationAngles,
                                 float *aForces,
                                 PRUint32 aCount,
                                 PRInt32 aModifiers,
                                 bool aIgnoreRootScrollFrame,
                                 bool *aPreventDefault)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget) {
    return NS_ERROR_NULL_POINTER;
  }
  PRInt32 msg;
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
  nsTouchEvent event(true, msg, widget);
  event.modifiers = GetWidgetModifiers(aModifiers);
  event.widget = widget;
  event.time = PR_Now();

  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return NS_ERROR_FAILURE;
  }
  event.touches.SetCapacity(aCount);
  PRInt32 appPerDev = presContext->AppUnitsPerDevPixel();
  for (PRUint32 i = 0; i < aCount; ++i) {
    nsIntPoint pt(0, 0);
    pt.x =
      NSAppUnitsToIntPixels(nsPresContext::CSSPixelsToAppUnits(aXs[i]) + offset.x,
                            appPerDev);
    pt.y =
      NSAppUnitsToIntPixels(nsPresContext::CSSPixelsToAppUnits(aYs[i]) + offset.y,
                            appPerDev);
    nsCOMPtr<nsIDOMTouch> t(new nsDOMTouch(aIdentifiers[i],
                                           pt,
                                           nsIntPoint(aRxs[i], aRys[i]),
                                           aRotationAngles[i],
                                           aForces[i]));
    event.touches.AppendElement(t);
  }

  nsEventStatus status;
  nsresult rv = widget->DispatchEvent(&event, status);
  *aPreventDefault = (status == nsEventStatus_eConsumeNoDefault);
  return rv;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendKeyEvent(const nsAString& aType,
                               PRInt32 aKeyCode,
                               PRInt32 aCharCode,
                               PRInt32 aModifiers,
                               bool aPreventDefault,
                               bool* aDefaultActionTaken)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  PRInt32 msg;
  if (aType.EqualsLiteral("keydown"))
    msg = NS_KEY_DOWN;
  else if (aType.EqualsLiteral("keyup"))
    msg = NS_KEY_UP;
  else if (aType.EqualsLiteral("keypress"))
    msg = NS_KEY_PRESS;
  else
    return NS_ERROR_FAILURE;

  nsKeyEvent event(true, msg, widget);
  event.modifiers = GetWidgetModifiers(aModifiers);

  event.keyCode = aKeyCode;
  event.charCode = aCharCode;
  event.refPoint.x = event.refPoint.y = 0;
  event.time = PR_IntervalNow();

  if (aPreventDefault) {
    event.flags |= NS_EVENT_FLAG_NO_DEFAULT;
  }

  nsEventStatus status;
  nsresult rv = widget->DispatchEvent(&event, status);
  NS_ENSURE_SUCCESS(rv, rv);

  *aDefaultActionTaken = (status != nsEventStatus_eConsumeNoDefault);
  
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                     PRInt32 aNativeKeyCode,
                                     PRInt32 aModifiers,
                                     const nsAString& aCharacters,
                                     const nsAString& aUnmodifiedCharacters)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  return widget->SynthesizeNativeKeyEvent(aNativeKeyboardLayout, aNativeKeyCode,
                                          aModifiers, aCharacters, aUnmodifiedCharacters);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeMouseEvent(PRInt32 aScreenX,
                                       PRInt32 aScreenY,
                                       PRInt32 aNativeMessage,
                                       PRInt32 aModifierFlags,
                                       nsIDOMElement* aElement)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidgetForElement(aElement);
  if (!widget)
    return NS_ERROR_FAILURE;

  return widget->SynthesizeNativeMouseEvent(nsIntPoint(aScreenX, aScreenY),
                                            aNativeMessage, aModifierFlags);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendNativeMouseScrollEvent(PRInt32 aScreenX,
                                             PRInt32 aScreenY,
                                             PRUint32 aNativeMessage,
                                             double aDeltaX,
                                             double aDeltaY,
                                             double aDeltaZ,
                                             PRUint32 aModifierFlags,
                                             PRUint32 aAdditionalFlags,
                                             nsIDOMElement* aElement)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidgetForElement(aElement);
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  return widget->SynthesizeNativeMouseScrollEvent(nsIntPoint(aScreenX,
                                                             aScreenY),
                                                  aNativeMessage,
                                                  aDeltaX, aDeltaY, aDeltaZ,
                                                  aModifierFlags,
                                                  aAdditionalFlags);
}

NS_IMETHODIMP
nsDOMWindowUtils::ActivateNativeMenuItemAt(const nsAString& indexString)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  return widget->ActivateNativeMenuItemAt(indexString);
}

NS_IMETHODIMP
nsDOMWindowUtils::ForceUpdateNativeMenuAt(const nsAString& indexString)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

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
      nsCOMPtr<nsIPresShell> presShell;
      docShell->GetPresShell(getter_AddRefs(presShell));
      if (presShell) {
        nsIFrame* frame = presShell->GetRootFrame();
        if (frame)
          return frame->GetView()->GetNearestWidget(aOffset);
      }
    }
  }

  return nsnull;
}

nsIWidget*
nsDOMWindowUtils::GetWidgetForElement(nsIDOMElement* aElement)
{
  if (!aElement)
    return GetWidget();

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  nsIDocument* doc = content->GetCurrentDoc();
  nsIPresShell* presShell = doc ? doc->GetShell() : nsnull;

  if (presShell) {
    nsIFrame* frame = content->GetPrimaryFrame();
    if (!frame) {
      frame = presShell->GetRootFrame();
    }
    if (frame)
      return frame->GetNearestWidget();
  }

  return nsnull;
}

NS_IMETHODIMP
nsDOMWindowUtils::Focus(nsIDOMElement* aElement)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

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
                                 PRInt32 aExtraForgetSkippableCalls)
{
  SAMPLE_LABEL("GC", "GarbageCollect");
  // Always permit this in debug builds.
#ifndef DEBUG
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
#endif

  nsJSContext::GarbageCollectNow(js::gcreason::DOM_UTILS);
  nsJSContext::CycleCollectNow(aListener, aExtraForgetSkippableCalls);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::CycleCollect(nsICycleCollectorListener *aListener,
                               PRInt32 aExtraForgetSkippableCalls)
{
  // Always permit this in debug builds.
#ifndef DEBUG
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
#endif

  nsJSContext::CycleCollectNow(aListener, aExtraForgetSkippableCalls);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendSimpleGestureEvent(const nsAString& aType,
                                         float aX,
                                         float aY,
                                         PRUint32 aDirection,
                                         PRFloat64 aDelta,
                                         PRInt32 aModifiers)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsPoint offset;
  nsCOMPtr<nsIWidget> widget = GetWidget(&offset);
  if (!widget)
    return NS_ERROR_FAILURE;

  PRInt32 msg;
  if (aType.EqualsLiteral("MozSwipeGesture"))
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
  else
    return NS_ERROR_FAILURE;
 
  nsSimpleGestureEvent event(true, msg, widget, aDirection, aDelta);
  event.modifiers = GetWidgetModifiers(aModifiers);
  event.time = PR_IntervalNow();

  nsPresContext* presContext = GetPresContext();
  if (!presContext)
    return NS_ERROR_FAILURE;

  PRInt32 appPerDev = presContext->AppUnitsPerDevPixel();
  event.refPoint.x =
    NSAppUnitsToIntPixels(nsPresContext::CSSPixelsToAppUnits(aX) + offset.x,
                          appPerDev);
  event.refPoint.y =
    NSAppUnitsToIntPixels(nsPresContext::CSSPixelsToAppUnits(aY) + offset.y,
                          appPerDev);

  nsEventStatus status;
  return widget->DispatchEvent(&event, status);
}

NS_IMETHODIMP
nsDOMWindowUtils::ElementFromPoint(float aX, float aY,
                                   bool aIgnoreRootScrollFrame,
                                   bool aFlushLayout,
                                   nsIDOMElement** aReturn)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(window->GetExtantDocument()));
  NS_ENSURE_STATE(doc);

  return doc->ElementFromPointHelper(aX, aY, aIgnoreRootScrollFrame, aFlushLayout,
                                     aReturn);
}

NS_IMETHODIMP
nsDOMWindowUtils::NodesFromRect(float aX, float aY,
                                float aTopSize, float aRightSize,
                                float aBottomSize, float aLeftSize,
                                bool aIgnoreRootScrollFrame,
                                bool aFlushLayout,
                                nsIDOMNodeList** aReturn)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(window->GetExtantDocument()));
  NS_ENSURE_STATE(doc);

  return doc->NodesFromRectHelper(aX, aY, aTopSize, aRightSize, aBottomSize, aLeftSize, 
                                  aIgnoreRootScrollFrame, aFlushLayout, aReturn);
}

static already_AddRefed<gfxImageSurface>
CanvasToImageSurface(nsIDOMHTMLCanvasElement* aCanvas)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aCanvas);
  if (!node) {
    return nsnull;
  }

  NS_ABORT_IF_FALSE(node->IsElement(),
                    "An nsINode that implements nsIDOMHTMLCanvasElement should "
                    "be an element.");
  nsLayoutUtils::SurfaceFromElementResult result =
    nsLayoutUtils::SurfaceFromElement(node->AsElement(),
                                      nsLayoutUtils::SFE_WANT_IMAGE_SURFACE);
  return static_cast<gfxImageSurface*>(result.mSurface.forget().get());
}

NS_IMETHODIMP
nsDOMWindowUtils::CompareCanvases(nsIDOMHTMLCanvasElement *aCanvas1,
                                  nsIDOMHTMLCanvasElement *aCanvas2,
                                  PRUint32* aMaxDifference,
                                  PRUint32* retVal)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (aCanvas1 == nsnull ||
      aCanvas2 == nsnull ||
      retVal == nsnull)
    return NS_ERROR_FAILURE;

  nsRefPtr<gfxImageSurface> img1 = CanvasToImageSurface(aCanvas1);
  nsRefPtr<gfxImageSurface> img2 = CanvasToImageSurface(aCanvas2);

  if (img1 == nsnull || img2 == nsnull ||
      img1->GetSize() != img2->GetSize() ||
      img1->Stride() != img2->Stride())
    return NS_ERROR_FAILURE;

  int v;
  gfxIntSize size = img1->GetSize();
  PRUint32 stride = img1->Stride();

  // we can optimize for the common all-pass case
  if (stride == (PRUint32) size.width * 4) {
    v = memcmp(img1->Data(), img2->Data(), size.width * size.height * 4);
    if (v == 0) {
      if (aMaxDifference)
        *aMaxDifference = 0;
      *retVal = 0;
      return NS_OK;
    }
  }

  PRUint32 dc = 0;
  PRUint32 different = 0;

  for (int j = 0; j < size.height; j++) {
    unsigned char *p1 = img1->Data() + j*stride;
    unsigned char *p2 = img2->Data() + j*stride;
    v = memcmp(p1, p2, stride);

    if (v) {
      for (int i = 0; i < size.width; i++) {
        if (*(PRUint32*) p1 != *(PRUint32*) p2) {

          different++;

          dc = NS_MAX((PRUint32)abs(p1[0] - p2[0]), dc);
          dc = NS_MAX((PRUint32)abs(p1[1] - p2[1]), dc);
          dc = NS_MAX((PRUint32)abs(p1[2] - p2[2]), dc);
          dc = NS_MAX((PRUint32)abs(p1[3] - p2[3]), dc);
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
  nsPresContext* presContext = GetPresContext();
  if (!presContext)
    return NS_OK;
  presContext->ClearMozAfterPaintEvents();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::DisableNonTestMouseEvents(bool aDisable)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsIDocShell *docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  presShell->DisableNonTestMouseEvents(aDisable);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SuppressEventHandling(bool aSuppress)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(window->GetExtantDocument()));
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  if (aSuppress) {
    doc->SuppressEventHandling();
  } else {
    doc->UnsuppressEventHandlingAndFireEvents(true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScrollXY(bool aFlushLayout, PRInt32* aScrollX, PRInt32* aScrollY)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(window->GetExtantDocument()));
  NS_ENSURE_STATE(doc);

  if (aFlushLayout) {
    doc->FlushPendingNotifications(Flush_Layout);
  }

  nsPoint scrollPos(0,0);
  nsIPresShell *presShell = doc->GetShell();
  if (presShell) {
    nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
    if (sf) {
      scrollPos = sf->GetScrollPosition();
    }
  }

  *aScrollX = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.x);
  *aScrollY = nsPresContext::AppUnitsToIntCSSPixels(scrollPos.y);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetIMEIsOpen(bool *aState)
{
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
nsDOMWindowUtils::GetIMEStatus(PRUint32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  InputContext context = widget->GetInputContext();
  *aState = static_cast<PRUint32>(context.mIMEState.mEnabled);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFocusedInputType(char** aType)
{
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
  if (aID == FrameMetrics::ROOT_SCROLL_ID) {
    nsPresContext* presContext = GetPresContext();
    if (!presContext) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsIDocument* document = presContext->Document();
    mozilla::dom::Element* rootElement = document->GetRootElement();
    if (!rootElement) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    CallQueryInterface(rootElement, aResult);
    return NS_OK;
  }

  nsRefPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aID);
  return content ? CallQueryInterface(content, aResult) : NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetScreenPixelsPerCSSPixel(float* aScreenPixels)
{
  *aScreenPixels = 1;

  if (!nsContentUtils::IsCallerTrustedForRead())
    return NS_ERROR_DOM_SECURITY_ERR;
  nsPresContext* presContext = GetPresContext();
  if (!presContext)
    return NS_OK;

  *aScreenPixels = float(nsPresContext::AppUnitsPerCSSPixel())/
      presContext->AppUnitsPerDevPixel();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::DispatchDOMEventViaPresShell(nsIDOMNode* aTarget,
                                               nsIDOMEvent* aEvent,
                                               bool aTrusted,
                                               bool* aRetVal)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsPresContext* presContext = GetPresContext();
  NS_ENSURE_STATE(presContext);
  nsCOMPtr<nsIPresShell> shell = presContext->GetPresShell();
  NS_ENSURE_STATE(shell);
  nsCOMPtr<nsIPrivateDOMEvent> event = do_QueryInterface(aEvent);
  NS_ENSURE_STATE(event);
  event->SetTrusted(aTrusted);
  nsEvent* internalEvent = event->GetInternalNSEvent();
  NS_ENSURE_STATE(internalEvent);
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTarget);
  NS_ENSURE_STATE(content);

  nsEventStatus status = nsEventStatus_eIgnore;
  shell->HandleEventWithTarget(internalEvent, nsnull, content,
                               &status);
  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return NS_OK;
}

static void
InitEvent(nsGUIEvent &aEvent, nsIntPoint *aPt = nsnull)
{
  if (aPt) {
    aEvent.refPoint = *aPt;
  }
  aEvent.time = PR_IntervalNow();
}

NS_IMETHODIMP
nsDOMWindowUtils::SendCompositionEvent(const nsAString& aType,
                                       const nsAString& aData,
                                       const nsAString& aLocale)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 msg;
  if (aType.EqualsLiteral("compositionstart")) {
    msg = NS_COMPOSITION_START;
  } else if (aType.EqualsLiteral("compositionend")) {
    msg = NS_COMPOSITION_END;
  } else if (aType.EqualsLiteral("compositionupdate")) {
    msg = NS_COMPOSITION_UPDATE;
  } else {
    return NS_ERROR_FAILURE;
  }

  nsCompositionEvent compositionEvent(true, msg, widget);
  InitEvent(compositionEvent);
  if (msg != NS_COMPOSITION_START) {
    compositionEvent.data = aData;
  }

  nsEventStatus status;
  nsresult rv = widget->DispatchEvent(&compositionEvent, status);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static void
AppendClause(PRInt32 aClauseLength, PRUint32 aClauseAttr,
             nsTArray<nsTextRange>* aRanges)
{
  NS_PRECONDITION(aRanges, "aRange is null");
  if (aClauseLength == 0) {
    return;
  }
  nsTextRange range;
  range.mStartOffset = aRanges->Length() == 0 ? 0 :
    aRanges->ElementAt(aRanges->Length() - 1).mEndOffset + 1;
  range.mEndOffset = range.mStartOffset + aClauseLength;
  NS_ASSERTION(range.mStartOffset <= range.mEndOffset, "range is invalid");
  NS_PRECONDITION(aClauseAttr == NS_TEXTRANGE_RAWINPUT ||
                  aClauseAttr == NS_TEXTRANGE_SELECTEDRAWTEXT ||
                  aClauseAttr == NS_TEXTRANGE_CONVERTEDTEXT ||
                  aClauseAttr == NS_TEXTRANGE_SELECTEDCONVERTEDTEXT,
                  "aClauseAttr is invalid value");
  range.mRangeType = aClauseAttr;
  aRanges->AppendElement(range);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendTextEvent(const nsAString& aCompositionString,
                                PRInt32 aFirstClauseLength,
                                PRUint32 aFirstClauseAttr,
                                PRInt32 aSecondClauseLength,
                                PRUint32 aSecondClauseAttr,
                                PRInt32 aThirdClauseLength,
                                PRUint32 aThirdClauseAttr,
                                PRInt32 aCaretStart,
                                PRInt32 aCaretLength)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  nsTextEvent textEvent(true, NS_TEXT_TEXT, widget);
  InitEvent(textEvent);

  nsAutoTArray<nsTextRange, 4> textRanges;
  NS_ENSURE_TRUE(aFirstClauseLength >= 0,  NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aSecondClauseLength >= 0, NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aThirdClauseLength >= 0,  NS_ERROR_INVALID_ARG);
  AppendClause(aFirstClauseLength,  aFirstClauseAttr, &textRanges);
  AppendClause(aSecondClauseLength, aSecondClauseAttr, &textRanges);
  AppendClause(aThirdClauseLength,  aThirdClauseAttr, &textRanges);
  PRInt32 len = aFirstClauseLength + aSecondClauseLength + aThirdClauseLength;
  NS_ENSURE_TRUE(len == 0 || PRUint32(len) == aCompositionString.Length(),
                 NS_ERROR_FAILURE);

  if (aCaretStart >= 0) {
    nsTextRange range;
    range.mStartOffset = aCaretStart;
    range.mEndOffset = range.mStartOffset + aCaretLength;
    range.mRangeType = NS_TEXTRANGE_CARETPOSITION;
    textRanges.AppendElement(range);
  }

  textEvent.theText = aCompositionString;

  textEvent.rangeCount = textRanges.Length();
  textEvent.rangeArray = textRanges.Elements();

  nsEventStatus status;
  nsresult rv = widget->DispatchEvent(&textEvent, status);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendQueryContentEvent(PRUint32 aType,
                                        PRUint32 aOffset, PRUint32 aLength,
                                        PRInt32 aX, PRInt32 aY,
                                        nsIQueryContentEventResult **aResult)
{
  *aResult = nsnull;

  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsIDocShell *docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
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
  nsIntPoint pt(aX, aY);

  if (aType == QUERY_CHARACTER_AT_POINT) {
    // Looking for the widget at the point.
    nsQueryContentEvent dummyEvent(true, NS_QUERY_CONTENT_STATE, widget);
    InitEvent(dummyEvent, &pt);
    nsIFrame* popupFrame =
      nsLayoutUtils::GetPopupFrameForEventCoordinates(presContext->GetRootPresContext(), &dummyEvent);

    nsIntRect widgetBounds;
    nsresult rv = widget->GetClientBounds(widgetBounds);
    NS_ENSURE_SUCCESS(rv, rv);
    widgetBounds.MoveTo(0, 0);

    // There is no popup frame at the point and the point isn't in our widget,
    // we cannot process this request.
    NS_ENSURE_TRUE(popupFrame || widgetBounds.Contains(pt),
                   NS_ERROR_FAILURE);

    // Fire the event on the widget at the point
    if (popupFrame) {
      targetWidget = popupFrame->GetNearestWidget();
    }
  }

  pt += widget->WidgetToScreenOffset() - targetWidget->WidgetToScreenOffset();

  nsQueryContentEvent queryEvent(true, aType, targetWidget);
  InitEvent(queryEvent, &pt);

  switch (aType) {
    case NS_QUERY_TEXT_CONTENT:
      queryEvent.InitForQueryTextContent(aOffset, aLength);
      break;
    case NS_QUERY_CARET_RECT:
      queryEvent.InitForQueryCaretRect(aOffset);
      break;
    case NS_QUERY_TEXT_RECT:
      queryEvent.InitForQueryTextRect(aOffset, aLength);
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
nsDOMWindowUtils::SendSelectionSetEvent(PRUint32 aOffset,
                                        PRUint32 aLength,
                                        bool aReverse,
                                        bool *aResult)
{
  *aResult = false;

  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  nsSelectionEvent selectionEvent(true, NS_SELECTION_SET, widget);
  InitEvent(selectionEvent);

  selectionEvent.mOffset = aOffset;
  selectionEvent.mLength = aLength;
  selectionEvent.mReversed = aReverse;

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
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // get the widget to send the event to
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  PRInt32 msg;
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
 
  nsContentCommandEvent event(true, msg, widget);
  if (msg == NS_CONTENT_COMMAND_PASTE_TRANSFERABLE) {
    event.mTransferable = aTransferable;
  }

  nsEventStatus status;
  return widget->DispatchEvent(&event, status);
}

NS_IMETHODIMP
nsDOMWindowUtils::GetClassName(const JS::Value& aObject, JSContext* aCx, char** aName)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Our argument must be a non-null object.
  if (JSVAL_IS_PRIMITIVE(aObject)) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  *aName = NS_strdup(JS_GetClass(JSVAL_TO_OBJECT(aObject))->name);
  NS_ABORT_IF_FALSE(*aName, "NS_strdup should be infallible.");
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetVisitedDependentComputedStyle(
                    nsIDOMElement *aElement, const nsAString& aPseudoElement,
                    const nsAString& aPropertyName, nsAString& aResult)
{
  aResult.Truncate();

  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

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
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  window->EnterModalState();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::LeaveModalState()
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  window->LeaveModalState(nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::EnterModalStateWithWindow(nsIDOMWindow **aWindow)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  *aWindow = window->EnterModalState();
  NS_IF_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::LeaveModalStateWithWindow(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  NS_ENSURE_ARG_POINTER(aWindow);
  window->LeaveModalState(aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::IsInModalState(bool *retval)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  *retval = static_cast<nsGlobalWindow*>(window.get())->IsInModalState();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetParent(const JS::Value& aObject,
                            JSContext* aCx,
                            JS::Value* aParent)
{
  // This wasn't privileged in the past, but better to expose less than more.
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // First argument must be an object.
  if (JSVAL_IS_PRIMITIVE(aObject)) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  JSObject* parent = JS_GetParent(JSVAL_TO_OBJECT(aObject));
  *aParent = OBJECT_TO_JSVAL(parent);

  // Outerize if necessary.
  if (parent) {
    if (JSObjectOp outerize = js::GetObjectClass(parent)->ext.outerObject) {
      *aParent = OBJECT_TO_JSVAL(outerize(aCx, parent));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetOuterWindowID(PRUint64 *aWindowID)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  NS_ASSERTION(window->IsOuterWindow(), "How did that happen?");
  *aWindowID = window->WindowID();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetCurrentInnerWindowID(PRUint64 *aWindowID)
{
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
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  window->SuspendTimeouts();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ResumeTimeouts()
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  window->ResumeTimeouts();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetLayerManagerType(nsAString& aType)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  LayerManager *mgr = widget->GetLayerManager();
  if (!mgr)
    return NS_ERROR_FAILURE;

  mgr->GetBackendName(aType);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StartFrameTimeRecording()
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  LayerManager *mgr = widget->GetLayerManager();
  if (!mgr)
    return NS_ERROR_FAILURE;

  mgr->StartFrameTimeRecording();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StopFrameTimeRecording(PRUint32 *frameCount NS_OUTPARAM, float **frames NS_OUTPARAM)
{
  NS_ENSURE_ARG_POINTER(frameCount);
  NS_ENSURE_ARG_POINTER(frames);

  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  LayerManager *mgr = widget->GetLayerManager();
  if (!mgr)
    return NS_ERROR_FAILURE;

  nsTArray<float> frameTimes = mgr->StopFrameTimeRecording();

  *frames = nsnull;
  *frameCount = frameTimes.Length();

  if (*frameCount != 0) {
    *frames = (float*)nsMemory::Alloc(*frameCount * sizeof(float*));
    if (!*frames)
      return NS_ERROR_OUT_OF_MEMORY;

    /* copy over the frame times into the array we just allocated */
    for (PRUint32 i = 0; i < *frameCount; i++) {
      (*frames)[i] = frameTimes[i];
    }
  }

  return NS_OK;
}

static bool
ComputeAnimationValue(nsCSSProperty aProperty,
                      Element* aElement,
                      const nsAString& aInput,
                      nsStyleAnimation::Value& aOutput)
{

  if (!nsStyleAnimation::ComputeValue(aProperty, aElement, aInput,
                                      false, aOutput)) {
    return false;
  }

  // This matches TransExtractComputedValue in nsTransitionManager.cpp.
  if (aProperty == eCSSProperty_visibility) {
    NS_ABORT_IF_FALSE(aOutput.GetUnit() == nsStyleAnimation::eUnit_Enumerated,
                      "unexpected unit");
    aOutput.SetIntValue(aOutput.GetIntValue(),
                        nsStyleAnimation::eUnit_Visibility);
  }

  return true;
}

NS_IMETHODIMP
nsDOMWindowUtils::AdvanceTimeAndRefresh(PRInt64 aMilliseconds)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  GetPresContext()->RefreshDriver()->AdvanceTimeAndRefresh(aMilliseconds);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::RestoreNormalRefresh()
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  GetPresContext()->RefreshDriver()->RestoreNormalRefresh();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::ComputeAnimationDistance(nsIDOMElement* aElement,
                                           const nsAString& aProperty,
                                           const nsAString& aValue1,
                                           const nsAString& aValue2,
                                           double* aResult)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsresult rv;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert direction-dependent properties as appropriate, e.g.,
  // border-left to border-left-value.
  nsCSSProperty property = nsCSSProps::LookupProperty(aProperty);
  if (property != eCSSProperty_UNKNOWN && nsCSSProps::IsShorthand(property)) {
    nsCSSProperty subprop0 = *nsCSSProps::SubpropertyEntryFor(property);
    if (nsCSSProps::PropHasFlags(subprop0, CSS_PROPERTY_REPORT_OTHER_NAME) &&
        nsCSSProps::OtherNameFor(subprop0) == property) {
      property = subprop0;
    } else {
      property = eCSSProperty_UNKNOWN;
    }
  }

  NS_ABORT_IF_FALSE(property == eCSSProperty_UNKNOWN ||
                    !nsCSSProps::IsShorthand(property),
                    "should not have shorthand");

  nsStyleAnimation::Value v1, v2;
  if (property == eCSSProperty_UNKNOWN ||
      !ComputeAnimationValue(property, content->AsElement(), aValue1, v1) ||
      !ComputeAnimationValue(property, content->AsElement(), aValue2, v2)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (!nsStyleAnimation::ComputeDistance(property, v1, v2, *aResult)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsDOMWindowUtils::RenderDocument(const nsRect& aRect,
                                 PRUint32 aFlags,
                                 nscolor aBackgroundColor,
                                 gfxContext* aThebesContext)
{
    nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
    NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

    // Get DOM Document
    nsresult rv;
    nsCOMPtr<nsIDOMDocument> ddoc;
    rv = window->GetDocument(getter_AddRefs(ddoc));
    NS_ENSURE_SUCCESS(rv, rv);

    // Get Document
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(ddoc, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get Primary Shell
    nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    // Render Document
    return presShell->RenderDocument(aRect, aFlags, aBackgroundColor, aThebesContext);
}

NS_IMETHODIMP 
nsDOMWindowUtils::GetCursorType(PRInt16 *aCursor)
{
  NS_ENSURE_ARG_POINTER(aCursor);

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);  

  bool isSameDoc = false;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(window->GetExtantDocument()));

  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  do {
    if (nsEventStateManager::sMouseOverDocument == doc.get()) {
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
nsDOMWindowUtils::GoOnline()
{
  // This is only allowed from about:neterror, which is unprivileged, so it
  // can't access the io-service itself.
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(window->GetExtantDocument()));
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);
  nsCOMPtr<nsIURI> documentURI;
  documentURI = doc->GetDocumentURI();

  nsCAutoString spec;
  documentURI->GetSpec(spec);
  if (!StringBeginsWith(spec,  NS_LITERAL_CSTRING("about:neterror?")))
    return NS_ERROR_DOM_SECURITY_ERR;

  nsCOMPtr<nsIIOService> ios = do_GetService("@mozilla.org/network/io-service;1");
  if (ios) {
    ios->SetOffline(false); // !offline
    return NS_OK;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetDisplayDPI(float *aDPI)
{
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  *aDPI = widget->GetDPI();

  return NS_OK;
}


NS_IMETHODIMP
nsDOMWindowUtils::GetOuterWindowWithId(PRUint64 aWindowID,
                                       nsIDOMWindow** aWindow)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  *aWindow = nsGlobalWindow::GetOuterWindowWithId(aWindowID);
  NS_IF_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::WrapDOMFile(nsIFile *aFile,
                              nsIDOMFile **aDOMFile) {
  NS_ADDREF(*aDOMFile = new nsDOMFileFile(aFile));
  return NS_OK;
}

#ifdef DEBUG
static bool
CheckLeafLayers(Layer* aLayer, const nsIntPoint& aOffset, nsIntRegion* aCoveredRegion)
{
  gfxMatrix transform;
  if (!aLayer->GetTransform().Is2D(&transform) ||
      transform.HasNonIntegerTranslation())
    return false;
  transform.NudgeToIntegers();
  nsIntPoint offset = aOffset + nsIntPoint(transform.x0, transform.y0);

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
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

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
nsDOMWindowUtils::GetMayHaveTouchEventListeners(bool* aResult)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsPIDOMWindow* innerWindow = window->GetCurrentInnerWindow();
  *aResult = innerWindow ? innerWindow->HasTouchEventListeners() : false;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::CheckAndClearPaintedState(nsIDOMElement* aElement, bool* aResult)
{
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

  *aResult = frame->CheckAndClearPaintedState();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFileId(nsIDOMBlob* aBlob, PRInt64* aResult)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  *aResult = aBlob->GetFileId();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetFileReferences(const nsAString& aDatabaseName,
                                    PRInt64 aId, PRInt32* aRefCnt,
                                    PRInt32* aDBRefCnt, PRInt32* aSliceRefCnt,
                                    bool* aResult)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCString origin;
  nsresult rv = indexedDB::IndexedDatabaseManager::GetASCIIOriginFromWindow(
    window, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<indexedDB::IndexedDatabaseManager> mgr =
    indexedDB::IndexedDatabaseManager::Get();

  if (mgr) {
    nsRefPtr<indexedDB::FileManager> fileManager =
      mgr->GetFileManager(origin, aDatabaseName);

    if (fileManager) {
      nsRefPtr<indexedDB::FileInfo> fileInfo = fileManager->GetFileInfo(aId);

      if (fileInfo) {
        fileInfo->GetReferences(aRefCnt, aDBRefCnt, aSliceRefCnt);

        if (*aRefCnt != -1) {
          // We added an extra temp ref, so account for that accordingly.
          (*aRefCnt)--;
        }

        *aResult = true;
        return NS_OK;
      }
    }
  }

  *aRefCnt = *aDBRefCnt = *aSliceRefCnt = -1;
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::IsIncrementalGCEnabled(JSContext* cx, bool* aResult)
{
  *aResult = js::IsIncrementalGCEnabled(JS_GetRuntime(cx));
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StartPCCountProfiling(JSContext* cx)
{
  js::StartPCCountProfiling(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::StopPCCountProfiling(JSContext* cx)
{
  js::StopPCCountProfiling(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::PurgePCCounts(JSContext* cx)
{
  js::PurgePCCounts(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPCCountScriptCount(JSContext* cx, PRInt32 *result)
{
  *result = js::GetPCCountScriptCount(cx);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPCCountScriptSummary(PRInt32 script, JSContext* cx, nsAString& result)
{
  JSString *text = js::GetPCCountScriptSummary(cx, script);
  if (!text)
    return NS_ERROR_FAILURE;

  nsDependentJSString str;
  if (!str.init(cx, text))
    return NS_ERROR_FAILURE;

  result = str;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPCCountScriptContents(PRInt32 script, JSContext* cx, nsAString& result)
{
  JSString *text = js::GetPCCountScriptContents(cx, script);
  if (!text)
    return NS_ERROR_FAILURE;

  nsDependentJSString str;
  if (!str.init(cx, text))
    return NS_ERROR_FAILURE;

  result = str;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPaintingSuppressed(bool *aPaintingSuppressed)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsIDocShell *docShell = window->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  *aPaintingSuppressed = presShell->IsPaintingSuppressed();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetPlugins(JSContext* cx, jsval* aPlugins)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
  NS_ENSURE_STATE(window);

  nsIDOMDocument* ddoc = window->GetExtantDocument();

  nsresult rv;
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(ddoc, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<nsIObjectLoadingContent*> plugins;
  doc->GetPlugins(plugins);

  JSObject* jsPlugins = nsnull;
  rv = nsTArrayToJSArray(cx, plugins, &jsPlugins);
  NS_ENSURE_SUCCESS(rv, rv);

  *aPlugins = OBJECT_TO_JSVAL(jsPlugins);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetScrollPositionClampingScrollPortSize(float aWidth, float aHeight)
{
  if (!IsUniversalXPConnectCapable()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (!(aWidth >= 0.0 && aHeight >= 0.0)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsIPresShell* presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  presShell->SetScrollPositionClampingScrollPortSize(
    nsPresContext::CSSPixelsToAppUnits(aWidth),
    nsPresContext::CSSPixelsToAppUnits(aHeight));

  return NS_OK;
}
