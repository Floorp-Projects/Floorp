/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsOE5File.h"
#include "nsCRT.h"
#include "OEDebugLog.h"


#define	kIndexGrowBy		50
#define	kSignatureSize		12
#define	kDontSeek			0xFFFFFFFF

static char *gSig = 
	"\xCF\xAD\x12\xFE\xC5\xFD\x74\x6F\x66\xE3\xD1\x11";


PRBool nsOE5File::VerifyLocalMailFile( nsIFileSpec *pFile)
{
	char		sig[kSignatureSize];
	
	if (!ReadBytes( pFile, sig, 0, kSignatureSize)) {
		return( PR_FALSE);
	}
	
	PRBool	result = PR_TRUE;

	for (int i = 0; (i < kSignatureSize) && result; i++) {
		if (sig[i] != gSig[i]) {
			result = PR_FALSE;
		}
	}
	
	char	storeName[14];
	if (!ReadBytes( pFile, storeName, 0x24C1, 12))
		result = PR_FALSE;

	storeName[12] = 0;
	
	if (nsCRT::strcasecmp( "LocalStore", storeName))
		result = PR_FALSE;
	
	return( result);
}

PRBool nsOE5File::IsLocalMailFile( nsIFileSpec *pFile)
{
	nsresult	rv;
	PRBool		isFile = PR_FALSE;

	rv = pFile->IsFile( &isFile);
	if (NS_FAILED( rv) || !isFile)
		return( PR_FALSE);
	
	rv = pFile->OpenStreamForReading();	
	if (NS_FAILED( rv))
		return( PR_FALSE);
	
	PRBool result = VerifyLocalMailFile( pFile);
	pFile->CloseStream();

	return( result);
}

PRBool nsOE5File::ReadIndex( nsIFileSpec *pFile, PRUint32 **ppIndex, PRUint32 *pSize)
{
	*ppIndex = nsnull;
	*pSize = 0;

	char		signature[4];
	if (!ReadBytes( pFile, signature, 0, 4))
		return( PR_FALSE);

	for (int i = 0; i < 4; i++) {
		if (signature[i] != gSig[i]) {
			IMPORT_LOG0( "*** Outlook 5.0 dbx file signature doesn't match\n");
			return( PR_FALSE);
		}
	}
	
	PRUint32	offset = 0x00e4;				
	PRUint32	indexStart = 0;
	if (!ReadBytes( pFile, &indexStart, offset, 4)) {
		IMPORT_LOG0( "*** Unable to read offset to index start\n");
		return( PR_FALSE);
	}

	PRUint32Array array;
	array.count = 0;
	array.alloc = kIndexGrowBy;
	array.pIndex = new PRUint32[kIndexGrowBy];
	
	PRUint32 next = ReadMsgIndex( pFile, indexStart, &array);
	while (next) {
		next = ReadMsgIndex( pFile, next, &array);
	}

	if (array.count) {
		*pSize = array.count;
		*ppIndex = array.pIndex;
		return( PR_TRUE);
	}		
	
	delete [] array.pIndex;
	return( PR_FALSE);
}


PRUint32 nsOE5File::ReadMsgIndex( nsIFileSpec *file, PRUint32 offset, PRUint32Array *pArray)
{
	// Record is:
		// 4 byte marker
		// 4 byte unknown
		// 4 byte nextSubIndex
		// 4 byte (parentIndex?)
		// 2 bytes unknown
		// 1 byte length - # of entries in this record
		// 1 byte unknown
		// 4 byte unknown
		// length records consisting of 3 longs
			//	1 - pointer to record
			//	2 - child index pointer
			//	3 - number of records in child

	PRUint32	marker;

	if (!ReadBytes( file, &marker, offset, 4))
		return( 0);
	
	if (marker != offset)
		return( 0);
	

	PRUint32	vals[3];

	if (!ReadBytes( file, vals, offset + 4, 12))
		return( 0);

	
	PRUint8	len[4];
	if (!ReadBytes( file, len, offset + 16, 4))
		return( 0);


	
	PRUint32	cnt = (PRUint32) len[1];
	cnt *= 3;
	PRUint32	*pData = new PRUint32[cnt];
	
	if (!ReadBytes( file, pData, offset + 24, cnt * 4)) {
		delete [] pData;
		return( 0);
	}	

	PRUint32	next;
	PRUint32	indexOffset;
	PRUint32 *	pRecord = pData;
	PRUint32 *	pNewIndex;

	for (PRUint8 i = 0; i < (PRUint8)len[1]; i++, pRecord += 3) {
		indexOffset = pRecord[0];
	
		if (pArray->count >= pArray->alloc) {
			pNewIndex = new PRUint32[ pArray->alloc + kIndexGrowBy];
			nsCRT::memcpy( pNewIndex, pArray->pIndex, (pArray->alloc * 4));
			(pArray->alloc) += kIndexGrowBy;		
			delete [] pArray->pIndex;
			pArray->pIndex = pNewIndex;
		}
		
		/* 
			We could do some checking here if we wanted - 
			make sure the index is within the file,
			make sure there isn't a duplicate index, etc.
		*/

		pArray->pIndex[pArray->count] = indexOffset;
		(pArray->count)++;
		

		
		next = pRecord[1];
		if (next)
			while ((next = ReadMsgIndex( file, next, pArray)) != 0);
	}
	delete pData;

	// return the pointer to the next subIndex
	return( vals[1]);
}

PRBool nsOE5File::IsFromLine( char *pLine, PRUint32 len)
{
	if (len >= 5) {
		if ( (*pLine == 'F') && (pLine[1] = 'r') && (pLine[2] == 'o') && (pLine[3] == 'm') && (pLine[4] == ' '))
			return( PR_TRUE);
	}

	return( PR_FALSE);
}

// Anything over 16K will be assumed BAD, BAD, BAD!
#define	kMailboxBufferSize	0x4000
const char *nsOE5File::m_pFromLineSep = "From ????@???? 1 Jan 1965 00:00:00\x0D\x0A";

nsresult nsOE5File::ImportMailbox( PRBool *pAbort, nsString& name, nsIFileSpec *inFile, nsIFileSpec *pDestination)
{
	nsresult	rv;
	
	rv = inFile->OpenStreamForReading();
	if (NS_FAILED( rv)) return( rv);
	rv = pDestination->OpenStreamForWriting();
	if (NS_FAILED( rv)) {
		inFile->CloseStream();
		return( rv);
	}

	PRUint32 *	pIndex;
	PRUint32	indexSize;
	
	if (!ReadIndex( inFile, &pIndex, &indexSize)) {
		IMPORT_LOG1( "No messages found in mailbox: %S\n", name.GetUnicode());
		inFile->CloseStream();
		pDestination->CloseStream();
		return( NS_OK);
	}	
	
	char *		pBuffer = new char[kMailboxBufferSize];
	if (!(*pAbort))
		ConvertIndex( inFile, pBuffer, pIndex, indexSize);
	
	PRUint32	block[4];
	PRInt32		sepLen = (PRInt32) nsCRT::strlen( m_pFromLineSep);
	PRInt32		written;

	/*
		Each block is:
			marker - matches file offset
			block length
			text length in block
			pointer to next block. (0 if end)

		So what we do for each message is:
			Read the first block
			Write out the From message separator if the message doesn't already
				start with one.
			Write out all subsequent blocks of text keeping track of the last
				2 bytes written of the message.
			if the last 2 bytes aren't and end of line then write one out before
				proceeding to the next message.
	*/
	
	char		last2[2] = {0, 0};
	PRUint32	next;
	rv = NS_OK;
	for (PRUint32 i = 0; (i < indexSize) && !(*pAbort); i++) {
		if (pIndex[i]) {
			if (ReadBytes( inFile, block, pIndex[i], 16) && 
						(block[0] == pIndex[i]) &&
						(block[2] < kMailboxBufferSize) &&
						(ReadBytes( inFile, pBuffer, kDontSeek, block[2]))) {
				// write out the from separator.
				if (!IsFromLine( pBuffer, block[2])) {
					rv = pDestination->Write( m_pFromLineSep, sepLen, &written);
					// FIXME: Do I need to check the return value of written???
					if (NS_FAILED( rv))
						break;
					last2[0] = 13;
					last2[1] = 10;
				}
				rv = WriteMessageBuffer( pDestination, pBuffer, block[2], last2);
				if (NS_FAILED( rv))
					break;
				next = block[3];
				while (next && !(*pAbort)) {
					if (ReadBytes( inFile, block, next, 16) &&
						(block[0] == next) &&
						(block[2] < kMailboxBufferSize) &&
						(ReadBytes( inFile, pBuffer, kDontSeek, block[2]))) {
						if (block[2])
							rv = WriteMessageBuffer( pDestination, pBuffer, block[2], last2);
						if (NS_FAILED( rv))
							break;
						next = block[3];
					}
					else {
						IMPORT_LOG2( "Error reading message from %S at 0x%lx\n", name.GetUnicode(), pIndex[i]);
						pDestination->Write( "\x0D\x0A", 2, &written);
						next = 0;
						last2[0] = 13;
						last2[1] = 10;
					}
				}
				if (NS_FAILED( rv) || (*pAbort))
					break;
				if ((last2[0] != 13) || (last2[1] != 10)) {
					pDestination->Write( "\x0D\x0A", 2, &written);
					last2[0] = 13;
					last2[1] = 10;
				}
			}
			else {
				// Error reading message, should this be logged???
				IMPORT_LOG2( "Error reading message from %S at 0x%lx\n", name.GetUnicode(), pIndex[i]);
			}			
		}
	}
	

	delete [] pBuffer;
	inFile->CloseStream();
	pDestination->CloseStream();

	return( rv);
}

nsresult nsOE5File::WriteMessageBuffer( nsIFileSpec *stream, char *pBuffer, PRUint32 size, char *last2)
{
	if (!size)
		return( NS_OK);

	/*
		FIXME: Escape from lines!
	*/

	PRInt32		written;
	nsresult	rv;

	// FIXME: Do I need to check the return value of written???
	rv = stream->Write( pBuffer, (PRInt32) size, &written);

	if (NS_SUCCEEDED( rv)) {
		if (size > 1) {
			last2[0] = *(pBuffer + size - 2);
			last2[1] = *(pBuffer + size - 1);
		}
		else {
			last2[0] = last2[1];
			last2[1] = *pBuffer;
		}
	}

	return( rv);
}

/*
	A message index record consists of:
		4 byte marker - matches record offset
		4 bytes size - size of data after this header
		2 bytes unknown
		1 bytes - number of attributes
		1 byte unknown
		Each attribute is a 4 byte value with the 1st byte being the tag
		and the remaing 3 bytes being data.  The data is either a direct
		offset of an offset within the message index that points to the
		data for the tag.

		Current known tags are:
			0x02 - a time value
			0x04 - text offset pointer, the data is the offset after the attribute
				of a 4 byte pointer to the message text
			0x05 - offset to truncated subject
			0x08 - offste to subject
			0x0D - offset to from
			0x0E - offset to from addresses
			0x13 - offset to to name
			0x45 - offset to to address
			0x80 - msgId
			0x84 - direct text offset, direct pointer to message text
*/

void nsOE5File::ConvertIndex( nsIFileSpec *pFile, char *pBuffer, PRUint32 *pIndex, PRUint32 size)
{
	// for each index record, get the actual message offset!  If there is a problem
	// just record the message offset as 0 and the message reading code
	// can log that error information.

	PRUint8		recordHead[12];
	PRUint32	marker;
	PRUint32	recordSize;
	PRUint32	numAttrs;
	PRUint32	offset;
	PRUint32	attrIndex;
	PRUint32	attrOffset;
	PRUint8		tag;
	PRUint32	tagData;

	for (PRUint32 i = 0; i < size; i++) {
		offset = 0;
		if (ReadBytes( pFile, recordHead, pIndex[i], 12)) {
			nsCRT::memcpy( &marker, recordHead, 4);
			nsCRT::memcpy( &recordSize, recordHead + 4, 4);
			numAttrs = (PRUint32) recordHead[10];
			if ((marker == pIndex[i]) && (recordSize < kMailboxBufferSize) && ((numAttrs * 4) <= recordSize)) {
				if (ReadBytes( pFile, pBuffer, kDontSeek, recordSize)) {
					attrOffset = 0;
					for (attrIndex = 0; attrIndex < numAttrs; attrIndex++, attrOffset += 4) {
						tag = (PRUint8) pBuffer[attrOffset];
						if (tag == (PRUint8) 0x84) {
							tagData = 0;
							nsCRT::memcpy( &tagData, pBuffer + attrOffset + 1, 3);
							offset = tagData;
							break;
						}
						else if (tag == (PRUint8) 0x04) {
							tagData = 0;
							nsCRT::memcpy( &tagData, pBuffer + attrOffset + 1, 3);
							if (((numAttrs * 4) + tagData + 4) <= recordSize)
								nsCRT::memcpy( &offset, pBuffer + (numAttrs * 4) + tagData, 4);
						}
					}
				}
			}
		}
		pIndex[i] = offset;
	}
}


PRBool nsOE5File::ReadBytes( nsIFileSpec *stream, void *pBuffer, PRUint32 offset, PRUint32 bytes)
{
	nsresult	rv;
	
	if (offset != kDontSeek) {
		rv = stream->Seek( offset);
		if (NS_FAILED( rv))
			return( PR_FALSE);
	}
		
	if (!bytes)
		return( PR_TRUE);

	PRInt32	cntRead;
	char *	pReadTo = (char *)pBuffer;
	rv = stream->Read( &pReadTo, bytes, &cntRead);	
	if (NS_FAILED( rv) || ((PRUint32)cntRead != bytes))
		return( PR_FALSE);
	return( PR_TRUE);
	
}

