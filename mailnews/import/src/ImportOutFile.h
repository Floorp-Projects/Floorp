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
#ifndef ImportOutFile_h___
#define ImportOutFile_h___

#include "nsImportTranslator.h"
#include "nsIFileSpec.h"

#define kMaxMarkers		10

class ImportOutFile;

class ImportOutFile {
public:
	ImportOutFile();
	ImportOutFile( nsIFileSpec *pFile, PRUint8 * pBuf, PRUint32 sz);
	~ImportOutFile();

	PRBool	InitOutFile( nsIFileSpec *pFile, PRUint32 bufSz = 4096);
	void	InitOutFile( nsIFileSpec *pFile, PRUint8 * pBuf, PRUint32 sz);
	inline PRBool	WriteData( const PRUint8 * pSrc, PRUint32 len);
	inline PRBool	WriteByte( PRUint8 byte);
	PRBool	WriteStr( const char *pStr) {return( WriteU8NullTerm( (const PRUint8 *) pStr, PR_FALSE)); }
	PRBool	WriteU8NullTerm( const PRUint8 * pSrc, PRBool includeNull);
	PRBool	WriteEol( void) { return( WriteStr( "\x0D\x0A")); }
	PRBool	Done( void) {return( Flush());}

	// Marker support
	PRBool	SetMarker( int markerID);
	void	ClearMarker( int markerID);
	PRBool	WriteStrAtMarker( int markerID, const char *pStr);

	// 8-bit to 7-bit translation
	PRBool	Set8bitTranslator( nsImportTranslator *pTrans);
	PRBool	End8bitTranslation( PRBool *pEngaged, nsCString& useCharset, nsCString& encoding);

protected:
	PRBool	Flush( void);

protected:
	nsIFileSpec *	m_pFile;
	PRUint8 *		m_pBuf;
	PRUint32		m_bufSz;
	PRUint32		m_pos;
	PRBool			m_ownsFileAndBuffer;

	// markers
	PRUint32		m_markers[kMaxMarkers];

	// 8 bit to 7 bit translations
	nsImportTranslator	*	m_pTrans;
	PRBool					m_engaged;
	PRBool					m_supports8to7;
	ImportOutFile *			m_pTransOut;
	PRUint8 *				m_pTransBuf;
};

inline PRBool	ImportOutFile::WriteData( const PRUint8 * pSrc, PRUint32 len) {
	while ((len + m_pos) > m_bufSz) {
		if ((m_bufSz - m_pos)) {
			nsCRT::memcpy( m_pBuf + m_pos, pSrc, m_bufSz - m_pos);
			len -= (m_bufSz - m_pos);
			pSrc += (m_bufSz - m_pos);
			m_pos = m_bufSz;
		}
		if (!Flush())
			return( PR_FALSE);
	}
	
	if (len) {
		nsCRT::memcpy( m_pBuf + m_pos, pSrc, len);
		m_pos += len;
	}

	return( PR_TRUE);
}

inline PRBool	ImportOutFile::WriteByte( PRUint8 byte) {
	if (m_pos == m_bufSz) {
		if (!Flush())
			return( PR_FALSE);
	}
	*(m_pBuf + m_pos) = byte;
	m_pos++;
	return( PR_TRUE);
}

#endif /* ImportOutFile_h__ */


