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

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsGfxCIID.h"
#include "nsFontMetricsMac.h"
#include "nsRenderingContextMac.h"
#include "nsImageMac.h"
#include "nsDeviceContextMac.h"
#include "nsRegionMac.h"
#include "nsScriptableRegion.h"
#include "nsIImageManager.h"
#include "nsDeviceContextSpecMac.h"
#include "nsDeviceContextSpecFactoryM.h"
#include "nsScreenManagerMac.h"
#include "nsBlender.h"
#include "nsCOMPtr.h"
#include "nsPrintOptionsMac.h"

static NS_DEFINE_IID(kCFontMetrics, NS_FONT_METRICS_CID);
static NS_DEFINE_IID(kCFontEnumerator, NS_FONT_ENUMERATOR_CID);
static NS_DEFINE_IID(kCRenderingContext, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_IID(kCImage, NS_IMAGE_CID);
static NS_DEFINE_IID(kCDeviceContext, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_IID(kCRegion, NS_REGION_CID);
static NS_DEFINE_IID(kCScriptableRegion, NS_SCRIPTABLE_REGION_CID);
static NS_DEFINE_IID(kCDeviceContextSpec, NS_DEVICE_CONTEXT_SPEC_CID);
static NS_DEFINE_IID(kCDeviceContextSpecFactory, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);
static NS_DEFINE_IID(kImageManagerImpl, NS_IMAGEMANAGER_CID);
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
		NS_NEWXPCOM(inst, nsImageMac);
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
		NS_NEWXPCOM(inst, nsDeviceContextSpecMac);
	}
  	else if (mClassID.Equals(kCPrintOptions)) {
    	NS_NEWXPCOM(inst, nsPrintOptionsMac);
  	}
	else if (mClassID.Equals(kCDeviceContextSpecFactory)) {
		NS_NEWXPCOM(inst, nsDeviceContextSpecFactoryMac);
	}
	else if (mClassID.Equals(kImageManagerImpl))
	{
	  nsCOMPtr<nsIImageManager> iManager;
	  nsresult res = NS_NewImageManager(getter_AddRefs(iManager));
	  if (NS_FAILED(res)) return res;
	  return iManager->QueryInterface(aIID, aResult);
	}
	else if (mClassID.Equals(kCFontEnumerator)) {
    nsFontEnumeratorMac* fe;
    NS_NEWXPCOM(fe, nsFontEnumeratorMac);
    inst = (nsISupports *)fe;
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
                                        const char *aProgID,
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
