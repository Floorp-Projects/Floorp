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

#ifndef __GenericDocument_H
#define __GenericDocument_H
// gendoc.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CGenericDoc document

class CNetscapeCntrItem;
class CGenericDoc : public COleServerDoc
{
	//	We need some friends that can create us.
	friend class CDCCX;
	friend MWContext *FE_MakeGridWindow(MWContext *pOldContext, void *hist_list, void *pHistory, int32 lX, int32 lY, int32 lWidth, int32 lHeight, char *pUrlStr, char *pWindowName, int8 iScrollType);

public:
	CGenericDoc();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CGenericDoc)

//	How to set the document template.
public:
	void SetDocTemplate(CDocTemplate *pTemplate)	{
		m_pDocTemplate = pTemplate;
	}

//	Context access, every one of these docs should have one.
protected:
	CDCCX *m_pContext;
public:
	void SetContext(CDCCX *pContext)	{
		m_pContext = pContext;
	}
	CDCCX *GetContext() const	{
		return(m_pContext);
	}
	void ClearContext()	{
		TRACE("Clearing doc context\n");
		SetContext(NULL);
	}

//  Embed handling
public:
    virtual void GetEmbedSize(MWContext *pContext, LO_EmbedStruct *pLayoutData, NET_ReloadMethod bReload);
    virtual void FreeEmbedElement(MWContext *pContext, LO_EmbedStruct *pLayoutData);

//	OLE server handling
public:
	CNetscapeSrvrItem *GetEmbeddedItem()	{
		return((CNetscapeSrvrItem *)COleServerDoc::GetEmbeddedItem());
	}

//	OLE server extension to allow document and server item
//		to minimally function without a context being
//		present.
protected:
	CString m_csEphemeralHistoryAddress;
public:
	virtual void CacheEphemeralData();

//	Helpers to resolve between ephemeral and actual data.
public:
	void GetContextHistoryAddress(CString &csRetval);


private:
void GetPluginRect(MWContext *pContext,
                   LO_EmbedStruct *pLayoutData,
                   CRect &rect);

//	Way to disable closing of the owning frame window.
private:
	BOOL m_bCanClose;
public:
	void EnableClose(BOOL bEnable = TRUE);
    BOOL CanClose();

//  Serialize flags which aid when and when not to serialize
//  when using the MFC framework for ole server stuff.
private:
    CString m_csOpenDocumentFile;
    BOOL m_bOpenDocumentFileSet;

//  Cache sizing to avoid wild resizes when an OLE server.
public:
    CSize m_csViewExtent;
    CSize m_csDocumentExtent;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGenericDoc)
protected:
	virtual BOOL OnOpenDocument(const char *pszPathName);
	virtual BOOL CanCloseFrame(CFrameWnd *pFrame);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	virtual COleServerItem *OnGetEmbeddedItem();
	virtual void OnCloseDocument();
	//}}AFX_VIRTUAL

	virtual void OnShowControlBars( CFrameWnd *pFrameWnd, BOOL bShow );

public:
	virtual void OnDeactivateUI( BOOL bUndoable );

// Implementation
public:
	virtual ~CGenericDoc();
	virtual void Serialize(CArchive& ar);   // overridden for document i/o

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
	virtual BOOL OnSaveDocument( LPCTSTR lpszPathName );
	BOOL DoSaveFile( CNetscapeCntrItem* pItem, LPCTSTR lpszPathName);


private:
BOOL CGenericDoc::PromptForFileName(CNetscapeCntrItem* pItem, CString& lpszPathName, CString &orgFileName, BOOL useDefaultDocName);

	// Generated message map functions
protected:
	//{{AFX_MSG(CGenericDoc)
	afx_msg void OnEditCopy();
	afx_msg void OnOleUpdate();
	afx_msg void OnUpdateOleUpdate(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif // __GenericDocument_H
