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
#ifndef _MSGTMPL_H
#define _MSGTMPL_H

#include "template.h"

class CMsgTemplate: public CGenericDocTemplate {
protected:
	void _InitialUpdate(CFrameWnd *pFrame, CFrameWnd *pPrevFrame, BOOL bMakeVisible, 
						LPSTR lpszPosPref, LPSTR lpszShowPref); 
public:
    CMsgTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
				 CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass):
		CGenericDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass) {}
};

class CFolderTemplate: public CMsgTemplate {
protected:
	virtual void InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc,
									BOOL bMakeVisible = TRUE);
public:
    CFolderTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
					CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass):
		CMsgTemplate(nIDResource, pDocClass, pFrameClass, pViewClass) {}
};

class CThreadTemplate: public CMsgTemplate {
protected:
	virtual void InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc,
									BOOL bMakeVisible = TRUE);
public:
    CThreadTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
					CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass):
		CMsgTemplate(nIDResource, pDocClass, pFrameClass, pViewClass) {}
};

class CMessageTemplate: public CMsgTemplate {
protected:
	virtual void InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc,
									BOOL bMakeVisible = TRUE);
public:
    CMessageTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
					 CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass):
		CMsgTemplate(nIDResource, pDocClass, pFrameClass, pViewClass) {}
};
#endif
