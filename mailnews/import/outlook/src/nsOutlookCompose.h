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

#ifndef nsOutlookCompose_h__
#define nsOutlookCompose_h__

#include "nscore.h"
#include "nsString.h"
#include "nsIFileSpec.h"
#include "nsVoidArray.h"
#include "nsISupportsArray.h"

class nsIMsgSend;
class nsIMsgCompFields;
class nsIMsgIdentity;
class nsIMsgSendListener;
class nsIIOService;

#include "nsIMsgSend.h"


typedef struct {
	nsIFileSpec *	pAttachment;
	char *			mimeType;
	char *			description;
} OutlookAttachment;

typedef struct {
	PRUint32		offset;
	PRUint32		size;
	nsIFileSpec *	pFile;
} ReadFileState;

class SimpleBufferTonyRCopiedTwice {
public:
	SimpleBufferTonyRCopiedTwice() {m_pBuffer = nsnull; m_size = 0; m_growBy = 4096; m_writeOffset = 0;
					m_bytesInBuf = 0; m_convertCRs = PR_FALSE;}
	~SimpleBufferTonyRCopiedTwice() { if (m_pBuffer) delete [] m_pBuffer;}
	
	PRBool Allocate( PRInt32 sz) { 
		if (m_pBuffer) delete [] m_pBuffer; 
		m_pBuffer = new char[sz]; 
		if (m_pBuffer) { m_size = sz; return( PR_TRUE); }
		else { m_size = 0; return( PR_FALSE);}
	}

	PRBool Grow( PRInt32 newSize) { if (newSize > m_size) return( ReAllocate( newSize)); else return( PR_TRUE);}
	PRBool ReAllocate( PRInt32 newSize) {
		if (newSize <= m_size) return( PR_TRUE);
		char *pOldBuffer = m_pBuffer;
		PRInt32	oldSize = m_size;
		m_pBuffer = nsnull;
		while (m_size < newSize) m_size += m_growBy;
		if (Allocate( m_size)) {
			if (pOldBuffer) { nsCRT::memcpy( m_pBuffer, pOldBuffer, oldSize); delete [] pOldBuffer;}
			return( PR_TRUE);
		}
		else { m_pBuffer = pOldBuffer; m_size = oldSize; return( PR_FALSE);}
	}
	
	PRBool Write( PRInt32 offset, const char *pData, PRInt32 len, PRInt32 *pWritten) {
		*pWritten = len;
		if (!len) return( PR_TRUE);
		if (!Grow( offset + len)) return( PR_FALSE);
		if (m_convertCRs)
			return( SpecialMemCpy( offset, pData, len, pWritten));
		nsCRT::memcpy( m_pBuffer + offset, pData, len);
		return( PR_TRUE);
	}
	
	PRBool Write( const char *pData, PRInt32 len) { 
		PRInt32 written;
		if (Write( m_writeOffset, pData, len, &written)) { m_writeOffset += written; return( PR_TRUE);}
		else return( PR_FALSE);
	}

	PRBool	SpecialMemCpy( PRInt32 offset, const char *pData, PRInt32 len, PRInt32 *pWritten);
	
	PRBool	m_convertCRs;
	char *	m_pBuffer;
	PRInt32	m_bytesInBuf;	// used when reading into this buffer
	PRInt32	m_size;			// allocated size of buffer
	PRInt32	m_growBy;		// duh
	PRInt32	m_writeOffset;	// used when writing into and reading from the buffer
};



class nsOutlookCompose {
public:
	nsOutlookCompose();
	~nsOutlookCompose();

	nsresult	SendTheMessage( nsIFileSpec *pMsg);

	void		SetBody( const char *pBody, PRInt32 len) { m_pBody = pBody; m_bodyLen = len;}
	void		SetHeaders( const char *pHeaders, PRInt32 len) { m_pHeaders = pHeaders; m_headerLen = len;}
	void		SetAttachments( nsVoidArray *pAttachments) { m_pAttachments = pAttachments;}

	nsresult	CopyComposedMessage( nsCString& fromLine, nsIFileSpec *pSrc, nsIFileSpec *pDst, SimpleBufferTonyRCopiedTwice& copy);

	static nsresult	FillMailBuffer( ReadFileState *pState, SimpleBufferTonyRCopiedTwice& read);

private:
	nsresult	CreateComponents( void);
	nsresult	CreateIdentity( void);
	
	void		GetNthHeader( const char *pData, PRInt32 dataLen, PRInt32 n, nsCString& header, nsCString& val, PRBool unwrap);
	void		GetHeaderValue( const char *pData, PRInt32 dataLen, const char *pHeader, nsCString& val, PRBool unwrap = PR_TRUE);
	void		GetHeaderValue( const char *pData, PRInt32 dataLen, const char *pHeader, nsString& val) {
		val.Truncate();
		nsCString	hVal;
		GetHeaderValue( pData, dataLen, pHeader, hVal, PR_TRUE);
		val.AssignWithConversion(hVal.get());
	}
	void		ExtractCharset( nsString& str);
	void		ExtractType( nsString& str);

	nsMsgAttachedFile * GetLocalAttachments( void);
	void				CleanUpAttach( nsMsgAttachedFile *a, PRInt32 count);

	nsresult	ReadHeaders( ReadFileState *pState, SimpleBufferTonyRCopiedTwice& copy, SimpleBufferTonyRCopiedTwice& header);
	PRInt32		FindNextEndLine( SimpleBufferTonyRCopiedTwice& data);
	PRInt32		IsEndHeaders( SimpleBufferTonyRCopiedTwice& data);
	PRInt32		IsSpecialHeader( const char *pHeader);
	nsresult	WriteHeaders( nsIFileSpec *pDst, SimpleBufferTonyRCopiedTwice& newHeaders);
	PRBool		IsReplaceHeader( const char *pHeader);


private:
	nsVoidArray *			m_pAttachments;
	nsIMsgSendListener *	m_pListener;
	nsIMsgSend *			m_pMsgSend;
	nsIMsgSend *			m_pSendProxy;
	nsIMsgCompFields *		m_pMsgFields;
	nsIMsgIdentity *		m_pIdentity;
	nsIIOService *			m_pIOService;
	PRInt32					m_headerLen;
	const char *			m_pHeaders;
	PRInt32					m_bodyLen;
	const char *			m_pBody;
	SimpleBufferTonyRCopiedTwice			m_readHeaders;
};


#endif /* nsOutlookCompose_h__ */
