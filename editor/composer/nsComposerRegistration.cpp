/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EditorSpellCheck.h"   // for NS_EDITORSPELLCHECK_CID, etc
#include "mozilla/HTMLEditorController.h" // for HTMLEditorController, etc
#include "mozilla/Module.h"             // for Module, Module::CIDEntry, etc
#include "mozilla/ModuleUtils.h"
#include "mozilla/mozalloc.h"           // for operator new
#include "nsCOMPtr.h"                   // for nsCOMPtr, getter_AddRefs, etc
#include "nsBaseCommandController.h"    // for nsBaseCommandController
#include "nsComponentManagerUtils.h"    // for do_CreateInstance
#include "nsDebug.h"                    // for NS_ENSURE_SUCCESS
#include "nsError.h"                    // for NS_ERROR_NO_AGGREGATION, etc
#include "nsIController.h"              // for nsIController
#include "nsIControllerContext.h"       // for nsIControllerContext
#include "nsID.h"                       // for NS_DEFINE_NAMED_CID, etc
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_RELEASE
#include "nsControllerCommandTable.h"   // for nsControllerCommandTable, etc
#include "nsServiceManagerUtils.h"      // for do_GetService
#include "nscore.h"                     // for nsresult

using mozilla::EditorSpellCheck;

class nsISupports;

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//

NS_GENERIC_FACTORY_CONSTRUCTOR(EditorSpellCheck)

// Constructor for a controller set up with a command table specified
// by the CID passed in. This function uses do_GetService to get the
// command table, so that every controller shares a single command
// table, for space-efficiency.
//
// The only reason to go via the service manager for the command table
// is that it holds onto the singleton for us, avoiding static variables here.
static nsresult
CreateControllerWithSingletonCommandTable(
    already_AddRefed<nsIControllerCommandTable> aComposerCommandTable,
    nsIController **aResult)
{
  nsCOMPtr<nsIController> controller = new nsBaseCommandController();

  nsCOMPtr<nsIControllerCommandTable> composerCommandTable(aComposerCommandTable);

  // this guy is a singleton, so make it immutable
  composerCommandTable->MakeImmutable();

  nsresult rv;
  nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = controllerContext->Init(composerCommandTable);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = controller;
  NS_ADDREF(*aResult);
  return NS_OK;
}


// Here we make an instance of the controller that holds doc state commands.
// We set it up with a singleton command table.
static nsresult
nsHTMLEditorDocStateControllerConstructor(nsISupports *aOuter, REFNSIID aIID,
                                              void **aResult)
{
  nsCOMPtr<nsIController> controller;
  nsresult rv = CreateControllerWithSingletonCommandTable(
                  nsControllerCommandTable::CreateHTMLEditorDocStateCommandTable(),
                  getter_AddRefs(controller));
  NS_ENSURE_SUCCESS(rv, rv);

  return controller->QueryInterface(aIID, aResult);
}

NS_DEFINE_NAMED_CID(NS_EDITORDOCSTATECONTROLLER_CID);
NS_DEFINE_NAMED_CID(NS_EDITORSPELLCHECK_CID);

static const mozilla::Module::CIDEntry kComposerCIDs[] = {
  { &kNS_EDITORDOCSTATECONTROLLER_CID, false, nullptr, nsHTMLEditorDocStateControllerConstructor },
  { &kNS_EDITORSPELLCHECK_CID, false, nullptr, EditorSpellCheckConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kComposerContracts[] = {
  { "@mozilla.org/editor/editordocstatecontroller;1", &kNS_EDITORDOCSTATECONTROLLER_CID },
  { "@mozilla.org/editor/editorspellchecker;1", &kNS_EDITORSPELLCHECK_CID },
  { nullptr }
};

static const mozilla::Module kComposerModule = {
  mozilla::Module::kVersion,
  kComposerCIDs,
  kComposerContracts
};

NSMODULE_DEFN(nsComposerModule) = &kComposerModule;
