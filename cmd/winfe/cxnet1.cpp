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
// network.cpp : implementation file
//

#include "stdafx.h"

#include "cxnet1.h"

/////////////////////////////////////////////////////////////////////////////
// Context functions

XP_Bool CNetworkCX::Confirm(MWContext *pContext, const char *pConfirmMessage)	{
	//	Always return true, we don't want any dialogs coming up.
	//	Don't call the base.
	//	This will cause the prompt functions to be called multiple times as
	//		needed since both the netlib and this object cache different
	//		passwords.
	return(TRUE);
}

void CNetworkCX::Alert(MWContext *pContext, const char *pMessage)	{
	//	Hand over this in the error string, even if it's not an error.
	//	Can't have dialogs popping up everywhere.
	m_lFlags |= m_ERRS;
	m_csErrorMessage = pMessage;

	//	Do not call the base or a dialog comes up.
}

char *CNetworkCX::Prompt(MWContext *pContext, const char *pPrompt, const char *pDefault)	{
	//	So, LJM tells me that this function is used to query for a username.
	//	Do that here.
	//	Only if not already asked for during this load.
	if((m_lFlags & m_USER) == 0)	{
		SetUsernameRequested();
		return(AllocUsername());
	}
	return(NULL);
}

char *CNetworkCX::PromptPassword(MWContext *pContext, const char *pMessage)	{
	//	The caller should have proactively set the password before an Open().
	//	If not, then they will have to figure it out.
	//	Mark that a password was required for the transfer.
	//	Only if not already asked for during this load.
	if((m_lFlags & m_PASS) == 0)	{
		SetPasswordRequested();
		return(AllocPassword());
	}
	return(NULL);
}

XP_Bool CNetworkCX::PromptUsernameAndPassword(MWContext *pContext, const char *pMessage, char **ppUsername, char **ppPassword)	{
	//	Initialize.
	*ppPassword = NULL;
	*ppUsername = NULL;

	//	If both have already been asked for, don't continue.
	if((m_lFlags & m_USER) != 0 && (m_lFlags & m_PASS) != 0)	{
		return(FALSE);
	}

	//	Prompt for both username and password.
	//	Default values are given, and if not already specified by the caller, then we can just use them.
	//	Set that both a username and a password were required for the transfer.
	SetPasswordRequested();
	SetUsernameRequested();

	//	Copy and set any user/password values that the controller has preemptively set.
	char *pPass = AllocPassword();
	if(pPass != NULL)	{
		*ppPassword = pPass;
	}
	char *pUser = AllocUsername();
	if(pUser != NULL)	{
		*ppUsername = pUser;
	}
	return(TRUE);
}

XP_Bool CNetworkCX::ShowAllNewsArticles(MWContext *pContext)	{
	//	Set to show all the news articles.
	return(GetFlagShowAllNews());
}

XP_Bool CNetworkCX::UseFancyFTP(MWContext *pContext)	{
	//	Check to see if we are to use Fancy FTP.
	return(GetFlagFancyFTP());
}

XP_Bool CNetworkCX::UseFancyNewsgroupListing(MWContext *pContext)	{
	//	See if we should use full newsgroup listings with descriptions.
	return(GetFlagFancyNews());
}

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CNetworkCX
#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CNetworkCX, CCmdTarget)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

CNetworkCX::CNetworkCX()
{
	EnableAutomation();
	
	// To keep the application running as long as an OLE automation 
	//	object is active, the constructor calls AfxOleLockApp.
	
	AfxOleLockApp();

	//	Set the type of context that we are.
	m_cxType = Network;
	GetContext()->type = MWContextOleNetwork;

	//	Initialize our URL property, other members won't work unless this is defined.
	m_pUrlData = NULL;

	//	Our buffer is initially empty.
	ASSERT(m_cplBuffers.IsEmpty());

	//	We're done loading any stream, as none have started.
	m_bStreamComplete = TRUE;

	//	We don't want to initially show all news articles.
	m_bShowAllNews = FALSE;

	//	We do want to use Fancy News and Fancy FTP by default.
	m_bFancyNews = TRUE;
	m_bFancyFTP = TRUE;

	//	We've no current status flags.
	m_lFlags = m_OK;

	//	Username and password should be empty initially anyhow.

	TRACE("Netscape.Network.1 started\n");
}

CNetworkCX::~CNetworkCX()
{
	// To terminate the application when all objects created with
	// 	with OLE automation, the destructor calls AfxOleUnlockApp.
	
	AfxOleUnlockApp();

	TRACE("Netscape.Network.1 ended\n");
}

void CNetworkCX::OnFinalRelease()
{
	// When the last reference for an automation object is released
	//	OnFinalRelease is called.  This implementation deletes the 
	//	object.  Add additional cleanup required for your object before
	//	deleting it from memory.

	//	If we have a currently loading stream, we need to do cleanup there.
	if(m_pUrlData != NULL)	{
		Close();
	}

	DestroyContext();
}

char *CNetworkCX::AllocUsername()	{
	//	See if we even have a username to allocate.
	if(m_csUsername.IsEmpty() == TRUE)	{
		return(NULL);
	}
	return(XP_STRDUP(m_csUsername));
}

char *CNetworkCX::AllocPassword()	{
	//	See if we even have a password to allocate.
	if(m_csPassword.IsEmpty() == TRUE)	{
		return(NULL);
	}
	return(XP_STRDUP(m_csPassword));
}

BEGIN_MESSAGE_MAP(CNetworkCX, CCmdTarget)
	//{{AFX_MSG_MAP(CNetworkCX)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(CNetworkCX, CCmdTarget)
	//{{AFX_DISPATCH_MAP(CNetworkCX)
	DISP_PROPERTY_EX(CNetworkCX, "Username", GetUsername, SetUsername, VT_BSTR)
	DISP_PROPERTY_EX(CNetworkCX, "Password", GetPassword, SetPassword, VT_BSTR)
	DISP_PROPERTY_EX(CNetworkCX, "FlagShowAllNews", GetFlagShowAllNews, SetFlagShowAllNews, VT_BOOL)
	DISP_PROPERTY_EX(CNetworkCX, "FlagFancyFTP", GetFlagFancyFTP, SetFlagFancyFTP, VT_BOOL)
	DISP_PROPERTY_EX(CNetworkCX, "FlagFancyNews", GetFlagFancyNews, SetFlagFancyNews, VT_BOOL)
	DISP_FUNCTION(CNetworkCX, "Close", Close, VT_EMPTY, VTS_NONE)
	DISP_FUNCTION(CNetworkCX, "Read", Read, VT_I2, VTS_PBSTR VTS_I2)
	DISP_FUNCTION(CNetworkCX, "GetStatus", GetStatus, VT_I4, VTS_NONE)
	DISP_FUNCTION(CNetworkCX, "Open", Open, VT_BOOL, VTS_BSTR VTS_I2 VTS_BSTR VTS_I4 VTS_BSTR)
	DISP_FUNCTION(CNetworkCX, "GetErrorMessage", GetErrorMessage, VT_BSTR, VTS_NONE)
	DISP_FUNCTION(CNetworkCX, "GetServerStatus", GetServerStatus, VT_I2, VTS_NONE)
	DISP_FUNCTION(CNetworkCX, "GetContentLength", GetContentLength, VT_I4, VTS_NONE)
	DISP_FUNCTION(CNetworkCX, "GetContentType", GetContentType, VT_BSTR, VTS_NONE)
	DISP_FUNCTION(CNetworkCX, "GetContentEncoding", GetContentEncoding, VT_BSTR, VTS_NONE)
	DISP_FUNCTION(CNetworkCX, "GetExpires", GetExpires, VT_BSTR, VTS_NONE)
	DISP_FUNCTION(CNetworkCX, "GetLastModified", GetLastModified, VT_BSTR, VTS_NONE)
	DISP_FUNCTION(CNetworkCX, "Resolve", Resolve, VT_BSTR, VTS_BSTR VTS_BSTR)
	DISP_FUNCTION(CNetworkCX, "IsFinished", IsFinished, VT_BOOL, VTS_NONE)
	DISP_FUNCTION(CNetworkCX, "BytesReady", BytesReady, VT_I2, VTS_NONE)
	//}}AFX_DISPATCH_MAP
END_DISPATCH_MAP()

IMPLEMENT_OLECREATE(CNetworkCX, "Netscape.Network.1", 0xef5f7050, 0x385a, 0x11ce, 0x81, 0x93, 0x0, 0x20, 0xaf, 0x18, 0xf9, 0x5)

/////////////////////////////////////////////////////////////////////////////
// CNetworkCX message handlers

BOOL CNetworkCX::Open(LPCTSTR pURL, short iMethod, LPCTSTR pPostData, long lPostDataSize, LPCTSTR pPostHeaders) 
{
	//	Reset our status flags.
	m_lFlags = m_OK;
	m_csErrorMessage.Empty();

	//	See if we're busy.
	if(winfeInProcessNet == TRUE)	{
		m_lFlags |= m_BUSY;
		return(FALSE);
	}

	//	If we're handling a request already, shut it down.
	if(m_pUrlData != NULL)	{
		Close();
	}

	//	Create the URL we want to load.
	m_pUrlData = NET_CreateURLStruct(pURL, NET_DONT_RELOAD);

	//	Create the ncapi data for the URL.
	//	Don't let it free off the URL in the exit routine.
	//	This is done in close.
	//	A pointer to the class is saved in the URL struct, and will
	//		be freed off in FE_DeleteUrlData.
	CNcapiUrlData *pDontCare = new CNcapiUrlData(this, m_pUrlData);
	pDontCare->DontFreeUrl();

	//	Set the method for the load.
	//	GET 0
	//	POST 1
	//	HEAD 3
	m_pUrlData->method = iMethod;

	//	Contruct the post stuff.
	if(pPostData != NULL && strlen(pPostData) != 0 && m_pUrlData->method == 1)	{
		m_pUrlData->post_data = (char *)XP_ALLOC(lPostDataSize);
		memcpy(m_pUrlData->post_data, pPostData, CASTSIZE_T(lPostDataSize));

		m_pUrlData->post_data_size = lPostDataSize;

		if(pPostHeaders != NULL && strlen(pPostHeaders) != 0)	{
			m_pUrlData->post_headers = (char *)XP_ALLOC(strlen(pPostHeaders) + 1);
			memcpy(m_pUrlData->post_headers, pPostHeaders, strlen(pPostHeaders) + 1);
		}
		else	{
			m_pUrlData->post_headers = strdup("Content-type: application/x-www-form-urlencoded");

		}
		StrAllocCat(m_pUrlData->post_headers, CRLF);

		//	Manually add the content-length.
		//	This was done automatically in versions of Netscape prior to 2.0.
		char aBuffer[1024];
		sprintf(aBuffer, "Content-length: %ld", lPostDataSize);
		StrAllocCat(m_pUrlData->post_headers, aBuffer);
		StrAllocCat(m_pUrlData->post_headers, CRLF);
	}

	//	Finally, set that the stream is not yet completed.
	m_bStreamComplete = FALSE;

	//	Need to request a URL for a particular format out, in our special context.
	//	Any errors should be caught in the exit routine.
	GetUrl(m_pUrlData, FO_CACHE_AND_OLE_NETWORK);

	return TRUE;
}

short CNetworkCX::Read(BSTR FAR* pBuffer, short iAmount) 
{
	//	First, check for end of file condition.
	if(m_cplBuffers.IsEmpty() && m_bStreamComplete == TRUE)	{
		return(-1);
	}

	//	If the buffer's empty, we should just return right now.
	if(m_cplBuffers.IsEmpty())	{
		return(0);
	}

	//	We'll want to either copy over the amount of data they are asking for,
	//		or copy over how much data we have in the buffer,
	//		whichever is less.
	//	To do this, we need to figure out the amount of data in the buffers first
	//		entry only.

	//	Get the first entry out.
	CNetBuffer *pNetBuffer = (CNetBuffer *)m_cplBuffers.GetHead();

	//	See how much data we should be handling here.
	int iBuffer = pNetBuffer->m_iSize - pNetBuffer->m_iHead;
	ASSERT(iBuffer);

	iAmount = min(iBuffer, iAmount);
	if(iAmount <= 0)	{
		return(0);
	}

	//	Copy over that amount from our buffer.
#if !defined(_UNICODE) && !defined(OLE2ANSI) && defined(MSVC4)
    //  Need to encode unicode ourselves.
    MultiByteToWideChar(CP_ACP, 0, (pNetBuffer->m_pData + pNetBuffer->m_iHead), iAmount, *pBuffer, sizeof(OLECHAR) * iAmount);

#ifdef DEBUG
    //  Here's some extra goodies to make sure the conversion isn't lossy, as we can also transfer binary data.

    //  Can we reverse it correctly?
    char *pCompare = new char[iAmount];
    int iConvert = WideCharToMultiByte(CP_ACP, 0, *pBuffer, iAmount, pCompare, iAmount, NULL, NULL);
    ASSERT(iConvert);

    //  Was the conversion correct?
    int iCompare = memcmp(pCompare, (pNetBuffer->m_pData + pNetBuffer->m_iHead), iAmount);
    ASSERT(!iCompare);

    delete [] pCompare;
#endif

#else
	memcpy(*pBuffer, (pNetBuffer->m_pData + pNetBuffer->m_iHead), iAmount);
#endif
	pNetBuffer->m_iHead += iAmount;
	ASSERT(pNetBuffer->m_iHead <= pNetBuffer->m_iSize);

	//	See if we should get rid of the buffer entry if completely read now.
	if(pNetBuffer->m_iHead == pNetBuffer->m_iSize)	{
		//	Get rid of it.
		m_cplBuffers.RemoveHead();
		delete pNetBuffer;
	}

	//	Return the amount read.
	return(iAmount);
}

void CNetworkCX::Close() 
{
	//	Destroy the URL, if we have one.
	if(m_pUrlData != NULL)	{
		//	We really should check for reentrancy, but can't, as this event could happen beyond our control, such
		//		as the controlling application deleting their automation object.
		//	Only do this if we are actually loading, as netlib has probably unregistered this otherwise...
		if(m_bStreamComplete == FALSE)	{
			BOOL bOld = winfeInProcessNet;
			winfeInProcessNet = TRUE;
			NET_InterruptStream(m_pUrlData);
			winfeInProcessNet = bOld;
		}

		NET_FreeURLStruct(m_pUrlData);
		m_pUrlData = NULL;
	}

	//	Reset the end of the buffer, our stutus, et al.
	CNetBuffer *pDelMe;
	while(m_cplBuffers.IsEmpty() == FALSE)	{
		pDelMe = (CNetBuffer *)m_cplBuffers.RemoveHead();
		delete pDelMe;
	}

	m_lFlags = m_OK;
	m_csErrorMessage.Empty();
	m_bStreamComplete = TRUE;
}

BSTR CNetworkCX::GetUsername() 
{
	//	Return what the current username is.
	//	Up to caller to free the information.
	return m_csUsername.AllocSysString();
}

void CNetworkCX::SetUsername(LPCTSTR lpszNewValue) 
{
	//	Modify the current username to be something new.
	if(lpszNewValue == NULL)	{
		m_csUsername.Empty();
		return;
	}
	m_csUsername = lpszNewValue;
}

BSTR CNetworkCX::GetPassword() 
{
	//	Return the current password setting.
	//	Up to caller to free the information.
	return m_csPassword.AllocSysString();
}

void CNetworkCX::SetPassword(LPCTSTR lpszNewValue) 
{
	//	Set the password to something new.
	if(lpszNewValue == NULL)	{
		m_csPassword.Empty();
		return;
	}
	m_csPassword = lpszNewValue;
}

long CNetworkCX::GetStatus() 
{
	//	Give them the current status flags since the last open occurred.
	return(m_lFlags);
}

BOOL CNetworkCX::GetFlagShowAllNews() 
{
	return m_bShowAllNews;
}

void CNetworkCX::SetFlagShowAllNews(BOOL bNewValue) 
{
	m_bShowAllNews = bNewValue;
}

BOOL CNetworkCX::GetFlagFancyFTP() 
{
	return m_bFancyFTP;
}

void CNetworkCX::SetFlagFancyFTP(BOOL bNewValue) 
{
	m_bFancyFTP = bNewValue;
}

BOOL CNetworkCX::GetFlagFancyNews() 
{
	return m_bFancyNews;
}

void CNetworkCX::SetFlagFancyNews(BOOL bNewValue) 
{
	m_bFancyNews = bNewValue;
}

int CNetworkCX::StreamWrite(const char *pWriteData, int32 lLength)	{
	//	if we don't have a url, then don't do this.
	if(m_pUrlData == NULL)	{
		return(-1);
	}

	//	We should never get called to copy over more data than is reported by StreamReady.
	//	However, we don't care anymore.
	//	Allocate a new buffer in which to store the data.
	ASSERT(lLength <= NETBUFSIZE);
	CNetBuffer *pNewBuf = new CNetBuffer(CASTINT(lLength));
	memcpy(pNewBuf->m_pData, pWriteData, pNewBuf->m_iSize);

	//	Add it to the tail of our buffer list.
	m_cplBuffers.AddTail((void *)pNewBuf);

	return(MK_DATA_LOADED);
}

unsigned int CNetworkCX::StreamReady()	{
	//	If we don't have a URL, we won't take data.
	if(m_pUrlData == NULL)	{
		return(0);
	}

	//	If we already have n entries in our buffered data, don't do this.
	//	We can however take more if the netlib screws up due to this new
	//		buffer system.
	if(m_cplBuffers.IsEmpty() == FALSE && m_cplBuffers.GetCount() >= 10)	{
		return(0);
	}

	//	Return our max allowed size, don't really care if they fulfill this
	//		completely.
	return(NETBUFSIZE);
}

void CNetworkCX::StreamComplete()	{
	//	Function not utilized.
	//	Stream complete considered to be the URL exit routine.
}

void CNetworkCX::StreamAbort(int iStatus)	{
	//	Function not utilized.
	//	Stream complete considered to be the URL exit routine.
}

void CNetworkCX::GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext)	{
	//	URL is done loading.
	m_bStreamComplete = TRUE;

	//	Check for internal loading errors.
	if(iStatus != MK_DATA_LOADED)	{
		m_lFlags |= m_INTL;
	}
	else if(pUrl->server_status / 100 != 2 && pUrl->server_status / 100 != 3 && iStatus == MK_DATA_LOADED && pUrl->server_status != 0)	{
		m_lFlags |= m_SRVR;
	}

	//	If we have any error message what so ever, then we will save it here and set that an error occurred.
	if(m_lFlags & (m_SRVR | m_INTL))	{
		if(pUrl->error_msg != NULL)	{
			if(strlen(pUrl->error_msg) != 0)	{
				m_lFlags |= m_ERRS;
				m_csErrorMessage = pUrl->error_msg;
			}
		}
	}

	//	Call the base.
	CStubsCX::GetUrlExitRoutine(pUrl, iStatus, pContext);
}

BSTR CNetworkCX::GetErrorMessage() 
{
	//	Simply return any error message that we currently have.
	return m_csErrorMessage.AllocSysString();
}

extern "C"	{

NET_StreamClass *nfe_OleStream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext)	{
	//	Return a new stream class, pass the object along for the ride.
	return(NET_NewStream("Netscape_Network_1",
		nfe_StreamWrite,
		nfe_StreamComplete,
		nfe_StreamAbort,
		nfe_StreamReady,
		CX2VOID(pContext->fe.cx, CNetworkCX),
		pContext));
}

int nfe_StreamWrite(NET_StreamClass *stream, const char *pWriteData, int32 lLength)	{
	void *pDataObj=stream->data_object;
	CNetworkCX *pOle = VOID2CX(pDataObj, CNetworkCX);	

	//	Have our object handle it.
	return(pOle->StreamWrite(pWriteData, lLength));
}

void nfe_StreamComplete(NET_StreamClass *stream)	{
	void *pDataObj=stream->data_object;
	CNetworkCX *pOle = VOID2CX(pDataObj, CNetworkCX);	

	//	Have our object handle it.
	pOle->StreamComplete();
}

void nfe_StreamAbort(NET_StreamClass *stream, int iStatus)	{
	void *pDataObj=stream->data_object;
	CNetworkCX *pOle = VOID2CX(pDataObj, CNetworkCX);

	//	Have our object handle it.
	pOle->StreamAbort(iStatus);
}

unsigned int nfe_StreamReady(NET_StreamClass *stream)	{
	void *pDataObj=stream->data_object;
	CNetworkCX *pOle = VOID2CX(pDataObj, CNetworkCX);
	
	//	Have our object handle it.
	return(pOle->StreamReady());
}

};

short CNetworkCX::GetServerStatus() 
{
	//	Don't do this if we don't have a URL.
	if(m_pUrlData == NULL)	{
		return(-1);
	}
	else if(m_bStreamComplete != TRUE)	{
		//	Also can't do this unless the load is done.
		return(-1);
	}

	return(m_pUrlData->server_status);
}

long CNetworkCX::GetContentLength() 
{
	//	Don't do this if we don't have a URL.
	if(m_pUrlData == NULL)	{
		return(-1);
	}

	return(m_pUrlData->content_length);
}

BSTR CNetworkCX::GetContentType() 
{
	//	Don't do this if we don't have a URL.
	if(m_pUrlData == NULL)	{
		return(NULL);
	}
	else if(m_pUrlData->content_type == NULL)	{
		return(NULL);
	}
	else if(strlen(m_pUrlData->content_type) == 0)	{
		return(NULL);
	}

	CString s = m_pUrlData->content_type;
	return s.AllocSysString();
}

BSTR CNetworkCX::GetContentEncoding() 
{
	//	Don't do this if we don't have a URL.
	if(m_pUrlData == NULL)	{
		return(NULL);
	}
	else if(m_pUrlData->content_encoding == NULL)	{
		return(NULL);
	}
	else if(strlen(m_pUrlData->content_encoding) == 0)	{
		return(NULL);
	}

	CString s = m_pUrlData->content_encoding;
	return s.AllocSysString();
}

BSTR CNetworkCX::GetExpires() 
{
	//	Don't do this if we don't have a URL.
	if(m_pUrlData == NULL)	{
		return(NULL);
	}
	else if(m_pUrlData->expires == (time_t)0)	{
		return(NULL);
	}

	char *pTime = ctime(&(m_pUrlData->expires));
	if(pTime == NULL)	{
		return(NULL);
	}

	CString s = pTime;
	return s.AllocSysString();
}

BSTR CNetworkCX::GetLastModified() 
{
	//	Don't do this if we don't have a URL.
	if(m_pUrlData == NULL)	{
		return(NULL);
	}
	else if(m_pUrlData->last_modified == (time_t)0)	{
		return(NULL);
	}

	char *pTime = ctime(&(m_pUrlData->last_modified));
	if(pTime == NULL)	{
		return(NULL);
	}

	CString s = pTime;
	return s.AllocSysString();
}

BSTR CNetworkCX::Resolve(LPCTSTR pBase, LPCTSTR pRelative) 
{
	//	Have the netlib resolve the url stuff for us.
	char *cpURL = NET_MakeAbsoluteURL((char *)pBase, (char *)pRelative);
	if(cpURL == NULL)	{
		return(NULL);
	}

	CString s = cpURL;
	XP_FREE(cpURL);
	return s.AllocSysString();
}

BOOL CNetworkCX::IsFinished() 
{
	//	Check for end of file condition.
	if(m_cplBuffers.IsEmpty() && m_bStreamComplete == TRUE)	{
		return(TRUE);
	}

	return(FALSE);
}

short CNetworkCX::BytesReady() 
{
	//	Check to see if there's any load ready.
	if(m_cplBuffers.IsEmpty() && m_bStreamComplete == TRUE)	{
		return(0);
	}

	//	Return the number of bytes we are currently ready to dish out.
	CNetBuffer *pReady = (CNetBuffer *)m_cplBuffers.GetHead();
	int iBuffer = pReady->m_iSize - pReady->m_iHead;
	ASSERT(iBuffer);
	return(iBuffer);
}
