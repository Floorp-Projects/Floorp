/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef MapiMessage_h___
#define MapiMessage_h___

#include "nsUInt32Array.h"
#include "nsString.h"
#include "nsIFileSpec.h"
#include "MapiApi.h"


class nsSimpleUInt32Array {
public:
	nsSimpleUInt32Array( int growBy = 20) 
		{ m_growBy = growBy; m_pData = nsnull; m_used = 0; m_allocated = 0;}
	~nsSimpleUInt32Array() { if (m_pData) delete [] m_pData;}

	void Add( PRUint32 data) { Allocate(); if (m_used < m_allocated) { m_pData[m_used] = data; m_used++;}}
	PRUint32 GetAt( PRInt32 index) { if ((index >= 0) && (index < m_used)) return( m_pData[index]); else return( 0);}
	PRInt32	GetSize( void) { return( m_used);}
	void RemoveAll( void) { m_used = 0;}

private:
	void Allocate( void);
	
	PRInt32		m_allocated;
	PRInt32		m_used;
	PRUint32 *	m_pData;
	int			m_growBy;
};



class CMapiMessage {
public:
	CMapiMessage( LPMESSAGE	lpMsg);
	~CMapiMessage();

	// Headers - fetch will get PR_TRANSPORT_MESSAGE_HEADERS
	// or if they do not exist will build a header from
	//	PR_DISPLAY_TO, _CC, _BCC
	//	PR_SUBJECT
	//  PR_MESSAGE_RECIPIENTS
	// and PR_CREATION_TIME if needed?
	BOOL		FetchHeaders( void);
	
	// Do the headers need a From separator line.
	// TRUE if a From line needs to precede the headers, FALSE
	// if the headers already include a from line	
	BOOL		NeedsFromLine( void);
	
	// Fetch the 
	BOOL		FetchBody( void);

	// Attachments
	int			CountAttachments( void);
	BOOL		GetAttachmentInfo( int idx);

	// Retrieve info for message
	BOOL		BodyIsHtml( void) { return( m_bodyIsHtml);}
	const char *GetFromLine( int& len) { if (m_fromLine.IsEmpty()) return( NULL); else { len = m_fromLine.Length(); return( (const char *)m_fromLine);}}
	const char *GetHeaders( int& len) { if (m_headers.IsEmpty()) return( NULL); else { len = m_headers.Length(); return( (const char *)m_headers);}}
	const char *GetBody( int& len) { if (m_body.IsEmpty()) return( NULL); else { len = m_body.Length(); return( (const char *)m_body);}}
	const char *GetBody( void) { return( (const char *)m_body);}
	const char *GetHeaders( void) { return( (const char *)m_headers);}
	PRInt32		GetBodyLen( void) { return( m_body.Length());}
	PRInt32		GetHeaderLen( void) { return( m_headers.Length());}


	BOOL		IsMultipart( void);
	BOOL		HasContentHeader( void) { return( !m_mimeContentType.IsEmpty());}
	BOOL		HasMimeVersion( void) { return( m_bMimeVersion);}
	const char *GetMimeContent( void) { return( (const char *)m_mimeContentType);}
	const char *GetMimeBoundary( void) { return( (const char *)m_mimeBoundary);}
	void		GenerateBoundary( void);

	BOOL		GetAttachFileLoc( nsIFileSpec *pLoc);

	const char *GetMimeType( void) { return( (const char *) m_attachMimeType);}
	const char *GetFileName( void) { return( (const char *) m_attachFileName);}

protected:
	BOOL		IterateAttachTable( void);
	void		ClearTempAttachFile( void);
	BOOL		CopyBinAttachToFile( LPATTACH lpAttach);

	void		ProcessHeaderLine( nsCString& line);
	void		ProcessHeaders( void);
	void		FormatDateTime( SYSTEMTIME & tm, nsCString& s, BOOL includeTZ = TRUE);
	void		BuildHeaders( void);
	void		BuildFromLine( void);
	void		AddSubject( nsCString& str);
	void		AddFrom( nsCString& str);
	BOOL		AddHeader( nsCString& str, ULONG tag, const char *pPrefix);
	void		AddDate( nsCString& str);
	
	BOOL		IsSpace( PRUnichar c) { return( m_whitespace.FindChar( c) != -1);}

private:
	LPMESSAGE		m_lpMsg;
	LPMAPITABLE		m_pAttachTable;
	nsCString		m_headers;
	nsCString		m_fromLine;
	nsCString		m_body;
	nsCString		m_mimeContentType;
	nsCString		m_mimeBoundary;
	nsCString		m_mimeCharset;
	BOOL			m_bMimeVersion;
	BOOL			m_bMimeEncoding;
	nsSimpleUInt32Array	m_attachNums;
	nsCString		m_attachMimeType;
	nsCString		m_attachPath;
	nsCString		m_attachFileName;
	BOOL			m_ownsAttachFile;
	BOOL			m_bodyIsHtml;
	BOOL			m_bHasSubject;
	BOOL			m_bHasFrom;
	BOOL			m_bHasDate;
	nsCString		m_whitespace;
};




#endif /* MapiMessage_h__ */
