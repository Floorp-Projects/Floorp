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
#include "nsIWidget.h"
#include "nsIWebShell.h"
#include "nsIPresShell.h"
#include "nsPrivateTextRange.h"
#include "nsIDocument.h"
#include "nsIPrivateCompositionEvent.h"

static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMEventIID, NS_IDOMEVENT_IID);
static NS_DEFINE_IID(kIDOMUIEventIID, NS_IDOMUIEVENT_IID);
static NS_DEFINE_IID(kIDOMNSUIEventIID, NS_IDOMNSUIEVENT_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID, NS_IPRIVATEDOMEVENT_IID);
static NS_DEFINE_IID(kIPrivateTextEventIID, NS_IPRIVATETEXTEVENT_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIPrivateCompositionEventIID,NS_IPRIVATECOMPOSITIONEVENT_IID);

static char* mEventNames[] = {
  "mousedown", "mouseup", "click", "dblclick", "mouseover",
  "mouseout", "mousemove", "keydown", "keyup", "keypress",
  "focus", "blur", "load", "unload", "abort", "error",
  "submit", "reset", "change", "select", "paint" ,"text",
  "create", "destroy", "command", "dragenter", "dragover", "dragexit",
  "dragdrop", "draggesture"
}; 

nsDOMEvent::nsDOMEvent(nsIPresContext* aPresContext, nsEvent* aEvent) {
  mPresContext = aPresContext;
  NS_ADDREF(mPresContext);
  mEvent = aEvent;
  mTarget = nsnull;
  mText = nsnull;
  mTextRange = nsnull;

  if (aEvent->eventStructType ==NS_TEXT_EVENT) {
	  //
	  // extract the IME composition string
	  //
	  mText = new nsString(((nsTextEvent*)aEvent)->theText);
	  //
	  // build the range list -- ranges need to be DOM-ified since the IME transaction
	  //  will hold a ref, the widget representation isn't persistent
	  //
	  nsIPrivateTextRange** tempTextRangeList = new nsIPrivateTextRange*[((nsTextEvent*)aEvent)->rangeCount];
	  if (tempTextRangeList!=nsnull) {
		for(PRUint16 i=0;i<((nsTextEvent*)aEvent)->rangeCount;i++) {
			nsPrivateTextRange* tempPrivateTextRange = new nsPrivateTextRange((((nsTextEvent*)aEvent)->rangeArray[i]).mStartOffset,
														(((nsTextEvent*)aEvent)->rangeArray[i]).mEndOffset,
														(((nsTextEvent*)aEvent)->rangeArray[i]).mRangeType);
			if (tempPrivateTextRange!=nsnull) {
				tempPrivateTextRange->AddRef();
				tempTextRangeList[i] = (nsIPrivateTextRange*)tempPrivateTextRange;
			}
		}
		
		mTextRange = (nsIPrivateTextRangeList*) new nsPrivateTextRangeList(((nsTextEvent*)aEvent)->rangeCount,tempTextRangeList);
		if (mTextRange!=nsnull)  mTextRange->AddRef();
	  }
  }

  NS_INIT_REFCNT();
}

nsDOMEvent::~nsDOMEvent() {
  NS_RELEASE(mPresContext);
  NS_IF_RELEASE(mTarget);
  NS_IF_RELEASE(mTextRange);

  if (mText!=nsnull)
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
  if (aIID.Equals(kIDOMUIEventIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMUIEvent*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNSUIEventIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMNSUIEvent*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIPrivateDOMEventIID)) {
    *aInstancePtrResult = (void*) ((nsIPrivateDOMEvent*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIPrivateTextEventIID)) {
	*aInstancePtrResult=(void*)((nsIPrivateTextEvent*)this);
	AddRef();
	return NS_OK;
  }
  if (aIID.Equals(kIPrivateCompositionEventIID)) {
    *aInstancePtrResult = (void*)((nsIPrivateCompositionEvent*)this);
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
    aType = mName;
    return NS_OK;
  }
  
  return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::GetTarget(nsIDOMNode** aTarget)
{
  if (nsnull != mTarget) {
    *aTarget = mTarget;
    NS_ADDREF(mTarget);
    return NS_OK;
  }
  
  nsIEventStateManager *manager;
  nsIContent *targetContent;

  if (NS_OK == mPresContext->GetEventStateManager(&manager)) {
    manager->GetEventTargetContent(&targetContent);
    NS_RELEASE(manager);
  }
  
  if (targetContent) {    
    if (NS_OK == targetContent->QueryInterface(kIDOMNodeIID, (void**)&mTarget)) {
      *aTarget = mTarget;
      NS_ADDREF(mTarget);
    }
    NS_RELEASE(targetContent);
  }
  else {
    //Always want a target.  Use document if nothing else.
    nsIPresShell* presShell;
    nsIDocument* doc;
    if (NS_SUCCEEDED(mPresContext->GetShell(&presShell))) {
      presShell->GetDocument(&doc);
      NS_RELEASE(presShell);
    }

    if (doc) {
      if (NS_OK == doc->QueryInterface(kIDOMNodeIID, (void**)&mTarget)) {
        *aTarget = mTarget;
        NS_ADDREF(mTarget);
      }      
      NS_RELEASE(doc);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetCurrentNode(nsIDOMNode** aCurrentNode)
{
  *aCurrentNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetEventPhase(PRUint16* aEventPhase)
{
  if (mEvent->flags & NS_EVENT_FLAG_CAPTURE) {
    *aEventPhase = CAPTURING_PHASE;
  }
  else if (mEvent->flags & NS_EVENT_FLAG_BUBBLE) {
    *aEventPhase = BUBBLING_PHASE;
  }
  else if (mEvent->flags & NS_EVENT_FLAG_INIT) {
    *aEventPhase = AT_TARGET;
  }
  else {
    *aEventPhase = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::PreventBubble()
{
  if (mEvent->flags & NS_EVENT_FLAG_BUBBLE || mEvent->flags & NS_EVENT_FLAG_INIT) {
    mEvent->flags |= NS_EVENT_FLAG_STOP_DISPATCH;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::PreventCapture()
{
  if (mEvent->flags & NS_EVENT_FLAG_CAPTURE) {
    mEvent->flags |= NS_EVENT_FLAG_STOP_DISPATCH;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::PreventDefault()
{
  mEvent->flags |= NS_EVENT_FLAG_NO_DEFAULT;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetText(nsString& aText)
{
	if (mEvent->message == NS_TEXT_EVENT) {
		aText = *mText;
		return NS_OK;
	}

	return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::GetInputRange(nsIPrivateTextRangeList** aInputRange)
{
	if (mEvent->message == NS_TEXT_EVENT) {
		*aInputRange = mTextRange;
		return NS_OK;
	}

	return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::GetEventReply(nsTextEventReply** aReply)
{
	if (mEvent->message==NS_TEXT_EVENT) {
			*aReply = &(((nsTextEvent*)mEvent)->theReply);
			return NS_OK;
	}

	return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::GetCompositionReply(nsTextEventReply** aReply)
{
  if (mEvent->message==NS_COMPOSITION_START) {
    *aReply = &(((nsCompositionEvent*)mEvent)->theReply);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;

}

NS_METHOD nsDOMEvent::GetScreenX(PRInt32* aScreenX)
{
  // pinkerton -- i don't understand how we can assume that mEvent
  // is a nsGUIEvent, but we are.
  if ( !mEvent || !((nsGUIEvent*)mEvent)->widget )
    return NS_ERROR_FAILURE;
    
  nsRect bounds, offset;
  bounds.x = mEvent->refPoint.x;
  
  ((nsGUIEvent*)mEvent)->widget->WidgetToScreen ( bounds, offset );
  *aScreenX = offset.x;
  
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetScreenY(PRInt32* aScreenY)
{
  // pinkerton -- i don't understand how we can assume that mEvent
  // is a nsGUIEvent, but we are.
  if ( !mEvent || !((nsGUIEvent*)mEvent)->widget )
    return NS_ERROR_FAILURE;

  nsRect bounds, offset;
  bounds.y = mEvent->refPoint.y;
  
  ((nsGUIEvent*)mEvent)->widget->WidgetToScreen ( bounds, offset );
  *aScreenY = offset.y;
  
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetClientX(PRInt32* aClientX)
{
  //My god, man, there *must* be a better way to do this.
  nsIPresShell* shell;
  nsIWidget* rootWidget = nsnull;
  mPresContext->GetShell(&shell);

  if (shell) {
    nsIViewManager* vm;
    shell->GetViewManager(&vm);
    if (vm) {
      nsIView* rootView = nsnull;
      vm->GetRootView(rootView);
      if (rootView) {
        rootView->GetWidget(rootWidget);
      }
      NS_RELEASE(vm);
    }
    NS_RELEASE(shell);
  }


  nsRect bounds, offset;
  offset.x = 0;

  nsIWidget* parent = ((nsGUIEvent*)mEvent)->widget;
  //Add extra ref since loop will free one.
  NS_ADDREF(parent);
  nsIWidget* tmp;
  while (rootWidget != parent) {
    parent->GetBounds(bounds);
    offset.x += bounds.x;
    tmp = parent;
    parent = tmp->GetParent();
    NS_RELEASE(tmp);
  }
  NS_IF_RELEASE(parent);
  NS_IF_RELEASE(rootWidget);
  
  *aClientX = mEvent->refPoint.x + offset.x;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetClientY(PRInt32* aClientY)
{
  //My god, man, there *must* be a better way to do this.
  nsIPresShell* shell;
  nsIWidget* rootWidget = nsnull;
  mPresContext->GetShell(&shell);

  if (shell) {
    nsIViewManager* vm;
    shell->GetViewManager(&vm);
    if (vm) {
      nsIView* rootView = nsnull;
      vm->GetRootView(rootView);
      if (rootView) {
        rootView->GetWidget(rootWidget);
      }
      NS_RELEASE(vm);
    }
    NS_RELEASE(shell);
  }


  nsRect bounds, offset;
  offset.y = 0;

  nsIWidget* parent = ((nsGUIEvent*)mEvent)->widget;
  //Add extra ref since loop will free one.
  NS_ADDREF(parent);
  nsIWidget* tmp;
  while (rootWidget != parent) {
    parent->GetBounds(bounds);
    offset.y += bounds.y;
    tmp = parent;
    parent = tmp->GetParent();
    NS_RELEASE(tmp);
  }
  NS_IF_RELEASE(parent);
  NS_IF_RELEASE(rootWidget);
  
  *aClientY = mEvent->refPoint.y + offset.y;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetAltKey(PRBool* aIsDown)
{
  *aIsDown = ((nsInputEvent*)mEvent)->isAlt;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetCtrlKey(PRBool* aIsDown)
{
  *aIsDown = ((nsInputEvent*)mEvent)->isControl;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetShiftKey(PRBool* aIsDown)
{
  *aIsDown = ((nsInputEvent*)mEvent)->isShift;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetMetaKey(PRBool* aIsDown)
{
  #ifdef XP_MAC
  *aIsDown = ((nsInputEvent*)mEvent)->isCommand;
  #else
  *aIsDown = ((nsInputEvent*)mEvent)->isControl;
  #endif
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetCharCode(PRUint32* aCharCode)
{
  switch (mEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_DOWN:
#ifdef NS_DEBUG
    printf("GetCharCode used for wrong key event; should use onkeypress.\n");
#endif
    *aCharCode = 0;
    break;
  case NS_KEY_PRESS:
    *aCharCode = ((nsKeyEvent*)mEvent)->charCode;
#if defined(NS_DEBUG) && defined(DEBUG_buster)
    if (0==*aCharCode)
      printf("GetCharCode used correctly but no valid key!\n");
#endif
    break;
  default:
    break;
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetKeyCode(PRUint32* aKeyCode)
{
  switch (mEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_PRESS:
  case NS_KEY_DOWN:
    *aKeyCode = ((nsKeyEvent*)mEvent)->keyCode;
    break;
  default:
    break;
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetButton(PRUint16* aButton)
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
    break;
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetClickcount(PRUint16* aClickcount) 
{
  //XXX implement me.
  *aClickcount = 1;
  return NS_OK;
}

// nsINSEventInterface
NS_METHOD nsDOMEvent::GetLayerX(PRInt32* aLayerX)
{
  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  *aLayerX = NSTwipsToIntPixels(mEvent->point.x, t2p);
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetLayerY(PRInt32* aLayerY)
{
  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  *aLayerY = NSTwipsToIntPixels(mEvent->point.y, t2p);
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetPageX(PRInt32* aPageX)
{
  return GetClientX(aPageX);
}

NS_METHOD nsDOMEvent::GetPageY(PRInt32* aPageY)
{
  return GetClientY(aPageY);
}

NS_METHOD nsDOMEvent::GetWhich(PRUint32* aWhich)
{
  switch (mEvent->eventStructType) {
  case NS_KEY_EVENT:
    return GetKeyCode(aWhich);
  case NS_MOUSE_EVENT:
    {
      PRUint16 button;
      nsresult ret = GetButton(&button);
      *aWhich = button;
    }
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetRangeParent(nsIDOMNode** aRangeParent)
{
  nsIFrame* targetFrame;
  nsIEventStateManager* manager;

  if (NS_OK == mPresContext->GetEventStateManager(&manager)) {
    manager->GetEventTarget(&targetFrame);
    NS_RELEASE(manager);
  }

  if (targetFrame) {
    nsIContent* parent = nsnull;
    PRInt32 offset, endOffset;

    if (NS_SUCCEEDED(targetFrame->GetPosition(*mPresContext, 
                                              mEvent->point.x,
                                              &parent,
                                              offset,
                                              endOffset))) {
      if (parent && NS_SUCCEEDED(parent->QueryInterface(kIDOMNodeIID, (void**)aRangeParent))) {
        NS_RELEASE(parent);
        return NS_OK;
      }
      NS_IF_RELEASE(parent);
    }
  }
  return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::GetRangeOffset(PRInt32* aRangeOffset)
{
  nsIFrame* targetFrame;
  nsIEventStateManager* manager;

  if (NS_OK == mPresContext->GetEventStateManager(&manager)) {
    manager->GetEventTarget(&targetFrame);
    NS_RELEASE(manager);
  }

  if (targetFrame) {
    nsIContent* parent = nsnull;
    PRInt32 endOffset;

    if (NS_SUCCEEDED(targetFrame->GetPosition(*mPresContext, 
                                              mEvent->point.x,
                                              &parent,
                                              *aRangeOffset,
                                              endOffset))) {
      NS_IF_RELEASE(parent);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMEvent::DuplicatePrivateData()
{
  //XXX Write me!

  //XXX And when you do, make sure to copy over the event target here, too!
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
  case NS_FOCUS_CONTENT:
    return mEventNames[eDOMEvents_focus];
  case NS_BLUR_CONTENT:
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
  case NS_FORM_SELECTED:
    return mEventNames[eDOMEvents_select];
  case NS_PAINT:
    return mEventNames[eDOMEvents_paint];
  case NS_TEXT_EVENT:
	  return mEventNames[eDOMEvents_text];
  case NS_MENU_CREATE:
    return mEventNames[eDOMEvents_create];
  case NS_MENU_DESTROY:
    return mEventNames[eDOMEvents_destroy];
  case NS_MENU_ACTION:
    return mEventNames[eDOMEvents_action];
  case NS_DRAGDROP_ENTER:
    return mEventNames[eDOMEvents_dragenter];
  case NS_DRAGDROP_OVER:
    return mEventNames[eDOMEvents_dragover];
  case NS_DRAGDROP_EXIT:
    return mEventNames[eDOMEvents_dragexit];
  case NS_DRAGDROP_DROP:
    return mEventNames[eDOMEvents_dragdrop];
  case NS_DRAGDROP_GESTURE:
    return mEventNames[eDOMEvents_draggesture];
  default:
    break;
  }
  return nsnull;
}

nsresult NS_NewDOMUIEvent(nsIDOMEvent** aInstancePtrResult, nsIPresContext& aPresContext, nsEvent *aEvent) 
{
  nsDOMEvent* it = new nsDOMEvent(&aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return it->QueryInterface(kIDOMEventIID, (void **) aInstancePtrResult);
}

nsresult NS_NewDOMEvent(nsIDOMEvent** aInstancePtrResult, nsIPresContext& aPresContext, nsEvent *aEvent) 
{
  return NS_ERROR_FAILURE;
}
