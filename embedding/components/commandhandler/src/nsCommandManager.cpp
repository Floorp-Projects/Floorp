/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsString.h"

#include "nsIController.h"
#include "nsIObserver.h"
#include "nsIObserverList.h"

#include "nsIComponentManager.h"

#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIFocusController.h"

#include "nsCommandManager.h"


nsCommandManager::nsCommandManager()
: mWindow(nsnull)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsCommandManager::~nsCommandManager()
{
  /* destructor code */
}


NS_IMPL_ISUPPORTS3(nsCommandManager, nsICommandManager, nsPICommandUpdater, nsISupportsWeakReference)

#if 0
#pragma mark -
#endif

/* void init (in nsIDOMWindow aWindow); */
NS_IMETHODIMP
nsCommandManager::Init(nsIDOMWindow *aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);
  
  NS_ASSERTION(aWindow, "Need non-null window here");
  mWindow = aWindow;      // weak ptr
  return NS_OK;
}

/* void commandStatusChanged (in DOMString aCommandName, in long aChangeFlags); */
NS_IMETHODIMP
nsCommandManager::CommandStatusChanged(const nsAReadableString & aCommandName, PRInt32 aChangeFlags)
{
	nsStringKey hashKey(aCommandName);

  nsPromiseFlatString flatCommand = PromiseFlatString(aCommandName);
  
  nsresult rv = NS_OK;
	nsCOMPtr<nsISupports>      commandSupports  = getter_AddRefs(mCommandObserversTable.Get(&hashKey));
	nsCOMPtr<nsIObserverList>  commandObservers = do_QueryInterface(commandSupports);
	if (commandObservers)
	{	
	  nsCOMPtr<nsIEnumerator>   observers;	
	  rv = commandObservers->EnumerateObserverList(getter_AddRefs(observers));
	  if (observers)
	  {
      rv = observers->First();

      // Continue until error or end of list.
      while (observers->IsDone() != NS_OK && NS_SUCCEEDED(rv))
      {
        nsCOMPtr<nsISupports>   itemSupports;
        rv = observers->CurrentItem(getter_AddRefs(itemSupports));
        nsCOMPtr<nsIObserver>   itemObserver = do_QueryInterface(itemSupports);
        if (itemObserver)
        {
        	// should we get the command state to pass here? This might be expensive.
          itemObserver->Observe((nsICommandManager *)this, flatCommand.get(), NS_LITERAL_STRING("").get());
        }
      
        rv = observers->Next();
      }
      	  
	  }
	}

  return NS_OK;
}

#if 0
#pragma mark -
#endif

/* void addCommandObserver (in nsIObserver aCommandObserver, in wstring aCommandToObserve); */
NS_IMETHODIMP
nsCommandManager::AddCommandObserver(nsIObserver *aCommandObserver, const nsAReadableString & aCommandToObserve)
{
	NS_ENSURE_ARG(aCommandObserver);
	
	nsresult rv = NS_OK;

	// XXX todo: handle special cases of aCommandToObserve being null, or empty
	
	// for each command in the table, we make a list of observers for that command
	nsStringKey hashKey(aCommandToObserve);
	
	nsCOMPtr<nsISupports>      commandSupports  = getter_AddRefs(mCommandObserversTable.Get(&hashKey));
	nsCOMPtr<nsIObserverList>  commandObservers = do_QueryInterface(commandSupports);
	if (!commandObservers)
	{
		commandObservers = do_CreateInstance("@mozilla.org/xpcom/observer-list;1", &rv);
  	if (NS_FAILED(rv)) return rv;
  	
  	rv = mCommandObserversTable.Put(&hashKey, commandObservers);
  	if (NS_FAILED(rv)) return rv;
	}
	
	return commandObservers->AddObserver(aCommandObserver);
}

/* void removeCommandObserver (in nsIObserver aCommandObserver, in wstring aCommandObserved); */
NS_IMETHODIMP
nsCommandManager::RemoveCommandObserver(nsIObserver *aCommandObserver, const nsAReadableString & aCommandObserved)
{
	NS_ENSURE_ARG(aCommandObserver);

	// XXX todo: handle special cases of aCommandToObserve being null, or empty
	nsStringKey hashKey(aCommandObserved);

	nsCOMPtr<nsISupports>      commandSupports  = getter_AddRefs(mCommandObserversTable.Get(&hashKey));
	nsCOMPtr<nsIObserverList>  commandObservers = do_QueryInterface(commandSupports);
	if (!commandObservers)
	  return NS_ERROR_UNEXPECTED;
	
	return commandObservers->RemoveObserver(aCommandObserver);
}

/* boolean isCommandSupported (in wstring aCommandName); */
NS_IMETHODIMP
nsCommandManager::IsCommandSupported(const nsAReadableString & aCommandName, PRBool *outCommandSupported)
{
  NS_ENSURE_ARG_POINTER(outCommandSupported);

  nsCOMPtr<nsIController> controller;
  nsresult rv = GetControllerForCommand(aCommandName, getter_AddRefs(controller)); 
  *outCommandSupported = (controller.get() != nsnull);
  return NS_OK;
}

/* boolean isCommandEnabled (in wstring aCommandName); */
NS_IMETHODIMP
nsCommandManager::IsCommandEnabled(const nsAReadableString & aCommandName, PRBool *outCommandEnabled)
{
  NS_ENSURE_ARG_POINTER(outCommandEnabled);
  
  PRBool  commandEnabled = PR_FALSE;
  
  nsCOMPtr<nsIController> controller;
  nsresult rv = GetControllerForCommand(aCommandName, getter_AddRefs(controller)); 
  if (controller)
  {
    controller->IsCommandEnabled(aCommandName, &commandEnabled);
  }
  *outCommandEnabled = commandEnabled;
  return NS_OK;
}

/* void doCommand (in wstring aCommandName, in wstring aCommandParams); */
NS_IMETHODIMP
nsCommandManager::DoCommand(const nsAReadableString & aCommandName)
{
  nsCOMPtr<nsIController> controller;
  nsresult rv = GetControllerForCommand(aCommandName, getter_AddRefs(controller)); 
  if (!controller)
    return NS_ERROR_FAILURE;
    
  return controller->DoCommand(aCommandName);
}

#ifdef XP_MAC
#pragma mark -
#endif

NS_IMETHODIMP
nsCommandManager::GetControllerForCommand(const nsAReadableString& aCommand, nsIController** outController)
{
  nsresult rv = NS_ERROR_FAILURE;
  
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mWindow);
  if (!window)
    return NS_ERROR_FAILURE;
    
  nsCOMPtr<nsIFocusController> focusController;
  rv = window->GetRootFocusController(getter_AddRefs(focusController));
  if (!focusController)
    return NS_ERROR_FAILURE;

  return focusController->GetControllerForCommand(aCommand, getter_AddRefs(outController));
}

