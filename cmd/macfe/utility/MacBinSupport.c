/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* MacBinary support.c

This file implements MacBinary (actually MacBinary II) support for the following:

	-Determine the size of a file when encoded in MacBinary

	-Inline encoding of a file in MacBinary format

The MacBinary II format consists of a 128-byte header containing all the
information necessary to reproduce the document's directory entry on the
receiving Macintosh; followed by the document's Data Fork (if it has one),
padded with nulls to a multiple of 128 bytes (if necessary); followed by the
document's Resource Fork (again, padded if necessary). The lengths of these
forks (either or both of which may be zero) are contained in the header. 

The format of the header for MacBinary II is as follows:

  Offset 000	Byte		old version number, must be kept at zero for compatibility
  Offset 001	Byte		Length of filename (must be in the range 1-63)
  Offset 002	1 to 63 chars, filename (only "length" bytes are significant).
  Offset 065	Long	file type (normally expressed as four characters)
  Offset 069	Long	file creator (normally expressed as four characters)
  Offset 073	Byte		original Finder flags
                                 Bit 7 - Locked.
                                 Bit 6 - Invisible.
                                 Bit 5 - Bundle.
                                 Bit 4 - System.
                                 Bit 3 - Bozo.
                                 Bit 2 - Busy.
                                 Bit 1 - Changed.
                                 Bit 0 - Inited.
  Offset 074	Byte	zero fill, must be zero for compatibility
  Offset 075	Short	file's vertical position within its window.
  Offset 077	Short	file's horizontal position within its window.
  Offset 079	Short	file's window or folder ID.
  Offset 081	Byte	"Protected" flag (in low order bit).
  Offset 082	Byte	zero fill, must be zero for compatibility
  Offset 083	Long	Data Fork length (bytes, zero if no Data Fork).
  Offset 087	Long	Resource Fork length (bytes, zero if no R.F.).
  Offset 091	Long	File's creation date
  Offset 095	Long	File's "last modified" date.
  Offset 099	Short	zero fill (was file comment length which is not supported)
  Offset 101	Byte	Finder Flags, bits 0-7. (Bits 8-15 are already in byte 73)
  Offset 116	Long	Length of total files when packed files are unpacked.
			             This is only used by programs that pack and unpack on the fly,
			             mimicing a standalone utility such as PackIt.  A program that is
			             uploading a single file must zero this location when sending a
			             file.  Programs that do not unpack/uncompress files when
			             downloading may ignore this value.
  Offset 120	Short	zero fill (was length of a secondary header which is not supported)
  Offset 122	Byte	Version number of Macbinary II that the uploading program
             				is written for (the version begins at 129)
  Offset 123	Byte	Minimum MacBinary II version needed to read this file
             				(start this value at 129)
  Offset 124	Short	CRC of previous 124 bytes

All values are stored in normal 68000 order, with Most Significant Byte
appearing first then the file.  Any bytes in the header not defined above
should be set to zero.


MacBinary header creation and CRC calculation based on Erny Tontlinger's free 'Terminal' source

*/

#include "xp.h"

#include "MacBinSupport.h"

#include "MoreFilesExtras.h"

enum
{
	kMB_SendingHeader,			/* Data from read is MB header */
	kMB_SetupDataFork,			/* Finished sending MB header so prepare data fork */
	kMB_SendingDataFork,		/* Data from read is file's data fork */
	kMB_SetupResFork,			/* Finished sending data fork so prepare res fork */
	kMB_SendingResFork,			/* Data from read is file's data fork */
	kMB_FinishedFile			/* Nothing left to send - file finished */
	
};

static Byte fillerBuf[kMBHeaderLength];

static unsigned short CalcMacBinaryCRC(Byte *ptr, unsigned long count)
{
	unsigned short crc;
	unsigned short i;

	crc = 0;
	while (count-- > 0) {
		crc = crc ^ (unsigned short)*ptr++ << 8;
		for (i = 0; i < 8; ++i)
			if (crc & 0x8000)
				crc = crc << 1 ^ 0x1021;
			else
				crc = crc << 1;
	}
	return crc;
}

static OSErr InitMacBinaryHeader(MB_FileSpec *mbFileSpec)
{
#define	MB_VersionNumber	129
#define MBH_name			1
#define MBH_info1			65
#define MBH_protected		81
#define MBH_dLength			83
#define MBH_rLength			87
#define MBH_creation		91
#define MBH_modification	95
#define MBH_getInfoLength	99
#define MBH_info2			101
#define MBH_filesLength		116
#define MBH_sHeaderLength	120
#define MBH_newVersion		122
#define MBH_minimumVersion	123
#define MBH_crc				124

	OSErr 			theErr;
	unsigned short	crc;
	HParamBlockRec	param;

	XP_MEMSET(&param, 0, sizeof(param));
	param.fileParam.ioNamePtr = (StringPtr)mbFileSpec->theFileSpec.name;
	param.fileParam.ioVRefNum = mbFileSpec->theFileSpec.vRefNum;
	param.fileParam.ioDirID = mbFileSpec->theFileSpec.parID;
	theErr = PBHGetFInfoSync(&param);
	if (theErr != noErr)
		return (theErr);

	XP_MEMSET(mbFileSpec->mbHeader, 0, kMBHeaderLength);

	XP_MEMCPY(
		&mbFileSpec->mbHeader[MBH_name],
		mbFileSpec->theFileSpec.name, mbFileSpec->theFileSpec.name[0] + 1);
	mbFileSpec->mbHeader[MBH_info2] = param.fileParam.ioFlFndrInfo.fdFlags & 0x00FF;
	param.fileParam.ioFlFndrInfo.fdFlags &= 0xFF00;
	*(long *)&param.fileParam.ioFlFndrInfo.fdLocation = 0;
	param.fileParam.ioFlFndrInfo.fdFldr = 0;
	XP_MEMCPY(&mbFileSpec->mbHeader[MBH_info1], (void *)&param.fileParam.ioFlFndrInfo, 16);
	mbFileSpec->mbHeader[MBH_protected] = (Byte)(param.fileParam.ioFlAttrib) & 0x01;
	XP_MEMCPY(&mbFileSpec->mbHeader[MBH_dLength], (void *)&param.fileParam.ioFlLgLen, 4);
	XP_MEMCPY(&mbFileSpec->mbHeader[MBH_rLength], (void *)&param.fileParam.ioFlRLgLen, 4);
	XP_MEMCPY(&mbFileSpec->mbHeader[MBH_creation], (void *)&param.fileParam.ioFlCrDat, 4);
	XP_MEMCPY(&mbFileSpec->mbHeader[MBH_modification], (void *)&param.fileParam.ioFlMdDat, 4);
	mbFileSpec->mbHeader[MBH_newVersion] = MB_VersionNumber;
	mbFileSpec->mbHeader[MBH_minimumVersion] = MB_VersionNumber;
	crc = CalcMacBinaryCRC(mbFileSpec->mbHeader, 124);
	XP_MEMCPY(&mbFileSpec->mbHeader[MBH_crc], (void *)&crc, 2);
	
	mbFileSpec->dataForkLength = param.fileParam.ioFlLgLen;
	mbFileSpec->resForkLength = param.fileParam.ioFlRLgLen;
	
	return (noErr);
}

static long PadBufferToMacBinBlock(char *buf, long bytesInBuf)
{
	long	paddingNeeded = 0;
	
	if (bytesInBuf % kMBHeaderLength)
	{
		paddingNeeded = kMBHeaderLength - (bytesInBuf % kMBHeaderLength);
		XP_MEMCPY(&buf[bytesInBuf], fillerBuf, paddingNeeded);
	}
	
	return (bytesInBuf + paddingNeeded);
}

/* MB_Stat

	Returns the size of the file when encoded in MacBinary format
	
	This is computed as 128 bytes for the MacBinary header + the size of the data fork
	rounded up to a multiple of 128 bytes + the size of the resource fork rounded up to
	a multiple of 128 bytes.
*/

int MB_Stat( const char* name, XP_StatStruct * outStat,  XP_FileType type )
{
	int		result = -1;
	long	totalFileSize = 0;
	char	*newName = WH_FileName( name, type );
	FSSpec	fileSpec = {0, 0, "\p"};
	OSErr	theErr;
	long	dataSize;
	long	rsrcSize;
	long	sizeRemainder;
	
	/* See if we managed to copy the name */
	if (!newName)
		return (result);
	
	/* Now see if we can find out something about the file */
	theErr = FSSpecFromPathname_CWrapper(newName, &fileSpec);
	if (theErr == noErr)
	{
		theErr = FSpGetFileSize(&fileSpec, &dataSize, &rsrcSize);
		if (theErr == noErr)
		{
			result = 0;				/* Set the result code to success */
			
			totalFileSize = kMBHeaderLength;	/* 128 bytes for the MacBin header */
			
			/* Make sure data and rsrc sizes are multiples of 128 bytes
				before adding them to totalFileSize */
			sizeRemainder = dataSize % kMBHeaderLength;
			if (sizeRemainder)
			{
				dataSize += (kMBHeaderLength - sizeRemainder);
			}
			totalFileSize += dataSize;
			
			sizeRemainder = rsrcSize % kMBHeaderLength;
			if (sizeRemainder)
			{
				rsrcSize += (kMBHeaderLength - sizeRemainder);
			}
			totalFileSize += rsrcSize;
			
			/* Stuff the calculated file size into the appropriate stat field */
			outStat->st_size = totalFileSize;
		}
	}
	
	XP_FREE(newName);
	
	return (result);
}

/* MB_Open

	Prepares a file for inline encoding in the MacBinary format.  It builds the
	MacBinary header and initializes the state machine for the encoding process.
	
	
	Get file info
	Build MacBinary header
	Set state machine to kMB_SendingHeader

*/

OSErr MB_Open(const char *name, MB_FileSpec *mbFileSpec)
{
	char	*newName = WH_FileName( name, xpFileToPost );
	OSErr	theErr = noErr;

	/* See if we managed to copy the name */
	if (!newName)
		return (fnfErr);
	
	/* Now see if we can find make an FSSpec for the file */
	theErr = FSSpecFromPathname_CWrapper(newName, &mbFileSpec->theFileSpec);
	if (theErr == noErr)
	{
		theErr = InitMacBinaryHeader(mbFileSpec);
		if (theErr == noErr)
		{
			// Set up the rest of the MB_FileSpec info
			mbFileSpec->fileState = kMB_SendingHeader;
			mbFileSpec->fileRefNum = -1;
			mbFileSpec->dataBytesRead = 0;
			mbFileSpec->resBytesRead = 0;
		}
	}
	
	/* Make sure our filler buffer is cleared */
	XP_MEMSET(fillerBuf, 0, kMBHeaderLength);

	return (theErr);
}

/* MB_Read

	Provides a MacBinary encoded version of a file.  All reads are done to the supplied
	buffer.  The MB_Read function just returns how many bytes have been placed in the
	buffer.
	
	Switch on state
		kMB_SendingHeader
			set state to kMB_SetupDataFork
			Return the # of bytes in an MB header
		kMB_SetupDataFork
			If hasDataFork
				Open the data fork of the file
				Set the state to kMB_SendingDataFork
				GOTO kMB_SendingDataFork
			else
				GOTO kMB_SetupResFork
		kMB_SendingDataFork
			Read a buffer's worth of data
				Make sure buffer is padded to MB block boundary
				If EOF set state to kMB_SetupResFork
			Return the # of bytes in buffer
		kMB_SetupResFork
			Close data fork if open
			If hasResFork
				Open the resource fork of the file
				Set the state to kMB_SendingResFork
				GOTO kMB_SendingResFork
			else
				GOTO kMB_FinishedFile
		kMB_SendingResFork
			Read a buffer's worth of data
				Make sure buffer is padded to MB block boundary
				If EOF set state to kMB_FinishedFile
			Return the # of bytes in buffer
		kMB_FinishedFile
			Close resource fork if open
			Set the state to kMB_FinishedFile
		
*/
int32	MB_Read(char *buf, int bufSize, MB_FileSpec *mbFileSpec)
{
	long	bytesInBuf = 0;
	OSErr	theErr = noErr;
	long	count = bufSize;
	
	switch (mbFileSpec->fileState)
	{
		case kMB_SendingHeader:
			XP_MEMCPY(buf, mbFileSpec->mbHeader, kMBHeaderLength);
			bytesInBuf = kMBHeaderLength;
			mbFileSpec->fileState = kMB_SetupDataFork;
			break;
		
		case kMB_SetupDataFork:
			if (mbFileSpec->dataForkLength)
			{
				theErr = FSpOpenDF(&mbFileSpec->theFileSpec, fsRdPerm, &mbFileSpec->fileRefNum);
				if (theErr == noErr)
				{
					mbFileSpec->fileState = kMB_SendingDataFork;
					
					/* Now that the data fork is open jump into the read state */
					goto SendingDataFork;
				}
				else
				{	/* Couldn't open the data fork so exit the state machine */
					goto FinishedFile;
				}
			}
			else
			{	/* Apparently no data fork so go ahead and try the res fork */
				goto SetupResFork;
			}
			break;

		case kMB_SendingDataFork:
SendingDataFork:
			theErr = FSRead(mbFileSpec->fileRefNum, &count, buf);
			if (theErr == noErr || theErr == eofErr)
			{
				bytesInBuf = PadBufferToMacBinBlock(buf, count);
				mbFileSpec->dataBytesRead += count;
				
				/* See if we've reached EOF for the data */
				if (mbFileSpec->dataBytesRead == mbFileSpec->dataForkLength)
					mbFileSpec->fileState = kMB_SetupResFork;
			}
			else
			{	/* Got some sort of error reading the data fork so exit the state machine */
				goto FinishedFile;
			}
			break;
		
		case kMB_SetupResFork:
SetupResFork:
			/* Close the data fork */
			if (mbFileSpec->fileRefNum != -1)
			{
				FSClose(mbFileSpec->fileRefNum);
				mbFileSpec->fileRefNum = -1;
			}
			
			/* See if we have a res fork to send */
			if (mbFileSpec->resForkLength)
			{
				theErr = FSpOpenRF(&mbFileSpec->theFileSpec, fsRdPerm, &mbFileSpec->fileRefNum);
				if (theErr == noErr)
				{
					mbFileSpec->fileState = kMB_SendingResFork;
					/* Now that the res fork is open jump into the read state */
					goto SendingResFork;
				}
				else
				{	/* Couldn't open the res fork so exit the state machine */
					goto FinishedFile;
				}
			}
			else
			{	/* Apparently no res fork so go ahead and exite the state machine */
				goto FinishedFile;
			}

		case kMB_SendingResFork:
SendingResFork:
			theErr = FSRead(mbFileSpec->fileRefNum, &count, buf);
			if (theErr == noErr || theErr == eofErr)
			{
				bytesInBuf = PadBufferToMacBinBlock(buf, count);
				mbFileSpec->resBytesRead += count;
				
				/* See if we've reached EOF for the res fork */
				if (mbFileSpec->resBytesRead == mbFileSpec->resForkLength)
					mbFileSpec->fileState = kMB_FinishedFile;
			}
			else
			{	/* Got some sort of error reading the res fork so exit the state machine */
				goto FinishedFile;
			}
			break;
		
		case kMB_FinishedFile:
FinishedFile:
			/* If we're here then we're done with the file */
			mbFileSpec->fileState = kMB_FinishedFile;
			
			/* If we have a fork open, close it */
			if (mbFileSpec->fileRefNum != -1)
			{
				FSClose(mbFileSpec->fileRefNum);
				mbFileSpec->fileRefNum = -1;
			}
			break;
	}
	
	return ((int32)bytesInBuf);
}

/* MB_Close

	Nothing really to do since the file forks are actually closed in the MB_Read
	routine after they have been sent.
*/
void MB_Close(MB_FileSpec *mbFileSpec)
{
	if (mbFileSpec->fileRefNum != -1)
		FSClose(mbFileSpec->fileRefNum);
	
	mbFileSpec->fileState = kMB_FinishedFile;
}
