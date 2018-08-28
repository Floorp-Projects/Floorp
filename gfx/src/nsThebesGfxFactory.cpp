/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatform.h"                // for gfxPlatform
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"         // for final
#include "mozilla/Module.h"             // for Module, Module::CIDEntry, etc
#include "mozilla/ModuleUtils.h"
#include "mozilla/mozalloc.h"           // for operator new
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsError.h"                    // for NS_ERROR_NO_AGGREGATION, etc
#include "nsGfxCIID.h"                  // for NS_FONT_ENUMERATOR_CID, etc
#include "nsID.h"                       // for NS_DEFINE_NAMED_CID, etc
#include "nsISupports.h"                // for NS_DECL_ISUPPORTS, etc
#include "nsThebesFontEnumerator.h"     // for nsThebesFontEnumerator

NS_GENERIC_FACTORY_CONSTRUCTOR(nsThebesFontEnumerator)

NS_DEFINE_NAMED_CID(NS_FONT_ENUMERATOR_CID);

static const mozilla::Module::CIDEntry kThebesCIDs[] = {
    { &kNS_FONT_ENUMERATOR_CID, false, nullptr, nsThebesFontEnumeratorConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kThebesContracts[] = {
    { "@mozilla.org/gfx/fontenumerator;1", &kNS_FONT_ENUMERATOR_CID },
    { nullptr }
};

static const mozilla::Module kThebesModule = {
    mozilla::Module::kVersion,
    kThebesCIDs,
    kThebesContracts,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

NSMODULE_DEFN(nsGfxModule) = &kThebesModule;
