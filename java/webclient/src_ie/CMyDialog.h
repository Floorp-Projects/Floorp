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
 *                 Ron Capelli <capelli@us.ibm.com>
 */

#ifndef cmydialog_h
#define cmydialog_h

#include "ie_util.h"

#include <tlogstg.h>  // Travel Log  (History)

/*
 * lifetime: shared with WebShellInitContext
 */

class CMyDialog:

    public CAxWindow,
    public IDispEventImpl<1, CMyDialog, &DIID_DWebBrowserEvents2, &LIBID_SHDocVw, 1, 0>
{

public:

    CMyDialog(WebShellInitContext *yourInitContext);
    virtual ~CMyDialog();

    CComPtr<IUnknown>         spUnk;
    CComPtr<IWebBrowser2>     m_pWB;
    CComPtr<IServiceProvider> m_pISP;
    CComPtr<ITravelLogStg>    m_pTLStg;


    void JNICALL OnBeforeNavigate2(IDispatch* pDisp, CComVariant &URL,
                                                     CComVariant &Flags,
						     CComVariant &TargetFramename,
						     CComVariant &PostData,
						     CComVariant &Headers,
						     BOOL &Cancel);
    void JNICALL OnNavigateComplete2(IDispatch* pDisp, CComVariant &URL);
    void JNICALL OnNavigateError(IDispatch* pDisp, CComVariant &URL,
                                                   CComVariant &TargetFrameName,
                                                   CComVariant &StatusCode,
                                                   BOOL &Cancel);
    void JNICALL OnDownloadBegin();
    void JNICALL OnDownloadComplete();
    void JNICALL OnDocumentComplete(IDispatch* pDisp, CComVariant &URL);
    void JNICALL OnCommandStateChange(long lCommand, BOOL bEnable);
    void JNICALL OnProgressChange(long Progress, long ProgressMax);
    void JNICALL OnNewWindow2(IDispatch** &ppDisp, BOOL &Cancel);

    jboolean JNICALL GetForwardState() const;
    jboolean JNICALL GetBackState() const;

    void JNICALL AddDocumentLoadListener(jobject target);
    void JNICALL RemoveDocumentLoadListener(jobject target);
    void JNICALL RemoveAllListeners();


    BEGIN_SINK_MAP(CMyDialog)
	SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2,    OnBeforeNavigate2)
	SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2,  OnNavigateComplete2)
	SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_NAVIGATEERROR,      OnNavigateError)
//	SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_DOWNLOADBEGIN,      OnDownloadBegin)
//	SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_DOWNLOADCOMPLETE,   OnDownloadComplete)
	SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE,   OnDocumentComplete)
	SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_COMMANDSTATECHANGE, OnCommandStateChange)
	SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_PROGRESSCHANGE,     OnProgressChange)
	SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW2,         OnNewWindow2)
    END_SINK_MAP()

protected:

    WebShellInitContext *m_initContext; // not the prime-reference, don't delete

    jobject  m_docTarget;

    jint     docLoadState;              // bit-significant state mask
    #define  docLoadState_TopLevelDocumentComplete    1
    #define  docLoadState_CommandStateChange_Back     2
    #define  docLoadState_CommandStateChange_Forward  4
    #define  docLoadState_DocumentComplete            7

    jboolean forwardState;
    jboolean backState;

private:

    void CheckDocCompleteEvent();
};

#endif // cmydialog_h
