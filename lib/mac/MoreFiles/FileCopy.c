/*
**	Apple Macintosh Developer Technical Support
**
**	FileCopy: A robust, general purpose file copy routine.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		FileCopy.c
**
**	Copyright © 1992-1996 Apple Computer, Inc.
**	All rights reserved.
**
**	You may incorporate this sample code into your applications without
**	restriction, though the sample code has been provided "AS IS" and the
**	responsibility for its operation is 100% yours.  However, what you are
**	not permitted to do is to redistribute the source as "DSC Sample Code"
**	after having made changes. If you're going to re-distribute the source,
**	we require that you make it clear in the source that the code was
**	descended from Apple Sample Code, but that you've made changes.
*/

#include <Types.h>
#include <Errors.h>
#include <Memory.h>
#include <Files.h>

#define	__COMPILINGMOREFILES

#include "MoreFiles.h"
#include "MoreFilesExtras.h"
#include "MoreDesktopMgr.h"
#include "FileCopy.h"

/*****************************************************************************/

/* local constants */

/*	The deny-mode privileges to use when opening the source and destination files. */

enum
{
	srcCopyMode = dmRdDenyWr,
	dstCopyMode = dmWrDenyRdWr
};

/*	The largest (16K) and smallest (.5K) copy buffer to use if the caller doesn't supply 
**	their own copy buffer. */

enum
{
	bigCopyBuffSize  = 0x00004000,
	minCopyBuffSize  = 0x00000200
};

/*****************************************************************************/

/* static prototypes */

static	OSErr	GetDestinationDirInfo(short vRefNum,
									  long dirID,
									  StringPtr name,
									  long *theDirID,
									  Boolean *isDirectory,
									  Boolean *isDropBox);
/*	GetDestinationDirInfo tells us if the destination is a directory, it's
	directory ID, and if it's an AppleShare drop box (write privileges only --
	no read or search privileges).
	vRefNum		input:	Volume specification.
	dirID		input:	Directory ID.
	name		input:	Pointer to object name, or nil when dirID
						specifies a directory that's the object.
	theDirID	output:	If the object is a file, then its parent directory
						ID. If the object is a directory, then its ID.
	isDirectory	output:	True if object is a directory; false if
						object is a file.
	isDropBox	output:	True if directory is an AppleShare drop box.
*/

static	OSErr	CheckForForks(short vRefNum,
							  long dirID,
							  ConstStr255Param name,
							  Boolean *hasDataFork,
							  Boolean *hasResourceFork);
/*	CheckForForks tells us if there is a data or resource fork to copy.
	vRefNum		input:	Volume specification of the file's current
							location.
	dirID		input:	Directory ID of the file's current location.
	name		input:	The name of the file.
*/

static	OSErr	PreflightFileCopySpace(short srcVRefNum,
									   long srcDirID,
									   ConstStr255Param srcName,
									   StringPtr dstVolName,
									   short dstVRefNum,
									   Boolean *spaceOK);
/*	PreflightFileCopySpace determines if there's enough space on a
	volume to copy the specified file to that volume.
	Note: The results of this routine are not perfect. For example if the
	volume's catalog or extents overflow file grows when the new file is
	created, more allocation blocks may be needed beyond those needed for
	the file's data and resource forks.

	srcVRefNum		input:	Volume specification of the file's current
							location.
	srcDirID		input:	Directory ID of the file's current location.
	srcName			input:	The name of the file.
	dstVolName		input:	A pointer to the name of the volume where
							the file will be copied or NULL.
	dstVRefNum		input:	Volume specification indicating the volume
							where the file will be copied.
	spaceOK			output:	true if there's enough space on the volume for
							the file's data and resource forks.
*/

/*****************************************************************************/

static	OSErr	GetDestinationDirInfo(short vRefNum,
									  long dirID,
									  StringPtr name,
									  long *theDirID,
									  Boolean *isDirectory,
									  Boolean *isDropBox)
{
	CInfoPBRec pb;
	OSErr error;

	pb.dirInfo.ioACUser = 0;	/* ioACUser used to be filler2, clear it before calling GetCatInfo */
	error = GetCatInfoNoName(vRefNum, dirID, name, &pb);
	*theDirID = pb.dirInfo.ioDrDirID;
	*isDirectory = (pb.dirInfo.ioFlAttrib & ioDirMask) != 0;
	/* see if access priviledges are make changes, not see folder, and not see files (drop box) */
	*isDropBox = ((pb.dirInfo.ioACUser & 0x07) == 0x03);
	return ( error );
}

/*****************************************************************************/

static	OSErr	CheckForForks(short vRefNum,
							  long dirID,
							  ConstStr255Param name,
							  Boolean *hasDataFork,
							  Boolean *hasResourceFork)
{
	HParamBlockRec pb;
	OSErr error;
	
	pb.fileParam.ioNamePtr = (StringPtr)name;
	pb.fileParam.ioVRefNum = vRefNum;
	pb.fileParam.ioFVersNum = 0;
	pb.fileParam.ioDirID = dirID;
	pb.fileParam.ioFDirIndex = 0;
	error = PBHGetFInfoSync(&pb);
	if ( error == noErr )
	{
		*hasDataFork = (pb.fileParam.ioFlLgLen != 0);
		*hasResourceFork = (pb.fileParam.ioFlRLgLen != 0);
	}
	
	return ( error );
}

/*****************************************************************************/

static	OSErr	PreflightFileCopySpace(short srcVRefNum,
									   long srcDirID,
									   ConstStr255Param srcName,
									   StringPtr dstVolName,
									   short dstVRefNum,
									   Boolean *spaceOK)
{
	HParamBlockRec pb;
	OSErr error;
	unsigned short dstFreeBlocks;
	unsigned short dstBlksPerAllocBlk;
	unsigned short srcDataBlks;
	unsigned short srcResourceBlks;
	
	/* Get the number of 512 byte blocks per allocation block and */
	/* number of free allocation blocks on the destination volume */
	error = GetVolumeInfoNoName(dstVolName, dstVRefNum, &pb);
	if ( error == noErr )
	{
		/* get allocation block size (always multiple of 512) and divide by 512
		  to get number of 512-byte blocks per allocation block */
		dstBlksPerAllocBlk = ((unsigned long)pb.volumeParam.ioVAlBlkSiz >> 9);
		dstFreeBlocks = (unsigned short)pb.volumeParam.ioVFrBlk;
		
		/* Now, get the size of the file's data resource forks */
		pb.fileParam.ioNamePtr = (StringPtr)srcName;
		pb.fileParam.ioVRefNum = srcVRefNum;
		pb.fileParam.ioFVersNum = 0;
		pb.fileParam.ioDirID = srcDirID;
		pb.fileParam.ioFDirIndex = 0;
		error = PBHGetFInfoSync(&pb);
		if ( error == noErr )
		{
			/* get number of 512-byte blocks needed for data fork */
			srcDataBlks = ((unsigned long)pb.fileParam.ioFlLgLen % 512) ?
						  (((unsigned long)pb.fileParam.ioFlLgLen >> 9) + 1) :
						  ((unsigned long)pb.fileParam.ioFlLgLen >> 9);
			/* now, calculate number of new allocation blocks needed */
			srcDataBlks = (srcDataBlks % dstBlksPerAllocBlk) ?
						  ((srcDataBlks / dstBlksPerAllocBlk) + 1) :
						  (srcDataBlks / dstBlksPerAllocBlk);
		
			/* get number of 512-byte blocks needed for resource fork */
			srcResourceBlks = ((unsigned long)pb.fileParam.ioFlRLgLen % 512) ?
							  (((unsigned long)pb.fileParam.ioFlRLgLen >> 9) + 1) :
							  ((unsigned long)pb.fileParam.ioFlRLgLen >> 9);
			/* now, calculate number of new allocation blocks needed */
			srcResourceBlks = (srcResourceBlks % dstBlksPerAllocBlk) ?
							  ((srcResourceBlks / dstBlksPerAllocBlk) + 1) :
							  (srcResourceBlks / dstBlksPerAllocBlk);
			
			/* Is there enough room on the destination volume for the source file? */
			*spaceOK = ((srcDataBlks + srcResourceBlks) <= dstFreeBlocks);
		}
	}
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FileCopy(short srcVRefNum,
						 long srcDirID,
						 ConstStr255Param srcName,
						 short dstVRefNum,
						 long dstDirID,
						 StringPtr dstPathname,
						 StringPtr copyName,
						 void *copyBufferPtr,
						 long copyBufferSize,
						 Boolean preflight)
{
	OSErr	err;

	short	srcRefNum = 0,			/* 0 when source data and resource fork are closed  */
			dstDataRefNum = 0,		/* 0 when destination data fork is closed */
			dstRsrcRefNum = 0;		/* 0 when destination resource fork is closed */
	
	Str63	dstName;				/* The filename of the destination. It might be the
									** source filename, it might be a new name... */
	
	GetVolParmsInfoBuffer infoBuffer; /* Where PBGetVolParms dumps its info */
	long	srcServerAdr;			/* AppleTalk server address of source (if any) */
	
	Boolean	dstCreated = false,		/* true when destination file has been created */
			ourCopyBuffer = false,	/* true if we had to allocate the copy buffer */
			isDirectory,			/* true if destination is really a directory */
			isDropBox;				/* true if destination is an AppleShare drop box */
	
	long	tempLong;
	short	tempInt;
	
	Boolean	spaceOK;				/* true if there's enough room to copy the file to the destination volume */

	Boolean	hasDataFork;
	Boolean	hasResourceFork;

	/* Preflight for size */
	if ( preflight )
	{
		err = PreflightFileCopySpace(srcVRefNum, srcDirID, srcName,
									 dstPathname, dstVRefNum, &spaceOK);
		if ( err != noErr )
			return ( err );
		if ( !spaceOK )
			return ( dskFulErr );
	}

	/* get the destination's real dirID and make sure it really is a directory */
	err = GetDestinationDirInfo(dstVRefNum, dstDirID, dstPathname,
								&dstDirID, &isDirectory, &isDropBox);
	if ( err != noErr )
		goto ErrorExit;
	if ( !isDirectory )
		return ( dirNFErr );

	/* get the destination's real vRefNum */
	err = DetermineVRefNum(dstPathname, dstVRefNum, &dstVRefNum);
	if ( err != noErr )
		goto ErrorExit;
	
	/* See if PBHCopyFile can be used.  Using PBHCopyFile saves time by letting the file server
	** copy the file if the source and destination locations are on the same file server. */
	tempLong = sizeof(infoBuffer);
	err = HGetVolParms((StringPtr)srcName, srcVRefNum, &infoBuffer, &tempLong);
	if ( (err != noErr) && (err != paramErr) )
		return ( err );

	if ( (err != paramErr) && hasCopyFile(infoBuffer) )
	{
		/* The source volume supports PBHCopyFile. */
		srcServerAdr = infoBuffer.vMServerAdr;

		/* Now, see if the destination volume is on the same file server. */
		tempLong = sizeof(infoBuffer);
		err = HGetVolParms(NULL, dstVRefNum, &infoBuffer, &tempLong);
		if ( (err != noErr) && (err != paramErr) )
			return ( err );
		if ( (err != paramErr) && (srcServerAdr == infoBuffer.vMServerAdr) )
		{
			/* Source and Dest are on same server and PBHCopyFile is supported. Copy with CopyFile. */
			err = HCopyFile(srcVRefNum, srcDirID, srcName, dstVRefNum, dstDirID, NULL, copyName);
			if ( err != noErr )
				goto ErrorExit;
			
			dstCreated = true;
			
			/* AppleShare's CopyFile clears the isAlias bit, so I still need to attempt to copy
			   the File's attributes to attempt to get things right. */
			if ( copyName != NULL )				/* Did caller supply copy file name? */
			{
				/* Yes, use the caller supplied copy file name. */
				return ( CopyFileMgrAttributes(srcVRefNum, srcDirID, (StringPtr)srcName,
											 dstVRefNum, dstDirID, copyName, true) );
			}
			else
			{
				/* They didn't, so get the source file name and use it. */
				if ( GetFilenameFromPathname(srcName, dstName) == noErr )
				{
					/* */
					return ( CopyFileMgrAttributes(srcVRefNum, srcDirID, (StringPtr)srcName,
												 dstVRefNum, dstDirID, dstName, true) );
				}
			}
		}
	}

	/* If we're here, then PBHCopyFile couldn't be used so we have to copy the file by hand. */

	/* Make sure a copy buffer is allocated. */
	if ( copyBufferPtr == NULL )
	{
		/* The caller didn't supply a copy buffer so grab one from the application heap.
		** Try to get a big copy buffer, if we can't, try for a 512-byte buffer.
		** If 512 bytes aren't available, we're in trouble. */
		copyBufferSize = bigCopyBuffSize;
		copyBufferPtr = NewPtr(copyBufferSize);
		if ( copyBufferPtr == NULL )
		{
			copyBufferSize = minCopyBuffSize;
			copyBufferPtr = NewPtr(copyBufferSize);
			if ( copyBufferPtr == NULL )
				return ( memFullErr );
		}
		ourCopyBuffer = true;
	}

	/* Open the source data fork. */
	err = HOpenAware(srcVRefNum, srcDirID, srcName, srcCopyMode, &srcRefNum);
	if ( err != noErr )
		goto ErrorExit;
	
	/* See if the copy will be renamed. */
	if ( copyName != NULL )				/* Did caller supply copy file name? */
		BlockMoveData(copyName, dstName, copyName[0] + 1);	/* Yes, use the caller supplied copy file name. */
	else
	{	/* They didn't, so get the source file name and use it. */
		err = GetFileLocation(srcRefNum, &tempInt, &tempLong, (StringPtr)&dstName);
		if ( err != noErr )
			goto ErrorExit;
	}

	/* Create the destination file. */
	err = HCreateMinimum(dstVRefNum, dstDirID, dstName);
	if ( err != noErr )
		goto ErrorExit;
	dstCreated = true;	/* After creating the destination file, any
						** error conditions should delete the destination file */

	/* An AppleShare dropbox folder is a folder for which the user has the Make Changes
	** privilege (write access), but not See Files (read access) and See Folders (search access).
	** Copying a file into an AppleShare dropbox presents some special problems. Here are the
	** rules we have to follow to copy a file into a dropbox:
	** ¥ File attributes can be changed only when both forks of a file are empty.
	** ¥ DeskTop Manager comments can be added to a file only when both forks of a file 
	**   are empty.
	** ¥ A fork can be opened for write access only when both forks of a file are empty.
	** So, with those rules to live with, we'll do those operations now while both forks
	** are empty. */

	if ( isDropBox )
	{
		/* We only set the file attributes now if the file is being copied into a
		** drop box. In all other cases, it is better to set the attributes last
		** so that if FileCopy is modified to give up time to other processes
		** periodicly, the Finder won't try to read any bundle information (because
		** the bundle-bit will still be clear) from a partially copied file. If the
		** copy is into a drop box, we have to set the attributes now, but since the
		** destination forks are opened with write/deny-read/deny-write permissions,
		** any Finder that might see the file in the drop box won't be able to open
		** its resource fork until the resource fork is closed.
		**
		** Note: if you do modify FileCopy to give up time to other processes, don't
		** give up time between the time the destination file is created (above) and
		** the time both forks are opened (below). That way, you stand the best chance
		** of making sure the Finder doesn't read a partially copied resource fork.
		*/
		/* Copy attributes but don't lock the destination. */
		err = CopyFileMgrAttributes(srcVRefNum, srcDirID, (StringPtr)srcName,
									dstVRefNum, dstDirID, (StringPtr)&dstName, false);
		if ( err != noErr )
			goto ErrorExit;
	}

	/* Attempt to copy the comments while both forks are empty.
	** Ignore the result because we really don't care if it worked or not. */
	(void) DTCopyComment(srcVRefNum, srcDirID, (StringPtr)srcName, dstVRefNum, dstDirID, (StringPtr)&dstName);

	/* See which forks we need to copy. By doing this, we won't create a data or resource fork
	** for the destination unless it's really needed (some foreign file systems such as
	** the ProDOS File System and Macintosh PC Exchange have to create additional disk
	** structures to support resource forks). */
	err = CheckForForks(srcVRefNum, srcDirID, srcName, &hasDataFork, &hasResourceFork);
	if ( err != noErr )
		goto ErrorExit;	
	
	if ( hasDataFork )
	{
		/* Open the destination data fork. */
		err = HOpenAware(dstVRefNum, dstDirID, dstName, dstCopyMode, &dstDataRefNum);
		if ( err != noErr )
			goto ErrorExit;
	}

	if ( hasResourceFork )
	{
		/* Open the destination resource fork. */
		err = HOpenRFAware(dstVRefNum, dstDirID, dstName, dstCopyMode, &dstRsrcRefNum);
		if ( err != noErr )
			goto ErrorExit;
	}

	if ( hasDataFork )
	{
		/* Copy the data fork. */
		err = CopyFork(srcRefNum, dstDataRefNum, copyBufferPtr, copyBufferSize);
		if ( err != noErr )
			goto ErrorExit;
	
		/* Close both data forks and clear reference numbers. */
		FSClose(srcRefNum);
		FSClose(dstDataRefNum);
		srcRefNum = dstDataRefNum = 0;
	}
	else
	{
		/* Close the source data fork since it was opened earlier */
		FSClose(srcRefNum);
		srcRefNum = 0;
	}

	if ( hasResourceFork )
	{
		/* Open the source resource fork. */
		err = HOpenRFAware(srcVRefNum, srcDirID, srcName, srcCopyMode, &srcRefNum);
		if ( err != noErr )
			goto ErrorExit;
	
		/* Copy the resource fork. */
		err = CopyFork(srcRefNum, dstRsrcRefNum, copyBufferPtr, copyBufferSize);
		if ( err != noErr )
			goto ErrorExit;
	
		/* Close both resource forks and clear reference numbers. */
		FSClose(srcRefNum);
		FSClose(dstRsrcRefNum);
		srcRefNum = dstRsrcRefNum = 0;
	}

	/* Get rid of the copy buffer if we allocated it. */
	if ( ourCopyBuffer )
		DisposePtr((Ptr)copyBufferPtr);

	/* Attempt to copy attributes again to set mod date.  Copy lock condition this time
	** since we're done with the copy operation.  This operation will fail if we're copying
	** into an AppleShare dropbox, so we don't check for error conditions. */
	CopyFileMgrAttributes(srcVRefNum, srcDirID, (StringPtr)srcName,
							dstVRefNum, dstDirID, (StringPtr)&dstName, true);

	/* Hey, we did it! */
	return ( noErr );
	
ErrorExit:
	if ( srcRefNum != 0 )
		FSClose(srcRefNum);		/* Close the source file */
	if ( dstDataRefNum != 0 )
		FSClose(dstDataRefNum);	/* Close the destination file data fork */
	if ( dstRsrcRefNum != 0 )
		FSClose(dstRsrcRefNum);	/* Close the destination file resource fork */
	if ( dstCreated )
		HDelete(dstVRefNum, dstDirID, dstName);	/* Delete dest file.  This may fail if the file 
												   is in a "drop folder" */
	if ( ourCopyBuffer )	/* dispose of any memory we allocated */
		DisposePtr((Ptr)copyBufferPtr);
	return ( err );
}

/*****************************************************************************/

pascal	OSErr	FSpFileCopy(const FSSpec *srcSpec,
							const FSSpec *dstSpec,
							StringPtr copyName,
							void *copyBufferPtr,
							long copyBufferSize,
							Boolean preflight)
{
	return ( FileCopy(srcSpec->vRefNum, srcSpec->parID, srcSpec->name,
					 dstSpec->vRefNum, dstSpec->parID, (StringPtr)dstSpec->name,
					 copyName, copyBufferPtr, copyBufferSize, preflight) );
}

/*****************************************************************************/

