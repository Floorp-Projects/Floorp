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

#include <atlbase.h> //for CComPtr
#include <AtlApp.h> // for CAppModule decl

 CAppModule _Module;
 

#include <Atlwin.h> // for AtlWin
#include <Atlcom.h>

#include <Exdisp.h>  //for IWebBrowser2
#include <exdispid.h>

#include <atlhost.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atlctrlw.h>
#include <atlmisc.h>
#include <atlimpl.cpp>
#include <objbase.h>


#ifdef XP_PC
#include <windows.h>
#endif

#include <stdio.h>

#include "prlog.h" // for PR_ASSERT

#ifdef XP_UNIX
#include <unistd.h>
#include "gdksuperwin.h"
#include "gtkmozarea.h"
#endif

class CMyDialog:
	

	public CAxWindow,
	public IDispEventImpl<1, CMyDialog, &DIID_DWebBrowserEvents2,&LIBID_SHDocVw, 1, 0>
{


public:
   //ComPtr<IUnknown> spUnk;
	CComPtr<IWebBrowser2> m_pWB;
   //CAxWindow happyday;
	
  void __stdcall OnCommandStateChange(long lCommand, BOOL bEnable);
  void __stdcall CMyDialog::OnDownloadBegin();
  void __stdcall OnDownloadComplete();
  void __stdcall OnNavigateComplete2(IDispatch* pDisp, CComVariant& URL);

	BEGIN_SINK_MAP(CMyDialog)
		SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_COMMANDSTATECHANGE, OnCommandStateChange)
		SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_DOWNLOADBEGIN, OnDownloadBegin)
		SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_DOWNLOADCOMPLETE, OnDownloadComplete)
		SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2, OnNavigateComplete2)
	END_SINK_MAP()
};


//
// Local functions
//

//initializes IE stuff
HRESULT InitIEStuff (WebShellInitContext * arg);
int processEventLoop(WebShellInitContext *initContext);

//
// Local data
//

HWND localParent;  //these two are temporarily being used in order to test the
HWND localChild;   //OnCommandStateChange functions, they may be eventually removed


extern void util_ThrowExceptionToJava (JNIEnv * , const char * );

char * errorMessages[] = {
	"No Error",
	"Could not obtain the event queue service.",
	"Unable to create the WebShell instance.",
	"Unable to initialize the WebShell instance.",
	"Unable to show the WebShell."
};

/**

 * a null terminated array of listener interfaces we support.

 * PENDING(): this should probably live in EventRegistration.h

 * PENDING(edburns): need to abstract this out so we can use it from uno
 * and JNI.

 */

const char *gSupportedListenerInterfaces[] = {
    "org/mozilla/webclient/DocumentLoadListener",
    0
};

// these index into the gSupportedListenerInterfaces array, this should
// also live in EventRegistration.h

typedef enum {
    DOCUMENT_LOAD_LISTENER = 0,
    LISTENER_NOT_FOUND
} LISTENER_CLASSES;


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
(JNIEnv *env, jobject obj, jint webShellPtr, jobject typedListener)
{
    printf("debug: glenn: nativeAddListener\n");
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeCleanUp
(JNIEnv *env, jobject obj, jint webShellPtr)
{

  WebShellInitContext * initContext = (WebShellInitContext *) webShellPtr;
  	
  //AtlAdviseSinkMap(&browserHome, false)
  
	//_Module.RemoveMessageLoop();
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
				hr = (initContext->m_pWB)->Refresh();
				break;
			case WM_NAVIGATE:
 				hr = (initContext->m_pWB)->Navigate(CComBSTR(initContext->wcharURL), NULL, NULL, NULL, NULL);
                free((void *) initContext->wcharURL);
                initContext->wcharURL = nsnull;
				break;
			case WM_BACK:
				hr = (initContext->m_pWB)->GoBack();
				break;
			case WM_FORWARD:
				hr = (initContext->m_pWB)->GoForward();
				break;
			case WM_STOP:
				hr = (initContext->m_pWB)->Stop();
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

  

	CMyDialog browserHome;
	
	AtlAxWinInit();

	m_hWndClient = browserHome.Create(
		initContext->parentHWnd, 
		rect,
		_T("about:blank"), 
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | 
			WS_VSCROLL | WS_HSCROLL, 
		WS_EX_CLIENTEDGE, 
	    ID_WEBBROWSER);

	hr = browserHome.QueryControl(&browserHome.m_pWB);
	initContext->m_pWB = browserHome.m_pWB;


  (initContext->browserHost) = m_hWndClient;



	if FAILED(hr)
        {
           ATLTRACE(_T("Couldn't retrieve webbrowser"));
           return (-1);
        }

  if SUCCEEDED(hr)
        {
	     
	     ATLTRACE(_T("Browser succesfully retrieved"));
            
        }

//	CComPtr<IUnknown> spUnk;  //Unk Ptr will be used to sink the map
//    hr = browserHome.QueryControl(&spUnk);
  
	//hr = browserHome.DispEventAdvise(spUnk);

	if FAILED(hr)
	{
		ATLTRACE(_T("Couldn't establish connection points"));
		return -1;
	}

  processEventLoop(initContext);
 

	return 0;

}

void  __stdcall CMyDialog::OnCommandStateChange(long lCommand, BOOL bEnable)
{


//	HRESULT hr = ::PostMessage(localChild, WM_BIGTEST, 0, 0);
//	if (CSC_NAVIGATEFORWARD == lCommand)
//	{
//	SetForwarders(bEnable, localParent);
//	}
//	else if (CSC_NAVIGATEBACK == lCommand)
//	{
//	SetBackers(bEnable, localParent);
//	}

}

void __stdcall CMyDialog::OnDownloadBegin()
{
}

void __stdcall CMyDialog::OnDownloadComplete()
{
}

void __stdcall CMyDialog::OnNavigateComplete2(IDispatch* pDisp, CComVariant& URL)
{
}
