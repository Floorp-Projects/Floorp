/*
**	Apple Macintosh Developer Technical Support
**
**	DirectoryCopy: A robust, general purpose directory copy routine.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		DirectoryCopy.c
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
#include <Script.h>

#define	__COMPILINGMOREFILES

#include "MoreFiles.h"
#include "MoreFilesExtras.h"
#include "MoreDesktopMgr.h"
#include "FileCopy.h"
#include "DirectoryCopy.h"

/*****************************************************************************/

/* local constants */

enum
{
	dirCopyBigCopyBuffSize  = 0x00004000,
	dirCopyMinCopyBuffSize  = 0x00000200
};


/*****************************************************************************/

/* local data structures */

/* The EnumerateGlobals structure is used to minimize the amount of
** stack space used when recursively calling CopyLevel and to hold
** global information that might be needed at any time. */

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif
struct EnumerateGlobals
{
	Ptr			copyBuffer;			/* pointer to buffer used for file copy operations */
	long		bufferSize;			/* the size of the copy buffer */
	CopyErrProcPtr errorHandler;	/* pointer to error handling function */
	CopyFilterProcPtr copyFilterProc; /* pointer to filter function */
	OSErr		error;				/* temporary holder of results - saves 2 bytes of stack each level */
	Boolean		bailout;			/* set to true to by error handling function if fatal error */
	short		destinationVRefNum;	/* the destination vRefNum */
	Str63		itemName;			/* the name of the current item */
	CInfoPBRec	myCPB;				/* the parameter block used for PBGetCatInfo calls */
};
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

typedef struct EnumerateGlobals EnumerateGlobals;
typedef EnumerateGlobals *EnumerateGlobalsPtr;


/* The PreflightGlobals structure is used to minimize the amount of
** stack space used when recursively calling GetLevelSize and to hold
** global information that might be needed at any time. */

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif
struct PreflightGlobals
{
	OSErr			result;				/* temporary holder of results - saves 2 bytes of stack each level */
	Str63			itemName;			/* the name of the current item */
	CInfoPBRec		myCPB;				/* the parameter block used for PBGetCatInfo calls */

	unsigned short	dstBlksPerAllocBlk;	/* the number of 512 byte blocks per allocation block on destination */
	unsigned short	allocBlksNeeded;	/* the total number of allocation blocks needed  */

	unsigned short	tempBlocks;			/* temporary storage for calculations (save some stack space)  */
	CopyFilterProcPtr copyFilterProc;	/* pointer to filter function */
};
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

typedef struct PreflightGlobals PreflightGlobals;
typedef PreflightGlobals *PreflightGlobalsPtr;

/*****************************************************************************/

/* static prototypes */

static	void	GetLevelSize(long currentDirID,
							 PreflightGlobalsPtr theGlobals);

static	OSErr	PreflightDirectoryCopySpace(short srcVRefNum,
											long srcDirID,
											short dstVRefNum,
											CopyFilterProcPtr copyFilterProc,
											Boolean *spaceOK);

static	void	CopyLevel(long sourceDirID,
						  long dstDirID,
						  EnumerateGlobalsPtr theGlobals);
						  
/*****************************************************************************/

static	void	GetLevelSize(long currentDirID,
							 PreflightGlobalsPtr theGlobals)
{
	short	index = 1;
	
	do
	{
		theGlobals->myCPB.dirInfo.ioFDirIndex = index;
		theGlobals->myCPB.dirInfo.ioDrDirID = currentDirID;	/* we need to do this every time */
															/* through, since GetCatInfo  */
															/* returns ioFlNum in this field */
		theGlobals->result = PBGetCatInfoSync(&theGlobals->myCPB);
		if ( theGlobals->result == noErr )
		{
			if ( (theGlobals->copyFilterProc == NULL) ||
				 CallCopyFilterProc(theGlobals->copyFilterProc, &theGlobals->myCPB) ) /* filter if filter proc was supplied */
			{
				/* Either there's no filter proc OR the filter proc says to use this item */
				if ( (theGlobals->myCPB.dirInfo.ioFlAttrib & ioDirMask) != 0 )
				{
					/* we have a directory */
					
					GetLevelSize(theGlobals->myCPB.dirInfo.ioDrDirID, theGlobals); /* recurse */
					theGlobals->result = noErr; /* clear error return on way back */
				}
				else
				{
					/* we have a file - add its allocation blocks to allocBlksNeeded */
					
					/* get number of 512-byte blocks needed for data fork */
					theGlobals->tempBlocks = ((unsigned long)theGlobals->myCPB.hFileInfo.ioFlLgLen % 512) ?
												(((unsigned long)theGlobals->myCPB.hFileInfo.ioFlLgLen >> 9) + 1) :
												((unsigned long)theGlobals->myCPB.hFileInfo.ioFlLgLen >> 9);
					/* now, calculate number of new allocation blocks needed */
					theGlobals->allocBlksNeeded += (theGlobals->tempBlocks % theGlobals->dstBlksPerAllocBlk) ?
													((theGlobals->tempBlocks / theGlobals->dstBlksPerAllocBlk) + 1) :
													(theGlobals->tempBlocks / theGlobals->dstBlksPerAllocBlk);
					
					/* get number of 512-byte blocks needed for resource fork */
					theGlobals->tempBlocks = ((unsigned long)theGlobals->myCPB.hFileInfo.ioFlRLgLen % 512) ?
												(((unsigned long)theGlobals->myCPB.hFileInfo.ioFlRLgLen >> 9) + 1) :
												((unsigned long)theGlobals->myCPB.hFileInfo.ioFlRLgLen >> 9);
					/* now, calculate number of new allocation blocks needed */
					theGlobals->allocBlksNeeded += (theGlobals->tempBlocks % theGlobals->dstBlksPerAllocBlk) ?
													((theGlobals->tempBlocks / theGlobals->dstBlksPerAllocBlk) + 1) :
													(theGlobals->tempBlocks / theGlobals->dstBlksPerAllocBlk);
				}
			}
		}
		++index;
	} while ( theGlobals->result == noErr );
}

/*****************************************************************************/

static	OSErr	PreflightDirectoryCopySpace(short srcVRefNum,
											long srcDirID,
											short dstVRefNum,
											CopyFilterProcPtr copyFilterProc,
											Boolean *spaceOK)
{
	HParamBlockRec pb;
	OSErr error;
	unsigned short dstFreeBlocks;
	PreflightGlobals theGlobals;
	
	/* Get the number of 512 byte blocks per allocation block and */
	/* number of free allocation blocks on the destination volume */
	error = GetVolumeInfoNoName(NULL, dstVRefNum, &pb);
	if ( error == noErr )
	{
		dstFreeBlocks = (unsigned short)pb.volumeParam.ioVFrBlk;
		
		/* get allocation block size (always multiple of 512) and divide by 512
		  to get number of 512-byte blocks per allocation block */
		theGlobals.dstBlksPerAllocBlk = ((unsigned long)pb.volumeParam.ioVAlBlkSiz >> 9);
		
		theGlobals.allocBlksNeeded = 0;

		theGlobals.myCPB.dirInfo.ioNamePtr = theGlobals.itemName;
		theGlobals.myCPB.dirInfo.ioVRefNum = srcVRefNum;
		
		theGlobals.copyFilterProc = copyFilterProc;
		
		GetLevelSize(srcDirID, &theGlobals);
		
		/* Is there enough room on the destination volume for the source file? */
		*spaceOK = (theGlobals.allocBlksNeeded <= dstFreeBlocks);
	}
	return ( error );
}

/*****************************************************************************/

static	void	CopyLevel(long sourceDirID,
						  long dstDirID,
						  EnumerateGlobalsPtr theGlobals)
{
	long currentSrcDirID;
	long newDirID;
	short index = 1;
	
	do
	{	
		/* Get next source item at the current directory level */
		
		theGlobals->myCPB.dirInfo.ioFDirIndex = index;
		theGlobals->myCPB.dirInfo.ioDrDirID = sourceDirID;
		theGlobals->error = PBGetCatInfoSync(&theGlobals->myCPB);		

		if ( theGlobals->error == noErr )
		{
			if ( (theGlobals->copyFilterProc == NULL) ||
				 CallCopyFilterProc(theGlobals->copyFilterProc, &theGlobals->myCPB) ) /* filter if filter proc was supplied */
			{
				/* Either there's no filter proc OR the filter proc says to use this item */

				/* We have an item.  Is it a file or directory? */
				if ( (theGlobals->myCPB.hFileInfo.ioFlAttrib & ioDirMask) != 0 )
				{
					/* We have a directory */
					
					/* Create a new directory at the destination. No errors allowed! */
					theGlobals->error = DirCreate(theGlobals->destinationVRefNum, dstDirID, theGlobals->itemName, &newDirID);
					if ( theGlobals->error == noErr )
					{
						/* Save the current source directory ID where we can get it when we come back
						** from recursion land. */
						currentSrcDirID = theGlobals->myCPB.dirInfo.ioDrDirID;
						
						/* Dive again (copy the directory level we just found below this one) */
						CopyLevel(theGlobals->myCPB.dirInfo.ioDrDirID, newDirID, theGlobals);
						
						if ( !theGlobals->bailout )
						{
							/* Copy comment from old to new directory. */
							/* Ignore the result because we really don't care if it worked or not. */
							(void) DTCopyComment(theGlobals->myCPB.dirInfo.ioVRefNum, currentSrcDirID, NULL, theGlobals->destinationVRefNum, newDirID, NULL);
							
							/* Copy directory attributes (dates, etc.) to newDirID. */
							/* No errors allowed */
							theGlobals->error = CopyFileMgrAttributes(theGlobals->myCPB.dirInfo.ioVRefNum, currentSrcDirID, NULL, theGlobals->destinationVRefNum, newDirID, NULL, true);
							
							/* handle any errors from CopyFileMgrAttributes */
							if ( theGlobals->error != noErr )
							{
								if ( theGlobals->errorHandler != NULL )
								{
									theGlobals->bailout =  CallCopyErrProc(theGlobals->errorHandler, theGlobals->error, copyDirFMAttributesOp,
															theGlobals->myCPB.dirInfo.ioVRefNum, currentSrcDirID, NULL,
															theGlobals->destinationVRefNum, newDirID, NULL);
								}
								else
								{
									/* If you don't handle the errors with an error handler, */
									/* then the copy stops here. */
									theGlobals->bailout = true;
								}
							}
						}
					}
					else	/* error handling for DirCreate */
					{
						if ( theGlobals->errorHandler != NULL )
						{
							theGlobals->bailout = CallCopyErrProc(theGlobals->errorHandler, theGlobals->error, dirCreateOp,
														theGlobals->myCPB.dirInfo.ioVRefNum, currentSrcDirID, NULL,
														theGlobals->destinationVRefNum, dstDirID, theGlobals->itemName);
						}
						else
						{
							/* If you don't handle the errors with an error handler, */
							/* then the copy stops here. */
							theGlobals->bailout = true;
						}
					}
					
					if ( !theGlobals->bailout )
					{
						/* clear error return on way back if we aren't bailing out */
						theGlobals->error = noErr;
					}
				}
				else
				{
					/* We have a file, so copy it */
					
					theGlobals->error = FileCopy(theGlobals->myCPB.hFileInfo.ioVRefNum,
												 theGlobals->myCPB.hFileInfo.ioFlParID,
												 theGlobals->itemName,
												 theGlobals->destinationVRefNum,
												 dstDirID,
												 NULL,
												 NULL,
												 theGlobals->copyBuffer,
												 theGlobals->bufferSize,
												 false);
							
					/* handle any errors from FileCopy */
					if ( theGlobals->error != noErr )
					{
						if ( theGlobals->errorHandler != NULL )
						{
							theGlobals->bailout = CallCopyErrProc(theGlobals->errorHandler, theGlobals->error, fileCopyOp,
													theGlobals->myCPB.hFileInfo.ioVRefNum, theGlobals->myCPB.hFileInfo.ioFlParID, theGlobals->itemName,
													theGlobals->destinationVRefNum, dstDirID, NULL);
							if ( !theGlobals->bailout )
							{
								/* If the CopyErrProc handled the problem, clear the error here */
								theGlobals->error = noErr;
							}
						}
						else
						{
							/* If you don't handle the errors with an error handler, */
							/* then the copy stops here. */
							theGlobals->bailout = true;
						}
					}
				}
			}
		}
		else
		{	/* error handling for PBGetCatInfo */
			/* it's normal to get a fnfErr when indexing; that only means you've hit the end of the directory */
			if ( theGlobals->error != fnfErr )
			{
				if ( theGlobals->errorHandler != NULL )
				{ 
					theGlobals->bailout = CallCopyErrProc(theGlobals->errorHandler, theGlobals->error, getNextItemOp,
											theGlobals->myCPB.dirInfo.ioVRefNum, sourceDirID, NULL, 0, 0, NULL);
					if ( !theGlobals->bailout )
					{
						/* If the CopyErrProc handled the problem, clear the error here */
						theGlobals->error = noErr;
					}
				}
				else
				{
					/* If you don't handle the errors with an error handler, */
					/* then the copy stops here. */
					theGlobals->bailout = true;
				}
			}
		}
		++index; /* prepare to get next item */
	} while ( (theGlobals->error == noErr) && (!theGlobals->bailout) ); /* time to fall back a level? */
}

/*****************************************************************************/

pascal	OSErr	FilteredDirectoryCopy(short srcVRefNum,
									  long srcDirID,
									  StringPtr srcName,
									  short dstVRefNum,
									  long dstDirID,
									  StringPtr dstName,
									  void *copyBufferPtr,
									  long copyBufferSize,
									  Boolean preflight,
									  CopyErrProcPtr copyErrHandler,
									  CopyFilterProcPtr copyFilterProc)
{
	EnumerateGlobals theGlobals;
	Boolean	isDirectory;
	OSErr	error;
	Boolean ourCopyBuffer = false;
	Str63	srcDirName, oldDiskName;
	Boolean spaceOK;			
	
	/* Make sure a copy buffer is allocated. */
	if ( copyBufferPtr == NULL )
	{
		/* The caller didn't supply a copy buffer so grab one from the application heap.
		** Try to get a big copy buffer, if we can't, try for a 512-byte buffer.
		** If 512 bytes aren't available, we're in trouble. */
		copyBufferSize = dirCopyBigCopyBuffSize;
		copyBufferPtr = NewPtr(copyBufferSize);
		if ( copyBufferPtr == NULL )
		{
			copyBufferSize = dirCopyMinCopyBuffSize;
			copyBufferPtr = NewPtr(copyBufferSize);
			if ( copyBufferPtr == NULL )
				return ( memFullErr );
		}
		ourCopyBuffer = true;
	}
	
	/* Get the real dirID where we're copying from and make sure it is a directory. */
	error = GetDirectoryID(srcVRefNum, srcDirID, srcName, &srcDirID, &isDirectory);
	if ( error != noErr )
		goto ErrorExit;
	if ( !isDirectory )
	{
		error = dirNFErr;
		goto ErrorExit;
	}
	
	/* Special case destination if it is the root parent directory. */
	/* Since you can't create the root directory, this is needed if */
	/* you want to copy a directory's content to a disk's root directory. */
	if ( (dstDirID == fsRtParID) && (dstName == NULL) )
	{
		dstDirID = fsRtParID;
		isDirectory = true;
		error = noErr;
	}
	else
	{
		/*  Get the real dirID where we're going to put the copy and make sure it is a directory. */
		error = GetDirectoryID(dstVRefNum, dstDirID, dstName, &dstDirID, &isDirectory);
		if ( error != noErr )
			goto ErrorExit;
		if ( !isDirectory )
		{
			error =  dirNFErr;
			goto ErrorExit;
		}
	}
	
	/* Get the real vRefNum of both the source and destination */
	error = DetermineVRefNum(srcName, srcVRefNum, &srcVRefNum);
	if ( error != noErr )
		goto ErrorExit;
	error = DetermineVRefNum(dstName, dstVRefNum, &dstVRefNum);
	if ( error != noErr )
		goto ErrorExit;
	
	if ( preflight )
	{
		error = PreflightDirectoryCopySpace(srcVRefNum, srcDirID, dstVRefNum, copyFilterProc, &spaceOK);
		if ( error != noErr )
			goto ErrorExit;
		if ( !spaceOK )
		{
			error = dskFulErr; /* not enough room on destination */
			goto ErrorExit;
		}
	}

	/* Create the new directory in the destination directory with the */
	/* same name as the source directory. */
	error = GetDirName(srcVRefNum, srcDirID, srcDirName);
	if ( error != noErr )
		goto ErrorExit;
	
	/* Again, special case destination if the destination is the */
	/* root parent directory. This time, we'll rename the disk to */
	/* the source directory name. */
	if ( dstDirID == fsRtParID )
	{
		/* Get the current name of the destination disk */
		error = GetDirName(dstVRefNum, fsRtDirID, oldDiskName);
		if ( error == noErr )	
		{
			/* Shorten the name if it's too long to be the volume name */
			TruncPString(srcDirName, srcDirName, 27);
			
			/* Rename the disk */
			error = HRename(dstVRefNum, fsRtParID, oldDiskName, srcDirName);
			/* and copy to the root directory */
			dstDirID = fsRtDirID;
		}
	}
	else
	{
		error = DirCreate(dstVRefNum, dstDirID, srcDirName, &dstDirID);
	}
	if ( error != noErr )
	{
		/* handle any errors from DirCreate */
		if ( copyErrHandler != NULL )
		{
			if ( CallCopyErrProc(copyErrHandler, error, dirCreateOp,
													srcVRefNum, srcDirID, NULL,
													dstVRefNum, dstDirID, srcDirName) )
			{
				goto ErrorExit;
			}
			else
			{
				/* If the CopyErrProc handled the problem, clear the error here */
				/* and continue */
				error = noErr;
			}
		}
		else
		{
			/* If you don't handle the errors with an error handler, */
			/* then the copy stops here. */
			goto ErrorExit;
		}
	}
	
	/* dstDirID is now the newly created directory! */
		
	/* Set up the globals we need to access from the recursive routine. */
	theGlobals.copyBuffer = (Ptr)copyBufferPtr;
	theGlobals.bufferSize = copyBufferSize;
	theGlobals.destinationVRefNum = dstVRefNum; /* so we can get to it always */
	theGlobals.myCPB.hFileInfo.ioNamePtr = (StringPtr)&theGlobals.itemName;
	theGlobals.myCPB.hFileInfo.ioVRefNum = srcVRefNum;
	theGlobals.errorHandler = copyErrHandler;
	theGlobals.bailout = false;
	theGlobals.copyFilterProc =  copyFilterProc;
		
	/* Here we go into recursion land... */
	CopyLevel(srcDirID, dstDirID, &theGlobals);
	error = theGlobals.error;	/* get the result */
	
	if ( !theGlobals.bailout )
	{
		/* Copy comment from source to destination directory. */
		/* Ignore the result because we really don't care if it worked or not. */
		(void) DTCopyComment(srcVRefNum, srcDirID, NULL, dstVRefNum, dstDirID, NULL);
		
		/* Copy the File Manager attributes */
		error = CopyFileMgrAttributes(srcVRefNum, srcDirID, NULL,
					dstVRefNum, dstDirID, NULL, true);
		
		/* handle any errors from CopyFileMgrAttributes */
		if ( (error != noErr) && (copyErrHandler != NULL) )
			theGlobals.bailout = CallCopyErrProc(copyErrHandler, error, copyDirFMAttributesOp,
												srcVRefNum, srcDirID, NULL,
												dstVRefNum, dstDirID, NULL);
	}

ErrorExit:
	/* Get rid of the copy buffer if we allocated it. */
	if ( ourCopyBuffer )
		DisposePtr((Ptr)copyBufferPtr);

	return ( error );
}

/*****************************************************************************/

pascal	OSErr	DirectoryCopy(short srcVRefNum,
							  long srcDirID,
							  StringPtr srcName,
							  short dstVRefNum,
							  long dstDirID,
							  StringPtr dstName,
							  void *copyBufferPtr,
							  long copyBufferSize,
							  Boolean preflight,
							  CopyErrProcPtr copyErrHandler)
{
	return ( FilteredDirectoryCopy(srcVRefNum, srcDirID, srcName,
								   dstVRefNum, dstDirID, dstName,
								   copyBufferPtr, copyBufferSize, preflight,
								   copyErrHandler, NULL) );
}

/*****************************************************************************/

pascal	OSErr	FSpFilteredDirectoryCopy(const FSSpec *srcSpec,
										 const FSSpec *dstSpec,
										 void *copyBufferPtr,
										 long copyBufferSize,
										 Boolean preflight,
										 CopyErrProcPtr copyErrHandler,
										 CopyFilterProcPtr copyFilterProc)
{
	return ( FilteredDirectoryCopy(srcSpec->vRefNum, srcSpec->parID, (StringPtr)srcSpec->name,
								   dstSpec->vRefNum, dstSpec->parID, (StringPtr)dstSpec->name,
								   copyBufferPtr, copyBufferSize, preflight,
								   copyErrHandler, copyFilterProc) );
}

/*****************************************************************************/

pascal	OSErr	FSpDirectoryCopy(const FSSpec *srcSpec,
								 const FSSpec *dstSpec,
								 void *copyBufferPtr,
								 long copyBufferSize,
								 Boolean preflight,
								 CopyErrProcPtr copyErrHandler)
{
	return ( FilteredDirectoryCopy(srcSpec->vRefNum, srcSpec->parID, (StringPtr)srcSpec->name,
								   dstSpec->vRefNum, dstSpec->parID, (StringPtr)dstSpec->name,
								   copyBufferPtr, copyBufferSize, preflight,
								   copyErrHandler, NULL) );
}

/*****************************************************************************/

