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

#ifndef cmydialog_h
#define cmydialog_h

#include "ie_util.h"

/*

 * lifetime: shared with WebShellInitContext

 */ 

class CMyDialog:

	public CAxWindow,
	public IDispEventImpl<1, CMyDialog, &DIID_DWebBrowserEvents2,&LIBID_SHDocVw, 1, 0>
{


public:
    CMyDialog(WebShellInitContext *yourInitContext);
    virtual ~CMyDialog();

    CComPtr<IUnknown> spUnk;
	CComPtr<IWebBrowser2> m_pWB;

  void JNICALL OnCommandStateChange(long lCommand, BOOL bEnable);
  void JNICALL OnDownloadBegin();
  void JNICALL OnDownloadComplete();
  void JNICALL OnNavigateComplete2(IDispatch* pDisp, CComVariant& URL);

    jboolean JNICALL GetForwardState() const;
    jboolean JNICALL GetBackState() const;

    void JNICALL AddDocumentLoadListener(jobject target);
    void JNICALL RemoveDocumentLoadListener(jobject target);
    void JNICALL RemoveAllListeners();


	BEGIN_SINK_MAP(CMyDialog)
		SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_COMMANDSTATECHANGE, OnCommandStateChange)
		SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_DOWNLOADBEGIN, OnDownloadBegin)
		SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_DOWNLOADCOMPLETE, OnDownloadComplete)
		SINK_ENTRY_EX(1, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2, OnNavigateComplete2)
	END_SINK_MAP()

protected:
    WebShellInitContext *m_initContext; // not the prime-reference, don't delete
    jobject m_docTarget;

    jboolean forwardState;
    jboolean backState;
};

#endif // cmydialog_h
