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
 */

#include "nsIGenericFactory.h" 
#include "nsIModule.h" 
#include "nsCOMPtr.h" 
#include "nsGfxCIID.h" 
 
#include "nsBlender.h" 
#include "nsFontMetricsBeOS.h"
#include "nsRenderingContextBeOS.h"
// aka    nsDeviceContextSpecBeOS.h 
#include "nsDeviceContextSpecB.h"
// aka    nsDeviceContextSpecFactoryBeOS.h
#include "nsDeviceContextSpecFactoryB.h" 
#include "nsScreenManagerBeOS.h" 
#include "nsScriptableRegion.h" 
#include "nsIImageManager.h" 
#include "nsDeviceContextBeOS.h" 
#include "nsImageBeOS.h" 

// objects that just require generic constructors 

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontMetricsBeOS) 
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextBeOS) 
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRenderingContextBeOS) 
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImageBeOS) 
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlender) 
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRegionBeOS) 
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecBeOS) 
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecFactoryBeOS) 
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontEnumeratorBeOS) 
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerBeOS) 
 
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
  // create an nsRegionGtk and get the scriptable region from it 
  nsCOMPtr <nsIRegion> rgn; 
  NS_NEWXPCOM(rgn, nsRegionBeOS); 
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
 
static nsresult nsImageManagerConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult) 
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
  { "BeOS Font Metrics", 
    NS_FONT_METRICS_CID, 
    //    "@mozilla.org/gfx/font_metrics/beos;1", 
    "@mozilla.org/gfx/fontmetrics;1", 
    nsFontMetricsBeOSConstructor }, 
  { "BeOS Device Context", 
    NS_DEVICE_CONTEXT_CID, 
    //    "@mozilla.org/gfx/device_context/beos;1", 
    "@mozilla.org/gfx/devicecontext;1", 
    nsDeviceContextBeOSConstructor }, 
  { "BeOS Rendering Context", 
    NS_RENDERING_CONTEXT_CID, 
    //    "@mozilla.org/gfx/rendering_context/beos;1", 
    "@mozilla.org/gfx/renderingcontext;1", 
    nsRenderingContextBeOSConstructor }, 
  { "BeOS Image", 
    NS_IMAGE_CID, 
    //    "@mozilla.org/gfx/image/beos;1", 
    "@mozilla.org/gfx/image;1", 
    nsImageBeOSConstructor }, 
  { "BeOS Region", 
    NS_REGION_CID, 
    "@mozilla.org/gfx/region/gtk;1", 
    nsRegionBeOSConstructor }, 
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
  { "BeOS Device Context Spec", 
    NS_DEVICE_CONTEXT_SPEC_CID, 
    //    "@mozilla.org/gfx/device_context_spec/beos;1", 
    "@mozilla.org/gfx/devicecontextspec;1", 
    nsDeviceContextSpecBeOSConstructor }, 
  { "BeOS Device Context Spec Factory", 
    NS_DEVICE_CONTEXT_SPEC_FACTORY_CID, 
    //    "@mozilla.org/gfx/device_context_spec_factory/beos;1", 
    "@mozilla.org/gfx/devicecontextspecfactory;1", 
    nsDeviceContextSpecFactoryBeOSConstructor }, 
  { "Image Manager", 
    NS_IMAGEMANAGER_CID, 
    //    "@mozilla.org/gfx/image_manager;1", 
    "@mozilla.org/gfx/imagemanager;1", 
    nsImageManagerConstructor }, 
  { "BeOS Font Enumerator", 
    NS_FONT_ENUMERATOR_CID, 
    //    "@mozilla.org/gfx/font_enumerator/beos;1", 
    "@mozilla.org/gfx/fontenumerator;1", 
    nsFontEnumeratorBeOSConstructor }, 
  { "BeOS Screen Manager", 
    NS_SCREENMANAGER_CID, 
    //    "@mozilla.org/gfx/screenmanager/beos;1", 
    "@mozilla.org/gfx/screenmanager;1", 
    nsScreenManagerBeOSConstructor } 
};   

NS_IMPL_NSGETMODULE("nsGfxBeOSModule", components) 
