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

#include "cxstubs.h"

/////////////////////////////////////////////////////////////////////////////
// CNetworkCX command target

class CNetworkCX : public CCmdTarget, public CStubsCX
{
	DECLARE_DYNCREATE(CNetworkCX)
protected:
	CNetworkCX();           // protected constructor used by dynamic creation

// Attributes
private:
	URL_Struct *m_pUrlData;	//	The url to load.
	BOOL m_bStreamComplete;	//	Wether or not the load is completed.

	BOOL m_bShowAllNews;	//	Wether or not to show all news articles.
	BOOL m_bFancyNews;		//	Wether or not to use Fancy News.
	BOOL m_bFancyFTP;		//	Wether or not to use Fancy FTP.
	CString m_csUsername;	//	The registered user name.
	CString m_csPassword;	//	The registered password.

	enum	{
		m_OK = 0x0000,		//	Data is loaded ok
		m_USER = 0x0001,	//	User name requested, may have still loaded if supplied
		m_PASS = 0x0002,	//	User password requested, may have still loaded if supplied
		m_BUSY = 0x0100,	//	Busy, try back later
		m_SRVR = 0x0200,	//	Server reported an irregular status, probably an error.
		m_INTL = 0x0400,	//	Internal loading error, never got to server.
		m_ERRS = 0x0800		//	A helpful error string is available provided by Netscape.
	};
	long m_lFlags;	//	Some status flags, to mark what happened, to help caller figure out what went wrong.
	CString m_csErrorMessage;

//	Read buffers to store data as it comes in.
private:
	CPtrList m_cplBuffers;

public:
	char *AllocUsername();
	char *AllocPassword();
	void SetPasswordRequested()	{
		m_lFlags |= m_PASS;
	}
	void SetUsernameRequested()	{
		m_lFlags |= m_USER;
	}

// Operations
public:
	int StreamWrite(const char *pWriteData, int32 lLength);
	void StreamComplete();
	void StreamAbort(int iStatus);
	unsigned int StreamReady();

//	Context overrides
public:
	virtual void Alert(MWContext *pContext, const char *pMessage);
	virtual XP_Bool Confirm(MWContext *pContext, const char *pConfirmMessage);
	virtual char *Prompt(MWContext *pContext, const char *pPrompt, const char *pDefault);
	virtual char *PromptPassword(MWContext *pContext, const char *pMessage);
	virtual XP_Bool PromptUsernameAndPassword(MWContext *pContext, const char *pMessage, char **ppUsername, char **ppPassword);
	virtual XP_Bool ShowAllNewsArticles(MWContext *pContext);
	virtual XP_Bool UseFancyFTP(MWContext *pContext);
	virtual XP_Bool UseFancyNewsgroupListing(MWContext *pContext);

	virtual void GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNetworkCX)
	public:
	virtual void OnFinalRelease();
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CNetworkCX();

	// Generated message map functions
	//{{AFX_MSG(CNetworkCX)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
	DECLARE_OLECREATE(CNetworkCX)

	// Generated OLE dispatch map functions
	//{{AFX_DISPATCH(CNetworkCX)
	afx_msg BSTR GetUsername();
	afx_msg void SetUsername(LPCTSTR lpszNewValue);
	afx_msg BSTR GetPassword();
	afx_msg void SetPassword(LPCTSTR lpszNewValue);
	afx_msg BOOL GetFlagShowAllNews();
	afx_msg void SetFlagShowAllNews(BOOL bNewValue);
	afx_msg BOOL GetFlagFancyFTP();
	afx_msg void SetFlagFancyFTP(BOOL bNewValue);
	afx_msg BOOL GetFlagFancyNews();
	afx_msg void SetFlagFancyNews(BOOL bNewValue);
	afx_msg void Close();
	afx_msg short Read(BSTR FAR* pBuffer, short iAmount);
	afx_msg long GetStatus();
	afx_msg BOOL Open(LPCTSTR pURL, short iMethod, LPCTSTR pPostData, long lPostDataSize, LPCTSTR pPostHeaders);
	afx_msg BSTR GetErrorMessage();
	afx_msg short GetServerStatus();
	afx_msg long GetContentLength();
	afx_msg BSTR GetContentType();
	afx_msg BSTR GetContentEncoding();
	afx_msg BSTR GetExpires();
	afx_msg BSTR GetLastModified();
	afx_msg BSTR Resolve(LPCTSTR pBase, LPCTSTR pRelative);
	afx_msg BOOL IsFinished();
	afx_msg short BytesReady();
	//}}AFX_DISPATCH
	DECLARE_DISPATCH_MAP()
};

/////////////////////////////////////////////////////////////////////////////
//	Miscellaneous Functions
extern "C"	{
NET_StreamClass *nfe_OleStream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext);
int nfe_StreamWrite(NET_StreamClass *stream, const char *pWriteData, int32 lLength);
void nfe_StreamComplete(NET_StreamClass *stream);
void nfe_StreamAbort(NET_StreamClass *stream, int iStatus);
unsigned int nfe_StreamReady(NET_StreamClass *stream);
};


//	Max amount of data to store in each CNetBuffer.
#define NETBUFSIZE (30 * 1024)

//	Structure to store read data.
struct CNetBuffer	{
	int m_iHead;
	int m_iSize;
	char *m_pData;

	CNetBuffer(int iSize)	{
		ASSERT(iSize);
		m_iHead = 0;
		m_iSize = iSize;
		m_pData = new char[iSize];
	}
	~CNetBuffer()	{
		if(m_pData)	{
			delete m_pData;
		}
	}
};

/////////////////////////////////////////////////////////////////////////////
