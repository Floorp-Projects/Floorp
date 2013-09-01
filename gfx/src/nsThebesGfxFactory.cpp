/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatform.h"                // for gfxPlatform
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"         // for MOZ_FINAL
#include "mozilla/Module.h"             // for Module, Module::CIDEntry, etc
#include "mozilla/ModuleUtils.h"
#include "mozilla/mozalloc.h"           // for operator new
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsError.h"                    // for NS_ERROR_NO_AGGREGATION, etc
#include "nsGfxCIID.h"                  // for NS_FONT_ENUMERATOR_CID, etc
#include "nsID.h"                       // for NS_DEFINE_NAMED_CID, etc
#include "nsIScriptableRegion.h"        // for nsIScriptableRegion
#include "nsISupports.h"                // for NS_DECL_ISUPPORTS, etc
#include "nsScriptableRegion.h"         // for nsScriptableRegion
#include "nsThebesFontEnumerator.h"     // for nsThebesFontEnumerator

// This class doesn't do anything; its only purpose is to give
// gfxPlatform::Init a way to force this component to be registered,
// so that gfxPlatform::Shutdown will be called at an appropriate
// time.  (Component teardown is the only shutdown hook that runs
// late enough; see bug 651498.)

namespace {
class GfxInitialization MOZ_FINAL : public nsISupports {
    NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS0(GfxInitialization)
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsThebesFontEnumerator)

static nsresult
nsScriptableRegionConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nullptr;
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsCOMPtr<nsIScriptableRegion> scriptableRgn = new nsScriptableRegion();
  return scriptableRgn->QueryInterface(aIID, aResult);
}

NS_GENERIC_FACTORY_CONSTRUCTOR(GfxInitialization)

NS_DEFINE_NAMED_CID(NS_FONT_ENUMERATOR_CID);
NS_DEFINE_NAMED_CID(NS_SCRIPTABLE_REGION_CID);
NS_DEFINE_NAMED_CID(NS_GFX_INITIALIZATION_CID);

static const mozilla::Module::CIDEntry kThebesCIDs[] = {
    { &kNS_FONT_ENUMERATOR_CID, false, nullptr, nsThebesFontEnumeratorConstructor },
    { &kNS_SCRIPTABLE_REGION_CID, false, nullptr, nsScriptableRegionConstructor },
    { &kNS_GFX_INITIALIZATION_CID, false, nullptr, GfxInitializationConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kThebesContracts[] = {
    { "@mozilla.org/gfx/fontenumerator;1", &kNS_FONT_ENUMERATOR_CID },
    { "@mozilla.org/gfx/region;1", &kNS_SCRIPTABLE_REGION_CID },
    { "@mozilla.org/gfx/init;1", &kNS_GFX_INITIALIZATION_CID },
    { nullptr }
};

static void
nsThebesGfxModuleDtor()
{
    gfxPlatform::Shutdown();
}

static const mozilla::Module kThebesModule = {
    mozilla::Module::kVersion,
    kThebesCIDs,
    kThebesContracts,
    nullptr,
    nullptr,
    nullptr,
    nsThebesGfxModuleDtor
};

NSMODULE_DEFN(nsGfxModule) = &kThebesModule;
