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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsGfxCIID.h"

#include "nsBlender.h"
#include "nsFontMetricsXlib.h"
#include "nsRenderingContextXlib.h"
// aka    nsDeviceContextSpecXlib.h
#include "nsDeviceContextSpecXlib.h"
// aka    nsDeviceContextSpecFactoryXlib.h
#include "nsDeviceContextSpecFactoryX.h"
#include "nsScreenManagerXlib.h"
#include "nsScriptableRegion.h"
#include "nsIImageManager.h"
#include "nsDeviceContextXlib.h"
#include "nsImageXlib.h"

// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontMetricsXlib)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextXlib)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRenderingContextXlib)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImageXlib)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlender)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRegionXlib)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecXlib)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecFactoryXlib)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontEnumeratorXlib)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerXlib)

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
  // create an nsRegionXlib and get the scriptable region from it
  nsCOMPtr <nsIRegion> rgn;
  NS_NEWXPCOM(rgn, nsRegionXlib);
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
  { "Xlib Font Metrics",
    NS_FONT_METRICS_CID,
    //    "@mozilla.org/gfx/font_metrics/xlib;1",
    "@mozilla.org/gfx/fontmetrics;1",
    nsFontMetricsXlibConstructor },
  { "Xlib Device Context",
    NS_DEVICE_CONTEXT_CID,
    //    "@mozilla.org/gfx/device_context/xlib;1",
    "@mozilla.org/gfx/devicecontext;1",
    nsDeviceContextXlibConstructor },
  { "Xlib Rendering Context",
    NS_RENDERING_CONTEXT_CID,
    //    "@mozilla.org/gfx/rendering_context/xlib;1",
    "@mozilla.org/gfx/renderingcontext;1",
    nsRenderingContextXlibConstructor },
  { "Xlib Image",
    NS_IMAGE_CID,
    //    "@mozilla.org/gfx/image/xlib;1",
    "@mozilla.org/gfx/image;1",
    nsImageXlibConstructor },
  { "Xlib Region",
    NS_REGION_CID,
    "@mozilla.org/gfx/region/xlib;1",
    nsRegionXlibConstructor },
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
  { "Xlib Device Context Spec",
    NS_DEVICE_CONTEXT_SPEC_CID,
    //    "@mozilla.org/gfx/device_context_spec/xlib;1",
    "@mozilla.org/gfx/devicecontextspec;1",
    nsDeviceContextSpecXlibConstructor },
  { "Xlib Device Context Spec Factory",
    NS_DEVICE_CONTEXT_SPEC_FACTORY_CID,
    //    "@mozilla.org/gfx/device_context_spec_factory/xlib;1",
    "@mozilla.org/gfx/devicecontextspecfactory;1",
    nsDeviceContextSpecFactoryXlibConstructor },
  { "Image Manager",
    NS_IMAGEMANAGER_CID,
    //    "@mozilla.org/gfx/image_manager;1",
    "@mozilla.org/gfx/imagemanager;1",
    nsImageManagerConstructor },
  { "Xlib Font Enumerator",
    NS_FONT_ENUMERATOR_CID,
    //    "@mozilla.org/gfx/font_enumerator/xlib;1",
    "@mozilla.org/gfx/fontenumerator;1",
      nsFontEnumeratorXlibConstructor },
  { "Xlib Screen Manager",
    NS_SCREENMANAGER_CID,
    //    "@mozilla.org/gfx/screenmanager/xlib;1",
    "@mozilla.org/gfx/screenmanager;1",
    nsScreenManagerXlibConstructor }
};

NS_IMPL_NSGETMODULE("nsGfxXlibModule", components)

