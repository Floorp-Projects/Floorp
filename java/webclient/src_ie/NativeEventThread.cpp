/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Glenn Barney <gbarney@uiuc.edu>
 */

#define IDD_MYDIALOG  128
#define _WIN32_WINNT  0x0400
#define _WIN32_IE	0x0400

#include "NativeEventThread.h"

#include "ie_globals.h"
#include "ie_util.h"

#include "CMyDialog.h"

#include <atlbase.h> //for CComPtr

 CAppModule _Module;
 

#include <Atlwin.h> // for AtlWin
#include <Atlcom.h>

#include <Exdisp.h>  //for IWebBrowser2
#include <exdispid.h>

#include <atlhost.h>
//#include <atlframe.h>//WTL
#include <atlctrls.h>//WTL
//#include <atlctrlw.h>//WTL
//#include <atlmisc.h>//WTL
//#include <atlimpl.cpp>
#include <objbase.h>


#ifdef XP_PC
#include <windows.h>
#endif

#include <stdio.h>

HWND localParent;  //these two are temporarily being used in order to test the
HWND localChild;   //OnCommandStateChange functions, they may be eventually removed


//
// Local functions
//

//initializes IE stuff
HRESULT InitIEStuff (WebShellInitContext * arg);
int processEventLoop(WebShellInitContext *initContext);

//
// Local data
//



extern void util_ThrowExceptionToJava (JNIEnv * , const char * );

char * errorMessages[] = {
	"No Error",
	"Could not obtain the event queue service.",
	"Unable to create the WebShell instance.",
	"Unable to initialize the WebShell instance.",
	"Unable to show the WebShell."
};

//
// JNI methods
//

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeInitialize
(JNIEnv *env, jobject obj, jint webShellPtr)
{
	
	WebShellInitContext * initContext = (WebShellInitContext *) webShellPtr;
	if (NULL == initContext) {
	   ::util_ThrowExceptionToJava (env,
			   "NULL webShellPtr passed to nativeInitialize.");
	   return;
	}


	InitIEStuff (initContext);

} 

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeProcessEvents
(JNIEnv *env, jobject obj, jint webShellPtr)
{

    WebShellInitContext * initContext = (WebShellInitContext *) webShellPtr;
    
    if (nsnull == initContext) {
	    ::util_ThrowExceptionToJava(env, 
                                    "NULL webShellPtr passed to nativeProcessEvents.");
        return;
    }
    processEventLoop(initContext);


}

 /**

 * <P> This method takes the typedListener, which is a
 * WebclientEventListener java subclass, figures out what type of
 * subclass it is, using the gSupportedListenerInterfaces array, and
 * calls the appropriate add*Listener local function.  </P>

 *<P> PENDING(): we could do away with the switch statement using
 * function pointers, or some other mechanism.  </P>

 * <P>the NewGlobalRef call is very important, since the argument
 * typedListener is used to call back into java, at another time, as a
 * result of the a mozilla event being fired.</P>

 * PENDING(): implement nativeRemoveListener, which must call
 * RemoveGlobalRef.

 * See the comments for EventRegistration.h:addDocumentLoadListener

 */

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeAddListener
(JNIEnv *env, jobject obj, jint webShellPtr, jobject typedListener, 
 jstring listenerString)
{
    WebShellInitContext *initContext = (WebShellInitContext *)webShellPtr;
    
    if (initContext == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null initContext passed tonativeAddListener");
        return;
    }
    
    if (nsnull == initContext->nativeEventThread) {
        // store the java EventRegistrationImpl class in the initContext
        initContext->nativeEventThread = 
            ::util_NewGlobalRef(env, obj); // VERY IMPORTANT!!
        
        // This enables the listener to call back into java
    }
    
    jclass clazz = nsnull;
    int listenerType = 0;
    const char *listenerStringChars = ::util_GetStringUTFChars(env, 
                                                               listenerString);
    if (listenerStringChars == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeAddListener: Can't get className for listener.");
        return;
    }
    
    while (nsnull != gSupportedListenerInterfaces[listenerType]) {
        if (0 == strcmp(gSupportedListenerInterfaces[listenerType], 
                        listenerStringChars)) {
            // We've got a winner!
            break;
        }
        listenerType++;
    }
    ::util_ReleaseStringUTFChars(env, listenerString, listenerStringChars);
    listenerStringChars = nsnull;
    
    if (LISTENER_NOT_FOUND == (LISTENER_CLASSES) listenerType) {
        ::util_ThrowExceptionToJava(env, "Exception: NativeEventThread.nativeAddListener(): can't find listener \n\tclass for argument");
        return;
    }
    
    jobject globalRef = nsnull;
    
    // PENDING(edburns): make sure do DeleteGlobalRef on the removeListener
    if (nsnull == (globalRef = ::util_NewGlobalRef(env, typedListener))) {
        ::util_ThrowExceptionToJava(env, "Exception: NativeEventThread.nativeAddListener(): can't create NewGlobalRef\n\tfor argument");
        return;
    }
    util_Assert(initContext->browserObject);
    
    switch(listenerType) {
    case DOCUMENT_LOAD_LISTENER:
        initContext->browserObject->AddDocumentLoadListener(globalRef); 
        break;
    }

    return;
    
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeRemoveListener
(JNIEnv *env, jobject obj, jint webShellPtr, jobject typedListener,
 jstring listenerString)
{
    printf("debug: glenn: nativeRemoveListener\n");
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeCleanUp
(JNIEnv *env, jobject obj, jint webShellPtr)
{

  WebShellInitContext * initContext = (WebShellInitContext *) webShellPtr;
  	
  //AtlAdviseSinkMap(&browserHome, false)
  
	//_Module.RemoveMessageLoop();
    initContext->browserObject->DispEventUnadvise(initContext->browserObject->spUnk);
  	_Module.Term();
    ::CoUninitialize();
  
}


int processEventLoop(WebShellInitContext * initContext)
{

	HRESULT hr;
    MSG msg;


    if (::PeekMessage(&msg, nsnull, 0, 0, PM_NOREMOVE)) {
        if (::GetMessage(&msg, nsnull, 0, 0)) {
			
			switch (msg.message)
			{
			case WM_REFRESH:
				hr = (initContext->browserObject->m_pWB)->Refresh();
				break;
			case WM_NAVIGATE:
 				hr = (initContext->browserObject->m_pWB)->Navigate(CComBSTR(initContext->wcharURL), NULL, NULL, NULL, NULL);
                free((void *) initContext->wcharURL);
                initContext->wcharURL = NULL;
				break;
			case WM_BACK:
				hr = (initContext->browserObject->m_pWB)->GoBack();
				break;
			case WM_FORWARD:
				hr = (initContext->browserObject->m_pWB)->GoForward();
				break;
			case WM_STOP:
				hr = (initContext->browserObject->m_pWB)->Stop();
				break;
			case WM_RESIZE :
				hr = MoveWindow(initContext->browserHost, initContext->x, initContext->y, initContext->w, initContext->h, TRUE);
				break;
			case WM_BIGTEST:
				hr = ::MessageBox(initContext->browserHost, "command state changed", "youknow", MB_OK);
				break;
			
			}
			::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            }
    }
    initContext->canForward = initContext->browserObject->GetForwardState();
    initContext->canBack = initContext->browserObject->GetBackState();
    return 1;
}

HRESULT InitIEStuff (WebShellInitContext * initContext)
{

	HRESULT hr;
    HWND m_hWndClient;
    RECT rect;

	HWND localParent = initContext->parentHWnd;
	HWND localChild = initContext->browserHost;

   	HRESULT hRes = CoInitializeEx(NULL,  COINIT_APARTMENTTHREADED);
	ATLASSERT(SUCCEEDED(hRes));
  

/*if (_WIN32_IE >= 0x0300)
	INITCOMMONCONTROLSEX iccx;
	iccx.dwSize = sizeof(iccx);
	iccx.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES;
	BOOL bRet = ::InitCommonControlsEx(&iccx);
	bRet;
	ATLASSERT(bRet);
#else
*/
  ::InitCommonControls();
//#endif

    GetClientRect(initContext->parentHWnd, &rect);
   
    HINSTANCE newInst = GetModuleHandleA(NULL);
	hRes = _Module.Init(NULL, newInst);
	ATLASSERT(SUCCEEDED(hRes));

    
    AtlAxWinInit();

	m_hWndClient = initContext->browserObject->Create(
		initContext->parentHWnd, 
		rect,
		_T("about:blank"), 
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | 
			WS_VSCROLL | WS_HSCROLL, 
		WS_EX_CLIENTEDGE, 
	    ID_WEBBROWSER);

	hr = initContext->browserObject->QueryControl(&(initContext->browserObject->m_pWB));
	

	if FAILED(hr)
        {
           ATLTRACE(_T("Couldn't retrieve webbrowser"));
           return (-1);
        }

  if SUCCEEDED(hr)
        {
	     
	     ATLTRACE(_T("Browser succesfully retrieved"));
            
        }

    (initContext->browserHost) = m_hWndClient;

	if (!initContext->browserObject->spUnk) {    
	hr = initContext->browserObject->QueryControl(&(initContext->browserObject->spUnk));
	hr = initContext->browserObject->DispEventAdvise(initContext->browserObject->spUnk);
	}

	if FAILED(hr)
	{
		ATLTRACE(_T("Couldn't establish connection points"));
		return -1;
	}

    processEventLoop(initContext);
 

	return 0;

}

