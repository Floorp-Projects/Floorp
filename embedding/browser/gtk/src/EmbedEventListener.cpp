/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 tw=80 et cindent: */
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
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Oleg Romashin <romaxa@gmail.com>
 *   Tomaz Noleto <tnoleto@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsIDOMMouseEvent.h"

#include "nsIDOMNSEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIDOMNodeList.h"

#include "EmbedEventListener.h"
#include "EmbedPrivate.h"
#include "gtkmozembed_internal.h"

static PRInt32 sLongPressTimer = 0, mLongMPressDelay = 1000;
static PRInt32 sX = 0, sY = 0;
static PRBool  sMPressed = PR_FALSE, sIsScrolling = PR_FALSE;
static char* gFavLocation = NULL;

EmbedEventListener::EmbedEventListener(void)
{
  mOwner = nsnull;
}

EmbedEventListener::~EmbedEventListener()
{
  delete mCtxInfo;
}

NS_IMPL_ADDREF(EmbedEventListener)
NS_IMPL_RELEASE(EmbedEventListener)
NS_INTERFACE_MAP_BEGIN(EmbedEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMUIListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseMotionListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END

nsresult
EmbedEventListener::Init(EmbedPrivate *aOwner)
{
  mOwner = aOwner;
  mCtxInfo = nsnull;
  mClickCount = 1;
#ifdef MOZ_WIDGET_GTK2
  mCtxInfo = new EmbedContextMenuInfo(aOwner);
#endif
  mOwner->mNeedFav = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::HandleLink(nsIDOMNode* node)
{
  nsresult rv;

  nsCOMPtr<nsIDOMElement> linkElement;
  linkElement = do_QueryInterface(node);
  if (!linkElement) return NS_ERROR_FAILURE;

  nsString name;
  rv = linkElement->GetAttribute(NS_LITERAL_STRING("rel"), name);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  nsString link;
  rv = linkElement->GetAttribute(NS_LITERAL_STRING("href"), link);
  if (NS_FAILED(rv) || link.IsEmpty()) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = node->GetOwnerDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(rv) || !domDoc) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOM3Node> domnode = do_QueryInterface(domDoc);
  if (!domnode) return NS_ERROR_FAILURE;

  nsString spec;
  domnode->GetBaseURI(spec);

  nsCOMPtr<nsIURI> baseURI;
  rv = NewURI(getter_AddRefs(baseURI), NS_ConvertUTF16toUTF8(spec).get());
  if (NS_FAILED(rv) || !baseURI) return NS_ERROR_FAILURE;

  nsCString url;
  rv = baseURI->Resolve(NS_ConvertUTF16toUTF8(link), url);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  nsString type;
  rv = linkElement->GetAttribute(NS_LITERAL_STRING("type"), type);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  nsString title;
  rv = linkElement->GetAttribute(NS_LITERAL_STRING("title"), title);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  // XXX This does not handle |BLAH ICON POWER"
  if (mOwner->mNeedFav && (name.LowerCaseEqualsLiteral("icon") ||
      name.LowerCaseEqualsLiteral("shortcut icon"))) {
    mOwner->mNeedFav = PR_FALSE;
    this->GetFaviconFromURI(url.get());
  }
  else if (name.LowerCaseEqualsLiteral("alternate") &&
           type.LowerCaseEqualsLiteral("application/rss+xml")) {

    NS_ConvertUTF16toUTF8 narrowTitle(title);

    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                    moz_embed_signals[RSS_REQUEST],
                    (gchar *)url.get(),
                    narrowTitle.get());
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::HandleEvent(nsIDOMEvent* aDOMEvent)
{
  nsString eventType;
  aDOMEvent->GetType(eventType);
#ifdef MOZ_WIDGET_GTK2
  if (eventType.EqualsLiteral("focus"))
    if (mCtxInfo->GetFormControlType(aDOMEvent)) {
      if (mCtxInfo->mEmbedCtxType & GTK_MOZ_EMBED_CTX_INPUT) {
        gint return_val = FALSE;
        gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                        moz_embed_signals[DOM_FOCUS],
                        (void *)aDOMEvent, &return_val);
        if (return_val) {
          aDOMEvent->StopPropagation();
          aDOMEvent->PreventDefault();
        }
      }
    }
#endif

  if (eventType.EqualsLiteral("DOMLinkAdded")) {

    nsresult rv;
    nsCOMPtr<nsIDOMEventTarget> eventTarget;

    aDOMEvent->GetTarget(getter_AddRefs(eventTarget));
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(eventTarget, &rv);
    if (NS_FAILED(rv) || !node)
      return NS_ERROR_FAILURE;
    HandleLink(node);
  } else if (eventType.EqualsLiteral("load")) {

    nsIWebBrowser *webBrowser = nsnull;
    gtk_moz_embed_get_nsIWebBrowser(mOwner->mOwningWidget, &webBrowser);
    if (!webBrowser) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMWindow> DOMWindow;
    webBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));
    if (!DOMWindow) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMDocument> doc;
    DOMWindow->GetDocument(getter_AddRefs(doc));
    if (!doc) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMNodeList> nodelist = nsnull;
    doc->GetElementsByTagName( NS_LITERAL_STRING( "rss" ), getter_AddRefs( nodelist ));
    if (nodelist) {
      PRUint32 length = 0;
      nodelist->GetLength(&length);
      if (length >= 1) {
        char *url = gtk_moz_embed_get_location(mOwner->mOwningWidget);
        char *title = gtk_moz_embed_get_title(mOwner->mOwningWidget);
        gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                        moz_embed_signals[RSS_REQUEST],
                        (gchar*)url,
                        (gchar*)title);
        if (url)
          NS_Free(url);
        if (title)
          NS_Free(title);
      }
    }
  }
  else if (mOwner->mNeedFav) {
    mOwner->mNeedFav = PR_FALSE;
    nsCString favicon_url = mOwner->mPrePath;
	favicon_url.AppendLiteral("/favicon.ico");
    this->GetFaviconFromURI(favicon_url.get());
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::KeyDown(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aDOMEvent);
  if (!keyEvent)
    return NS_OK;
  // Return FALSE to this function to mark the event as not
  // consumed...
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_KEY_DOWN],
                  (void *)keyEvent, &return_val);
  if (return_val) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::KeyUp(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aDOMEvent);
  if (!keyEvent)
    return NS_OK;
  // return FALSE to this function to mark this event as not
  // consumed...
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_KEY_UP],
                  (void *)keyEvent, &return_val);
  if (return_val) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  } else {
    //mCtxInfo->UpdateContextData(aDOMEvent);
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::KeyPress(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent;
  keyEvent = do_QueryInterface(aDOMEvent);
  if (!keyEvent)
    return NS_OK;
  // Return TRUE from your signal handler to mark the event as consumed.
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_KEY_PRESS],
                  (void *)keyEvent, &return_val);
  if (return_val) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  }
  return NS_OK;
}

static gboolean
sLongMPress(void *aOwningWidget)
{
  // Return TRUE from your signal handler to mark the event as consumed.
  if (!sMPressed || sIsScrolling)
    return FALSE;
  sMPressed = PR_FALSE;
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(aOwningWidget),
                  moz_embed_signals[DOM_MOUSE_LONG_PRESS],
                  (void *)0, &return_val);
  if (return_val) {
    sMPressed = PR_FALSE;
  }
  return FALSE;
}

NS_IMETHODIMP
EmbedEventListener::MouseDown(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;

  // Return TRUE from your signal handler to mark the event as consumed.
  sMPressed = PR_TRUE;
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_MOUSE_DOWN],
                  (void *)mouseEvent, &return_val);
  if (return_val) {
    mClickCount = 2;
    sMPressed = PR_FALSE;
#if 1
    if (sLongPressTimer)
      g_source_remove(sLongPressTimer);
#else
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
#endif
  } else {
    mClickCount = 1;
    sLongPressTimer = g_timeout_add(mLongMPressDelay, sLongMPress, mOwner->mOwningWidget);
    ((nsIDOMMouseEvent*)mouseEvent)->GetScreenX(&sX);
    ((nsIDOMMouseEvent*)mouseEvent)->GetScreenY(&sY);
  }

  // handling event internally.
#ifdef MOZ_WIDGET_GTK2
  HandleSelection(mouseEvent);
#endif

  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseUp(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;

  // handling event internally, first.
  HandleSelection(mouseEvent);

  // Return TRUE from your signal handler to mark the event as consumed.
  if (sLongPressTimer)
    g_source_remove(sLongPressTimer);
  sMPressed = PR_FALSE;
  mOwner->mOpenBlock = sIsScrolling;
  sIsScrolling = sMPressed;
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_MOUSE_UP],
                  (void *)mouseEvent, &return_val);
  if (return_val) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseClick(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // Return TRUE from your signal handler to mark the event as consumed.
  sMPressed = PR_FALSE;
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_MOUSE_CLICK],
                  (void *)mouseEvent, &return_val);
  if (return_val) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseDblClick(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // Return TRUE from your signal handler to mark the event as consumed.
  if (sLongPressTimer)
    g_source_remove(sLongPressTimer);
  sMPressed = PR_FALSE;
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_MOUSE_DBL_CLICK],
                  (void *)mouseEvent, &return_val);
  if (return_val) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseOver(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // Return TRUE from your signal handler to mark the event as consumed.
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_MOUSE_OVER],
                  (void *)mouseEvent, &return_val);
  if (return_val) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  } else {
    //mCtxInfo->UpdateContextData(aDOMEvent);
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseOut(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent;
  mouseEvent = do_QueryInterface(aDOMEvent);
  if (!mouseEvent)
    return NS_OK;
  // Return TRUE from your signal handler to mark the event as consumed.
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_MOUSE_OUT],
                  (void *)mouseEvent, &return_val);
  if (return_val) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::Activate(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMUIEvent> uiEvent = do_QueryInterface(aDOMEvent);
  if (!uiEvent)
    return NS_OK;
  // Return TRUE from your signal handler to mark the event as consumed.
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_ACTIVATE],
                  (void *)uiEvent, &return_val);
  if (return_val) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::FocusIn(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMUIEvent> uiEvent = do_QueryInterface(aDOMEvent);
  if (!uiEvent)
    return NS_OK;
  // Return TRUE from your signal handler to mark the event as consumed.
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_FOCUS_IN],
                  (void *)uiEvent, &return_val);
  if (return_val) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::FocusOut(nsIDOMEvent* aDOMEvent)
{
  nsCOMPtr<nsIDOMUIEvent> uiEvent = do_QueryInterface(aDOMEvent);
  if (!uiEvent)
    return NS_OK;
  // Return TRUE from your signal handler to mark the event as consumed.
  gint return_val = FALSE;
  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[DOM_FOCUS_OUT],
                  (void *)uiEvent, &return_val);
  if (return_val) {
    aDOMEvent->StopPropagation();
    aDOMEvent->PreventDefault();
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::MouseMove(nsIDOMEvent* aDOMEvent)
{
  if (mCurSelCon)
    mCurSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);

  if (sMPressed &&
      gtk_signal_handler_pending(GTK_OBJECT(mOwner->mOwningWidget),
                                 moz_embed_signals[DOM_MOUSE_SCROLL], TRUE)) {
    // Return TRUE from your signal handler to mark the event as consumed.
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aDOMEvent);
    if (!mouseEvent)
      return NS_OK;
    PRInt32  newX, newY, subX, subY;
    ((nsIDOMMouseEvent*)mouseEvent)->GetScreenX(&newX);
    ((nsIDOMMouseEvent*)mouseEvent)->GetScreenY(&newY);
    subX = newX - sX;
    subY = newY - sY;
    nsresult rv = NS_OK;
    if (ABS(subX) > 10 || ABS(subY) > 10 || (sIsScrolling && sMPressed)) {
      if (!sIsScrolling) {
        gint return_val = FALSE;
        gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                        moz_embed_signals[DOM_MOUSE_SCROLL],
                        (void *)mouseEvent, &return_val);
        if (!return_val) {
          sIsScrolling = PR_TRUE;
#ifdef MOZ_WIDGET_GTK2
          if (mCtxInfo)
            rv = mCtxInfo->GetElementForScroll(aDOMEvent);
#endif
        } else {
          sMPressed = PR_FALSE;
          sIsScrolling = PR_FALSE;
        }
      }
      if (sIsScrolling)
      {
        if (sLongPressTimer)
          g_source_remove(sLongPressTimer);
#ifdef MOZ_WIDGET_GTK2
        if (mCtxInfo->mNSHHTMLElementSc) {
          PRInt32 x, y;
          mCtxInfo->mNSHHTMLElementSc->GetScrollTop(&y);
          mCtxInfo->mNSHHTMLElementSc->GetScrollLeft(&x);
#ifdef MOZ_SCROLL_TOP_LEFT_HACK
          rv = mCtxInfo->mNSHHTMLElementSc->ScrollTopLeft(y - subY, x - subX);
#endif
        } else
#endif
        {
          rv = NS_ERROR_UNEXPECTED;
        }
        if (rv == NS_ERROR_UNEXPECTED) {
          nsCOMPtr<nsIDOMWindow> DOMWindow;
          nsIWebBrowser *webBrowser = nsnull;
          gtk_moz_embed_get_nsIWebBrowser(mOwner->mOwningWidget, &webBrowser);
          webBrowser->GetContentDOMWindow(getter_AddRefs(DOMWindow));
          DOMWindow->ScrollBy(-subX, -subY);
        }
      }
      sX = newX;
      sY = newY;
      sIsScrolling = sMPressed;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::DragMove(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::Focus(nsIDOMEvent* aEvent)
{
  nsString eventType;
  aEvent->GetType(eventType);
#ifdef MOZ_WIDGET_GTK2
  if (eventType.EqualsLiteral("focus") &&
      mCtxInfo->GetFormControlType(aEvent) &&
      mCtxInfo->mEmbedCtxType & GTK_MOZ_EMBED_CTX_INPUT) {
    gint return_val = FALSE;
    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                    moz_embed_signals[DOM_FOCUS],
                    (void *)aEvent, &return_val);
    if (return_val) {
      aEvent->StopPropagation();
      aEvent->PreventDefault();
    }
  }
#endif

  return NS_OK;
}


NS_IMETHODIMP
EmbedEventListener::Blur(nsIDOMEvent* aEvent)
{
  gint return_val = FALSE;
  mFocusInternalFrame = PR_FALSE;

  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aEvent));
  nsCOMPtr<nsIDOMEventTarget> target;
  nsevent->GetOriginalTarget(getter_AddRefs(target));

  if (!target)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);

  if (targetContent) {
#ifdef MOZILLA_1_8_BRANCH
    if (targetContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL)) {
#else
    if (targetContent->IsNodeOfType(nsIContent::eHTML_FORM_CONTROL)) {
#endif
      if (sLongPressTimer)
        g_source_remove(sLongPressTimer);

      sMPressed = sIsScrolling ? PR_FALSE : sMPressed;
      sIsScrolling = PR_FALSE;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::HandleSelection(nsIDOMMouseEvent* aDOMMouseEvent)
{
  nsresult rv;
#ifdef MOZ_WIDGET_GTK2
  /* This function gets called everytime that a mousedown or a mouseup
   * event occurs.
   */
  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aDOMMouseEvent));

  nsCOMPtr<nsIDOMEventTarget> target;
  rv = nsevent->GetOriginalTarget(getter_AddRefs(target));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMNode> eventNode = do_QueryInterface(target);
  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = eventNode->GetOwnerDocument(getter_AddRefs(domDoc));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc, &rv);
  if (NS_FAILED(rv) || !doc)
    return NS_ERROR_FAILURE;

  nsIPresShell *presShell = doc->GetShellAt(0);

  /* Gets nsISelectionController interface for the current context */
  mCurSelCon = do_QueryInterface(presShell);
  if (!mCurSelCon)
    return NS_ERROR_FAILURE;

  /* Gets event type */
  nsString eventType;
  rv = aDOMMouseEvent->GetType(eventType);
  if (NS_FAILED(rv))
    return rv;

  /* Updates context to check which context is being clicked on */
  mCtxInfo->UpdateContextData(aDOMMouseEvent);

  /* If a mousedown after 1 click is done (and if clicked context is not a XUL
   * one (e.g. scrollbar), the selection is disabled for that context.
   */
  if (mCtxInfo->mEmbedCtxType & GTK_MOZ_EMBED_CTX_XUL || mCtxInfo->mEmbedCtxType & GTK_MOZ_EMBED_CTX_RICHEDIT)
    return rv;

  if (eventType.EqualsLiteral("mousedown")) {

    if (mClickCount == 1)
      rv = mCurSelCon->SetDisplaySelection(nsISelectionController::SELECTION_OFF);

  } // mousedown

  /* If a mouseup occurs, the selection for context is enabled again (despite of
   * number of clicks). If this event occurs after 1 click, the selection of
   * both last and current contexts are cleaned up.
   */
  if (eventType.EqualsLiteral("mouseup")) {

    /* Selection controller of current event context */
    if (mCurSelCon) {
      rv = mCurSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
      if (mClickCount == 1) {
        nsCOMPtr<nsISelection> domSel;
        mCurSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                 getter_AddRefs(domSel));
        rv = domSel->RemoveAllRanges();
      }
    }
    /* Selection controller of previous event context */
    if (mLastSelCon) {
      rv = mLastSelCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
      if (mClickCount == 1) {
        nsCOMPtr<nsISelection> domSel;
        mLastSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                  getter_AddRefs(domSel));
        rv = domSel->RemoveAllRanges();
      }
    }

    /* If 1 click was done (despite the event type), sets the last context's
     * selection controller with current one
     */
    if (mClickCount == 1)
      mLastSelCon = mCurSelCon;
  } // mouseup
#endif
  return rv;
}

nsresult
EmbedEventListener::NewURI(nsIURI **result,
                            const char *spec)
{
  nsresult rv;
  nsCString cSpec(spec);
  nsCOMPtr<nsIIOService> ioService;
  rv = GetIOService(getter_AddRefs(ioService));
  if (NS_FAILED(rv))
    return rv;

  rv = ioService->NewURI(cSpec, nsnull, nsnull, result);
  return rv;
}

nsresult
EmbedEventListener::GetIOService(nsIIOService **ioService)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> mgr;
  NS_GetServiceManager(getter_AddRefs(mgr));
  if (!mgr) return NS_ERROR_FAILURE;

  rv = mgr->GetServiceByContractID("@mozilla.org/network/io-service;1",
                                   NS_GET_IID(nsIIOService),
                                   (void **)ioService);
  return rv;
}

#ifdef MOZ_WIDGET_GTK2
void
EmbedEventListener::GeneratePixBuf()
{
  GdkPixbuf *pixbuf = NULL;
  pixbuf = gdk_pixbuf_new_from_file(::gFavLocation, NULL);
  if (!pixbuf) {
    gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                    moz_embed_signals[ICON_CHANGED],
                    NULL );

    // remove the wrong favicon
    // FIXME: need better impl...
    nsCOMPtr<nsILocalFile> faviconFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);

    if (!faviconFile) {
      NS_Free(::gFavLocation);
      gFavLocation = nsnull;
      return;
    }

    nsCString faviconLocation(::gFavLocation);
    faviconFile->InitWithNativePath(faviconLocation);
    faviconFile->Remove(FALSE);
    NS_Free(::gFavLocation);
    gFavLocation = nsnull;
    return;
  }

  gtk_signal_emit(GTK_OBJECT(mOwner->mOwningWidget),
                  moz_embed_signals[ICON_CHANGED],
                  pixbuf );
  //mOwner->mNeedFav = PR_FALSE;
  NS_Free(::gFavLocation);
  gFavLocation = nsnull;
}
#endif

void
EmbedEventListener::GetFaviconFromURI(const char* aURI)
{
  gchar *file_name = NS_strdup(aURI);
  gchar *favicon_uri = NS_strdup(aURI);

  gint i = 0;
  gint rv = 0;

  nsCOMPtr<nsIWebBrowserPersist> persist = do_CreateInstance(NS_WEBBROWSERPERSIST_CONTRACTID);
  if (!persist) {
    NS_Free(file_name);
    NS_Free(favicon_uri);
    return;
  }
  persist->SetProgressListener(this);

  while (file_name[i] != '\0') {
    if (file_name[i] == '/' || file_name[i] == ':')
      file_name[i] = '_';
    i++;
  }

  nsCString fileName(file_name);

  nsCOMPtr<nsILocalFile> favicon_dir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);

  if (!favicon_dir) {
    NS_Free(favicon_uri);
    NS_Free(file_name);
    return;
  }

  nsCString faviconDir("~/.mozilla/favicon");
  favicon_dir->InitWithNativePath(faviconDir);

  PRBool isExist;
  rv = favicon_dir->Exists(&isExist);
  if (NS_SUCCEEDED(rv) && !isExist) {
    rv = favicon_dir->Create(nsIFile::DIRECTORY_TYPE,0775);
    if (NS_FAILED(rv)) {
      NS_Free(file_name);
      NS_Free(favicon_uri);
      return;
    }
  }

  nsCAutoString favicon_path("~/.mozilla/favicon");
  nsCOMPtr<nsILocalFile> target_file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  if (!target_file) {
    NS_Free(file_name);
    NS_Free(favicon_uri);
    return;
  }
  target_file->InitWithNativePath(favicon_path);
  target_file->Append(NS_ConvertUTF8toUTF16(fileName));

  nsString path;
  target_file->GetPath(path);
  ::gFavLocation = NS_strdup(NS_ConvertUTF16toUTF8(path).get());
  nsCOMPtr<nsIIOService> ios(do_GetService(NS_IOSERVICE_CONTRACTID));
  if (!ios) {
    NS_Free(file_name);
    NS_Free(favicon_uri);
    NS_Free(::gFavLocation);
    gFavLocation = nsnull;
    return;
  }

  nsCOMPtr<nsIURI> uri;

  rv = ios->NewURI(nsDependentCString(favicon_uri), "", nsnull, getter_AddRefs(uri));
  if (!uri) {
    NS_Free(file_name);
    NS_Free(favicon_uri);
    NS_Free(::gFavLocation);
    gFavLocation = nsnull;
    return;
  }
  NS_Free(file_name);
  NS_Free(favicon_uri);

  // save the favicon if the icon does not exist
  rv = target_file->Exists(&isExist);
  if (NS_SUCCEEDED(rv) && !isExist) {
    rv = persist->SaveURI(uri, nsnull, nsnull, nsnull, "", target_file);
    if (NS_FAILED(rv)) {
      return;
    }
  }
  else {
#ifdef MOZ_WIDGET_GTK2
    GeneratePixBuf();
#endif
  }

}

NS_IMETHODIMP
EmbedEventListener::OnStateChange(nsIWebProgress *aWebProgress,
                                  nsIRequest *aRequest,
                                  PRUint32 aStateFlags,
                                  nsresult aStatus)
{
  /* if (!(aStateFlags & (STATE_STOP | STATE_IS_NETWORK | STATE_IS_DOCUMENT))){*/
#ifdef MOZ_WIDGET_GTK2
  if (aStateFlags & STATE_STOP)
    /* FINISH DOWNLOADING */
    /* XXX sometimes this==0x0 and it cause crash in GeneratePixBuf, need workaround check for this */
    if (NS_SUCCEEDED(aStatus) && this)
      GeneratePixBuf();
#endif
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::OnProgressChange(nsIWebProgress *aWebProgress,
                                     nsIRequest *aRequest,
                                     PRInt32 aCurSelfProgress,
                                     PRInt32 aMaxSelfProgress,
                                     PRInt32 aCurTotalProgress,
                                     PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
EmbedEventListener::OnLocationChange(nsIWebProgress *aWebProgress,
                                     nsIRequest *aRequest,
                                     nsIURI *aLocation)
{
  return NS_OK;
}


NS_IMETHODIMP
EmbedEventListener::OnStatusChange(nsIWebProgress *aWebProgress,
                                   nsIRequest *aRequest,
                                   nsresult aStatus,
                                   const PRUnichar *aMessage)
{
  return NS_OK;
}


NS_IMETHODIMP
EmbedEventListener::OnSecurityChange(nsIWebProgress *aWebProgress,
                                     nsIRequest *aRequest,
                                     PRUint32 aState)
{
  return NS_OK;
}
