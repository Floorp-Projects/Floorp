/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsString.h"
#include "nsIControllerCommand.h"
#include "nsControllerCommandTable.h"

// prototype;
nsresult
NS_NewControllerCommandTable(nsIControllerCommandTable** aResult);


// this value is used to size the hash table. Just a sensible upper bound
#define NUM_COMMANDS_BOUNDS       64


nsControllerCommandTable::nsControllerCommandTable()
: mCommandsTable(NUM_COMMANDS_BOUNDS, PR_FALSE)
, mMutable(PR_TRUE)
{
}


nsControllerCommandTable::~nsControllerCommandTable()
{
}

NS_IMPL_ISUPPORTS2(nsControllerCommandTable, nsIControllerCommandTable, nsISupportsWeakReference)

NS_IMETHODIMP
nsControllerCommandTable::MakeImmutable(void)
{
  mMutable = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsControllerCommandTable::RegisterCommand(const char * aCommandName, nsIControllerCommand *aCommand)
{
  NS_ENSURE_TRUE(mMutable, NS_ERROR_FAILURE);
  
  nsCStringKey commandKey(aCommandName);
  
  if (mCommandsTable.Put(&commandKey, aCommand))
  {
#if DEBUG
    NS_WARNING("Replacing existing command -- ");
#endif
  }  
  return NS_OK;
}


NS_IMETHODIMP
nsControllerCommandTable::UnregisterCommand(const char * aCommandName, nsIControllerCommand *aCommand)
{
  NS_ENSURE_TRUE(mMutable, NS_ERROR_FAILURE);

  nsCStringKey commandKey(aCommandName);

  bool wasRemoved = mCommandsTable.Remove(&commandKey);
  return wasRemoved ? NS_OK : NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsControllerCommandTable::FindCommandHandler(const char * aCommandName, nsIControllerCommand **outCommand)
{
  NS_ENSURE_ARG_POINTER(outCommand);
  
  *outCommand = NULL;
  
  nsCStringKey commandKey(aCommandName);
  nsISupports* foundCommand = mCommandsTable.Get(&commandKey);
  if (!foundCommand) return NS_ERROR_FAILURE;
  
  // no need to addref since the .Get does it for us
  *outCommand = reinterpret_cast<nsIControllerCommand*>(foundCommand);
  return NS_OK;
}



/* boolean isCommandEnabled (in wstring command); */
NS_IMETHODIMP
nsControllerCommandTable::IsCommandEnabled(const char * aCommandName, nsISupports *aCommandRefCon, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = PR_FALSE;
      
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  FindCommandHandler(aCommandName, getter_AddRefs(commandHandler));  
  if (!commandHandler) 
  {
#if DEBUG
    NS_WARNING("Controller command table asked about a command that it does not handle -- ");
#endif
    return NS_OK;    // we don't handle this command
  }
  
  return commandHandler->IsCommandEnabled(aCommandName, aCommandRefCon, aResult);
}


NS_IMETHODIMP
nsControllerCommandTable::UpdateCommandState(const char * aCommandName, nsISupports *aCommandRefCon)
{
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  FindCommandHandler(aCommandName, getter_AddRefs(commandHandler));  
  if (!commandHandler)
  {
#if DEBUG
    NS_WARNING("Controller command table asked to update the state of a command that it does not handle -- ");
#endif
    return NS_OK;    // we don't handle this command
  }
  
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsControllerCommandTable::SupportsCommand(const char * aCommandName, nsISupports *aCommandRefCon, bool *aResult)
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
nsControllerCommandTable::DoCommand(const char * aCommandName, nsISupports *aCommandRefCon)
{
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  FindCommandHandler(aCommandName, getter_AddRefs(commandHandler));
  if (!commandHandler)
  {
#if DEBUG
    NS_WARNING("Controller command table asked to do a command that it does not handle -- ");
#endif
    return NS_OK;    // we don't handle this command
  }
  
  return commandHandler->DoCommand(aCommandName, aCommandRefCon);
}

NS_IMETHODIMP
nsControllerCommandTable::DoCommandParams(const char *aCommandName, nsICommandParams *aParams, nsISupports *aCommandRefCon)
{
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  nsresult rv;
  rv = FindCommandHandler(aCommandName, getter_AddRefs(commandHandler));
  if (!commandHandler)
  {
#if DEBUG
    NS_WARNING("Controller command table asked to do a command that it does not handle -- ");
#endif
    return NS_OK;    // we don't handle this command
  }
  return commandHandler->DoCommandParams(aCommandName, aParams, aCommandRefCon);
}


NS_IMETHODIMP
nsControllerCommandTable::GetCommandState(const char *aCommandName, nsICommandParams *aParams, nsISupports *aCommandRefCon)
{
  // find the command  
  nsCOMPtr<nsIControllerCommand> commandHandler;
  nsresult rv;
  rv = FindCommandHandler(aCommandName, getter_AddRefs(commandHandler));
  if (!commandHandler)
  {
#if DEBUG
    NS_WARNING("Controller command table asked to do a command that it does not handle -- ");
#endif
    return NS_OK;    // we don't handle this command
  }
  return commandHandler->GetCommandStateParams(aCommandName, aParams, aCommandRefCon);
}


nsresult
NS_NewControllerCommandTable(nsIControllerCommandTable** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsControllerCommandTable* newCommandTable = new nsControllerCommandTable();
  if (! newCommandTable)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(newCommandTable);
  *aResult = newCommandTable;
  return NS_OK;
}



