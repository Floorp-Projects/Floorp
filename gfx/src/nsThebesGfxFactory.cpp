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

#include "nsScriptableRegion.h"

#include "nsThebesDeviceContext.h"
#include "nsThebesRenderingContext.h"
#include "nsThebesRegion.h"
#include "nsThebesFontMetrics.h"
#include "nsThebesFontEnumerator.h"
#include "gfxPlatform.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsThebesFontMetrics)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsThebesDeviceContext)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsThebesRenderingContext)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsThebesRegion)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsThebesFontEnumerator)

static nsresult
nsScriptableRegionConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;

  nsIScriptableRegion *inst = nsnull;

  if ( !aResult )
  {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = nsnull;
  if (aOuter)
  {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }

  nsCOMPtr <nsIRegion> rgn = new nsThebesRegion();
  nsCOMPtr<nsIScriptableRegion> scriptableRgn;
  if (rgn != nsnull)
  {
    scriptableRgn = new nsScriptableRegion(rgn);
    inst = scriptableRgn;
  }
  if (!inst)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    return rv;
  }
  NS_ADDREF(inst);
  // release our variable above now that we have created our owning
  // reference - we don't want this to go out of scope early!
  scriptableRgn = nsnull;
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

NS_DEFINE_NAMED_CID(NS_FONT_METRICS_CID);
NS_DEFINE_NAMED_CID(NS_FONT_ENUMERATOR_CID);
NS_DEFINE_NAMED_CID(NS_DEVICE_CONTEXT_CID);
NS_DEFINE_NAMED_CID(NS_RENDERING_CONTEXT_CID);
NS_DEFINE_NAMED_CID(NS_REGION_CID);
NS_DEFINE_NAMED_CID(NS_SCRIPTABLE_REGION_CID);

static const mozilla::Module::CIDEntry kThebesCIDs[] = {
    { &kNS_FONT_METRICS_CID, false, NULL, nsThebesFontMetricsConstructor },
    { &kNS_FONT_ENUMERATOR_CID, false, NULL, nsThebesFontEnumeratorConstructor },
    { &kNS_DEVICE_CONTEXT_CID, false, NULL, nsThebesDeviceContextConstructor },
    { &kNS_RENDERING_CONTEXT_CID, false, NULL, nsThebesRenderingContextConstructor },
    { &kNS_REGION_CID, false, NULL, nsThebesRegionConstructor },
    { &kNS_SCRIPTABLE_REGION_CID, false, NULL, nsScriptableRegionConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kThebesContracts[] = {
    { "@mozilla.org/gfx/fontmetrics;1", &kNS_FONT_METRICS_CID },
    { "@mozilla.org/gfx/fontenumerator;1", &kNS_FONT_ENUMERATOR_CID },
    { "@mozilla.org/gfx/devicecontext;1", &kNS_DEVICE_CONTEXT_CID },
    { "@mozilla.org/gfx/renderingcontext;1", &kNS_RENDERING_CONTEXT_CID },
    { "@mozilla.org/gfx/region/nsThebes;1", &kNS_REGION_CID },
    { "@mozilla.org/gfx/region;1", &kNS_SCRIPTABLE_REGION_CID },
    { NULL }
};

static nsresult
nsThebesGfxModuleCtor()
{
    return gfxPlatform::Init();
}

static void
nsThebesGfxModuleDtor()
{
    nsThebesDeviceContext::Shutdown();
    gfxPlatform::Shutdown();
}

static const mozilla::Module kThebesModule = {
    mozilla::Module::kVersion,
    kThebesCIDs,
    kThebesContracts,
    NULL,
    NULL,
    nsThebesGfxModuleCtor,
    nsThebesGfxModuleDtor
};

NSMODULE_DEFN(nsGfxModule) = &kThebesModule;
