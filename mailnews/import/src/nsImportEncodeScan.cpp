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
#include "nsCRT.h"
#include "nsImportEncodeScan.h"

#define	kBeginAppleSingle		0
#define	kBeginDataFork			1
#define	kBeginResourceFork		2
#define	kAddEntries				3
#define	kScanningDataFork		4
#define	kScanningRsrcFork		5
#define	kDoneWithFile			6

PRUint32	gAppleSingleHeader[6] = {0x00051600, 0x00020000, 0, 0, 0, 0};
#define kAppleSingleHeaderSize	(6 * sizeof( PRUint32))

#ifdef _MAC_IMPORT_CODE
#include "MoreFilesExtras.h"
#include "MoreDesktopMgr.h"

CInfoPBRec	gCatInfoPB;
U32			g2000Secs = 0;
long		gGMTDelta = 0;

long GetGmtDelta( void);
U32 Get2000Secs( void);


long GetGmtDelta( void)
{
	MachineLocation myLocation;
	ReadLocation( &myLocation);
	long	myDelta = BitAnd( myLocation.u.gmtDelta, 0x00FFFFFF);
	if (BitTst( &myDelta, 23))
		myDelta = BitOr( myDelta, 0xFF000000);
	return( myDelta);
}

U32 Get2000Secs( void)
{
	DateTimeRec	dr;
	dr.year = 2000;
	dr.month = 1;
	dr.day = 1;
	dr.hour = 0;
	dr.minute = 0;
	dr.second = 0;
	dr.dayOfWeek = 0;
	U32	result;
	DateToSeconds( &dr, &result);
	return( result);
}
#endif

nsImportEncodeScan::nsImportEncodeScan()
{
	m_pFile = nsnull;
	m_isAppleSingle = PR_FALSE;
	m_encodeScanState = 0;
	m_resourceForkSize = 0;
	m_dataForkSize = 0;
	m_pInputFile = nsnull;
}

nsImportEncodeScan::~nsImportEncodeScan()
{
	NS_IF_RELEASE( m_pInputFile);
}

PRBool nsImportEncodeScan::InitEncodeScan( PRBool appleSingleEncode, nsIFileSpec *fileLoc, const char *pName, PRUint8 * pBuf, PRUint32 sz)
{
	CleanUpEncodeScan();
	m_isAppleSingle = appleSingleEncode;
	m_encodeScanState = kBeginAppleSingle;
	m_pInputFile = fileLoc;
	NS_IF_ADDREF( m_pInputFile);
	m_useFileName = pName;
	m_pBuf = pBuf;
	m_bufSz = sz;
	if (!m_isAppleSingle) {
		PRBool	open = PR_FALSE;
		nsresult rv = m_pInputFile->IsStreamOpen( &open);
		if (NS_FAILED( rv) || !open) {
			rv = m_pInputFile->OpenStreamForReading();
			if (NS_FAILED( rv))
				return( PR_FALSE);
		}

		InitScan( m_pInputFile, pBuf, sz);
	}
	else {
	#ifdef _MAC_IMPORT_CODE
		// Fill in the file sizes
		m_resourceForkSize = fileLoc.GetMacFileSize( UFileLocation::eResourceFork);
		m_dataForkSize = fileLoc.GetMacFileSize( UFileLocation::eDataFork);
	#endif
	}

	return( PR_TRUE);
}

void nsImportEncodeScan::CleanUpEncodeScan( void)
{
	NS_IF_RELEASE( m_pInputFile);
	m_pInputFile = nsnull;
}


// 26 + 12 per entry

void nsImportEncodeScan::FillInEntries( int numEntries)
{
#ifdef _MAC_IMPORT_CODE
	int		len = m_useFileName.GetLength();
	if (len < 32)
		len = 32;
	long	entry[3];
	long	fileOffset = 26 + (12 * numEntries);
	entry[0] = 3;
	entry[1] = fileOffset;
	entry[2] = m_useFileName.GetLength();
	fileOffset += len;
	MemCpy( m_pBuf + m_bytesInBuf, entry, 12);
	m_bytesInBuf += 12;		
	
	
	Str255	comment;
	comment[0] = 0;
	OSErr err = FSpDTGetComment( m_inputFileLoc, comment);
	if (comment[0] > 200)
		comment[0] = 200;
	entry[0] = 4;
	entry[1] = fileOffset;
	entry[2] = comment[0];
	fileOffset += 200;
	MemCpy( m_pBuf + m_bytesInBuf, entry, 12);
	m_bytesInBuf += 12;		
	
	
	entry[0] = 8;
	entry[1] = fileOffset;
	entry[2] = 16;
	fileOffset += 16;
	MemCpy( m_pBuf + m_bytesInBuf, entry, 12);
	m_bytesInBuf += 12;		
	
	entry[0] = 9;
	entry[1] = fileOffset;
	entry[2] = 32;
	fileOffset += 32;
	MemCpy( m_pBuf + m_bytesInBuf, entry, 12);
	m_bytesInBuf += 12;		

	
	entry[0] = 10;
	entry[1] = fileOffset;
	entry[2] = 4;
	fileOffset += 4;
	MemCpy( m_pBuf + m_bytesInBuf, entry, 12);
	m_bytesInBuf += 12;
	
	if (m_resourceForkSize) {
		entry[0] = 2;
		entry[1] = fileOffset;
		entry[2] = m_resourceForkSize;
		fileOffset += m_resourceForkSize;
		MemCpy( m_pBuf + m_bytesInBuf, entry, 12);
		m_bytesInBuf += 12;		
	}
	
	if (m_dataForkSize) {
		entry[0] = 1;
		entry[1] = fileOffset;
		entry[2] = m_dataForkSize;
		fileOffset += m_dataForkSize;
		MemCpy( m_pBuf + m_bytesInBuf, entry, 12);
		m_bytesInBuf += 12;
	}
	
#endif		
}

PRBool nsImportEncodeScan::AddEntries( void)
{
#ifdef _MAC_IMPORT_CODE
	if (!g2000Secs) {
		g2000Secs = Get2000Secs();
		gGMTDelta = GetGmtDelta();
	}
	MemCpy( m_pBuf + m_bytesInBuf, (PC_S8) m_useFileName, m_useFileName.GetLength());
	m_bytesInBuf += m_useFileName.GetLength();		
	if (m_useFileName.GetLength() < 32) {
		int len = m_useFileName.GetLength();
		while (len < 32) {
			*((P_S8)m_pBuf + m_bytesInBuf) = 0;
			m_bytesInBuf++;
			len++;
		}
	}
	
	Str255	comment;
	comment[0] = 0;
	OSErr err = FSpDTGetComment( m_inputFileLoc, comment);
	comment[0] = 200;
	MemCpy( m_pBuf + m_bytesInBuf, &(comment[1]), comment[0]);
	m_bytesInBuf += comment[0];
	
	long	dates[4];
	dates[0] = gCatInfoPB.hFileInfo.ioFlCrDat;
	dates[1] = gCatInfoPB.hFileInfo.ioFlMdDat;
	dates[2] = gCatInfoPB.hFileInfo.ioFlBkDat;
	dates[3] = 0x80000000;
	for (short i = 0; i < 3; i++) {
		dates[i] -= g2000Secs;
		dates[i] += gGMTDelta;
	}
	MemCpy( m_pBuf + m_bytesInBuf, dates, 16);
	m_bytesInBuf += 16;
	
	
	FInfo	fInfo = gCatInfoPB.hFileInfo.ioFlFndrInfo;
	FXInfo	fxInfo = gCatInfoPB.hFileInfo.ioFlXFndrInfo;
	fInfo.fdFlags = 0;
	fInfo.fdLocation.h = 0;
	fInfo.fdLocation.v = 0;
	fInfo.fdFldr = 0;
	MemSet( &fxInfo, 0, sizeof( fxInfo));
	MemCpy( m_pBuf + m_bytesInBuf, &fInfo, 16);
	m_bytesInBuf += 16;
	MemCpy( m_pBuf + m_bytesInBuf, &fxInfo, 16);
	m_bytesInBuf += 16;
	
	
	dates[0] = 0;
	if ((gCatInfoPB.hFileInfo.ioFlAttrib & 1) != 0)
		dates[0] |= 1;
	MemCpy( m_pBuf + m_bytesInBuf, dates, 4);
	m_bytesInBuf += 4;
	
	
#endif
	return( PR_TRUE);
}

PRBool nsImportEncodeScan::Scan( PRBool *pDone)
{
	nsresult	rv;

	*pDone = PR_FALSE;
	if (m_isAppleSingle) {
		// Stuff the buffer with things needed to encode the file...
		// then just allow UScanFile to handle each fork, but be careful
		// when handling eof.
		switch( m_encodeScanState) {
			case kBeginAppleSingle: {
				#ifdef _MAC_IMPORT_CODE
				OSErr err = GetCatInfoNoName( m_inputFileLoc.GetVRefNum(), m_inputFileLoc.GetParID(), m_inputFileLoc.GetFileNamePtr(), &gCatInfoPB);
				if (err != noErr)
					return( FALSE);
				#endif
				m_eof = PR_FALSE;
				m_pos = 0;
				nsCRT::memcpy( m_pBuf, gAppleSingleHeader, kAppleSingleHeaderSize);
				m_bytesInBuf = kAppleSingleHeaderSize;
				int numEntries = 5;
				if (m_dataForkSize)
					numEntries++;
				if (m_resourceForkSize)
					numEntries++;
				nsCRT::memcpy( m_pBuf + m_bytesInBuf, &numEntries, sizeof( numEntries));
				m_bytesInBuf += sizeof( numEntries);
				FillInEntries( numEntries);
				m_encodeScanState = kAddEntries;
				return( ScanBuffer( pDone));
			}
			break;
			
			case kBeginDataFork: {
				if (!m_dataForkSize) {
					m_encodeScanState = kDoneWithFile;
					return( PR_TRUE);
				}
				// Initialize the scan of the data fork...
				PRBool open = PR_FALSE;
				rv = m_pInputFile->IsStreamOpen( &open);
				if (!open)
					rv = m_pInputFile->OpenStreamForReading();
				if (NS_FAILED( rv))
					return( PR_FALSE);	
				m_encodeScanState = kScanningDataFork;
				return( PR_TRUE);
			}
			break;
			
			case kScanningDataFork: {
				PRBool result = FillBufferFromFile();
				if (!result)
					return( PR_FALSE);
				if (m_eof) {
					m_eof = PR_FALSE;
					result = ScanBuffer( pDone);
					if (!result)
						return( PR_FALSE);
					m_pInputFile->CloseStream();
					m_encodeScanState = kDoneWithFile;
					return( PR_TRUE);
				}
				else
					return( ScanBuffer( pDone));
			}
			break;
			
			case kScanningRsrcFork: {
				PRBool result = FillBufferFromFile();
				if (!result)
					return( PR_FALSE);
				if (m_eof) {
					m_eof = PR_FALSE;
					result = ScanBuffer( pDone);
					if (!result)
						return( PR_FALSE);
					m_pInputFile->CloseStream();
					m_encodeScanState = kBeginDataFork;
					return( PR_TRUE);
				}
				else
					return( ScanBuffer( pDone));
			}
			break;
			
			case kBeginResourceFork: {
				if (!m_resourceForkSize) {
					m_encodeScanState = kBeginDataFork;
					return( PR_TRUE);
				}
				/*
				// FIXME: Open the resource fork on the Mac!!!
				m_fH = UFile::OpenRsrcFileRead( m_inputFileLoc);
				if (m_fH == TR_FILE_ERROR)
					return( FALSE);	
				*/
				m_encodeScanState = kScanningRsrcFork;
				return( PR_TRUE);
			}
			break;
			
			case kAddEntries: {
				ShiftBuffer();
				if (!AddEntries())
					return( PR_FALSE);
				m_encodeScanState = kBeginResourceFork;
				return( ScanBuffer( pDone));
			}
			break;
			
			case kDoneWithFile: {
				ShiftBuffer();
				m_eof = PR_TRUE;
				if (!ScanBuffer( pDone))
					return( PR_FALSE);
				*pDone = PR_TRUE;
				return( PR_TRUE);
			}
			break;
		}
		
	}
	else
		return( nsImportScanFile::Scan( pDone));
		
	return( PR_FALSE);
}

