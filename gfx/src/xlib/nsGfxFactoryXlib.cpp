/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsGfxCIID.h"
// our local includes for different interfaces
#include "nsFontMetricsXlib.h"
#include "nsDeviceContextXlib.h"
#include "nsImageXlib.h"
#include "nsRegionXlib.h"
#include "nsDrawingSurfaceXlib.h"
#include "nsDeviceContextSpecXlib.h"
#include "nsRenderingContextXlib.h"
#include "nsDeviceContextSpecFactoryX.h"

static NS_DEFINE_IID(kCFontMetrics, NS_FONT_METRICS_CID);
static NS_DEFINE_IID(kCRenderingContext, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_IID(kCImage, NS_IMAGE_CID);
static NS_DEFINE_IID(kCBlender, NS_BLENDER_CID);
static NS_DEFINE_IID(kCDeviceContext, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_IID(kCRegion, NS_REGION_CID);
static NS_DEFINE_IID(kCDeviceContextSpec, NS_DEVICE_CONTEXT_SPEC_CID);
static NS_DEFINE_IID(kCDeviceContextSpecFactory, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);
static NS_DEFINE_IID(kCDrawingSurface, NS_DRAWING_SURFACE_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

class nsGfxFactoryXlib : public nsIFactory
{
public:
  // nsISuports methods
  NS_IMETHOD QueryInterface(const nsIID &aIID,
                                       void **aResult);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
  // nsIFactory Methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);
  
  NS_IMETHOD LockFactory(PRBool aLock);

  nsGfxFactoryXlib(const nsCID &aClass);
  virtual ~nsGfxFactoryXlib();
private:
  nsrefcnt mRefCnt;
  nsCID    mClassID;
};

nsGfxFactoryXlib::nsGfxFactoryXlib(const nsCID &aClass)
{
  mRefCnt = 0;
  mClassID = aClass;
}

nsGfxFactoryXlib::~nsGfxFactoryXlib()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}

nsresult nsGfxFactoryXlib::QueryInterface(const nsIID &aIID,
                                          void **aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  // always set this to zero, in case of a failure
  *aResult = NULL;
  if (aIID.Equals(kISupportsIID)) {
    *aResult = (void *)(nsISupports*)this;
  } else if (aIID.Equals(kIFactoryIID)) {
    *aResult = (void *)(nsIFactory*)this;
  }

  if (*aResult == NULL) {
    return NS_NOINTERFACE;
  }
  AddRef();  // increase reference count for caller
  return NS_OK;
}

nsrefcnt nsGfxFactoryXlib::AddRef()
{
  return ++mRefCnt;
}

nsrefcnt nsGfxFactoryXlib::Release()
{
  if (--mRefCnt == 0) {
    delete this;
    return 0; // don't access mRefCnt after a delete!
  }
  return mRefCnt;
}

nsresult nsGfxFactoryXlib::CreateInstance(nsISupports *aOuter,
                                          const nsIID &aIID,
                                          void **aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  } 
  
  *aResult = NULL;

  nsISupports *inst = nsnull;

  if (mClassID.Equals(kCFontMetrics)) {
    nsFontMetricsXlib* fm;
    NS_NEWXPCOM(fm, nsFontMetricsXlib);
    inst = (nsISupports *)fm;
  }
  else if (mClassID.Equals(kCDeviceContext)) {
    nsDeviceContextXlib* dc;
    NS_NEWXPCOM(dc, nsDeviceContextXlib);
    inst = (nsISupports *)dc;
  }
  else if (mClassID.Equals(kCRenderingContext)) {
    nsRenderingContextXlib*  rc;
    NS_NEWXPCOM(rc, nsRenderingContextXlib);
    inst = (nsISupports *)((nsIRenderingContext*)rc);
  }
  else if (mClassID.Equals(kCImage)) {
    nsImageXlib* image;
    NS_NEWXPCOM(image, nsImageXlib);
    inst = (nsISupports *)image;
  }
  else if (mClassID.Equals(kCRegion)) {
    nsRegionXlib*  region;
    NS_NEWXPCOM(region, nsRegionXlib);
    inst = (nsISupports *)region;
  }
  // XXX blender doesn't exist yet for xlib...
  //  else if (mClassID.Equals(kCBlender)) {
  //    nsBlenderXlib* blender;
  //    NS_NEWXPCOM(blender, nsBlenderXlib);
  //    inst = (nsISupports *)blender;
  //  }
  else if (mClassID.Equals(kCBlender)) {
    nsDrawingSurfaceXlib* ds;
    NS_NEWXPCOM(ds, nsDrawingSurfaceXlib);
    inst = (nsISupports *)((nsIDrawingSurface *)ds);
  }
  else if (mClassID.Equals(kCDeviceContextSpec)) {
    nsDeviceContextSpecXlib* dcs;
    NS_NEWXPCOM(dcs, nsDeviceContextSpecXlib);
    inst = (nsISupports *)dcs;
  }
  else if (mClassID.Equals(kCDeviceContextSpecFactory)) {
    nsDeviceContextSpecFactoryXlib* dcs;
    NS_NEWXPCOM(dcs, nsDeviceContextSpecFactoryXlib);
    inst = (nsISupports *)dcs;
  }

  if (inst == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {
    // We didn't get the right interface, so clean up  
    delete inst;
  }
//  else {
//    inst->Release();
//  }

  return res;
}

nsresult nsGfxFactoryXlib::LockFactory(PRBool aLock)
{
  return NS_OK;
}

extern "C" NS_GFXNONXP nsresult NSGetFactory(nsISupports *servMgr,
                                             const nsCID &aClass,
                                             const char *aClassName,
                                             const char *aProgID,
                                             nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }
  *aFactory = new nsGfxFactoryXlib(aClass);
  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return (*aFactory)->QueryInterface(kIFactoryIID, (void **)aFactory);
}
