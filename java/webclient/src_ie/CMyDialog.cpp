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


#include "CMyDialog.h"

CMyDialog::CMyDialog(WebShellInitContext *yourInitContext) :
    m_initContext(yourInitContext),
    m_docTarget(NULL),
    docLoadState(0),
    forwardState(JNI_FALSE),
    backState(JNI_TRUE)
{
    // initialize the string constants (including properties keys)
    if (!util_StringConstantsAreInitialized()) {
        util_InitStringConstants();
    }
}

CMyDialog::~CMyDialog()
{
    m_initContext = NULL;
}

void  CMyDialog::CheckDocCompleteEvent()
{
    /*
     * IE sometimes fires the OnCommandStateChange events after OnDocumentComplete.
     * Ensure that all required OnCommandStateChange and OnDocumentComplete events
     * are obtained from IE before sending Webclient End Document Load event.
     */

    if (docLoadState == docLoadState_DocumentComplete) {
        if (m_docTarget) {
	    util_SendEventToJava(m_initContext->env,
                                 m_initContext->nativeEventThread, m_docTarget,
                                 DOCUMENT_LOAD_LISTENER_CLASSNAME,
                                 DocumentLoader_maskValues[END_DOCUMENT_LOAD_EVENT_MASK],
                                 NULL);
        }

	docLoadState = 0;
    }
}

void  JNICALL CMyDialog::OnCommandStateChange(long lCommand, BOOL bEnable)
{
    if (CSC_NAVIGATEFORWARD == lCommand)
    {
	// printf("CMyDialog::OnCommandStateChange NAVIGATEFORWARD: bEnable = %d\n", bEnable);

	if (bEnable == 0)
	    forwardState = JNI_FALSE;
	else
	    forwardState = JNI_TRUE;

	docLoadState |= docLoadState_CommandStateChange_Forward;
    }
    else if (CSC_NAVIGATEBACK == lCommand)
    {
	// printf("CMyDialog::OnCommandStateChange NAVIGATEBACK: bEnable = %d\n", bEnable);

	if (bEnable == 0)
	    backState = JNI_FALSE;
	else
	    backState = JNI_TRUE;

	docLoadState |= docLoadState_CommandStateChange_Back;
    }

    CheckDocCompleteEvent();
}

void JNICALL CMyDialog::OnDocumentComplete(IDispatch* pDisp, CComVariant &URL)
{
    // printf("CMyDialog::OnDocumentComplete pDisp = %08x, m_pWB = %08x\n", pDisp, m_pWB);

    if (pDisp == m_pWB) {
        docLoadState |= docLoadState_TopLevelDocumentComplete;

	CheckDocCompleteEvent();
    }
}

void JNICALL CMyDialog::OnNavigateComplete2(IDispatch* pDisp, CComVariant& URL)
{
    // printf("CMyDialog::OnNavigateComplete2 pDisp = %08x, m_pWB = %08x\n", pDisp, m_pWB);

    JNIEnv *env = (JNIEnv *)JNU_GetEnv(gVm, JNI_VERSION);

    jstring urlString = NULL;
    BSTR szURL = URL.bstrVal;

    if (szURL) {
	urlString = ::util_NewString(env, szURL, SysStringLen(szURL));
    }

    if (m_docTarget) {
        util_SendEventToJava(m_initContext->env,
                             m_initContext->nativeEventThread, m_docTarget,
                             DOCUMENT_LOAD_LISTENER_CLASSNAME,
                             DocumentLoader_maskValues[END_URL_LOAD_EVENT_MASK],
                             urlString);
    }

    if (urlString) {
        ::util_DeleteString(env, urlString);
    }
}

void JNICALL CMyDialog::OnBeforeNavigate2(IDispatch* pDisp, CComVariant &URL,
                                                            CComVariant &Flags,
				                            CComVariant &TargetFramename,
						            CComVariant &PostData,
						            CComVariant &Headers,
						            BOOL &Cancel)
{
    // printf("CMyDialog::OnBeforeNavigate2 pDisp = %08x, m_pWB = %08x\n", pDisp, m_pWB);

    JNIEnv *env = (JNIEnv *)JNU_GetEnv(gVm, JNI_VERSION);

    jstring urlString = NULL;
    BSTR szURL = URL.bstrVal;

    if (szURL) {
	urlString = ::util_NewString(env, szURL, SysStringLen(szURL));
    }

    if (pDisp == m_pWB) {

	docLoadState = 0;

	if (m_docTarget) {
	    util_SendEventToJava(m_initContext->env,
                                 m_initContext->nativeEventThread, m_docTarget,
                                 DOCUMENT_LOAD_LISTENER_CLASSNAME,
                                 DocumentLoader_maskValues[START_DOCUMENT_LOAD_EVENT_MASK],
                                 urlString);
	}
    }

    if (m_docTarget) {
        util_SendEventToJava(m_initContext->env,
                             m_initContext->nativeEventThread, m_docTarget,
                             DOCUMENT_LOAD_LISTENER_CLASSNAME,
                             DocumentLoader_maskValues[START_URL_LOAD_EVENT_MASK],
                             urlString);
    }

    if (urlString) {
        ::util_DeleteString(env, urlString);
    }

    Cancel = FALSE;
}

void JNICALL CMyDialog::OnNavigateError(IDispatch* pDisp, CComVariant &URL,
                                                          CComVariant &TargetFrameName,
                                                          CComVariant &StatusCode,
                                                          BOOL &Cancel)
{
    printf("CMyDialog::OnNavigateError pDisp = %08x\n", pDisp);
    Cancel = FALSE;
}

void JNICALL CMyDialog::OnDownloadBegin()
{
    printf("CMyDialog::OnDownloadBegin\n");
}

void JNICALL CMyDialog::OnDownloadComplete()
{
    printf("CMyDialog::OnDownloadComplete\n");
}

void JNICALL CMyDialog::OnProgressChange(long Progress, long ProgressMax)
{
    if ( (ProgressMax > 0)
      && (Progress > 0)
      && (Progress <= ProgressMax) )
    {
        char szPercent[8];

    	int percent = (Progress * 100)/ProgressMax;

	sprintf(szPercent, "%3d", percent);
	strcat(szPercent, "%");

//      printf("CMyDialog::OnProgressChange - %s - progress = %d, max = %d\n",
//                                            szPercent,      Progress, ProgressMax);

        JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

        jstring percentJString = ::util_NewStringUTF(env, szPercent);

        if (m_docTarget) {
            util_SendEventToJava(m_initContext->env,
                                 m_initContext->nativeEventThread, m_docTarget,
                                 DOCUMENT_LOAD_LISTENER_CLASSNAME,
                                 DocumentLoader_maskValues[PROGRESS_URL_LOAD_EVENT_MASK],
                                 percentJString);
        }

        if (percentJString) {
            ::util_DeleteString(env, percentJString);
        }
    }
//  else
//      printf("CMyDialog::OnProgressChange - **** - progress = %d, max = %d\n",
//	                                                        Progress, ProgressMax);
}

void JNICALL CMyDialog::OnNewWindow2(IDispatch** &ppDisp, BOOL &Cancel)
{
    printf("CMyDialog::OnNewWindow2 ppDisp = %08x\n", ppDisp);
    Cancel = FALSE;
}

jboolean JNICALL CMyDialog::GetForwardState() const
{
    return forwardState;
}

jboolean JNICALL CMyDialog::GetBackState() const
{
    return backState;
}

void JNICALL CMyDialog::AddDocumentLoadListener(jobject target)
{
    // printf("CMyDialog::AddDocumentLoadListener\n");
    // PENDING(glenn): do some kind of check to make sure our state is ok.

    if (-1 == DocumentLoader_maskValues[0]) {
        util_InitializeEventMaskValuesFromClass("org/mozilla/webclient/DocumentLoadEvent",
                                                DocumentLoader_maskNames,
                                                DocumentLoader_maskValues);
    }
    m_docTarget = target;
}

void JNICALL CMyDialog::RemoveDocumentLoadListener(jobject target)
{
    printf("CMyDialog::RemoveDocumentLoadListener\n");
}

void JNICALL CMyDialog::RemoveAllListeners()
{
    printf("CMyDialog::RemoveAllListeners\n");
}

