/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsMenuBarListener.h"
#include "nsMenuBarFrame.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"

// Drag & Drop, Clipboard
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsCOMPtr.h"
#include "nsIDOMUIEvent.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsXULAtoms.h"

#include "nsIEventStateManager.h"

#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsISupportsArray.h"

/*
 * nsMenuBarListener implementation
 */

NS_IMPL_ADDREF(nsMenuBarListener)
NS_IMPL_RELEASE(nsMenuBarListener)


////////////////////////////////////////////////////////////////////////
nsMenuBarListener::nsMenuBarListener(nsMenuBarFrame* aMenuBar) 
{
  NS_INIT_REFCNT();
  mMenuBarFrame = aMenuBar;
}

////////////////////////////////////////////////////////////////////////
nsMenuBarListener::~nsMenuBarListener() 
{
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(nsCOMTypeInfo<nsIDOMEventReceiver>::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)(nsIDOMMouseMotionListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsIDOMMouseListener>::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMMouseListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsIDOMKeyListener>::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMKeyListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsIDOMMouseMotionListener>::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMMouseMotionListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {                                      
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMMouseMotionListener*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }
  return NS_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////
// This is temporary until the bubbling of events for CSS actions work
////////////////////////////////////////////////////////////////////////
static void ForceDrawFrame(nsIFrame * aFrame)
{
  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(pnt, &view);
  aFrame->GetRect(rect);
  rect.x = pnt.x;
  rect.y = pnt.y;
  if (view) {
    nsCOMPtr<nsIViewManager> viewMgr;
    view->GetViewManager(*getter_AddRefs(viewMgr));
    if (viewMgr)
      viewMgr->UpdateView(view, rect, NS_VMREFRESH_AUTO_DOUBLE_BUFFER);
  }

}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::MouseMove(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::DragMove(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::MouseClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyDown(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuBarListener::KeyPress(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}
