/*
**	Apple Macintosh Developer Technical Support
**
**	A collection of useful high-level File Manager routines.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		MoreFilesExtras.c
**
**	Copyright © 1992-1998 Apple Computer, Inc.
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

/* 
 * This code, which was decended from Apple Sample Code, has been modified by 
 * Netscape.
 */

#include <Types.h>
#include <Traps.h>
#include <OSUtils.h>
#include <Errors.h>
#include <Files.h>
#include <Devices.h>
#include <Finder.h>
#include <Folders.h>
#include <FSM.h>
#include <Disks.h>
#include <Gestalt.h>
#include <TextUtils.h>
#include <Script.h>
#include <Script.h>
#include <stddef.h>

#define	__COMPILINGMOREFILES

#include "MoreFiles.h"
#include "MoreFilesExtras.h"
#include "MoreDesktopMgr.h"
#include "FSpCompat.h"

/*****************************************************************************/

/* local data structures */

/* The DeleteEnumGlobals structure is used to minimize the amount of
** stack space used when recursively calling DeleteLevel and to hold
** global information that might be needed at any time. */

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif
struct DeleteEnumGlobals
{
	OSErr			error;				/* temporary holder of results - saves 2 bytes of stack each level */
	Str63			itemName;			/* the name of the current item */
	UniversalFMPB	myPB;				/* the parameter block used for PBGetCatInfo calls */
};
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

typedef struct DeleteEnumGlobals DeleteEnumGlobals;
typedef DeleteEnumGlobals *DeleteEnumGlobalsPtr;

/*****************************************************************************/

pascal	void	TruncPString(StringPtr destination,
							 ConstStr255Param source,
							 short maxLength)
{
	short	charType;
	
	if ( source != NULL && destination != NULL )	/* don't do anything stupid */
	{
		if ( source[0] > maxLength )
		{
			/* Make sure the string isn't truncated in the middle of */
			/* a multi-byte character. */
			while (maxLength != 0)
			{
				charType = CharByte((Ptr)&source[1], maxLength);
				if ( (charType == smSingleByte) || (charType == smLastByte) )
					break;	/* source[maxLength] is now a valid last character */ 
				--maxLength;
			}
		}
		else
		{
			maxLength = source[0];
		}
		/* Set the destination string length */
		destination[0] = maxLength;
		/* and copy maxLength characters (if needed) */
		if ( source != destination )
		{
			while ( maxLength != 0 )
			{
				destination[maxLength] = source[maxLength];
				--maxLength;
			}
		}
	}
}

/*****************************************************************************/

pascal	Ptr	GetTempBuffer(long buffReqSize,
						  long *buffActSize)
{
	enum
	{
		kSlopMemory = 0x00008000	/* 32K - Amount of free memory to leave when allocating buffers */
	};
	Ptr	tempPtr;
	
	/* Make request a multiple of 1024 bytes */
	buffReqSize = buffReqSize & 0xfffffc00;
	
	if ( buffReqSize < 0x00000400 )
	{
		/* Request was smaller than 1024 bytes - make it 1024 */
		buffReqSize = 0x00000400;
	}
	
	/* Attempt to allocate the memory */
	tempPtr = NewPtr(buffReqSize);
	
	/* If request failed, go to backup plan */
	if ( (tempPtr == NULL) && (buffReqSize > 0x00000400) )
	{
		/*
		**	Try to get largest 1024-byte block available
		**	leaving some slop for the toolbox if possible
		*/
		long freeMemory = (FreeMem() - kSlopMemory) & 0xfffffc00;
		
		buffReqSize = MaxBlock() & 0xfffffc00;
		
		if ( buffReqSize > freeMemory )
		{
			buffReqSize = freeMemory;
		}
		
		if ( buffReqSize == 0 )
		{
			buffReqSize = 0x00000400;
		}
		
		tempPtr = NewPtr(buffReqSize);
	}
	
	/* Return bytes allocated */
	if ( tempPtr != NULL )
	{
		*buffActSize = buffReqSize;
	}
	else
	{
		*buffActSize = 0;
	}
	
	return ( tempPtr );
}

/*****************************************************************************/

/*
**	GetVolumeInfoNoName uses pathname and vRefNum to call PBHGetVInfoSync
**	in cases where the returned volume name is not needed by the caller.
**	The pathname and vRefNum parameters are not touched, and the pb
**	parameter is initialized by PBHGetVInfoSync except that ioNamePtr in
**	the parameter block is always returned as NULL (since it might point
**	to the local tempPathname).
**
**	I noticed using this code in several places, so here it is once.
**	This reduces the code size of MoreFiles.
*/
pascal	OSErr	GetVolumeInfoNoName(ConstStr255Param pathname,
									short vRefNum,
									HParmBlkPtr pb)
{
	Str255 tempPathname;
	OSErr error;
	
	/* Make sure pb parameter is not NULL */ 
	if ( pb != NULL )
	{
		pb->volumeParam.ioVRefNum = vRefNum;
		if ( pathname == NULL )
		{
			pb->volumeParam.ioNamePtr = NULL;
			pb->volumeParam.ioVolIndex = 0;		/* use ioVRefNum only */
		}
		else
		{
			BlockMoveData(pathname, tempPathname, pathname[0] + 1);	/* make a copy of the string and */
			pb->volumeParam.ioNamePtr = (StringPtr)tempPathname;	/* use the copy so original isn't trashed */
			pb->volumeParam.ioVolIndex = -1;	/* use ioNamePtr/ioVRefNum combination */
		}
		error = PBHGetVInfoSync(pb);
		pb->volumeParam.ioNamePtr = NULL;	/* ioNamePtr may point to local	tempPathname, so don't return it */
	}
	else
	{
		error = paramErr;
	}
	return ( error );
}

/*****************************************************************************/

/*
**	XGetVolumeInfoNoName uses pathname and vRefNum to call PBXGetVolInfoSync
**	in cases where the returned volume name is not needed by the caller.
**	The pathname and vRefNum parameters are not touched, and the pb
**	parameter is initialized by PBXGetVolInfoSync except that ioNamePtr in
**	the parameter block is always returned as NULL (since it might point
**	to the local tempPathname).
*/
pascal	OSErr	XGetVolumeInfoNoName(ConstStr255Param pathname,
									short vRefNum,
									XVolumeParamPtr pb)
{
	Str255 tempPathname;
#if	!__MACOSSEVENFIVEONEORLATER
/* shd 11/16/99 - Remove warning when __MACOSSEVENFIVEONEORLATER defined */
	long response;
#endif	// !__MACOSSEVENFIVEONEORLATER
	OSErr error;
	
	/* Make sure pb parameter is not NULL */ 
	if ( pb != NULL )
	{
		pb->ioVRefNum = vRefNum;
		pb->ioXVersion = 0;			/* this XVolumeParam version (0) */
		if ( pathname == NULL )
		{
			pb->ioNamePtr = NULL;
			pb->ioVolIndex = 0;		/* use ioVRefNum only */
		}
		else
		{
			BlockMoveData(pathname, tempPathname, pathname[0] + 1);	/* make a copy of the string and */
			pb->ioNamePtr = (StringPtr)tempPathname;	/* use the copy so original isn't trashed */
			pb->ioVolIndex = -1;	/* use ioNamePtr/ioVRefNum combination */
		}
#if	!__MACOSSEVENFIVEONEORLATER
		/* Is PBXGetVolInfo available? */
		if ( ( Gestalt(gestaltFSAttr, &response) != noErr ) || ((response & (1L << gestaltFSSupports2TBVols)) == 0) )
		{
			/* No, fall back on PBHGetVInfo */
			error = PBHGetVInfoSync((HParmBlkPtr)pb);
			if ( error == noErr )
			{
				/* calculate the ioVTotalBytes and ioVFreeBytes fields */
				pb->ioVTotalBytes.hi = 0;
				pb->ioVTotalBytes.lo = pb->ioVNmAlBlks * pb->ioVAlBlkSiz;	/* calculated total number of bytes on volume */
				pb->ioVFreeBytes.hi = 0;
				pb->ioVFreeBytes.lo = pb->ioVFrBlk * pb->ioVAlBlkSiz;	/* calculated number of free bytes on volume */
			}
		}
		else
#endif	// !__MACOSSEVENFIVEONEORLATER
		{
			/* Yes, so use it */
			error = PBXGetVolInfoSync(pb);
		}
		pb->ioNamePtr = NULL;		/* ioNamePtr may point to local	tempPathname, so don't return it */
	}
	else
	{
		error = paramErr;
	}
	return ( error );
}

/*****************************************************************************/

pascal	OSErr GetCatInfoNoName(short vRefNum,
							   long dirID,
							   ConstStr255Param name,
							   CInfoPBPtr pb)
{
	Str31 tempName;
	OSErr error;
	
	/* Protection against File Sharing problem */
	if ( (name == NULL) || (name[0] == 0) )
	{
		tempName[0] = 0;
		pb->dirInfo.ioNamePtr = tempName;
		pb->dirInfo.ioFDirIndex = -1;	/* use ioDirID */
	}
	else
	{
		pb->dirInfo.ioNamePtr = (StringPtr)name;
		pb->dirInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	}
	pb->dirInfo.ioVRefNum = vRefNum;
	pb->dirInfo.ioDrDirID = dirID;
	error = PBGetCatInfoSync(pb);
	pb->dirInfo.ioNamePtr = NULL;
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	DetermineVRefNum(ConstStr255Param pathname,
								 short vRefNum,
								 short *realVRefNum)
{
	HParamBlockRec pb;
	OSErr error;

	error = GetVolumeInfoNoName(pathname,vRefNum, &pb);
	if ( error == noErr )
	{
		*realVRefNum = pb.volumeParam.ioVRefNum;
	}
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	HGetVInfo(short volReference,
						  StringPtr volName,
						  short *vRefNum,
						  unsigned long *freeBytes,
						  unsigned long *totalBytes)
{
	HParamBlockRec	pb;
	unsigned long	allocationBlockSize;
	unsigned short	numAllocationBlocks;
	unsigned short	numFreeBlocks;
	VCB				*theVCB;
	Boolean			vcbFound;
	OSErr			result;
	
	/* Use the File Manager to get the real vRefNum */
	pb.volumeParam.ioVRefNum = volReference;
	pb.volumeParam.ioNamePtr = volName;
	pb.volumeParam.ioVolIndex = 0;	/* use ioVRefNum only, return volume name */
	result = PBHGetVInfoSync(&pb);
	
	if ( result == noErr )
	{
		/* The volume name was returned in volName (if not NULL) and */
		/* we have the volume's vRefNum and allocation block size */
		*vRefNum = pb.volumeParam.ioVRefNum;
		allocationBlockSize = (unsigned long)pb.volumeParam.ioVAlBlkSiz;
		
		/* System 7.5 (and beyond) pins the number of allocation blocks and */
		/* the number of free allocation blocks returned by PBHGetVInfo to */
		/* a value so that when multiplied by the allocation block size, */
		/* the volume will look like it has $7fffffff bytes or less. This */
		/* was done so older applications that use signed math or that use */
		/* the GetVInfo function (which uses signed math) will continue to work. */
		/* However, the unpinned numbers (which we want) are always available */
		/* in the volume's VCB so we'll get those values from the VCB if possible. */
		
		/* Find the volume's VCB */
		vcbFound = false;
#if !TARGET_CARBON
// pinkerton - GetVCBQHdr does not exist under Carbon. Not sure the best way to fix this
		theVCB = (VCB *)(GetVCBQHdr()->qHead);
		while ( (theVCB != NULL) && !vcbFound )
		{
			/* Check VCB signature before using VCB. Don't have to check for */
			/* MFS (0xd2d7) because they can't get big enough to be pinned */
			if ( theVCB->vcbSigWord == 0x4244 )
			{
				if ( theVCB->vcbVRefNum == *vRefNum )
				{
					vcbFound = true;
				}
			}
			
			if ( !vcbFound )
			{
				theVCB = (VCB *)(theVCB->qLink);
			}
		}
#else
		theVCB = NULL;
#endif			
		
		if ( theVCB != NULL )
		{
			/* Found a VCB we can use. Get the un-pinned number of allocation blocks */
			/* and the number of free blocks from the VCB. */
			numAllocationBlocks = (unsigned short)theVCB->vcbNmAlBlks;
			numFreeBlocks = (unsigned short)theVCB->vcbFreeBks;
		}
		else
		{
			/* Didn't find a VCB we can use. Return the number of allocation blocks */
			/* and the number of free blocks returned by PBHGetVInfoSync. */
			numAllocationBlocks = (unsigned short)pb.volumeParam.ioVNmAlBlks;
			numFreeBlocks = (unsigned short)pb.volumeParam.ioVFrBlk;
		}
		
		/* Now, calculate freeBytes and totalBytes using unsigned values */
		*freeBytes = numFreeBlocks * allocationBlockSize;
		*totalBytes = numAllocationBlocks * allocationBlockSize;
	}
	
	return ( result );
}

/*****************************************************************************/

/*
**	PBXGetVolInfoSync is the glue code needed to make PBXGetVolInfoSync
**	File Manager requests from CFM-based programs. At some point, Apple
**	will get around to adding this to the standard libraries you link with
**	and you'll get a duplicate symbol link error. At that time, just delete
**	this code (or comment it out).
**
**	Non-CFM 68K programs don't needs this glue (and won't get it) because
**	they instead use the inline assembly glue found in the Files.h interface
**	file.
**
**  NSCP - pinkerton
**  This code does not work under Carbon since NGetTrapAddress is not supported
**  Luckily, i don't think we use this routine at all.
*/

#if	__WANTPASCALELIMINATION
#undef	pascal
#endif

#if GENERATINGCFM && !TARGET_CARBON
pascal OSErr PBXGetVolInfoSync(XVolumeParamPtr paramBlock)
{
	enum
	{
		kXGetVolInfoSelector = 0x0012,	/* Selector for XGetVolInfo */
		
		uppFSDispatchProcInfo = kRegisterBased
			 | REGISTER_RESULT_LOCATION(kRegisterD0)
			 | RESULT_SIZE(SIZE_CODE(sizeof(OSErr)))
			 | REGISTER_ROUTINE_PARAMETER(1, kRegisterD0, SIZE_CODE(sizeof(long)))	/* selector */
			 | REGISTER_ROUTINE_PARAMETER(2, kRegisterD1, SIZE_CODE(sizeof(long)))	/* trap word */
			 | REGISTER_ROUTINE_PARAMETER(3, kRegisterA0, SIZE_CODE(sizeof(XVolumeParamPtr)))
	};
	
	return ( CallOSTrapUniversalProc(NGetTrapAddress(_FSDispatch, kOSTrapType),
										uppFSDispatchProcInfo,
										kXGetVolInfoSelector,
										_FSDispatch,
										paramBlock) );
}
#endif

#if	__WANTPASCALELIMINATION
#define	pascal	
#endif

/*****************************************************************************/

pascal	OSErr	XGetVInfo(short volReference,
						  StringPtr volName,
						  short *vRefNum,
						  UnsignedWide *freeBytes,
						  UnsignedWide *totalBytes)
{
	OSErr			result;
	long			response;
	XVolumeParam	pb;
	
	/* See if large volume support is available */
	if ( ( Gestalt(gestaltFSAttr, &response) == noErr ) && ((response & (1L << gestaltFSSupports2TBVols)) != 0) )
	{
		/* Large volume support is available */
		pb.ioVRefNum = volReference;
		pb.ioNamePtr = volName;
		pb.ioXVersion = 0;	/* this XVolumeParam version (0) */
		pb.ioVolIndex = 0;	/* use ioVRefNum only, return volume name */
		result = PBXGetVolInfoSync(&pb);
		if ( result == noErr )
		{
			/* The volume name was returned in volName (if not NULL) and */
			/* we have the volume's vRefNum and allocation block size */
			*vRefNum = pb.ioVRefNum;
			
			/* return the freeBytes and totalBytes */
#if TARGET_CARBON || (UNIVERSAL_INTERFACES_VERSION >= 0x0330)
			/* NSCP - API changes for Carbon */
			totalBytes->hi = pb.ioVTotalBytes & 0xFFFFFFFF00000000;
			totalBytes->lo = pb.ioVTotalBytes & 0x00000000FFFFFFFF;
			freeBytes->hi = pb.ioVFreeBytes & 0xFFFFFFFF00000000;
			freeBytes->lo = pb.ioVFreeBytes & 0x00000000FFFFFFFF;
#else
			*totalBytes = pb.ioVTotalBytes;
			*freeBytes = pb.ioVFreeBytes;
#endif
		}
	}
	else
	{
		/* No large volume support */
		
		/* Use HGetVInfo to get the results */
		result = HGetVInfo(volReference, volName, vRefNum, &freeBytes->lo, &totalBytes->lo);
		if ( result == noErr )
		{
			/* zero the high longs of totalBytes and freeBytes */
			totalBytes->hi = 0;
			freeBytes->hi = 0;
		}
	}
	return ( result );
}

/*****************************************************************************/

pascal	OSErr	CheckVolLock(ConstStr255Param pathname,
							 short vRefNum)
{
	HParamBlockRec pb;
	OSErr error;

	error = GetVolumeInfoNoName(pathname,vRefNum, &pb);
	if ( error == noErr )
	{
		if ( (pb.volumeParam.ioVAtrb & 0x0080) != 0 )
		{
			error = wPrErr;		/* volume locked by hardware */
		}
		else if ( (pb.volumeParam.ioVAtrb & 0x8000) != 0 )
		{
			error = vLckdErr;	/* volume locked by software */
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr GetDriverName(short driverRefNum,
							Str255 driverName)
{
	OSErr result;
	DCtlHandle theDctl;
	DRVRHeaderPtr dHeaderPtr;
	
	theDctl = GetDCtlEntry(driverRefNum);
	if ( theDctl != NULL )
	{
	    if ( (**theDctl).dCtlFlags & 0x40 )
	    {
	    	/* dctlDriver is handle - dereference */
			dHeaderPtr = *((DRVRHeaderHandle)(**theDctl).dCtlDriver);
	    }
	    else
	    {
			/* dctlDriver is pointer */
	      dHeaderPtr = (DRVRHeaderPtr)(**theDctl).dCtlDriver;
	    }
		BlockMoveData((*dHeaderPtr).drvrName, driverName, (*dHeaderPtr).drvrName[0] + 1);
		result = noErr;
	}
	else
	{
		driverName[0] = 0;
		result = badUnitErr;	/* bad reference number */
	}
	
	return ( result );
}

/*****************************************************************************/

pascal	OSErr	FindDrive(ConstStr255Param pathname,
						  short vRefNum,
						  DrvQElPtr *driveQElementPtr)
{
	OSErr			result;
	HParamBlockRec	hPB;
	short			driveNumber;
	
	*driveQElementPtr = NULL;
	
	/* First, use GetVolumeInfoNoName to determine the volume */
	result = GetVolumeInfoNoName(pathname, vRefNum, &hPB);
	if ( result == noErr )
	{
		/*
		**	The volume can be either online, offline, or ejected. What we find in
		**	ioVDrvInfo and ioVDRefNum will tell us which it is.
		**	See Inside Macintosh: Files page 2-80 and the Technical Note
		**	"FL 34 - VCBs and Drive Numbers : The Real Story"
		**	Where we get the drive number depends on the state of the volume.
		*/
		if ( hPB.volumeParam.ioVDrvInfo != 0 )
		{
			/* The volume is online and not ejected */
			/* Get the drive number */
			driveNumber = hPB.volumeParam.ioVDrvInfo;
		}
		else
		{
			/* The volume's is either offline or ejected */
			/* in either case, the volume is NOT online */

			/* Is it ejected or just offline? */
			if ( hPB.volumeParam.ioVDRefNum > 0 )
			{
				/* It's ejected, the drive number is ioVDRefNum */
				driveNumber = hPB.volumeParam.ioVDRefNum;
			}
			else
			{
				/* It's offline, the drive number is the negative of ioVDRefNum */
				driveNumber = (short)-hPB.volumeParam.ioVDRefNum;
			}
		}
		
		/* Get pointer to first element in drive queue */
		*driveQElementPtr = (DrvQElPtr)(GetDrvQHdr()->qHead);
		
		/* Search for a matching drive number */
		while ( (*driveQElementPtr != NULL) && ((*driveQElementPtr)->dQDrive != driveNumber) )
		{
			*driveQElementPtr = (DrvQElPtr)(*driveQElementPtr)->qLink;
		}
		
		if ( *driveQElementPtr == NULL )
		{
			/* This should never happen since every volume must have a drive, but... */
			result = nsDrvErr;
		}
	}
	
	return ( result );
}

/*****************************************************************************/

pascal	OSErr	GetDiskBlocks(ConstStr255Param pathname,
							  short vRefNum,
							  unsigned long *numBlocks)
{
	/* Various constants for GetDiskBlocks() */
	enum
	{
		/* return format list status code */
		kFmtLstCode = 6,
		
		/* reference number of .SONY driver */
		kSonyRefNum = 0xfffb,
		
		/* values returned by DriveStatus in DrvSts.twoSideFmt */
		kSingleSided = 0,
		kDoubleSided = -1,
		kSingleSidedSize = 800,		/* 400K */
		kDoubleSidedSize = 1600,	/* 800K */
		
		/* values in DrvQEl.qType */
		kWordDrvSiz = 0,
		kLongDrvSiz = 1,
		
		/* more than enough formatListRecords */
		kMaxFormatListRecs = 16
	};
	
	DrvQElPtr		driveQElementPtr;
	unsigned long	blocks;
	ParamBlockRec	pb;
	FormatListRec	formatListRecords[kMaxFormatListRecs];
	DrvSts			status;
	short			formatListRecIndex;
	OSErr			result;

	blocks = 0;
	
	/* Find the drive queue element for this volume */
	result = FindDrive(pathname, vRefNum, &driveQElementPtr);
	
	/* 
	**	Make sure this is a real driver (dQRefNum < 0).
	**	AOCE's Mail Enclosures volume uses 0 for dQRefNum which will cause
	**	problems if you try to use it as a driver refNum.
	*/ 
	if ( (result == noErr) && (driveQElementPtr->dQRefNum >= 0) )
	{
		result = paramErr;
	}
	else
	{
		/* Attempt to get the drive's format list. */
		/* (see the Technical Note "What Your Sony Drives For You") */
		
		pb.cntrlParam.ioVRefNum = driveQElementPtr->dQDrive;
		pb.cntrlParam.ioCRefNum = driveQElementPtr->dQRefNum;
		pb.cntrlParam.csCode = kFmtLstCode;
		pb.cntrlParam.csParam[0] = kMaxFormatListRecs;
		*(long *)&pb.cntrlParam.csParam[1] = (long)&formatListRecords[0];
		
		result = PBStatusSync(&pb);
		
		if ( result == noErr )
		{
			/* The drive supports ReturnFormatList status call. */
			
			/* Get the current disk's size. */
			for( formatListRecIndex = 0;
				 formatListRecIndex < pb.cntrlParam.csParam[0];
	    		 ++formatListRecIndex )
	    	{
	    		if ( (formatListRecords[formatListRecIndex].formatFlags &
	    			  diCIFmtFlagsCurrentMask) != 0 )
	    		{
	    			blocks = formatListRecords[formatListRecIndex].volSize;
	    		}
			}
    		if ( blocks == 0 )
    		{
    			/* This should never happen */
    			result = paramErr;
    		}
		}
		else if ( driveQElementPtr->dQRefNum == (short)kSonyRefNum )
		{
			/* The drive is a non-SuperDrive floppy which only supports 400K and 800K disks */
			
			result = DriveStatus(driveQElementPtr->dQDrive, &status);
			if ( result == noErr )
			{
				switch ( status.twoSideFmt )
				{
				case kSingleSided:
					blocks = kSingleSidedSize;
					break;
				case kDoubleSided:
					blocks = kDoubleSidedSize;
					break;
				default:
					/* This should never happen */
					result = paramErr;
					break;
				}
			}
		}
		else
		{
			/* The drive is not a floppy and it doesn't support ReturnFormatList */
			/* so use the dQDrvSz field(s) */
			
			result = noErr;	/* reset result */
			switch ( driveQElementPtr->qType )
			{
			case kWordDrvSiz:
				blocks = driveQElementPtr->dQDrvSz;
				break;
			case kLongDrvSiz:
				blocks = ((unsigned long)driveQElementPtr->dQDrvSz2 << 16) +
						 driveQElementPtr->dQDrvSz;
				break;
			default:
				/* This should never happen */
				result = paramErr;
				break;
			}
		}
	}
	
	if ( result == noErr )
	{
		*numBlocks = blocks;
	}
	
	return ( result );
}

/*****************************************************************************/

pascal	OSErr	GetVolFileSystemID(ConstStr255Param pathname,
								   short vRefNum,
								   short *fileSystemID)
{
	HParamBlockRec pb;
	OSErr error;

	error = GetVolumeInfoNoName(pathname,vRefNum, &pb);
	if ( error == noErr )
	{
		*fileSystemID = pb.volumeParam.ioVFSID;
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	GetVolState(ConstStr255Param pathname,
							short vRefNum,
							Boolean *volumeOnline,
							Boolean *volumeEjected,
							Boolean *driveEjectable,
							Boolean *driverWantsEject)
{
	HParamBlockRec pb;
	short driveNumber;
	OSErr error;

	error = GetVolumeInfoNoName(pathname,vRefNum, &pb);
	if ( error == noErr )
	{
		if ( pb.volumeParam.ioVDrvInfo != 0 )
		{
			/* the volume is online and not ejected */
			*volumeOnline = true;
			*volumeEjected = false;
			
			/* Get the drive number */
			driveNumber = pb.volumeParam.ioVDrvInfo;
		}
		else
		{
			/* the volume's is either offline or ejected */
			/* in either case, the volume is NOT online */
			*volumeOnline = false;

			/* Is it ejected? */
			*volumeEjected = pb.volumeParam.ioVDRefNum > 0;
			
			if ( *volumeEjected )
			{
				/* If ejected, the drive number is ioVDRefNum */
				driveNumber = pb.volumeParam.ioVDRefNum;
			}
			else
			{
				/* If offline, the drive number is the negative of ioVDRefNum */
				driveNumber = (short)-pb.volumeParam.ioVDRefNum;
			}
		}
		
		{
			DrvQElPtr drvQElem;
			
			/* Find the drive queue element by searching the drive queue */
			drvQElem = (DrvQElPtr)(GetDrvQHdr()->qHead);
			while ( (drvQElem != NULL) && (drvQElem->dQDrive != driveNumber) )
			{
				drvQElem = (DrvQElPtr)drvQElem->qLink;
			}
			
			if ( drvQElem != NULL )
			{
				/*
				**	Each drive queue element is preceded by 4 flag bytes.
				**	Byte 1 (the second flag byte) has bits that tell us if a
				**	drive is ejectable and if its driver wants an eject call.
				**	See Inside Macintosh: Files, page 2-85.
				*/
				{
					Ptr		flagBytePtr;
					
					/* point to byte 1 of the flag bytes */
					flagBytePtr = (Ptr)drvQElem;
					flagBytePtr -= 3;
					
					/*
					**	The drive is ejectable if flag byte 1 does not contain
					**	0x08 (nonejectable) or 0x48 (nonejectable, but wants eject call).
					*/
					
					*driveEjectable = (*flagBytePtr != 0x08) && (*flagBytePtr != 0x48);
					
					/*
					**	The driver wants an eject call if flag byte 1 does not contain
					**	0x08 (nonejectable). This may seem like a minor point, but some
					**	disk drivers use the Eject request to flush their caches to disk
					**	and you wouldn't want to skip that step after unmounting a volume.
					*/
					
					*driverWantsEject = (*flagBytePtr != 0x08);
				}
			}
			else
			{
				/* Didn't find the drive (this should never happen) */
				*driveEjectable = false;
				*driverWantsEject = false;
			}
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	UnmountAndEject(ConstStr255Param pathname,
								short vRefNum)
{
	HParamBlockRec pb;
	short driveNum;
	Boolean ejected, wantsEject;
	DrvQElPtr drvQElem;
	OSErr error;

	error = GetVolumeInfoNoName(pathname, vRefNum, &pb);
	if ( error == noErr )
	{
		if ( pb.volumeParam.ioVDrvInfo != 0 )
		{
			/* the volume is online and not ejected */
			ejected = false;
			
			/* Get the drive number */
			driveNum = pb.volumeParam.ioVDrvInfo;
		}
		else
		{
			/* the volume is ejected or offline */
			
			/* Is it ejected? */
			ejected = pb.volumeParam.ioVDRefNum > 0;
			
			if ( ejected )
			{
				/* If ejected, the drive number is ioVDRefNum */
				driveNum = pb.volumeParam.ioVDRefNum;
			}
			else
			{
				/* If offline, the drive number is the negative of ioVDRefNum */
				driveNum = (short)-pb.volumeParam.ioVDRefNum;
			}
		}
		
		/* find the drive queue element */
		drvQElem = (DrvQElPtr)(GetDrvQHdr()->qHead);
		while ( (drvQElem != NULL) && (drvQElem->dQDrive != driveNum) )
		{
			drvQElem = (DrvQElPtr)drvQElem->qLink;
		}
		
		if ( drvQElem != NULL )
		{
			/* does the drive want an eject call */
			wantsEject = (*((Ptr)((Ptr)drvQElem - 3)) != 8);
		}
		else
		{
			/* didn't find the drive!! */
			wantsEject = false;
		}
		
		/* unmount the volume */
		pb.volumeParam.ioNamePtr = NULL;
		/* ioVRefNum is already filled in from PBHGetVInfo */
		error = PBUnmountVol((ParmBlkPtr)&pb);
		if ( error == noErr )
		{
			if ( wantsEject && !ejected )
			{
				/* eject the media from the drive if needed */
				pb.volumeParam.ioVRefNum = driveNum;
				error = PBEject((ParmBlkPtr)&pb);
			}
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	OnLine(FSSpecPtr volumes,
					   short reqVolCount,
					   short *actVolCount,
					   short *volIndex)
{
	HParamBlockRec pb;
	OSErr error = noErr;
	FSSpec *endVolArray;

	if ( *volIndex > 0 )
	{
		*actVolCount = 0;
		for ( endVolArray = volumes + reqVolCount; (volumes < endVolArray) && (error == noErr); ++volumes )
		{
			pb.volumeParam.ioNamePtr = (StringPtr) & volumes->name;
			pb.volumeParam.ioVolIndex = *volIndex;
			error = PBHGetVInfoSync(&pb);
			if ( error == noErr )
			{
				volumes->parID = fsRtParID;		/* the root directory's parent is 1 */
				volumes->vRefNum = pb.volumeParam.ioVRefNum;
				++*volIndex;
				++*actVolCount;
			}
		}
	}
	else
	{
		error = paramErr;
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr SetDefault(short newVRefNum,
						 long newDirID,
						 short *oldVRefNum,
						 long *oldDirID)
{
	OSErr	error;
	
	/* Get the current default volume/directory. */
	error = HGetVol(NULL, oldVRefNum, oldDirID);
	if ( error == noErr )
	{
		/* Set the new default volume/directory */
		error = HSetVol(NULL, newVRefNum, newDirID);
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr RestoreDefault(short oldVRefNum,
							 long oldDirID)
{
	OSErr	error;
	short	defaultVRefNum;
	long	defaultDirID;
	long	defaultProcID;
	
	/* Determine if the default volume was a wdRefNum. */
	error = GetWDInfo(oldVRefNum, &defaultVRefNum, &defaultDirID, &defaultProcID);
	if ( error == noErr )
	{
		/* Restore the old default volume/directory, one way or the other. */
		if ( defaultDirID != fsRtDirID )
		{
			/* oldVRefNum was a wdRefNum - use SetVol */
			error = SetVol(NULL, oldVRefNum);
		}
		else
		{
			/* oldVRefNum was a real vRefNum - use HSetVol */
			error = HSetVol(NULL, oldVRefNum, oldDirID);
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr GetDInfo(short vRefNum,
					   long dirID,
					   ConstStr255Param name,
					   DInfo *fndrInfo)
{
	CInfoPBRec pb;
	OSErr error;
	
	error = GetCatInfoNoName(vRefNum, dirID, name, &pb);
	if ( error == noErr )
	{
		if ( (pb.dirInfo.ioFlAttrib & ioDirMask) != 0 )
		{
			/* it's a directory, return the DInfo */
			*fndrInfo = pb.dirInfo.ioDrUsrWds;
		}
		else
		{
			/* oops, a file was passed */
			error = dirNFErr;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr FSpGetDInfo(const FSSpec *spec,
						  DInfo *fndrInfo)
{
	return ( GetDInfo(spec->vRefNum, spec->parID, spec->name, fndrInfo) );
}

/*****************************************************************************/

pascal	OSErr SetDInfo(short vRefNum,
					   long dirID,
					   ConstStr255Param name,
					   const DInfo *fndrInfo)
{
	CInfoPBRec pb;
	Str31 tempName;
	OSErr error;

	/* Protection against File Sharing problem */
	if ( (name == NULL) || (name[0] == 0) )
	{
		tempName[0] = 0;
		pb.dirInfo.ioNamePtr = tempName;
		pb.dirInfo.ioFDirIndex = -1;	/* use ioDirID */
	}
	else
	{
		pb.dirInfo.ioNamePtr = (StringPtr)name;
		pb.dirInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	}
	pb.dirInfo.ioVRefNum = vRefNum;
	pb.dirInfo.ioDrDirID = dirID;
	error = PBGetCatInfoSync(&pb);
	if ( error == noErr )
	{
		if ( (pb.dirInfo.ioFlAttrib & ioDirMask) != 0 )
		{
			/* it's a directory, set the DInfo */
			if ( pb.dirInfo.ioNamePtr == tempName )
			{
				pb.dirInfo.ioDrDirID = pb.dirInfo.ioDrParID;
			}
			else
			{
				pb.dirInfo.ioDrDirID = dirID;
			}
			pb.dirInfo.ioDrUsrWds = *fndrInfo;
			error = PBSetCatInfoSync(&pb);
		}
		else
		{
			/* oops, a file was passed */
			error = dirNFErr;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr FSpSetDInfo(const FSSpec *spec,
						  const DInfo *fndrInfo)
{
	return ( SetDInfo(spec->vRefNum, spec->parID, spec->name, fndrInfo) );
}

/*****************************************************************************/

pascal	OSErr	GetDirectoryID(short vRefNum,
							   long dirID,
							   ConstStr255Param name,
							   long *theDirID,
							   Boolean *isDirectory)
{
	CInfoPBRec pb;
	OSErr error;

	error = GetCatInfoNoName(vRefNum, dirID, name, &pb);
	if ( error == noErr )
	{
		*isDirectory = (pb.hFileInfo.ioFlAttrib & ioDirMask) != 0;
		if ( *isDirectory )
		{
			*theDirID = pb.dirInfo.ioDrDirID;
		}
		else
		{
			*theDirID = pb.hFileInfo.ioFlParID;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpGetDirectoryID(const FSSpec *spec,
								  long *theDirID,
								  Boolean *isDirectory)
{
	return ( GetDirectoryID(spec->vRefNum, spec->parID, spec->name,
			 theDirID, isDirectory) );
}

/*****************************************************************************/

pascal	OSErr	GetDirName(short vRefNum,
						   long dirID,
						   Str31 name)
{
	CInfoPBRec pb;
	OSErr error;

	if ( name != NULL )
	{
		pb.dirInfo.ioNamePtr = name;
		pb.dirInfo.ioVRefNum = vRefNum;
		pb.dirInfo.ioDrDirID = dirID;
		pb.dirInfo.ioFDirIndex = -1;	/* get information about ioDirID */
		error = PBGetCatInfoSync(&pb);
	}
	else
	{
		error = paramErr;
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	GetIOACUser(short vRefNum,
							long dirID,
							ConstStr255Param name,
							SInt8 *ioACUser)
{
	CInfoPBRec pb;
	OSErr error;
	
	/* Clear ioACUser before calling PBGetCatInfo since some file systems
	** don't bother to set or clear this field. If ioACUser isn't set by the
	** file system, then you'll get the zero value back (full access) which
	** is the access you have on volumes that don't support ioACUser.
	*/
	pb.dirInfo.ioACUser = 0;	/* ioACUser used to be filler2 */
	error = GetCatInfoNoName(vRefNum, dirID, name, &pb);
	if ( error == noErr )
	{
		if ( (pb.hFileInfo.ioFlAttrib & ioDirMask) == 0 )
		{
			/* oops, a file was passed */
			error = dirNFErr;
		}
		else
		{
			*ioACUser = pb.dirInfo.ioACUser;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpGetIOACUser(const FSSpec *spec,
							   SInt8 *ioACUser)
{
	return ( GetIOACUser(spec->vRefNum, spec->parID, spec->name, ioACUser) );
}

/*****************************************************************************/

pascal	OSErr	GetParentID(short vRefNum,
							long dirID,
							ConstStr255Param name,
							long *parID)
{
	CInfoPBRec pb;
	Str31 tempName;
	OSErr error;
	short realVRefNum;
	
	/* Protection against File Sharing problem */
	if ( (name == NULL) || (name[0] == 0) )
	{
		tempName[0] = 0;
		pb.hFileInfo.ioNamePtr = tempName;
		pb.hFileInfo.ioFDirIndex = -1;	/* use ioDirID */
	}
	else
	{
		pb.hFileInfo.ioNamePtr = (StringPtr)name;
		pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	}
	pb.hFileInfo.ioVRefNum = vRefNum;
	pb.hFileInfo.ioDirID = dirID;
	error = PBGetCatInfoSync(&pb);
	if ( error == noErr )
	{
		/*
		**	There's a bug in HFS where the wrong parent dir ID can be
		**	returned if multiple separators are used at the end of a
		**	pathname. For example, if the pathname:
		**		'volumeName:System Folder:Extensions::'
		**	is passed, the directory ID of the Extensions folder is
		**	returned in the ioFlParID field instead of fsRtDirID. Since
		**	multiple separators at the end of a pathname always specifies
		**	a directory, we only need to work-around cases where the
		**	object is a directory and there are multiple separators at
		**	the end of the name parameter.
		*/
		if ( (pb.hFileInfo.ioFlAttrib & ioDirMask) != 0 )
		{
			/* Its a directory */
			
			/* is there a pathname? */
			if ( pb.hFileInfo.ioNamePtr == name )	
			{
				/* could it contain multiple separators? */
				if ( name[0] >= 2 )
				{
					/* does it contain multiple separators at the end? */
					if ( (name[name[0]] == ':') && (name[name[0] - 1] == ':') )
					{
						/* OK, then do the extra stuff to get the correct parID */
						
						/* Get the real vRefNum (this should not fail) */
						error = DetermineVRefNum(name, vRefNum, &realVRefNum);
						if ( error == noErr )
						{
							/* we don't need the parent's name, but add protect against File Sharing problem */
							tempName[0] = 0;
							pb.dirInfo.ioNamePtr = tempName;
							pb.dirInfo.ioVRefNum = realVRefNum;
							/* pb.dirInfo.ioDrDirID already contains the */
							/* dirID of the directory object */
							pb.dirInfo.ioFDirIndex = -1;	/* get information about ioDirID */
							error = PBGetCatInfoSync(&pb);
							/* now, pb.dirInfo.ioDrParID contains the correct parID */
						}
					}
				}
			}
		}
		
		if ( error == noErr )
		{
			/* if no errors, then pb.hFileInfo.ioFlParID (pb.dirInfo.ioDrParID) */
			/* contains the parent ID */
			*parID = pb.hFileInfo.ioFlParID;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	GetFilenameFromPathname(ConstStr255Param pathname,
										Str255 filename)
{
	short	index;
	short	nameEnd;
	OSErr	error;

	/* default to no filename */
	filename[0] = 0;

	/* check for no pathname */
	if ( pathname != NULL )
	{
		/* get string length */
		index = pathname[0];
		
		/* check for empty string */
		if ( index != 0 )
		{
			/* skip over last trailing colon (if any) */
			if ( pathname[index] == ':' )
			{
				--index;
			}

			/* save the end of the string */
			nameEnd = index;

			/* if pathname ends with multiple colons, then this pathname refers */
			/* to a directory, not a file */
			if ( pathname[index] != ':' )
			{
				/* parse backwards until we find a colon or hit the beginning of the pathname */
				while ( (index != 0) && (pathname[index] != ':') )
				{
					--index;
				}
				
				/* if we parsed to the beginning of the pathname and the pathname ended */
				/* with a colon, then pathname is a full pathname to a volume, not a file */
				if ( (index != 0) || (pathname[pathname[0]] != ':') )
				{
					/* get the filename and return noErr */
					filename[0] = (char)(nameEnd - index);
					BlockMoveData(&pathname[index+1], &filename[1], nameEnd - index);
					error = noErr;
				}
				else
				{
					/* pathname to a volume, not a file */
					error = notAFileErr;
				}
			}
			else
			{
				/* directory, not a file */
				error = notAFileErr;
			}
		}
		else
		{
			/* empty string isn't a file */
			error = notAFileErr;
		}
	}
	else
	{
		/* NULL pathname isn't a file */
		error = notAFileErr;
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	GetObjectLocation(short vRefNum,
								  long dirID,
								  ConstStr255Param pathname,
								  short *realVRefNum,
								  long *realParID,
								  Str255 realName,
								  Boolean *isDirectory)
{
	OSErr error;
	CInfoPBRec pb;
	Str255 tempPathname;
	
	/* clear results */
	*realVRefNum = 0;
	*realParID = 0;
	realName[0] = 0;
	
	/*
	**	Get the real vRefNum
	*/
	error = DetermineVRefNum(pathname, vRefNum, realVRefNum);
	if ( error == noErr )
	{
		/*
		**	Determine if the object already exists and if so,
		**	get the real parent directory ID if it's a file
		*/
		
		/* Protection against File Sharing problem */
		if ( (pathname == NULL) || (pathname[0] == 0) )
		{
			tempPathname[0] = 0;
			pb.hFileInfo.ioNamePtr = tempPathname;
			pb.hFileInfo.ioFDirIndex = -1;	/* use ioDirID */
		}
		else
		{
			pb.hFileInfo.ioNamePtr = (StringPtr)pathname;
			pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
		}
		pb.hFileInfo.ioVRefNum = vRefNum;
		pb.hFileInfo.ioDirID = dirID;
		error = PBGetCatInfoSync(&pb);
		if ( error == noErr )
		{
			/*
			**	The file system object is present and we have the file's real parID
			*/
			
			/*	Is it a directory or a file? */
			*isDirectory = (pb.hFileInfo.ioFlAttrib & ioDirMask) != 0;
			if ( *isDirectory )
			{
				/*
				**	It's a directory, get its name and parent dirID, and then we're done
				*/
				
				pb.dirInfo.ioNamePtr = realName;
				pb.dirInfo.ioVRefNum = *realVRefNum;
				/* pb.dirInfo.ioDrDirID already contains the dirID of the directory object */
				pb.dirInfo.ioFDirIndex = -1;	/* get information about ioDirID */
				error = PBGetCatInfoSync(&pb);
				
				/* get the parent ID here, because the file system can return the */
				/* wrong parent ID from the last call. */
				*realParID = pb.dirInfo.ioDrParID;
			}
			else
			{
				/*
				**	It's a file - use the parent directory ID from the last call
				**	to GetCatInfoparse, get the file name, and then we're done
				*/
				*realParID = pb.hFileInfo.ioFlParID;	
				error = GetFilenameFromPathname(pathname, realName);
			}
		}
		else if ( error == fnfErr )
		{
			/*
			**	The file system object is not present - see if its parent is present
			*/
			
			/*
			**	Parse to get the object name from end of pathname
			*/
			error = GetFilenameFromPathname(pathname, realName);
			
			/* if we can't get the object name from the end, we can't continue */
			if ( error == noErr )
			{
				/*
				**	What we want now is the pathname minus the object name
				**	for example:
				**	if pathname is 'vol:dir:file' tempPathname becomes 'vol:dir:'
				**	if pathname is 'vol:dir:file:' tempPathname becomes 'vol:dir:'
				**	if pathname is ':dir:file' tempPathname becomes ':dir:'
				**	if pathname is ':dir:file:' tempPathname becomes ':dir:'
				**	if pathname is ':file' tempPathname becomes ':'
				**	if pathname is 'file or file:' tempPathname becomes ''
				*/
				
				/* get a copy of the pathname */
				BlockMoveData(pathname, tempPathname, pathname[0] + 1);
				
				/* remove the object name */
				tempPathname[0] -= realName[0];
				/* and the trailing colon (if any) */
				if ( pathname[pathname[0]] == ':' )
				{
					--tempPathname[0];
				}
				
				/* OK, now get the parent's directory ID */
				
				/* Protection against File Sharing problem */
				pb.hFileInfo.ioNamePtr = (StringPtr)tempPathname;
				if ( tempPathname[0] != 0 )
				{
					pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
				}
				else
				{
					pb.hFileInfo.ioFDirIndex = -1;	/* use ioDirID */
				}
				pb.hFileInfo.ioVRefNum = vRefNum;
				pb.hFileInfo.ioDirID = dirID;
				error = PBGetCatInfoSync(&pb);
				*realParID = pb.dirInfo.ioDrDirID;

				*isDirectory = false;	/* we don't know what the object is really going to be */
			}
			
			if ( error != noErr )
			{
				error = dirNFErr;	/* couldn't find parent directory */
			}
			else
			{
				error = fnfErr;	/* we found the parent, but not the file */
			}
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	GetDirItems(short vRefNum,
							long dirID,
							ConstStr255Param name,
							Boolean getFiles,
							Boolean getDirectories,
							FSSpecPtr items,
							short reqItemCount,
							short *actItemCount,
							short *itemIndex) /* start with 1, then use what's returned */
{
	CInfoPBRec pb;
	OSErr error;
	long theDirID;
	Boolean isDirectory;
	FSSpec *endItemsArray;
	
	if ( *itemIndex > 0 )
	{
		/* NOTE: If I could be sure that the caller passed a real vRefNum and real directory */
		/* to this routine, I could rip out calls to DetermineVRefNum and GetDirectoryID and this */
		/* routine would be much faster because of the overhead of DetermineVRefNum and */
		/* GetDirectoryID and because GetDirectoryID blows away the directory index hint the Macintosh */
		/* file system keeps for indexed calls. I can't be sure, so for maximum throughput, */
		/* pass a big array of FSSpecs so you can get the directory's contents with few calls */
		/* to this routine. */
		
		/* get the real volume reference number */
		error = DetermineVRefNum(name, vRefNum, &pb.hFileInfo.ioVRefNum);
		if ( error == noErr )
		{
			/* and the real directory ID of this directory (and make sure it IS a directory) */
			error = GetDirectoryID(vRefNum, dirID, name, &theDirID, &isDirectory);
			if ( error == noErr )
			{
				if ( isDirectory )
				{
					*actItemCount = 0;
					endItemsArray = items + reqItemCount;
					while ( (items < endItemsArray) && (error == noErr) )
					{
						pb.hFileInfo.ioNamePtr = (StringPtr) &items->name;
						pb.hFileInfo.ioDirID = theDirID;
						pb.hFileInfo.ioFDirIndex = *itemIndex;
						error = PBGetCatInfoSync(&pb);
						if ( error == noErr )
						{
							items->parID = pb.hFileInfo.ioFlParID;	/* return item's parID */
							items->vRefNum = pb.hFileInfo.ioVRefNum;	/* return item's vRefNum */
							++*itemIndex;	/* prepare to get next item in directory */
							
							if ( (pb.hFileInfo.ioFlAttrib & ioDirMask) != 0 )
							{
								if ( getDirectories )
								{
									++*actItemCount; /* keep this item */
									++items; /* point to next item */
								}
							}
							else
							{
								if ( getFiles )
								{
									++*actItemCount; /* keep this item */
									++items; /* point to next item */
								}
							}
						}
					}
				}
				else
				{
					/* it wasn't a directory */
					error = dirNFErr;
				}
			}
		}
	}
	else
	{
		/* bad itemIndex */
		error = paramErr;
	}
	
	return ( error );
}

/*****************************************************************************/

static	void	DeleteLevel(long dirToDelete,
							DeleteEnumGlobalsPtr theGlobals)
{
	long savedDir;
	
	do
	{
		/* prepare to delete directory */
		theGlobals->myPB.ciPB.dirInfo.ioNamePtr = (StringPtr)&theGlobals->itemName;
		theGlobals->myPB.ciPB.dirInfo.ioFDirIndex = 1;	/* get first item */
		theGlobals->myPB.ciPB.dirInfo.ioDrDirID = dirToDelete;	/* in this directory */
		theGlobals->error = PBGetCatInfoSync(&(theGlobals->myPB.ciPB));
		if ( theGlobals->error == noErr )
		{
			savedDir = dirToDelete;
			/* We have an item.  Is it a file or directory? */
			if ( (theGlobals->myPB.ciPB.dirInfo.ioFlAttrib & ioDirMask) != 0 )
			{
				/* it's a directory */
				savedDir = theGlobals->myPB.ciPB.dirInfo.ioDrDirID;	/* save dirID of directory instead */
				DeleteLevel(theGlobals->myPB.ciPB.dirInfo.ioDrDirID, theGlobals);	/* Delete its contents */
				theGlobals->myPB.ciPB.dirInfo.ioNamePtr = NULL;	/* prepare to delete directory */
			}
			if ( theGlobals->error == noErr )
			{
				theGlobals->myPB.ciPB.dirInfo.ioDrDirID = savedDir;	/* restore dirID */
				theGlobals->myPB.hPB.fileParam.ioFVersNum = 0;	/* just in case it's used on an MFS volume... */
				theGlobals->error = PBHDeleteSync(&(theGlobals->myPB.hPB));	/* delete this item */
				if ( theGlobals->error == fLckdErr )
				{
					(void) PBHRstFLockSync(&(theGlobals->myPB.hPB));	/* unlock it */
					theGlobals->error = PBHDeleteSync(&(theGlobals->myPB.hPB));	/* and try again */
				}
			}
		}
	} while ( theGlobals->error == noErr );
	
	if ( theGlobals->error == fnfErr )
	{
		theGlobals->error = noErr;
	}
}

/*****************************************************************************/

pascal	OSErr	DeleteDirectoryContents(short vRefNum,
								 		long dirID,
										ConstStr255Param name)
{
	DeleteEnumGlobals theGlobals;
	Boolean	isDirectory;
	OSErr error;

	/*  Get the real dirID and make sure it is a directory. */
	error = GetDirectoryID(vRefNum, dirID, name, &dirID, &isDirectory);
	if ( error == noErr )
	{
		if ( isDirectory )
		{
			/* Get the real vRefNum */
			error = DetermineVRefNum(name, vRefNum, &vRefNum);
			if ( error == noErr )
			{
				/* Set up the globals we need to access from the recursive routine. */
				theGlobals.myPB.ciPB.dirInfo.ioVRefNum = vRefNum;
					
				/* Here we go into recursion land... */
				DeleteLevel(dirID, &theGlobals);
				error = theGlobals.error;
			}
		}
		else
		{
			error = dirNFErr;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	DeleteDirectory(short vRefNum,
								long dirID,
								ConstStr255Param name)
{
	OSErr error;
	
	/* Make sure a directory was specified and then delete its contents */
	error = DeleteDirectoryContents(vRefNum, dirID, name);
	if ( error == noErr )
	{
		error = HDelete(vRefNum, dirID, name);
		if ( error == fLckdErr )
		{
			(void) HRstFLock(vRefNum, dirID, name);	/* unlock the directory locked by AppleShare */
			error = HDelete(vRefNum, dirID, name);	/* and try again */
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	CheckObjectLock(short vRefNum,
								long dirID,
								ConstStr255Param name)
{
	CInfoPBRec pb;
	OSErr error;
	
	error = GetCatInfoNoName(vRefNum, dirID, name, &pb);
	if ( error == noErr )
	{
		/* check locked bit */
		if ( (pb.hFileInfo.ioFlAttrib & 0x01) != 0 )
		{
			error = fLckdErr;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpCheckObjectLock(const FSSpec *spec)
{
	return ( CheckObjectLock(spec->vRefNum, spec->parID, spec->name) );
}

/*****************************************************************************/

pascal	OSErr	GetFileSize(short vRefNum,
							long dirID,
							ConstStr255Param fileName,
							long *dataSize,
							long *rsrcSize)
{
	HParamBlockRec pb;
	OSErr error;
	
	pb.fileParam.ioNamePtr = (StringPtr)fileName;
	pb.fileParam.ioVRefNum = vRefNum;
	pb.fileParam.ioFVersNum = 0;
	pb.fileParam.ioDirID = dirID;
	pb.fileParam.ioFDirIndex = 0;
	error = PBHGetFInfoSync(&pb);
	if ( error == noErr )
	{
		*dataSize = pb.fileParam.ioFlLgLen;
		*rsrcSize = pb.fileParam.ioFlRLgLen;
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpGetFileSize(const FSSpec *spec,
							   long *dataSize,
							   long *rsrcSize)
{
	return ( GetFileSize(spec->vRefNum, spec->parID, spec->name, dataSize, rsrcSize) );
}

/*****************************************************************************/

pascal	OSErr	BumpDate(short vRefNum,
						 long dirID,
						 ConstStr255Param name)
/* Given a file or directory, change its modification date to the current date/time. */
{
	CInfoPBRec pb;
	Str31 tempName;
	OSErr error;
	unsigned long secs;

	/* Protection against File Sharing problem */
	if ( (name == NULL) || (name[0] == 0) )
	{
		tempName[0] = 0;
		pb.hFileInfo.ioNamePtr = tempName;
		pb.hFileInfo.ioFDirIndex = -1;	/* use ioDirID */
	}
	else
	{
		pb.hFileInfo.ioNamePtr = (StringPtr)name;
		pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	}
	pb.hFileInfo.ioVRefNum = vRefNum;
	pb.hFileInfo.ioDirID = dirID;
	error = PBGetCatInfoSync(&pb);
	if ( error == noErr )
	{
		GetDateTime(&secs);
		/* set mod date to current date, or one second into the future
			if mod date = current date */
		pb.hFileInfo.ioFlMdDat = (secs == pb.hFileInfo.ioFlMdDat) ? (++secs) : (secs);
		if ( pb.dirInfo.ioNamePtr == tempName )
		{
			pb.hFileInfo.ioDirID = pb.hFileInfo.ioFlParID;
		}
		else
		{
			pb.hFileInfo.ioDirID = dirID;
		}
		error = PBSetCatInfoSync(&pb);
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpBumpDate(const FSSpec *spec)
{
	return ( BumpDate(spec->vRefNum, spec->parID, spec->name) );
}

/*****************************************************************************/

pascal	OSErr	ChangeCreatorType(short vRefNum,
								  long dirID,
								  ConstStr255Param name,
								  OSType creator,
								  OSType fileType)
{
	CInfoPBRec pb;
	OSErr error;
	short realVRefNum;
	long parID;

	pb.hFileInfo.ioNamePtr = (StringPtr)name;
	pb.hFileInfo.ioVRefNum = vRefNum;
	pb.hFileInfo.ioDirID = dirID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	error = PBGetCatInfoSync(&pb);
	if ( error == noErr )
	{
		if ( (pb.hFileInfo.ioFlAttrib & ioDirMask) == 0 )	/* if file */
		{
			parID = pb.hFileInfo.ioFlParID;	/* save parent dirID for BumpDate call */

			/* If creator not 0x00000000, change creator */
			if ( creator != (OSType)0x00000000 )
			{
				pb.hFileInfo.ioFlFndrInfo.fdCreator = creator;
			}
			
			/* If fileType not 0x00000000, change fileType */
			if ( fileType != (OSType)0x00000000 )
			{
				pb.hFileInfo.ioFlFndrInfo.fdType = fileType;
			}
				
			pb.hFileInfo.ioDirID = dirID;
			error = PBSetCatInfoSync(&pb);	/* now, save the new information back to disk */

			if ( (error == noErr) && (parID != fsRtParID) ) /* can't bump fsRtParID */
			{
				/* get the real vRefNum in case a full pathname was passed */
				error = DetermineVRefNum(name, vRefNum, &realVRefNum);
				if ( error == noErr )
				{
					error = BumpDate(realVRefNum, parID, NULL);
						/* and bump the parent directory's mod date to wake up the Finder */
						/* to the change we just made */
				}
			}
		}
		else
		{
			/* it was a directory, not a file */
			error = notAFileErr;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpChangeCreatorType(const FSSpec *spec,
									 OSType creator,
									 OSType fileType)
{
	return ( ChangeCreatorType(spec->vRefNum, spec->parID, spec->name, creator, fileType) );
}

/*****************************************************************************/

pascal	OSErr	ChangeFDFlags(short vRefNum,
							  long dirID,
							  ConstStr255Param name,
							  Boolean	setBits,
							  unsigned short flagBits)
{
	CInfoPBRec pb;
	Str31 tempName;
	OSErr error;
	short realVRefNum;
	long parID;

	/* Protection against File Sharing problem */
	if ( (name == NULL) || (name[0] == 0) )
	{
		tempName[0] = 0;
		pb.hFileInfo.ioNamePtr = tempName;
		pb.hFileInfo.ioFDirIndex = -1;	/* use ioDirID */
	}
	else
	{
		pb.hFileInfo.ioNamePtr = (StringPtr)name;
		pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	}
	pb.hFileInfo.ioVRefNum = vRefNum;
	pb.hFileInfo.ioDirID = dirID;
	error = PBGetCatInfoSync(&pb);
	if ( error == noErr )
	{
		parID = pb.hFileInfo.ioFlParID;	/* save parent dirID for BumpDate call */
		
		/* set or clear the appropriate bits in the Finder flags */
		if ( setBits )
		{
			/* OR in the bits */
			pb.hFileInfo.ioFlFndrInfo.fdFlags |= flagBits;
		}
		else
		{
			/* AND out the bits */
			pb.hFileInfo.ioFlFndrInfo.fdFlags &= ~flagBits;
		}
			
		if ( pb.dirInfo.ioNamePtr == tempName )
		{
			pb.hFileInfo.ioDirID = pb.hFileInfo.ioFlParID;
		}
		else
		{
			pb.hFileInfo.ioDirID = dirID;
		}
		
		error = PBSetCatInfoSync(&pb);	/* now, save the new information back to disk */

		if ( (error == noErr) && (parID != fsRtParID) ) /* can't bump fsRtParID */
		{
			/* get the real vRefNum in case a full pathname was passed */
			error = DetermineVRefNum(name, vRefNum, &realVRefNum);
			if ( error == noErr )
			{
				error = BumpDate(realVRefNum, parID, NULL);
					/* and bump the parent directory's mod date to wake up the Finder */
					/* to the change we just made */
			}
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpChangeFDFlags(const FSSpec *spec,
								 Boolean setBits,
								 unsigned short flagBits)
{
	return ( ChangeFDFlags(spec->vRefNum, spec->parID, spec->name, setBits, flagBits) );
}

/*****************************************************************************/

pascal	OSErr	SetIsInvisible(short vRefNum,
							   long dirID,
							   ConstStr255Param name)
	/* Given a file or directory, make it invisible. */
{
	return ( ChangeFDFlags(vRefNum, dirID, name, true, kIsInvisible) );
}

/*****************************************************************************/

pascal	OSErr	FSpSetIsInvisible(const FSSpec *spec)
	/* Given a file or directory, make it invisible. */
{
	return ( ChangeFDFlags(spec->vRefNum, spec->parID, spec->name, true, kIsInvisible) );
}

/*****************************************************************************/

pascal	OSErr	ClearIsInvisible(short vRefNum,
								 long dirID,
								 ConstStr255Param name)
	/* Given a file or directory, make it visible. */
{
	return ( ChangeFDFlags(vRefNum, dirID, name, false, kIsInvisible) );
}

/*****************************************************************************/

pascal	OSErr	FSpClearIsInvisible(const FSSpec *spec)
	/* Given a file or directory, make it visible. */
{
	return ( ChangeFDFlags(spec->vRefNum, spec->parID, spec->name, false, kIsInvisible) );
}

/*****************************************************************************/

pascal	OSErr	SetNameLocked(short vRefNum,
							  long dirID,
							  ConstStr255Param name)
	/* Given a file or directory, lock its name. */
{
	return ( ChangeFDFlags(vRefNum, dirID, name, true, kNameLocked) );
}

/*****************************************************************************/

pascal	OSErr	FSpSetNameLocked(const FSSpec *spec)
	/* Given a file or directory, lock its name. */
{
	return ( ChangeFDFlags(spec->vRefNum, spec->parID, spec->name, true, kNameLocked) );
}

/*****************************************************************************/

pascal	OSErr	ClearNameLocked(short vRefNum,
								long dirID,
								ConstStr255Param name)
	/* Given a file or directory, unlock its name. */
{
	return ( ChangeFDFlags(vRefNum, dirID, name, false, kNameLocked) );
}

/*****************************************************************************/

pascal	OSErr	FSpClearNameLocked(const FSSpec *spec)
	/* Given a file or directory, unlock its name. */
{
	return ( ChangeFDFlags(spec->vRefNum, spec->parID, spec->name, false, kNameLocked) );
}

/*****************************************************************************/

pascal	OSErr	SetIsStationery(short vRefNum,
								long dirID,
								ConstStr255Param name)
	/* Given a file, make it a stationery pad. */
{
	return ( ChangeFDFlags(vRefNum, dirID, name, true, kIsStationery) );
}

/*****************************************************************************/

pascal	OSErr	FSpSetIsStationery(const FSSpec *spec)
	/* Given a file, make it a stationery pad. */
{
	return ( ChangeFDFlags(spec->vRefNum, spec->parID, spec->name, true, kIsStationery) );
}

/*****************************************************************************/

pascal	OSErr	ClearIsStationery(short vRefNum,
								  long dirID,
								  ConstStr255Param name)
	/* Given a file, clear the stationery bit. */
{
	return ( ChangeFDFlags(vRefNum, dirID, name, false, kIsStationery) );
}

/*****************************************************************************/

pascal	OSErr	FSpClearIsStationery(const FSSpec *spec)
	/* Given a file, clear the stationery bit. */
{
	return ( ChangeFDFlags(spec->vRefNum, spec->parID, spec->name, false, kIsStationery) );
}

/*****************************************************************************/

pascal	OSErr	SetHasCustomIcon(short vRefNum,
								 long dirID,
								 ConstStr255Param name)
	/* Given a file or directory, indicate that it has a custom icon. */
{
	return ( ChangeFDFlags(vRefNum, dirID, name, true, kHasCustomIcon) );
}

/*****************************************************************************/

pascal	OSErr	FSpSetHasCustomIcon(const FSSpec *spec)
	/* Given a file or directory, indicate that it has a custom icon. */
{
	return ( ChangeFDFlags(spec->vRefNum, spec->parID, spec->name, true, kHasCustomIcon) );
}

/*****************************************************************************/

pascal	OSErr	ClearHasCustomIcon(short vRefNum,
								   long dirID,
								   ConstStr255Param name)
	/* Given a file or directory, indicate that it does not have a custom icon. */
{
	return ( ChangeFDFlags(vRefNum, dirID, name, false, kHasCustomIcon) );
}

/*****************************************************************************/

pascal	OSErr	FSpClearHasCustomIcon(const FSSpec *spec)
	/* Given a file or directory, indicate that it does not have a custom icon. */
{
	return ( ChangeFDFlags(spec->vRefNum, spec->parID, spec->name, false, kHasCustomIcon) );
}

/*****************************************************************************/

pascal	OSErr	ClearHasBeenInited(short vRefNum,
								   long dirID,
								   ConstStr255Param name)
	/* Given a file, clear its "has been inited" bit. */
{
	return ( ChangeFDFlags(vRefNum, dirID, name, false, kHasBeenInited) );
}

/*****************************************************************************/

pascal	OSErr	FSpClearHasBeenInited(const FSSpec *spec)
	/* Given a file, clear its "has been inited" bit. */
{
	return ( ChangeFDFlags(spec->vRefNum, spec->parID, spec->name, false, kHasBeenInited) );
}

/*****************************************************************************/

pascal	OSErr	CopyFileMgrAttributes(short srcVRefNum,
									  long srcDirID,
									  ConstStr255Param srcName,
									  short dstVRefNum,
									  long dstDirID,
									  ConstStr255Param dstName,
									  Boolean copyLockBit)
{
	UniversalFMPB pb;
	Str31 tempName;
	OSErr error;
	Boolean objectIsDirectory;

	pb.ciPB.hFileInfo.ioVRefNum = srcVRefNum;
	pb.ciPB.hFileInfo.ioDirID = srcDirID;

	/* Protection against File Sharing problem */
	if ( (srcName == NULL) || (srcName[0] == 0) )
	{
		tempName[0] = 0;
		pb.ciPB.hFileInfo.ioNamePtr = tempName;
		pb.ciPB.hFileInfo.ioFDirIndex = -1;	/* use ioDirID */
	}
	else
	{
		pb.ciPB.hFileInfo.ioNamePtr = (StringPtr)srcName;
		pb.ciPB.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	}
	error = PBGetCatInfoSync(&pb.ciPB);
	if ( error == noErr )
	{
		objectIsDirectory = ( (pb.ciPB.hFileInfo.ioFlAttrib & ioDirMask) != 0 );
		pb.ciPB.hFileInfo.ioVRefNum = dstVRefNum;
		pb.ciPB.hFileInfo.ioDirID = dstDirID;
		if ( (dstName != NULL) && (dstName[0] == 0) )
		{
			pb.ciPB.hFileInfo.ioNamePtr = NULL;
		}
		else
		{
			pb.ciPB.hFileInfo.ioNamePtr = (StringPtr)dstName;
		}
		/* don't copy the hasBeenInited bit */
		pb.ciPB.hFileInfo.ioFlFndrInfo.fdFlags = ( pb.ciPB.hFileInfo.ioFlFndrInfo.fdFlags & 0xfeff );
		error = PBSetCatInfoSync(&pb.ciPB);
		if ( (error == noErr) && (copyLockBit) && ((pb.ciPB.hFileInfo.ioFlAttrib & 0x01) != 0) )
		{
			pb.hPB.fileParam.ioFVersNum = 0;
			error = PBHSetFLockSync(&pb.hPB);
			if ( (error != noErr) && (objectIsDirectory) )
			{
				error = noErr; /* ignore lock errors if destination is directory */
			}
		}
	}
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpCopyFileMgrAttributes(const FSSpec *srcSpec,
										 const FSSpec *dstSpec,
										 Boolean copyLockBit)
{
	return ( CopyFileMgrAttributes(srcSpec->vRefNum, srcSpec->parID, srcSpec->name,
								   dstSpec->vRefNum, dstSpec->parID, dstSpec->name,
								   copyLockBit) );
}

/*****************************************************************************/

pascal	OSErr	HOpenAware(short vRefNum,
						   long dirID,
						   ConstStr255Param fileName,
						   short denyModes,
						   short *refNum)
{
	HParamBlockRec pb;
	OSErr error;
	GetVolParmsInfoBuffer volParmsInfo;
	long infoSize = sizeof(GetVolParmsInfoBuffer);

	pb.ioParam.ioMisc = NULL;
	pb.fileParam.ioFVersNum = 0;
	pb.fileParam.ioNamePtr = (StringPtr)fileName;
	pb.fileParam.ioVRefNum = vRefNum;
	pb.fileParam.ioDirID = dirID;

	/* get volume attributes */
	/* this preflighting is needed because Foreign File Access based file systems don't */
	/* return the correct error result to the OpenDeny call */
	error = HGetVolParms(fileName, vRefNum, &volParmsInfo, &infoSize);
	if ( (error == noErr) && hasOpenDeny(volParmsInfo) )
	{
		/* if volume supports OpenDeny, use it and return */
			pb.accessParam.ioDenyModes = denyModes;
			error = PBHOpenDenySync(&pb);
			*refNum = pb.ioParam.ioRefNum;
	}
	else if ( (error == noErr) || (error == paramErr) )	/* paramErr is OK, it just means this volume doesn't support GetVolParms */
	{
		/* OpenDeny isn't supported, so try File Manager Open functions */
		
		/* If request includes write permission, then see if the volume is */
		/* locked by hardware or software. The HFS file system doesn't check */
		/* for this when a file is opened - you only find out later when you */
		/* try to write and the write fails with a wPrErr or a vLckdErr. */
		
		if ( (denyModes & dmWr) != 0 )
		{
			error = CheckVolLock(fileName, vRefNum);
		}
		else
		{
			error = noErr;
		}
		
		if ( error == noErr )
		{
			/* Set File Manager permissions to closest thing possible */
			if ( (denyModes == dmWr) || (denyModes == dmRdWr) )
			{
				pb.ioParam.ioPermssn = fsRdWrShPerm;
			}
			else
			{
				pb.ioParam.ioPermssn = denyModes % 4;
			}

			error = PBHOpenDFSync(&pb);				/* Try OpenDF */
			if ( error == paramErr )
			{
				error = PBHOpenSync(&pb);			/* OpenDF not supported, so try Open */
			}
			*refNum = pb.ioParam.ioRefNum;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpOpenAware(const FSSpec *spec,
							 short denyModes,
							 short *refNum)
{
	return ( HOpenAware(spec->vRefNum, spec->parID, spec->name, denyModes, refNum) );
}

/*****************************************************************************/

pascal	OSErr	HOpenRFAware(short vRefNum,
							 long dirID,
							 ConstStr255Param fileName,
							 short denyModes,
							 short *refNum)
{
	HParamBlockRec pb;
	OSErr error;
	GetVolParmsInfoBuffer volParmsInfo;
	long infoSize = sizeof(GetVolParmsInfoBuffer);

	pb.ioParam.ioMisc = NULL;
	pb.fileParam.ioFVersNum = 0;
	pb.fileParam.ioNamePtr = (StringPtr)fileName;
	pb.fileParam.ioVRefNum = vRefNum;
	pb.fileParam.ioDirID = dirID;

	/* get volume attributes */
	/* this preflighting is needed because Foreign File Access based file systems don't */
	/* return the correct error result to the OpenRFDeny call */
	error = HGetVolParms(fileName, vRefNum, &volParmsInfo, &infoSize);
	if ( (error == noErr) && hasOpenDeny(volParmsInfo) )
	{
		/* if volume supports OpenRFDeny, use it and return */
		if ( hasOpenDeny(volParmsInfo) )
		{
			pb.accessParam.ioDenyModes = denyModes;
			error = PBHOpenRFDenySync(&pb);
			*refNum = pb.ioParam.ioRefNum;
		}
	}
	else if ( (error == noErr) || (error == paramErr) )	/* paramErr is OK, it just means this volume doesn't support GetVolParms */
	{
		/* OpenRFDeny isn't supported, so try File Manager OpenRF function */
		
		/* If request includes write permission, then see if the volume is */
		/* locked by hardware or software. The HFS file system doesn't check */
		/* for this when a file is opened - you only find out later when you */
		/* try to write and the write fails with a wPrErr or a vLckdErr. */
		
		if ( (denyModes & dmWr) != 0 )
		{
			error = CheckVolLock(fileName, vRefNum);
		}
		else
		{
			error = noErr;
		}
		
		if ( error == noErr )
		{
			/* Set File Manager permissions to closest thing possible */
			if ( (denyModes == dmWr) || (denyModes == dmRdWr) )
			{
				pb.ioParam.ioPermssn = fsRdWrShPerm;
			}
			else
			{
				pb.ioParam.ioPermssn = denyModes % 4;
			}

			error = PBHOpenRFSync(&pb);
			*refNum = pb.ioParam.ioRefNum;
		}
	}

	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpOpenRFAware(const FSSpec *spec,
							   short denyModes,
							   short *refNum)
{
	return ( HOpenRFAware(spec->vRefNum, spec->parID, spec->name, denyModes, refNum) );
}

/*****************************************************************************/

pascal	OSErr	FSReadNoCache(short refNum,
							  long *count,
							  void *buffPtr)
{
	ParamBlockRec pb;
	OSErr error;

	pb.ioParam.ioRefNum = refNum;
	pb.ioParam.ioBuffer = (Ptr)buffPtr;
	pb.ioParam.ioReqCount = *count;
	pb.ioParam.ioPosMode = fsAtMark + 0x0020;	/* fsAtMark + noCacheBit */
	pb.ioParam.ioPosOffset = 0;
	error = PBReadSync(&pb);
	*count = pb.ioParam.ioActCount;				/* always return count */
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSWriteNoCache(short refNum,
							   long *count,
							   const void *buffPtr)
{
	ParamBlockRec pb;
	OSErr error;

	pb.ioParam.ioRefNum = refNum;
	pb.ioParam.ioBuffer = (Ptr)buffPtr;
	pb.ioParam.ioReqCount = *count;
	pb.ioParam.ioPosMode = fsAtMark + 0x0020;	/* fsAtMark + noCacheBit */
	pb.ioParam.ioPosOffset = 0;
	error = PBWriteSync(&pb);
	*count = pb.ioParam.ioActCount;				/* always return count */
	return ( error );
}

/*****************************************************************************/

/*
**	See if numBytes bytes of buffer1 are equal to buffer2.
*/
static	Boolean EqualMemory(const void *buffer1, const void *buffer2, unsigned long numBytes)
{
	register unsigned char *b1 = (unsigned char *)buffer1;
	register unsigned char *b2 = (unsigned char *)buffer2;

	if ( b1 != b2 )	/* if buffer pointers are same, then they are equal */
	{
		while ( numBytes > 0 )
		{
			/* compare the bytes and then increment the pointers */
			if ( (*b1++ - *b2++) != 0 )
			{
				return ( false );
			}
			--numBytes;
		}
	}
	
	return ( true );
}

/*****************************************************************************/

/*
**	Read any number of bytes from an open file using read-verify mode.
**	The FSReadVerify function reads any number of bytes from an open file
**	and verifies them against the data in the buffer pointed to by buffPtr.
**	
**	Because of a bug in the HFS file system, only non-block aligned parts of
**	the read are verified against the buffer data and the rest is *copied*
**	into the buffer.  Thus, you shouldn't verify against your original data;
**	instead, you should verify against a copy of the original data and then
**	compare the read-verified copy against the original data after calling
**	FSReadVerify. That's why this function isn't exported - it needs the
**	wrapper provided by FSWriteVerify.
*/
static	OSErr	FSReadVerify(short refNum,
							 long *count,
							 void *buffPtr)
{
	ParamBlockRec	pb;
	OSErr			result;

	pb.ioParam.ioRefNum = refNum;
	pb.ioParam.ioBuffer = (Ptr)buffPtr;
	pb.ioParam.ioReqCount = *count;
	pb.ioParam.ioPosMode = fsAtMark + rdVerify;
	pb.ioParam.ioPosOffset = 0;
	result = PBReadSync(&pb);
	*count = pb.ioParam.ioActCount;			/* always return count */
	return ( result );
}

/*****************************************************************************/

pascal	OSErr	FSWriteVerify(short refNum,
							  long *count,
							  const void *buffPtr)
{
	Ptr		verifyBuffer;
	long	position;
	long	bufferSize;
	long	byteCount;
	long	bytesVerified;
	Ptr		startVerify;
	OSErr	result;
	
	/*
	**	Allocate the verify buffer
	**	Try to get get a large enough buffer to verify in one pass.
	**	If that fails, use GetTempBuffer to get a buffer.
	*/
	bufferSize = *count;
	verifyBuffer = NewPtr(bufferSize);
	if ( verifyBuffer == NULL )
	{
		verifyBuffer = GetTempBuffer(bufferSize, &bufferSize);
	}
	if ( verifyBuffer != NULL )
	{		
		/* Save the current position */
		result = GetFPos(refNum, &position);
		if ( result == noErr )
		{
			/* Write the data */
			result = FSWrite(refNum, count, buffPtr);
			if ( result == noErr )
			{
				/* Restore the original position */
				result = SetFPos(refNum, fsFromStart, position);
				if ( result == noErr )
				{
					/*
					**	*count			= total number of bytes to verify
					**	bufferSize		= the size of the verify buffer
					**	bytesVerified	= number of bytes verified
					**	byteCount		= number of bytes to verify this pass
					**	startVerify		= position in buffPtr
					*/
					bytesVerified = 0;
					startVerify = (Ptr)buffPtr;
					while ( (bytesVerified < *count) && ( result == noErr ) )
					{
						if ( (*count - bytesVerified) > bufferSize )
						{
							byteCount = bufferSize;
						}
						else
						{
							byteCount = *count - bytesVerified;
						}
						/*
						**	Copy the write buffer into the verify buffer.
						**	This step is needed because the File Manager
						**	compares the data in any non-block aligned
						**	data at the beginning and end of the read-verify
						**	request back into the file system's cache
						**	to the data in verify Buffer. However, the
						**	File Manager does not compare any full blocks
						**	and instead copies them into the verify buffer
						**	so we still have to compare the buffers again
						**	after the read-verify request completes.
						*/
						BlockMoveData(startVerify, verifyBuffer, byteCount);
						
						/* Read-verify the data back into the verify buffer */
						result = FSReadVerify(refNum, &byteCount, verifyBuffer);
						if ( result == noErr )
						{
							/* See if the buffers are the same */
							if ( !EqualMemory(verifyBuffer, startVerify, byteCount) )
							{
								result = ioErr;
							}
							startVerify += byteCount;
							bytesVerified += byteCount;
						}
					}
				}
			}
		}
		DisposePtr(verifyBuffer);
	}
	else
	{
		result = memFullErr;
	}
	return ( result );
}

/*****************************************************************************/

pascal	OSErr	CopyFork(short srcRefNum,
						 short dstRefNum,
						 void *copyBufferPtr,
						 long copyBufferSize)
{
	ParamBlockRec srcPB;
	ParamBlockRec dstPB;
	OSErr srcError;
	OSErr dstError;

	if ( (copyBufferPtr == NULL) || (copyBufferSize == 0) )
		return ( paramErr );
	
	srcPB.ioParam.ioRefNum = srcRefNum;
	dstPB.ioParam.ioRefNum = dstRefNum;

	/* preallocate the destination fork and */
	/* ensure the destination fork's EOF is correct after the copy */
	srcError = PBGetEOFSync(&srcPB);
	if ( srcError != noErr )
		return ( srcError );
	dstPB.ioParam.ioMisc = srcPB.ioParam.ioMisc;
	dstError = PBSetEOFSync(&dstPB);
	if ( dstError != noErr )
		return ( dstError );

	/* reset source fork's mark */
	srcPB.ioParam.ioPosMode = fsFromStart;
	srcPB.ioParam.ioPosOffset = 0;
	srcError = PBSetFPosSync(&srcPB);
	if ( srcError != noErr )
		return ( srcError );

	/* reset destination fork's mark */
	dstPB.ioParam.ioPosMode = fsFromStart;
	dstPB.ioParam.ioPosOffset = 0;
	dstError = PBSetFPosSync(&dstPB);
	if ( dstError != noErr )
		return ( dstError );

	/* set up fields that won't change in the loop */
	srcPB.ioParam.ioBuffer = (Ptr)copyBufferPtr;
	srcPB.ioParam.ioPosMode = fsAtMark + 0x0020;/* fsAtMark + noCacheBit */
	/* If copyBufferSize is greater than 512 bytes, make it a multiple of 512 bytes */
	/* This will make writes on local volumes faster */
	if ( (copyBufferSize >= 512) && ((copyBufferSize & 0x1ff) != 0) )
	{
		srcPB.ioParam.ioReqCount = copyBufferSize & 0xfffffe00;
	}
	else
	{
		srcPB.ioParam.ioReqCount = copyBufferSize;
	}
	dstPB.ioParam.ioBuffer = (Ptr)copyBufferPtr;
	dstPB.ioParam.ioPosMode = fsAtMark + 0x0020;/* fsAtMark + noCacheBit */

	while ( (srcError == noErr) && (dstError == noErr) )
	{
		srcError = PBReadSync(&srcPB);
		dstPB.ioParam.ioReqCount = srcPB.ioParam.ioActCount;
		dstError = PBWriteSync(&dstPB);
	}

	/* make sure there were no errors at the destination */
	if ( dstError != noErr )
		return ( dstError );

	/* make sure the only error at the source was eofErr */
	if ( srcError != eofErr )
		return ( srcError );

	return ( noErr );
}

/*****************************************************************************/

pascal	OSErr	GetFileLocation(short refNum,
								short *vRefNum,
								long *dirID,
								StringPtr fileName)
{
	FCBPBRec pb;
	OSErr error;

	pb.ioNamePtr = fileName;
	pb.ioVRefNum = 0;
	pb.ioRefNum = refNum;
	pb.ioFCBIndx = 0;
	error = PBGetFCBInfoSync(&pb);
	if ( error == noErr )
	{
		*vRefNum = pb.ioFCBVRefNum;
		*dirID = pb.ioFCBParID;
	}
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpGetFileLocation(short refNum,
								   FSSpec *spec)
{
	return ( GetFileLocation(refNum, &(spec->vRefNum), &(spec->parID), spec->name) );
}

/*****************************************************************************/

pascal	OSErr	CopyDirectoryAccess(short srcVRefNum,
									long srcDirID,
									ConstStr255Param srcName,
									short dstVRefNum,
									long dstDirID,
									ConstStr255Param dstName)
{	
	OSErr error;
	GetVolParmsInfoBuffer infoBuffer;	/* Where PBGetVolParms dumps its info */
	long	dstServerAdr;				/* AppleTalk server address of destination (if any) */
	long	ownerID, groupID, accessRights;
	long	tempLong;

	/* See if destination supports directory access control */
	tempLong = sizeof(infoBuffer);
	error = HGetVolParms(dstName, dstVRefNum, &infoBuffer, &tempLong);
	if ( (error == noErr) && hasAccessCntl(infoBuffer) )
	{
		if ( hasAccessCntl(infoBuffer) )
		{
			dstServerAdr = infoBuffer.vMServerAdr;
			
			/* See if source supports directory access control and is on same server */
			tempLong = sizeof(infoBuffer);
			error = HGetVolParms(srcName, srcVRefNum, &infoBuffer, &tempLong);
			if ( error == noErr )
			{
				if ( hasAccessCntl(infoBuffer) && (dstServerAdr == infoBuffer.vMServerAdr) )
				{
					/* both volumes support directory access control and they are */
					/*  on same server, so copy the access information */
					error = HGetDirAccess(srcVRefNum, srcDirID, srcName, &ownerID, &groupID, &accessRights);
					if ( error == noErr )
					{
						error = HSetDirAccess(dstVRefNum, dstDirID, dstName, ownerID, groupID, accessRights);
					}
				}
				else
				{
					/* destination doesn't support directory access control or */
					/* they volumes aren't on the same server */
					error = paramErr;
				}
			}
		}
		else
		{
			/* destination doesn't support directory access control */
			error = paramErr;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpCopyDirectoryAccess(const FSSpec *srcSpec,
									   const FSSpec *dstSpec)
{
	return ( CopyDirectoryAccess(srcSpec->vRefNum, srcSpec->parID, srcSpec->name,
								dstSpec->vRefNum, dstSpec->parID, dstSpec->name) );
}

/*****************************************************************************/

pascal	OSErr	HMoveRenameCompat(short vRefNum,
								  long srcDirID,
								  ConstStr255Param srcName,
								  long dstDirID,
								  ConstStr255Param dstpathName,
								  ConstStr255Param copyName)
{
	OSErr					error;
	GetVolParmsInfoBuffer	volParmsInfo;
	long					infoSize;
	short					realVRefNum;
	long					realParID;
	Str31					realName;
	Boolean					isDirectory;
	long					tempItemsDirID;
	long					uniqueTempDirID;
	Str31					uniqueTempDirName;
	unsigned short			uniqueNameoverflow;
	
	/* Get volume attributes */
	infoSize = sizeof(GetVolParmsInfoBuffer);
	error = HGetVolParms((StringPtr)srcName, vRefNum, &volParmsInfo, &infoSize);
	if ( (error == noErr) && hasMoveRename(volParmsInfo) )
	{
		/* If volume supports move and rename, so use it and return */
		error = HMoveRename(vRefNum, srcDirID, srcName, dstDirID, dstpathName, copyName);
	}
	else if ( (error == noErr) || (error == paramErr) ) /* paramErr is OK, it just means this volume doesn't support GetVolParms */
	{
		/* MoveRename isn't supported by this volume, so do it by hand */
		
		/* If copyName isn't supplied, we can simply CatMove and return */
		if ( copyName == NULL )
		{
			error = CatMove(vRefNum, srcDirID, srcName, dstDirID, dstpathName);
		}
		else
		{
			/* Renaming is required, so we have some work to do... */
			
			/* Get the object's real name, real parent ID and real vRefNum */
			error = GetObjectLocation(vRefNum, srcDirID, (StringPtr)srcName,
										&realVRefNum, &realParID, realName, &isDirectory);
			if ( error == noErr )
			{
				/* Find the Temporary Items Folder on that volume */
				error = FindFolder(realVRefNum, kTemporaryFolderType, kCreateFolder,
									&realVRefNum, &tempItemsDirID);
				if ( error == noErr )
				{
					/* Create a new uniquely named folder in the temporary items folder. */
					/* This is done to avoid the case where 'realName' or 'copyName' already */
					/* exists in the temporary items folder. */
					
					/* Start with current tick count as uniqueTempDirName */					
					NumToString(TickCount(), uniqueTempDirName);
					uniqueNameoverflow = 0;
					do
					{
						error = DirCreate(realVRefNum, tempItemsDirID, uniqueTempDirName, &uniqueTempDirID);
						if ( error == dupFNErr )
						{
							/* Duplicate name - change the first character to the next ASCII character */
							++uniqueTempDirName[1];
							/* Make sure it isn't a colon! */
							if ( uniqueTempDirName[1] == ':' )
							{
								++uniqueTempDirName[1];
							}
							/* Don't go too far... */
							++uniqueNameoverflow;
						}
					} while ( (error == dupFNErr) && (uniqueNameoverflow <= 64) ); /* 64 new files per 1/60th second - not likely! */
					if ( error == noErr )
					{
						/* Move the object to the folder with uniqueTempDirID for renaming */
						error = CatMove(realVRefNum, realParID, realName, uniqueTempDirID, NULL);
						if ( error == noErr )
						{
							/* Rename the object */	
							error = HRename(realVRefNum, uniqueTempDirID, realName, copyName);
							if ( error == noErr )
							{
								/* Move object to its new home */
								error = CatMove(realVRefNum, uniqueTempDirID, copyName, dstDirID, dstpathName);
								if ( error != noErr )
								{
									/* Error handling: rename object back to original name - ignore errors */
									(void) HRename(realVRefNum, uniqueTempDirID, copyName, realName);
								}
							}
							if ( error != noErr )
							{
								/* Error handling: move object back to original location - ignore errors */
								(void) CatMove(realVRefNum, uniqueTempDirID, realName, realParID, NULL);
							}
						}
						/* Done with ourTempDir, so delete it - ignore errors */
						(void) HDelete(realVRefNum, uniqueTempDirID, NULL);
					}
				}
			}
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpMoveRenameCompat(const FSSpec *srcSpec,
									const FSSpec *dstSpec,
									ConstStr255Param copyName)
{
	/* make sure the FSSpecs refer to the same volume */
	if (srcSpec->vRefNum != dstSpec->vRefNum)
		return (diffVolErr);
	return ( HMoveRenameCompat(srcSpec->vRefNum, srcSpec->parID, srcSpec->name,
					  dstSpec->parID, dstSpec->name, copyName) );
}

/*****************************************************************************/

pascal	OSErr	BuildAFPVolMountInfo(short flags,
									 char nbpInterval,
									 char nbpCount,
									 short uamType,
									 Str32 zoneName,
									 Str32 serverName,
									 Str27 volName,
									 Str31 userName,
									 Str8 userPassword,
									 Str8 volPassword,
									 AFPVolMountInfoPtr *afpInfoPtr)
{
	MyAFPVolMountInfoPtr	infoPtr;
	OSErr					error;
	
	/* Allocate the AFPXVolMountInfo record */
	infoPtr = (MyAFPVolMountInfoPtr)NewPtrClear(sizeof(MyAFPVolMountInfo));
	if ( infoPtr != NULL )
	{
		/* Fill in an AFPVolMountInfo record that can be passed to VolumeMount */
		infoPtr->length = sizeof(MyAFPVolMountInfo);
		infoPtr->media = AppleShareMediaType;
		infoPtr->flags = flags;
		infoPtr->nbpInterval = nbpInterval;
		infoPtr->nbpCount = nbpCount;
		infoPtr->uamType = uamType;
		
		infoPtr->zoneNameOffset = offsetof(MyAFPVolMountInfo, zoneName);
		infoPtr->serverNameOffset = offsetof(MyAFPVolMountInfo, serverName);
		infoPtr->volNameOffset = offsetof(MyAFPVolMountInfo, volName);
		infoPtr->userNameOffset = offsetof(MyAFPVolMountInfo, userName);
		infoPtr->userPasswordOffset = offsetof(MyAFPVolMountInfo, userPassword);
		infoPtr->volPasswordOffset = offsetof(MyAFPVolMountInfo, volPassword);
		
		BlockMoveData(zoneName, infoPtr->zoneName, sizeof(Str32));
		BlockMoveData(serverName, infoPtr->serverName, sizeof(Str32));
		BlockMoveData(volName, infoPtr->volName, sizeof(Str27));
		BlockMoveData(userName, infoPtr->userName, sizeof(Str31));
		BlockMoveData(userPassword, infoPtr->userPassword, sizeof(Str8));
		BlockMoveData(volPassword, infoPtr->volPassword, sizeof(Str8));
		
		*afpInfoPtr = (AFPVolMountInfoPtr)infoPtr;
		error = noErr;
	}
	else
	{
		error = memFullErr;
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	RetrieveAFPVolMountInfo(AFPVolMountInfoPtr afpInfoPtr,
										short *flags,
										short *uamType,
										StringPtr zoneName,
										StringPtr serverName,
										StringPtr volName,
										StringPtr userName)
{
	StringPtr	tempPtr;
	OSErr		error;
		
	/* Retrieve the AFP mounting information from an AFPVolMountInfo record. */
	if ( afpInfoPtr->media == AppleShareMediaType )
	{
		*flags = afpInfoPtr->flags;
		*uamType = afpInfoPtr->uamType;
		
		if ( afpInfoPtr->zoneNameOffset != 0)
		{
			tempPtr = (StringPtr)((long)afpInfoPtr + afpInfoPtr->zoneNameOffset);
			BlockMoveData(tempPtr, zoneName, tempPtr[0] + 1);
		}
		
		if ( afpInfoPtr->serverNameOffset != 0)
		{
			tempPtr = (StringPtr)((long)afpInfoPtr + afpInfoPtr->serverNameOffset);
			BlockMoveData(tempPtr, serverName, tempPtr[0] + 1);
		}
		
		if ( afpInfoPtr->volNameOffset != 0)
		{
			tempPtr = (StringPtr)((long)afpInfoPtr + afpInfoPtr->volNameOffset);
			BlockMoveData(tempPtr, volName, tempPtr[0] + 1);
		}
		
		if ( afpInfoPtr->userNameOffset != 0)
		{
			tempPtr = (StringPtr)((long)afpInfoPtr + afpInfoPtr->userNameOffset);
			BlockMoveData(tempPtr, userName, tempPtr[0] + 1);
		}
		
		error = noErr;
	}
	else
	{
		error = paramErr;
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	BuildAFPXVolMountInfo(short flags,
									  char nbpInterval,
									  char nbpCount,
									  short uamType,
									  Str32 zoneName,
									  Str32 serverName,
									  Str27 volName,
									  Str31 userName,
									  Str8 userPassword,
									  Str8 volPassword,
									  Str32 uamName,
									  unsigned long alternateAddressLength,
									  void *alternateAddress,
									  AFPXVolMountInfoPtr *afpXInfoPtr)
{
	Size					infoSize;
	MyAFPXVolMountInfoPtr	infoPtr;
	OSErr					error;
	
	/* Calculate the size of the AFPXVolMountInfo record */
	infoSize = sizeof(MyAFPXVolMountInfo) + alternateAddressLength - 1;
	
	/* Allocate the AFPXVolMountInfo record */
	infoPtr = (MyAFPXVolMountInfoPtr)NewPtrClear(infoSize);
	if ( infoPtr != NULL )
	{
		/* Fill in an AFPXVolMountInfo record that can be passed to VolumeMount */
		infoPtr->length = infoSize;
		infoPtr->media = AppleShareMediaType;
		infoPtr->flags = flags;
		if ( alternateAddressLength != 0 )
		{
			/* make sure the volMountExtendedFlagsBit is set if there's extended address info */
			infoPtr->flags |= volMountExtendedFlagsMask;
			/* and set the only extendedFlags bit we know about */
			infoPtr->extendedFlags = kAFPExtendedFlagsAlternateAddressMask;
		}
		else
		{
			/* make sure the volMountExtendedFlagsBit is clear if there's no extended address info */
			infoPtr->flags &= ~volMountExtendedFlagsMask;
			/* and clear the extendedFlags */
			infoPtr->extendedFlags = 0;
		}
		infoPtr->nbpInterval = nbpInterval;
		infoPtr->nbpCount = nbpCount;
		infoPtr->uamType = uamType;
		
		infoPtr->zoneNameOffset = offsetof(MyAFPXVolMountInfo, zoneName);		
		infoPtr->serverNameOffset = offsetof(MyAFPXVolMountInfo, serverName);
		infoPtr->volNameOffset = offsetof(MyAFPXVolMountInfo, volName);
		infoPtr->userNameOffset = offsetof(MyAFPXVolMountInfo, userName);
		infoPtr->userPasswordOffset = offsetof(MyAFPXVolMountInfo, userPassword);
		infoPtr->volPasswordOffset = offsetof(MyAFPXVolMountInfo, volPassword);
		infoPtr->uamNameOffset = offsetof(MyAFPXVolMountInfo, uamName);
		infoPtr->alternateAddressOffset = offsetof(MyAFPXVolMountInfo, alternateAddress);
		
		BlockMoveData(zoneName, infoPtr->zoneName, sizeof(Str32));
		BlockMoveData(serverName, infoPtr->serverName, sizeof(Str32));
		BlockMoveData(volName, infoPtr->volName, sizeof(Str27));
		BlockMoveData(userName, infoPtr->userName, sizeof(Str31));
		BlockMoveData(userPassword, infoPtr->userPassword, sizeof(Str8));
		BlockMoveData(volPassword, infoPtr->volPassword, sizeof(Str8));
		BlockMoveData(uamName, infoPtr->uamName, sizeof(Str32));
		BlockMoveData(alternateAddress, infoPtr->alternateAddress, alternateAddressLength);
		
		*afpXInfoPtr = (AFPXVolMountInfoPtr)infoPtr;
		error = noErr;
	}
	else
	{
		error = memFullErr;
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	RetrieveAFPXVolMountInfo(AFPXVolMountInfoPtr afpXInfoPtr,
										 short *flags,
										 short *uamType,
										 StringPtr zoneName,
										 StringPtr serverName,
										 StringPtr volName,
										 StringPtr userName,
										 StringPtr uamName,
										 unsigned long *alternateAddressLength,
										 AFPAlternateAddress **alternateAddress)
{
	StringPtr	tempPtr;
	Ptr			alternateAddressStart;
	Ptr			alternateAddressEnd;
	Size		alternateAddressDataSize;
	OSErr		error;
	UInt8		addressCount;
		
	/* Retrieve the AFP mounting information from an AFPVolMountInfo record. */
	if ( afpXInfoPtr->media == AppleShareMediaType )
	{
		/* default to noErr */
		error = noErr;
		
		/* Is this an extended record? */
		if ( (afpXInfoPtr->flags & volMountExtendedFlagsMask) != 0 )
		{
			if ( ((afpXInfoPtr->extendedFlags & kAFPExtendedFlagsAlternateAddressMask) != 0) &&
				 (afpXInfoPtr->alternateAddressOffset != 0) )
			{
				
				alternateAddressStart = (Ptr)((long)afpXInfoPtr + afpXInfoPtr->alternateAddressOffset);
				alternateAddressEnd = alternateAddressStart + 1;	/* skip over alternate address version byte */
				addressCount = *(UInt8*)alternateAddressEnd;		/* get the address count */
				++alternateAddressEnd;								/* skip over alternate address count byte */
				/* alternateAddressEnd now equals &AFPAlternateAddress.fAddressList[0] */
				while ( addressCount != 0 )
				{
					/* parse the address list to find the end */
					alternateAddressEnd += *(UInt8*)alternateAddressEnd;	/* add length of each AFPTagData record */
					--addressCount;
				}
				/* get the size of the alternateAddressData */
				alternateAddressDataSize = alternateAddressEnd - alternateAddressStart;
				/* allocate memory for it */
				*alternateAddress = (AFPAlternateAddress *)NewPtr(alternateAddressDataSize);
				if ( *alternateAddress != NULL )
				{
					/* and return the data */
					BlockMoveData(alternateAddressStart, *alternateAddress, alternateAddressDataSize);
					*alternateAddressLength = alternateAddressDataSize;
				}
				else
				{
					/* no memory - fail now */
					error = memFullErr;
				}
			}
			
			if ( error == noErr )	/* fill in more output parameters if everything is OK */
			{
				if ( afpXInfoPtr->uamNameOffset != 0 )
				{
					tempPtr = (StringPtr)((long)afpXInfoPtr + afpXInfoPtr->uamNameOffset);
					BlockMoveData(tempPtr, uamName, tempPtr[0] + 1);
				}
			}
		}
		
		if ( error == noErr )	/* fill in more output parameters if everything is OK */
		{
			*flags = afpXInfoPtr->flags;
			*uamType = afpXInfoPtr->uamType;
			
			if ( afpXInfoPtr->zoneNameOffset != 0 )
			{
				tempPtr = (StringPtr)((long)afpXInfoPtr + afpXInfoPtr->zoneNameOffset);
				BlockMoveData(tempPtr, zoneName, tempPtr[0] + 1);
			}
			
			if ( afpXInfoPtr->serverNameOffset != 0 )
			{
				tempPtr = (StringPtr)((long)afpXInfoPtr + afpXInfoPtr->serverNameOffset);
				BlockMoveData(tempPtr, serverName, tempPtr[0] + 1);
			}
			
			if ( afpXInfoPtr->volNameOffset != 0 )
			{
				tempPtr = (StringPtr)((long)afpXInfoPtr + afpXInfoPtr->volNameOffset);
				BlockMoveData(tempPtr, volName, tempPtr[0] + 1);
			}
			
			if ( afpXInfoPtr->userNameOffset != 0 )
			{
				tempPtr = (StringPtr)((long)afpXInfoPtr + afpXInfoPtr->userNameOffset);
				BlockMoveData(tempPtr, userName, tempPtr[0] + 1);
			}
		}
	}
	else
	{
		error = paramErr;
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	GetUGEntries(short objType,
							 UGEntryPtr entries,
							 long reqEntryCount,
							 long *actEntryCount,
							 long *objID)
{
	HParamBlockRec pb;
	OSErr error = noErr;
	UGEntry *endEntryArray;

	pb.objParam.ioObjType = objType;
	*actEntryCount = 0;
	for ( endEntryArray = entries + reqEntryCount; (entries < endEntryArray) && (error == noErr); ++entries )
	{
		pb.objParam.ioObjNamePtr = (StringPtr)entries->name;
		pb.objParam.ioObjID = *objID;
		/* Files.h in the universal interfaces, PBGetUGEntrySync takes a CMovePBPtr */
		/* as the parameter. Inside Macintosh and the original glue used HParmBlkPtr. */
		/* A CMovePBPtr works OK, but this will be changed in the future  back to */
		/* HParmBlkPtr, so I'm just casting it here. */
		error = PBGetUGEntrySync(&pb);
		if ( error == noErr )
		{
			entries->objID = *objID = pb.objParam.ioObjID;
			entries->objType = objType;
			++*actEntryCount;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

