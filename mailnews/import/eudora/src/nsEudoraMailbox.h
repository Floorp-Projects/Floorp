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
#include "nsEudoraCompose.h"


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
	nsresult ImportMailbox( PRUint32 *pBytes, PRBool *pAbort, const PRUnichar *pName, nsIFileSpec *pSrc, nsIFileSpec *pDst, PRInt32 *pMsgCount);

	static PRInt32		IsEudoraFromSeparator( const char *pData, PRInt32 maxLen, nsCString& defaultDate);

protected:
	nsresult	CreateTempFile( nsIFileSpec **ppSpec);
	nsresult	DeleteFile( nsIFileSpec *pSpec);


private:
	nsresult	CompactMailbox( PRUint32 *pBytes, PRBool *pAbort, nsIFileSpec *pMail, nsIFileSpec *pToc, nsIFileSpec *pDst);
	nsresult	ReadNextMessage( ReadFileState *pState, SimpleBufferTonyRCopiedOnce& copy, SimpleBufferTonyRCopiedOnce& header, SimpleBufferTonyRCopiedOnce& body, nsCString& defaultDate);
	PRInt32		FindStartLine( SimpleBufferTonyRCopiedOnce& data);
	PRInt32		FindNextEndLine( SimpleBufferTonyRCopiedOnce& data);
	PRInt32		IsEndHeaders( SimpleBufferTonyRCopiedOnce& data);
	nsresult	WriteFromSep( nsIFileSpec *pDst);
	nsresult	FillMailBuffer( ReadFileState *pState, SimpleBufferTonyRCopiedOnce& read);
	
	void		EmptyAttachments( void);
	nsresult	ExamineAttachment( SimpleBufferTonyRCopiedOnce& data);
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

