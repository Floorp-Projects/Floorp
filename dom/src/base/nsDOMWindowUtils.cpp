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
#include "nsDOMClassInfo.h"
#include "nsDOMError.h"
#include "nsIDOMNSEvent.h"

#include "nsDOMWindowUtils.h"
#include "nsGlobalWindow.h"
#include "nsIDocument.h"
#include "nsIFocusController.h"

#include "nsContentUtils.h"

#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsIView.h"
#include "nsGUIEvent.h"

#ifdef MOZ_ENABLE_GTK2
#include <gdk/gdkx.h>
#endif

NS_INTERFACE_MAP_BEGIN(nsDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY(nsIDOMWindowUtils)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WindowUtils)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMWindowUtils)
NS_IMPL_RELEASE(nsDOMWindowUtils)

nsDOMWindowUtils::nsDOMWindowUtils(nsGlobalWindow *aWindow)
  : mWindow(aWindow)
{
}

nsDOMWindowUtils::~nsDOMWindowUtils()
{
}

NS_IMETHODIMP
nsDOMWindowUtils::GetImageAnimationMode(PRUint16 *aMode)
{
  NS_ENSURE_ARG_POINTER(aMode);
  *aMode = 0;
  if (mWindow) {
    nsIDocShell *docShell = mWindow->GetDocShell();
    if (docShell) {
      nsCOMPtr<nsPresContext> presContext;
      docShell->GetPresContext(getter_AddRefs(presContext));
      if (presContext) {
        *aMode = presContext->ImageAnimationMode();
        return NS_OK;
      }
    }
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMWindowUtils::SetImageAnimationMode(PRUint16 aMode)
{
  if (mWindow) {
    nsIDocShell *docShell = mWindow->GetDocShell();
    if (docShell) {
      nsCOMPtr<nsPresContext> presContext;
      docShell->GetPresContext(getter_AddRefs(presContext));
      if (presContext) {
        presContext->SetImageAnimationMode(aMode);
        return NS_OK;
      }
    }
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMWindowUtils::GetDocumentMetadata(const nsAString& aName,
                                      nsAString& aValue)
{
  PRBool hasCap = PR_FALSE;
  if (NS_FAILED(nsContentUtils::GetSecurityManager()->IsCapabilityEnabled("UniversalXPConnect", &hasCap))
      || !hasCap)
    return NS_ERROR_DOM_SECURITY_ERR;

  if (mWindow) {
    nsCOMPtr<nsIDocument> doc(do_QueryInterface(mWindow->GetExtantDocument()));
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
nsDOMWindowUtils::Redraw()
{
  nsresult rv;

  nsCOMPtr<nsIDocShell> docShell = mWindow->GetDocShell();
  if (docShell) {
    nsCOMPtr<nsIPresShell> presShell;

    rv = docShell->GetPresShell(getter_AddRefs(presShell));
    if (NS_SUCCEEDED(rv) && presShell) {
      nsIFrame *rootFrame = presShell->GetRootFrame();

      if (rootFrame) {
        nsRect r(nsPoint(0, 0), rootFrame->GetSize());
        rootFrame->Invalidate(r, PR_TRUE);

#ifdef MOZ_ENABLE_GTK2
        XSync(GDK_DISPLAY(), False);
#endif
        return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMWindowUtils::SendMouseEvent(const nsAString& aType,
                                 PRInt32 aX,
                                 PRInt32 aY,
                                 PRInt32 aButton,
                                 PRInt32 aClickCount,
                                 PRInt32 aModifiers)
{
  PRBool hasCap = PR_FALSE;
  if (NS_FAILED(nsContentUtils::GetSecurityManager()->IsCapabilityEnabled("UniversalXPConnect", &hasCap))
      || !hasCap)
    return NS_ERROR_DOM_SECURITY_ERR;

  // get the widget to send the event to
  nsIWidget* widget = GetWidget();
  if (!widget)
    return NS_ERROR_FAILURE;

  PRInt32 msg;
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
  else if (aType.EqualsLiteral("contextmenu"))
    msg = NS_CONTEXTMENU;
  else
    return NS_ERROR_FAILURE;

  nsMouseEvent event(PR_TRUE, msg, widget, nsMouseEvent::eReal);
  event.isShift = (aModifiers & nsIDOMNSEvent::SHIFT_MASK) ? PR_TRUE : PR_FALSE;
  event.isControl = (aModifiers & nsIDOMNSEvent::CONTROL_MASK) ? PR_TRUE : PR_FALSE;
  event.isAlt = (aModifiers & nsIDOMNSEvent::ALT_MASK) ? PR_TRUE : PR_FALSE;
  event.isMeta = (aModifiers & nsIDOMNSEvent::META_MASK) ? PR_TRUE : PR_FALSE;
  event.button = aButton;
  event.widget = widget;

  event.clickCount = aClickCount;
  event.time = PR_IntervalNow();
  event.refPoint.x = aX;
  event.refPoint.y = aY;

  nsEventStatus status;
  return widget->DispatchEvent(&event, status);
}

NS_IMETHODIMP
nsDOMWindowUtils::SendKeyEvent(const nsAString& aType,
                               PRInt32 aKeyCode,
                               PRInt32 aCharCode,
                               PRInt32 aModifiers)
{
  PRBool hasCap = PR_FALSE;
  if (NS_FAILED(nsContentUtils::GetSecurityManager()->IsCapabilityEnabled("UniversalXPConnect", &hasCap))
      || !hasCap)
    return NS_ERROR_DOM_SECURITY_ERR;

  // get the widget to send the event to
  nsIWidget* widget = GetWidget();
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

  nsKeyEvent event(PR_TRUE, msg, widget);
  event.isShift = (aModifiers & nsIDOMNSEvent::SHIFT_MASK) ? PR_TRUE : PR_FALSE;
  event.isControl = (aModifiers & nsIDOMNSEvent::CONTROL_MASK) ? PR_TRUE : PR_FALSE;
  event.isAlt = (aModifiers & nsIDOMNSEvent::ALT_MASK) ? PR_TRUE : PR_FALSE;
  event.isMeta = (aModifiers & nsIDOMNSEvent::META_MASK) ? PR_TRUE : PR_FALSE;

  event.keyCode = aKeyCode;
  event.charCode = aCharCode;
  event.refPoint.x = event.refPoint.y = 0;
  event.time = PR_IntervalNow();

  nsEventStatus status;
  return widget->DispatchEvent(&event, status);
}

nsIWidget*
nsDOMWindowUtils::GetWidget()
{
  if (mWindow) {
    nsIDocShell *docShell = mWindow->GetDocShell();
    if (docShell) {
      nsCOMPtr<nsIPresShell> presShell;
      docShell->GetPresShell(getter_AddRefs(presShell));
      if (presShell) {
        nsIFrame* frame = presShell->GetRootFrame();
        if (frame)
          return frame->GetWindow();
      }
    }
  }

  return nsnull;
}
