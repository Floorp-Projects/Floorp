/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*
  This file provides the implementation for the sort service manager.
 */

#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIXULPopupListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMFocusListener.h"
#include "nsRDFCID.h"

#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMUIEvent.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kXULPopupListenerCID,      NS_XULPOPUPLISTENER_CID);
static NS_DEFINE_IID(kIXULPopupListenerIID,     NS_IXULPOPUPLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDomEventListenerIID,   NS_IDOMEVENTLISTENER_IID);

////////////////////////////////////////////////////////////////////////
// PopupListenerImpl
//
//   This is the popup listener implementation for popup menus, context menus,
//   and tooltips.
//
class XULPopupListenerImpl : public nsIXULPopupListener,
                             public nsIDOMMouseListener,
                             public nsIDOMFocusListener
{
public:
    XULPopupListenerImpl(void);
    virtual ~XULPopupListenerImpl(void);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIXULPopupListener
    NS_IMETHOD Init(nsIDOMElement* aElement, const XULPopupType& popupType);

    // nsIDOMMouseListener
    virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
    virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent) { return NS_OK; };

    // nsIDOMFocusListener
    virtual nsresult Focus(nsIDOMEvent* aEvent) { return NS_OK; };
    virtual nsresult Blur(nsIDOMEvent* aEvent);

    // nsIDOMEventListener
    virtual nsresult HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

protected:
    virtual nsresult LaunchPopup(nsIDOMEvent* anEvent);

private:
    nsIDOMElement* element; // Weak reference. The element will go away first.
    XULPopupType popupType;
};

////////////////////////////////////////////////////////////////////////

XULPopupListenerImpl::XULPopupListenerImpl(void)
{
	NS_INIT_REFCNT();
	
}

XULPopupListenerImpl::~XULPopupListenerImpl(void)
{
}

NS_IMPL_ADDREF(XULPopupListenerImpl)
NS_IMPL_RELEASE(XULPopupListenerImpl)

NS_IMETHODIMP
XULPopupListenerImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(nsIXULPopupListener::GetIID())) {
        *result = NS_STATIC_CAST(nsIXULPopupListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(nsIDOMMouseListener::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMMouseListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(nsIDOMFocusListener::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMFocusListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(kIDomEventListenerIID)) {
        *result = (nsIDOMEventListener*)(nsIDOMMouseListener*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

NS_IMETHODIMP
XULPopupListenerImpl::Init(nsIDOMElement* aElement, const XULPopupType& popup)
{
  element = aElement; // Weak reference. Don't addref it.
  popupType = popup;
  return NS_OK;
}

////////////////////////////////////////////////////////////////
// nsIDOMMouseListener

nsresult
XULPopupListenerImpl::MouseDown(nsIDOMEvent* aMouseEvent)
{
  PRUint32 button;

  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aMouseEvent);
  if (!uiEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  switch (popupType) {
    case eXULPopupType_popup:
      // Check for left mouse button down
      uiEvent->GetButton(&button);
      if (button == 1) {
        // Time to launch a popup menu.
        LaunchPopup(aMouseEvent);
      }
      break;
    case eXULPopupType_context:
#ifdef	XP_MAC
      // XXX: Handle Mac (currently checks if CTRL key is down)
	PRBool	ctrlKey = PR_FALSE;
	uiEvent->GetCtrlKey(&ctrlKey);
	if (ctrlKey == PR_TRUE)
#else
      // Check for right mouse button down
      uiEvent->GetButton(&button);
      if (button == 3)
#endif
      {
        // Time to launch a context menu.
        LaunchPopup(aMouseEvent);
      }
      break;
  }
  return NS_OK;
}

nsresult
XULPopupListenerImpl::LaunchPopup(nsIDOMEvent* anEvent)
{
  nsresult rv = NS_OK;

  nsString type("popup");
  if (eXULPopupType_context == popupType)
    type = "context";

  nsString identifier;
  element->GetAttribute(type, identifier);

  if (identifier == "")
    return rv;

  // Try to find the popup content.
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content = do_QueryInterface(element);
  if (NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document)))) {
    NS_ERROR("Unable to retrieve the document.");
    return rv;
  }

  // Turn the document into a XUL document so we can use getElementById
  nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
  if (xulDocument == nsnull) {
    NS_ERROR("Popup attached to an element that isn't in XUL!");
    return NS_ERROR_FAILURE;
  }

  // XXX Handle the _child case for popups and context menus!

  // Use getElementById to obtain the popup content.
  nsCOMPtr<nsIDOMElement> popupContent;
  if (NS_FAILED(rv = xulDocument->GetElementById(identifier, getter_AddRefs(popupContent)))) {
    NS_ERROR("GetElementById had some kind of spasm.");
    return rv;
  }

  if (popupContent == nsnull) {
    // Gracefully fail in this case.
    return NS_OK;
  }

  // We have some popup content. Obtain our window.
  nsIScriptContextOwner* owner = document->GetScriptContextOwner();
  nsCOMPtr<nsIScriptContext> context;
  if (NS_OK == owner->GetScriptContext(getter_AddRefs(context))) {
    nsIScriptGlobalObject* global = context->GetGlobalObject();
    if (global) {
      // Get the DOM window
      nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(global);
      if (domWindow != nsnull) {
        // Find out if we're anchored.
        nsString anchorAlignment;
        element->GetAttribute("popupanchor", anchorAlignment);

        nsString popupAlignment("topleft");
        element->GetAttribute("popupalign", popupAlignment);

		    // Set the popup in the document for the duration of this call.
		    xulDocument->SetPopup(element);
        if (anchorAlignment == "") {
          // We aren't anchored. Create on the point.
          // Retrieve our x and y position.
          nsCOMPtr<nsIDOMUIEvent>uiEvent;
          uiEvent = do_QueryInterface(anEvent);
          if (!uiEvent) {
            //non-ui event passed in.  bad things.
            return NS_OK;
          }

          PRInt32 xPos, yPos;
    		  uiEvent->GetScreenX(&xPos); 
          uiEvent->GetScreenY(&yPos); 
                
          domWindow->CreatePopup(element, popupContent, 
                               xPos, yPos, 
                               type, popupAlignment);
        }
        else {
          // We're anchored. Pass off to the window, and let it figure out
          // from the frame where it wants to put us.
          domWindow->CreateAnchoredPopup(element, popupContent,
                                         anchorAlignment, type, popupAlignment);
        }
		    xulDocument->SetPopup(nsnull);
      }
      NS_RELEASE(global);
    }
  }
  NS_IF_RELEASE(owner);

  return NS_OK;
}

nsresult
XULPopupListenerImpl::Blur(nsIDOMEvent* aMouseEvent)
{
  nsresult rv = NS_OK;

  // Try to find the popup content.
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content = do_QueryInterface(element);
  if (NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document)))) {
    NS_ERROR("Unable to retrieve the document.");
    return rv;
  }

  // Blur events don't bubble, so this means our window lost focus.
  // Let's check just to make sure.
  nsCOMPtr<nsIDOMNode> eventTarget;
  aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
  
  // We have some popup content. Obtain our window.
  nsIScriptContextOwner* owner = document->GetScriptContextOwner();
  nsCOMPtr<nsIScriptContext> context;
  if (NS_OK == owner->GetScriptContext(getter_AddRefs(context))) {
    nsIScriptGlobalObject* global = context->GetGlobalObject();
    if (global) {
      // Get the DOM window
      nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(global);
  
      // Close, but only if we are the same target.
      nsCOMPtr<nsIDOMNode> windowNode = do_QueryInterface(domWindow);
      if (windowNode.get() == eventTarget.get())
        domWindow->Close();
    }
  }

  // XXX Figure out how to fire the DESTRUCT event for the
  // arbitrary XUL case

  return rv;
}

////////////////////////////////////////////////////////////////
nsresult
NS_NewXULPopupListener(nsIXULPopupListener** pop)
{
    XULPopupListenerImpl* popup = new XULPopupListenerImpl();
    if (!popup)
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(popup);
    *pop = popup;
    return NS_OK;
}
