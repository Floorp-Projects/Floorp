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
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsGfxCIID.h"

#include "nsBlender.h"
#include "nsFontMetricsPh.h"
#include "nsRenderingContextPh.h"
// aka    nsDeviceContextSpecPh.h
#include "nsDeviceContextSpecPh.h"
// aka    nsDeviceContextSpecFactoryPh.h
#include "nsDeviceContextSpecFactoryP.h"
#include "nsScreenManagerPh.h"
#include "nsScriptableRegion.h"
#include "nsIImageManager.h"
#include "nsDeviceContextPh.h"

// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontMetricsPh)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextPh)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRenderingContextPh)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImagePh)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlender)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRegionPh)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecPh)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecFactoryPh)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontEnumeratorPh)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerPh)

// our custom constructors

static nsresult nsScriptableRegionConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;

  nsIScriptableRegion *inst;

  if ( NULL == aResult )
  {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = NULL;
  if (NULL != aOuter)
  {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }
  // create an nsRegionPh and get the scriptable region from it
  nsCOMPtr <nsIRegion> rgn;
  NS_NEWXPCOM(rgn, nsRegionPh);
  if (rgn != nsnull)
  {
    nsCOMPtr<nsIScriptableRegion> scriptableRgn = new nsScriptableRegion(rgn);
    inst = scriptableRgn;
  }
  if (NULL == inst)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    return rv;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

static nsresult nsImageManagerConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;

  if ( NULL == aResult )
  {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = NULL;
  if (NULL != aOuter)
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
  { "Ph Font Metrics",
    NS_FONT_METRICS_CID,
    "@mozilla.org/gfx/font_metrics/Ph;1",
    nsFontMetricsPhConstructor },
  { "Ph Device Context",
    NS_DEVICE_CONTEXT_CID,
    "@mozilla.org/gfx/device_context/Ph;1",
    nsDeviceContextPhConstructor },
  { "Ph Rendering Context",
    NS_RENDERING_CONTEXT_CID,
    "@mozilla.org/gfx/rendering_context/Ph;1",
    nsRenderingContextPhConstructor },
  { "Ph Image",
    NS_IMAGE_CID,
    "@mozilla.org/gfx/image/Ph;1",
    nsImagePhConstructor },
  { "Ph Region",
    NS_REGION_CID,
    "@mozilla.org/gfx/region/Ph;1",
    nsRegionPhConstructor },
  { "Scriptable Region",
    NS_SCRIPTABLE_REGION_CID,
    "@mozilla.org/gfx/scriptable_region;1",
    nsScriptableRegionConstructor },
  { "Blender",
    NS_BLENDER_CID,
    "@mozilla.org/gfx/blender;1",
    nsBlenderConstructor },
  { "Ph Device Context Spec",
    NS_DEVICE_CONTEXT_SPEC_CID,
    "@mozilla.org/gfx/device_context_spec/Ph;1",
    nsDeviceContextSpecPhConstructor },
  { "Ph Device Context Spec Factory",
    NS_DEVICE_CONTEXT_SPEC_FACTORY_CID,
    "@mozilla.org/gfx/device_context_spec_factory/Ph;1",
    nsDeviceContextSpecFactoryPhConstructor },
  { "Image Manager",
    NS_IMAGEMANAGER_CID,
    "@mozilla.org/gfx/image_manager;1",
    nsImageManagerConstructor },
   { "Ph Font Enumerator",
    NS_FONT_ENUMERATOR_CID,
    "@mozilla.org/gfx/font_enumerator/Ph;1",
    nsFontEnumeratorPhConstructor },
  { "Ph Screen Manager",
    NS_SCREENMANAGER_CID,
    "@mozilla.org/gfx/screenmanager/Ph;1",
    nsScreenManagerPhConstructor }
};

NS_IMPL_NSGETMODULE("nsGfxPhModule", components)

