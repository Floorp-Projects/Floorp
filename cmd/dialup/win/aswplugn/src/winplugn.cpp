/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
///////////////////////////////////////////////////////////////////////////////
// Winplugn.cpp
//
// Revision History:
// Date        Author            Reason
// ----------------------------------------------------------------------------
// 01/08/97    xxxxxxxxxxxxxx    Add support for win3.x
//             xxxxxxxxxxxxxx    Define APIs
///////////////////////////////////////////////////////////////////////////////

#include <npapi.h>
#include "plugin.h"


// resource include
#ifdef WIN32 // **************************** WIN32 *****************************
#include "resource.h"
#else        // **************************** WIN16 *****************************
#include "asw16res.h"
#endif // !WIN32


#ifdef WIN32  //******************** WIN32 Includes ***********************
#include <winbase.h>
#include <shlguid.h>
#else         //******************** WIN16 Includes ***********************
#include <windows.h>
              //********************** WIN 16 Decls ***********************
#define VER_PLATFORM_WIN16       -1
#endif // WIN32

#include <string.h>
#include "errmsg.h"

// java include
#include "netscape_npasw_SetupPlugin.h"
#include "java_lang_String.h"
#include "netscape_plugin_Plugin.h"

extern BOOL LoadRasFunctions(LPCSTR lpszLibrary);
extern BOOL LoadRasFunctionsNT(LPCSTR lpszLibrary);
extern BOOL LoadRas16Functions();

extern long countRegItems(void* regDataBuf, long regDataLen, BOOL extendLen);
extern java_lang_String * getRegElement(JRIEnv *env, void *RegDataBuf, long itemNum, BOOL extendLen);
extern BOOL getMsgString(char *buf, UINT uID);
extern void CheckDNS();
extern BOOL CheckDUN();           
extern BOOL CheckDUN_NT();
extern void SizeofRAS();
extern void DialerHangup();

// keep a global execution environment
JRIEnv* env;

// Keeps track of OS version, either win95, winNT, or win16 
#ifdef WIN32
int platformOS;
#else
int platformOS = VER_PLATFORM_WIN16;
#endif

// pointer to the data passed by regi server
void* RegDataBuf = NULL;
void* RegDataArray;
long RegDataLength = 0;
BOOL RegExtendedDataFlag = FALSE;
HINSTANCE DLLinstance = NULL;

//JRIGlobalRef  globalRef=NULL; 
JRIGlobalRef  g_globalRefReg = NULL; 


//********************************************************************************
//
// NPP_Initialize
//
// provides global initialization for plug-in
// allocate any memory or resources share by all instance of our plug-in here
//********************************************************************************
NPError NPP_Initialize(void)
{
	// gets the java environment here
    env = NPN_GetJavaEnv();

    // gets the java_lang_String class
    if (env) {
        //register_java_lang_String(env); //Not necessary?
    }

	trace( "initialized plug-in" );
	
#ifdef WIN32 // ******************* WIN32 ***********************
    // gets the OS version
    // note: we need another way to check for win31
    OSVERSIONINFO OsVersionInfo;
    memset(&OsVersionInfo, 0, sizeof(OSVERSIONINFO));
    OsVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    if (GetVersionEx(&OsVersionInfo)) {
        platformOS = (int) OsVersionInfo.dwPlatformId;
    } else {
        return FALSE;
    }
#else      // ********************* WIN16 ***********************
   platformOS = VER_PLATFORM_WIN16;
#endif // WIN32

    return NPERR_NO_ERROR;
}

//*********************************************************************************
//
// NPP_GetJavaClass
//
// return the java class associated with our plug-in
// if no java is used, return NULL
//*********************************************************************************
jref NP_LOADDS NPP_GetJavaClass(void)
{
	struct java_lang_Class* myClass;
	JRIEnv* env = NPN_GetJavaEnv();
	if (env == NULL) {
		return NULL;			/* Java disabled */
	}

	//register_netscape_npasw_SetupPlugin(env); //Not desirable
	use_netscape_plugin_Plugin( env );
	myClass = use_netscape_npasw_SetupPlugin(env);
	
    return (jref)myClass;
}

//*********************************************************************************
//
// NPP_Shutdown
//
// provides global deinitialization for plug-in
// release any memory or resource shared across all instance of our plug-in here
//*********************************************************************************
void NPP_Shutdown(void)
{
	// hang up any live modem connections if we're in the middle of reggie
	if (RegiMode) {
		DialerHangup();
	}

#ifdef WIN32
	// free modem list
	if (ModemList) {
		for (int i=0; i<ModemListLen; i++)
			free(ModemList[i]);
		ModemList = NULL;
	}
#endif
		
	if (env) {
		unuse_netscape_npasw_SetupPlugin(env);
		unuse_netscape_plugin_Plugin(env);
		//unregister_netscape_npasw_SetupPlugin(env);
	//	SetupPlugin::_unregister(env);
	//	SetupPlugin::_unuse(env);
		//unregister_java_lang_String(env); //Not necessary?
	}

	// free loaded libraries
	if (m_hRasInst)
		::FreeLibrary(m_hRasInst);
#ifndef WIN32
   if (m_hRasInst)
		::FreeLibrary(m_hShivaModemWizInst);
#endif // !WIN32
}



//*******************************************************************************
//
// NPP_NEW
//
// called when our plug-in is  instantiated (when an EMBED tag appears
// on a page)
//*******************************************************************************
NPError NP_LOADDS NPP_New(NPMIMEType pluginType,
                          NPP instance,
                          uint16 mode,
                          int16 argc,
                          char* argn[],
                          char* argv[],
                          NPSavedData* saved)
{
        NPError result = NPERR_NO_ERROR;
        PluginInstance* This;

        if (instance == NULL)
                return NPERR_INVALID_INSTANCE_ERROR;
                
		  if (mode == NP_EMBED) {

			// for 16bit only, we need to check and remove startup
			// folder icon created by us
		  }

        
        instance->pdata = NPN_MemAlloc(sizeof(PluginInstance));
        
        This = (PluginInstance*) instance->pdata;
        if (This == NULL)
            return NPERR_OUT_OF_MEMORY_ERROR;

        {
                /* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
                This->fWindow = NULL;
                This->fMode = mode;

                This->fhWnd = NULL;
                This->fDefaultWindowProc = NULL;

        }

        return result;
}


//******************************************************************************
//
// NPP_Destroy
//
// deletes a specific instance of a plug-in and returns an error value
// called when a plug-in instance is deleted
//******************************************************************************
NPError NP_LOADDS NPP_Destroy(NPP instance,
                              NPSavedData** save)
{
        PluginInstance* This;

        if (instance == NULL)
                return NPERR_INVALID_INSTANCE_ERROR;

        This = (PluginInstance*) instance->pdata;

        if (This != NULL) {
                NPN_MemFree(instance->pdata);
                instance->pdata = NULL;
        }

        return NPERR_NO_ERROR;
}


//******************************************************************************
//
// NPP_SetWindow
//
// sets the window in which a plug-in draws
//******************************************************************************
NPError NP_LOADDS NPP_SetWindow(NPP instance,
                                NPWindow* window)
{
        NPError result = NPERR_NO_ERROR;
        PluginInstance* This;

        if (!window)
                return NPERR_GENERIC_ERROR;

        if (!instance)
                return NPERR_INVALID_INSTANCE_ERROR;

        // get the plugin instance object
        This = (PluginInstance*) instance->pdata;

        This->fWindow = window;
        return result;
}



//*******************************************************************************
//
// NPP_NewStream
//
// notifies an instance of a new data stream
//*******************************************************************************
NPError NP_LOADDS NPP_NewStream(NPP instance,
								NPMIMEType type,
								NPStream *stream,
								NPBool seekable,
								uint16 *stype)
{
        PluginInstance* This;

        if (instance == NULL)
                return NPERR_INVALID_INSTANCE_ERROR;

        This = (PluginInstance*) instance->pdata;

		if (type)	{
			RegExtendedDataFlag = (!strcmp(type,"application/x-netscape-autoconfigure-dialer-v2")) ? TRUE:FALSE;
		}


        return NPERR_NO_ERROR;
}



int32 STREAMBUFSIZE = 0X0FFFFFFF; /* If we are reading from a file in NPAsFile
                                   * mode so we can take any size stream in our
                                   * write call (since we ignore it) */

//********************************************************************************
//
// NPP_WriteReady
//
// returns the maximum number of bytes that an instance is prepared to accpet
// in NPP_Write()
//********************************************************************************
int32 NP_LOADDS NPP_WriteReady(NPP instance,
                                                           NPStream *stream)
{
        PluginInstance* This;
        if (instance != NULL)
                This = (PluginInstance*) instance->pdata;

        return STREAMBUFSIZE;
}


//******************************************************************************
//
// NPP_Write
//
// deliveries the data from a stream and return the number of bytes written
//******************************************************************************
int32 NP_LOADDS NPP_Write(NPP instance,
                          NPStream *stream,
                          int32 offset,
                          int32 len,
                          void *buffer)
{
        PluginInstance* This;

        if (instance != NULL)
        {
                This = (PluginInstance*) instance->pdata;
        }

        if (!RegDataBuf) {
                // assume it's the begining of the data stream
                RegDataBuf = (void *)malloc((size_t)(sizeof(char) * len));
        } else {
                RegDataBuf = (void *)realloc(RegDataBuf, (size_t)(sizeof(char) * RegDataLength + len));
        }

        if (len) {
                if (RegDataBuf) {
                        // copy data to buffer
                        memcpy(&((char *)RegDataBuf)[RegDataLength], buffer, (size_t) len);
                        RegDataLength += (long) len;
                }
        }

        return len;             /* The number of bytes accepted */
}


//*******************************************************************************
//
// NPP_DestroyStream
//
// indicates the closure and deletion of a stream
//*******************************************************************************
NPError NP_LOADDS NPP_DestroyStream(NPP instance,
									NPStream *stream,
									NPError reason)
{
        PluginInstance* This;

        if (instance == NULL)
                return NPERR_INVALID_INSTANCE_ERROR;
        This = (PluginInstance*) instance->pdata;

        // if done passing data
        if (reason == NPRES_DONE) {

            JRIEnv* env = NPN_GetJavaEnv();

            if (RegDataBuf && env) {

				java_lang_String *Element;

                // read and parse regi data here
				long numItems = countRegItems(RegDataBuf, RegDataLength, RegExtendedDataFlag);				

				RegDataArray = JRI_NewObjectArray(env, numItems, class_java_lang_String(env), NULL);
				if (RegDataArray == NULL)
					return NULL;

				// lock the JRI array reference, dispose old reference if necessary
				if (g_globalRefReg)
					JRI_DisposeGlobalRef(env, g_globalRefReg); 
				g_globalRefReg = JRI_NewGlobalRef(env, RegDataArray); 

				for (long x=0; x<numItems; x++) {
					Element =  getRegElement(env, RegDataBuf, x, RegExtendedDataFlag);
					JRI_SetObjectArrayElement(env, RegDataArray, x, Element);
				}
			}

            if (RegDataBuf) {
                    // free regi data buffer
                    free(RegDataBuf);
                    RegDataBuf = NULL;
                    // set data length to zero
                    RegDataLength = 0;
            }

        }

        return NPERR_NO_ERROR;
}


//*******************************************************************************
//
// NPP_StreamAsFile
//
// provides a local filename for the data from a stream
//*******************************************************************************
void NP_LOADDS NPP_StreamAsFile(NPP instance,
                                NPStream *stream,
                                const char* fname)
{
        PluginInstance* This;
        if (instance != NULL)
                This = (PluginInstance*) instance->pdata;

}


//******************************************************************************
//
// NPP_Print
//
// request a platform-specific print operation for the instance
//******************************************************************************
void NP_LOADDS NPP_Print(NPP instance, NPPrint* printInfo)
{

        if(printInfo == NULL)
                return;

        if (instance != NULL) {
                PluginInstance* This = (PluginInstance*) instance->pdata;
        
                if (printInfo->mode == NP_FULL) {

                        void* platformPrint = printInfo->print.fullPrint.platformPrint;
                        NPBool printOne = printInfo->print.fullPrint.printOne;
                        
                        /* Do the default*/
                        printInfo->print.fullPrint.pluginPrinted = FALSE;
                }
                else {  /* If not fullscreen, we must be embedded */

                        NPWindow* printWindow = &(printInfo->print.embedPrint.window);
                        void* platformPrint = printInfo->print.embedPrint.platformPrint;
                }
        }
}



//********************************************************************************
//
// NPP_URLNotify
//
// notifies the instance of the comp.etion of a URL request
//********************************************************************************
void NP_LOADDS NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{                    
}


//********************************************************************************
// native method:
//
// SetKiosk
//
// runs navigator in kiosk mode.  (mac only, just a stub here)
//********************************************************************************
extern JRI_PUBLIC_API(void)
native_netscape_npasw_SetupPlugin_SECURE_0005fSetKiosk(JRIEnv* env,
										struct netscape_npasw_SetupPlugin* self,
										jbool a)
{
	return;
}



//********************************************************************************
// native method:
//
// SetKiosk
//
// runs navigator in kiosk mode.  (mac only, just a stub here)
//********************************************************************************
extern JRI_PUBLIC_API(jbool)
native_netscape_npasw_SetupPlugin_SECURE_0005fMilan(JRIEnv* env, struct netscape_npasw_SetupPlugin* self, 
									 struct java_lang_String *a, 
									 struct java_lang_String *b, 
									 jbool c, 
									 jbool d)
{
	return (FALSE);
}



//********************************************************************************
// native method:
//
// CheckEnvironment
//
// checks for DUN, RAS function loading correctly
//********************************************************************************
extern JRI_PUBLIC_API(jbool)
native_netscape_npasw_SetupPlugin_SECURE_0005fCheckEnvironment(JRIEnv* env,
												struct netscape_npasw_SetupPlugin* self)
{
	// try loading RAS routines in RAS dlls
	// if fails return FALSE

    switch (platformOS) {
#ifdef WIN32 // ********************* WIN32 ***********************
      case VER_PLATFORM_WIN32_NT:   // NT
         SizeofRASNT40();           // Sizeof.
         
         //check if it's WinNT40 first
         if (!LoadRasFunctionsNT("RASAPI32.DLL")) {

            // Err: Unable to dynamically load extended RAS functions!                                
            char *buf = (char *)malloc(sizeof(char) * 255);
            if (buf) {
               if (getMsgString(buf, IDS_NO_RAS_FUNCTIONS))
				   DisplayErrMsg(buf, MB_OK | MB_ICONEXCLAMATION);
               free(buf);
            }
                                
            return (FALSE);
         }
         break;

      case VER_PLATFORM_WIN32_WINDOWS:    // defaults to win95
#else
      case VER_PLATFORM_WIN16:            // win16
#endif // WIN32

         SizeofRAS();               // Sizeof.

#ifdef WIN32 // *********************** WIN32 **************************
         if (!LoadRasFunctions("RASAPI32.DLL") && !LoadRasFunctions("RNAPH.DLL")) {
#else        // *********************** WIN16 **************************
         if (!LoadRas16Functions()) {
#endif
            // Err: Unable to dynamically load extended RAS functions!
            char *buf = (char *)malloc(sizeof(char) * 255);
            if (buf) {
               if (getMsgString(buf, IDS_NO_RAS_FUNCTIONS))
                  DisplayErrMsg(buf, MB_OK | MB_ICONEXCLAMATION);
               free(buf);
            }
                                
            return (FALSE);
         }
    }

			// Check to make sure Dial-Up Networking is installed.
			//   It may be uninstalled by user.
			// return FALSE if Dialup Networking is not installed
#ifdef WIN32
			switch (platformOS) {
			  case VER_PLATFORM_WIN32_NT:
				 if (FALSE == CheckDUN_NT()) {
 					char buf[255];
					if (getMsgString((char *)buf, IDS_NO_DUN_INSTALLED))
						DisplayErrMsg((char *)buf, MB_OK | MB_ICONEXCLAMATION);
					return (FALSE);
				 }
				 break;
			  default:
				 if (FALSE == CheckDUN())  {

					char buf[255];
					if (getMsgString((char *)buf, IDS_NO_DUN_INSTALLED))
						DisplayErrMsg((char *)buf, MB_OK | MB_ICONEXCLAMATION);
					return (FALSE);
				 }
				 break;
			}

		   // for win95 only:
			// Check to see if DNS is already configured for a LAN connection.
			// If so warn the user that this may cause conflicts, and continue.
		   if (platformOS == VER_PLATFORM_WIN32_WINDOWS)
			  CheckDNS();
#endif

	return (TRUE);
}

void trace( const char* traceStatement )
{
	if ( !env )
		return;
	
	java_lang_Class* self = (java_lang_Class*)NPP_GetJavaClass();
	if ( !self )
		return;
	
	java_lang_String* traceString = JRI_NewStringPlatform( env, traceStatement,
		strlen( traceStatement), NULL, 0 );
		
	netscape_npasw_SetupPlugin_debug( env, self, traceString );
}


/*******************************************************************************
 * Native Methods: 
 * These are the signatures of the native methods which you must implement.
 ******************************************************************************/
/*** public native intern ()Ljava/lang/String; ***/
extern JRI_PUBLIC_API(struct java_lang_String *)
native_java_lang_String_intern(JRIEnv* env, struct java_lang_String* self)
{
	return NULL;
}


HINSTANCE g_hDllInstance = NULL;


// DDL Entry point for WIN32 & WIN16
#ifdef WIN32 //******************* WIN 32 ***************
//************************************************************************
// DllEntryPoint
//************************************************************************

BOOL WINAPI 
DllMain( HINSTANCE  hinstDLL,           // handle of DLL module 
                DWORD  fdwReason,               // reason for calling function 
                LPVOID  lpvReserved)
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_PROCESS_DETACH:
		{
			DLLinstance = hinstDLL;       // keeps DLL instance
			break;
		}
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			break;
	}
	return TRUE;
}

#else //*********************** WIN16 *******************

//************************************************************************
// DllEntryPoint
//************************************************************************
int FAR PASCAL LibMain(HANDLE hInstance, WORD wDataSeg, WORD wHeapSize, LPSTR lpszCndLine)
{
   if (wHeapSize > 0)
      UnlockData(0);
   g_hDllInstance = hInstance;
   return 1;
}

int FAR PASCAL _export WEP(int nParam)
{
   return 1;
}

#endif
