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

#ifndef TEMPLATE_H
#define TEMPLATE_H

class CGenericDocTemplate : public CDocTemplate
{
    DECLARE_DYNAMIC(CGenericDocTemplate)

public:
    CGenericDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
        CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass);

public:
	HMENU m_hMenuShared;
	HACCEL m_hAccelTable;

     ~CGenericDocTemplate();

public: 
	BOOL GetDocString(CString& rString, enum DocStringIndex index) const;
    void AddDocument(CDocument *pDoc);
    void RemoveDocument(CDocument *pDoc);
    POSITION GetFirstDocPosition() const;
    CDocument *GetNextDoc(POSITION& rPos) const;
	CDocument* OpenDocumentFile(const char* pszPathName,
								BOOL bMakeVisible = TRUE);
	void SetDefaultTitle(CDocument* pDocument);

protected:
	CPtrList m_docList;
	UINT m_nUntitledCount;
	BOOL m_bHideTitlebar;
	BOOL m_bDependent;
	BOOL m_bPopupWindow;
	HWND m_hPopupParent;
	BOOL m_bBorder;
};


//////////////////////////////////////////////////////////////////////////
// Browser template support
class CNetscapeDocTemplate : public CGenericDocTemplate
{
    DECLARE_DYNAMIC(CNetscapeDocTemplate)

// Constructors
public:
	CDocument* OpenDocumentFile(const char* pszPathName,
								BOOL bMakeVisible = TRUE,
								BOOL bHideTitlebar = FALSE,
								BOOL bDependent = FALSE,
								BOOL bPopupWindow = FALSE,
								HWND hPopupParent = NULL)
	{
		m_bHideTitlebar = bHideTitlebar;
		m_bDependent = bDependent;
		m_bPopupWindow = bPopupWindow;
		m_hPopupParent = hPopupParent;

		return CGenericDocTemplate::OpenDocumentFile(pszPathName, bMakeVisible);
	}

    CNetscapeDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
        CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass);

//	Overloaded
public:
	void InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc,
							BOOL bMakeVisible = TRUE);
};

//////////////////////////////////////////////////////////////////////////
// Edit template support
class CNetscapeEditTemplate : public CGenericDocTemplate
{
    DECLARE_DYNAMIC(CNetscapeEditTemplate)

// Constructors
public:
    CNetscapeEditTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
        CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass);

// Overloaded
public:
	void InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc,
							BOOL bMakeVisible = TRUE);
};

//////////////////////////////////////////////////////////////////////////
// Compose template support
class CNetscapeComposeTemplate : public CGenericDocTemplate
{
    DECLARE_DYNAMIC(CNetscapeComposeTemplate)

protected:
	int win_csid;

// Constructors
public:
    CNetscapeComposeTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
        CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass);

// Overloaded
public:
	CDocument* OpenDocumentFile(const char* pszPathName,
								int csid,
								BOOL bMakeVisible = TRUE)
	{
		if( csid == CS_DEFAULT || csid == CS_UNKNOWN)
			win_csid = INTL_DefaultWinCharSetID(0);
		else
			win_csid = csid;
		return CGenericDocTemplate::OpenDocumentFile(pszPathName, bMakeVisible);
	}
	void InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc,
		BOOL bMakeVisible = TRUE);
};

//////////////////////////////////////////////////////////////////////////
// Text Compose template support
class CNetscapeTextComposeTemplate : public CGenericDocTemplate
{
    DECLARE_DYNAMIC(CNetscapeTextComposeTemplate)

protected:
	int win_csid;

// Constructors
public:
    CNetscapeTextComposeTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
        CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass);

// Overloaded
public:
	CDocument* OpenDocumentFile(const char* pszPathName,
								int csid,
								BOOL bMakeVisible = TRUE)
	{
		win_csid = csid;
		return CGenericDocTemplate::OpenDocumentFile(pszPathName, bMakeVisible);
	}
	void InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc,
		BOOL bMakeVisible = TRUE);
};

//////////////////////////////////////////////////////////////////////////
// Addr template support
class CNetscapeAddrTemplate : public CGenericDocTemplate
{
    DECLARE_DYNAMIC(CNetscapeAddrTemplate)

protected:
	int win_csid;

// Constructors
public:
    CNetscapeAddrTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
        CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass);

// Overloaded
public:
	void InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc,
		BOOL bMakeVisible = TRUE);
};
#endif     
