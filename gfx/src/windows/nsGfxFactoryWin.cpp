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
#include "nsFontMetricsWin.h"
#include "nsRenderingContextWin.h"
#include "nsImageWin.h"
#include "nsDeviceContextWin.h"
#include "nsRegionWin.h"
#include "nsBlender.h"
#include "nsDeviceContextSpecWin.h"
#include "nsDeviceContextSpecFactoryW.h"
#include "nsScriptableRegion.h"
#include "nsIImageManager.h"
#include "nsScreenManagerWin.h"
#include "nsPrintOptionsWin.h"

static NS_DEFINE_IID(kCFontMetrics, NS_FONT_METRICS_CID);
static NS_DEFINE_IID(kCFontEnumerator, NS_FONT_ENUMERATOR_CID);
static NS_DEFINE_IID(kCRenderingContext, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_IID(kCImage, NS_IMAGE_CID);
static NS_DEFINE_IID(kCBlender, NS_BLENDER_CID);
static NS_DEFINE_IID(kCDeviceContext, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_IID(kCRegion, NS_REGION_CID);
static NS_DEFINE_IID(kCDeviceContextSpec, NS_DEVICE_CONTEXT_SPEC_CID);
static NS_DEFINE_IID(kCDeviceContextSpecFactory, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);
static NS_DEFINE_IID(kCDrawingSurface, NS_DRAWING_SURFACE_CID);
static NS_DEFINE_IID(kImageManagerImpl, NS_IMAGEMANAGER_CID);
static NS_DEFINE_IID(kCScreenManager, NS_SCREENMANAGER_CID);
static NS_DEFINE_IID(kCPrintOptions, NS_PRINTOPTIONS_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kCScriptableRegion, NS_SCRIPTABLE_REGION_CID);

class nsGfxFactoryWin : public nsIFactory
{   
  public:   
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFACTORY

    nsGfxFactoryWin(const nsCID &aClass);   
    ~nsGfxFactoryWin();   

  private:   
    nsCID     mClassID;
};   

static int gUseAFunctions = 0;

nsGfxFactoryWin::nsGfxFactoryWin(const nsCID &aClass)   
{   
  static int init = 0;
  if (!init) {
    init = 1;
    OSVERSIONINFO os;
    os.dwOSVersionInfoSize = sizeof(os);
    ::GetVersionEx(&os);
    if ((os.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) &&
        (os.dwMajorVersion == 4) &&
        (os.dwMinorVersion == 0) &&    // Windows 95 (not 98)
        (::GetACP() == 932)) {         // Shift-JIS (Japanese)
      gUseAFunctions = 1;
    }
  }

  NS_INIT_REFCNT();
  mClassID = aClass;
}   

nsGfxFactoryWin::~nsGfxFactoryWin()   
{   
}   

nsresult nsGfxFactoryWin::QueryInterface(const nsIID &aIID,   
                                         void **aResult)   
{   
  if (aResult == NULL) {   
    return NS_ERROR_NULL_POINTER;   
  }   

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  if (aIID.Equals(kISupportsIID)) {   
    *aResult = (void *)(nsISupports*)this;   
  } else if (aIID.Equals(kIFactoryIID)) {   
    *aResult = (void *)(nsIFactory*)this;   
  }   

  if (*aResult == NULL) {   
    return NS_NOINTERFACE;   
  }   

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

NS_IMPL_ADDREF(nsGfxFactoryWin);
NS_IMPL_RELEASE(nsGfxFactoryWin);

nsresult nsGfxFactoryWin::CreateInstance(nsISupports *aOuter,  
                                          const nsIID &aIID,  
                                          void **aResult)  
{ 
  nsresult res;
  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  nsISupports *inst = nsnull;
  PRBool already_addreffed = PR_FALSE;

  if (mClassID.Equals(kCFontMetrics)) {
    nsFontMetricsWin* fm;
    if (gUseAFunctions) {
      NS_NEWXPCOM(fm, nsFontMetricsWinA);
    }
    else {
      NS_NEWXPCOM(fm, nsFontMetricsWin);
    }
    inst = (nsISupports *)fm;
  }
  else if (mClassID.Equals(kCDeviceContext)) {
    nsDeviceContextWin* dc;
    NS_NEWXPCOM(dc, nsDeviceContextWin);
    inst = (nsISupports *)dc;
  }
  else if (mClassID.Equals(kCRenderingContext)) {
    nsRenderingContextWin*  rc;
    if (gUseAFunctions) {
      NS_NEWXPCOM(rc, nsRenderingContextWinA);
    }
    else {
      NS_NEWXPCOM(rc, nsRenderingContextWin);
    }
    inst = (nsISupports *)((nsIRenderingContext*)rc);
  }
  else if (mClassID.Equals(kCImage)) {
    nsImageWin* image;
    NS_NEWXPCOM(image, nsImageWin);
    inst = (nsISupports *)image;
  }
  else if (mClassID.Equals(kCRegion)) {
    nsRegionWin*  region;
    NS_NEWXPCOM(region, nsRegionWin);
    inst = (nsISupports *)region;
  }
  else if (mClassID.Equals(kCBlender)) {
    nsBlender* blender;
    NS_NEWXPCOM(blender, nsBlender);
    inst = (nsISupports *)blender;
  }
  else if (mClassID.Equals(kCDrawingSurface)) {
    nsDrawingSurfaceWin* ds;
    NS_NEWXPCOM(ds, nsDrawingSurfaceWin);
    inst = (nsISupports *)((nsIDrawingSurface *)ds);
  }
  else if (mClassID.Equals(kCDeviceContextSpec)) {
    nsDeviceContextSpecWin* dcs;
    NS_NEWXPCOM(dcs, nsDeviceContextSpecWin);
    inst = (nsISupports *)dcs;
  }
  else if (mClassID.Equals(kCDeviceContextSpecFactory)) {
    nsDeviceContextSpecFactoryWin* dcs;
    NS_NEWXPCOM(dcs, nsDeviceContextSpecFactoryWin);
    inst = (nsISupports *)dcs;
  } 
  else if (mClassID.Equals(kCScriptableRegion)) {
    nsCOMPtr<nsIRegion> rgn;
    NS_NEWXPCOM(rgn, nsRegionWin);
    if (rgn != nsnull) {
      nsIScriptableRegion* scriptableRgn = new nsScriptableRegion(rgn);
      inst = (nsISupports *)scriptableRgn;
    }
  }
  else if (mClassID.Equals(kImageManagerImpl)) {
    nsCOMPtr<nsIImageManager> iManager;
    res = NS_NewImageManager(getter_AddRefs(iManager));
    already_addreffed = PR_TRUE;
    if (NS_SUCCEEDED(res))
    {
      res = iManager->QueryInterface(NS_GET_IID(nsISupports), (void**)&inst);
    }
  }
  else if (mClassID.Equals(kCPrintOptions)) {
    NS_NEWXPCOM(inst, nsPrintOptionsWin);
  }
  else if (mClassID.Equals(kCFontEnumerator)) {
    nsFontEnumeratorWin* fe;
    NS_NEWXPCOM(fe, nsFontEnumeratorWin);
    inst = (nsISupports *)fe;
  } 
	else if (mClassID.Equals(kCScreenManager)) {
		NS_NEWXPCOM(inst, nsScreenManagerWin);
  } 


  if (inst == NULL) {  
    return NS_ERROR_OUT_OF_MEMORY;  
  }  

  if (already_addreffed == PR_FALSE)
    NS_ADDREF(inst);  // Stabilize
  
  res = inst->QueryInterface(aIID, aResult);

  NS_RELEASE(inst); // Destabilize and avoid leaks. Avoid calling delete <interface pointer>  

  return res;  
}  

nsresult nsGfxFactoryWin::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

// return the proper factory to the caller
extern "C" NS_GFXNONXP nsresult NSGetFactory(nsISupports* servMgr,
                                             const nsCID &aClass,
                                             const char *aClassName,
                                             const char *aContractID,
                                             nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsGfxFactoryWin(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}
