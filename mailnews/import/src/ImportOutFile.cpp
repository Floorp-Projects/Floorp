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

#include "nscore.h"
#include "nsString.h"
#include "nsCRT.h"
#include "ImportOutFile.h"
#include "ImportCharSet.h"

#include "ImportDebug.h"

/*
#ifdef _MAC
#define	kMacNoCreator		'????'
#define kMacTextFile		'TEXT'
#else
#define	kMacNoCreator		0
#define kMacTextFile		0
#endif
*/

ImportOutFile::ImportOutFile()
{
	m_ownsFileAndBuffer = PR_FALSE;
	m_pos = 0;
	m_pBuf = nsnull;
	m_bufSz = 0;
	m_pFile = nsnull;
	m_pTrans = nsnull;
	m_pTransOut = nsnull;
	m_pTransBuf = nsnull;
}

ImportOutFile::ImportOutFile( nsIFileSpec *pSpec, PRUint8 * pBuf, PRUint32 sz)
{
	m_pTransBuf = nsnull;
	m_pTransOut = nsnull;
	m_pTrans = nsnull;
	m_ownsFileAndBuffer = PR_FALSE;
	InitOutFile( pSpec, pBuf, sz);
}

ImportOutFile::~ImportOutFile()
{
	if (m_ownsFileAndBuffer) {
		Flush();
		if (m_pBuf)
			delete [] m_pBuf;
	}
	
	NS_IF_RELEASE( m_pFile);

	if (m_pTrans)
		delete m_pTrans;
	if (m_pTransOut)
		delete m_pTransOut;
	if (m_pTransBuf)
		delete m_pTransBuf;
}

PRBool ImportOutFile::Set8bitTranslator( nsImportTranslator *pTrans)
{
	if (!Flush())
		return( PR_FALSE);

	m_engaged = PR_FALSE;
	m_pTrans = pTrans;
	m_supports8to7 = pTrans->Supports8bitEncoding();


	return( PR_TRUE);
}

PRBool ImportOutFile::End8bitTranslation( PRBool *pEngaged, nsCString& useCharset, nsCString& encoding)
{
	if (!m_pTrans)
		return( PR_FALSE);


	PRBool bResult = Flush();
	if (m_supports8to7 && m_pTransOut) {
		if (bResult)
			bResult = m_pTrans->FinishConvertToFile( m_pTransOut);
		if (bResult)
			bResult = Flush();
	}

	if (m_supports8to7) {
		m_pTrans->GetCharset( useCharset);
		m_pTrans->GetEncoding( encoding);
	}
	else
		useCharset.Truncate();
	*pEngaged = m_engaged;
	delete m_pTrans;
	m_pTrans = nsnull;
	if (m_pTransOut)
		delete m_pTransOut;
	m_pTransOut = nsnull;
	if (m_pTransBuf)
		delete m_pTransBuf;
	m_pTransBuf = nsnull;

	return( bResult);
}

PRBool ImportOutFile::InitOutFile( nsIFileSpec *pSpec, PRUint32 bufSz)
{
	if (!bufSz)
		bufSz = 32 * 1024;	
	if (!m_pBuf) {
		m_pBuf = new PRUint8[ bufSz];
	}
	
	// m_fH = UFile::CreateFile( oFile, kMacNoCreator, kMacTextFile);
	PRBool	open = PR_FALSE;
	nsresult rv = pSpec->IsStreamOpen( &open);
	if (NS_FAILED( rv) || !open) {
		rv = pSpec->OpenStreamForWriting();
		if (NS_FAILED( rv)) {
			IMPORT_LOG0( "Couldn't create outfile\n");
			delete [] m_pBuf;
			m_pBuf = nsnull;
			return( PR_FALSE);
		}
	}
	m_pFile = pSpec;
	NS_ADDREF( m_pFile);
	m_ownsFileAndBuffer = PR_TRUE;
	m_pos = 0;
	m_bufSz = bufSz;

	return( PR_TRUE);
}

void ImportOutFile::InitOutFile( nsIFileSpec *pSpec, PRUint8 * pBuf, PRUint32 sz)
{
	m_ownsFileAndBuffer = PR_FALSE;
	m_pFile = pSpec;
	NS_IF_ADDREF( m_pFile);
	m_pBuf = pBuf;
	m_bufSz = sz;
	m_pos = 0;
}



PRBool ImportOutFile::Flush( void)
{
	if (!m_pos)
		return( PR_TRUE);

	PRUint32	transLen;
	PRBool		duddleyDoWrite = PR_FALSE;

	// handle translations if appropriate
	if (m_pTrans) {
		if (m_engaged && m_supports8to7) {
			// Markers can get confused by this crap!!!
			// TLR: FIXME: Need to update the markers based on
			// the difference between the translated len and untranslated len

			if (!m_pTrans->ConvertToFile(  m_pBuf, m_pos, m_pTransOut, &transLen))
				return( PR_FALSE);
			if (!m_pTransOut->Flush())
				return( PR_FALSE);
			// now update our buffer...
			if (transLen < m_pos) {
				nsCRT::memcpy( m_pBuf, m_pBuf + transLen, m_pos - transLen);
			}
			m_pos -= transLen;
		}
		else if (m_engaged) {
			// does not actually support translation!
			duddleyDoWrite = PR_TRUE;
		}
		else {
			// should we engage?
			PRUint8 *	pChar = m_pBuf;
			PRUint32	len = m_pos;
			while (len) {
				if (!ImportCharSet::IsUSAscii( *pChar))
					break;
				pChar++;
				len--;
			}
			if (len) {
				m_engaged = PR_TRUE;
				if (m_supports8to7) {
					// allocate our translation output buffer and file...
					m_pTransBuf = new PRUint8[m_bufSz];
					m_pTransOut = new ImportOutFile( m_pFile, m_pTransBuf, m_bufSz);
					return( Flush());
				}
				else
					duddleyDoWrite = PR_TRUE;
			}
			else {
				duddleyDoWrite = PR_TRUE;
			}
		}
	}
	else
		duddleyDoWrite = PR_TRUE;

	if (duddleyDoWrite) {
		PRInt32 written = 0;
		nsresult rv = m_pFile->Write( (const char *)m_pBuf, (PRInt32)m_pos, &written);
		if (NS_FAILED( rv) || ((PRUint32)written != m_pos))
			return( PR_FALSE);
		m_pos = 0;
	}
	
	return( PR_TRUE);
}

PRBool ImportOutFile::WriteU8NullTerm( const PRUint8 * pSrc, PRBool includeNull) 
{
	while (*pSrc) {
		if (m_pos >= m_bufSz) {
			if (!Flush())
				return( PR_FALSE);
		}
		*(m_pBuf + m_pos) = *pSrc;
		m_pos++;
		pSrc++;
	}
	if (includeNull) {
		if (m_pos >= m_bufSz) {
			if (!Flush())
				return( PR_FALSE);
		}
		*(m_pBuf + m_pos) = 0;
		m_pos++;
	}

	return( PR_TRUE);
}

PRBool ImportOutFile::SetMarker( int markerID)
{
	if (!Flush()) {
		return( PR_FALSE);
	}

	if (markerID < kMaxMarkers) {
		PRInt32 pos = 0;
		nsresult rv;
		if (m_pFile) {
			rv = m_pFile->Tell( &pos);
			if (NS_FAILED( rv)) {
				IMPORT_LOG0( "*** Error, Tell failed on output stream\n");
				return( PR_FALSE);
			}
		}
		m_markers[markerID] = (PRUint32)pos + m_pos;
	}

	return( PR_TRUE);
}

void ImportOutFile::ClearMarker( int markerID)
{
	if (markerID < kMaxMarkers)
		m_markers[markerID] = 0;
}

PRBool ImportOutFile::WriteStrAtMarker( int markerID, const char *pStr)
{
	if (markerID >= kMaxMarkers)
		return( PR_FALSE);

	if (!Flush())
		return( PR_FALSE);
	nsresult	rv;
	PRInt32		pos;
	rv = m_pFile->Tell( &pos);
	if (NS_FAILED( rv))
		return( PR_FALSE);
	rv = m_pFile->Seek( (PRInt32) m_markers[markerID]);
	if (NS_FAILED( rv))
		return( PR_FALSE);
	PRInt32 written;
	rv = m_pFile->Write( pStr, nsCRT::strlen( pStr), &written);
	if (NS_FAILED( rv))
		return( PR_FALSE);

	rv = m_pFile->Seek( pos);
	if (NS_FAILED( rv))
		return( PR_FALSE);

	return( PR_TRUE);
}

