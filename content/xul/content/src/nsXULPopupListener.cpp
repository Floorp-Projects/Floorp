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
#include "nsRDFCID.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kXULPopupListenerCID,      NS_XULPOPUPLISTENER_CID);
static NS_DEFINE_IID(kIXULPopupListenerIID,     NS_IXULPOPUPLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);

////////////////////////////////////////////////////////////////////////
// PopupListenerImpl
//
//   This is the popup listener implementation for popup menus, context menus,
//   and tooltips.
//
class XULPopupListenerImpl : public nsIXULPopupListener,
                             public nsIDOMMouseListener
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

    // nsIDOMEventListener
    virtual nsresult HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

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
    else if (iid.Equals(nsIDOMEventListener::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMEventListener*, this);
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

  switch (popupType) {
    case eXULPopupType_popup:
      // Check for left mouse button down
      aMouseEvent->GetButton(&button);
      if (button == 1) {
        // Time to launch a popup menu.
        printf("Popup menu launching now!\n");
      }
      break;
    case eXULPopupType_context:
      // Check for right mouse button down
      aMouseEvent->GetButton(&button);
      // XXX: Handle Mac 
      if (button == 2) {
        // Time to launch a context menu.
        printf("Context menu launching now!\n");
      }
      break;
  }
  return NS_OK;
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
