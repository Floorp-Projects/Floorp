/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsString.h"
#include "nsControllerCommandManager.h"

// prototype;
nsresult
NS_NewControllerCommandManager(nsIControllerCommandManager** aResult);


// this value is used to size the hash table. Just a sensible upper bound
#define NUM_COMMANDS_BOUNDS       64


nsControllerCommandManager::nsControllerCommandManager()
: mCommandsTable(NUM_COMMANDS_BOUNDS, PR_FALSE)
{
  NS_INIT_REFCNT();
}


nsControllerCommandManager::~nsControllerCommandManager()
{

}

NS_IMPL_ISUPPORTS2(nsControllerCommandManager, nsIControllerCommandManager, nsISupportsWeakReference);


NS_IMETHODIMP
nsControllerCommandManager::RegisterCommand(const nsAString & aCommandName, nsIControllerCommand *aCommand)
{
  nsStringKey commandKey(aCommandName);
  
  if (mCommandsTable.Put (&commandKey, aCommand))
  {
#if DEBUG
    NS_WARNING("Replacing existing command -- ");
#endif
  }  
  return NS_OK;
}


NS_IMETHODIMP
nsControllerCommandManager::UnregisterCommand(const nsAString & aCommandName, nsIControllerCommand *aCommand)
{
  nsStringKey commandKey(aCommandName);

  PRBool wasRemoved = mCommandsTable.Remove (&commandKey);
  return wasRemoved ? NS_OK : NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsControllerCommandManager::FindCommandHandler(const nsAString & aCommandName, nsIControllerCommand **outCommand)
{
  NS_ENSURE_ARG_POINTER(outCommand);
  
  *outCommand = NULL;
  
  nsStringKey commandKey(aCommandName);
  nsISupports* foundCommand = mCommandsTable.Get(&commandKey);   // this does the addref
  if (!foundCommand) return NS_ERROR_FAILURE;
  
  *outCommand = NS_REINTERPRET_CAST(nsIControllerCommand*, foundCommand);
  return NS_OK;
}



/* boolean isCommandEnabled (in wstring command); */
NS_IMETHODIMP
nsControllerCommandManager::IsCommandEnabled(const nsAString & aCommandName, nsISupports *aCommandRefCon, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = PR_FALSE;
      
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  FindCommandHandler(aCommandName, getter_AddRefs(commandHandler));  
  if (!commandHandler) 
  {
#if DEBUG
    NS_WARNING("Controller command manager asked about a command that it does not handle -- ");
#endif
    return NS_OK;    // we don't handle this command
  }
  
  return commandHandler->IsCommandEnabled(aCommandName, aCommandRefCon, aResult);
}


NS_IMETHODIMP
nsControllerCommandManager::UpdateCommandState(const nsAString & aCommandName, nsISupports *aCommandRefCon)
{
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  FindCommandHandler(aCommandName, getter_AddRefs(commandHandler));  
  if (!commandHandler)
  {
#if DEBUG
    NS_WARNING("Controller command manager asked to update the state of a command that it does not handle -- ");
#endif
    return NS_OK;    // we don't handle this command
  }
  
  nsCOMPtr<nsIStateUpdatingControllerCommand> stateCommand = do_QueryInterface(commandHandler);
  if (!stateCommand)
  {
#if DEBUG
    NS_WARNING("Controller command manager asked to update the state of a command that doesn't do state updating -- ");
#endif
    return NS_ERROR_NO_INTERFACE;
  }
  
  return stateCommand->UpdateCommandState(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
nsControllerCommandManager::SupportsCommand(const nsAString & aCommandName, nsISupports *aCommandRefCon, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  // XXX: need to check the readonly and disabled states

  *aResult = PR_FALSE;
  
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  FindCommandHandler(aCommandName, getter_AddRefs(commandHandler));

  *aResult = (commandHandler.get() != nsnull);
  return NS_OK;
}

/* void doCommand (in wstring command); */
NS_IMETHODIMP
nsControllerCommandManager::DoCommand(const nsAString & aCommandName, nsISupports *aCommandRefCon)
{
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
 FindCommandHandler(aCommandName, getter_AddRefs(commandHandler));
  if (!commandHandler)
  {
#if DEBUG
    NS_WARNING("Controller command manager asked to do a command that it does not handle -- ");
#endif
    return NS_OK;    // we don't handle this command
  }
  
  return commandHandler->DoCommand(aCommandName, aCommandRefCon);
}

#define COMMAND_NAME NS_ConvertASCIItoUCS2("cmd_name")

NS_IMETHODIMP
nsControllerCommandManager::DoCommandParams(nsICommandParams *aParams, nsISupports *aCommandRefCon)
{
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  nsAutoString tValue;
  nsresult rv;
  if (NS_SUCCEEDED(rv = aParams->GetStringValue(COMMAND_NAME,tValue)))
  {
    FindCommandHandler(tValue, getter_AddRefs(commandHandler));
    if (!commandHandler)
    {
  #if DEBUG
      NS_WARNING("Controller command manager asked to do a command that it does not handle -- ");
  #endif
      return NS_OK;    // we don't handle this command
    }
    return commandHandler->DoCommandParams(aParams, aCommandRefCon);
  }  
  return rv;
}


NS_IMETHODIMP
nsControllerCommandManager::GetCommandState(nsICommandParams *aParams, nsISupports *aCommandRefCon)
{
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  nsAutoString tValue;
  nsresult rv;
  if (NS_SUCCEEDED(rv = aParams->GetStringValue(COMMAND_NAME,tValue)))
  {
    FindCommandHandler(tValue, getter_AddRefs(commandHandler));
    if (!commandHandler)
    {
  #if DEBUG
      NS_WARNING("Controller command manager asked to do a command that it does not handle -- ");
  #endif
      return NS_OK;    // we don't handle this command
    }
    return commandHandler->GetCommandState(aParams, aCommandRefCon);
  }  
  return rv;
}


nsresult
NS_NewControllerCommandManager(nsIControllerCommandManager** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsControllerCommandManager* newCommandManager = new nsControllerCommandManager();
  if (! newCommandManager)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(newCommandManager);
  *aResult = newCommandManager;
  return NS_OK;
}



