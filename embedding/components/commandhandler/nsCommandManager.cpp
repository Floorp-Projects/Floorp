/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"

#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIObserver.h"

#include "nsIComponentManager.h"

#include "nsServiceManagerUtils.h"
#include "nsIScriptSecurityManager.h"

#include "nsContentUtils.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsIFocusManager.h"

#include "nsCOMArray.h"

#include "nsCommandManager.h"

nsCommandManager::nsCommandManager()
  : mWindow(nullptr)
{
}

nsCommandManager::~nsCommandManager()
{
}

static PLDHashOperator
TraverseCommandObservers(const char* aKey,
                         nsCommandManager::ObserverList* aObservers,
                         void* aClosure)
{
  nsCycleCollectionTraversalCallback* cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aClosure);

  int32_t i, numItems = aObservers->Length();
  for (i = 0; i < numItems; ++i) {
    cb->NoteXPCOMChild(aObservers->ElementAt(i));
  }

  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsCommandManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsCommandManager)
  tmp->mObserversTable.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsCommandManager)
  tmp->mObserversTable.EnumerateRead(TraverseCommandObservers, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCommandManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCommandManager)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCommandManager)
  NS_INTERFACE_MAP_ENTRY(nsICommandManager)
  NS_INTERFACE_MAP_ENTRY(nsPICommandUpdater)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICommandManager)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsCommandManager::Init(nsIDOMWindow* aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);

  NS_ASSERTION(aWindow, "Need non-null window here");
  mWindow = aWindow; // weak ptr
  return NS_OK;
}

NS_IMETHODIMP
nsCommandManager::CommandStatusChanged(const char* aCommandName)
{
  ObserverList* commandObservers;
  mObserversTable.Get(aCommandName, &commandObservers);

  if (commandObservers) {
    // XXX Should we worry about observers removing themselves from Observe()?
    int32_t i, numItems = commandObservers->Length();
    for (i = 0; i < numItems; ++i) {
      nsCOMPtr<nsIObserver> observer = commandObservers->ElementAt(i);
      // should we get the command state to pass here? This might be expensive.
      observer->Observe(NS_ISUPPORTS_CAST(nsICommandManager*, this),
                        aCommandName,
                        MOZ_UTF16("command_status_changed"));
    }
  }

  return NS_OK;
}

#if 0
#pragma mark -
#endif

NS_IMETHODIMP
nsCommandManager::AddCommandObserver(nsIObserver* aCommandObserver,
                                     const char* aCommandToObserve)
{
  NS_ENSURE_ARG(aCommandObserver);

  // XXX todo: handle special cases of aCommandToObserve being null, or empty

  // for each command in the table, we make a list of observers for that command
  ObserverList* commandObservers;
  if (!mObserversTable.Get(aCommandToObserve, &commandObservers)) {
    commandObservers = new ObserverList;
    mObserversTable.Put(aCommandToObserve, commandObservers);
  }

  // need to check that this command observer hasn't already been registered
  int32_t existingIndex = commandObservers->IndexOf(aCommandObserver);
  if (existingIndex == -1) {
    commandObservers->AppendElement(aCommandObserver);
  } else {
    NS_WARNING("Registering command observer twice on the same command");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCommandManager::RemoveCommandObserver(nsIObserver* aCommandObserver,
                                        const char* aCommandObserved)
{
  NS_ENSURE_ARG(aCommandObserver);

  // XXX todo: handle special cases of aCommandToObserve being null, or empty

  ObserverList* commandObservers;
  if (!mObserversTable.Get(aCommandObserved, &commandObservers)) {
    return NS_ERROR_UNEXPECTED;
  }

  commandObservers->RemoveElement(aCommandObserver);

  return NS_OK;
}

NS_IMETHODIMP
nsCommandManager::IsCommandSupported(const char* aCommandName,
                                     nsIDOMWindow* aTargetWindow,
                                     bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsCOMPtr<nsIController> controller;
  GetControllerForCommand(aCommandName, aTargetWindow,
                          getter_AddRefs(controller));
  *aResult = (controller.get() != nullptr);
  return NS_OK;
}

NS_IMETHODIMP
nsCommandManager::IsCommandEnabled(const char* aCommandName,
                                   nsIDOMWindow* aTargetWindow,
                                   bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  bool commandEnabled = false;

  nsCOMPtr<nsIController> controller;
  GetControllerForCommand(aCommandName, aTargetWindow,
                          getter_AddRefs(controller));
  if (controller) {
    controller->IsCommandEnabled(aCommandName, &commandEnabled);
  }
  *aResult = commandEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsCommandManager::GetCommandState(const char* aCommandName,
                                  nsIDOMWindow* aTargetWindow,
                                  nsICommandParams* aCommandParams)
{
  nsCOMPtr<nsIController> controller;
  nsAutoString tValue;
  nsresult rv = GetControllerForCommand(aCommandName, aTargetWindow,
                                        getter_AddRefs(controller));
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsICommandController> commandController =
    do_QueryInterface(controller);
  if (commandController) {
    rv = commandController->GetCommandStateWithParams(aCommandName,
                                                      aCommandParams);
  } else {
    rv = NS_ERROR_NOT_IMPLEMENTED;
  }
  return rv;
}

NS_IMETHODIMP
nsCommandManager::DoCommand(const char* aCommandName,
                            nsICommandParams* aCommandParams,
                            nsIDOMWindow* aTargetWindow)
{
  nsCOMPtr<nsIController> controller;
  nsresult rv = GetControllerForCommand(aCommandName, aTargetWindow,
                                        getter_AddRefs(controller));
  if (!controller) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsICommandController> commandController =
    do_QueryInterface(controller);
  if (commandController && aCommandParams) {
    rv = commandController->DoCommandWithParams(aCommandName, aCommandParams);
  } else {
    rv = controller->DoCommand(aCommandName);
  }
  return rv;
}

nsresult
nsCommandManager::GetControllerForCommand(const char* aCommand,
                                          nsIDOMWindow* aTargetWindow,
                                          nsIController** aResult)
{
  nsresult rv = NS_ERROR_FAILURE;
  *aResult = nullptr;

  // check if we're in content or chrome
  // if we're not chrome we must have a target window or we bail
  if (!nsContentUtils::IsCallerChrome()) {
    if (!aTargetWindow) {
      return rv;
    }

    // if a target window is specified, it must be the window we expect
    if (aTargetWindow != mWindow) {
      return NS_ERROR_FAILURE;
    }
  }

  if (aTargetWindow) {
    // get the controller for this particular window
    nsCOMPtr<nsIControllers> controllers;
    rv = aTargetWindow->GetControllers(getter_AddRefs(controllers));
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (!controllers) {
      return NS_ERROR_FAILURE;
    }

    // dispatch the command
    return controllers->GetControllerForCommand(aCommand, aResult);
  }

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mWindow));
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  nsCOMPtr<nsPIWindowRoot> root = window->GetTopWindowRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

  // no target window; send command to focus controller
  return root->GetControllerForCommand(aCommand, aResult);
}
