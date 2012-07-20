/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  This file provides the implementation for the XUL "controllers"
  object.

*/

#include "nsString.h"

#include "nsIControllers.h"
#include "nsIDOMElement.h"
#include "nsXULControllers.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"

//----------------------------------------------------------------------

nsXULControllers::nsXULControllers()
: mCurControllerID(0)
{
}

nsXULControllers::~nsXULControllers(void)
{
  DeleteControllers();
}

void
nsXULControllers::DeleteControllers()
{
  PRUint32 count = mControllers.Length();
  for (PRUint32 i = 0; i < count; i++)
  {
    nsXULControllerData* controllerData = mControllers.ElementAt(i);
    delete controllerData;    // releases the nsIController
  }
  
  mControllers.Clear();
}


nsresult
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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULControllers)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULControllers)
  tmp->DeleteControllers();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULControllers)
  {
    PRUint32 i, count = tmp->mControllers.Length();
    for (i = 0; i < count; ++i) {
      nsXULControllerData* controllerData = tmp->mControllers[i];
      if (controllerData) {
        cb.NoteXPCOMChild(controllerData->mController);
      }
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(XULControllers, nsXULControllers)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULControllers)
  NS_INTERFACE_MAP_ENTRY(nsIControllers)
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIControllers)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XULControllers)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULControllers)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULControllers)

NS_IMETHODIMP
nsXULControllers::GetControllerForCommand(const char *aCommand, nsIController** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  PRUint32 count = mControllers.Length();
  for (PRUint32 i=0; i < count; i++)
  {
    nsXULControllerData* controllerData = mControllers.ElementAt(i);
    if (controllerData)
    {
      nsCOMPtr<nsIController> controller;
      controllerData->GetController(getter_AddRefs(controller));
      if (controller)
      {
        bool supportsCommand;
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
nsXULControllers::InsertControllerAt(PRUint32 aIndex, nsIController *controller)
{
  nsXULControllerData*  controllerData = new nsXULControllerData(++mCurControllerID, controller);
  if (!controllerData) return NS_ERROR_OUT_OF_MEMORY;
#ifdef DEBUG
  nsXULControllerData** inserted =
#endif
  mControllers.InsertElementAt(aIndex, controllerData);
  NS_ASSERTION(inserted != nsnull, "Insertion of controller failed");
  return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::RemoveControllerAt(PRUint32 aIndex, nsIController **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  nsXULControllerData* controllerData = mControllers.SafeElementAt(aIndex);
  if (!controllerData) return NS_ERROR_FAILURE;

  mControllers.RemoveElementAt(aIndex);

  controllerData->GetController(_retval);
  delete controllerData;
  
  return NS_OK;
}


NS_IMETHODIMP
nsXULControllers::GetControllerAt(PRUint32 aIndex, nsIController **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  nsXULControllerData* controllerData = mControllers.SafeElementAt(aIndex);
  if (!controllerData) return NS_ERROR_FAILURE;

  return controllerData->GetController(_retval);   // does the addref  
}

NS_IMETHODIMP
nsXULControllers::AppendController(nsIController *controller)
{
  // This assigns controller IDs starting at 1 so we can use 0 to test if an ID was obtained
  nsXULControllerData*  controllerData = new nsXULControllerData(++mCurControllerID, controller);
  if (!controllerData) return NS_ERROR_OUT_OF_MEMORY;

#ifdef DEBUG
  nsXULControllerData** appended =
#endif
  mControllers.AppendElement(controllerData);
  NS_ASSERTION(appended != nsnull, "Appending controller failed");
  return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::RemoveController(nsIController *controller)
{
  // first get the identity pointer
  nsCOMPtr<nsISupports> controllerSup(do_QueryInterface(controller));
  // then find it
  PRUint32 count = mControllers.Length();
  for (PRUint32 i = 0; i < count; i++)
  {
    nsXULControllerData* controllerData = mControllers.ElementAt(i);
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

  PRUint32 count = mControllers.Length();
  for (PRUint32 i = 0; i < count; i++)
  {
    nsXULControllerData* controllerData = mControllers.ElementAt(i);
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

/* nsIController getControllerById (in unsigned long controllerID); */
NS_IMETHODIMP
nsXULControllers::GetControllerById(PRUint32 controllerID, nsIController **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
    
  PRUint32 count = mControllers.Length();
  for (PRUint32 i = 0; i < count; i++)
  {
    nsXULControllerData* controllerData = mControllers.ElementAt(i);
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
  *_retval = mControllers.Length();
  return NS_OK;
}

// nsISecurityCheckedComponent implementation

static char* cloneAllAccess()
{
  static const char allAccess[] = "AllAccess";
  return (char*)nsMemory::Clone(allAccess, sizeof(allAccess));
}

static char* cloneUniversalXPConnect()
{
  static const char universalXPConnect[] = "UniversalXPConnect";
  return (char*)nsMemory::Clone(universalXPConnect, sizeof(universalXPConnect));
}

NS_IMETHODIMP
nsXULControllers::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  *_retval = cloneAllAccess();
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsXULControllers::CanCallMethod(const nsIID * iid, const PRUnichar *methodName,
                                char **_retval)
{
  // OK if you're cool enough
  *_retval = cloneUniversalXPConnect();
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsXULControllers::CanGetProperty(const nsIID * iid,
                                 const PRUnichar *propertyName,
                                 char **_retval)
{
  // OK if you're cool enough
  *_retval = cloneUniversalXPConnect();
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP
nsXULControllers::CanSetProperty(const nsIID * iid,
                                 const PRUnichar *propertyName,
                                 char **_retval)
{
  // OK if you're cool enough
  *_retval = cloneUniversalXPConnect();
  return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
