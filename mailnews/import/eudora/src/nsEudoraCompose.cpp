/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nscore.h"
#include "prthread.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"

#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"

#include "nsIMsgCompose.h"
#include "nsIMsgCompFields.h"
#include "nsIMsgSend.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgI18N.h"

#include "nsNetCID.h"

#include "nsEudoraCompose.h"

#include "EudoraDebugLog.h"

#include "nsMimeTypes.h"
#include "nsIPref.h"
#include "nsMsgUtils.h"

static NS_DEFINE_CID( kMsgSendCID, NS_MSGSEND_CID);
static NS_DEFINE_CID( kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID); 
static NS_DEFINE_CID( kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID( kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);


// We need to do some calculations to set these numbers to something reasonable!
// Unless of course, CreateAndSendMessage will NEVER EVER leave us in the lurch
#define kHungCount		100000
#define kHungAbortCount	1000


#ifdef IMPORT_DEBUG
static char *p_test_headers = 
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

static char *p_test_body = 
"Hello world?\n\
";
#else
#define	p_test_headers	nsnull
#define	p_test_body nsnull
#endif


#define kWhitespace	"\b\t\r\n "


// First off, a listener
class EudoraSendListener : public nsIMsgSendListener
{
public:
	EudoraSendListener() {
		m_done = PR_FALSE;
		m_location = nsnull;
	}

	virtual ~EudoraSendListener() { NS_IF_RELEASE( m_location); }

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

    /* void OnSendNotPerformed */
    NS_IMETHOD OnSendNotPerformed(const char *aMsgID, nsresult aStatus) {return NS_OK;}

  /* void OnGetDraftFolderURI (); */
  NS_IMETHOD OnGetDraftFolderURI(const char *aFolderURI) {return NS_OK;}

	static nsresult CreateSendListener( nsIMsgSendListener **ppListener);

	void Reset() { m_done = PR_FALSE; NS_IF_RELEASE( m_location); m_location = nsnull;}

public:
	PRBool			m_done;
	nsIFileSpec *	m_location;
};


NS_IMPL_THREADSAFE_ISUPPORTS1(EudoraSendListener, nsIMsgSendListener)

nsresult EudoraSendListener::CreateSendListener( nsIMsgSendListener **ppListener)
{
    NS_PRECONDITION(ppListener != nsnull, "null ptr");
    if (! ppListener)
        return NS_ERROR_NULL_POINTER;

    *ppListener = new EudoraSendListener();
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
	m_pSendProxy = nsnull;
	m_pMsgFields = nsnull;
	m_pIdentity = nsnull;
	m_pHeaders = p_test_headers;
	if (m_pHeaders)
		m_headerLen = strlen( m_pHeaders);
	else
		m_headerLen = 0;
	m_pBody = p_test_body;
	if (m_pBody)
		m_bodyLen = strlen( m_pBody);
	else
		m_bodyLen = 0;

	m_readHeaders.m_convertCRs = PR_TRUE;
}


nsEudoraCompose::~nsEudoraCompose()
{
	NS_IF_RELEASE( m_pSendProxy);
	NS_IF_RELEASE( m_pIOService);
	NS_IF_RELEASE( m_pMsgSend);
	NS_IF_RELEASE( m_pListener);
	NS_IF_RELEASE( m_pMsgFields);

	if (m_pIdentity) {	
		nsresult rv = m_pIdentity->ClearAllValues();
        NS_ASSERTION(NS_SUCCEEDED(rv),"failed to clear values");
		if (NS_FAILED(rv)) return;

		NS_WITH_PROXIED_SERVICE(nsIMsgAccountManager, accMgr, NS_MSGACCOUNTMANAGER_CONTRACTID, NS_UI_THREAD_EVENTQ, &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv) && accMgr,"failed to get account manager");
		if (NS_FAILED(rv) || !accMgr) return;

		rv = accMgr->RemoveIdentity(m_pIdentity);
        NS_ASSERTION(NS_SUCCEEDED(rv),"failed to remove identity");
		if (NS_FAILED(rv)) return;

	    NS_RELEASE(m_pIdentity);
	}

}

nsresult nsEudoraCompose::CreateIdentity( void)
{
	if (m_pIdentity)
		return( NS_OK);

	nsresult	rv;
    NS_WITH_PROXIED_SERVICE(nsIMsgAccountManager, accMgr, NS_MSGACCOUNTMANAGER_CONTRACTID, NS_UI_THREAD_EVENTQ, &rv);
    if (NS_FAILED(rv)) return( rv);
	rv = accMgr->CreateIdentity( &m_pIdentity);
	nsString	name(NS_LITERAL_STRING("Import Identity"));
	if (m_pIdentity) {
		m_pIdentity->SetFullName( name.get());
		m_pIdentity->SetIdentityName( name.get());
		m_pIdentity->SetEmail( "import@import.service");
	}
	
	return( rv);
}

nsresult nsEudoraCompose::CreateComponents( void)
{
	nsresult	rv = NS_OK;
	
	if (!m_pIOService) {
		IMPORT_LOG0( "Creating nsIOService\n");

		NS_WITH_PROXIED_SERVICE(nsIIOService, service, kIOServiceCID, NS_UI_THREAD_EVENTQ, &rv);
		if (NS_FAILED(rv)) 
			return( rv);
		m_pIOService = service;
		NS_IF_ADDREF( m_pIOService);
	}
	
	NS_IF_RELEASE( m_pMsgFields);
	if (!m_pMsgSend) {
		rv = nsComponentManager::CreateInstance( kMsgSendCID, nsnull, NS_GET_IID( nsIMsgSend), (void **) &m_pMsgSend); 
		if (NS_SUCCEEDED( rv) && m_pMsgSend) {
			nsCOMPtr<nsIProxyObjectManager> proxyMgr = 
			         do_GetService(kProxyObjectManagerCID, &rv);
			if (NS_SUCCEEDED(rv)) {
				rv = proxyMgr->GetProxyForObject( NS_UI_THREAD_EVENTQ, NS_GET_IID(nsIMsgSend),
										m_pMsgSend, PROXY_SYNC, (void**)&m_pSendProxy);
				if (NS_FAILED( rv)) {
					m_pSendProxy = nsnull;
				}
			}
			if (NS_FAILED( rv)) {
				NS_RELEASE( m_pMsgSend);
				m_pMsgSend = nsnull;
			}
		}
	}
	if (!m_pListener && NS_SUCCEEDED( rv)) {
		rv = EudoraSendListener::CreateSendListener( &m_pListener);
	}

	if (NS_SUCCEEDED(rv) && m_pMsgSend) { 
	    rv = nsComponentManager::CreateInstance( kMsgCompFieldsCID, nsnull, nsCOMTypeInfo<nsIMsgCompFields>::GetIID(), (void **) &m_pMsgFields); 
		if (NS_SUCCEEDED(rv) && m_pMsgFields) {
			// IMPORT_LOG0( "nsOutlookCompose - CreateComponents succeeded\n");
			m_pMsgFields->SetForcePlainText( PR_FALSE);
			return( NS_OK);
		}
	}

	return( NS_ERROR_FAILURE);
}

void nsEudoraCompose::GetNthHeader( const char *pData, PRInt32 dataLen, PRInt32 n, nsCString& header, nsCString& val, PRBool unwrap)
{
	header.Truncate();
	val.Truncate();
	if (!pData)
		return;

	PRInt32	index = 0;
	PRInt32	len;
	PRInt32	start = 0;
	const char *pChar = pData;
	const char *pStart;
	if (n == 0) {
		pStart = pChar;
		len = 0;
		while ((start < dataLen) && (*pChar != ':')) {
			start++;
			len++;
			pChar++;
		}
		header.Append( pStart, len);
		header.Trim( kWhitespace);
		start++;
		pChar++;
	}
	else {
		while (start < dataLen) {
			if ((*pChar != ' ') && (*pChar != 9)) {
				if (n == index) {
					pStart = pChar;
					len = 0;
					while ((start < dataLen) && (*pChar != ':')) {
						start++;
						len++;
						pChar++;
					}
					header.Append( pStart, len);
					header.Trim( kWhitespace);
					start++;
					pChar++;
					break;
				}
				else
					index++;
			}

			while ((start < dataLen) && (*pChar != 0x0D) && (*pChar != 0x0A)) {
				start++;
				pChar++;
			}
			while ((start < dataLen) && ((*pChar == 0x0D) || (*pChar == 0x0A))) {
				start++;
				pChar++;
			}
		}
	}

	if (start >= dataLen)
		return;

	PRInt32		lineEnd;
	PRInt32		end = start;
	while (end < dataLen) {
		while ((end < dataLen) && (*pChar != 0x0D) && (*pChar != 0x0A)) {
			end++;
			pChar++;
		}
		if (end > start) {
			val.Append( pData + start, end - start);
		}
		
		lineEnd = end;
		pStart = pChar;
		while ((end < dataLen) && ((*pChar == 0x0D) || (*pChar == 0x0A))) {
			end++;
			pChar++;
		}
		
		start = end;

		while ((end < dataLen) && ((*pChar == ' ') || (*pChar == '\t'))) {
			end++;
			pChar++;
		}

		if (start == end)
			break;
		
		if (unwrap)
			val.Append( ' ');
		else {
			val.Append( pStart, end - lineEnd);
		}

		start = end;
	}
	
	val.Trim( kWhitespace);
}


void nsEudoraCompose::GetHeaderValue( const char *pData, PRInt32 dataLen, const char *pHeader, nsCString& val, PRBool unwrap)
{
	val.Truncate();
	if (!pData)
		return;

	PRInt32	start = 0;
	PRInt32 len = strlen( pHeader);
	const char *pChar = pData;
	if (!nsCRT::strncasecmp( pHeader, pData, len)) {
		start = len;
	}
	else {
		while (start < dataLen) {
			while ((start < dataLen) && (*pChar != 0x0D) && (*pChar != 0x0A)) {
				start++;
				pChar++;
			}
			while ((start < dataLen) && ((*pChar == 0x0D) || (*pChar == 0x0A))) {
				start++;
				pChar++;
			}
			if ((start < dataLen) && !nsCRT::strncasecmp( pChar, pHeader, len))
				break;
		}
		if (start < dataLen)
			start += len;
	}

	if (start >= dataLen)
		return;

	PRInt32			end = start;
	PRInt32			lineEnd;
	const char *	pStart;

	pChar =		pData + start;

	while (end < dataLen) {
		while ((end < dataLen) && (*pChar != 0x0D) && (*pChar != 0x0A)) {
			end++;
			pChar++;
		}
		if (end > start) {
			val.Append( pData + start, end - start);
		}
		
		lineEnd = end;
		pStart = pChar;
		while ((end < dataLen) && ((*pChar == 0x0D) || (*pChar == 0x0A))) {
			end++;
			pChar++;
		}
		
		start = end;

		while ((end < dataLen) && ((*pChar == ' ') || (*pChar == '\t'))) {
			end++;
			pChar++;
		}

		if (start == end)
			break;
		
		if (unwrap)
			val.Append( ' ');
		else {
			val.Append( pStart, end - lineEnd);
		}

		start = end;
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

	// if multipart then ignore it since no outlook message body is ever
	// valid multipart!
	if (str.Length() > 10) {
		str.Left( tStr, 10);
		if (tStr.LowerCaseEqualsLiteral("multipart/"))
			str.Truncate();
	}
}

void nsEudoraCompose::CleanUpAttach( nsMsgAttachedFile *a, PRInt32 count)
{
  for (PRInt32 i = 0; i < count; i++) {
    a[i].orig_url=nsnull;
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
	memset(a, 0, sizeof(nsMsgAttachedFile) * (count + 1));
	
	nsresult			rv;
	nsXPIDLCString   	urlStr;
	ImportAttachment *	pAttach;

	for (PRInt32 i = 0; i < count; i++) {
		// nsMsgNewURL(&url, "file://C:/boxster.jpg");
		// a[i].orig_url = url;

		// NS_PRECONDITION( PR_FALSE, "Forced Break");

		pAttach = (ImportAttachment *) m_pAttachments->ElementAt( i);
		a[i].file_spec = new nsFileSpec;
		pAttach->pAttachment->GetFileSpec( a[i].file_spec);
		urlStr.Adopt(0);
		pAttach->pAttachment->GetURLString(getter_Copies(urlStr));
		if (!urlStr) {
			CleanUpAttach( a, count);
			return( nsnull);
		}
		rv = m_pIOService->NewURI( urlStr, nsnull, nsnull, getter_AddRefs(a[i].orig_url));
		if (NS_FAILED( rv)) {
			CleanUpAttach( a, count);
			return( nsnull);
		}

		a[i].type = nsCRT::strdup( pAttach->mimeType);
		a[i].real_name = nsCRT::strdup( pAttach->description);
		a[i].encoding = nsCRT::strdup( ENCODING_BINARY);
	}

	return( a);
}

// Test a message send????
nsresult nsEudoraCompose::SendTheMessage( nsIFileSpec *pMsg)
{
	nsresult	rv = CreateComponents();
	if (NS_SUCCEEDED( rv))
		rv = CreateIdentity();
	if (NS_FAILED( rv))
		return( rv);
	
	// IMPORT_LOG0( "Outlook Compose created necessary components\n");

	nsString	bodyType;
	nsString	charSet;
	nsString	headerVal;
	GetHeaderValue( m_pHeaders, m_headerLen, "From:", headerVal);
	if (!headerVal.IsEmpty())
		m_pMsgFields->SetFrom( headerVal);
	GetHeaderValue( m_pHeaders, m_headerLen, "To:", headerVal);
	if (!headerVal.IsEmpty())
		m_pMsgFields->SetTo( headerVal);
	GetHeaderValue( m_pHeaders, m_headerLen, "Subject:", headerVal);
	if (!headerVal.IsEmpty())
		m_pMsgFields->SetSubject( headerVal);
	GetHeaderValue( m_pHeaders, m_headerLen, "Content-type:", headerVal);
	bodyType = headerVal;
	ExtractType( bodyType);
	ExtractCharset( headerVal);
  // Use platform charset as default if the msg doesn't specify one
  // (ie, no 'charset' param in the Content-Type: header). As the last
  // resort we'll use the mail defaul charset.
  if (headerVal.IsEmpty())
  {
    headerVal.AssignWithConversion(nsMsgI18NFileSystemCharset());
    if (headerVal.IsEmpty())
    { // last resort
      if (m_defCharset.IsEmpty())
      {
        nsXPIDLString defaultCharset;
        nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv))
          rv = prefs->GetLocalizedUnicharPref("mailnews.view_default_charset", getter_Copies(defaultCharset));
        m_defCharset.Assign(defaultCharset ? defaultCharset.get() : NS_LITERAL_STRING("ISO-8859-1").get());
      }
      headerVal = m_defCharset;
    }
  }
  m_pMsgFields->SetCharacterSet( NS_LossyConvertUCS2toASCII(headerVal).get() );
  charSet = headerVal;
	GetHeaderValue( m_pHeaders, m_headerLen, "CC:", headerVal);
	if (!headerVal.IsEmpty())
		m_pMsgFields->SetCc( headerVal);
	GetHeaderValue( m_pHeaders, m_headerLen, "Message-ID:", headerVal);
	if (!headerVal.IsEmpty())
		m_pMsgFields->SetMessageId( NS_LossyConvertUCS2toASCII(headerVal).get() );
	GetHeaderValue( m_pHeaders, m_headerLen, "Reply-To:", headerVal);
	if (!headerVal.IsEmpty())
		m_pMsgFields->SetReplyTo( headerVal);

	// what about all of the other headers?!?!?!?!?!?!
	char *pMimeType = nsnull;
	if (!bodyType.IsEmpty())
		pMimeType = ToNewCString(bodyType);
	
	// IMPORT_LOG0( "Outlook compose calling CreateAndSendMessage\n");
	nsMsgAttachedFile *pAttach = GetLocalAttachments();


	/*
		l10n - I have the body of the message in the system charset,
		I need to "encode" it to be the charset for the message
		*UNLESS* of course, I don't know what the charset of the message
		should be?  How do I determine what the charset should
		be if it doesn't exist?

	*/
	
	nsString	uniBody;
	NS_CopyNativeToUnicode( nsDependentCString(m_pBody), uniBody);

	nsCString	body;

	rv = nsMsgI18NConvertFromUnicode( NS_LossyConvertUTF16toASCII(charSet).get(),
                                    uniBody, body);
	if (NS_FAILED( rv)) {
		// in this case, if we did not use the default compose
		// charset, then try that.
		if (!charSet.Equals( m_defCharset)) {
			body.Truncate();
			rv = nsMsgI18NConvertFromUnicode( NS_LossyConvertUTF16toASCII(charSet).get(),
                                        uniBody, body);
		}
	}
	uniBody.Truncate();


  // See if it's a draft msg (ie, no From: or no To: AND no Cc: AND no Bcc:).
  // Eudora saves sent and draft msgs in Out folder (ie, mixed) and it does
  // store Bcc: header in the msg itself.
  nsMsgDeliverMode mode = nsIMsgSend::nsMsgDeliverNow;
  nsAutoString from, to, cc, bcc;
  rv = m_pMsgFields->GetFrom(from);
  rv = m_pMsgFields->GetTo(to);
  rv = m_pMsgFields->GetCc(cc);
  rv = m_pMsgFields->GetBcc(bcc);
  if ( from.IsEmpty() || to.IsEmpty() && cc.IsEmpty() && bcc.IsEmpty() )
    mode = nsIMsgSend::nsMsgSaveAsDraft;

	if (NS_FAILED( rv)) {

		rv = m_pSendProxy->CreateAndSendMessage(
                    nsnull,			                  // no editor shell
										m_pIdentity,	                // dummy identity
                                                                                nsnull,                         // account key
										m_pMsgFields,	                // message fields
										PR_FALSE,		                  // digest = NO
										PR_TRUE,		                  // dont_deliver = YES, make a file
										mode,                         // mode
										nsnull,			                  // no message to replace
										pMimeType,		                // body type
										m_pBody,		                  // body pointer
										m_bodyLen,		                // body length
										nsnull,			                  // remote attachment data
										pAttach,		                  // local attachments
										nsnull,			                  // related part
										nsnull,                       // parent window
										nsnull,                       // progress listener
										m_pListener,		              // listener
                    nsnull);                      // password

	}
	else {
		rv = m_pSendProxy->CreateAndSendMessage(
                    nsnull,			                  // no editor shell
										m_pIdentity,	                // dummy identity
                                                                                nsnull,                         // account key
										m_pMsgFields,	                // message fields
										PR_FALSE,		                  // digest = NO
										PR_TRUE,		                  // dont_deliver = YES, make a file
										mode,                         // mode
										nsnull,			                  // no message to replace
										pMimeType,		                // body type
										body.get(),		                      // body pointer
										body.Length(),		            // body length
										nsnull,			                  // remote attachment data
										pAttach,		                  // local attachments
										nsnull,			                  // related part
										nsnull,                       // parent window
										nsnull,                       // progress listener
										m_pListener,		              // listener
                    nsnull);                      // password

	}

	// IMPORT_LOG0( "Returned from CreateAndSendMessage\n");

	if (pAttach)
		delete [] pAttach;

	EudoraSendListener *pListen = (EudoraSendListener *)m_pListener;
	if (NS_FAILED( rv)) {
		IMPORT_LOG1( "*** Error, CreateAndSendMessage FAILED: 0x%lx\n", rv);
		// IMPORT_LOG1( "Headers: %80s\n", m_pHeaders);
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

	}

	if (pMimeType)
		nsCRT::free( pMimeType);

	if (pListen->m_location) {
		pMsg->FromFileSpec( pListen->m_location);
		rv = NS_OK;
	}
	else {
		rv = NS_ERROR_FAILURE;
		IMPORT_LOG0( "*** Error, Outlook compose unsuccessful\n");
	}
	
	pListen->Reset();

	return( rv);
}


PRBool SimpleBufferTonyRCopiedOnce::SpecialMemCpy( PRInt32 offset, const char *pData, PRInt32 len, PRInt32 *pWritten)
{
	// Arg!!!!!  Mozilla can't handle plain CRs in any mail messages.  Particularly a 
	// problem with Eudora since it doesn't give a rats a**
	*pWritten = len;
	PRInt32	sz = offset + len;
	if (offset) {
		if ((m_pBuffer[offset - 1] == 0x0D) && (*pData != 0x0A)) {
			sz++;
			if (!Grow( sz)) return( PR_FALSE);
			m_pBuffer[offset] = 0x0A;
			offset++;
			(*pWritten)++;
		}
	}
	while (len > 0) {
		if ((*pData == 0x0D) && (*(pData + 1) != 0x0A)) {
			sz++;
			if (!Grow( sz)) return( PR_FALSE);
			m_pBuffer[offset] = 0x0D;
			offset++;
			m_pBuffer[offset] = 0x0A;								
			(*pWritten)++;
		}
		else {
			m_pBuffer[offset] = *pData;
		}
		offset++;
		pData++;
		len--;
	}
	
	return( PR_TRUE);
}

nsresult nsEudoraCompose::ReadHeaders( ReadFileState *pState, SimpleBufferTonyRCopiedOnce& copy, SimpleBufferTonyRCopiedOnce& header)
{
	// This should be the headers...
	header.m_writeOffset = 0;

	nsresult	rv;
	PRInt32		lineLen;
	PRInt32		endLen = -1;
	PRInt8		endBuffer = 0;

	while ((endLen = IsEndHeaders( copy)) == -1) {
		while ((lineLen = FindNextEndLine( copy)) == -1) {
			copy.m_writeOffset = copy.m_bytesInBuf;
			if (!header.Write( copy.m_pBuffer, copy.m_writeOffset)) {
				IMPORT_LOG0( "*** ERROR, writing headers\n");
				return( NS_ERROR_FAILURE);
			}
			if (NS_FAILED( rv = FillMailBuffer( pState, copy))) {
				IMPORT_LOG0( "*** Error reading message headers\n");
				return( rv);
			}
			if (!copy.m_bytesInBuf) {
				IMPORT_LOG0( "*** Error, end of file while reading headers\n");
				return( NS_ERROR_FAILURE);
			}
		}
		copy.m_writeOffset += lineLen;
		if ((copy.m_writeOffset + 4) >= copy.m_bytesInBuf) {
			if (!header.Write( copy.m_pBuffer, copy.m_writeOffset)) {
				IMPORT_LOG0( "*** ERROR, writing headers 2\n");
				return( NS_ERROR_FAILURE);
			}
			if (NS_FAILED( rv = FillMailBuffer( pState, copy))) {
				IMPORT_LOG0( "*** Error reading message headers 2\n");
				return( rv);
			}
		}
	}

	if (!header.Write( copy.m_pBuffer, copy.m_writeOffset)) {
		IMPORT_LOG0( "*** Error writing final headers\n");
		return( NS_ERROR_FAILURE);
	}
	if (!header.Write( (const char *)&endBuffer, 1)) {
		IMPORT_LOG0( "*** Error writing header trailing null\n");
		return( NS_ERROR_FAILURE);
	}

	copy.m_writeOffset += endLen;
	
	return( NS_OK);
}

PRInt32 nsEudoraCompose::FindNextEndLine( SimpleBufferTonyRCopiedOnce& data)
{
	PRInt32 len = data.m_bytesInBuf - data.m_writeOffset;
	if (!len)
		return( -1);
	PRInt32	count = 0;
	const char *pData = data.m_pBuffer + data.m_writeOffset;
	while (((*pData == 0x0D) || (*pData == 0x0A)) && (count < len)) {
		pData++;
		count++;
	}
	while ((*pData != 0x0D) && (*pData != 0x0A) && (count < len)) {
		pData++;
		count++;
	}
	
	if (count < len)
		return( count);

	return( -1);
}

PRInt32 nsEudoraCompose::IsEndHeaders( SimpleBufferTonyRCopiedOnce& data)
{
	PRInt32 len = data.m_bytesInBuf - data.m_writeOffset;
	if (len < 2)
		return( -1);
	const char *pChar = data.m_pBuffer + data.m_writeOffset;
	if ((*pChar == 0x0D) && (*(pChar + 1) == 0x0D))
		return( 2);

	if (len < 4)
		return( -1);
	if ((*pChar == 0x0D) && (*(pChar + 1) == 0x0A) &&
		(*(pChar + 2) == 0x0D) && (*(pChar + 3) == 0x0A))
		return( 4);

	return( -1);
}


nsresult nsEudoraCompose::CopyComposedMessage( nsCString& fromLine, nsIFileSpec *pSrc, nsIFileSpec *pDst, SimpleBufferTonyRCopiedOnce& copy)
{
	copy.m_bytesInBuf = 0;
	copy.m_writeOffset = 0;
	ReadFileState	state;
	state.pFile = pSrc;
	state.offset = 0;
	state.size = 0;
	pSrc->GetFileSize( &state.size);
	if (!state.size) {
		IMPORT_LOG0( "*** Error, unexpected zero file size for composed message\n");
		return( NS_ERROR_FAILURE);
	}

	nsresult rv = pSrc->OpenStreamForReading();
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Error, unable to open composed message file\n");
		return( NS_ERROR_FAILURE);
	}
	
	PRInt32 written;
	rv = pDst->Write( fromLine.get(), fromLine.Length(), &written);

	// well, isn't this a hoot!
	// Read the headers from the new message, get the ones we like
	// and write out only the headers we want from the new message,
	// along with all of the other headers from the "old" message!
	if (NS_SUCCEEDED( rv))
		rv = FillMailBuffer( &state, copy);
	if (NS_SUCCEEDED( rv))
		rv = ReadHeaders( &state, copy, m_readHeaders);
	
	if (NS_SUCCEEDED( rv)) {
		rv = WriteHeaders( pDst, m_readHeaders);
	}

	// We need to go ahead and write out the rest of the copy buffer
	// so that the following will properly copy the rest of the body
	char	lastChar = 0;

	if (NS_SUCCEEDED( rv)) {
    rv = EscapeFromSpaceLine(pDst, copy.m_pBuffer + copy.m_writeOffset, copy.m_pBuffer+copy.m_bytesInBuf);
		if (copy.m_bytesInBuf)
			lastChar = copy.m_pBuffer[copy.m_bytesInBuf - 1];
    if (NS_SUCCEEDED(rv))
		  copy.m_writeOffset = copy.m_bytesInBuf;
	}

	while ((state.offset < state.size) && NS_SUCCEEDED( rv)) {
		rv = FillMailBuffer( &state, copy);
		if (NS_SUCCEEDED( rv)) {
      rv = EscapeFromSpaceLine(pDst, copy.m_pBuffer + copy.m_writeOffset, copy.m_pBuffer+copy.m_bytesInBuf);
			lastChar = copy.m_pBuffer[copy.m_bytesInBuf - 1];
			if (NS_SUCCEEDED( rv))
		    copy.m_writeOffset = copy.m_bytesInBuf;
	    else
        IMPORT_LOG0( "*** Error writing to destination mailbox\n");
		}
	}

	pSrc->CloseStream();
	
	if ((lastChar != 0x0A) && NS_SUCCEEDED( rv)) {
		rv = pDst->Write( "\x0D\x0A", 2, &written);
		if (written != 2)
			rv = NS_ERROR_FAILURE;
	}

	return( rv);
}

nsresult nsEudoraCompose::FillMailBuffer( ReadFileState *pState, SimpleBufferTonyRCopiedOnce& read)
{
	if (read.m_writeOffset >= read.m_bytesInBuf) {
		read.m_writeOffset = 0;
		read.m_bytesInBuf = 0;
	}
	else if (read.m_writeOffset) {
		memcpy( read.m_pBuffer, read.m_pBuffer + read.m_writeOffset, read.m_bytesInBuf - read.m_writeOffset);
		read.m_bytesInBuf -= read.m_writeOffset;
		read.m_writeOffset = 0;
	}

	PRInt32	count = read.m_size - read.m_bytesInBuf;
	if (((PRUint32)count + pState->offset) > pState->size)
		count = pState->size - pState->offset;
	if (count) {
		PRInt32		bytesRead = 0;
		char *		pBuffer = read.m_pBuffer + read.m_bytesInBuf;
		nsresult	rv = pState->pFile->Read( &pBuffer, count, &bytesRead);
		if (NS_FAILED( rv)) return( rv);
		if (bytesRead != count) return( NS_ERROR_FAILURE);
		read.m_bytesInBuf += bytesRead;
		pState->offset += bytesRead;
	}

	return( NS_OK);
}


#define kMaxSpecialHeaders	3
static const char *gSpecialHeaders[kMaxSpecialHeaders] = {
	"Content-type",
	"MIME-Version",
	"Content-transfer-encoding"
};
// consider "X-Accept-Language"?

#define kMaxReplaceHeaders	5
static const char *gReplaceHeaders[kMaxReplaceHeaders] = {
	"From",
	"To",
	"Subject",
	"Reply-to",
	"cc"
};

PRBool	nsEudoraCompose::IsReplaceHeader( const char *pHeader)
{
	for (int i = 0; i < kMaxReplaceHeaders; i++) {
		if (!nsCRT::strcasecmp( pHeader, gReplaceHeaders[i]))
			return( PR_TRUE);
	}

	return( PR_FALSE);
}

PRInt32 nsEudoraCompose::IsSpecialHeader( const char *pHeader)
{
	for (int i = 0; i < kMaxSpecialHeaders; i++) {
		if (!nsCRT::strcasecmp( pHeader, gSpecialHeaders[i]))
			return( (PRInt32) i);
	}

	return( -1);
}


nsresult nsEudoraCompose::WriteHeaders( nsIFileSpec *pDst, SimpleBufferTonyRCopiedOnce& newHeaders)
{
	// Well, ain't this a peach?
	// This is rather disgusting but there really isn't much to be done about it....

	// 1. For each "old" header, replace it with the new one if we want,
	// then right it out.
	// 2. Then if we haven't written the "important" new headers, write them out
	// 3. Terminate the headers with an extra eol.
	
	PRInt32		n = 0;
	nsCString	header;
	nsCString	val;
	nsCString	replaceVal;
	PRInt32		written;
	nsresult	rv = NS_OK; // it's ok if we don't have the first header on the predefined lists.
	PRInt32		specialHeader;
	PRBool		specials[kMaxSpecialHeaders];
  PRBool    hasDateHeader = PR_FALSE;
	int			i;

	for (i = 0; i < kMaxSpecialHeaders; i++)
		specials[i] = PR_FALSE;

  // m_pHeaders - contains headers from a Eudora msg.
  // newHeaders - contains headers from a mozilla msg (more headers here).
	do {
		GetNthHeader( m_pHeaders, m_headerLen, n, header, val, PR_FALSE);
		// GetNthHeader( newHeaders.m_pBuffer, newHeaders.m_writeOffset, n, header, val, PR_FALSE);
		if (!header.IsEmpty()) {
			if ((specialHeader = IsSpecialHeader( header.get())) != -1) {
				header.Append( ':');
				GetHeaderValue( newHeaders.m_pBuffer, newHeaders.m_writeOffset - 1, header.get(), val, PR_FALSE);
				header.Truncate( header.Length() - 1);
				specials[specialHeader] = PR_TRUE;
			}
			else if (IsReplaceHeader( header.get())) {
				replaceVal.Truncate( 0);
				header.Append( ':');
				GetHeaderValue( newHeaders.m_pBuffer, newHeaders.m_writeOffset - 1, header.get(), replaceVal, PR_FALSE);
				header.Truncate( header.Length() - 1);
				if (!replaceVal.IsEmpty())
					val = replaceVal;
			}
			if (!val.IsEmpty()) {
        // See if we're writing out a Date: header.
        if (!nsCRT::strcasecmp(header.get(), "Date"))
          hasDateHeader = PR_TRUE;
				rv = pDst->Write( header.get(), header.Length(), &written);
				if (NS_SUCCEEDED( rv))
					rv = pDst->Write( ": ", 2, &written);
				if (NS_SUCCEEDED( rv))
					rv = pDst->Write( val.get(), val.Length(), &written);
				if (NS_SUCCEEDED( rv))
					rv = pDst->Write( "\x0D\x0A", 2, &written);

			}
		}
		n++;
	} while (NS_SUCCEEDED( rv) && !header.IsEmpty());

  // If we don't have Date: header so far then use the default one (taken from Eudora "From " line).
  if (!hasDateHeader)
  {
    rv = pDst->Write(m_defaultDate.get(), m_defaultDate.Length(), &written);
    if (NS_SUCCEEDED( rv))
      rv = pDst->Write( "\x0D\x0A", 2, &written);
  }

	for (i = 0; (i < kMaxSpecialHeaders) && NS_SUCCEEDED( rv); i++) {
		if (!specials[i]) {
			header = gSpecialHeaders[i];
			header.Append( ':');
			GetHeaderValue( newHeaders.m_pBuffer, newHeaders.m_writeOffset - 1, header.get(), val, PR_FALSE);
			header.Truncate( header.Length() - 1);
			if (!val.IsEmpty()) {
				rv = pDst->Write( header.get(), header.Length(), &written);
				if (NS_SUCCEEDED( rv))
					rv = pDst->Write( ": ", 2, &written);
				if (NS_SUCCEEDED( rv))
					rv = pDst->Write( val.get(), val.Length(), &written);
				if (NS_SUCCEEDED( rv))
					rv = pDst->Write( "\x0D\x0A", 2, &written);
			}
		}
	}
	

	if (NS_SUCCEEDED( rv))
		rv = pDst->Write( "\x0D\x0A", 2, &written);
	return( rv);
}


