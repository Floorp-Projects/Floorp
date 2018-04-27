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
#include "nsXULControllers.h"
#include "nsIController.h"

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
  uint32_t count = mControllers.Length();
  for (uint32_t i = 0; i < count; i++)
  {
    nsXULControllerData* controllerData = mControllers.ElementAt(i);
    delete controllerData;    // releases the nsIController
  }

  mControllers.Clear();
}


nsresult
NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aOuter == nullptr, "no aggregation");
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsXULControllers* controllers = new nsXULControllers();
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
    uint32_t i, count = tmp->mControllers.Length();
    for (i = 0; i < count; ++i) {
      nsXULControllerData* controllerData = tmp->mControllers[i];
      if (controllerData) {
        cb.NoteXPCOMChild(controllerData->mController);
      }
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULControllers)
  NS_INTERFACE_MAP_ENTRY(nsIControllers)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIControllers)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULControllers)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULControllers)

NS_IMETHODIMP
nsXULControllers::GetControllerForCommand(const char *aCommand, nsIController** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  uint32_t count = mControllers.Length();
  for (uint32_t i=0; i < count; i++)
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
          controller.forget(_retval);
          return NS_OK;
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::InsertControllerAt(uint32_t aIndex, nsIController *controller)
{
  nsXULControllerData*  controllerData = new nsXULControllerData(++mCurControllerID, controller);
#ifdef DEBUG
  nsXULControllerData** inserted =
#endif
  mControllers.InsertElementAt(aIndex, controllerData);
  NS_ASSERTION(inserted != nullptr, "Insertion of controller failed");
  return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::RemoveControllerAt(uint32_t aIndex, nsIController **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  nsXULControllerData* controllerData = mControllers.SafeElementAt(aIndex);
  if (!controllerData) return NS_ERROR_FAILURE;

  mControllers.RemoveElementAt(aIndex);

  controllerData->GetController(_retval);
  delete controllerData;

  return NS_OK;
}


NS_IMETHODIMP
nsXULControllers::GetControllerAt(uint32_t aIndex, nsIController **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  nsXULControllerData* controllerData = mControllers.SafeElementAt(aIndex);
  if (!controllerData) return NS_ERROR_FAILURE;

  return controllerData->GetController(_retval);   // does the addref
}

NS_IMETHODIMP
nsXULControllers::AppendController(nsIController *controller)
{
  // This assigns controller IDs starting at 1 so we can use 0 to test if an ID was obtained
  nsXULControllerData*  controllerData = new nsXULControllerData(++mCurControllerID, controller);

#ifdef DEBUG
  nsXULControllerData** appended =
#endif
  mControllers.AppendElement(controllerData);
  NS_ASSERTION(appended != nullptr, "Appending controller failed");
  return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::RemoveController(nsIController *controller)
{
  // first get the identity pointer
  nsCOMPtr<nsISupports> controllerSup(do_QueryInterface(controller));
  // then find it
  uint32_t count = mControllers.Length();
  for (uint32_t i = 0; i < count; i++)
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

NS_IMETHODIMP
nsXULControllers::GetControllerId(nsIController *controller, uint32_t *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  uint32_t count = mControllers.Length();
  for (uint32_t i = 0; i < count; i++)
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

NS_IMETHODIMP
nsXULControllers::GetControllerById(uint32_t controllerID, nsIController **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  uint32_t count = mControllers.Length();
  for (uint32_t i = 0; i < count; i++)
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
nsXULControllers::GetControllerCount(uint32_t *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = mControllers.Length();
  return NS_OK;
}

