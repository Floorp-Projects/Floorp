/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 *
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3.
 *
 * Date         Modified by     Description of modification
 * 05/10/2000   IBM Corp.      Make it look more like Windows.
 */

// ToDo: nowt (except get rid of unicode hack)
#define INCL_DOS
#include "nsGfxDefs.h"
#include "libprint.h"
#include <stdio.h>

//#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsGfxCIID.h"
#include "nsFontMetricsOS2.h"
#include "nsRenderingContextOS2.h"
#include "nsImageOS2.h"
#include "nsDeviceContextOS2.h"
#include "nsRegionOS2.h"
#include "nsBlender.h"
#include "nsDeviceContextSpecOS2.h"
#include "nsDeviceContextSpecFactoryO.h"
//#include "nsScriptableRegion.h"
#include "nsIImageManager.h"
#include "nsScreenManagerOS2.h"
#include "nsString.h"

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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kCScriptableRegion, NS_SCRIPTABLE_REGION_CID);


class nsGfxFactoryOS2 : public nsIFactory
{   
 public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFACTORY

    nsGfxFactoryOS2(const nsCID &aClass);   
    ~nsGfxFactoryOS2();   

 private:
   nsCID     mClassID;
};

static int gUseAFunctions = 0;

nsGfxFactoryOS2::nsGfxFactoryOS2(const nsCID &aClass)
{
  static int init = 0;
  if (!init) {
    init = 1;
  /* OS2TODO
    OSVERSIONINFO os;
    os.dwOSVersionInfoSize = sizeof(os);
    ::GetVersionEx(&os);
    if ((os.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) &&
        (os.dwMajorVersion == 4) &&
        (os.dwMinorVersion == 0) &&    // Windows 95 (not 98)
        (::GetACP() == 932)) {         // Shift-JIS (Japanese)
      gUseAFunctions = 1;
    }
  */
  }

  NS_INIT_REFCNT();
  mClassID = aClass;
}

nsGfxFactoryOS2::~nsGfxFactoryOS2()   
{   
}   

nsresult nsGfxFactoryOS2::QueryInterface(const nsIID &aIID,   
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

NS_IMPL_ADDREF(nsGfxFactoryOS2);
NS_IMPL_RELEASE(nsGfxFactoryOS2);
 
nsresult nsGfxFactoryOS2::CreateInstance(nsISupports *aOuter,
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
    nsFontMetricsOS2* fm;
    if (gUseAFunctions) {
     // NS_NEWXPCOM(fm, nsFontMetricsOS2A);
    }
    else {
      NS_NEWXPCOM(fm, nsFontMetricsOS2);
    }
    inst = (nsISupports *)fm;
  }
  else if (mClassID.Equals(kCDeviceContext)) {
    nsDeviceContextOS2* dc;
    NS_NEWXPCOM(dc, nsDeviceContextOS2);
    inst = (nsISupports *)dc;
  }
  else if (mClassID.Equals(kCRenderingContext)) {
    nsRenderingContextOS2*  rc;
    if (gUseAFunctions) {
     // NS_NEWXPCOM(rc, nsRenderingContextOS2A);
    }
    else {
      NS_NEWXPCOM(rc, nsRenderingContextOS2);
    }
    inst = (nsISupports *)((nsIRenderingContext*)rc);
  }
  else if (mClassID.Equals(kCImage)) {
    nsImageOS2* image;
    NS_NEWXPCOM(image, nsImageOS2);
    inst = (nsISupports *)image;
  }
  else if (mClassID.Equals(kCRegion)) {
    nsRegionOS2*  region;
    NS_NEWXPCOM(region, nsRegionOS2);
    inst = (nsISupports *)region;
  }
  else if (mClassID.Equals(kCBlender)) {
    nsBlender* blender;
    NS_NEWXPCOM(blender, nsBlender);
    inst = (nsISupports *)blender;
  }
  // OS2TODO
  /*else if (mClassID.Equals(kCDrawingSurface)) {
    nsDrawingSurfaceOS2* ds;
    NS_NEWXPCOM(ds, nsDrawingSurfaceOS2);
    inst = (nsISupports *)((nsIDrawingSurface *)ds);
  }*/
  else if (mClassID.Equals(kCDeviceContextSpec)) {
    nsDeviceContextSpecOS2* dcs;
    NS_NEWXPCOM(dcs, nsDeviceContextSpecOS2);
    inst = (nsISupports *)dcs;
  }
  else if (mClassID.Equals(kCDeviceContextSpecFactory)) {
    nsDeviceContextSpecFactoryOS2* dcs;
    NS_NEWXPCOM(dcs, nsDeviceContextSpecFactoryOS2);
    inst = (nsISupports *)dcs;
  }
  else if (mClassID.Equals(kCScriptableRegion)) {
    nsCOMPtr<nsIRegion> rgn;
    NS_NEWXPCOM(rgn, nsRegionOS2);
    /*if (rgn != nsnull) {
      nsIScriptableRegion* scriptableRgn = new nsScriptableRegion(rgn);
      inst = (nsISupports *)scriptableRgn;
    }*/
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
    nsFontEnumeratorOS2* fe;
    NS_NEWXPCOM(fe, nsFontEnumeratorOS2);
    inst = (nsISupports *)fe;
  }
	else if (mClassID.Equals(kCScreenManager)) {
		NS_NEWXPCOM(inst, nsScreenManagerOS2);
  } 


  if( inst == NULL) {
     return NS_ERROR_OUT_OF_MEMORY;
  }

  if (already_addreffed == PR_FALSE)
     NS_ADDREF(inst);  // Stabilize

  res = inst->QueryInterface(aIID, aResult);
 
  NS_RELEASE(inst); // Destabilize and avoid leaks. Avoid calling delete <interface pointer> 
   
  return res;
}  

nsresult nsGfxFactoryOS2::LockFactory(PRBool aLock)  
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

  *aFactory = new nsGfxFactoryOS2(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}

// Module-level data ---------------------------------------------------------
void PMERROR( const char *api)
{
   ERRORID eid = WinGetLastError(0);
   USHORT usError = ERRORIDERROR(eid);
   printf( "%s failed, error = 0x%X\n", api, usError);
}

nsGfxModuleData::nsGfxModuleData() : hModResources(0), hpsScreen(0),
                                     lDisplayDepth(0)
{}

void nsGfxModuleData::Init()
{
   char   buffer[CCHMAXPATH];
   APIRET rc;

   rc = DosLoadModule( buffer, CCHMAXPATH, "GFX_OS2", &hModResources);

   if( rc)
   {
      printf( "Gfx failed to load self.  rc = %d, cause = %s\n", (int)rc, buffer);
      // rats.  Can't load ourselves.  Oh well.  Try to be harmless...
      hModResources = 0;
   }
   PrnInitialize( hModResources);

   // get screen bit-depth
   hpsScreen = WinGetScreenPS( HWND_DESKTOP);
   HDC hdc = GpiQueryDevice( hpsScreen);
   DevQueryCaps( hdc, CAPS_COLOR_BITCOUNT, 1, &lDisplayDepth);
}

nsGfxModuleData::~nsGfxModuleData()
{
  /* Free any converters that were created */
  for (int i=0; i < 15 /* eCharSet_COUNT from nsFontMetricsOS2.cpp */ ; i++ ) {
    if (gUconvInfo[i].mConverter) {
      UniFreeUconvObject(gUconvInfo[i].mConverter);
    } /* endif */
  } /* endfor */

  PrnTerminate();
  if( hModResources)
     DosFreeModule( hModResources);
  WinReleasePS( hpsScreen);
}

nsGfxModuleData gModuleData;


int WideCharToMultiByte( int CodePage, const PRUnichar *pText, ULONG ulLength, char* szBuffer, ULONG ulSize )
{
  UconvObject* pConverter = 0;
  /* Free any converters that were created */
  for (int i=0; i < 15 /* eCharSet_COUNT from nsFontMetricsOS2.cpp */ ; i++ ) {
    if (gUconvInfo[i].mCodePage == CodePage) {
      if (!gUconvInfo[i].mConverter) {
        UniChar codepage[20];
        int unirc = UniMapCpToUcsCp( CodePage, codepage, 20);
        UniCreateUconvObject( codepage, &gUconvInfo[i].mConverter);
        break;
      } /* endif */
      pConverter = &gUconvInfo[i].mConverter;
    } /* endif */
  } /* endfor */
  if (!pConverter) {
      pConverter = &gUconvInfo[0].mConverter;
  } /* endif */

  UniChar *ucsString = (UniChar*) pText;
  size_t   ucsLen = ulLength;
  size_t   cplen = ulSize;
  size_t   cSubs = 0;

  char *tmp = szBuffer; // function alters the out pointer

   int unirc = UniUconvFromUcs( *pConverter, &ucsString, &ucsLen,
                                (void**) &tmp, &cplen, &cSubs);

  if( unirc == UCONV_E2BIG) // k3w1
  {
    // terminate output string (truncating)
    *(szBuffer + ulSize - 1) = '\0';
  }
  else if( unirc != ULS_SUCCESS)
  {
     printf("very bad");
  }
  return ulSize - cplen;
}
