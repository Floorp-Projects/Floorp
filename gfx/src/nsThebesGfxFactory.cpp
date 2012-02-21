/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is thebes gfx
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "mozilla/ModuleUtils.h"
#include "nsCOMPtr.h"
#include "nsGfxCIID.h"

#include "nsThebesFontEnumerator.h"
#include "nsScriptableRegion.h"

#include "gfxPlatform.h"

// This class doesn't do anything; its only purpose is to give
// gfxPlatform::Init a way to force this component to be registered,
// so that gfxPlatform::Shutdown will be called at an appropriate
// time.  (Component teardown is the only shutdown hook that runs
// late enough; see bug 651498.)

namespace {
class GfxInitialization : public nsISupports {
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
  *aResult = nsnull;
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
    { &kNS_FONT_ENUMERATOR_CID, false, NULL, nsThebesFontEnumeratorConstructor },
    { &kNS_SCRIPTABLE_REGION_CID, false, NULL, nsScriptableRegionConstructor },
    { &kNS_GFX_INITIALIZATION_CID, false, NULL, GfxInitializationConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kThebesContracts[] = {
    { "@mozilla.org/gfx/fontenumerator;1", &kNS_FONT_ENUMERATOR_CID },
    { "@mozilla.org/gfx/region;1", &kNS_SCRIPTABLE_REGION_CID },
    { "@mozilla.org/gfx/init;1", &kNS_GFX_INITIALIZATION_CID },
    { NULL }
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
    NULL,
    NULL,
    NULL,
    nsThebesGfxModuleDtor
};

NSMODULE_DEFN(nsGfxModule) = &kThebesModule;
