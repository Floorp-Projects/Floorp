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


/**

 * Util methods

 */

#ifndef ie_util_h
#define ie_util_h

#include "jni_util.h" // located in ../src_share, 
                      // pulls in ../src_share/jni_util_export.h
#include <Exdisp.h>
#include <atlbase.h> //for CComPtr
#include <AtlApp.h> // for CAppModule decl WTL


extern CAppModule _Module;
#include <Atlwin.h> // for AtlWin 
#include <Atlcom.h>

//#include <atlhost.h>

#include <Exdisp.h>  //for IWebBrowser2
#include <exdispid.h>

#include <atlhost.h>
//#include <atlframe.h>//WTL
#include <atlctrls.h>//WTL
//#include <atlctrlw.h>//WTL
//#include <atlmisc.h> //WTL
//#include <atlimpl.cpp>
#include <objbase.h>

class CMyDialog;

extern const char *gSupportedListenerInterfaces[]; // defined in ie_util.cpp

struct WebShellInitContext {

    HWND		parentHWnd;
    HWND                browserHost;
    JNIEnv          *   env;
    jobject             nativeEventThread;
    const wchar_t *			wcharURL;
	int					initComplete;
	int					initFailCode;
	int					x;
	int					y;
	int					w;
	int					h;
	jboolean	canForward;
	jboolean	canBack;
    CMyDialog   *browserObject;

};

#endif // ie_util_h
