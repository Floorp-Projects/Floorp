/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

class nsISupports;

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//

static const mozilla::Module::CIDEntry kComposerCIDs[] = {
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kComposerContracts[] = {
  { nullptr }
};

static const mozilla::Module kComposerModule = {
  mozilla::Module::kVersion,
  kComposerCIDs,
  kComposerContracts
};

NSMODULE_DEFN(nsComposerModule) = &kComposerModule;
