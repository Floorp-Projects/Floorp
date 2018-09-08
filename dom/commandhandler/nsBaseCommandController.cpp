/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsBaseCommandController.h"
#include "nsControllerCommandTable.h"

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
  : mCommandContextRawPtr(nullptr)
{
}

nsBaseCommandController::~nsBaseCommandController()
{
}

NS_IMETHODIMP
nsBaseCommandController::Init(nsIControllerCommandTable* aCommandTable)
{
  if (aCommandTable) {
    mCommandTable = aCommandTable;
  } else {
    mCommandTable = new nsControllerCommandTable();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBaseCommandController::SetCommandContext(nsISupports* aCommandContext)
{
  mCommandContextWeakPtr = nullptr;
  mCommandContextRawPtr = nullptr;

  if (aCommandContext) {
    nsCOMPtr<nsISupportsWeakReference> weak = do_QueryInterface(aCommandContext);
    if (weak) {
      nsresult rv =
        weak->GetWeakReference(getter_AddRefs(mCommandContextWeakPtr));
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      mCommandContextRawPtr = aCommandContext;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBaseCommandController::GetInterface(const nsIID& aIID, void** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (NS_SUCCEEDED(QueryInterface(aIID, aResult))) {
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIControllerCommandTable))) {
    if (mCommandTable) {
      return mCommandTable->QueryInterface(aIID, aResult);
    }
    return NS_ERROR_NOT_INITIALIZED;
  }

  return NS_NOINTERFACE;
}

/* =======================================================================
 * nsIController
 * ======================================================================= */

NS_IMETHODIMP
nsBaseCommandController::IsCommandEnabled(const char* aCommand, bool* aResult)
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
nsBaseCommandController::SupportsCommand(const char* aCommand, bool* aResult)
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
nsBaseCommandController::DoCommand(const char* aCommand)
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
nsBaseCommandController::DoCommandWithParams(const char* aCommand,
                                             nsICommandParams* aParams)
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
nsBaseCommandController::GetCommandStateWithParams(const char* aCommand,
                                                   nsICommandParams* aParams)
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
nsBaseCommandController::OnEvent(const char* aEventName)
{
  NS_ENSURE_ARG_POINTER(aEventName);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseCommandController::GetSupportedCommands(uint32_t* aCount,
                                              char*** aCommands)
{
  NS_ENSURE_STATE(mCommandTable);
  return mCommandTable->GetSupportedCommands(aCount, aCommands);
}

already_AddRefed<nsIController>
nsBaseCommandController::CreateWindowController()
{
  nsCOMPtr<nsIController> controller = new nsBaseCommandController();

  nsCOMPtr<nsIControllerCommandTable> windowCommandTable =
    nsControllerCommandTable::CreateWindowCommandTable();

  // this is a singleton; make it immutable
  windowCommandTable->MakeImmutable();

  nsresult rv;
  nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller, &rv);
  if (NS_FAILED(rv)) return nullptr;

  rv = controllerContext->Init(windowCommandTable);
  if (NS_FAILED(rv)) return nullptr;

  return controller.forget();
}

already_AddRefed<nsIController>
nsBaseCommandController::CreateEditorController()
{
  nsCOMPtr<nsIController> controller = new nsBaseCommandController();

  nsCOMPtr<nsIControllerCommandTable> editorCommandTable =
    nsControllerCommandTable::CreateEditorCommandTable();

  // this guy is a singleton, so make it immutable
  editorCommandTable->MakeImmutable();

  nsresult rv;
  nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller, &rv);
  if (NS_FAILED(rv)) return nullptr;

  rv = controllerContext->Init(editorCommandTable);
  if (NS_FAILED(rv)) return nullptr;

  return controller.forget();
}

already_AddRefed<nsIController>
nsBaseCommandController::CreateEditingController()
{
  nsCOMPtr<nsIController> controller = new nsBaseCommandController();

  nsCOMPtr<nsIControllerCommandTable> editingCommandTable =
    nsControllerCommandTable::CreateEditingCommandTable();

  // this guy is a singleton, so make it immutable
  editingCommandTable->MakeImmutable();

  nsresult rv;
  nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller, &rv);
  if (NS_FAILED(rv)) return nullptr;

  rv = controllerContext->Init(editingCommandTable);
  if (NS_FAILED(rv)) return nullptr;

  return controller.forget();
}

already_AddRefed<nsIController>
nsBaseCommandController::CreateHTMLEditorController()
{
  nsCOMPtr<nsIController> controller = new nsBaseCommandController();

  nsCOMPtr<nsIControllerCommandTable> composerCommandTable =
    nsControllerCommandTable::CreateHTMLEditorCommandTable();

  // this guy is a singleton, so make it immutable
  composerCommandTable->MakeImmutable();

  nsresult rv;
  nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = controllerContext->Init(composerCommandTable);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return controller.forget();
}

