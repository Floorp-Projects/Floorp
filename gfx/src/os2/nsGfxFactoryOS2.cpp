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
 *
 */

// ToDo: nowt (except get rid of unicode hack)
#define INCL_DOS
#include "nsGfxDefs.h"
#include "libprint.h"
#include <stdio.h>

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsGfxCIID.h"
#include "nsFontMetricsOS2.h"
#include "nsRenderingContextOS2.h"
#include "nsImageOS2.h"
#include "nsDeviceContextOS2.h"
#include "nsRegionOS2.h"
#include "nsBlender.h"
#include "nsPaletteOS2.h"
#include "nsDeviceContextSpecOS2.h"
#include "nsDeviceContextSpecFactoryO.h"

// nsGfxFactory.cpp - factory for creating os/2 graphics objects.

static NS_DEFINE_IID(kCFontMetrics, NS_FONT_METRICS_CID);
static NS_DEFINE_IID(kCRenderingContext, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_IID(kCImage, NS_IMAGE_CID);
static NS_DEFINE_IID(kCDeviceContext, NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_IID(kCRegion, NS_REGION_CID);
static NS_DEFINE_IID(kCBlender, NS_BLENDER_CID);
static NS_DEFINE_IID(kCDeviceContextSpec, NS_DEVICE_CONTEXT_SPEC_CID);
static NS_DEFINE_IID(kCDeviceContextSpecFactory, NS_DEVICE_CONTEXT_SPEC_FACTORY_CID);

class nsGfxFactoryOS2 : public nsIFactory
{   
 public:

   NS_DECL_ISUPPORTS

   // nsIFactory methods   
   NS_IMETHOD CreateInstance( nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

   NS_IMETHOD LockFactory( PRBool aLock) { return NS_OK; }

   nsGfxFactoryOS2( const nsCID &aClass);
   virtual ~nsGfxFactoryOS2();

 private:
   nsCID     mClassID;
};

nsGfxFactoryOS2::nsGfxFactoryOS2( const nsCID &aClass)
{
   NS_INIT_REFCNT();
   mClassID = aClass;
}

nsGfxFactoryOS2::~nsGfxFactoryOS2()   
{   
   NS_ASSERTION( mRefCnt == 0, "non-zero refcnt at destruction");   
}   

NS_IMPL_ISUPPORTS(nsGfxFactoryOS2,nsIFactory::GetIID())

nsresult nsGfxFactoryOS2::CreateInstance( nsISupports *aOuter,
                                          const nsIID &aIID,
                                          void **aResult)
{  
   if( !aResult)
      return NS_ERROR_NULL_POINTER;  

   *aResult = 0;

   nsISupports *inst = nsnull;

   if( mClassID.Equals( kCFontMetrics)) {
      inst = new nsFontMetricsOS2;
   }
   else if( mClassID.Equals( kCDeviceContext)) {
      inst = new nsDeviceContextOS2;
   }
   else if( mClassID.Equals( kCRenderingContext)) {
      inst = (nsISupports *)((nsIRenderingContext*)new nsRenderingContextOS2);
   }
   else if( mClassID.Equals( kCImage)) {
      inst = new nsImageOS2;
   }
   else if( mClassID.Equals( kCRegion)) {
      inst = new nsRegionOS2;
   }
   else if( mClassID.Equals( kCBlender)) {
      inst = new nsBlender;
   }
   else if( mClassID.Equals( kCDeviceContextSpec)) {
      inst = new nsDeviceContextSpecOS2;
   }
   else if( mClassID.Equals( kCDeviceContextSpecFactory)) {
      inst = new nsDeviceContextSpecFactoryOS2;
   }

   if( !inst)
      return NS_ERROR_OUT_OF_MEMORY;
 
   nsresult res = inst->QueryInterface(aIID, aResult);
 
   if( NS_FAILED(res))
      // We didn't get the right interface, so clean up
      delete inst;

   return res;
}  

// This is a factory-factory: create a factory for the desired type.
extern "C" NS_GFXNONXP nsresult NSGetFactory(nsISupports* servMgr,
                                             const nsCID &aClass,
                                             const char *aClassName,
                                             const char *aProgID,
                                             nsIFactory **aFactory)
{
   if( !aFactory)
     return NS_ERROR_NULL_POINTER;

   *aFactory = new nsGfxFactoryOS2( aClass);

   if( !*aFactory)
    return NS_ERROR_OUT_OF_MEMORY;

  return (*aFactory)->QueryInterface( nsIFactory::GetIID(), (void**) aFactory);
}

// Module-level data ---------------------------------------------------------
void PMERROR( const char *api)
{
   ERRORID eid = WinGetLastError(0);
   USHORT usError = ERRORIDERROR(eid);
   printf( "%s failed, error = 0x%X\n", api, usError);
}

nsGfxModuleData::nsGfxModuleData() : hModResources(0), hpsScreen(0),
                                     lDisplayDepth(0), uiPalette(0)
{}

void nsGfxModuleData::Init()
{
   char   buffer[CCHMAXPATH];
   APIRET rc;

   rc = DosLoadModule( buffer, CCHMAXPATH, "GFXOS2", &hModResources);

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

   // XXX XXX temp hack XXX XXX XXX
   converter = 0;
   supplantConverter = FALSE;
   if( !getenv( "MOZ_DONT_DRAW_UNICODE"))
   {
      ULONG ulDummy = 0;
      renderingHints = 0;
      DosQueryCp( 4, &ulCodepage, &ulDummy);
   }
   else
   {
      renderingHints = NS_RENDERING_HINT_FAST_8BIT_TEXT;
      ulCodepage = 1004;
   }
   // XXX XXX end temp hack XXX XXX XXX
}

nsGfxModuleData::~nsGfxModuleData()
{
   // XXX XXX temp hack XXX XXX XXX
   if( converter)
      UniFreeUconvObject( converter);
   // XXX XXX end temp hack XXX XXX XXX

   PrnTerminate();
   if( hModResources)
      DosFreeModule( hModResources);
   WinReleasePS( hpsScreen);

   NS_IF_RELEASE(uiPalette);
}

nsIPaletteOS2 *nsGfxModuleData::GetUIPalette( nsIDeviceContext *aContext)
{
   if( !uiPalette)
      NS_CreatePalette( aContext, uiPalette);

   NS_ADDREF(uiPalette);

   return uiPalette;
}

nsGfxModuleData gModuleData;

// XXX XXX XXX XXX Temp hack until font-switching comes on-line XXX XXX XXX XXX

// Conversion from unicode to appropriate codepage
char *nsGfxModuleData::ConvertFromUcs( const PRUnichar *pText, ULONG ulLength,
                                       char *szBuffer, ULONG ulSize)
{
   if( supplantConverter)
   {
      // We couldn't create a converter for some reason, so do this 'by hand'.
      // Note this algorithm is fine for most of most western charsets, but
      // fails dismally for various glyphs, baltic, points east...
      ULONG ulCount = 0;
      char *szSave = szBuffer;
      while( *pText && ulCount < ulSize - 1) // (one for terminator)
      {
         *szBuffer = (char) *pText;
         szBuffer++;
         pText++;
         ulCount++;
      }

      // terminate string
      *szBuffer = '\0';

      return szSave;
   }

   if( !converter)
   {
      // Create a converter from unicode to a codepage which PM can display.
      UniChar codepage[20];
      int unirc = UniMapCpToUcsCp( 0, codepage, 20);
      if( unirc == ULS_SUCCESS)
      {
         unirc = UniCreateUconvObject( codepage, &converter);
         // XXX do we need to set substitution options here?
      }
      if( unirc != ULS_SUCCESS)
      {
         supplantConverter = TRUE;
         renderingHints = NS_RENDERING_HINT_FAST_8BIT_TEXT;
         ulCodepage = 1004;
         printf( "Couldn't create gfx unicode converter.\n");
         return ConvertFromUcs( pText, szBuffer, ulSize);
      }
   }

   // Have converter, now get it to work...

   UniChar *ucsString = (UniChar*) pText;
   size_t   ucsLen = ulLength;
   size_t   cplen = ulSize;
   size_t   cSubs = 0;

   char *tmp = szBuffer; // function alters the out pointer

   int unirc = UniUconvFromUcs( converter, &ucsString, &ucsLen,
                                (void**) &tmp, &cplen, &cSubs);

   if( unirc == UCONV_E2BIG) // k3w1
   {
      // terminate output string (truncating)
      *(szBuffer + ulSize - 1) = '\0';
   }
   else if( unirc != ULS_SUCCESS)
   {
      printf( "UniUconvFromUcs failed, rc %X\n", unirc);
      supplantConverter = TRUE;
      szBuffer = ConvertFromUcs( pText, szBuffer, ulSize);
      supplantConverter = FALSE;
   }

   return szBuffer;
}

char *nsGfxModuleData::ConvertFromUcs( const nsString &aString,
                                       char *szBuffer, ULONG ulSize)
{
   char *szRet = 0;
   const PRUnichar *pUnicode = aString.GetUnicode();

   if( pUnicode)
      szRet = ConvertFromUcs( pUnicode, aString.Length() + 1, szBuffer, ulSize);
   else
      szRet = aString.ToCString( szBuffer, ulSize);

   return szRet;
}

const char *nsGfxModuleData::ConvertFromUcs( const PRUnichar *pText, ULONG ulLength)
{
   // This is probably okay; longer strings will be truncated but istr there's
   // a PM limit on things like windowtext
   // (which these routines are usually used for)

   static char buffer[1024]; // XXX (multithread)
   *buffer = '\0';
   return ConvertFromUcs( pText, ulLength, buffer, 1024);
}

const char *nsGfxModuleData::ConvertFromUcs( const nsString &aString)
{
   const char *szRet = 0;
   const PRUnichar *pUnicode = aString.GetUnicode();

   if( pUnicode)
      szRet = ConvertFromUcs( pUnicode, aString.Length() + 1);
   else
      szRet = aString.GetBuffer(); // hrm.

   return szRet;
}

// XXX XXX XXX XXX End Temp hack until font-switching comes on-line XXX XXX XXX
