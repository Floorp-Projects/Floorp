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


#include "CMyDialog.h"

CMyDialog::CMyDialog(WebShellInitContext *yourInitContext) : m_initContext(yourInitContext), m_docTarget(NULL), forwardState(JNI_FALSE), backState(JNI_TRUE)
{
    // initialize the string constants (including properties keys)
    if (!util_StringConstantsAreInitialized()) {
        util_InitStringConstants(m_initContext->env);
    }
}

CMyDialog::~CMyDialog() 
{
    m_initContext = NULL;
    
}

void  JNICALL CMyDialog::OnCommandStateChange(long lCommand, BOOL bEnable)
{

	if (CSC_NAVIGATEFORWARD == lCommand)
	{
		if (bEnable == 0)
			forwardState = JNI_FALSE;
		if (bEnable == 65535)
			forwardState = JNI_TRUE;
	}
		else if (CSC_NAVIGATEBACK == lCommand)
		{
			if (bEnable== 0)
				backState = JNI_FALSE;
			if (bEnable == 65535)
				backState = JNI_TRUE;
		}
}

void JNICALL CMyDialog::OnDownloadBegin()
{
}

void JNICALL CMyDialog::OnDownloadComplete()
{
}

void JNICALL CMyDialog::OnNavigateComplete2(IDispatch* pDisp, CComVariant& URL)
{
    if (m_docTarget) {
        util_SendEventToJava(m_initContext->env, 
                             m_initContext->nativeEventThread, m_docTarget, 
                             DOCUMENT_LOAD_LISTENER_CLASSNAME,
                             DocumentLoader_maskValues[END_DOCUMENT_LOAD_EVENT_MASK], 
                             NULL);
    }
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
}

void JNICALL CMyDialog::RemoveAllListeners()
{

}
