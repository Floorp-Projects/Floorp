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
#include "nsFontMetricsGTK.h"
#include "nsRenderingContextGTK.h"
#include "nsImageGTK.h"
#include "nsDeviceContextGTK.h"
#include "nsRegionGTK.h"
#include "nsScriptableRegion.h"
#include "nsBlender.h"
#include "nsDeviceContextSpecG.h"
#include "nsDeviceContextSpecFactoryG.h" 
#include "nsIDeviceContextSpecPS.h"
#include "nsIImageManager.h"
#include "nsScreenManagerGtk.h"
#include <gtk/gtk.h>

static NS_DEFINE_IID(kCFontMetrics, NS_FONT_METRICS_CID);
static NS_DEFINE_IID(kCFontEnumerator, NS_FONT_ENUMERATOR_CID);
static NS_DEFINE_IID(kCRenderingContext, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_IID(kCImage, NS_IMAGE_CID);
static NS_DEFINE_IID(kCDeviceContext, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_IID(kCRegion, NS_REGION_CID);
static NS_DEFINE_IID(kCScriptableRegion, NS_SCRIPTABLE_REGION_CID);

static NS_DEFINE_IID(kCBlender, NS_BLENDER_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

static NS_DEFINE_IID(kCDeviceContextSpec, NS_DEVICE_CONTEXT_SPEC_CID);
static NS_DEFINE_IID(kCDeviceContextSpecFactory, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID); 
static NS_DEFINE_IID(kImageManagerImpl, NS_IMAGEMANAGER_CID);
static NS_DEFINE_IID(kCScreenManager, NS_SCREENMANAGER_CID);



class nsGfxFactoryGTK : public nsIFactory
{   
  public:   
    // nsISupports methods
    NS_DECL_ISUPPORTS

    NS_DECL_NSIFACTORY

    nsGfxFactoryGTK(const nsCID &aClass);   
    virtual ~nsGfxFactoryGTK();   

  private:   
    nsCID     mClassID;
};   

nsGfxFactoryGTK::nsGfxFactoryGTK(const nsCID &aClass)   
{   
  NS_INIT_ISUPPORTS();
  mClassID = aClass;
}   

nsGfxFactoryGTK::~nsGfxFactoryGTK()   
{   
}   

nsresult nsGfxFactoryGTK::QueryInterface(const nsIID &aIID,   
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

NS_IMPL_ADDREF(nsGfxFactoryGTK);
NS_IMPL_RELEASE(nsGfxFactoryGTK);

nsresult nsGfxFactoryGTK::CreateInstance(nsISupports *aOuter,  
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
    inst = (nsISupports *)new nsFontMetricsGTK();
  }
  else if (mClassID.Equals(kCDeviceContext)) {
    inst = (nsISupports *)new nsDeviceContextGTK();
  }
  else if (mClassID.Equals(kCRenderingContext)) {
    inst = (nsISupports *)new nsRenderingContextGTK();
  }
  else if (mClassID.Equals(kCImage)) {
    inst = (nsISupports *)new nsImageGTK();
  }
  else if (mClassID.Equals(kCRegion)) {
    nsRegionGTK* dcs;
    NS_NEWXPCOM(dcs, nsRegionGTK);
    inst = (nsISupports *)dcs;
  }
  else if (mClassID.Equals(kCScriptableRegion)) {
    nsCOMPtr<nsIRegion> rgn;
    NS_NEWXPCOM(rgn, nsRegionGTK);
    if (rgn != nsnull) {
      nsCOMPtr<nsIScriptableRegion> scriptableRgn = new nsScriptableRegion(rgn);
      inst = scriptableRgn;
    }
  }
  else if (mClassID.Equals(kCBlender)) {
    inst = (nsISupports *)new nsBlender;
  }
  else if (mClassID.Equals(kCDeviceContextSpec)) {
    nsDeviceContextSpecGTK* dcs;
    NS_NEWXPCOM(dcs, nsDeviceContextSpecGTK);
    inst = (nsISupports *)((nsIDeviceContextSpecPS *)dcs);
  }
  else if (mClassID.Equals(kCDeviceContextSpecFactory)) {
    nsDeviceContextSpecFactoryGTK* dcs;
    NS_NEWXPCOM(dcs, nsDeviceContextSpecFactoryGTK);
    inst = (nsISupports *)dcs;
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
  else if (mClassID.Equals(kCFontEnumerator)) {
    nsFontEnumeratorGTK* fe;
    NS_NEWXPCOM(fe, nsFontEnumeratorGTK);
    inst = (nsISupports *)fe;
  } 
	else if (mClassID.Equals(kCScreenManager)) {
		NS_NEWXPCOM(inst, nsScreenManagerGtk);
  } 
	
  if (inst == NULL) {  
    return NS_ERROR_OUT_OF_MEMORY;  
  }  

  if (already_addreffed == PR_FALSE)
   NS_ADDREF(inst);

  res = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return res;  
}  

nsresult nsGfxFactoryGTK::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

// return the proper factory to the caller
extern "C" NS_GFXNONXP nsresult NSGetFactory(nsISupports* servMgr,
                                             const nsCID &aClass,
                                             const char *aClassName,
                                             const char *aProgID,
                                             nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsGfxFactoryGTK(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}
