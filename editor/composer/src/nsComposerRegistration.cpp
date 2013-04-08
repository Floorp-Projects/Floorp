/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stddef.h>                     // for nullptr

#include "mozilla/Module.h"             // for Module, Module::CIDEntry, etc
#include "mozilla/ModuleUtils.h"
#include "mozilla/mozalloc.h"           // for operator new
#include "nsCOMPtr.h"                   // for nsCOMPtr, getter_AddRefs, etc
#include "nsComponentManagerUtils.h"    // for do_CreateInstance
#include "nsComposeTxtSrvFilter.h"      // for nsComposeTxtSrvFilter, etc
#include "nsComposerController.h"       // for nsComposerController, etc
#include "nsDebug.h"                    // for NS_ENSURE_SUCCESS
#include "nsEditingSession.h"           // for NS_EDITINGSESSION_CID, etc
#include "nsEditorSpellCheck.h"         // for NS_EDITORSPELLCHECK_CID, etc
#include "nsError.h"                    // for NS_ERROR_NO_AGGREGATION, etc
#include "nsIController.h"              // for nsIController
#include "nsIControllerCommandTable.h"  // for nsIControllerCommandTable, etc
#include "nsIControllerContext.h"       // for nsIControllerContext
#include "nsID.h"                       // for NS_DEFINE_NAMED_CID, etc
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_RELEASE
#include "nsServiceManagerUtils.h"      // for do_GetService
#include "nscore.h"                     // for nsresult

class nsISupports;

#define NS_HTMLEDITOR_COMMANDTABLE_CID \
{ 0x13e50d8d, 0x9cee, 0x4ad1, { 0xa3, 0xa2, 0x4a, 0x44, 0x2f, 0xdf, 0x7d, 0xfa } }

#define NS_HTMLEDITOR_DOCSTATE_COMMANDTABLE_CID \
{ 0xa33982d3, 0x1adf, 0x4162, { 0x99, 0x41, 0xf7, 0x34, 0xbc, 0x45, 0xe4, 0xed } }


static NS_DEFINE_CID(kHTMLEditorCommandTableCID, NS_HTMLEDITOR_COMMANDTABLE_CID);
static NS_DEFINE_CID(kHTMLEditorDocStateCommandTableCID, NS_HTMLEDITOR_DOCSTATE_COMMANDTABLE_CID);


////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditingSession)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorSpellCheck)

// There are no macros that enable us to have 2 constructors 
// for the same object
//
// Here we are creating the same object with two different contract IDs
// and then initializing it different.
// Basically, we need to tell the filter whether it is doing mail or not
static nsresult
nsComposeTxtSrvFilterConstructor(nsISupports *aOuter, REFNSIID aIID,
                                 void **aResult, bool aIsForMail)
{
    *aResult = nullptr;
    if (nullptr != aOuter) 
    {
        return NS_ERROR_NO_AGGREGATION;
    }
    nsComposeTxtSrvFilter * inst = new nsComposeTxtSrvFilter();
    if (nullptr == inst) 
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(inst);
	  inst->Init(aIsForMail);
    nsresult rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);
    return rv;
}

static nsresult
nsComposeTxtSrvFilterConstructorForComposer(nsISupports *aOuter, 
                                            REFNSIID aIID,
                                            void **aResult)
{
    return nsComposeTxtSrvFilterConstructor(aOuter, aIID, aResult, false);
}

static nsresult
nsComposeTxtSrvFilterConstructorForMail(nsISupports *aOuter, 
                                        REFNSIID aIID,
                                        void **aResult)
{
    return nsComposeTxtSrvFilterConstructor(aOuter, aIID, aResult, true);
}


// Constructor for a controller set up with a command table specified
// by the CID passed in. This function uses do_GetService to get the
// command table, so that every controller shares a single command
// table, for space-efficiency.
// 
// The only reason to go via the service manager for the command table
// is that it holds onto the singleton for us, avoiding static variables here.
static nsresult
CreateControllerWithSingletonCommandTable(const nsCID& inCommandTableCID, nsIController **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIController> controller = do_CreateInstance("@mozilla.org/embedcomp/base-command-controller;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIControllerCommandTable> composerCommandTable = do_GetService(inCommandTableCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // this guy is a singleton, so make it immutable
  composerCommandTable->MakeImmutable();
  
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
  nsresult rv = CreateControllerWithSingletonCommandTable(kHTMLEditorDocStateCommandTableCID, getter_AddRefs(controller));
  NS_ENSURE_SUCCESS(rv, rv);

  return controller->QueryInterface(aIID, aResult);
}

// Tere we make an instance of the controller that holds composer commands.
// We set it up with a singleton command table.
static nsresult
nsHTMLEditorControllerConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsCOMPtr<nsIController> controller;
  nsresult rv = CreateControllerWithSingletonCommandTable(kHTMLEditorCommandTableCID, getter_AddRefs(controller));
  NS_ENSURE_SUCCESS(rv, rv);

  return controller->QueryInterface(aIID, aResult);
}

// Constructor for a command table that is pref-filled with HTML editor commands
static nsresult
nsHTMLEditorCommandTableConstructor(nsISupports *aOuter, REFNSIID aIID, 
                                              void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIControllerCommandTable> commandTable =
      do_CreateInstance(NS_CONTROLLERCOMMANDTABLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = nsComposerController::RegisterHTMLEditorCommands(commandTable);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // we don't know here whether we're being created as an instance,
  // or a service, so we can't become immutable
  
  return commandTable->QueryInterface(aIID, aResult);
}


// Constructor for a command table that is pref-filled with HTML editor doc state commands
static nsresult
nsHTMLEditorDocStateCommandTableConstructor(nsISupports *aOuter, REFNSIID aIID, 
                                              void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIControllerCommandTable> commandTable =
      do_CreateInstance(NS_CONTROLLERCOMMANDTABLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = nsComposerController::RegisterEditorDocStateCommands(commandTable);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // we don't know here whether we're being created as an instance,
  // or a service, so we can't become immutable
  
  return commandTable->QueryInterface(aIID, aResult);
}

NS_DEFINE_NAMED_CID(NS_HTMLEDITORCONTROLLER_CID);
NS_DEFINE_NAMED_CID(NS_EDITORDOCSTATECONTROLLER_CID);
NS_DEFINE_NAMED_CID(NS_HTMLEDITOR_COMMANDTABLE_CID);
NS_DEFINE_NAMED_CID(NS_HTMLEDITOR_DOCSTATE_COMMANDTABLE_CID);
NS_DEFINE_NAMED_CID(NS_EDITINGSESSION_CID);
NS_DEFINE_NAMED_CID(NS_EDITORSPELLCHECK_CID);
NS_DEFINE_NAMED_CID(NS_COMPOSERTXTSRVFILTER_CID);
NS_DEFINE_NAMED_CID(NS_COMPOSERTXTSRVFILTERMAIL_CID);


static const mozilla::Module::CIDEntry kComposerCIDs[] = {
  { &kNS_HTMLEDITORCONTROLLER_CID, false, nullptr, nsHTMLEditorControllerConstructor },
  { &kNS_EDITORDOCSTATECONTROLLER_CID, false, nullptr, nsHTMLEditorDocStateControllerConstructor },
  { &kNS_HTMLEDITOR_COMMANDTABLE_CID, false, nullptr, nsHTMLEditorCommandTableConstructor },
  { &kNS_HTMLEDITOR_DOCSTATE_COMMANDTABLE_CID, false, nullptr, nsHTMLEditorDocStateCommandTableConstructor },
  { &kNS_EDITINGSESSION_CID, false, nullptr, nsEditingSessionConstructor },
  { &kNS_EDITORSPELLCHECK_CID, false, nullptr, nsEditorSpellCheckConstructor },
  { &kNS_COMPOSERTXTSRVFILTER_CID, false, nullptr, nsComposeTxtSrvFilterConstructorForComposer },
  { &kNS_COMPOSERTXTSRVFILTERMAIL_CID, false, nullptr, nsComposeTxtSrvFilterConstructorForMail },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kComposerContracts[] = {
  { "@mozilla.org/editor/htmleditorcontroller;1", &kNS_HTMLEDITORCONTROLLER_CID },
  { "@mozilla.org/editor/editordocstatecontroller;1", &kNS_EDITORDOCSTATECONTROLLER_CID },
  { "@mozilla.org/editor/editingsession;1", &kNS_EDITINGSESSION_CID },
  { "@mozilla.org/editor/editorspellchecker;1", &kNS_EDITORSPELLCHECK_CID },
  { COMPOSER_TXTSRVFILTER_CONTRACTID, &kNS_COMPOSERTXTSRVFILTER_CID },
  { COMPOSER_TXTSRVFILTERMAIL_CONTRACTID, &kNS_COMPOSERTXTSRVFILTERMAIL_CID },
  { nullptr }
};

static const mozilla::Module kComposerModule = {
  mozilla::Module::kVersion,
  kComposerCIDs,
  kComposerContracts
};

NSMODULE_DEFN(nsComposerModule) = &kComposerModule;
