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
#include "nsIFileSpec.h"
#include "nsCRT.h"
#include "nsImportScanFile.h"
#include "ImportCharSet.h"

nsImportScanFile::nsImportScanFile()
{
	m_allocated = PR_FALSE;
	m_eof = PR_FALSE;
	m_pFile = nsnull;
	m_pBuf = nsnull;
}

nsImportScanFile::nsImportScanFile( nsIFileSpec *pSpec, PRUint8 * pBuf, PRUint32 sz)
{
	m_allocated = PR_FALSE;
	m_eof = PR_FALSE;
	InitScan( pSpec, pBuf, sz);
}

nsImportScanFile::~nsImportScanFile()
{
	if (m_allocated) {
		CleanUpScan();
	}
	
	NS_IF_RELEASE( m_pFile);
}

PRBool nsImportScanFile::OpenScan( nsIFileSpec *pSpec, PRUint32 bufSz)
{
	if (!bufSz)
		bufSz = 32 * 1024;
	if (!m_pBuf) {
		m_pBuf = new PRUint8[bufSz];
	}
	
	PRBool	open = PR_FALSE;
	nsresult rv = pSpec->IsStreamOpen( &open);
	if (NS_FAILED( rv) || !open) {
		rv = pSpec->OpenStreamForReading();
		if (NS_FAILED( rv)) {
			delete [] m_pBuf;
			m_pBuf = nsnull;
			return( PR_FALSE);
		}
	}
	m_pFile = pSpec;
	NS_IF_ADDREF( m_pFile);
	m_allocated = PR_TRUE;
	m_bytesInBuf = 0;
	m_pos = 0;
	m_bufSz = bufSz;
	return( PR_TRUE);
}


void nsImportScanFile::InitScan( nsIFileSpec *pSpec, PRUint8 * pBuf, PRUint32 sz)
{
	m_pFile = pSpec;
	NS_IF_ADDREF( pSpec);
	m_pBuf = pBuf;
	m_bufSz = sz;
	m_bytesInBuf = 0;
	m_pos = 0;
}

void nsImportScanFile::CleanUpScan( void)
{
	NS_IF_RELEASE( m_pFile);
	m_pFile = nsnull;
	if (m_allocated) {
		if (m_pBuf)
			delete [] m_pBuf;
		m_pBuf = NULL;
	}
}

void nsImportScanFile::ShiftBuffer( void)
{
	PRUint8 *	pTop;
	PRUint8 *	pCurrent;

	if (m_pos < m_bytesInBuf) {
		pTop = m_pBuf;
		pCurrent = pTop + m_pos;
		PRUint32		cnt = m_bytesInBuf - m_pos;
		while (cnt) {
			*pTop = *pCurrent;
			pTop++; pCurrent++;
			cnt--;
		}
	}

	m_bytesInBuf -= m_pos;
	m_pos = 0;
}

PRBool nsImportScanFile::FillBufferFromFile( void)
{
	PRBool eof = PR_FALSE;
	nsresult rv = m_pFile->Eof( &eof);
	if (eof) {
		return( PR_FALSE);
	}

	// Fill up a buffer and scan it
	ShiftBuffer();

	// Read in some more bytes
	PRUint32	cnt = m_bufSz - m_bytesInBuf;
	// To distinguish from disk errors
	// Check first for end of file?
	// Set a done flag if true...
	PRInt32 read;
	char *pBuf = (char *)m_pBuf;
	pBuf += m_bytesInBuf;
	rv = m_pFile->Read( &pBuf, (PRInt32) cnt, &read);

	if (NS_FAILED( rv))
		return( PR_FALSE);
	eof = PR_FALSE;
	rv = m_pFile->Eof( &eof);
	if (eof)
		m_eof = PR_TRUE;

	m_bytesInBuf += cnt;
	return( PR_TRUE);
}

PRBool nsImportScanFile::Scan( PRBool *pDone)
{
	PRBool eof = PR_FALSE;
	m_pFile->Eof( &eof);

	if (eof) {
		if (m_pos < m_bytesInBuf) {
			ScanBuffer( pDone);
		}
		*pDone = PR_TRUE;
		return( PR_TRUE);
	}

	// Fill up a buffer and scan it
	if (!FillBufferFromFile())
		return( PR_FALSE);

	return( ScanBuffer( pDone));
}

PRBool nsImportScanFile::ScanBuffer( PRBool *)
{
	return( PR_TRUE);
}


PRBool nsImportScanFileLines::ScanBuffer( PRBool *pDone)
{
	// m_pos, m_bytesInBuf, m_eof, m_pBuf are relevant

	PRUint32		pos = m_pos;
	PRUint32		max = m_bytesInBuf;
	PRUint8 *		pChar = m_pBuf + pos;
	PRUint32		startPos;
	
	while (pos < max) {
		if (m_needEol) {
			// Find the next eol...
			while ((pos < max) && (*pChar != ImportCharSet::cCRChar) && (*pChar != ImportCharSet::cLinefeedChar)) {
				pos++;
				pChar++;
			}
			m_pos = pos;
			if (pos < max)
				m_needEol = PR_FALSE;
			if (pos == max) // need more buffer for an end of line
				break;
		}
		// Skip past any eol characters
		while ((pos < max) && ((*pChar == ImportCharSet::cCRChar) || (*pChar == ImportCharSet::cLinefeedChar))) {
			pos++;
			pChar++;
		}
		m_pos = pos;
		if (pos == max)
			break;
		// Make sure we can find either the eof or the 
		// next end of line
		startPos = pos;
		while ((pos < max) && (*pChar != ImportCharSet::cCRChar) && (*pChar != ImportCharSet::cLinefeedChar)) {
			pos++;
			pChar++;
		}
		
		// Is line too big for our buffer?
		if ((pos == max) && !m_eof) {
			if (!m_pos) { // line too big for our buffer
				m_pos = pos;
				m_needEol = PR_TRUE;
			}
			break;
		}
		
		if (!ProcessLine( m_pBuf + startPos, pos - startPos, pDone)) {
			return( PR_FALSE);
		}
		m_pos = pos;
	}

	return( PR_TRUE);
}

