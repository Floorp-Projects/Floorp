/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stddef.h>

#include "mozilla/Module.h"
#include "mozilla/ModuleUtils.h"
#include "nsID.h"
#include "nsITransactionManager.h"
#include "nsTransactionManager.h"
#include "nsTransactionManagerCID.h"

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTransactionManager)
NS_DEFINE_NAMED_CID(NS_TRANSACTIONMANAGER_CID);

static const mozilla::Module::CIDEntry kTxMgrCIDs[] = {
    { &kNS_TRANSACTIONMANAGER_CID, false, nullptr, nsTransactionManagerConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kTxMgrContracts[] = {
    { NS_TRANSACTIONMANAGER_CONTRACTID, &kNS_TRANSACTIONMANAGER_CID },
    { nullptr }
};

static const mozilla::Module kTxMgrModule = {
    mozilla::Module::kVersion,
    kTxMgrCIDs,
    kTxMgrContracts
};
NSMODULE_DEFN(nsTransactionManagerModule) = &kTxMgrModule;
