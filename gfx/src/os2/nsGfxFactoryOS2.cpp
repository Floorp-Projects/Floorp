/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher
 * Blizzard.  Portions created by Christopher Blizzard are
 * Copyright (C) 2000 Christopher Blizzard. All Rights Reserved.
 *
 * Contributor(s): 
 *   Christopher Blizzzard <blizzard@mozilla.org>
 *   IBM Corp.
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsGfxCIID.h"

#include "nsBlender.h"
#include "nsFontMetricsOS2.h"
#include "nsRenderingContextOS2.h"
#include "nsDeviceContextSpecOS2.h"
#include "nsDeviceContextSpecFactoryO.h"
#include "nsScreenManagerOS2.h"
#include "nsScriptableRegion.h"
#include "nsIImageManager.h"
#include "nsDeviceContextOS2.h"
#include "nsImageOS2.h"
#include "nsRegionOS2.h"
#include "nsPrintOptionsOS2.h"

// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontMetricsOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRenderingContextOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImageOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlender)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRegionOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecFactoryOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontEnumeratorOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerOS2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrintOptionsOS2)

// our custom constructors

static NS_IMETHODIMP nsScriptableRegionConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;

  nsIScriptableRegion *inst;

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
  // create an nsRegionOS2 and get the scriptable region from it
  nsCOMPtr <nsIRegion> rgn;
  NS_NEWXPCOM(rgn, nsRegionOS2);
  if (rgn != nsnull)
  {
    nsCOMPtr<nsIScriptableRegion> scriptableRgn = new nsScriptableRegion(rgn);
    inst = scriptableRgn;
  }
  if (!inst)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    return rv;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

static NS_IMETHODIMP nsImageManagerConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;

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
  // this will return an image manager with a count of 1
  rv = NS_NewImageManager((nsIImageManager **)aResult);
  return rv;
}

static nsModuleComponentInfo components[] =
{
  { "OS/2 Font Metrics",
    NS_FONT_METRICS_CID,
    //    "@mozilla.org/gfx/font_metrics/gtk;1",
    "@mozilla.org/gfx/fontmetrics;1",
    nsFontMetricsOS2Constructor },
  { "OS/2 Device Context",
    NS_DEVICE_CONTEXT_CID,
    //    "@mozilla.org/gfx/device_context/gtk;1",
    "@mozilla.org/gfx/devicecontext;1",
    nsDeviceContextOS2Constructor },
  { "OS/2 Rendering Context",
    NS_RENDERING_CONTEXT_CID,
    //    "@mozilla.org/gfx/rendering_context/gtk;1",
    "@mozilla.org/gfx/renderingcontext;1",
    nsRenderingContextOS2Constructor },
  { "OS/2 Image",
    NS_IMAGE_CID,
    //    "@mozilla.org/gfx/image/gtk;1",
    "@mozilla.org/gfx/image;1",
    nsImageOS2Constructor },
  { "OS/2 Region",
    NS_REGION_CID,
    "@mozilla.org/gfx/region/gtk;1",
    nsRegionOS2Constructor },
  { "Scriptable Region",
    NS_SCRIPTABLE_REGION_CID,
    //    "@mozilla.org/gfx/scriptable_region;1",
    "@mozilla.org/gfx/region;1",
    nsScriptableRegionConstructor },
  { "Blender",
    NS_BLENDER_CID,
    //    "@mozilla.org/gfx/blender;1",
    "@mozilla.org/gfx/blender;1",
    nsBlenderConstructor },
  { "OS/2 Device Context Spec",
    NS_DEVICE_CONTEXT_SPEC_CID,
    //    "@mozilla.org/gfx/device_context_spec/gtk;1",
    "@mozilla.org/gfx/devicecontextspec;1",
    nsDeviceContextSpecOS2Constructor },
  { "OS/2 Device Context Spec Factory",
    NS_DEVICE_CONTEXT_SPEC_FACTORY_CID,
    //    "@mozilla.org/gfx/device_context_spec_factory/gtk;1",
    "@mozilla.org/gfx/devicecontextspecfactory;1",
    nsDeviceContextSpecFactoryOS2Constructor },
  { "Image Manager",
    NS_IMAGEMANAGER_CID,
    //    "@mozilla.org/gfx/image_manager;1",
    "@mozilla.org/gfx/imagemanager;1",
    nsImageManagerConstructor },
  { "Print Options",
    NS_PRINTOPTIONS_CID,
    //    "@mozilla.org/gfx/printoptions;1",
    "@mozilla.org/gfx/printoptions;1",
    nsPrintOptionsOS2Constructor },
  { "OS2 Font Enumerator",
    NS_FONT_ENUMERATOR_CID,
    //    "@mozilla.org/gfx/font_enumerator/gtk;1",
    "@mozilla.org/gfx/fontenumerator;1",
    nsFontEnumeratorOS2Constructor },
  { "OS/2 Screen Manager",
    NS_SCREENMANAGER_CID,
    //    "@mozilla.org/gfx/screenmanager/gtk;1",
    "@mozilla.org/gfx/screenmanager;1",
    nsScreenManagerOS2Constructor }
};

PR_STATIC_CALLBACK(void)
nsGfxOS2ModuleDtor(nsIModule *self)
{
//  nsRenderingContextOS2::Shutdown();
}

NS_IMPL_NSGETMODULE_WITH_DTOR("nsGfxOS2Module", components, nsGfxOS2ModuleDtor)

