/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Mark Hammond <MarkH@ActiveState.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  This file provides the implementation for the XUL "controllers"
  object.

*/

#include "nsString.h"

#include "nsIControllers.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsXULControllers.h"
#include "nsString.h"
#include "nsContentUtils.h"

//----------------------------------------------------------------------

nsXULControllers::nsXULControllers()
: mCurControllerID(0)
{
	NS_INIT_REFCNT();
}

nsXULControllers::~nsXULControllers(void)
{
  DeleteControllers();
}

void
nsXULControllers::DeleteControllers()
{
  PRUint32 count = mControllers.Count();
  for (PRUint32 i = 0; i < count; i++)
  {
    nsXULControllerData*  controllerData = NS_STATIC_CAST(nsXULControllerData*, mControllers.ElementAt(i));
    if (controllerData)
      delete controllerData;    // releases the nsIController
  }
  
  mControllers.Clear();
}


NS_IMETHODIMP
NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsXULControllers* controllers = new nsXULControllers();
    if (! controllers)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    NS_ADDREF(controllers);
    rv = controllers->QueryInterface(aIID, aResult);
    NS_RELEASE(controllers);
    return rv;
}

// QueryInterface implementation for nsXULControllers
NS_INTERFACE_MAP_BEGIN(nsXULControllers)
  NS_INTERFACE_MAP_ENTRY(nsIControllers)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XULControllers)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsXULControllers)
NS_IMPL_RELEASE(nsXULControllers)


NS_IMETHODIMP
nsXULControllers::GetCommandDispatcher(nsIDOMXULCommandDispatcher** _result)
{
    nsCOMPtr<nsIDOMXULCommandDispatcher> dispatcher = do_QueryReferent(mCommandDispatcher);
    *_result = dispatcher;
    NS_IF_ADDREF(*_result);
    return NS_OK;
}


NS_IMETHODIMP
nsXULControllers::SetCommandDispatcher(nsIDOMXULCommandDispatcher* aCommandDispatcher)
{
    mCommandDispatcher = getter_AddRefs(NS_GetWeakReference(aCommandDispatcher));
    return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::GetControllerForCommand(const nsAReadableString& aCommand, nsIController** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;

  PRUint32 count = mControllers.Count();
  for (PRUint32 i=0; i < count; i++)
  {
    nsXULControllerData*  controllerData = NS_STATIC_CAST(nsXULControllerData*, mControllers.ElementAt(i));
    if (controllerData)
    {
        nsCOMPtr<nsIController> controller;
      controllerData->GetController(getter_AddRefs(controller));
      if (controller)
      {
            PRBool supportsCommand;
        controller->SupportsCommand(aCommand, &supportsCommand);
        if (supportsCommand) {
                *_retval = controller;
                NS_ADDREF(*_retval);
                return NS_OK;
            }
        }
    }
  }
  
    return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::InsertControllerAt(PRUint32 index, nsIController *controller)
{
  nsXULControllerData*  controllerData = new nsXULControllerData(mCurControllerID++, controller);
  if (!controllerData) return NS_ERROR_OUT_OF_MEMORY;
  PRBool  inserted = mControllers.InsertElementAt((void *)controllerData, index);
  NS_ASSERTION(inserted, "Insertion of controller failed");
    return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::RemoveControllerAt(PRUint32 index, nsIController **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
        *_retval = nsnull;
  nsXULControllerData*  controllerData = NS_STATIC_CAST(nsXULControllerData*, mControllers.ElementAt(index));
  if (!controllerData) return NS_ERROR_FAILURE;
  
  PRBool removed = mControllers.RemoveElementAt(index);
  NS_ASSERTION(removed, "Removal of controller failed");
    
  controllerData->GetController(_retval);
  delete controllerData;
  
    return NS_OK;
}


NS_IMETHODIMP
nsXULControllers::GetControllerAt(PRUint32 index, nsIController **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
        *_retval = nsnull;

  nsXULControllerData*  controllerData = NS_STATIC_CAST(nsXULControllerData*, mControllers.ElementAt(index));
  if (!controllerData) return NS_ERROR_FAILURE;

  return controllerData->GetController(_retval);   // does the addref  
}

NS_IMETHODIMP
nsXULControllers::AppendController(nsIController *controller)
{
  nsXULControllerData*  controllerData = new nsXULControllerData(mCurControllerID++, controller);
  if (!controllerData) return NS_ERROR_OUT_OF_MEMORY;
  PRBool  appended = mControllers.AppendElement((void *)controllerData);
  NS_ASSERTION(appended, "Appending controller failed");
    return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::RemoveController(nsIController *controller)
{
  // first get the identity pointer
  nsCOMPtr<nsISupports> controllerSup(do_QueryInterface(controller));
  // then find it
  PRUint32 count = mControllers.Count();
  for (PRUint32 i = 0; i < count; i++)
  {
    nsXULControllerData*  controllerData = NS_STATIC_CAST(nsXULControllerData*, mControllers.ElementAt(i));
    if (controllerData)
    {
      nsCOMPtr<nsIController> thisController;
      controllerData->GetController(getter_AddRefs(thisController));
      nsCOMPtr<nsISupports> thisControllerSup(do_QueryInterface(thisController)); // get identity
      if (thisControllerSup == controllerSup)
      {
        mControllers.RemoveElementAt(i);
        delete controllerData;
        return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;      // right thing to return if no controller found?
}
    
/* unsigned long getControllerId (in nsIController controller); */
NS_IMETHODIMP
nsXULControllers::GetControllerId(nsIController *controller, PRUint32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  PRUint32 count = mControllers.Count();
  for (PRUint32 i = 0; i < count; i++)
  {
    nsXULControllerData*  controllerData = NS_STATIC_CAST(nsXULControllerData*, mControllers.ElementAt(i));
    if (controllerData)
    {
      nsCOMPtr<nsIController> thisController;
      controllerData->GetController(getter_AddRefs(thisController));
      if (thisController.get() == controller)
      {
        *_retval = controllerData->GetControllerID();
    return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;  // none found
}

/* nsIController getControlleryById (in unsigned long controllerID); */
NS_IMETHODIMP
nsXULControllers::GetControlleryById(PRUint32 controllerID, nsIController **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
    
  PRUint32 count = mControllers.Count();
  for (PRUint32 i = 0; i < count; i++)
  {
    nsXULControllerData*  controllerData = NS_STATIC_CAST(nsXULControllerData*, mControllers.ElementAt(i));
    if (controllerData && controllerData->GetControllerID() == controllerID)
    {
      return controllerData->GetController(_retval);
    }
  }
  return NS_ERROR_FAILURE;  // none found
}

NS_IMETHODIMP
nsXULControllers::GetControllerCount(PRUint32 *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = mControllers.Count();
    return NS_OK;
}
