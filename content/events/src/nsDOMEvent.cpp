/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsDOMEvent.h"
#include "nsIDOMNode.h"
#include "nsIEventStateManager.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIRenderingContext.h"
#include "nsIDOMRenderingContext.h"

static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMEventIID, NS_IDOMEVENT_IID);
static NS_DEFINE_IID(kIDOMNSEventIID, NS_IDOMNSEVENT_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID, NS_IPRIVATEDOMEVENT_IID);

static char* mEventNames[] = {
  "onmousedown", "onmouseup", "onclick", "ondblclick", "onmouseover",
  "onmouseout", "onmousemove", "onkeydown", "onkeyup", "onkeypress",
  "onfocus", "onblur", "onload", "onunload", "onabort", "onerror",
  "onsubmit", "onreset", "onchange", "onpaint" ,"text"
};

nsDOMEvent::nsDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent) {
  mPresContext = aPresContext;
  NS_ADDREF(mPresContext);
  mEvent = aEvent;
  mTarget = nsnull;
  mText = nsnull;

  if (aEvent->eventStructType ==NS_TEXT_EVENT) {
	  mText = new nsString(((nsTextEvent*)aEvent)->theText);
	  mCommitText = ((nsTextEvent*)aEvent)->commitText;
  }

  NS_INIT_REFCNT();
}

nsDOMEvent::~nsDOMEvent() {
  NS_RELEASE(mPresContext);
  NS_IF_RELEASE(mTarget);

  delete mText;
}

NS_IMPL_ADDREF(nsDOMEvent)
NS_IMPL_RELEASE(nsDOMEvent)

nsresult nsDOMEvent::QueryInterface(const nsIID& aIID,
                                       void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMEventIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMEvent*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNSEventIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMNSEvent*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIPrivateDOMEventIID)) {
    *aInstancePtrResult = (void*) ((nsIPrivateDOMEvent*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

// nsIDOMEventInterface
NS_METHOD nsDOMEvent::GetType(nsString& aType)
{
  const char* mName = GetEventName(mEvent->message);

  if (nsnull != mName) {
    aType = nsString(mName);
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::GetText(nsString& aText)
{
	if (mEvent->message == NS_TEXT_EVENT) {
		aText = *mText;
		return NS_OK;
	}

	return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::GetCommitText(PRBool* aCommitText)
{
	if (mEvent->message == NS_TEXT_EVENT) {
		*aCommitText = mCommitText;
		return NS_OK;
	}
	
	return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::SetCommitText(PRBool aCommitText)
{	
	return NS_ERROR_FAILURE;
}
NS_METHOD nsDOMEvent::SetType(const nsString& aType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetTarget(nsIDOMNode** aTarget)
{
  if (nsnull != mTarget) {
    *aTarget = mTarget;
    NS_ADDREF(mTarget);
    return NS_OK;
  }
  
  nsIEventStateManager *manager;
  nsIFrame *targetFrame;
  nsIContent *targetContent;

  if (NS_OK == mPresContext->GetEventStateManager(&manager)) {
    manager->GetEventTarget(&targetFrame);
    NS_RELEASE(manager);
  }
  
  if (NS_OK == targetFrame->GetContent(&targetContent) && nsnull != targetContent) {    
    if (NS_OK == targetContent->QueryInterface(kIDOMNodeIID, (void**)&mTarget)) {
      *aTarget = mTarget;
      NS_ADDREF(mTarget);
    }
    NS_RELEASE(targetContent);
  }

  return NS_OK;
}

NS_METHOD nsDOMEvent::SetTarget(nsIDOMNode* aTarget)
{
  if (mTarget != aTarget) {
    NS_IF_RELEASE(mTarget);
    NS_IF_ADDREF(aTarget);
    mTarget = aTarget;
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetScreenX(PRInt32* aScreenX)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::SetScreenX(PRInt32 aScreenX)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetScreenY(PRInt32* aScreenY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::SetScreenY(PRInt32 aScreenY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetClientX(PRInt32* aClientX)
{
  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  *aClientX = NSTwipsToIntPixels(mEvent->point.x, t2p);
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetClientX(PRInt32 aClientX)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetClientY(PRInt32* aClientY)
{
  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  *aClientY = NSTwipsToIntPixels(mEvent->point.y, t2p);
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetClientY(PRInt32 aClientY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetAltKey(PRBool* aIsDown)
{
  *aIsDown = ((nsInputEvent*)mEvent)->isAlt;
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetAltKey(PRBool aAltKey)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetCtrlKey(PRBool* aIsDown)
{
  *aIsDown = ((nsInputEvent*)mEvent)->isControl;
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetCtrlKey(PRBool aCtrlKey)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetShiftKey(PRBool* aIsDown)
{
  *aIsDown = ((nsInputEvent*)mEvent)->isShift;
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetShiftKey(PRBool aShiftKey)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetMetaKey(PRBool* aIsDown)
{
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetMetaKey(PRBool aMetaKey)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetCharCode(PRUint32* aCharCode)
{
  switch (mEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_DOWN:
    *aCharCode = ((nsKeyEvent*)mEvent)->charCode;
#ifdef NS_DEBUG
    if (0==*aCharCode)
      printf("key event broken, GetCharChode returning 0x0 as the char code.\n");
#endif
    break;
  default:
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetCharCode(PRUint32 aCharCode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetKeyCode(PRUint32* aKeyCode)
{
  switch (mEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_DOWN:
    *aKeyCode = ((nsKeyEvent*)mEvent)->keyCode;
    break;
  default:
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetKeyCode(PRUint32 aKeyCode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetButton(PRUint32* aButton)
{
  switch (mEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
  case NS_MOUSE_LEFT_BUTTON_DOWN:
  case NS_MOUSE_LEFT_CLICK:
  case NS_MOUSE_LEFT_DOUBLECLICK:
    *aButton = 1;
    break;
  case NS_MOUSE_MIDDLE_BUTTON_UP:
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
  case NS_MOUSE_MIDDLE_CLICK:
  case NS_MOUSE_MIDDLE_DOUBLECLICK:
    *aButton = 2;
    break;
  case NS_MOUSE_RIGHT_BUTTON_UP:
  case NS_MOUSE_RIGHT_BUTTON_DOWN:
  case NS_MOUSE_RIGHT_CLICK:
  case NS_MOUSE_RIGHT_DOUBLECLICK:
    *aButton = 3;
    break;
  default:
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetButton(PRUint32 aButton)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetCancelBubble(PRBool* aCancelBubble)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::SetCancelBubble(PRBool aCancelBubble)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsINSEventInterface
NS_METHOD nsDOMEvent::GetLayerX(PRInt32* aLayerX)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::SetLayerX(PRInt32 aLayerX)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetLayerY(PRInt32* aLayerY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::SetLayerY(PRInt32 aLayerY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetPageX(PRInt32* aPageX)
{
  return GetClientX(aPageX);
}

NS_METHOD nsDOMEvent::SetPageX(PRInt32 aPageX)
{
  return SetClientX(aPageX);
}

NS_METHOD nsDOMEvent::GetPageY(PRInt32* aPageY)
{
  return GetClientY(aPageY);
}

NS_METHOD nsDOMEvent::SetPageY(PRInt32 aPageY)
{
  return SetClientY(aPageY);
}

NS_METHOD nsDOMEvent::GetWhich(PRUint32* aWhich)
{
  switch (mEvent->eventStructType) {
  case NS_KEY_EVENT:
    return GetKeyCode(aWhich);
  case NS_MOUSE_EVENT:
    return GetButton(aWhich);
  }
  return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::SetWhich(PRUint32 aWhich)
{
  switch (mEvent->eventStructType) {
  case NS_KEY_EVENT:
    return SetKeyCode(aWhich);
  case NS_MOUSE_EVENT:
    return SetButton(aWhich);
  }
  return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::DuplicatePrivateData()
{
  //XXX Write me!

  //XXX And when you do, make sure to copy over the event target here, too!
  return NS_OK;
}

const char* nsDOMEvent::GetEventName(PRUint32 aEventType)
{
  switch(aEventType) {
  case NS_MOUSE_LEFT_BUTTON_DOWN:
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
  case NS_MOUSE_RIGHT_BUTTON_DOWN:
    return mEventNames[eDOMEvents_mousedown];
  case NS_MOUSE_LEFT_BUTTON_UP:
  case NS_MOUSE_MIDDLE_BUTTON_UP:
  case NS_MOUSE_RIGHT_BUTTON_UP:
    return mEventNames[eDOMEvents_mouseup];
  case NS_MOUSE_LEFT_CLICK:
  case NS_MOUSE_MIDDLE_CLICK:
  case NS_MOUSE_RIGHT_CLICK:
    return mEventNames[eDOMEvents_click];
  case NS_MOUSE_LEFT_DOUBLECLICK:
  case NS_MOUSE_MIDDLE_DOUBLECLICK:
  case NS_MOUSE_RIGHT_DOUBLECLICK:
    return mEventNames[eDOMEvents_dblclick];
  case NS_MOUSE_ENTER:
    return mEventNames[eDOMEvents_mouseover];
  case NS_MOUSE_EXIT:
    return mEventNames[eDOMEvents_mouseout];
  case NS_MOUSE_MOVE:
    return mEventNames[eDOMEvents_mousemove];
  case NS_KEY_UP:
    return mEventNames[eDOMEvents_keyup];
  case NS_KEY_DOWN:
    return mEventNames[eDOMEvents_keydown];
  case NS_KEY_PRESS:
    return mEventNames[eDOMEvents_keypress];
  case NS_GOTFOCUS:
    return mEventNames[eDOMEvents_focus];
  case NS_LOSTFOCUS:
    return mEventNames[eDOMEvents_blur];
  case NS_PAGE_LOAD:
  case NS_IMAGE_LOAD:
    return mEventNames[eDOMEvents_load];
  case NS_PAGE_UNLOAD:
    return mEventNames[eDOMEvents_unload];
  case NS_IMAGE_ABORT:
    return mEventNames[eDOMEvents_abort];
  case NS_IMAGE_ERROR:
    return mEventNames[eDOMEvents_error];
  case NS_FORM_SUBMIT:
    return mEventNames[eDOMEvents_submit];
  case NS_FORM_RESET:
    return mEventNames[eDOMEvents_reset];
  case NS_FORM_CHANGE:
    return mEventNames[eDOMEvents_change];
  case NS_PAINT:
    return mEventNames[eDOMEvents_paint];
  case NS_TEXT_EVENT:
	return mEventNames[eDOMEvents_text];
  default:
    break;
  }
  return nsnull;
}

static NS_DEFINE_IID(kIDOMRenderingContextIID, NS_IDOMRENDERINGCONTEXT_IID);

NS_IMETHODIMP
nsDOMEvent::GetRc(nsIDOMRenderingContext** aRc)
{
  *aRc = nsnull;
  if (NS_PAINT_EVENT == mEvent->eventStructType) {
    nsPaintEvent* pe = (nsPaintEvent*) mEvent;
    if (nsnull != pe->renderingContext) {
      nsIDOMRenderingContext* domrc;
      if (NS_OK == pe->renderingContext->QueryInterface(kIDOMRenderingContextIID, (void**) &domrc)) {
        *aRc = domrc;
      }
    }
  }
  return NS_OK;
}

nsresult NS_NewDOMEvent(nsIDOMEvent** aInstancePtrResult, nsIPresContext& aPresContext, nsEvent *aEvent) 
{
  nsDOMEvent* it = new nsDOMEvent(&aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return it->QueryInterface(kIDOMEventIID, (void **) aInstancePtrResult);
}
