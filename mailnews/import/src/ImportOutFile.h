/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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


