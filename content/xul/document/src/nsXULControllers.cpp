/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*
  This file provides the implementation for the XUL Command Dispatcher.
 */

#include "nsCOMPtr.h"

#include "nsVoidArray.h"
#include "nsISupportsArray.h"

#include "nsIDOMElement.h"
#include "nsIXULCommandDispatcher.h"
#include "nsIDOMFocusListener.h"
#include "nsRDFCID.h"

#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMUIEvent.h"

#include "nsIDOMXULElement.h"
#include "nsIControllers.h"

#include "nsIPresContext.h"
#include "nsIPresShell.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIScriptObjectOwnerIID,      NS_ISCRIPTOBJECTOWNER_IID);

static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDomEventListenerIID,   NS_IDOMEVENTLISTENER_IID);

////////////////////////////////////////////////////////////////////////
// XULControllersImpl
//
//   This is the focus manager for XUL documents.
//
class XULControllersImpl : public nsIControllers
{
public:
    XULControllersImpl(void);
    virtual ~XULControllersImpl(void);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

	NS_DECL_NSICONTROLLERS
   
  
protected:
    nsISupportsArray* mControllers;
};

////////////////////////////////////////////////////////////////////////

XULControllersImpl::XULControllersImpl(void) :
  mControllers(nsnull)
{
	NS_INIT_REFCNT();
}

XULControllersImpl::~XULControllersImpl(void)
{
	NS_IF_RELEASE(mControllers);
}

NS_IMPL_ADDREF(XULControllersImpl)
NS_IMPL_RELEASE(XULControllersImpl)

NS_IMETHODIMP
XULControllersImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kISupportsIID)) {
        *result = (nsISupports*)(nsIControllers*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(nsIControllers::GetIID())) {
        *result = NS_STATIC_CAST(nsIControllers*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    } 

    return NS_NOINTERFACE;
}


  /* boolean SupportsCommand (in string command); */
  NS_IMETHODIMP XULControllersImpl::GetControllerForCommand(const PRUnichar *command, nsIController** _retval)
  {
    *_retval = nsnull;
    if(!mControllers) 
      return NS_OK;

    PRUint32 count;
    mControllers->Count(&count);
    for(PRUint32 i=0; i < count; i++) {
        nsCOMPtr<nsIController> controller;
        nsCOMPtr<nsISupports>   supports;
        mControllers->GetElementAt(i, getter_AddRefs(supports));
        controller = do_QueryInterface(supports);
        if( controller ) {
          PRBool supportsCommand;
          controller->SupportsCommand(command, &supportsCommand);
          if(supportsCommand) {
            *_retval = controller;
            NS_ADDREF(*_retval);
            return NS_OK;
          }
        }
    }
    return NS_OK;
  }

  /* void InsertControllerAt (in short index, in nsIController controller); */
  NS_IMETHODIMP XULControllersImpl::InsertControllerAt(PRUint32 index, nsIController *controller)
  {
    if(! mControllers )
      NS_NewISupportsArray(&mControllers);

    mControllers->InsertElementAt(controller, index);
    return NS_OK;
  }

  /* nsIController RemoveControllerAt (in short index); */
  NS_IMETHODIMP XULControllersImpl::RemoveControllerAt(PRUint32 index, nsIController **_retval)
  {
    if(mControllers) {
       nsCOMPtr<nsISupports> supports;
       mControllers->GetElementAt(index, getter_AddRefs(supports));
       supports->QueryInterface(nsIController::GetIID(), (void**)_retval);
       mControllers->RemoveElementAt(index);  
    } else
      *_retval = nsnull;
    
    return NS_OK;
  }

  /* nsIController GetControllerAt (in short index); */
  NS_IMETHODIMP XULControllersImpl::GetControllerAt(PRUint32 index, nsIController **_retval)
  {
    if(mControllers) {
       nsCOMPtr<nsISupports> supports;
       mControllers->GetElementAt(index, getter_AddRefs(supports));
       supports->QueryInterface(nsIController::GetIID(), (void**)_retval);
    } else 
      *_retval = nsnull;
    
    return NS_OK;
  }

  /* void AppendController (in nsIController controller); */
  NS_IMETHODIMP XULControllersImpl::AppendController(nsIController *controller)
  {
    if(! mControllers )
      NS_NewISupportsArray(&mControllers);
     
    mControllers->AppendElement(controller);
    return NS_OK;
  }

  /* void RemoveController (in nsIController controller); */
  NS_IMETHODIMP XULControllersImpl::RemoveController(nsIController *controller)
  {
    if(mControllers) {
       nsCOMPtr<nsISupports> supports = do_QueryInterface(controller);
       mControllers->RemoveElement(supports);  
    }
    
    return NS_OK;
  }

  /* short GetControllerCount (); */
  NS_IMETHODIMP XULControllersImpl::GetControllerCount(PRUint32 *_retval)
  {
    *_retval = 0;
    if(mControllers) 
      mControllers->Count(_retval);
    
    return NS_OK;
  }

////////////////////////////////////////////////////////////////
nsresult
NS_NewXULControllers(nsIControllers** aControllers)
{
    nsIControllers* impl = new XULControllersImpl();
    if (!impl)
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(impl);
    *aControllers = impl;
    return NS_OK;
}
