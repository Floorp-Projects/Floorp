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
#include "nsFontMetricsPh.h"
#include "nsRenderingContextPh.h"
#include "nsImagePh.h"
#include "nsDeviceContextPh.h"
#include "nsRegionPh.h"
#include "nsScriptableRegion.h"
#include "nsBlender.h"
#include "nsDeviceContextSpecPh.h"
#include "nsDeviceContextSpecFactoryP.h"
#include "nsIImageManager.h"
#include "nsPhGfxLog.h"

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



class nsGfxFactoryPh : public nsIFactory
{   
  public:   
    // nsISupports methods
    NS_DECL_ISUPPORTS

    NS_DECL_NSIFACTORY

    nsGfxFactoryPh(const nsCID &aClass);   
    ~nsGfxFactoryPh();   

  private:   
    nsCID     mClassID;
};   

nsGfxFactoryPh::nsGfxFactoryPh(const nsCID &aClass)   
{   
  NS_INIT_ISUPPORTS();
  mClassID = aClass;
}   

nsGfxFactoryPh::~nsGfxFactoryPh()   
{   
  PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::~nsGfxFactoryPh Destructor\n"));
}   

nsresult nsGfxFactoryPh::QueryInterface(const nsIID &aIID,   
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

NS_IMPL_ADDREF(nsGfxFactoryPh);
NS_IMPL_RELEASE(nsGfxFactoryPh);

nsresult nsGfxFactoryPh::CreateInstance(nsISupports *aOuter,  
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
  
  if (mClassID.Equals(kCFontMetrics))
  {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::CreateInstance asking for nsFontMetricsPh.\n"));
    inst = (nsISupports *) new nsFontMetricsPh();
  }
  else if (mClassID.Equals(kCDeviceContext)) {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::CreateInstance asking for nsDeviceContextPh.\n"));
    inst = (nsISupports *)new nsDeviceContextPh();
  }
  else if (mClassID.Equals(kCRenderingContext)) {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::CreateInstance asking for nsRenderingContextPh.\n"));
    inst = (nsISupports *)new nsRenderingContextPh();
  }
  else if (mClassID.Equals(kCImage)) {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::CreateInstance asking for nsImagePh.\n"));
    inst = (nsISupports *)new nsImagePh();
  }
  else if (mClassID.Equals(kCRegion)) {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::CreateInstance asking for nsRegionPh.\n"));
    nsRegionPh*  region;
    NS_NEWXPCOM(region, nsRegionPh);
    inst = (nsISupports *)region;
  }
  else if (mClassID.Equals(kCScriptableRegion)) {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::CreateInstance asking for kCScriptableRegion nsRegionPh.\n"));
    nsCOMPtr<nsIRegion> rgn;
    NS_NEWXPCOM(rgn, nsRegionPh);
    if (rgn != nsnull) {
      nsCOMPtr<nsIScriptableRegion> scriptableRgn = new nsScriptableRegion(rgn);
      inst = scriptableRgn;
    }
  }
  else if (mClassID.Equals(kCBlender)) {
    inst = (nsISupports *)new nsBlender;
  }
  else if (mClassID.Equals(kCDeviceContextSpec)) {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::CreateInstance asking for nsDeviceContextSpecPh.\n"));
    nsDeviceContextSpecPh* dcs;
    NS_NEWXPCOM(dcs, nsDeviceContextSpecPh);
    inst = (nsISupports *)dcs;
  }
  else if (mClassID.Equals(kCDeviceContextSpecFactory)) {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::CreateInstance asking for nsDeviceContextSpecFactoryPh.\n"));
    nsDeviceContextSpecFactoryPh* dcs;
    NS_NEWXPCOM(dcs, nsDeviceContextSpecFactoryPh);
    inst = (nsISupports *)dcs;
  }
  else if (mClassID.Equals(kImageManagerImpl)) {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::CreateInstance asking for ImageManagerImpl.\n"));
    nsCOMPtr<nsIImageManager> iManager;
    res = NS_NewImageManager(getter_AddRefs(iManager));
    already_addreffed = PR_TRUE;
    if (NS_SUCCEEDED(res))
    {
      res = iManager->QueryInterface(NS_GET_IID(nsISupports), (void**)&inst);
    }
  }          
  else if (mClassID.Equals(kCFontEnumerator)) {
    PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::CreateInstance asking for FontEnumerator.\n"));
    nsFontEnumeratorPh* fe;
    NS_NEWXPCOM(fe, nsFontEnumeratorPh);
    inst = (nsISupports *)fe;
  } 

  if (inst == NULL)
  {  
    PR_LOG(PhGfxLog, PR_LOG_ERROR,("nsGfxFactoryPh::CreateInstance Failed.\n"));
    return NS_ERROR_OUT_OF_MEMORY;  
  }  

  if (already_addreffed == PR_FALSE)
   NS_ADDREF(inst);

  res = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);
  
  return res;  
}  

nsresult nsGfxFactoryPh::LockFactory(PRBool aLock)  
{  
 PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::LockFactory - Not Implmented\n"));

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
 PR_LOG(PhGfxLog, PR_LOG_DEBUG,("nsGfxFactoryPh::NSGetFactory\n"));

  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsGfxFactoryPh(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}
