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
#include "nsFontMetricsGTK.h"
#include "nsRenderingContextGTK.h"
// aka    nsDeviceContextSpecGTK.h
#include "nsDeviceContextSpecG.h"
// aka    nsDeviceContextSpecFactoryGTK.h
#include "nsDeviceContextSpecFactoryG.h"
#include "nsScreenManagerGtk.h"
#include "nsScriptableRegion.h"
#include "nsIImageManager.h"
#include "nsDeviceContextGTK.h"
#include "nsImageGTK.h"

// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontMetricsGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRenderingContextGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImageGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlender)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRegionGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecFactoryGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontEnumeratorGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerGtk)

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
  NS_NEWXPCOM(rgn, nsRegionGTK);
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
  { "Gtk Font Metrics",
    NS_FONT_METRICS_CID,
    //    "mozilla.gfx.font_metrics.gtk.1",
    "component://netscape/gfx/fontmetrics",
    nsFontMetricsGTKConstructor },
  { "Gtk Device Context",
    NS_DEVICE_CONTEXT_CID,
    //    "mozilla.gfx.device_context.gtk.1",
    "component://netscape/gfx/devicecontext",
    nsDeviceContextGTKConstructor },
  { "Gtk Rendering Context",
    NS_RENDERING_CONTEXT_CID,
    //    "mozilla.gfx.rendering_context.gtk.1",
    "component://netscape/gfx/renderingcontext",
    nsRenderingContextGTKConstructor },
  { "Gtk Image",
    NS_IMAGE_CID,
    //    "mozilla.gfx.image.gtk.1",
    "component://netscape/gfx/image",
    nsImageGTKConstructor },
  { "Gtk Region",
    NS_REGION_CID,
    "mozilla.gfx.region.gtk.1",
    nsRegionGTKConstructor },
  { "Scriptable Region",
    NS_SCRIPTABLE_REGION_CID,
    //    "mozilla.gfx.scriptable_region.1",
    "component://netscape/gfx/region",
    nsScriptableRegionConstructor },
  { "Blender",
    NS_BLENDER_CID,
    //    "mozilla.gfx.blender.1",
    "component://netscape/gfx/blender",
    nsBlenderConstructor },
  { "Gtk Device Context Spec",
    NS_DEVICE_CONTEXT_SPEC_CID,
    //    "mozilla.gfx.device_context_spec.gtk.1",
    "component://netscape/gfx/devicecontextspec",
    nsDeviceContextSpecGTKConstructor },
  { "Gtk Device Context Spec Factory",
    NS_DEVICE_CONTEXT_SPEC_FACTORY_CID,
    //    "mozilla.gfx.device_context_spec_factory.gtk.1",
    "component://netscape/gfx/devicecontextspecfactory",
    nsDeviceContextSpecFactoryGTKConstructor },
  { "Image Manager",
    NS_IMAGEMANAGER_CID,
    //    "mozilla.gfx.image_manager.1",
    "component://netscape/gfx/imagemanager",
    nsImageManagerConstructor },
  { "GTK Font Enumerator",
    NS_FONT_ENUMERATOR_CID,
    //    "mozilla.gfx.font_enumerator.gtk.1",
    "component://netscape/gfx/fontenumerator",
    nsFontEnumeratorGTKConstructor },
  { "Gtk Screen Manager",
    NS_SCREENMANAGER_CID,
    //    "mozilla.gfx.screenmanager.gtk.1",
    "component://netscape/gfx/screenmanager",
    nsScreenManagerGtkConstructor }
};

NS_IMPL_NSGETMODULE("nsGfxGTKModule", components)

