/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsOEMailbox.h"

#include "OEDebugLog.h"


class CMbxScanner {
public:
	CMbxScanner( nsString& name, nsIFileSpec * mbxFile, nsIFileSpec * dstFile);
	~CMbxScanner();

	virtual PRBool	Initialize( void);
	virtual	PRBool	DoWork( PRBool *pAbort, PRUint32 *pDone, PRUint32 *pCount);

	PRBool		WasErrorFatal( void) { return( m_fatalError);}
	PRUint32	BytesProcessed( void) { return( m_didBytes);}

protected:
	PRBool	WriteMailItem( PRUint32 flags, PRUint32 offset, PRUint32 size, PRUint32 *pTotalMsgSize = nsnull);
	virtual	void	CleanUp( void);
	
private:
	void	ReportWriteError( nsIFileSpec * file, PRBool fatal = PR_TRUE);
	void	ReportReadError( nsIFileSpec * file, PRBool fatal = PR_TRUE);
	PRBool	CopyMbxFileBytes( PRUint32 numBytes);
	PRBool	IsFromLineKey( PRUint8 *pBuf, PRUint32 max);

public:
	PRUint32			m_msgCount;

protected:
	PRUint32 *			m_pDone;
	nsString			m_name;
	nsIFileSpec *		m_mbxFile;
	nsIFileSpec *		m_dstFile;
	PRUint8 *			m_pInBuffer;
	PRUint8 *			m_pOutBuffer;
	PRUint32			m_bufSz;
	PRUint32			m_didBytes;
	PRBool				m_fatalError;
	PRUint32			m_mbxFileSize;
	PRUint32			m_mbxOffset;
	
static const char *	m_pFromLine;

};


class CIndexScanner : public CMbxScanner {
public:
	CIndexScanner( nsString& name, nsIFileSpec * idxFile, nsIFileSpec * mbxFile, nsIFileSpec *dstFile);
	~CIndexScanner();

	virtual PRBool	Initialize( void);
	virtual	PRBool	DoWork( PRBool *pAbort, PRUint32 *pDone, PRUint32 *pCount);

protected:
	virtual	void	CleanUp( void);
	
private:
	PRBool	ValidateIdxFile( void);
	PRBool	GetMailItem( PRUint32 *pFlags, PRUint32 *pOffset, PRUint32 *pSize);
	

private:
	nsIFileSpec *		m_idxFile;
	PRUint32			m_numMessages;
	PRUint32			m_idxOffset;
	PRUint32			m_curItemIndex;
};


PRBool CImportMailbox::ImportMailbox( PRUint32 *pDone, PRBool *pAbort, nsString& name, nsIFileSpec * inFile, nsIFileSpec * outFile, PRUint32 *pCount)
{
	PRBool		done = PR_FALSE;
	nsIFileSpec *idxFile;
	if (NS_FAILED( NS_NewFileSpec( &idxFile))) {
		IMPORT_LOG0( "New file spec failed!\n");
		return( PR_FALSE);
	}
	
	idxFile->FromFileSpec( inFile);
	if (GetIndexFile( idxFile)) {
		
		IMPORT_LOG1( "Using index file for: %S\n", name.get());
				
		CIndexScanner *pIdxScanner = new CIndexScanner( name, idxFile, inFile, outFile);
		if (pIdxScanner->Initialize()) {
			if (pIdxScanner->DoWork( pAbort, pDone, pCount)) {
				done = PR_TRUE;
			}
			else {
				IMPORT_LOG0( "CIndexScanner::DoWork() failed\n");
			}
		}
		else {
			IMPORT_LOG0( "CIndexScanner::Initialize() failed\n");
		}
		
		delete pIdxScanner;
	}
	
	idxFile->Release();
	
	if (done)
		return( done);

	/* 
		something went wrong with the index file, just scan the mailbox
		file itself.
	*/
	CMbxScanner *pMbx = new CMbxScanner( name, inFile, outFile);
	if (pMbx->Initialize()) {
		if (pMbx->DoWork( pAbort, pDone, pCount)) {
			done = PR_TRUE;
		}
		else {
			IMPORT_LOG0( "CMbxScanner::DoWork() failed\n");
		}
	}
	else {
		IMPORT_LOG0( "CMbxScanner::Initialize() failed\n");
	}
	
	delete pMbx;
	return( done);		
}


PRBool CImportMailbox::GetIndexFile( nsIFileSpec* file)
{
	char *pLeaf = nsnull;
	if (NS_FAILED( file->GetLeafName( &pLeaf)))
		return( PR_FALSE);
	PRInt32	len = nsCRT::strlen( pLeaf);
	if (len < 5) {
		nsCRT::free( pLeaf);
		return( PR_FALSE);
	}
	pLeaf[len - 1] = 'x';
	pLeaf[len - 2] = 'd';
	pLeaf[len - 3] = 'i';
	
	IMPORT_LOG1( "Looking for index leaf name: %s\n", pLeaf);
	
	nsresult	rv;
	rv = file->SetLeafName( pLeaf);
	nsCRT::free( pLeaf);
	
	PRBool	isFile = PR_FALSE;
	PRBool	exists = PR_FALSE;
	if (NS_SUCCEEDED( rv)) rv = file->IsFile( &isFile);
	if (NS_SUCCEEDED( rv)) rv = file->Exists( &exists);
		
	if (isFile && exists)
		return( PR_TRUE);
	else
		return( PR_FALSE);
}


const char *CMbxScanner::m_pFromLine = "From - Jan 1965 00:00:00\x0D\x0A";
// let's try a 16K buffer and see how well that works?
#define	kBufferKB	16


CMbxScanner::CMbxScanner( nsString& name, nsIFileSpec* mbxFile, nsIFileSpec* dstFile)
{
	m_msgCount = 0;
	m_name = name;
	m_mbxFile = mbxFile;
	m_mbxFile->AddRef();
	m_dstFile = dstFile;
	m_dstFile->AddRef();
	m_pInBuffer = nsnull;
	m_pOutBuffer = nsnull;
	m_bufSz = 0;
	m_fatalError = PR_FALSE;
	m_didBytes = 0;
	m_mbxFileSize = 0;
	m_mbxOffset = 0;
}

CMbxScanner::~CMbxScanner()
{
	CleanUp();
	if (m_mbxFile)
		m_mbxFile->Release();
	if (m_dstFile)
		m_dstFile->Release();	
}

void CMbxScanner::ReportWriteError( nsIFileSpec * file, PRBool fatal)
{
	m_fatalError = fatal;
}

void CMbxScanner::ReportReadError( nsIFileSpec * file, PRBool fatal)
{
	m_fatalError = fatal;
}

PRBool CMbxScanner::Initialize( void)
{
	m_bufSz = (kBufferKB * 1024);
	m_pInBuffer = new PRUint8[m_bufSz];
	m_pOutBuffer = new PRUint8[m_bufSz];
	if (!m_pInBuffer || !m_pOutBuffer) {
		return( PR_FALSE);
	}
	
	m_mbxFile->GetFileSize( &m_mbxFileSize);
	// open the mailbox file...
	if (NS_FAILED( m_mbxFile->OpenStreamForReading())) {
		CleanUp();
		return( PR_FALSE);
	}
	
	if (NS_FAILED( m_dstFile->OpenStreamForWriting())) {
		CleanUp();
		return( PR_FALSE);
	}
	
	return( PR_TRUE);
}


#define	kMbxHeaderSize		0x0054
#define kMbxMessageHeaderSz	16

PRBool CMbxScanner::DoWork( PRBool *pAbort, PRUint32 *pDone, PRUint32 *pCount)
{
	m_mbxOffset = kMbxHeaderSize;
	m_didBytes = kMbxHeaderSize;
	
	while (!(*pAbort) && ((m_mbxOffset + kMbxMessageHeaderSz) < m_mbxFileSize)) {
		PRUint32		msgSz;
		if (!WriteMailItem( 0, m_mbxOffset, 0, &msgSz)) {
			if (!WasErrorFatal())
				ReportReadError( m_mbxFile);
			return( PR_FALSE);
		}
		m_mbxOffset += msgSz;
		m_didBytes += msgSz;
		m_msgCount++;
		if (pDone)
			*pDone = m_didBytes;
		if (pCount)
			*pCount = m_msgCount;
	}
	
	CleanUp();

	return( PR_TRUE);
}


void CMbxScanner::CleanUp( void)
{
	m_mbxFile->CloseStream();
	m_dstFile->CloseStream();
	
	if (m_pInBuffer != nsnull) {
		delete [] m_pInBuffer;
		m_pInBuffer = nsnull;
	}
	
	if (m_pOutBuffer != nsnull) {
		delete [] m_pOutBuffer;
		m_pOutBuffer = nsnull;
	}
}


#define	kNumMbxLongsToRead	4

PRBool CMbxScanner::WriteMailItem( PRUint32 flags, PRUint32 offset, PRUint32 size, PRUint32 *pTotalMsgSize)
{
	PRUint32	values[kNumMbxLongsToRead];
	PRInt32		cnt = kNumMbxLongsToRead * sizeof( PRUint32);
	nsresult	rv;
	PRBool		failed = PR_FALSE;
	PRInt32		cntRead;
	PRInt8 *	pChar = (PRInt8 *) values;
	
	rv = m_mbxFile->Seek( offset);
	m_mbxFile->Failed( &failed);
	
	if (NS_FAILED( rv) || failed) {
		IMPORT_LOG1( "Mbx seek error: 0x%lx\n", offset);
		return( PR_FALSE);
	}
	rv = m_mbxFile->Read( (char **) &pChar, cnt, &cntRead);
	if (NS_FAILED( rv) || (cntRead != cnt)) {
		IMPORT_LOG1( "Mbx read error at: 0x%lx\n", offset);
		return( PR_FALSE);
	}
	if (values[0] != 0x7F007F00) {
		IMPORT_LOG2( "Mbx tag field doesn't match: 0x%lx, at offset: 0x%lx\n", values[0], offset);
		return( PR_FALSE);
	}
	if (size && (values[2] != size)) {
		IMPORT_LOG3( "Mbx size doesn't match idx, mbx: %ld, idx: %ld, at offset: 0x%lx\n", values[2], size, offset);
		return( PR_FALSE);
	}

	if (pTotalMsgSize != nsnull)
		*pTotalMsgSize = values[2];

	// everything looks kosher...
	// the actual message text follows and is values[3] bytes long...
	return( CopyMbxFileBytes( values[3]));
}

PRBool CMbxScanner::IsFromLineKey( PRUint8 * pBuf, PRUint32 max)
{
	if (max < 5)
		return( PR_FALSE);
	if ((pBuf[0] == 'F') && (pBuf[1] == 'r') && (pBuf[2] == 'o') && (pBuf[3] == 'm') && (pBuf[4] == ' '))
		return( PR_TRUE);
	return( PR_FALSE);
}


#define IS_ANY_SPACE( _ch) ((_ch == ' ') || (_ch == '\t') || (_ch == 10) || (_ch == 13))


PRBool CMbxScanner::CopyMbxFileBytes( PRUint32 numBytes)
{
	if (!numBytes)
		return( PR_TRUE);

	PRUint32		cnt;
	PRUint8			last[2] = {0, 0};
	PRUint32		inIdx = 0;
	PRBool			first = PR_TRUE;
	PRUint8 *		pIn;
	PRUint8 *		pStart;
	PRInt32			fromLen = nsCRT::strlen( m_pFromLine);
	nsresult		rv;
	PRInt32			cntRead;
	PRUint8 *		pChar;
	
	while (numBytes) {
		if (numBytes > (m_bufSz - inIdx))
			cnt = m_bufSz - inIdx;
		else
			cnt = numBytes;
		// Read some of the message from the file...
		pChar = m_pInBuffer + inIdx;
		rv = m_mbxFile->Read( (char **) &pChar, (PRInt32)cnt, &cntRead);
		if (NS_FAILED( rv) || (cntRead != (PRInt32)cnt)) {
			ReportReadError( m_mbxFile);
			return( PR_FALSE);
		}
		// Keep track of the last 2 bytes of the message for terminating EOL logic	
		if (cnt < 2) {
			last[0] = last[1];
			last[1] = m_pInBuffer[cnt - 1];
		}
		else {
			last[0] = m_pInBuffer[cnt - 2];
			last[1] = m_pInBuffer[cnt - 1];
		}
		
		inIdx = 0;
		// Handle the beginning line, don't duplicate an existing From seperator
		if (first) {
			// check the first buffer to see if it already starts with a From line
			// If it does, throw it away and use our own
			if (IsFromLineKey( m_pInBuffer, cnt)) {
				// skip past the first line
				while ((inIdx < cnt) && (m_pInBuffer[inIdx] != 0x0D))
					inIdx++;
				while ((inIdx < cnt) && (IS_ANY_SPACE( m_pInBuffer[inIdx])))
					inIdx++;
				if (inIdx >= cnt) {
					// This should not occurr - it means the message starts
					// with a From seperator line that is longer than our
					// file buffer!  In this bizarre case, just skip this message
					// since it is probably bogus anyway.
					return( PR_TRUE);
				}

			}
			// Begin every message with a From seperator
			rv = m_dstFile->Write( m_pFromLine, fromLen, &cntRead);
			if (NS_FAILED( rv) || (cntRead != fromLen)) {
				ReportWriteError( m_dstFile);
				return( PR_FALSE);
			}
			first = PR_FALSE;
		}

		// Handle generic data, escape any lines that begin with "From "
		pIn = m_pInBuffer + inIdx;
		numBytes -= cnt;
		m_didBytes += cnt;
		pStart = pIn;
		cnt -= inIdx;
		inIdx = 0;
		while (cnt) {
			if (*pIn == 0x0D) {
				// need more in buffer?
				if ((cnt < 7) && numBytes) {
					break;
				}
				else if (cnt > 6) {
					if ((pIn[1] == 0x0A) && (IsFromLineKey( pIn + 2, cnt))) {
						inIdx += 2;
						// Match, escape it
						rv = m_dstFile->Write( (const char *)pStart, (PRInt32)inIdx, &cntRead);
						if (NS_SUCCEEDED( rv) && (cntRead == (PRInt32)inIdx))
							rv = m_dstFile->Write( ">", 1, &cntRead);
						if (NS_FAILED( rv) || (cntRead != 1)) {
							ReportWriteError( m_dstFile);
							return( PR_FALSE);
						}
						cnt -= 2;
						pIn += 2;
						inIdx = 0;
						pStart = pIn;
						continue;
					}
				}
			} // == 0x0D
			cnt--;
			inIdx++;
			pIn++;
		}
		rv = m_dstFile->Write( (const char *)pStart, (PRInt32)inIdx, &cntRead);
		if (NS_FAILED( rv) || (cntRead != (PRInt32)inIdx)) {
			ReportWriteError( m_dstFile);
			return( PR_FALSE);
		}
		if (cnt) {
			inIdx = cnt;
			nsCRT::memcpy( m_pInBuffer, pIn, cnt);
		}
		else
			inIdx = 0;
	}

	// I used to check for an eol before writing one but
	// it turns out that adding a proper EOL before the next
	// seperator never really hurts so better to be safe
	// and always do it.
//	if ((last[0] != 0x0D) || (last[1] != 0x0A)) {
		rv = m_dstFile->Write( "\x0D\x0A", 2, &cntRead);
		if (NS_FAILED( rv) || (cntRead != 2)) {
			ReportWriteError( m_dstFile);
			return( PR_FALSE);
		}
//	}

	return( PR_TRUE);
}


CIndexScanner::CIndexScanner( nsString& name, nsIFileSpec * idxFile, nsIFileSpec * mbxFile, nsIFileSpec * dstFile)
	: CMbxScanner( name, mbxFile, dstFile)
{
	m_idxFile = idxFile;
	m_idxFile->AddRef();
	m_curItemIndex = 0;
	m_idxOffset = 0;
}

CIndexScanner::~CIndexScanner()
{
	CleanUp();
	if (m_idxFile)
		m_idxFile->Release();
}

PRBool CIndexScanner::Initialize( void)
{
	if (!CMbxScanner::Initialize())
		return( PR_FALSE);


	nsresult 	rv = m_idxFile->OpenStreamForReading();
	if (NS_FAILED( rv)) {
		CleanUp();
		return( PR_FALSE);
	}
	
	return( PR_TRUE);
}

PRBool CIndexScanner::ValidateIdxFile( void)
{
	PRInt8			id[4];
	PRInt32			cnt = 4;
	nsresult		rv;
	PRInt32			cntRead;
	PRInt8 *		pReadTo;
	
	pReadTo = id;
	rv = m_idxFile->Read( (char **) &pReadTo, cnt, &cntRead);
	if (NS_FAILED( rv) || (cntRead != cnt))
		return( PR_FALSE);
	if ((id[0] != 'J') || (id[1] != 'M') || (id[2] != 'F') || (id[3] != '9'))
		return( PR_FALSE);
	cnt = 4;
	PRUint32		subId;
	pReadTo = (PRInt8 *) &subId;
	rv = m_idxFile->Read( (char **) &pReadTo, cnt, &cntRead);
	if (NS_FAILED( rv) || (cntRead != cnt))
		return( PR_FALSE);
	if (subId != 0x00010004) {
		IMPORT_LOG1( "Idx file subid doesn't match: 0x%lx\n", subId);
		return( PR_FALSE);
	}
	
	pReadTo = (PRInt8 *) &m_numMessages;
	rv = m_idxFile->Read( (char **) &pReadTo, cnt, &cntRead);
	if (NS_FAILED( rv) || (cntRead != cnt))
		return( PR_FALSE);
	
	IMPORT_LOG1( "Idx file num messages: %ld\n", m_numMessages);

	m_didBytes += 80;
	m_idxOffset = 80;
	return( PR_TRUE);
}

/*
	Idx file...
	Header is 80 bytes, JMF9, subId? 0x00010004, numMessages, fileSize, 1, 0x00010010
	Entries start at byte 80
	4 byte numbers
		Flags? maybe
		?? who knows
		index
		start of this entry in the file
		length of this record
		msg offset in mbx
		msg length in mbx

*/

// #define DEBUG_SUBJECT_AND_FLAGS	1
#define	kNumIdxLongsToRead		7

PRBool CIndexScanner::GetMailItem( PRUint32 *pFlags, PRUint32 *pOffset, PRUint32 *pSize)
{
	PRUint32	values[kNumIdxLongsToRead];
	PRInt32		cnt = kNumIdxLongsToRead * sizeof( PRUint32);
	PRInt8 *	pReadTo = (PRInt8 *) values;
	PRInt32		cntRead;
	nsresult	rv;
	
	rv = m_idxFile->Seek( m_idxOffset);
	if (NS_FAILED( rv))
		return( PR_FALSE);
	
	rv = m_idxFile->Read( (char **) &pReadTo, cnt, &cntRead);		
	if (NS_FAILED( rv) || (cntRead != cnt))
		return( PR_FALSE);
		
	if (values[3] != m_idxOffset) {
		IMPORT_LOG2( "Self pointer invalid: m_idxOffset=0x%lx, self=0x%lx\n", m_idxOffset, values[3]);
		return( PR_FALSE);
	}

	// So... what do we have here???
#ifdef DEBUG_SUBJECT_AND_FLAGS
	IMPORT_LOG2( "Number: %ld, msg offset: 0x%lx, ", values[2], values[5]);
	IMPORT_LOG2( "msg length: %ld, Flags: 0x%lx\n", values[6], values[0]);
	m_idxFile->seek( m_idxOffset + 212);
	PRUint32	subSz = 0;
	cnt = 4;
	pReadTo = (PRInt8 *) &subSz;
	m_idxFile->Read( (char **) &pReadTo, cnt, &cntRead);
	if ((subSz >= 0) && (subSz < 1024)) {
		char *pSub = new char[subSz + 1];
		m_idxFile->Read( &pSub, subSz, &cntRead);
		pSub[subSz] = 0;
		IMPORT_LOG1( "    Subject: %s\n", pSub);
		delete [] pSub;
	}
#endif

	m_idxOffset += values[4];
	m_didBytes += values[4];

	*pFlags = values[0];
	*pOffset = values[5];
	*pSize = values[6];
	return( PR_TRUE);
}

#define	kOEDeletedFlag		0x0001

PRBool CIndexScanner::DoWork( PRBool *pAbort, PRUint32 *pDone, PRUint32 *pCount)
{
	m_didBytes = 0;
	if (!ValidateIdxFile())
		return( PR_FALSE);

	PRBool	failed = PR_FALSE;
	while ((m_curItemIndex < m_numMessages) && !failed && !(*pAbort)) {
		PRUint32	flags, offset, size;
		if (!GetMailItem( &flags, &offset, &size)) {
			CleanUp();
			return( PR_FALSE);
		}
		m_curItemIndex++;
		if (!(flags & kOEDeletedFlag)) {
			if (!WriteMailItem( flags, offset, size))
				failed = PR_TRUE;
			else {
				m_msgCount++;
			}
		}
		m_didBytes += size;
		if (pDone)
			*pDone = m_didBytes;
		if (pCount)
			*pCount = m_msgCount;
	}
	
	CleanUp();
	return( !failed);
}


void CIndexScanner::CleanUp( void)
{
	CMbxScanner::CleanUp();
	m_idxFile->CloseStream();	
}

