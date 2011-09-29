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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsIComponentManager.h"
#include "nsBaseCommandController.h"

#include "nsHashtable.h"
#include "nsString.h"
#include "nsWeakPtr.h"

NS_IMPL_ADDREF(nsBaseCommandController)
NS_IMPL_RELEASE(nsBaseCommandController)

NS_INTERFACE_MAP_BEGIN(nsBaseCommandController)
  NS_INTERFACE_MAP_ENTRY(nsIController)
  NS_INTERFACE_MAP_ENTRY(nsICommandController)
  NS_INTERFACE_MAP_ENTRY(nsIControllerContext)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIControllerContext)
NS_INTERFACE_MAP_END

nsBaseCommandController::nsBaseCommandController()
  : mCommandContextRawPtr(nsnull)
{
}

nsBaseCommandController::~nsBaseCommandController()
{
}

NS_IMETHODIMP
nsBaseCommandController::Init(nsIControllerCommandTable *aCommandTable)
{
  nsresult rv = NS_OK;

  if (aCommandTable)
    mCommandTable = aCommandTable;    // owning addref
  else
    mCommandTable = do_CreateInstance(NS_CONTROLLERCOMMANDTABLE_CONTRACTID, &rv);
  
  return rv;
}

NS_IMETHODIMP
nsBaseCommandController::SetCommandContext(nsISupports *aCommandContext)
{
  mCommandContextWeakPtr = nsnull;
  mCommandContextRawPtr = nsnull;

  if (aCommandContext) {
    nsCOMPtr<nsISupportsWeakReference> weak = do_QueryInterface(aCommandContext);
    if (weak) {
      nsresult rv =
        weak->GetWeakReference(getter_AddRefs(mCommandContextWeakPtr));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      mCommandContextRawPtr = aCommandContext;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBaseCommandController::GetInterface(const nsIID & aIID, void * *result)
{
  NS_ENSURE_ARG_POINTER(result);

  if (NS_SUCCEEDED(QueryInterface(aIID, result)))
    return NS_OK;

  if (aIID.Equals(NS_GET_IID(nsIControllerCommandTable)))
  {
    if (mCommandTable)
      return mCommandTable->QueryInterface(aIID, result);
    return NS_ERROR_NOT_INITIALIZED;
  }
    
  return NS_NOINTERFACE;
}



/* =======================================================================
 * nsIController
 * ======================================================================= */

NS_IMETHODIMP
nsBaseCommandController::IsCommandEnabled(const char *aCommand,
                                          bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_STATE(mCommandTable);

  nsISupports* context = mCommandContextRawPtr;
  nsCOMPtr<nsISupports> weak;
  if (!context) {
    weak = do_QueryReferent(mCommandContextWeakPtr);
    context = weak;
  }
  return mCommandTable->IsCommandEnabled(aCommand, context, aResult);
}

NS_IMETHODIMP
nsBaseCommandController::SupportsCommand(const char *aCommand, bool *aResult)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_STATE(mCommandTable);

  nsISupports* context = mCommandContextRawPtr;
  nsCOMPtr<nsISupports> weak;
  if (!context) {
    weak = do_QueryReferent(mCommandContextWeakPtr);
    context = weak;
  }
  return mCommandTable->SupportsCommand(aCommand, context, aResult);
}

NS_IMETHODIMP
nsBaseCommandController::DoCommand(const char *aCommand)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  NS_ENSURE_STATE(mCommandTable);

  nsISupports* context = mCommandContextRawPtr;
  nsCOMPtr<nsISupports> weak;
  if (!context) {
    weak = do_QueryReferent(mCommandContextWeakPtr);
    context = weak;
  }
  return mCommandTable->DoCommand(aCommand, context);
}

NS_IMETHODIMP
nsBaseCommandController::DoCommandWithParams(const char *aCommand,
                                             nsICommandParams *aParams)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  NS_ENSURE_STATE(mCommandTable);

  nsISupports* context = mCommandContextRawPtr;
  nsCOMPtr<nsISupports> weak;
  if (!context) {
    weak = do_QueryReferent(mCommandContextWeakPtr);
    context = weak;
  }
  return mCommandTable->DoCommandParams(aCommand, aParams, context);
}

NS_IMETHODIMP
nsBaseCommandController::GetCommandStateWithParams(const char *aCommand,
                                                   nsICommandParams *aParams)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  NS_ENSURE_STATE(mCommandTable);

  nsISupports* context = mCommandContextRawPtr;
  nsCOMPtr<nsISupports> weak;
  if (!context) {
    weak = do_QueryReferent(mCommandContextWeakPtr);
    context = weak;
  }
  return mCommandTable->GetCommandState(aCommand, aParams, context);
}

NS_IMETHODIMP
nsBaseCommandController::OnEvent(const char * aEventName)
{
  NS_ENSURE_ARG_POINTER(aEventName);
  return NS_OK;
}
