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
 *   Steve Clark (buster@netscape.com)
 *   Ilya Konstantinov (mozilla-code@future.shiny.co.il)
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

#include "nsCOMPtr.h"
#include "nsDOMEvent.h"
#include "nsEventStateManager.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIPrivateCompositionEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "prmem.h"
#include "nsGkAtoms.h"
#include "nsMutationEvent.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptError.h"

static const char* const sEventNames[] = {
  "mousedown", "mouseup", "click", "dblclick", "mouseover",
  "mouseout", "mousemove", "contextmenu", "keydown", "keyup", "keypress",
  "focus", "blur", "load", "beforeunload", "unload", "abort", "error",
  "submit", "reset", "change", "select", "input", "paint" ,"text",
  "compositionstart", "compositionend", "popupshowing", "popupshown",
  "popuphiding", "popuphidden", "close", "command", "broadcast", "commandupdate",
  "dragenter", "dragover", "dragexit", "dragdrop", "draggesture",
  "drag", "dragend", "dragstart", "dragleave", "drop", "resize",
  "scroll", "overflow", "underflow", "overflowchanged",
  "DOMSubtreeModified", "DOMNodeInserted", "DOMNodeRemoved", 
  "DOMNodeRemovedFromDocument", "DOMNodeInsertedIntoDocument",
  "DOMAttrModified", "DOMCharacterDataModified",
  "DOMActivate", "DOMFocusIn", "DOMFocusOut",
  "pageshow", "pagehide", "DOMMouseScroll"
#ifdef MOZ_SVG
 ,
  "SVGLoad", "SVGUnload", "SVGAbort", "SVGError", "SVGResize", "SVGScroll",
  "SVGZoom"
#endif // MOZ_SVG
};

static char *sPopupAllowedEvents;


nsDOMEvent::nsDOMEvent(nsPresContext* aPresContext, nsEvent* aEvent)
{
  mPresContext = aPresContext;

  if (aEvent) {
    mEvent = aEvent;
    mEventIsInternal = PR_FALSE;
  }
  else {
    mEventIsInternal = PR_TRUE;
    /*
      A derived class might want to allocate its own type of aEvent
      (derived from nsEvent). To do this, it should take care to pass
      a non-NULL aEvent to this ctor, e.g.:
      
        nsDOMFooEvent::nsDOMFooEvent(..., nsEvent* aEvent)
        : nsDOMEvent(..., aEvent ? aEvent : new nsFooEvent())
      
      Then, to override the mEventIsInternal assignments done by the
      base ctor, it should do this in its own ctor:

        nsDOMFooEvent::nsDOMFooEvent(..., nsEvent* aEvent)
        ...
        {
          ...
          if (aEvent) {
            mEventIsInternal = PR_FALSE;
          }
          else {
            mEventIsInternal = PR_TRUE;
          }
          ...
        }
     */
    mEvent = new nsEvent(PR_FALSE, 0);
    mEvent->time = PR_Now();
  }

  // Get the explicit original target (if it's anonymous make it null)
  {
    mExplicitOriginalTarget = GetTargetFromFrame();
    mTmpRealOriginalTarget = mExplicitOriginalTarget;
    nsCOMPtr<nsIContent> content = do_QueryInterface(mExplicitOriginalTarget);
    if (content) {
      if (content->IsNativeAnonymous() || content->GetBindingParent()) {
        mExplicitOriginalTarget = nsnull;
      }
    }
  }
}

nsDOMEvent::~nsDOMEvent() 
{
  NS_ASSERT_OWNINGTHREAD(nsDOMEvent);

  if (mEventIsInternal && mEvent) {
    delete mEvent;
  }
}

NS_IMPL_ADDREF(nsDOMEvent)
NS_IMPL_RELEASE(nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSEvent)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateDOMEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Event)
NS_INTERFACE_MAP_END

// nsIDOMEventInterface
NS_METHOD nsDOMEvent::GetType(nsAString& aType)
{
  const char* name = GetEventName(mEvent->message);

  if (name) {
    CopyASCIItoUTF16(name, aType);
    return NS_OK;
  } else if (mEvent->message == NS_USER_DEFINED_EVENT && mEvent->userType) {
    nsAutoString name;
    mEvent->userType->ToString(name);
    aType = Substring(name, 2, name.Length() - 2); // Remove "on"
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::GetTarget(nsIDOMEventTarget** aTarget)
{
  if (mEvent->target) {
    return CallQueryInterface(mEvent->target, aTarget);
  }
  *aTarget = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetCurrentTarget(nsIDOMEventTarget** aCurrentTarget)
{
  if (mEvent->currentTarget) {
    return CallQueryInterface(mEvent->currentTarget, aCurrentTarget);
  }
  *aCurrentTarget = nsnull;
  return NS_OK;
}

//
// Get the actual event target node (may have been retargeted for mouse events)
//
already_AddRefed<nsIDOMEventTarget>
nsDOMEvent::GetTargetFromFrame()
{
  if (!mPresContext) { return nsnull; }

  // Get the target frame (have to get the ESM first)
  nsIFrame* targetFrame = nsnull;
  mPresContext->EventStateManager()->GetEventTarget(&targetFrame);
  if (!targetFrame) { return nsnull; }

  // get the real content
  nsCOMPtr<nsIContent> realEventContent;
  targetFrame->GetContentForEvent(mPresContext, mEvent, getter_AddRefs(realEventContent));
  if (!realEventContent) { return nsnull; }

  // Finally, we have the real content.  QI it and return.
  nsIDOMEventTarget* target = nsnull;
  CallQueryInterface(realEventContent, &target);
  return target;
}

NS_IMETHODIMP
nsDOMEvent::GetExplicitOriginalTarget(nsIDOMEventTarget** aRealEventTarget)
{
  if (mExplicitOriginalTarget) {
    *aRealEventTarget = mExplicitOriginalTarget;
    NS_ADDREF(*aRealEventTarget);
    return NS_OK;
  }

  return GetTarget(aRealEventTarget);
}

NS_IMETHODIMP
nsDOMEvent::GetTmpRealOriginalTarget(nsIDOMEventTarget** aRealEventTarget)
{
  if (mTmpRealOriginalTarget) {
    *aRealEventTarget = mTmpRealOriginalTarget;
    NS_ADDREF(*aRealEventTarget);
    return NS_OK;
  }

  return GetOriginalTarget(aRealEventTarget);
}

NS_IMETHODIMP
nsDOMEvent::GetOriginalTarget(nsIDOMEventTarget** aOriginalTarget)
{
  if (mEvent->originalTarget) {
    return CallQueryInterface(mEvent->originalTarget, aOriginalTarget);
  }

  return GetTarget(aOriginalTarget);
}

NS_IMETHODIMP
nsDOMEvent::HasOriginalTarget(PRBool* aResult)
{
  *aResult = !!(mEvent->originalTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::SetTrusted(PRBool aTrusted)
{
  if (aTrusted) {
    mEvent->flags |= NS_EVENT_FLAG_TRUSTED;
  } else {
    mEvent->flags &= ~NS_EVENT_FLAG_TRUSTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetEventPhase(PRUint16* aEventPhase)
{
  // Note, remember to check that this works also
  // if or when Bug 235441 is fixed.
  if (mEvent->currentTarget == mEvent->target ||
      ((mEvent->flags & NS_EVENT_FLAG_CAPTURE) &&
       (mEvent->flags & NS_EVENT_FLAG_BUBBLE))) {
    *aEventPhase = nsIDOMEvent::AT_TARGET;
  } else if (mEvent->flags & NS_EVENT_FLAG_CAPTURE) {
    *aEventPhase = nsIDOMEvent::CAPTURING_PHASE;
  } else if (mEvent->flags & NS_EVENT_FLAG_BUBBLE) {
    *aEventPhase = nsIDOMEvent::BUBBLING_PHASE;
  } else {
    *aEventPhase = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetBubbles(PRBool* aBubbles)
{
  *aBubbles = !(mEvent->flags & NS_EVENT_FLAG_CANT_BUBBLE);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetCancelable(PRBool* aCancelable)
{
  *aCancelable = !(mEvent->flags & NS_EVENT_FLAG_CANT_CANCEL);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetTimeStamp(PRUint64* aTimeStamp)
{
  LL_UI2L(*aTimeStamp, mEvent->time);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::StopPropagation()
{
  mEvent->flags |= NS_EVENT_FLAG_STOP_DISPATCH;
  return NS_OK;
}

static void
ReportUseOfDeprecatedMethod(nsEvent* aEvent, nsIDOMEvent* aDOMEvent,
                            const char* aWarning)
{
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsINode> node = do_QueryInterface(aEvent->currentTarget);
  if (node) {
    doc = node->GetOwnerDoc();
  } else {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aEvent->currentTarget);
    if (window) {
      doc = do_QueryInterface(window->GetExtantDocument());
    }
  }

  nsAutoString type;
  aDOMEvent->GetType(type);
  const PRUnichar *strings[] = { type.get() };
  nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                  aWarning,
                                  strings, NS_ARRAY_LENGTH(strings),
                                  doc ? doc->GetDocumentURI() : nsnull,
                                  EmptyString(), 0, 0,
                                  nsIScriptError::warningFlag,
                                  "DOM Events");
}

NS_IMETHODIMP
nsDOMEvent::PreventBubble()
{
  ReportUseOfDeprecatedMethod(mEvent, this, "UseOfPreventBubbleWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::PreventCapture()
{
  ReportUseOfDeprecatedMethod(mEvent, this, "UseOfPreventCaptureWarning");
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetIsTrusted(PRBool *aIsTrusted)
{
  *aIsTrusted = NS_IS_TRUSTED_EVENT(mEvent);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::PreventDefault()
{
  if (!(mEvent->flags & NS_EVENT_FLAG_CANT_CANCEL)) {
    mEvent->flags |= NS_EVENT_FLAG_NO_DEFAULT;
  }
  return NS_OK;
}

nsresult
nsDOMEvent::SetEventType(const nsAString& aEventTypeArg)
{
  nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aEventTypeArg);
  mEvent->message = NS_USER_DEFINED_EVENT;

  if (mEvent->eventStructType == NS_MOUSE_EVENT) {
    if (atom == nsGkAtoms::onmousedown)
      mEvent->message = NS_MOUSE_BUTTON_DOWN;
    else if (atom == nsGkAtoms::onmouseup)
      mEvent->message = NS_MOUSE_BUTTON_UP;
    else if (atom == nsGkAtoms::onclick)
      mEvent->message = NS_MOUSE_CLICK;
    else if (atom == nsGkAtoms::ondblclick)
      mEvent->message = NS_MOUSE_DOUBLECLICK;
    else if (atom == nsGkAtoms::onmouseover)
      mEvent->message = NS_MOUSE_ENTER_SYNTH;
    else if (atom == nsGkAtoms::onmouseout)
      mEvent->message = NS_MOUSE_EXIT_SYNTH;
    else if (atom == nsGkAtoms::onmousemove)
      mEvent->message = NS_MOUSE_MOVE;
    else if (atom == nsGkAtoms::oncontextmenu)
      mEvent->message = NS_CONTEXTMENU;
  } else if (mEvent->eventStructType == NS_MOUSE_SCROLL_EVENT) {
    if (atom == nsGkAtoms::onDOMMouseScroll)
      mEvent->message = NS_MOUSE_SCROLL;
  } else if (mEvent->eventStructType == NS_KEY_EVENT) {
    if (atom == nsGkAtoms::onkeydown)
      mEvent->message = NS_KEY_DOWN;
    else if (atom == nsGkAtoms::onkeyup)
      mEvent->message = NS_KEY_UP;
    else if (atom == nsGkAtoms::onkeypress)
      mEvent->message = NS_KEY_PRESS;
  } else if (mEvent->eventStructType == NS_COMPOSITION_EVENT) {
    if (atom == nsGkAtoms::oncompositionstart)
      mEvent->message = NS_COMPOSITION_START;
    else if (atom == nsGkAtoms::oncompositionend)
      mEvent->message = NS_COMPOSITION_END;
  } else if (mEvent->eventStructType == NS_EVENT) {
    if (atom == nsGkAtoms::onfocus)
      mEvent->message = NS_FOCUS_CONTENT;
    else if (atom == nsGkAtoms::onblur)
      mEvent->message = NS_BLUR_CONTENT;
    else if (atom == nsGkAtoms::onsubmit)
      mEvent->message = NS_FORM_SUBMIT;
    else if (atom == nsGkAtoms::onreset)
      mEvent->message = NS_FORM_RESET;
    else if (atom == nsGkAtoms::onchange)
      mEvent->message = NS_FORM_CHANGE;
    else if (atom == nsGkAtoms::onselect)
      mEvent->message = NS_FORM_SELECTED;
    else if (atom == nsGkAtoms::onload)
      mEvent->message = NS_LOAD;
    else if (atom == nsGkAtoms::onunload)
      mEvent->message = NS_PAGE_UNLOAD;
    else if (atom == nsGkAtoms::onabort)
      mEvent->message = NS_IMAGE_ABORT;
    else if (atom == nsGkAtoms::onerror)
      mEvent->message = NS_LOAD_ERROR;
  } else if (mEvent->eventStructType == NS_MUTATION_EVENT) {
    if (atom == nsGkAtoms::onDOMAttrModified)
      mEvent->message = NS_MUTATION_ATTRMODIFIED;
    else if (atom == nsGkAtoms::onDOMCharacterDataModified)
      mEvent->message = NS_MUTATION_CHARACTERDATAMODIFIED;
    else if (atom == nsGkAtoms::onDOMNodeInserted)
      mEvent->message = NS_MUTATION_NODEINSERTED;
    else if (atom == nsGkAtoms::onDOMNodeRemoved)
      mEvent->message = NS_MUTATION_NODEREMOVED;
    else if (atom == nsGkAtoms::onDOMNodeInsertedIntoDocument)
      mEvent->message = NS_MUTATION_NODEINSERTEDINTODOCUMENT;
    else if (atom == nsGkAtoms::onDOMNodeRemovedFromDocument)
      mEvent->message = NS_MUTATION_NODEREMOVEDFROMDOCUMENT;
    else if (atom == nsGkAtoms::onDOMSubtreeModified)
      mEvent->message = NS_MUTATION_SUBTREEMODIFIED;
  } else if (mEvent->eventStructType == NS_UI_EVENT) {
    if (atom == nsGkAtoms::onDOMActivate)
      mEvent->message = NS_UI_ACTIVATE;
    else if (atom == nsGkAtoms::onDOMFocusIn)
      mEvent->message = NS_UI_FOCUSIN;
    else if (atom == nsGkAtoms::onDOMFocusOut)
      mEvent->message = NS_UI_FOCUSOUT;
    else if (atom == nsGkAtoms::oninput)
      mEvent->message = NS_FORM_INPUT;
  } else if (mEvent->eventStructType == NS_PAGETRANSITION_EVENT) {
    if (atom == nsGkAtoms::onpageshow)
      mEvent->message = NS_PAGE_SHOW;
    else if (atom == nsGkAtoms::onpagehide)
      mEvent->message = NS_PAGE_HIDE;
  } else if (mEvent->eventStructType == NS_XUL_COMMAND_EVENT) {
    if (atom == nsGkAtoms::oncommand)
      mEvent->message = NS_XUL_COMMAND;
  }
#ifdef MOZ_SVG
  else if (mEvent->eventStructType == NS_SVG_EVENT) {
    if (atom == nsGkAtoms::onSVGLoad)
      mEvent->message = NS_SVG_LOAD;
    else if (atom == nsGkAtoms::onSVGUnload)
      mEvent->message = NS_SVG_UNLOAD;
    else if (atom == nsGkAtoms::onSVGAbort)
      mEvent->message = NS_SVG_ABORT;
    else if (atom == nsGkAtoms::onSVGError)
      mEvent->message = NS_SVG_ERROR;
    else if (atom == nsGkAtoms::onSVGResize)
      mEvent->message = NS_SVG_RESIZE;
    else if (atom == nsGkAtoms::onSVGScroll)
      mEvent->message = NS_SVG_SCROLL;
  } else if (mEvent->eventStructType == NS_SVGZOOM_EVENT) {
    if (atom == nsGkAtoms::onSVGZoom)
      mEvent->message = NS_SVG_ZOOM;
  }
#endif // MOZ_SVG

  if (mEvent->message == NS_USER_DEFINED_EVENT)
    mEvent->userType = atom;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::InitEvent(const nsAString& aEventTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg)
{
  // Make sure this event isn't already being dispatched.
  NS_ENSURE_TRUE(!NS_IS_EVENT_IN_DISPATCH(mEvent), NS_ERROR_INVALID_ARG);

  if (NS_IS_TRUSTED_EVENT(mEvent)) {
    // Ensure the caller is permitted to dispatch trusted DOM events.

    PRBool enabled = PR_FALSE;
    nsContentUtils::GetSecurityManager()->
      IsCapabilityEnabled("UniversalBrowserWrite", &enabled);

    if (!enabled) {
      SetTrusted(PR_FALSE);
    }
  }

  NS_ENSURE_SUCCESS(SetEventType(aEventTypeArg), NS_ERROR_FAILURE);

  if (aCanBubbleArg) {
    mEvent->flags &= ~NS_EVENT_FLAG_CANT_BUBBLE;
  } else {
    mEvent->flags |= NS_EVENT_FLAG_CANT_BUBBLE;
  }

  if (aCancelableArg) {
    mEvent->flags &= ~NS_EVENT_FLAG_CANT_CANCEL;
  } else {
    mEvent->flags |= NS_EVENT_FLAG_CANT_CANCEL;
  }

  // Clearing the old targets, so that the event is targeted correctly when
  // re-dispatching it.
  mEvent->target = nsnull;
  mEvent->originalTarget = nsnull;

  // Unset the NS_EVENT_FLAG_STOP_DISPATCH_IMMEDIATELY bit (which is
  // set at the end of event dispatch) so that this event can be
  // dispatched.
  mEvent->flags &= ~NS_EVENT_FLAG_STOP_DISPATCH_IMMEDIATELY;

  return NS_OK;
}

NS_METHOD nsDOMEvent::DuplicatePrivateData()
{
  // FIXME! Simplify this method and make it somehow easily extendable,
  //        Bug 329127
  
  NS_ASSERTION(mEvent, "No nsEvent for nsDOMEvent duplication!");
  if (mEventIsInternal) {
    return NS_OK;
  }

  nsEvent* newEvent = nsnull;
  PRUint32 msg = mEvent->message;
  PRBool isInputEvent = PR_FALSE;

  switch (mEvent->eventStructType) {
    case NS_EVENT:
    {
      newEvent = new nsEvent(PR_FALSE, msg);
      break;
    }
    case NS_GUI_EVENT:
    {
      // Not copying widget, it is a weak reference.
      newEvent = new nsGUIEvent(PR_FALSE, msg, nsnull);
      break;
    }
    case NS_SIZE_EVENT:
    {
      nsSizeEvent* sizeEvent = new nsSizeEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(sizeEvent, NS_ERROR_OUT_OF_MEMORY);
      sizeEvent->mWinWidth = NS_STATIC_CAST(nsSizeEvent*, mEvent)->mWinWidth;
      sizeEvent->mWinHeight = NS_STATIC_CAST(nsSizeEvent*, mEvent)->mWinHeight;
      newEvent = sizeEvent;
      break;
    }
    case NS_SIZEMODE_EVENT:
    {
      newEvent = new nsSizeModeEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      NS_STATIC_CAST(nsSizeModeEvent*, newEvent)->mSizeMode =
        NS_STATIC_CAST(nsSizeModeEvent*, mEvent)->mSizeMode;
      break;
    }
    case NS_ZLEVEL_EVENT:
    {
      nsZLevelEvent* zLevelEvent = new nsZLevelEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(zLevelEvent, NS_ERROR_OUT_OF_MEMORY);
      nsZLevelEvent* oldZLevelEvent = NS_STATIC_CAST(nsZLevelEvent*, mEvent);
      zLevelEvent->mPlacement = oldZLevelEvent->mPlacement;
      zLevelEvent->mImmediate = oldZLevelEvent->mImmediate;
      zLevelEvent->mAdjusted = oldZLevelEvent->mAdjusted;
      newEvent = zLevelEvent;
      break;
    }
    case NS_PAINT_EVENT:
    {
      newEvent = new nsPaintEvent(PR_FALSE, msg, nsnull);
      break;
    }
    case NS_SCROLLBAR_EVENT:
    {
      newEvent = new nsScrollbarEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      NS_STATIC_CAST(nsScrollbarEvent*, newEvent)->position =
        NS_STATIC_CAST(nsScrollbarEvent*, mEvent)->position;
      break;
    }
    case NS_INPUT_EVENT:
    {
      newEvent = new nsInputEvent(PR_FALSE, msg, nsnull);
      isInputEvent = PR_TRUE;
      break;
    }
    case NS_KEY_EVENT:
    {
      nsKeyEvent* keyEvent = new nsKeyEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(keyEvent, NS_ERROR_OUT_OF_MEMORY);
      nsKeyEvent* oldKeyEvent = NS_STATIC_CAST(nsKeyEvent*, mEvent);
      isInputEvent = PR_TRUE;
      keyEvent->keyCode = oldKeyEvent->keyCode;
      keyEvent->charCode = oldKeyEvent->charCode;
      keyEvent->isChar = oldKeyEvent->isChar;
      newEvent = keyEvent;
      break;
    }
    case NS_MOUSE_EVENT:
    {
      nsMouseEvent* oldMouseEvent = NS_STATIC_CAST(nsMouseEvent*, mEvent);
      nsMouseEvent* mouseEvent =
        new nsMouseEvent(PR_FALSE, msg, nsnull, oldMouseEvent->reason);
      NS_ENSURE_TRUE(mouseEvent, NS_ERROR_OUT_OF_MEMORY);
      isInputEvent = PR_TRUE;
      mouseEvent->clickCount = oldMouseEvent->clickCount;
      mouseEvent->acceptActivation = oldMouseEvent->acceptActivation;
      mouseEvent->relatedTarget = oldMouseEvent->relatedTarget;
      mouseEvent->button = oldMouseEvent->button;
      newEvent = mouseEvent;
      break;
    }
    case NS_MENU_EVENT:
    {
      newEvent = new nsMenuEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      NS_STATIC_CAST(nsMenuEvent*, newEvent)->mCommand =
        NS_STATIC_CAST(nsMenuEvent*, mEvent)->mCommand;
      break;
    }
    case NS_SCRIPT_ERROR_EVENT:
    {
      newEvent = new nsScriptErrorEvent(PR_FALSE, msg);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      NS_STATIC_CAST(nsScriptErrorEvent*, newEvent)->lineNr =
        NS_STATIC_CAST(nsScriptErrorEvent*, mEvent)->lineNr;
      break;
    }
    case NS_TEXT_EVENT:
    {
      newEvent = new nsTextEvent(PR_FALSE, msg, nsnull);
      isInputEvent = PR_TRUE;
      break;
    }
    case NS_COMPOSITION_EVENT:
    {
      newEvent = new nsCompositionEvent(PR_FALSE, msg, nsnull);
      isInputEvent = PR_TRUE;
      break;
    }
    case NS_RECONVERSION_EVENT:
    {
      newEvent = new nsReconversionEvent(PR_FALSE, msg, nsnull);
      isInputEvent = PR_TRUE;
      break;
    }
    case NS_MOUSE_SCROLL_EVENT:
    {
      nsMouseScrollEvent* mouseScrollEvent =
        new nsMouseScrollEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(mouseScrollEvent, NS_ERROR_OUT_OF_MEMORY);
      isInputEvent = PR_TRUE;
      nsMouseScrollEvent* oldMouseScrollEvent =
        NS_STATIC_CAST(nsMouseScrollEvent*, mEvent);
      mouseScrollEvent->scrollFlags = oldMouseScrollEvent->scrollFlags;
      mouseScrollEvent->delta = oldMouseScrollEvent->delta;
      mouseScrollEvent->relatedTarget = oldMouseScrollEvent->relatedTarget;
      mouseScrollEvent->button = oldMouseScrollEvent->button;
      newEvent = mouseScrollEvent;
      break;
    }
    case NS_SCROLLPORT_EVENT:
    {
      newEvent = new nsScrollPortEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      NS_STATIC_CAST(nsScrollPortEvent*, newEvent)->orient =
        NS_STATIC_CAST(nsScrollPortEvent*, mEvent)->orient;
      break;
    }
    case NS_MUTATION_EVENT:
    {
      nsMutationEvent* mutationEvent = new nsMutationEvent(PR_FALSE, msg);
      NS_ENSURE_TRUE(mutationEvent, NS_ERROR_OUT_OF_MEMORY);
      nsMutationEvent* oldMutationEvent =
        NS_STATIC_CAST(nsMutationEvent*, mEvent);
      mutationEvent->mRelatedNode = oldMutationEvent->mRelatedNode;
      mutationEvent->mAttrName = oldMutationEvent->mAttrName;
      mutationEvent->mPrevAttrValue = oldMutationEvent->mPrevAttrValue;
      mutationEvent->mNewAttrValue = oldMutationEvent->mNewAttrValue;
      mutationEvent->mAttrChange = oldMutationEvent->mAttrChange;
      newEvent = mutationEvent;
      break;
    }
    case NS_ACCESSIBLE_EVENT:
    {
      newEvent = new nsAccessibleEvent(PR_FALSE, msg, nsnull);
      isInputEvent = PR_TRUE;
      break;
    }
    case NS_FORM_EVENT:
    {
      newEvent = new nsFormEvent(PR_FALSE, msg);
      break;
    }
    case NS_FOCUS_EVENT:
    {
      newEvent = new nsFocusEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      NS_STATIC_CAST(nsFocusEvent*, newEvent)->isMozWindowTakingFocus =
        NS_STATIC_CAST(nsFocusEvent*, mEvent)->isMozWindowTakingFocus;
      break;
    }
    case NS_POPUP_EVENT:
    {
      newEvent = new nsInputEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      isInputEvent = PR_TRUE;
      newEvent->eventStructType = NS_POPUP_EVENT;
      break;
    }
    case NS_COMMAND_EVENT:
    {
      newEvent = new nsCommandEvent(PR_FALSE, mEvent->userType,
        NS_STATIC_CAST(nsCommandEvent*, mEvent)->command, nsnull);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      break;
    }
    case NS_POPUPBLOCKED_EVENT:
    {
      NS_WARNING("nsPopupBlockedEvent should never be an external event!");
      newEvent = new nsPopupBlockedEvent(PR_FALSE, msg);
      break;
    }
    case NS_BEFORE_PAGE_UNLOAD_EVENT:
    {
      newEvent = new nsBeforePageUnloadEvent(PR_FALSE, msg);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      NS_STATIC_CAST(nsBeforePageUnloadEvent*, newEvent)->text =
        NS_STATIC_CAST(nsBeforePageUnloadEvent*, mEvent)->text;
      break;
    }
    case NS_UI_EVENT:
    {
      newEvent = new nsUIEvent(PR_FALSE, msg,
                               NS_STATIC_CAST(nsUIEvent*, mEvent)->detail);
      break;
    }
    case NS_QUERYCARETRECT_EVENT:
    {
      newEvent = new nsQueryCaretRectEvent(PR_FALSE, msg, nsnull);
      isInputEvent = PR_TRUE;
      break;
    }
    case NS_PAGETRANSITION_EVENT:
    {
      newEvent =
        new nsPageTransitionEvent(PR_FALSE, msg,
                                  ((nsPageTransitionEvent*) mEvent)->persisted);
      break;
    }
#ifdef MOZ_SVG
    case NS_SVG_EVENT:
    {
      newEvent = new nsEvent(PR_FALSE, msg);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      newEvent->eventStructType = NS_SVG_EVENT;
      break;
    }
    case NS_SVGZOOM_EVENT:
    {
      newEvent = new nsGUIEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      newEvent->eventStructType = NS_SVGZOOM_EVENT;
      break;
    }
#endif // MOZ_SVG
    case NS_XUL_COMMAND_EVENT:
    {
      newEvent = new nsXULCommandEvent(PR_FALSE, msg, nsnull);
      NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);
      isInputEvent = PR_TRUE;
      newEvent->eventStructType = NS_XUL_COMMAND_EVENT;
       NS_STATIC_CAST(nsXULCommandEvent*, newEvent)->sourceEvent =
         NS_STATIC_CAST(nsXULCommandEvent*, mEvent)->sourceEvent;
      break;
    }
    default:
    {
      NS_WARNING("Unknown event type!!!");
      return NS_ERROR_FAILURE;
    }
  }

  NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);

  if (isInputEvent) {
    nsInputEvent* oldInputEvent = NS_STATIC_CAST(nsInputEvent*, mEvent);
    nsInputEvent* newInputEvent = NS_STATIC_CAST(nsInputEvent*, newEvent);
    newInputEvent->isShift = oldInputEvent->isShift;
    newInputEvent->isControl = oldInputEvent->isControl;
    newInputEvent->isAlt = oldInputEvent->isAlt;
    newInputEvent->isMeta = oldInputEvent->isMeta;
  }

  newEvent->target                 = mEvent->target;
  newEvent->currentTarget          = mEvent->currentTarget;
  newEvent->originalTarget         = mEvent->originalTarget;
  newEvent->flags                  = mEvent->flags;
  newEvent->time                   = mEvent->time;
  newEvent->refPoint               = mEvent->refPoint;
  newEvent->userType               = mEvent->userType;

  mEvent = newEvent;
  mPresContext = nsnull;
  mEventIsInternal = PR_TRUE;

  return NS_OK;
}

NS_METHOD nsDOMEvent::SetTarget(nsIDOMEventTarget* aTarget)
{
#ifdef DEBUG
  {
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aTarget);

    NS_ASSERTION(!win || !win->IsInnerWindow(),
                 "Uh, inner window set as event target!");
  }
#endif

  mEvent->target = aTarget;
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetCurrentTarget(nsIDOMEventTarget* aCurrentTarget)
{
#ifdef DEBUG
  {
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aCurrentTarget);

    NS_ASSERTION(!win || !win->IsInnerWindow(),
                 "Uh, inner window set as event target!");
  }
#endif

  mEvent->currentTarget = aCurrentTarget;
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetOriginalTarget(nsIDOMEventTarget* aOriginalTarget)
{
#ifdef DEBUG
  {
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aOriginalTarget);

    NS_ASSERTION(!win || !win->IsInnerWindow(),
                 "Uh, inner window set as event target!");
  }
#endif

  mEvent->originalTarget = aOriginalTarget;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::IsDispatchStopped(PRBool* aIsDispatchStopped)
{
  if (mEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH) {
    *aIsDispatchStopped = PR_TRUE;
  } else {
    *aIsDispatchStopped = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetInternalNSEvent(nsEvent** aNSEvent)
{
  NS_ENSURE_ARG_POINTER(aNSEvent);
  *aNSEvent = mEvent;
  return NS_OK;
}

// return true if eventName is contained within events, delimited by
// spaces
static PRBool
PopupAllowedForEvent(const char *eventName)
{
  if (!sPopupAllowedEvents) {
    nsDOMEvent::PopupAllowedEventsChanged();

    if (!sPopupAllowedEvents) {
      return PR_FALSE;
    }
  }

  nsDependentCString events(sPopupAllowedEvents);

  nsAFlatCString::const_iterator start, end;
  nsAFlatCString::const_iterator startiter(events.BeginReading(start));
  events.EndReading(end);

  while (startiter != end) {
    nsAFlatCString::const_iterator enditer(end);

    if (!FindInReadable(nsDependentCString(eventName), startiter, enditer))
      return PR_FALSE;

    // the match is surrounded by spaces, or at a string boundary
    if ((startiter == start || *--startiter == ' ') &&
        (enditer == end || *enditer == ' ')) {
      return PR_TRUE;
    }

    // Move on and see if there are other matches. (The delimitation
    // requirement makes it pointless to begin the next search before
    // the end of the invalid match just found.)
    startiter = enditer;
  }

  return PR_FALSE;
}

// static
PopupControlState
nsDOMEvent::GetEventPopupControlState(nsEvent *aEvent)
{
  // generally if an event handler is running, new windows are disallowed.
  // check for exceptions:
  PopupControlState abuse = openAbused;

  switch(aEvent->eventStructType) {
  case NS_EVENT :
    // For these following events only allow popups if they're
    // triggered while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (nsEventStateManager::IsHandlingUserInput()) {
      switch(aEvent->message) {
      case NS_FORM_SELECTED :
        if (::PopupAllowedForEvent("select"))
          abuse = openControlled;
        break;
      case NS_FORM_CHANGE :
        if (::PopupAllowedForEvent("change"))
          abuse = openControlled;
        break;
      }
    }
    break;
  case NS_GUI_EVENT :
    // For this following event only allow popups if it's triggered
    // while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (nsEventStateManager::IsHandlingUserInput()) {
      switch(aEvent->message) {
      case NS_FORM_INPUT :
        if (::PopupAllowedForEvent("input"))
          abuse = openControlled;
        break;
      }
    }
    break;
  case NS_INPUT_EVENT :
    // For this following event only allow popups if it's triggered
    // while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (nsEventStateManager::IsHandlingUserInput()) {
      switch(aEvent->message) {
      case NS_FORM_CHANGE :
        if (::PopupAllowedForEvent("change"))
          abuse = openControlled;
        break;
      }
    }
    break;
  case NS_KEY_EVENT :
    if (NS_IS_TRUSTED_EVENT(aEvent)) {
      PRUint32 key = NS_STATIC_CAST(nsKeyEvent *, aEvent)->keyCode;
      switch(aEvent->message) {
      case NS_KEY_PRESS :
        // return key on focused button. see note at NS_MOUSE_CLICK.
        if (key == nsIDOMKeyEvent::DOM_VK_RETURN)
          abuse = openAllowed;
        else if (::PopupAllowedForEvent("keypress"))
          abuse = openControlled;
        break;
      case NS_KEY_UP :
        // space key on focused button. see note at NS_MOUSE_CLICK.
        if (key == nsIDOMKeyEvent::DOM_VK_SPACE)
          abuse = openAllowed;
        else if (::PopupAllowedForEvent("keyup"))
          abuse = openControlled;
        break;
      case NS_KEY_DOWN :
        if (::PopupAllowedForEvent("keydown"))
          abuse = openControlled;
        break;
      }
    }
    break;
  case NS_MOUSE_EVENT :
    if (NS_IS_TRUSTED_EVENT(aEvent) &&
        NS_STATIC_CAST(nsMouseEvent*, aEvent)->button == nsMouseEvent::eLeftButton) {
      switch(aEvent->message) {
      case NS_MOUSE_BUTTON_UP :
        if (::PopupAllowedForEvent("mouseup"))
          abuse = openControlled;
        break;
      case NS_MOUSE_BUTTON_DOWN :
        if (::PopupAllowedForEvent("mousedown"))
          abuse = openControlled;
        break;
      case NS_MOUSE_CLICK :
        /* Click events get special treatment because of their
           historical status as a more legitimate event handler. If
           click popups are enabled in the prefs, clear the popup
           status completely. */
        if (::PopupAllowedForEvent("click"))
          abuse = openAllowed;
        break;
      case NS_MOUSE_DOUBLECLICK :
        if (::PopupAllowedForEvent("dblclick"))
          abuse = openControlled;
        break;
      }
    }
    break;
  case NS_SCRIPT_ERROR_EVENT :
    switch(aEvent->message) {
    case NS_LOAD_ERROR :
      // Any error event will allow popups, if enabled in the pref.
      if (::PopupAllowedForEvent("error"))
        abuse = openControlled;
      break;
    }
    break;
  case NS_FORM_EVENT :
    // For these following events only allow popups if they're
    // triggered while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (nsEventStateManager::IsHandlingUserInput()) {
      switch(aEvent->message) {
      case NS_FORM_SUBMIT :
        if (::PopupAllowedForEvent("submit"))
          abuse = openControlled;
        break;
      case NS_FORM_RESET :
        if (::PopupAllowedForEvent("reset"))
          abuse = openControlled;
        break;
      }
    }
    break;
  }

  return abuse;
}

// static
void
nsDOMEvent::PopupAllowedEventsChanged()
{
  if (sPopupAllowedEvents) {
    nsMemory::Free(sPopupAllowedEvents);
  }

  nsAdoptingCString str =
    nsContentUtils::GetCharPref("dom.popup_allowed_events");

  // We'll want to do this even if str is empty to avoid looking up
  // this pref all the time if it's not set.
  sPopupAllowedEvents = ToNewCString(str);
}

// static
void
nsDOMEvent::Shutdown()
{
  if (sPopupAllowedEvents) {
    nsMemory::Free(sPopupAllowedEvents);
  }
}

// To be called ONLY by nsDOMEvent::GetType (which has the additional
// logic for handling user-defined events).
// static
const char* nsDOMEvent::GetEventName(PRUint32 aEventType)
{
  switch(aEventType) {
  case NS_MOUSE_BUTTON_DOWN:
    return sEventNames[eDOMEvents_mousedown];
  case NS_MOUSE_BUTTON_UP:
    return sEventNames[eDOMEvents_mouseup];
  case NS_MOUSE_CLICK:
    return sEventNames[eDOMEvents_click];
  case NS_MOUSE_DOUBLECLICK:
    return sEventNames[eDOMEvents_dblclick];
  case NS_MOUSE_ENTER_SYNTH:
    return sEventNames[eDOMEvents_mouseover];
  case NS_MOUSE_EXIT_SYNTH:
    return sEventNames[eDOMEvents_mouseout];
  case NS_MOUSE_MOVE:
    return sEventNames[eDOMEvents_mousemove];
  case NS_KEY_UP:
    return sEventNames[eDOMEvents_keyup];
  case NS_KEY_DOWN:
    return sEventNames[eDOMEvents_keydown];
  case NS_KEY_PRESS:
    return sEventNames[eDOMEvents_keypress];
  case NS_COMPOSITION_START:
    return sEventNames[eDOMEvents_compositionstart];
  case NS_COMPOSITION_END:
    return sEventNames[eDOMEvents_compositionend];
  case NS_FOCUS_CONTENT:
    return sEventNames[eDOMEvents_focus];
  case NS_BLUR_CONTENT:
    return sEventNames[eDOMEvents_blur];
  case NS_XUL_CLOSE:
    return sEventNames[eDOMEvents_close];
  case NS_LOAD:
    return sEventNames[eDOMEvents_load];
  case NS_BEFORE_PAGE_UNLOAD:
    return sEventNames[eDOMEvents_beforeunload];
  case NS_PAGE_UNLOAD:
    return sEventNames[eDOMEvents_unload];
  case NS_IMAGE_ABORT:
    return sEventNames[eDOMEvents_abort];
  case NS_LOAD_ERROR:
    return sEventNames[eDOMEvents_error];
  case NS_FORM_SUBMIT:
    return sEventNames[eDOMEvents_submit];
  case NS_FORM_RESET:
    return sEventNames[eDOMEvents_reset];
  case NS_FORM_CHANGE:
    return sEventNames[eDOMEvents_change];
  case NS_FORM_SELECTED:
    return sEventNames[eDOMEvents_select];
  case NS_FORM_INPUT:
    return sEventNames[eDOMEvents_input];
  case NS_PAINT:
    return sEventNames[eDOMEvents_paint];
  case NS_RESIZE_EVENT:
    return sEventNames[eDOMEvents_resize];
  case NS_SCROLL_EVENT:
    return sEventNames[eDOMEvents_scroll];
  case NS_TEXT_TEXT:
    return sEventNames[eDOMEvents_text];
  case NS_XUL_POPUP_SHOWING:
    return sEventNames[eDOMEvents_popupShowing];
  case NS_XUL_POPUP_SHOWN:
    return sEventNames[eDOMEvents_popupShown];
  case NS_XUL_POPUP_HIDING:
    return sEventNames[eDOMEvents_popupHiding];
  case NS_XUL_POPUP_HIDDEN:
    return sEventNames[eDOMEvents_popupHidden];
  case NS_XUL_COMMAND:
    return sEventNames[eDOMEvents_command];
  case NS_XUL_BROADCAST:
    return sEventNames[eDOMEvents_broadcast];
  case NS_XUL_COMMAND_UPDATE:
    return sEventNames[eDOMEvents_commandupdate];
  case NS_DRAGDROP_ENTER:
    return sEventNames[eDOMEvents_dragenter];
  case NS_DRAGDROP_OVER_SYNTH:
    return sEventNames[eDOMEvents_dragover];
  case NS_DRAGDROP_EXIT_SYNTH:
    return sEventNames[eDOMEvents_dragexit];
  case NS_DRAGDROP_DRAGDROP:
    return sEventNames[eDOMEvents_dragdrop];
  case NS_DRAGDROP_GESTURE:
    return sEventNames[eDOMEvents_draggesture];
  case NS_DRAGDROP_DRAG:
    return sEventNames[eDOMEvents_drag];
  case NS_DRAGDROP_END:
    return sEventNames[eDOMEvents_dragend];
  case NS_DRAGDROP_START:
    return sEventNames[eDOMEvents_dragstart];
  case NS_DRAGDROP_LEAVE_SYNTH:
    return sEventNames[eDOMEvents_dragleave];
  case NS_DRAGDROP_DROP:
    return sEventNames[eDOMEvents_drop];
  case NS_SCROLLPORT_OVERFLOW:
    return sEventNames[eDOMEvents_overflow];
  case NS_SCROLLPORT_UNDERFLOW:
    return sEventNames[eDOMEvents_underflow];
  case NS_SCROLLPORT_OVERFLOWCHANGED:
    return sEventNames[eDOMEvents_overflowchanged];
  case NS_MUTATION_SUBTREEMODIFIED:
    return sEventNames[eDOMEvents_subtreemodified];
  case NS_MUTATION_NODEINSERTED:
    return sEventNames[eDOMEvents_nodeinserted];
  case NS_MUTATION_NODEREMOVED:
    return sEventNames[eDOMEvents_noderemoved];
  case NS_MUTATION_NODEREMOVEDFROMDOCUMENT:
    return sEventNames[eDOMEvents_noderemovedfromdocument];
  case NS_MUTATION_NODEINSERTEDINTODOCUMENT:
    return sEventNames[eDOMEvents_nodeinsertedintodocument];
  case NS_MUTATION_ATTRMODIFIED:
    return sEventNames[eDOMEvents_attrmodified];
  case NS_MUTATION_CHARACTERDATAMODIFIED:
    return sEventNames[eDOMEvents_characterdatamodified];
  case NS_CONTEXTMENU:
    return sEventNames[eDOMEvents_contextmenu];
  case NS_UI_ACTIVATE:
    return sEventNames[eDOMEvents_DOMActivate];
  case NS_UI_FOCUSIN:
    return sEventNames[eDOMEvents_DOMFocusIn];
  case NS_UI_FOCUSOUT:
    return sEventNames[eDOMEvents_DOMFocusOut];
  case NS_PAGE_SHOW:
    return sEventNames[eDOMEvents_pageshow];
  case NS_PAGE_HIDE:
    return sEventNames[eDOMEvents_pagehide];
  case NS_MOUSE_SCROLL:
    return sEventNames[eDOMEvents_DOMMouseScroll];
#ifdef MOZ_SVG
  case NS_SVG_LOAD:
    return sEventNames[eDOMEvents_SVGLoad];
  case NS_SVG_UNLOAD:
    return sEventNames[eDOMEvents_SVGUnload];
  case NS_SVG_ABORT:
    return sEventNames[eDOMEvents_SVGAbort];
  case NS_SVG_ERROR:
    return sEventNames[eDOMEvents_SVGError];
  case NS_SVG_RESIZE:
    return sEventNames[eDOMEvents_SVGResize];
  case NS_SVG_SCROLL:
    return sEventNames[eDOMEvents_SVGScroll];
  case NS_SVG_ZOOM:
    return sEventNames[eDOMEvents_SVGZoom];
#endif // MOZ_SVG
  default:
    break;
  }
  // XXXldb We can hit this case for nsEvent objects that we didn't
  // create and that are not user defined events since this function and
  // SetEventType are incomplete.  (But fixing that requires fixing the
  // arrays in nsEventListenerManager too, since the events for which
  // this is a problem generally *are* created by nsDOMEvent.)
  return nsnull;
}

nsresult NS_NewDOMEvent(nsIDOMEvent** aInstancePtrResult,
                        nsPresContext* aPresContext,
                        nsEvent *aEvent) 
{
  nsDOMEvent* it = new nsDOMEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
