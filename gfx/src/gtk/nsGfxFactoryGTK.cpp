/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzzard <blizzard@mozilla.org>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "gfx-config.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsCOMPtr.h"
#include "nsGfxCIID.h"

#include "nsBlender.h"
#include "nsRenderingContextGTK.h"
#include "nsDeviceContextGTK.h"
// aka    nsDeviceContextSpecGTK.h
#include "nsDeviceContextSpecG.h"
// aka    nsDeviceContextSpecFactoryGTK.h
#include "nsDeviceContextSpecFactoryG.h"
#include "nsScreenManagerGtk.h"
#include "nsScriptableRegion.h"
#include "nsDeviceContextGTK.h"
#include "nsImageGTK.h"
#include "nsPrintOptionsGTK.h"
#include "nsFontList.h"
#include "nsRegionGTK.h"
#include "nsGCCache.h"
#ifdef NATIVE_THEME_SUPPORT
#include "nsNativeThemeGTK.h"
#endif
#ifdef MOZ_ENABLE_PANGO
#include "nsFontMetricsPango.h"
#endif
#ifdef MOZ_ENABLE_XFT
#include "nsFontMetricsXft.h"
#endif
#ifdef MOZ_ENABLE_COREXFONTS
#include "nsFontMetricsGTK.h"
#endif
#include "nsFontMetricsUtils.h"
#include "nsPrintSession.h"
#include "gfxImageFrame.h"
#ifdef MOZ_ENABLE_FREETYPE2
#include "nsFT2FontCatalog.h"
#include "nsFreeType.h"
#endif

// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRenderingContextGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImageGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlender)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRegionGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecFactoryGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontList)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerGtk)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrintOptionsGTK)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrinterEnumeratorGTK)
#ifdef NATIVE_THEME_SUPPORT
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNativeThemeGTK)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(gfxImageFrame)
#ifdef MOZ_ENABLE_FREETYPE2
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFT2FontCatalog)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsFreeType2, Init)
#endif

// our custom constructors

static nsresult
nsFontMetricsConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsIFontMetrics *result;

  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  *aResult = nsnull;

  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

#ifdef MOZ_ENABLE_PANGO
  if (NS_IsPangoEnabled()) {
    result = new nsFontMetricsPango();
    if (!result)
      return NS_ERROR_OUT_OF_MEMORY;
  } else {
#endif
#ifdef MOZ_ENABLE_XFT
  if (NS_IsXftEnabled()) {
    result = new nsFontMetricsXft();
    if (!result)
      return NS_ERROR_OUT_OF_MEMORY;
  } else {
#endif
#ifdef MOZ_ENABLE_COREXFONTS
    result = new nsFontMetricsGTK();
    if (!result)
      return NS_ERROR_OUT_OF_MEMORY;
#endif
#ifdef MOZ_ENABLE_XFT
  }
#endif
#ifdef MOZ_ENABLE_PANGO
  }
#endif

  NS_ADDREF(result);
  nsresult rv = result->QueryInterface(aIID, aResult);
  NS_RELEASE(result);

  return rv;
}

static nsresult
nsFontEnumeratorConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsIFontEnumerator *result;

  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  *aResult = nsnull;

  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

#ifdef MOZ_ENABLE_PANGO
  if (NS_IsPangoEnabled()) {
    result = new nsFontEnumeratorPango();
    if (!result)
      return NS_ERROR_OUT_OF_MEMORY;
  } else {
#endif
#ifdef MOZ_ENABLE_XFT
  if (NS_IsXftEnabled()) {
    result = new nsFontEnumeratorXft();
    if (!result)
      return NS_ERROR_OUT_OF_MEMORY;
  } else {
#endif
#ifdef MOZ_ENABLE_COREXFONTS
    result = new nsFontEnumeratorGTK();
    if (!result)
      return NS_ERROR_OUT_OF_MEMORY;
#endif
#ifdef MOZ_ENABLE_XFT
  }
#endif
#ifdef MOZ_ENABLE_PANGO
  }
#endif

  NS_ADDREF(result);
  nsresult rv = result->QueryInterface(aIID, aResult);
  NS_RELEASE(result);

  return rv;
}

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

static const nsModuleComponentInfo components[] =
{
  { "Gtk Font Metrics",
    NS_FONT_METRICS_CID,
    //    "@mozilla.org/gfx/font_metrics/gtk;1",
    "@mozilla.org/gfx/fontmetrics;1",
    nsFontMetricsConstructor },
  { "Gtk Device Context",
    NS_DEVICE_CONTEXT_CID,
    //    "@mozilla.org/gfx/device_context/gtk;1",
    "@mozilla.org/gfx/devicecontext;1",
    nsDeviceContextGTKConstructor },
  { "Gtk Rendering Context",
    NS_RENDERING_CONTEXT_CID,
    //    "@mozilla.org/gfx/rendering_context/gtk;1",
    "@mozilla.org/gfx/renderingcontext;1",
    nsRenderingContextGTKConstructor },
  { "Gtk Image",
    NS_IMAGE_CID,
    //    "@mozilla.org/gfx/image/gtk;1",
    "@mozilla.org/gfx/image;1",
    nsImageGTKConstructor },
  { "Gtk Region",
    NS_REGION_CID,
    "@mozilla.org/gfx/region/gtk;1",
    nsRegionGTKConstructor },
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
  { "Gtk Device Context Spec",
    NS_DEVICE_CONTEXT_SPEC_CID,
    //    "@mozilla.org/gfx/device_context_spec/gtk;1",
    "@mozilla.org/gfx/devicecontextspec;1",
    nsDeviceContextSpecGTKConstructor },
  { "Gtk Device Context Spec Factory",
    NS_DEVICE_CONTEXT_SPEC_FACTORY_CID,
    //    "@mozilla.org/gfx/device_context_spec_factory/gtk;1",
    "@mozilla.org/gfx/devicecontextspecfactory;1",
    nsDeviceContextSpecFactoryGTKConstructor },
  { "PrintSettings Service",
    NS_PRINTSETTINGSSERVICE_CID,
    //    "@mozilla.org/gfx/printsettings-service;1",
    "@mozilla.org/gfx/printsettings-service;1",
    nsPrintOptionsGTKConstructor },
  { "GTK Font Enumerator",
    NS_FONT_ENUMERATOR_CID,
    //    "@mozilla.org/gfx/font_enumerator/gtk;1",
    "@mozilla.org/gfx/fontenumerator;1",
    nsFontEnumeratorConstructor },
  { "Font List",  
    NS_FONTLIST_CID,
    //    "@mozilla.org/gfx/fontlist;1"
    NS_FONTLIST_CONTRACTID,
    nsFontListConstructor },
  { "Gtk Screen Manager",
    NS_SCREENMANAGER_CID,
    //    "@mozilla.org/gfx/screenmanager/gtk;1",
    "@mozilla.org/gfx/screenmanager;1",
    nsScreenManagerGtkConstructor },
  { "Gtk Printer Enumerator",
    NS_PRINTER_ENUMERATOR_CID,
    //    "@mozilla.org/gfx/printer_enumerator/gtk;1",
    "@mozilla.org/gfx/printerenumerator;1",
    nsPrinterEnumeratorGTKConstructor },
  { "windows image frame",
    GFX_IMAGEFRAME_CID,
    "@mozilla.org/gfx/image/frame;2",
    gfxImageFrameConstructor, },
  { "Print Session",
    NS_PRINTSESSION_CID,
    "@mozilla.org/gfx/printsession;1",
    nsPrintSessionConstructor },
#ifdef MOZ_ENABLE_FREETYPE2
  { "TrueType Font Catalog Service",
    NS_FONTCATALOGSERVICE_CID,
    "@mozilla.org/gfx/xfontcatalogservice;1",
    nsFT2FontCatalogConstructor },
  { "FreeType2 routines",
    NS_FREETYPE2_CID,
    NS_FREETYPE2_CONTRACTID,
    nsFreeType2Constructor },
#endif
#ifdef NATIVE_THEME_SUPPORT
   { "Native Theme Renderer",
    NS_THEMERENDERER_CID,
    "@mozilla.org/chrome/chrome-native-theme;1",
    nsNativeThemeGTKConstructor }
#endif
};

PR_STATIC_CALLBACK(nsresult)
nsGfxGTKModuleCtor(nsIModule *self)
{
  nsImageGTK::Startup();
  return NS_OK;
}

PR_STATIC_CALLBACK(void)
nsGfxGTKModuleDtor(nsIModule *self)
{
  nsRenderingContextGTK::Shutdown();
  nsDeviceContextGTK::Shutdown();
  nsImageGTK::Shutdown();
  nsGCCache::Shutdown();
#ifdef MOZ_WIDGET_GTK
  nsRegionGTK::Shutdown();
#endif
}

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(nsGfxGTKModule, components,
                                   nsGfxGTKModuleCtor, nsGfxGTKModuleDtor)
