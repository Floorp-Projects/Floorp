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
	virtual nsresult	GetAttachmentInfo( const char *pFileName, nsIFileSpec *pSpec, nsCString& mimeType, nsCString& aAttachment) { return( NS_ERROR_FAILURE);}
		
	// Non-platform specific common stuff
		// import a mailbox
	nsresult ImportMailbox( PRUint32 *pBytes, PRBool *pAbort, const PRUnichar *pName, nsIFileSpec *pSrc, nsIFileSpec *pDst, PRInt32 *pMsgCount);

	static PRInt32		IsEudoraFromSeparator( const char *pData, PRInt32 maxLen, nsCString& defaultDate);
	static PRBool		IsEudoraTag( const char *pChar, PRInt32 maxLen, PRBool &insideEudoraTags, nsCString &bodyType, PRInt32& tagLength);

protected:
	nsresult	CreateTempFile( nsIFileSpec **ppSpec);
	nsresult	DeleteFile( nsIFileSpec *pSpec);


private:
	nsresult	CompactMailbox( PRUint32 *pBytes, PRBool *pAbort, nsIFileSpec *pMail, nsIFileSpec *pToc, nsIFileSpec *pDst);
	nsresult	ReadNextMessage( ReadFileState *pState, SimpleBufferTonyRCopiedOnce& copy, SimpleBufferTonyRCopiedOnce& header, SimpleBufferTonyRCopiedOnce& body, nsCString& defaultDate, nsCString &defBodyType);
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

