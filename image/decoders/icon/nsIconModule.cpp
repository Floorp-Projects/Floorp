/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsServiceManagerUtils.h"

#include "nsIconProtocolHandler.h"
#include "nsIconURI.h"
#include "nsIconChannel.h"

// objects that just require generic constructors
//*****************************************************************************
// Protocol CIDs

#define NS_ICONPROTOCOL_CID { 0xd0f9db12, 0x249c, 0x11d5, \
                              { 0x99, 0x5, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsIconProtocolHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMozIconURI)
typedef nsMozIconURI::Mutator nsMozIconURIMutator;
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMozIconURIMutator)

NS_DEFINE_NAMED_CID(NS_ICONPROTOCOL_CID);
NS_DEFINE_NAMED_CID(NS_MOZICONURI_CID);
NS_DEFINE_NAMED_CID(NS_MOZICONURIMUTATOR_CID);

static const mozilla::Module::CIDEntry kIconCIDs[] = {
  { &kNS_ICONPROTOCOL_CID, false, nullptr, nsIconProtocolHandlerConstructor },
  { &kNS_MOZICONURI_CID, false, nullptr, nsMozIconURIConstructor },
  { &kNS_MOZICONURIMUTATOR_CID, false, nullptr, nsMozIconURIMutatorConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kIconContracts[] = {
  { NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "moz-icon", &kNS_ICONPROTOCOL_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kIconCategories[] = {
  { nullptr }
};

static void
IconDecoderModuleDtor()
{
}

static const mozilla::Module kIconModule = {
  mozilla::Module::kVersion,
  kIconCIDs,
  kIconContracts,
  kIconCategories,
  nullptr,
  nullptr,
  IconDecoderModuleDtor
};

NSMODULE_DEFN(nsIconDecoderModule) = &kIconModule;
