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
// cxsave.h : header file
//

#include "cxstubs.h"
#include "widgetry.h"

/////////////////////////////////////////////////////////////////////////////
// CSaveCX dialog

class CSaveCX : public CDialog, public CStubsCX
{
// Construction
private:
	char* GetMailNewsTempFileName(char* pTempPath, char *pFileName = NULL);
	BOOL CanCreate(URL_Struct * pUrl = NULL); //	Can we proceed with creation?  Used by SaveObject.
	BOOL Creator();
public:
    CSaveCX(const char *pAnchor, const char *pViewer = NULL, CWnd *pParent = NULL); //  For external viewing or saving
    ~CSaveCX();

//	Indirect Construction
private:
	void DoCreate()	{
		//	Set dimensions of progress
		m_crBounds = CRect(5, 50, 5 + 175, 50 + 15);
		MapDialogRect(m_crBounds);
		m_crBounds.InflateRect(-1, -1);	//	Shrink for border.
	}
  // Helper function to figure out correct file extension
#ifdef MOZ_MAIL_NEWS
	void AddFileExtension(char *& pFileName);
#endif
public:
	static BOOL SaveAnchorObject(const char *pAnchor, History_entry *pHist, int16 iCSID = 0, CWnd *pParent = NULL, char * pFileName = NULL);
    static BOOL SaveAnchorAsText(const char *pAnchor, History_entry *pHist,  CWnd *pParent, char *pFileName);
	static NET_StreamClass *SaveUrlObject(URL_Struct *pUrl, CWnd *pParent = NULL, char * pFileName = NULL);
	static NET_StreamClass *ViewUrlObject(URL_Struct *pUrl, const char *pViewer, CWnd *pParent = NULL);
	static NET_StreamClass *OleStreamObject(NET_StreamClass *pOleStream, URL_Struct *pUrl, const char *pViewer, CWnd *pParent = NULL);
	static BOOL SaveToGlobal(HGLOBAL *phGlobal, LPCSTR lpszUrl, LPCSTR lpszTitle); // Returns context id
#ifdef MOZ_MAIL_NEWS
    static BOOL SaveToFile(CFile *pFile, LPCSTR lpszUrl, LPCSTR lpszTitle);
#endif

//  What we're saving
private:
    CProgressMeter m_ProgressMeter;
    History_entry * m_pHist;

    CString m_csAnchor; //  The anchor to load
    CString m_csViewer; //  The viewer to spawn, if specified, file will be removed on exit and
                        //      the user is not prompted for a file name in which to save.

    int m_iFileType;    //  The format of the file (needed for text front end when saving basically....)
    CString m_csFileName;   //  The file to save it in

    CWnd *m_pParent;    //  Our parent (need for possible file dialogs).
	NET_StreamClass *m_pSecondaryStream;	//	A secondary stream, sometimes we chain together streams.
    long tFirstTime;
    long tLastTime;
    long tLastBarTime;
    int iLastPercent;

//  Wether or not we were interrupted (cleanup even more).
public:
    URL_Struct *m_pUrl;   //  The URL_Struct as known by the netlib
    BOOL m_bInterrupted;
    BOOL m_bAborted;
	BOOL m_bSavingToGlobal;
	int16  m_iCSID;		//WinCX pass default character set information here	

//	A way to manually set the URL we're handling without having
//		the dialog create one.
//	Also a way to manually set the secondary stream for the URL that
//		we're handling.
public:
	void SetUrl(URL_Struct *pUrl)	{
		ASSERT(m_pUrl == NULL);
		m_pUrl = pUrl;
	}
	void SetSecondaryStream(NET_StreamClass *pSecondaryStream)	{
		ASSERT(m_pSecondaryStream == NULL);
		ASSERT(m_pSink == NULL);	//	Secondary streams require no sink.
		ASSERT(!m_csViewer.IsEmpty() || m_bSavingToGlobal);
		pSecondaryStream->window_id = GetContext();
		m_pSecondaryStream = pSecondaryStream;
	}
	NET_StreamClass *GetSecondaryStream() const	{
		return(m_pSecondaryStream);
	}
	void ClearSecondary() {
		m_pSecondaryStream = NULL;
	}

//  Informational
public:
    BOOL IsSaving() const	{
        return(m_csViewer.IsEmpty());
    }
    BOOL IsViewing() const	{
        return(IsSaving() == FALSE ? TRUE : FALSE);
    }
    BOOL IsShellExecute() const {
        return(m_csViewer == "ShellExecute");
    }
	BOOL IsSavingToGlobal() const {
		return m_bSavingToGlobal;
	}
    CString GetFileName() const	{
        return(m_csFileName);
    }
	CString GetAnchor() const	{
		return(m_csAnchor);
	}
	CString GetViewer()	const;

//	For dialogs
public:
	virtual CWnd *GetDialogOwner() const;

//  Serialization information
private:
    CStdioFile *m_pSink;
public:
    void SetSink(CStdioFile *pSink) {
		ASSERT(m_pSecondaryStream == NULL);	//	Sinks require no secondary stream.
        m_pSink = pSink;
    }
    CStdioFile *GetSink() const	{
        return(m_pSink);
    }
    void ClearSink()    {
        m_pSink = NULL;
    }

//	Progress Information, manipulated by GraphProgress
private:
	void Progress(int32 lPercent);	//	Draw progress bar
	int32 m_lOldPercent;		//	Old percentage to optimize drawing in OnPaint.
	CRect m_crBounds;	//	Bounds of dialog progress bar.

//	Context Overrides
public:
    //  The URL exit routine.
    virtual void GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext);
    //  The text translation exit routine.
    virtual void TextTranslationExitRoutine(PrintSetup *pTextFE);
	//	All Connections are done.
	virtual void AllConnectionsComplete(MWContext *pContext);
	//	Textual progress information.
	virtual void Progress(MWContext *pContext, const char *pMessage);
	//	Progress bar function.
	virtual void SetProgressBarPercent(MWContext *pContext, int32 lPercent);
	//	Graph progression.
	virtual void GraphProgress(MWContext *pContext, URL_Struct *pURL, int32 lBytesReceived, int32 lBytesSinceLastTime, int32 lContentLength);

// Dialog Data
	//{{AFX_DATA(CSaveCX)
	enum { IDD = IDD_CONTEXT_SAVE };
	CString	m_csAction;
	CString	m_csDestination;
	CString	m_csLocation;
	CString m_csProgress;
	CString	m_csTimeLeft;
	CString	m_csPercentComplete;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSaveCX)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSaveCX)
	virtual void OnCancel();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

extern "C"  {
//  Netlib API to the stream that will save the file to disk.
NET_StreamClass *ContextSaveStream(int iFormatOut, void *pDataObj, URL_Struct *pUrl, MWContext *pContext);

unsigned int ContextSaveReady(NET_StreamClass *stream);
int ContextSaveWrite(NET_StreamClass *stream, const char *pWriteData, int32 iDataLength);
void ContextSaveComplete(NET_StreamClass *stream);
void ContextSaveAbort(NET_StreamClass *stream, int iStatus);
};
