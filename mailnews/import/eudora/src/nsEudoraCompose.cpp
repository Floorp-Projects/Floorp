/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


#include "nscore.h"
#include "prthread.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIURI.h"

#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"

#include "nsIMsgCompose.h"
#include "nsIMsgCompFields.h"
#include "nsIMsgSend.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgAccountManager.h"

#include "nsEudoraCompose.h"

#include "EudoraDebugLog.h"

#include "nsMimeTypes.h"
#include "nsEudoraMailbox.h"

static NS_DEFINE_CID( kMsgSendCID, NS_MSGSEND_CID);
static NS_DEFINE_CID( kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 
static NS_DEFINE_CID( kMsgMailSessionCID,	NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID( kIOServiceCID, NS_IOSERVICE_CID);


// We need to do some calculations to set these numbers to something reasonable!
// Unless of course, CreateAndSendMessage will NEVER EVER leave us in the lurch
#define kHungCount		100000
#define kHungAbortCount	1000


#ifdef IMPORT_DEBUG
char *p_test_headers = 
"Received: from netppl.fi (IDENT:monitor@get.freebsd.because.microsoftsucks.net [209.3.31.115])\n\
	by mail4.sirius.com (8.9.1/8.9.1) with SMTP id PAA27232;\n\
	Mon, 17 May 1999 15:27:43 -0700 (PDT)\n\
Message-ID: <ikGD3jRTsKklU.Ggm2HmE2A1Jsqd0p@netppl.fi>\n\
From: \"adsales@qualityservice.com\" <adsales@qualityservice.com>\n\
Subject: Re: Your College Diploma (36822)\n\
Date: Mon, 17 May 1999 15:09:29 -0400 (EDT)\n\
MIME-Version: 1.0\n\
Content-Type: TEXT/PLAIN; charset=\"US-ASCII\"\n\
Content-Transfer-Encoding: 7bit\n\
X-UIDL: 19990517.152941\n\
Status: RO";

char *p_test_body = 
"Hello world?\n\
";
#else
#define	p_test_headers	nsnull
#define	p_test_body nsnull
#endif


#define kWhitespace	"\b\t\r\n "


// First off, a listener
class SendListener : public nsIMsgSendListener
{
public:
	SendListener() {
		NS_INIT_REFCNT(); 
		m_done = PR_FALSE;
		m_location = nsnull;
	}

	virtual ~SendListener() { NS_IF_RELEASE( m_location); }

	// nsISupports interface
	NS_DECL_ISUPPORTS

	/* void OnStartSending (in string aMsgID, in PRUint32 aMsgSize); */
	NS_IMETHOD OnStartSending(const char *aMsgID, PRUint32 aMsgSize) {return NS_OK;}

	/* void OnProgress (in string aMsgID, in PRUint32 aProgress, in PRUint32 aProgressMax); */
	NS_IMETHOD OnProgress(const char *aMsgID, PRUint32 aProgress, PRUint32 aProgressMax) {return NS_OK;}

	/* void OnStatus (in string aMsgID, in wstring aMsg); */
	NS_IMETHOD OnStatus(const char *aMsgID, const PRUnichar *aMsg) {return NS_OK;}

	/* void OnStopSending (in string aMsgID, in nsresult aStatus, in wstring aMsg, in nsIFileSpec returnFileSpec); */
	NS_IMETHOD OnStopSending(const char *aMsgID, nsresult aStatus, const PRUnichar *aMsg, 
						   nsIFileSpec *returnFileSpec) {
		m_done = PR_TRUE;
		m_location = returnFileSpec;
		NS_IF_ADDREF( m_location);
		return NS_OK;
	}

	static nsresult CreateSendListener( nsIMsgSendListener **ppListener);

	void Reset() { m_done = PR_FALSE; NS_IF_RELEASE( m_location); m_location = nsnull;}

public:
	PRBool			m_done;
	nsIFileSpec *	m_location;
};


NS_IMPL_ISUPPORTS( SendListener, nsCOMTypeInfo<nsIMsgSendListener>::GetIID())

nsresult SendListener::CreateSendListener( nsIMsgSendListener **ppListener)
{
    NS_PRECONDITION(ppListener != nsnull, "null ptr");
    if (! ppListener)
        return NS_ERROR_NULL_POINTER;

    *ppListener = new SendListener();
    if (! *ppListener)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*ppListener);
    return NS_OK;
}


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////



nsEudoraCompose::nsEudoraCompose()
{
	m_pIOService = nsnull;
	m_pAttachments = nsnull;
	m_pListener = nsnull;
	m_pMsgSend = nsnull;
	m_pMsgFields = nsnull;
	m_pIdentity = nsnull;
	m_pHeaders = p_test_headers;
	if (m_pHeaders)
		m_headerLen = nsCRT::strlen( m_pHeaders);
	else
		m_headerLen = 0;
	m_pBody = p_test_body;
	if (m_pBody)
		m_bodyLen = nsCRT::strlen( m_pBody);
	else
		m_bodyLen = 0;
}

nsEudoraCompose::~nsEudoraCompose()
{
	NS_IF_RELEASE( m_pIOService);
	NS_IF_RELEASE( m_pMsgSend);
	NS_IF_RELEASE( m_pListener);
	NS_IF_RELEASE( m_pMsgFields);
	NS_IF_RELEASE( m_pIdentity);
}

nsresult nsEudoraCompose::CreateIdentity( void)
{
	if (m_pIdentity)
		return( NS_OK);

	nsresult	rv;
    NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) return( rv);
	nsCOMPtr<nsIMsgAccountManager> accMgr;
	rv = mailSession->GetAccountManager( getter_AddRefs( accMgr));
	if (NS_FAILED( rv)) return( rv);
	rv = accMgr->CreateIdentity( &m_pIdentity);
	if (m_pIdentity) {
		m_pIdentity->SetFullName( "Import Identity");
		m_pIdentity->SetIdentityName( "Import Identity");
		m_pIdentity->SetEmail( "import@import.service");
	}
	
	return( rv);
}

nsresult nsEudoraCompose::CreateComponents( void)
{
	nsresult	rv = NS_OK;
	
	if (!m_pIOService) {
		IMPORT_LOG0( "Creating nsIOService\n");

		NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &rv);
		if (NS_FAILED(rv)) 
			return( rv);
		m_pIOService = service;
		NS_IF_ADDREF( m_pIOService);
	}
	
	NS_IF_RELEASE( m_pMsgFields);
	if (!m_pMsgSend)
		rv = nsComponentManager::CreateInstance( kMsgSendCID, nsnull, nsCOMTypeInfo<nsIMsgSend>::GetIID(), (void **) &m_pMsgSend); 
	if (!m_pListener && NS_SUCCEEDED( rv)) {
		rv = SendListener::CreateSendListener( &m_pListener);
		if (NS_SUCCEEDED( rv)) {
			rv = m_pMsgSend->AddListener( m_pListener);
			if (NS_FAILED( rv)) {
				NS_IF_RELEASE( m_pListener);
				m_pListener = nsnull;
			}
		}
	}

	if (NS_SUCCEEDED(rv) && m_pMsgSend) { 
	    rv = nsComponentManager::CreateInstance( kMsgCompFieldsCID, nsnull, nsCOMTypeInfo<nsIMsgCompFields>::GetIID(), (void **) &m_pMsgFields); 
		if (NS_SUCCEEDED(rv) && m_pMsgFields)
			return( NS_OK);
	}

	return( NS_ERROR_FAILURE);
}

void nsEudoraCompose::GetHeaderValue( const char *pHeader, nsString& val)
{
	val.Truncate();
	if (!m_pHeaders)
		return;

	PRInt32	start = 0;
	PRInt32 len = nsCRT::strlen( pHeader);
	const char *pChar = m_pHeaders;
	if (!nsCRT::strncasecmp( pHeader, m_pHeaders, len)) {
		start = len;
	}
	else {
		while (start < m_headerLen) {
			while ((start < m_headerLen) && (*pChar != 0x0D) && (*pChar != 0x0A)) {
				start++;
				pChar++;
			}
			while ((start < m_headerLen) && ((*pChar == 0x0D) || (*pChar == 0x0A))) {
				start++;
				pChar++;
			}
			if ((start < m_headerLen) && !nsCRT::strncasecmp( pChar, pHeader, len))
				break;
		}
		if (start < m_headerLen)
			start += len;
	}

	if (start >= m_headerLen)
		return;

	PRInt32		end = start;
	pChar = m_pHeaders + start;
	while (end < m_headerLen) {
		while ((end < m_headerLen) && (*pChar != 0x0D) && (*pChar != 0x0A)) {
			end++;
			pChar++;
		}
		if (end > start) {
			val.Append( m_pHeaders + start, end - start);
		}

		while ((end < m_headerLen) && ((*pChar == 0x0D) || (*pChar == 0x0A))) {
			end++;
			pChar++;
		}
		
		start = end;

		while ((end < m_headerLen) && ((*pChar == ' ') || (*pChar == '\t'))) {
			end++;
			pChar++;
		}

		if (start == end)
			break;
		
		start = end;
		val.Append( ' ');
	}
	
	val.Trim( kWhitespace);
}

void nsEudoraCompose::ExtractCharset( nsString& str)
{
	nsString	tStr;
	PRInt32 idx = str.Find( "charset=", PR_TRUE);
	if (idx != -1) {
		idx += 8;
		str.Right( tStr, str.Length() - idx);
		idx = tStr.FindChar( ';');
		if (idx != -1)
			tStr.Left( str, idx);
		else
			str = tStr;
		str.Trim( kWhitespace);
		if ((str.CharAt( 0) == '"') && (str.Length() > 2)) {
			str.Mid( tStr, 1, str.Length() - 2);
			str = tStr;
			str.Trim( kWhitespace);
		}
	}
	else
		str.Truncate();
}

void nsEudoraCompose::ExtractType( nsString& str)
{
	nsString	tStr;
	PRInt32	idx = str.FindChar( ';');
	if (idx != -1) {
		str.Left( tStr, idx);
		str = tStr;
	}
	str.Trim( kWhitespace);

	if ((str.CharAt( 0) == '"') && (str.Length() > 2)) {
		str.Mid( tStr, 1, str.Length() - 2);
		str = tStr;
		str.Trim( kWhitespace);
	}

	// if multipart then ignore it since no eudora message body is ever
	// valid multipart!
	if (str.Length() > 10) {
		str.Left( tStr, 10);
		if (!tStr.Compare( "multipart/", PR_TRUE))
			str.Truncate();
	}
}

void nsEudoraCompose::CleanUpAttach( nsMsgAttachedFile *a, PRInt32 count)
{
	for (PRInt32 i = 0; i < count; i++) {
		NS_IF_RELEASE( a[i].orig_url);
		if (a[i].type)
			nsCRT::free( a[i].type);
		if (a[i].description)
			nsCRT::free( a[i].description);
		if (a[i].encoding)
			nsCRT::free( a[i].encoding);
	}

	delete [] a;
}

nsMsgAttachedFile * nsEudoraCompose::GetLocalAttachments( void)
{  
	/*
	nsIURI      *url = nsnull;
	*/

	PRInt32		count = 0;
	if (m_pAttachments)
		count = m_pAttachments->Count();
	if (!count)
		return( nsnull);

	nsMsgAttachedFile *a = (nsMsgAttachedFile *) new nsMsgAttachedFile[count + 1];
	if (!a)
		return( nsnull);
	nsCRT::memset(a, 0, sizeof(nsMsgAttachedFile) * (count + 1));
	
	nsresult	rv;
	char *		urlStr;
	EudoraAttachment *pAttach;
	for (PRInt32 i = 0; i < count; i++) {
		// nsMsgNewURL(&url, "file://C:/boxster.jpg");
		// a[i].orig_url = url;

		// NS_PRECONDITION( PR_FALSE, "Forced Break");

		pAttach = (EudoraAttachment *) m_pAttachments->ElementAt( i);
		a[i].file_spec = new nsFileSpec;
		pAttach->pAttachment->GetFileSpec( a[i].file_spec);
		urlStr = nsnull;
		pAttach->pAttachment->GetURLString( &urlStr);
		if (!urlStr) {
			CleanUpAttach( a, count);
			return( nsnull);
		}
		rv = m_pIOService->NewURI( urlStr, nsnull, &(a[i].orig_url));
		nsCRT::free( urlStr);
		if (NS_FAILED( rv)) {
			CleanUpAttach( a, count);
			return( nsnull);
		}

		a[i].type = nsCRT::strdup( pAttach->mimeType);
		a[i].description = nsCRT::strdup( pAttach->description);
		a[i].encoding = nsCRT::strdup( ENCODING_BINARY);
	}

	return( a);
}


// Test a message send????
nsresult nsEudoraCompose::SendMessage( nsIFileSpec *pMsg)
{
	nsresult	rv = CreateComponents();
	if (NS_SUCCEEDED( rv))
		rv = CreateIdentity();
	if (NS_FAILED( rv))
		return( rv);
	
	IMPORT_LOG0( "Eudora Compose created necessary components\n");

	nsString	bodyType;
	nsString	charSet;
	nsString	headerVal;
	GetHeaderValue( "From:", headerVal);
	if (headerVal.Length())
		m_pMsgFields->SetFrom( headerVal.GetUnicode());
	GetHeaderValue( "To:", headerVal);
	if (headerVal.Length())
		m_pMsgFields->SetTo( headerVal.GetUnicode());
	GetHeaderValue( "Subject:", headerVal);
	if (headerVal.Length())
		m_pMsgFields->SetSubject( headerVal.GetUnicode());
	GetHeaderValue( "Content-type:", headerVal);
	bodyType = headerVal;
	ExtractType( bodyType);
	ExtractCharset( headerVal);
	charSet = headerVal;
	if (headerVal.Length())
		m_pMsgFields->SetCharacterSet( headerVal.GetUnicode());
	GetHeaderValue( "CC:", headerVal);
	if (headerVal.Length())
		m_pMsgFields->SetCc( headerVal.GetUnicode());
	GetHeaderValue( "Message-ID:", headerVal);
	if (headerVal.Length())
		m_pMsgFields->SetMessageId( headerVal.GetUnicode());
	GetHeaderValue( "Reply-To:", headerVal);
	if (headerVal.Length())
		m_pMsgFields->SetReplyTo( headerVal.GetUnicode());

	// what about all of the other headers?!?!?!?!?!?!
	char *pMimeType = nsnull;
	if (bodyType.Length())
		pMimeType = bodyType.ToNewCString();
	
	IMPORT_LOG0( "Eudora compose calling CreateAndSendMessage\n");
	nsMsgAttachedFile *pAttach = GetLocalAttachments();

	rv = m_pMsgSend->CreateAndSendMessage(	nsnull,			// no editor shell
										m_pIdentity,	// dummy identity
										m_pMsgFields,	// message fields
										PR_FALSE,		// digest = NO
										PR_TRUE,		// dont_deliver = YES, make a file
										nsMsgDeliverNow,	// mode
										nsnull,			// no message to replace
										pMimeType,		// body type
										m_pBody,		// body pointer
										m_bodyLen,		// body length
										nsnull,			// remote attachment data
										pAttach,		// local attachments
										nsnull,			// related part
										nsnull);		// listener array

	
	if (pAttach)
		delete [] pAttach;

	SendListener *pListen = (SendListener *)m_pListener;
	if (NS_FAILED( rv)) {
		IMPORT_LOG1( "*** Error, CreateAndSendMessage FAILED: 0x%lx\n", rv);
		IMPORT_LOG1( "Headers: %s\n", m_pHeaders);
	}
	else {
		// wait for the listener to get done!
		PRInt32	abortCnt = 0;
		PRInt32	cnt = 0;
		PRInt32	sleepCnt = 1;		
		while (!pListen->m_done && (abortCnt < kHungAbortCount)) {
			PR_Sleep( sleepCnt);
			cnt++;
			if (cnt > kHungCount) {
				abortCnt++;
				sleepCnt *= 2;
				cnt = 0;
			}
		}

		if (abortCnt >= kHungAbortCount) {
			IMPORT_LOG0( "**** Create and send message hung\n");
			IMPORT_LOG1( "Headers: %s\n", m_pHeaders);
			IMPORT_LOG1( "Body: %s\n", m_pBody);
			rv = NS_ERROR_FAILURE;
		}

		
		IMPORT_LOG0( " done with CreateAndSendMessage\n");
	}

	if (pMimeType)
		nsCRT::free( pMimeType);

	if (pListen->m_location) {
		pMsg->FromFileSpec( pListen->m_location);
		rv = NS_OK;
		IMPORT_LOG0( "Eudora compose successful\n");
	}
	else {
		rv = NS_ERROR_FAILURE;
		IMPORT_LOG0( "*** Error, Eudora compose unsuccessful\n");
	}
	
	pListen->Reset();
		
	return( rv);
}

