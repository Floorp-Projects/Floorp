/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsDOMEvent.h"
#include "nsIDOMNode.h"
#include "nsIEventStateManager.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIRenderingContext.h"
#include "nsIWidget.h"
#include "nsIPresShell.h"
#include "nsPrivateTextRange.h"
#include "nsIDocument.h"
#include "nsIViewManager.h"
#include "nsIPrivateCompositionEvent.h"

static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);

static char* mEventNames[] = {
  "mousedown", "mouseup", "click", "dblclick", "mouseover",
  "mouseout", "mousemove", "keydown", "keyup", "keypress",
  "focus", "blur", "load", "unload", "abort", "error",
  "submit", "reset", "change", "select", "input", "paint" ,"text",
  "create", "close", "destroy", "command", "broadcast", "commandupdate",
  "dragenter", "dragover", "dragexit", "dragdrop", "draggesture"
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

NS_INTERFACE_MAP_BEGIN(nsDOMEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEvent, nsIDOMMouseEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMUIEvent, nsIDOMMouseEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSUIEvent)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateDOMEvent)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateTextEvent)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateCompositionEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMouseEvent)
NS_INTERFACE_MAP_END

// nsIDOMEventInterface
NS_METHOD nsDOMEvent::GetType(nsString& aType)
{
  const char* mName = GetEventName(mEvent->message);

  if (nsnull != mName) {
    aType.AssignWithConversion(mName);
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
    manager->GetEventTargetContent(mEvent, &targetContent);
    NS_RELEASE(manager);
  }
  
  if (targetContent) {    
    if (NS_OK == targetContent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)&mTarget)) {
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
      if (NS_OK == doc->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)&mTarget)) {
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
    *aEventPhase = nsIDOMMouseEvent::CAPTURING_PHASE;
  }
  else if (mEvent->flags & NS_EVENT_FLAG_BUBBLE) {
    *aEventPhase = nsIDOMMouseEvent::BUBBLING_PHASE;
  }
  else if (mEvent->flags & NS_EVENT_FLAG_INIT) {
    *aEventPhase = nsIDOMMouseEvent::AT_TARGET;
  }
  else {
    *aEventPhase = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetBubbles(PRBool* aBubbles)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMEvent::GetCancelable(PRBool* aCancelable)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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

NS_IMETHODIMP
nsDOMEvent::GetView(nsIDOMAbstractView** aView)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMEvent::GetDetail(PRInt32* aDetail)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
  if((mEvent->message==NS_COMPOSITION_START) ||
     (mEvent->message==NS_COMPOSITION_QUERY)) {
    *aReply = &(((nsCompositionEvent*)mEvent)->theReply);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;

}

NS_METHOD nsDOMEvent::GetScreenX(PRInt32* aScreenX)
{
  if (!mEvent || 
       (mEvent->eventStructType != NS_MOUSE_EVENT && mEvent->eventStructType != NS_DRAGDROP_EVENT) ) {
    *aScreenX = 0;
    return NS_OK;
  }

  if (!((nsGUIEvent*)mEvent)->widget )
    return NS_ERROR_FAILURE;
    
  nsRect bounds, offset;
  bounds.x = mEvent->refPoint.x;
  
  ((nsGUIEvent*)mEvent)->widget->WidgetToScreen ( bounds, offset );
  *aScreenX = offset.x;
  
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetScreenY(PRInt32* aScreenY)
{
  if (!mEvent || 
        (mEvent->eventStructType != NS_MOUSE_EVENT && mEvent->eventStructType != NS_DRAGDROP_EVENT) ) {
    *aScreenY = 0;
    return NS_OK;
  }

  if (!((nsGUIEvent*)mEvent)->widget )
    return NS_ERROR_FAILURE;

  nsRect bounds, offset;
  bounds.y = mEvent->refPoint.y;
  
  ((nsGUIEvent*)mEvent)->widget->WidgetToScreen ( bounds, offset );
  *aScreenY = offset.y;
  
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetClientX(PRInt32* aClientX)
{
  if (!mEvent || 
       (mEvent->eventStructType != NS_MOUSE_EVENT && mEvent->eventStructType != NS_DRAGDROP_EVENT) ) {
    *aClientX = 0;
    return NS_OK;
  }

  //My god, man, there *must* be a better way to do this.
  nsIPresShell* shell;
  nsIWidget* rootWidget = nsnull;
  mPresContext->GetShell(&shell);

  if (shell) {
    nsIViewManager* vm;
    shell->GetViewManager(&vm);
    if (vm) {
      vm->GetWidget(&rootWidget);
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
  while (rootWidget != parent && nsnull != parent) {
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
  if (!mEvent || 
       (mEvent->eventStructType != NS_MOUSE_EVENT && mEvent->eventStructType != NS_DRAGDROP_EVENT) ) {
    *aClientY = 0;
    return NS_OK;
  }

  //My god, man, there *must* be a better way to do this.
  nsIPresShell* shell;
  nsIWidget* rootWidget = nsnull;
  mPresContext->GetShell(&shell);

  if (shell) {
    nsIViewManager* vm;
    shell->GetViewManager(&vm);
    if (vm) {
      vm->GetWidget(&rootWidget);
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
  while (rootWidget != parent && nsnull != parent) {
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
  *aIsDown = ((nsInputEvent*)mEvent)->isMeta;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetCharCode(PRUint32* aCharCode)
{
  if (!mEvent || mEvent->eventStructType != NS_KEY_EVENT) {
    *aCharCode = 0;
    return NS_OK;
  }

  switch (mEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_DOWN:
#if defined(NS_DEBUG) && defined(DEBUG_brade)
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
  if (!mEvent || mEvent->eventStructType != NS_KEY_EVENT) {
    *aKeyCode = 0;
    return NS_OK;
  }

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
  if (!mEvent || mEvent->eventStructType != NS_MOUSE_EVENT) {
    *aButton = 0;
    return NS_OK;
  }

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

NS_METHOD nsDOMEvent::GetClickCount(PRUint16* aClickCount) 
{
  if (!mEvent || mEvent->eventStructType != NS_MOUSE_EVENT) {
    *aClickCount = 0;
    return NS_OK;
  }

  switch (mEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
  case NS_MOUSE_LEFT_BUTTON_DOWN:
  case NS_MOUSE_LEFT_CLICK:
  case NS_MOUSE_LEFT_DOUBLECLICK:
  case NS_MOUSE_MIDDLE_BUTTON_UP:
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
  case NS_MOUSE_MIDDLE_CLICK:
  case NS_MOUSE_MIDDLE_DOUBLECLICK:
  case NS_MOUSE_RIGHT_BUTTON_UP:
  case NS_MOUSE_RIGHT_BUTTON_DOWN:
  case NS_MOUSE_RIGHT_CLICK:
  case NS_MOUSE_RIGHT_DOUBLECLICK:
    *aClickCount = ((nsMouseEvent*)mEvent)->clickCount;
    break;
  default:
    break;
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetRelatedNode(nsIDOMNode** aRelatedNode)
{
  nsIEventStateManager *manager;
  nsIContent *relatedContent = nsnull;
  nsresult ret = NS_OK;

  if (NS_OK == mPresContext->GetEventStateManager(&manager)) {
    manager->GetEventRelatedContent(&relatedContent);
    NS_RELEASE(manager);
  }
  
  if (relatedContent) {    
    ret = relatedContent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aRelatedNode);
    NS_RELEASE(relatedContent);
  }
  else {
    *aRelatedNode = nsnull;
  }

  return ret;
}

// nsINSEventInterface
NS_METHOD nsDOMEvent::GetLayerX(PRInt32* aLayerX)
{
  if (!mEvent || mEvent->eventStructType != NS_MOUSE_EVENT) {
    *aLayerX = 0;
    return NS_OK;
  }

  float t2p;
  mPresContext->GetTwipsToPixels(&t2p);
  *aLayerX = NSTwipsToIntPixels(mEvent->point.x, t2p);
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetLayerY(PRInt32* aLayerY)
{
  if (!mEvent || mEvent->eventStructType != NS_MOUSE_EVENT) {
    *aLayerY = 0;
    return NS_OK;
  }

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
    (void) GetButton(&button);
    *aWhich = button;
    break;
    }
  default:
    *aWhich = 0;
    break;
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
    PRBool beginOfContent;
    if (NS_SUCCEEDED(targetFrame->GetContentAndOffsetsFromPoint(mPresContext, 
                                              mEvent->point,
                                              &parent,
                                              offset,
                                              endOffset,
                                              beginOfContent))) {
      if (parent && NS_SUCCEEDED(parent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aRangeParent))) {
        NS_RELEASE(parent);
        return NS_OK;
      }
      NS_IF_RELEASE(parent);
    }
  }
  *aRangeParent = nsnull;
  return NS_OK;
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
    PRBool beginOfContent;
    if (NS_SUCCEEDED(targetFrame->GetContentAndOffsetsFromPoint(mPresContext, 
                                              mEvent->point,
                                              &parent,
                                              *aRangeOffset,
                                              endOffset,
                                              beginOfContent))) {
      NS_IF_RELEASE(parent);
      return NS_OK;
    }
  }
  *aRangeOffset = 0;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetCancelBubble(PRBool* aCancelBubble)
{
  if (mEvent->flags & NS_EVENT_FLAG_BUBBLE || mEvent->flags & NS_EVENT_FLAG_INIT) {
    *aCancelBubble = (mEvent->flags &= NS_EVENT_FLAG_STOP_DISPATCH) ? PR_TRUE : PR_FALSE;
  }
  else {
    *aCancelBubble = PR_FALSE;
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetCancelBubble(PRBool aCancelBubble)
{
  if (mEvent->flags & NS_EVENT_FLAG_BUBBLE || mEvent->flags & NS_EVENT_FLAG_INIT) {
    if (aCancelBubble) {
      mEvent->flags |= NS_EVENT_FLAG_STOP_DISPATCH;
    }
    else {
      mEvent->flags &= ~NS_EVENT_FLAG_STOP_DISPATCH;
    }
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetIsChar(PRBool* aIsChar)
{
  if (!mEvent) {
    *aIsChar = PR_FALSE;
    return NS_OK;
  }
  if (mEvent->eventStructType == NS_KEY_EVENT) {
    *aIsChar = ((nsKeyEvent*)mEvent)->isChar;
    return NS_OK;
  }
  if (mEvent->eventStructType == NS_TEXT_EVENT) {
    *aIsChar = ((nsTextEvent*)mEvent)->isChar;
    return NS_OK;
  }

  *aIsChar = PR_FALSE;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetPreventDefault(PRBool* aReturn)
{
  if (!mEvent) {
    *aReturn = PR_FALSE; 
  }
  else {
    *aReturn = (mEvent->flags & NS_EVENT_FLAG_NO_DEFAULT) ? PR_TRUE : PR_FALSE;
  }

  return NS_OK;
}

//XXX The following four methods are for custom event dispatch inside the DOM.
//They will be implemented post-beta
NS_IMETHODIMP
nsDOMEvent::InitEvent(const nsString& aEventTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMEvent::InitUIEvent(const nsString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, nsIDOMAbstractView* aViewArg, PRInt32 aDetailArg)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMEvent::InitMouseEvent(const nsString& aTypeArg, PRBool aCtrlKeyArg, PRBool aAltKeyArg, PRBool aShiftKeyArg, PRBool aMetaKeyArg, PRInt32 aScreenXArg, PRInt32 aScreenYArg, PRInt32 aClientXArg, PRInt32 aClientYArg, PRUint16 aButtonArg, PRUint16 aDetailArg)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMEvent::InitKeyEvent(const nsString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, PRBool aCtrlKeyArg, PRBool aAltKeyArg, PRBool aShiftKeyArg, PRBool aMetaKeyArg, PRUint32 aKeyCodeArg, PRUint32 aCharCodeArg, nsIDOMAbstractView* aViewArg)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
  case NS_XUL_CLOSE:
    return mEventNames[eDOMEvents_close];
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
  case NS_FORM_INPUT:
    return mEventNames[eDOMEvents_input];
  case NS_PAINT:
    return mEventNames[eDOMEvents_paint];
  case NS_TEXT_EVENT:
	  return mEventNames[eDOMEvents_text];
  case NS_MENU_CREATE:
    return mEventNames[eDOMEvents_create];
  case NS_MENU_DESTROY:
    return mEventNames[eDOMEvents_destroy];
  case NS_MENU_ACTION:
    return mEventNames[eDOMEvents_command];
  case NS_XUL_BROADCAST:
    return mEventNames[eDOMEvents_broadcast];
  case NS_XUL_COMMAND_UPDATE:
    return mEventNames[eDOMEvents_commandupdate];
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

nsresult NS_NewDOMUIEvent(nsIDOMEvent** aInstancePtrResult, nsIPresContext* aPresContext, nsEvent *aEvent) 
{
  nsDOMEvent* it = new nsDOMEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return it->QueryInterface(NS_GET_IID(nsIDOMEvent), (void **) aInstancePtrResult);
}

nsresult NS_NewDOMEvent(nsIDOMEvent** aInstancePtrResult, nsIPresContext* aPresContext, nsEvent *aEvent) 
{
  return NS_ERROR_FAILURE;
}
