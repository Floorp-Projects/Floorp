/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIFontList.h"
#include "nsGfxCIID.h"
#include "nsFontList.h"
#include "nsFontMetricsMac.h"
#include "nsRenderingContextMac.h"
#include "nsImageMac.h"
#include "nsDeviceContextMac.h"
#include "nsRegionMac.h"
#include "nsScriptableRegion.h"
#if TARGET_CARBON
#include "nsDeviceContextSpecX.h"
#include "nsPrintOptionsX.h"
#else
#include "nsDeviceContextSpecMac.h"
#include "nsPrintOptionsMac.h"
#endif
#include "nsDeviceContextSpecFactoryM.h"
#include "nsScreenManagerMac.h"
#include "nsBlender.h"
#include "nsCOMPtr.h"
#include "nsPrintOptionsMac.h"

#ifdef XP_MACOSX

#include "nsIGenericFactory.h"

// XXX Implement the GFX module using NS_GENERIC_FACTORY_CONSTRUCTOR / NS_IMPL_NSGETMODULE

NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontMetricsMac)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextMac)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRenderingContextMac)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImageMac)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRegionMac)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBlender)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDrawingSurfaceMac)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecX)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPrintOptionsX)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDeviceContextSpecFactoryMac)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontEnumeratorMac)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFontList)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScreenManagerMac)

static NS_IMETHODIMP
nsScriptableRegionConstructor(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  NS_ENSURE_NO_AGGREGATION(aOuter);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;
  nsCOMPtr<nsIRegion> region = new nsRegionMac();
  NS_ENSURE_TRUE(region, NS_ERROR_OUT_OF_MEMORY);
  nsCOMPtr<nsIScriptableRegion> result(new nsScriptableRegion(region));
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);
  return result->QueryInterface(aIID, aResult);
}

static nsModuleComponentInfo components[] =
{
  { "nsFontMetrics",
    NS_FONT_METRICS_CID,
    "@mozilla.org/gfx/fontmetrics;1",
    nsFontMetricsMacConstructor },
  { "nsDeviceContext",
    NS_DEVICE_CONTEXT_CID,
    "@mozilla.org/gfx/devicecontext;1",
    nsDeviceContextMacConstructor },
  { "nsRenderingContext",
    NS_RENDERING_CONTEXT_CID,
    "@mozilla.org/gfx/renderingcontext;1",
    nsRenderingContextMacConstructor },
  { "nsImage",
    NS_IMAGE_CID,
    "@mozilla.org/gfx/image;1",
    nsImageMacConstructor },
  { "nsRegion",
    NS_REGION_CID,
    "@mozilla.org/gfx/unscriptable-region;1",
    nsRegionMacConstructor },
  { "nsBlender",
    NS_BLENDER_CID,
    "@mozilla.org/gfx/blender;1",
    nsBlenderConstructor },
  { "nsDrawingSurface",
    NS_DRAWING_SURFACE_CID,
    "@mozilla.org/gfx/drawing-surface;1",
    nsDrawingSurfaceMacConstructor },
  { "nsDeviceContextSpec",
    NS_DEVICE_CONTEXT_SPEC_CID,
    "@mozilla.org/gfx/devicecontextspec;1",
    nsDeviceContextSpecXConstructor },
  { "nsDeviceContextSpecFactory",
    NS_DEVICE_CONTEXT_SPEC_FACTORY_CID,
    "@mozilla.org/gfx/devicecontextspecfactory;1",
    nsDeviceContextSpecFactoryMacConstructor },
  { "nsScriptableRegion",
    NS_SCRIPTABLE_REGION_CID,
    "@mozilla.org/gfx/region;1",
    nsScriptableRegionConstructor },
  { "nsPrintOptions",
    NS_PRINTOPTIONS_CID,
    "@mozilla.org/gfx/printoptions;1",
    nsPrintOptionsXConstructor },
  { "nsFontEnumerator",
    NS_FONT_ENUMERATOR_CID,
    "@mozilla.org/gfx/fontenumerator;1",
    nsFontEnumeratorMacConstructor },
  { "nsFontList",
    NS_FONTLIST_CID,
    "@mozilla.org/gfx/fontlist;1",
    nsFontListConstructor },
  { "nsScreenManager",
    NS_SCREENMANAGER_CID,
    "@mozilla.org/gfx/screenmanager;1",
    nsScreenManagerMacConstructor }
};

NS_IMPL_NSGETMODULE(nsGfxModule, components)

#else

static NS_DEFINE_IID(kCFontMetrics, NS_FONT_METRICS_CID);
static NS_DEFINE_IID(kCFontEnumerator, NS_FONT_ENUMERATOR_CID);
static NS_DEFINE_IID(kCFontList, NS_FONTLIST_CID);
static NS_DEFINE_IID(kCRenderingContext, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_IID(kCImage, NS_IMAGE_CID);
static NS_DEFINE_IID(kCDeviceContext, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_IID(kCRegion, NS_REGION_CID);
static NS_DEFINE_IID(kCScriptableRegion, NS_SCRIPTABLE_REGION_CID);
static NS_DEFINE_IID(kCDeviceContextSpec, NS_DEVICE_CONTEXT_SPEC_CID);
static NS_DEFINE_IID(kCDeviceContextSpecFactory, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);
static NS_DEFINE_IID(kCBlender, NS_BLENDER_CID);
static NS_DEFINE_IID(kCScreenManager, NS_SCREENMANAGER_CID);
static NS_DEFINE_IID(kCPrintOptions, NS_PRINTOPTIONS_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

class nsGfxFactoryMac : public nsIFactory {
public:   
	// nsISupports methods
	NS_DECL_ISUPPORTS

	// nsIFactory methods   
	NS_IMETHOD CreateInstance(nsISupports *aOuter,   
	                          const nsIID &aIID,   
	                          void **aResult);

	NS_IMETHOD LockFactory(PRBool aLock);   

	nsGfxFactoryMac(const nsCID &aClass);   
	~nsGfxFactoryMac();   

private:   
	nsCID     mClassID;
};   

nsGfxFactoryMac::nsGfxFactoryMac(const nsCID &aClass)   
{
	NS_INIT_ISUPPORTS();
	mClassID = aClass;
}   

nsGfxFactoryMac::~nsGfxFactoryMac()   
{   
}   

NS_IMPL_ISUPPORTS1(nsGfxFactoryMac, nsIFactory)

nsresult nsGfxFactoryMac::CreateInstance(nsISupports *aOuter,  
                                          const nsIID &aIID,  
                                          void **aResult)  
{  
	if (aResult == NULL) {  
		return NS_ERROR_NULL_POINTER;  
	}  

	*aResult = NULL;  

	nsCOMPtr<nsISupports> inst;

	if (mClassID.Equals(kCFontMetrics)) {
		NS_NEWXPCOM(inst, nsFontMetricsMac);
	}
	else if (mClassID.Equals(kCDeviceContext)) {
		NS_NEWXPCOM(inst, nsDeviceContextMac);
	}
	else if (mClassID.Equals(kCRenderingContext)) {
		NS_NEWXPCOM(inst, nsRenderingContextMac);
	}
	else if (mClassID.Equals(kCImage)) {
	  inst = NS_STATIC_CAST(nsIImage*, new nsImageMac());
	}
	else if (mClassID.Equals(kCRegion)) {
		NS_NEWXPCOM(inst, nsRegionMac);
	}
    else if (mClassID.Equals(kCBlender)) {
        NS_NEWXPCOM(inst, nsBlender);
    }
	else if (mClassID.Equals(kCScriptableRegion)) {
		nsCOMPtr<nsIRegion> rgn;
		NS_NEWXPCOM(rgn, nsRegionMac);
		if (rgn != nsnull) {
			nsCOMPtr<nsIScriptableRegion> scriptableRgn = new nsScriptableRegion(rgn);
			inst = scriptableRgn;
		}
	}
	else if (mClassID.Equals(kCDeviceContextSpec)) {
	    nsCOMPtr<nsIDeviceContextSpec> dcSpec;
#if TARGET_CARBON
	    NS_NEWXPCOM(dcSpec, nsDeviceContextSpecX);
#else
	    NS_NEWXPCOM(dcSpec, nsDeviceContextSpecMac);
#endif
        inst = dcSpec;
	}
  else if (mClassID.Equals(kCPrintOptions)) {
#if TARGET_CARBON
    NS_NEWXPCOM(inst, nsPrintOptionsX);
#else
    NS_NEWXPCOM(inst, nsPrintOptionsMac);
#endif
  }
 	else if (mClassID.Equals(kCDeviceContextSpecFactory)) {
		NS_NEWXPCOM(inst, nsDeviceContextSpecFactoryMac);
	}
	else if (mClassID.Equals(kCFontEnumerator)) {
    nsFontEnumeratorMac* fe;
    NS_NEWXPCOM(fe, nsFontEnumeratorMac);
    inst = (nsISupports *)fe;
  } 
	else if (mClassID.Equals(kCFontList)) {
    nsFontList* fl;
    NS_NEWXPCOM(fl, nsFontList);
    inst = (nsISupports *)fl;
  } 
	else if (mClassID.Equals(kCScreenManager)) {
		NS_NEWXPCOM(inst, nsScreenManagerMac);
  } 


	if (inst == NULL) {  
		return NS_ERROR_OUT_OF_MEMORY;  
	}  

	return inst->QueryInterface(aIID, aResult);
}

nsresult nsGfxFactoryMac::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

// return the proper factory to the caller

extern "C" NS_GFX nsresult NSGetFactory(nsISupports* servMgr,
                                        const nsCID &aClass,
                                        const char *aClassName,
                                        const char *aContractID,
                                        nsIFactory **aFactory)
{
	if (nsnull == aFactory) {
		return NS_ERROR_NULL_POINTER;
	}

	nsCOMPtr<nsIFactory> factory = new nsGfxFactoryMac(aClass);
	if (nsnull == factory) {
		return NS_ERROR_OUT_OF_MEMORY;
	}
	
	return factory->QueryInterface(kIFactoryIID, (void**)aFactory);
}

#endif
