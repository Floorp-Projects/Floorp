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

#include "nsISupports.h"
#include "nsIEventStateManager.h"
#include "nsEventStateManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsIWidget.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsDOMEvent.h"
#include "nsHTMLAtoms.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLTextAreaElement.h"

static NS_DEFINE_IID(kIEventStateManagerIID, NS_IEVENTSTATEMANAGER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMHTMLAnchorElementIID, NS_IDOMHTMLANCHORELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLTextAreaElementIID, NS_IDOMHTMLTEXTAREAELEMENT_IID);

nsEventStateManager::nsEventStateManager() {
  mLastMouseOverFrame = nsnull;
  mCurrentTarget = nsnull;
  mLastLeftMouseDownFrame = nsnull;
  mLastMiddleMouseDownFrame = nsnull;
  mLastRightMouseDownFrame = nsnull;

  mActiveLink = nsnull;
  mHoverLink = nsnull;
  mCurrentFocus = nsnull;
  mDocument = nsnull;
  mPresContext = nsnull;
  NS_INIT_REFCNT();
}

nsEventStateManager::~nsEventStateManager() {
  NS_IF_RELEASE(mActiveLink);
  NS_IF_RELEASE(mHoverLink);
  NS_IF_RELEASE(mCurrentFocus);
  NS_IF_RELEASE(mDocument);
}

NS_IMPL_ADDREF(nsEventStateManager)
NS_IMPL_RELEASE(nsEventStateManager)
NS_IMPL_QUERY_INTERFACE(nsEventStateManager, kIEventStateManagerIID);

NS_IMETHODIMP
nsEventStateManager::PreHandleEvent(nsIPresContext& aPresContext, 
                                 nsGUIEvent *aEvent,
                                 nsIFrame* aTargetFrame,
                                 nsEventStatus& aStatus)
{
  mCurrentTarget = aTargetFrame;

  nsFrameState state;
  mCurrentTarget->GetFrameState(state);
  state |= NS_FRAME_EXTERNAL_REFERENCE;
  mCurrentTarget->SetFrameState(state);

  aStatus = nsEventStatus_eIgnore;
  
  switch (aEvent->message) {
  case NS_MOUSE_MOVE:
    UpdateCursor(aPresContext, aEvent->point, aTargetFrame, aStatus);
    GenerateMouseEnterExit(aPresContext, aEvent);
    break;
  case NS_MOUSE_EXIT:
    //GenerateMouseEnterExit(aPresContext, aEvent, aTargetFrame);
    break;
  case NS_GOTFOCUS:
    NS_IF_RELEASE(mCurrentFocus);
    if (nsnull != mCurrentTarget) {
      mCurrentTarget->GetContent(&mCurrentFocus);
    }
    break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::PostHandleEvent(nsIPresContext& aPresContext, 
                                 nsGUIEvent *aEvent,
                                 nsIFrame* aTargetFrame,
                                 nsEventStatus& aStatus)
{
  mCurrentTarget = aTargetFrame;
  nsresult ret = NS_OK;

  nsFrameState state;
  mCurrentTarget->GetFrameState(state);
  state |= NS_FRAME_EXTERNAL_REFERENCE;
  mCurrentTarget->SetFrameState(state);

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_DOWN:
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
  case NS_MOUSE_RIGHT_BUTTON_DOWN: 
    {
      if (nsnull != aEvent->widget) {
        aEvent->widget->SetFocus();
      }
    }
    //Break left out on purpose
  case NS_MOUSE_LEFT_BUTTON_UP:
  case NS_MOUSE_MIDDLE_BUTTON_UP:
  case NS_MOUSE_RIGHT_BUTTON_UP:
    ret = CheckForAndDispatchClick(aPresContext, (nsMouseEvent*)aEvent, aStatus);
    break;
  case NS_KEY_DOWN:
    ret = DispatchKeyPressEvent(aPresContext, (nsKeyEvent*)aEvent, aStatus);
    if (nsEventStatus_eConsumeNoDefault != aStatus) {
      switch(((nsKeyEvent*)aEvent)->keyCode) {
        case NS_VK_TAB:
          ShiftFocus();
          aStatus = nsEventStatus_eConsumeNoDefault;
          break;
        case NS_VK_PAGE_DOWN: 
        case NS_VK_PAGE_UP:
          break;
        case NS_VK_DOWN: 
        case NS_VK_UP:
          break;
      }
    }
    break;
  }
  return ret;
}

NS_IMETHODIMP
nsEventStateManager::SetPresContext(nsIPresContext* aPresContext)
{
  mPresContext = aPresContext;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::ClearFrameRefs(nsIFrame* aFrame)
{
  if (aFrame == mLastMouseOverFrame) {
    mLastMouseOverFrame = nsnull;
  }
  if (aFrame == mCurrentTarget) {
    mCurrentTarget = nsnull;
  }
  if (aFrame == mLastLeftMouseDownFrame) {
    mLastLeftMouseDownFrame = nsnull;
  }
  if (aFrame == mLastMiddleMouseDownFrame) {
    mLastMiddleMouseDownFrame = nsnull;
  }
  if (aFrame == mLastRightMouseDownFrame) {
    mLastRightMouseDownFrame = nsnull;
  }
  return NS_OK;
}

void
nsEventStateManager::UpdateCursor(nsIPresContext& aPresContext, nsPoint& aPoint, nsIFrame* aTargetFrame, 
                                  nsEventStatus& aStatus)
{
  PRInt32 cursor;
  nsCursor c;

  aTargetFrame->GetCursor(aPresContext, aPoint, cursor);

  switch (cursor) {
  default:
  case NS_STYLE_CURSOR_AUTO:
  case NS_STYLE_CURSOR_DEFAULT:
    c = eCursor_standard;
    break;
  case NS_STYLE_CURSOR_POINTER:
    c = eCursor_hyperlink;
    break;
  case NS_STYLE_CURSOR_TEXT:
    c = eCursor_select;
    break;
  case NS_STYLE_CURSOR_N_RESIZE:
  case NS_STYLE_CURSOR_S_RESIZE:
    c = eCursor_sizeNS;
    break;
  case NS_STYLE_CURSOR_W_RESIZE:
  case NS_STYLE_CURSOR_E_RESIZE:
    c = eCursor_sizeWE;
    break;
  case NS_STYLE_CURSOR_NE_RESIZE:
    c = eCursor_select;
    break;
  case NS_STYLE_CURSOR_NW_RESIZE:
    c = eCursor_select;
    break;
  case NS_STYLE_CURSOR_SE_RESIZE:
    c = eCursor_select;
    break;
  case NS_STYLE_CURSOR_SW_RESIZE:
    c = eCursor_select;
    break;
  }

  if (NS_STYLE_CURSOR_AUTO != cursor) {
    aStatus = nsEventStatus_eConsumeDoDefault;
  }

  nsIWidget* window;
  aTargetFrame->GetWindow(window);
  window->SetCursor(c);
  NS_RELEASE(window);
}

void
nsEventStateManager::GenerateMouseEnterExit(nsIPresContext& aPresContext, nsGUIEvent* aEvent)
{
  switch(aEvent->message) {
  case NS_MOUSE_MOVE:
    {
      if (mLastMouseOverFrame != mCurrentTarget) {
        //We'll need the content, too, to check if it changed separately from the frames.
        nsIContent *lastContent = nsnull;
        nsIContent *targetContent;

        mCurrentTarget->GetContent(&targetContent);

        if (nsnull != mLastMouseOverFrame) {
          //fire mouseout
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_MOUSE_EVENT;
          event.message = NS_MOUSE_EXIT;
          event.widget = nsnull;

          //The frame has change but the content may not have.  Check before dispatching to content
          mLastMouseOverFrame->GetContent(&lastContent);

          if (lastContent != targetContent) {
            //XXX This event should still go somewhere!!
            if (nsnull != lastContent) {
              lastContent->HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, status); 
            }
          }

          //Now dispatch to the frame
          if (nsnull != mLastMouseOverFrame) {
            //XXX Get the new frame
            mLastMouseOverFrame->HandleEvent(aPresContext, &event, status);   
          }
        }

        //fire mouseover
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event;
        event.eventStructType = NS_MOUSE_EVENT;
        event.message = NS_MOUSE_ENTER;
        event.widget = nsnull;

        //The frame has change but the content may not have.  Check before dispatching to content
        if (lastContent != targetContent) {
          //XXX This event should still go somewhere!!
          if (nsnull != targetContent) {
            targetContent->HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, status); 
          }
        }

        //Now dispatch to the frame
        if (nsnull != mCurrentTarget) {
          //XXX Get the new frame
          mCurrentTarget->HandleEvent(aPresContext, &event, status);
        }

        NS_IF_RELEASE(lastContent);
        NS_IF_RELEASE(targetContent);

        mLastMouseOverFrame = mCurrentTarget;
      }
    }
    break;
  case NS_MOUSE_EXIT:
    {
      //This is actually the window mouse exit event.  Such a think does not
      // yet exist but it will need to eventually.
      if (nsnull != mLastMouseOverFrame) {
        //fire mouseout
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event;
        event.eventStructType = NS_MOUSE_EVENT;
        event.message = NS_MOUSE_EXIT;
        event.widget = nsnull;

        nsIContent *lastContent;
        mLastMouseOverFrame->GetContent(&lastContent);

        if (nsnull != lastContent) {
          lastContent->HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, status); 
          NS_RELEASE(lastContent);
        }

        //Now dispatch to the frame
        if (nsnull != mLastMouseOverFrame) {
          //XXX Get the new frame
          mLastMouseOverFrame->HandleEvent(aPresContext, &event, status);   
        }
      }
    }
    break;
  }
  
}

NS_IMETHODIMP
nsEventStateManager::CheckForAndDispatchClick(nsIPresContext& aPresContext, 
                                              nsMouseEvent *aEvent,
                                              nsEventStatus& aStatus)
{
  nsresult ret;
  nsMouseEvent event;
  PRBool fireClick = PR_FALSE;

  switch (aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_DOWN:
    mLastLeftMouseDownFrame = mCurrentTarget;
    break;
  case NS_MOUSE_LEFT_BUTTON_UP:
    if (mLastLeftMouseDownFrame == mCurrentTarget) {
      fireClick = PR_TRUE;
      event.message = NS_MOUSE_LEFT_CLICK;
    }
    break;
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    mLastMiddleMouseDownFrame = mCurrentTarget;
    break;
  case NS_MOUSE_MIDDLE_BUTTON_UP:
    if (mLastMiddleMouseDownFrame == mCurrentTarget) {
      fireClick = PR_TRUE;
      event.message = NS_MOUSE_MIDDLE_CLICK;
    }
    break;
  case NS_MOUSE_RIGHT_BUTTON_DOWN:
    mLastRightMouseDownFrame = mCurrentTarget;
    break;
  case NS_MOUSE_RIGHT_BUTTON_UP:
    if (mLastRightMouseDownFrame == mCurrentTarget) {
      fireClick = PR_TRUE;
      event.message = NS_MOUSE_RIGHT_CLICK;
    }
    break;
  }

  if (fireClick) {
    //fire click
    event.eventStructType = NS_MOUSE_EVENT;
    event.widget = nsnull;

    nsIContent *content;
    mCurrentTarget->GetContent(&content);

    if (nsnull != content) {
      ret = content->HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, aStatus); 
      NS_RELEASE(content);
    }

    //Now dispatch to the frame
    if (nsnull != mCurrentTarget) {
      mCurrentTarget->HandleEvent(aPresContext, &event, aStatus);   
    }
  }
  return ret;
}

NS_IMETHODIMP
nsEventStateManager::DispatchKeyPressEvent(nsIPresContext& aPresContext, 
                                           nsKeyEvent *aEvent,
                                           nsEventStatus& aStatus)
{
  nsresult ret;

  //fire keypress
  nsKeyEvent event;
  event.eventStructType = NS_KEY_EVENT;
  event.message = NS_KEY_PRESS;
  event.widget = nsnull;
  event.keyCode = aEvent->keyCode;

  nsIContent *content;
  mCurrentTarget->GetContent(&content);

  if (nsnull != content) {
    ret = content->HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, aStatus); 
    NS_RELEASE(content);
  }

  //Now dispatch to the frame
  if (nsnull != mCurrentTarget) {
    mCurrentTarget->HandleEvent(aPresContext, &event, aStatus);   
  }

  return ret;
}

void
nsEventStateManager::ShiftFocus()
{
  if (nsnull == mPresContext) {
    return;
  }

  if (nsnull == mCurrentFocus) {
    if (nsnull == mDocument) {
      nsIPresShell* presShell = mPresContext->GetShell();
      if (nsnull != presShell) {
        mDocument = presShell->GetDocument();
        NS_RELEASE(presShell);
        if (nsnull == mDocument) {
          return;
        }
      }
    }
    mCurrentFocus = mDocument->GetRootContent();
    if (nsnull == mCurrentFocus) {  
      return;
    }
  }

  nsIContent* next = GetNextTabbableContent(mCurrentFocus, nsnull, mCurrentFocus);

  if (nsnull == next) {
    NS_IF_RELEASE(mCurrentFocus);
    //XXX pass focus up to webshellcontainer FocusAvailable
    return;
  }

  nsIDOMHTMLAnchorElement *nextAnchor;
  nsIDOMHTMLInputElement *nextInput;
  nsIDOMHTMLSelectElement *nextSelect;
  nsIDOMHTMLTextAreaElement *nextTextArea;
  if (NS_OK == next->QueryInterface(kIDOMHTMLAnchorElementIID,(void **)&nextAnchor)) {
    nextAnchor->Focus();
    NS_RELEASE(nextAnchor);
  }
  else if (NS_OK == next->QueryInterface(kIDOMHTMLInputElementIID,(void **)&nextInput)) {
    nextInput->Focus();
    NS_RELEASE(nextInput);
  }
  else if (NS_OK == next->QueryInterface(kIDOMHTMLSelectElementIID,(void **)&nextSelect)) {
    nextSelect->Focus();
    NS_RELEASE(nextSelect);
  }
  else if (NS_OK == next->QueryInterface(kIDOMHTMLTextAreaElementIID,(void **)&nextTextArea)) {
    nextTextArea->Focus();
    NS_RELEASE(nextTextArea);
  }

  NS_IF_RELEASE(mCurrentFocus);
  mCurrentFocus = next;
}

/*
 * At some point this will need to be linked into HTML 4.0 tabindex
 */
nsIContent*
nsEventStateManager::GetNextTabbableContent(nsIContent* aParent, nsIContent* aChild, nsIContent* aTop)
{
  PRInt32 count, index;
  aParent->ChildCount(count);

  if (nsnull != aChild) {
    aParent->IndexOf(aChild, index);
    index += 1;
  }
  else {
    index = 0;
  }

  for (;index < count;index++) {
    nsIContent* child;
    aParent->ChildAt(index, child);
    nsIContent* content = GetNextTabbableContent(child, nsnull, aTop);
    if (content != nsnull) {
      NS_IF_RELEASE(child);
      return content;
    }
    if (nsnull != child) {
      nsIAtom* tag;
      PRBool disabled = PR_TRUE;
      PRBool hidden = PR_FALSE;

      child->GetTag(tag);
      if (nsHTMLAtoms::input==tag) {
        nsIDOMHTMLInputElement *nextInput;
        if (NS_OK == child->QueryInterface(kIDOMHTMLInputElementIID,(void **)&nextInput)) {
          nextInput->GetDisabled(&disabled);

          nsAutoString type;
          nextInput->GetType(type);
          if (type.EqualsIgnoreCase("hidden")) {
            hidden = PR_TRUE;
          }
          NS_RELEASE(nextInput);
        }
      }
      else if (nsHTMLAtoms::select==tag) {
        nsIDOMHTMLSelectElement *nextSelect;
        if (NS_OK == child->QueryInterface(kIDOMHTMLSelectElementIID,(void **)&nextSelect)) {
          nextSelect->GetDisabled(&disabled);
          NS_RELEASE(nextSelect);
        }
      }
      else if (nsHTMLAtoms::textarea==tag) {
        nsIDOMHTMLTextAreaElement *nextTextArea;
        if (NS_OK == child->QueryInterface(kIDOMHTMLTextAreaElementIID,(void **)&nextTextArea)) {
          nextTextArea->GetDisabled(&disabled);
          NS_RELEASE(nextTextArea);
        }

      }
      //XXX Not all of these are focusable yet.
      else if(nsHTMLAtoms::a==tag
              //nsHTMLAtoms::area==tag ||
              //nsHTMLAtoms::button==tag ||
              //nsHTMLAtoms::object==tag
              ) {
        disabled = PR_FALSE;
      }

      if (!disabled && !hidden) {
        return child;
      }
      NS_RELEASE(child);
    }
  }

  if (aParent == aTop) {
    nsIContent* nextParent;
    aParent->GetParent(nextParent);
    if (nsnull != nextParent) {
      nsIContent* content = GetNextTabbableContent(nextParent, aParent, nextParent);
      NS_RELEASE(nextParent);
      return content;
    }
  }

  return nsnull;
}

NS_IMETHODIMP
nsEventStateManager::GetEventTarget(nsIFrame **aFrame)
{
  *aFrame = mCurrentTarget;
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::GetLinkState(nsIContent *aLink, nsLinkEventState& aState)
{
  if (aLink == mActiveLink) {
    aState = eLinkState_Active;
  }
  else if (aLink == mHoverLink) {
    aState = eLinkState_Hover;
  }
  else {
    aState = eLinkState_Unspecified;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::SetActiveLink(nsIContent *aLink)
{
  nsIDocument *mDocument;

  if (nsnull != mActiveLink) {
    if (NS_OK == mActiveLink->GetDocument(mDocument)) {
      mDocument->ContentChanged(mActiveLink, nsnull);
      NS_RELEASE(mDocument);
    }
  }
  NS_IF_RELEASE(mActiveLink);

  mActiveLink = aLink;

  NS_IF_ADDREF(mActiveLink);
  if (nsnull != mActiveLink) {
    if (NS_OK == mActiveLink->GetDocument(mDocument)) {
      mDocument->ContentChanged(mActiveLink, nsnull);
      NS_RELEASE(mDocument);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEventStateManager::SetHoverLink(nsIContent *aLink)
{
  nsIDocument *mDocument;

  if (nsnull != mHoverLink) {
    if (NS_OK == mHoverLink->GetDocument(mDocument)) {
      mDocument->ContentChanged(mHoverLink, nsnull);
      NS_RELEASE(mDocument);
    }
  }
  NS_IF_RELEASE(mHoverLink);

  mHoverLink = aLink;

  NS_IF_ADDREF(mHoverLink);
  if (nsnull != mHoverLink) {
    if (NS_OK == mHoverLink->GetDocument(mDocument)) {
      mDocument->ContentChanged(mHoverLink, nsnull);
      NS_RELEASE(mDocument);
    }
  }

  return NS_OK;
}

nsresult NS_NewEventStateManager(nsIEventStateManager** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIEventStateManager* manager = new nsEventStateManager();
  if (nsnull == manager) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return manager->QueryInterface(kIEventStateManagerIID, (void **) aInstancePtrResult);
}

