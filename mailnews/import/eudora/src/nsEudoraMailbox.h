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

#ifndef nsEudoraMailbox_h__
#define nsEudoraMailbox_h__

#include "nscore.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIFileSpec.h"
#include "nsISupportsArray.h"

typedef struct {
	nsIFileSpec *	pAttachment;
	char *			mimeType;
	char *			description;
} EudoraAttachment;

typedef struct {
	PRUint32		offset;
	PRUint32		size;
	nsIFileSpec *	pFile;
} ReadFileState;

class SimpleBuffer {
public:
	SimpleBuffer() {m_pBuffer = nsnull; m_size = 0; m_growBy = 4096; m_writeOffset = 0;
					m_bytesInBuf = 0;}
	~SimpleBuffer() { if (m_pBuffer) delete [] m_pBuffer;}
	
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
	
	PRBool Write( PRInt32 offset, const char *pData, PRInt32 len) {
		if (!len) return( PR_TRUE);
		if (!Grow( offset + len)) return( PR_FALSE);
		nsCRT::memcpy( m_pBuffer + offset, pData, len);
		return( PR_TRUE);
	}
	
	PRBool Write( const char *pData, PRInt32 len) { 
		if (Write( m_writeOffset, pData, len)) { m_writeOffset += len; return( PR_TRUE);}
		else return( PR_FALSE);
	}

	
	char *	m_pBuffer;
	PRInt32	m_bytesInBuf;	// used when reading into this buffer
	PRInt32	m_size;			// allocated size of buffer
	PRInt32	m_growBy;		// duh
	PRInt32	m_writeOffset;	// used when writing into and reading from the buffer
};


/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

class nsEudoraMailbox {
public:
	nsEudoraMailbox();
	virtual ~nsEudoraMailbox();

	// Things that must be overridden because they are platform specific.
		// retrieve the mail folder
	virtual PRBool		FindMailFolder( nsIFileSpec *pFolder) { return( PR_FALSE);}
		// get the list of mailboxes
	virtual nsresult	FindMailboxes( nsIFileSpec *pRoot, nsISupportsArray **ppArray) { return( NS_ERROR_FAILURE);}
		// get the toc file corresponding to this mailbox
	virtual nsresult	FindTOCFile( nsIFileSpec *pMailFile, nsIFileSpec **pTOCFile, PRBool *pDeleteToc) { return( NS_ERROR_FAILURE);}
		// interpret the attachment line and return the attached file
	virtual nsresult	GetAttachmentInfo( const char *pFileName, nsIFileSpec *pSpec, nsCString& mimeType) { return( NS_ERROR_FAILURE);}
		
	// Non-platform specific common stuff
		// import a mailbox
	nsresult ImportMailbox( PRBool *pAbort, const PRUnichar *pName, nsIFileSpec *pSrc, nsIFileSpec *pDst, PRInt32 *pMsgCount);

	static PRInt32		IsEudoraFromSeparator( const char *pData, PRInt32 maxLen);

protected:
	nsresult	CreateTempFile( nsIFileSpec **ppSpec);
	nsresult	DeleteFile( nsIFileSpec *pSpec);


private:
	nsresult	CompactMailbox( PRBool *pAbort, nsIFileSpec *pMail, nsIFileSpec *pToc, nsIFileSpec *pDst);
	nsresult	ReadNextMessage( ReadFileState *pState, SimpleBuffer& copy, SimpleBuffer& header, SimpleBuffer& body);
	nsresult	FillMailBuffer( ReadFileState *pState, SimpleBuffer& read);
	PRInt32		FindStartLine( SimpleBuffer& data);
	PRInt32		FindNextEndLine( SimpleBuffer& data);
	PRInt32		IsEndHeaders( SimpleBuffer& data);
	nsresult	CopyComposedMessage( nsIFileSpec *pSrc, nsIFileSpec *pDst, SimpleBuffer& copy);
	nsresult	WriteFromSep( nsIFileSpec *pDst);
	
	void		EmptyAttachments( void);
	nsresult	ExamineAttachment( SimpleBuffer& data);
	PRBool		AddAttachment( nsCString& fileName);

	static PRInt32		AsciiToLong( const char *pChar, PRInt32 len);
	static int			IsWeekDayStr( const char *pStr);
	static int			IsMonthStr( const char *pStr);

private:
	PRUint32		m_mailSize;
	PRInt32			m_fromLen;
	nsVoidArray		m_attachments;
};



#endif /* nsEudoraMailbox_h__ */

