/*
**	Apple Macintosh Developer Technical Support
**
**	A collection of useful high-level Desktop Manager routines.
**	If the Desktop Manager isn't available, use the Desktop file
**	for 'read' operations.
**
**	We do more because we can...
**
**	by Jim Luther and Nitin Ganatra, Apple Developer Technical Support Emeriti
**
**	File:	MoreDesktopMgr.c
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

/* 
 * This code, which was decended from Apple Sample Code, has been modified by 
 * Netscape.
 */

#include <Types.h>
#include <Errors.h>
#include <Memory.h>
#include <Files.h>
#include <Resources.h>
#include <Icons.h>

#define	__COMPILINGMOREFILES

#include "MoreFiles.h"
#include "MoreFilesExtras.h"
#include "MoreFilesSearch.h"
#include "MoreDesktopMgr.h"

/*****************************************************************************/

/*	Desktop file notes:
**
**	¥	The Desktop file is owned by the Finder and is normally open by the
**		Finder. That means that we only have read-only access to the Desktop
**		file.
**	¥	Since the Resource Manager doesn't support shared access to resource
**		files and we're using read-only access, we don't ever leave the
**		Desktop file open.  We open a path to it, get the data we want out
**		of it, and then close the open path. This is the only safe way to
**		open a resource file with read-only access since some other program
**		could have it open with write access.
**	¥	The bundle related resources in the Desktop file are normally
**		purgable, so when we're looking through them, we don't bother to
**		release resources we're done looking at - closing the resource file
**		(which we always do) will release them.
**	¥	Since we can't assume the Desktop file is named "Desktop"
**		(it probably is everywhere but France), we get the Desktop
**		file's name by searching the volume's root directory for a file
**		with fileType == 'FNDR' and creator == 'ERIK'. The only problem with
**		this scheme is that someone could create another file with that type
**		and creator in the root directory and we'd find the wrong file.
**		The chances of this are very slim.
*/

/*****************************************************************************/

/* local defines */

enum
{
	kBNDLResType	= 'BNDL',
	kFREFResType	= 'FREF',
	kIconFamResType	= 'ICN#',
	kFCMTResType	= 'FCMT',
	kAPPLResType	= 'APPL'
};

/*****************************************************************************/

/* local data structures */

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif

struct IDRec
{
	short		localID;
	short		rsrcID;
};
typedef struct IDRec IDRec;
typedef	IDRec *IDRecPtr;

struct BundleType
{
	OSType		type;			/* 'ICN#' or 'FREF' */
	short		count;			/* number of IDRecs - 1 */
	IDRec		idArray[1];
};
typedef struct BundleType BundleType;
typedef BundleType *BundleTypePtr;

struct BNDLRec
{
	OSType		signature;		/* creator type signature */
	short		versionID;		/* version - should always be 0 */
	short		numTypes;		/* number of elements in typeArray - 1 */
	BundleType	typeArray[1];
};
typedef struct BNDLRec BNDLRec;
typedef BNDLRec **BNDLRecHandle;

struct FREFRec
{
	OSType		fileType;		/* file type */
	short		iconID;			/* icon local ID */
	Str255		fileName;		/* file name */
};
typedef struct FREFRec FREFRec;
typedef FREFRec **FREFRecHandle;

struct APPLRec
{
	OSType		creator;		/* creator type signature */
	long		parID;			/* parent directory ID */
	Str255		applName;		/* application name */
};
typedef struct APPLRec APPLRec;
typedef APPLRec *APPLRecPtr;

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

/*****************************************************************************/

/* static prototypes */

static	OSErr	GetDesktopFileName(short vRefNum,
								   Str255 desktopName);

static	OSErr	GetAPPLFromDesktopFile(StringPtr volName,
									   short vRefNum,
									   OSType creator,
									   short *applVRefNum,
									   long *applParID,
									   Str255 applName);

static	OSErr	FindBundleGivenCreator(OSType creator,
									   BNDLRecHandle *returnBndl);
									   
static	OSErr	FindTypeInBundle(OSType typeToFind,
								 BNDLRecHandle theBndl,
								 BundleTypePtr *returnBundleType);
										 
static	OSErr	GetLocalIDFromFREF(BundleTypePtr theBundleType,
								   OSType fileType,
								   short *iconLocalID);

static	OSErr	GetIconRsrcIDFromLocalID(BundleTypePtr theBundleType,
										 short iconLocalID,
										 short *iconRsrcID);

pascal	OSType	DTIconToResIcon(short iconType);

static	OSErr	GetIconFromDesktopFile(StringPtr volName,
									   short vRefNum,
									   short iconType,
									   OSType fileCreator,
									   OSType fileType,
									   Handle *iconHandle);

static	OSErr	GetCommentID(short vRefNum,
							 long dirID,
							 StringPtr name,
							 short *commentID);

static	OSErr	GetCommentFromDesktopFile(short vRefNum,
										  long dirID,
										  StringPtr name,
										  Str255 comment);

/*****************************************************************************/

/*
**	GetDesktopFileName
**
**	Get the name of the Desktop file.
*/
static	OSErr	GetDesktopFileName(short vRefNum,
								   Str255 desktopName)
{
	OSErr			error;
	HParamBlockRec	pb;
	short			index;
	Boolean			found = false;
	
	pb.fileParam.ioNamePtr = desktopName;
	pb.fileParam.ioVRefNum = vRefNum;
	pb.fileParam.ioFVersNum = 0;
	index = 1;
	do
	{
		pb.fileParam.ioDirID = fsRtDirID;
		pb.fileParam.ioFDirIndex = index;
		error = PBHGetFInfoSync(&pb);
		if ( error == noErr )
		{
			if ( (pb.fileParam.ioFlFndrInfo.fdType == 'FNDR') &&
				 (pb.fileParam.ioFlFndrInfo.fdCreator == 'ERIK') )
			{
				found = true;
			}
		}
		++index;
	} while ( (error == noErr) && !found );
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	DTOpen(StringPtr volName,
					   short vRefNum,
					   short *dtRefNum,
					   Boolean *newDTDatabase)
{
	OSErr error;
	GetVolParmsInfoBuffer volParmsInfo;
	long infoSize;
	DTPBRec pb;
	
	/* Check for volume Desktop Manager support before calling */
	infoSize = sizeof(GetVolParmsInfoBuffer);
	error = HGetVolParms(volName, vRefNum, &volParmsInfo, &infoSize);
	if ( error == noErr )
	{
		if ( hasDesktopMgr(volParmsInfo) )
		{
			pb.ioNamePtr = volName;
			pb.ioVRefNum = vRefNum;
			error = PBDTOpenInform(&pb);
			/* PBDTOpenInform informs us if the desktop was just created */
			/* by leaving the low bit of ioTagInfo clear (0) */
			*newDTDatabase = ((pb.ioTagInfo & 1L) == 0);
			if ( error == paramErr )
			{
				error = PBDTGetPath(&pb);
				/* PBDTGetPath doesn't tell us if the database is new */
				/* so assume it is not new */
				*newDTDatabase = false;
			}
			*dtRefNum = pb.ioDTRefNum;
		}
		else
			error = paramErr;
	}
	return ( error );
}

/*****************************************************************************/

/*
**	GetAPPLFromDesktopFile
**
**	Get a application's location from the
**	Desktop file's 'APPL' resources.
*/
static	OSErr	GetAPPLFromDesktopFile(StringPtr volName,
									   short vRefNum,
									   OSType creator,
									   short *applVRefNum,
									   long *applParID,
									   Str255 applName)
{
	OSErr error;
	short realVRefNum;
	Str255 desktopName;
	short savedResFile;
	short dfRefNum;
	Handle applResHandle;
	Boolean foundCreator;
	Ptr applPtr;
	long applSize;
	
	error = DetermineVRefNum(volName, vRefNum, &realVRefNum);
	if ( error == noErr )
	{
		error = GetDesktopFileName(realVRefNum, desktopName);
		if ( error == noErr )
		{
			savedResFile = CurResFile();
			/*
			**	Open the 'Desktop' file in the root directory. (because
			**	opening the resource file could preload unwanted resources,
			**	bracket the call with SetResLoad(s))
			*/
			SetResLoad(false);
			dfRefNum = HOpenResFile(realVRefNum, fsRtDirID, desktopName, fsRdPerm);
			SetResLoad(true);
			
			if ( dfRefNum != -1)
			{
				/* Get 'APPL' resource ID 0 */
				applResHandle = Get1Resource(kAPPLResType, 0);
				if ( applResHandle != NULL )
				{
					applSize = GetHandleSize((Handle)applResHandle);
					if ( applSize != 0 )	/* make sure the APPL resource isn't empty */
					{
						foundCreator = false;
						applPtr = *applResHandle;
						
						/* APPL's don't have a count so I have to use the size as the bounds */
						while ( (foundCreator == false) &&
								(applPtr < (*applResHandle + applSize)) )
						{
							if ( ((APPLRecPtr)applPtr)->creator == creator )
							{
								foundCreator = true;
							}
							else
							{
								/* fun with pointer math... */
								applPtr += sizeof(OSType) +
										   sizeof(long) +
										   ((APPLRecPtr)applPtr)->applName[0] + 1;
								/* application mappings are word aligned within the resource */
								if ( ((unsigned long)applPtr % 2) != 0 )
									applPtr += 1;
							}
						}
						if ( foundCreator == true )
						{
							*applVRefNum = realVRefNum;
							*applParID = ((APPLRecPtr)applPtr)->parID;
							BlockMoveData(((APPLRecPtr)applPtr)->applName,
										  applName,
										  ((APPLRecPtr)applPtr)->applName[0] + 1);
							error = noErr;
						}
					}
					else
						error = afpItemNotFound;	/* no APPL mapping available */
				}
				else
					error = afpItemNotFound;	/* no APPL mapping available */
				
				/* restore the resource chain and close the Desktop file */
				UseResFile(savedResFile);
				CloseResFile(dfRefNum);
			}
			else
				error = afpItemNotFound;
		}
	}
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	DTGetAPPL(StringPtr volName,
						  short vRefNum,
						  OSType creator,
						  short *applVRefNum,
						  long *applParID,
						  Str255 applName)
{
	OSErr error;
	UniversalFMPB pb;
	short dtRefNum;
	Boolean newDTDatabase;
	short realVRefNum;
	short index;
	Boolean applFound;
	FSSpec spec;
	long actMatchCount;
	
	/* get the real vRefNum */
	error = DetermineVRefNum(volName, vRefNum, &realVRefNum);
	if ( error == noErr )
	{
		error = DTOpen(volName, vRefNum, &dtRefNum, &newDTDatabase);
		if ( error == noErr )
		{
			if ( !newDTDatabase )
			{
				index = 0;
				applFound = false;
				do
				{
					pb.dtPB.ioNamePtr = applName;
					pb.dtPB.ioDTRefNum = dtRefNum;
					pb.dtPB.ioIndex = index;
					pb.dtPB.ioFileCreator = creator;
					error = PBDTGetAPPLSync(&pb.dtPB);
					if ( error == noErr )
					{
						/* got a match - see if it is valid */
						
						*applVRefNum = realVRefNum; /* get the vRefNum now */
						*applParID = pb.dtPB.ioAPPLParID; /* get the parent ID now */
	
						/* pb.hPB.fileParam.ioNamePtr is already set */
						pb.hPB.fileParam.ioVRefNum = realVRefNum;
						pb.hPB.fileParam.ioFVersNum = 0;
						pb.hPB.fileParam.ioDirID = *applParID;
						pb.hPB.fileParam.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
						if ( PBHGetFInfoSync(&pb.hPB) == noErr )
						{
							if ( (pb.hPB.fileParam.ioFlFndrInfo.fdCreator == creator) &&
								 (pb.hPB.fileParam.ioFlFndrInfo.fdType == 'APPL') )
							{
								applFound = true;
							}
						}
					}
					++index;
				} while ( (error == noErr) && !applFound );
				if ( error != noErr )
					error = afpItemNotFound;
			}
			else
				/* Desktop database is empty (new), set error to try CatSearch */
				error = afpItemNotFound;
		}
		/* acceptable errors from Desktop Manager to continue are paramErr or afpItemNotFound */
		if ( error == paramErr )
		{
			/* if paramErr, the volume didn't support the Desktop Manager */
			/* try the Desktop file */
			
			error = GetAPPLFromDesktopFile(volName, vRefNum, creator,
											applVRefNum, applParID, applName);
			if ( error == noErr )
			{
				/* got a match - see if it is valid */
				
				pb.hPB.fileParam.ioNamePtr = applName;
				pb.hPB.fileParam.ioVRefNum = *applVRefNum;
				pb.hPB.fileParam.ioFVersNum = 0;
				pb.hPB.fileParam.ioDirID = *applParID;
				pb.hPB.fileParam.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
				if ( PBHGetFInfoSync(&pb.hPB) == noErr )
				{
					if ( (pb.hPB.fileParam.ioFlFndrInfo.fdCreator != creator) ||
						 (pb.hPB.fileParam.ioFlFndrInfo.fdType != 'APPL') )
					{
						error = afpItemNotFound;
					}
				}
				else if ( error == fnfErr )
					error = afpItemNotFound;
			}
		}
		/* acceptable error from DesktopFile code to continue is afpItemNotFound */
		if ( error == afpItemNotFound )
		{
			/* Couldn't be found in the Desktop file either, try searching with CatSearch */
			
			error = CreatorTypeFileSearch(NULL, realVRefNum, creator, kAPPLResType, &spec, 1,
											&actMatchCount, true);
			if ( (error == noErr) || (error == eofErr) )
			{
				if ( actMatchCount > 0 )
				{
					*applVRefNum = spec.vRefNum;
					*applParID = spec.parID;
					BlockMoveData(spec.name, applName, spec.name[0] + 1);
				}
				else
					error = afpItemNotFound;
			}
		}
	}
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	FSpDTGetAPPL(StringPtr volName,
							 short vRefNum,
							 OSType creator,
							 FSSpec *spec)
{
	return ( DTGetAPPL(volName, vRefNum, creator,
						&(spec->vRefNum), &(spec->parID), spec->name) );
}

/*****************************************************************************/

/*
**	FindBundleGivenCreator
**
**	Search the current resource file for the 'BNDL' resource with the given
**	creator and return a handle to it.
*/
static	OSErr	FindBundleGivenCreator(OSType creator,
									   BNDLRecHandle *returnBndl)
{
	OSErr			error;
	short			numOfBundles;
	short			index;
	BNDLRecHandle	theBndl;
	
	error = afpItemNotFound;	/* default to not found */
	
	/* Search each BNDL resource until we find the one with a matching creator. */
	
	numOfBundles = Count1Resources(kBNDLResType);
	index = 1;
	*returnBndl = NULL;
	
	while ( (index <= numOfBundles) && (*returnBndl == NULL) )
	{
		theBndl = (BNDLRecHandle)Get1IndResource(kBNDLResType, index);
		
		if ( theBndl != NULL )
		{
			if ( (*theBndl)->signature == creator )
			{
				/* numTypes and typeArray->count will always be the actual count minus 1, */
				/* so 0 in both fields is valid. */
				if ( ((*theBndl)->numTypes >= 0) && ((*theBndl)->typeArray->count >= 0) )
				{
					/* got it */
					*returnBndl = theBndl;
					error = noErr;
				}
			}
		}	
		
		index ++;
	}
	
	return ( error );
}

/*****************************************************************************/

/*
**	FindTypeInBundle
**
**	Given a Handle to a BNDL return a pointer to the desired type
**	in it. If the type is not found, or if the type's count < 0,
**	return afpItemNotFound.
*/
static	OSErr	FindTypeInBundle(OSType typeToFind,
								 BNDLRecHandle theBndl,
								 BundleTypePtr *returnBundleType)
{
	OSErr			error;
	short			index;
	Ptr				ptrIterator;	/* use a Ptr so we can do ugly pointer math */
	
	error = afpItemNotFound;	/* default to not found */
	
	ptrIterator = (Ptr)((*theBndl)->typeArray);
	index = 0;
	*returnBundleType = NULL;

	while ( (index < ((*theBndl)->numTypes + 1)) &&
			(*returnBundleType == NULL) )
	{
		if ( (((BundleTypePtr)ptrIterator)->type == typeToFind) &&
			 (((BundleTypePtr)ptrIterator)->count >= 0) )
		{
				*returnBundleType = (BundleTypePtr)ptrIterator;
				error = noErr;
		}
		else
		{
			ptrIterator += ( sizeof(OSType) +
							 sizeof(short) +
							 ( sizeof(IDRec) * (((BundleTypePtr)ptrIterator)->count + 1) ) );
			++index;
		}
	}
		
	return ( error );
}

/*****************************************************************************/

/*
**	GetLocalIDFromFREF
**
**	Given a pointer to a 'FREF' BundleType record, load each 'FREF' resource
**	looking for a matching fileType. If a matching fileType is found, return
**	its icon local ID. If no match is found, return afpItemNotFound as the
**	function result.
*/
static	OSErr	GetLocalIDFromFREF(BundleTypePtr theBundleType,
								   OSType fileType,
								   short *iconLocalID)
{
	OSErr			error;
	short			index;
	IDRecPtr		idIterator;
	FREFRecHandle	theFref;
	
	error = afpItemNotFound;	/* default to not found */
	
	/* For each localID in this type, get the FREF resource looking for fileType */
	index = 0;
	idIterator = &theBundleType->idArray[0];
	*iconLocalID = 0;
	
	while ( (index <= (theBundleType->count + 1)) && (*iconLocalID == 0) )
	{
		theFref = (FREFRecHandle)Get1Resource(kFREFResType, idIterator->rsrcID);
		if ( theFref != NULL )
		{
			if ( (*theFref)->fileType == fileType )
			{
				*iconLocalID = (*theFref)->iconID;
				error = noErr;
			}
		}
		
		++idIterator;
		++index;
	}
	
	return ( error );
}

/*****************************************************************************/

/*
**	GetIconRsrcIDFromLocalID
**
**	Given a pointer to a 'ICN#' BundleType record, look for the IDRec with
**	the localID that matches iconLocalID. If a matching IDRec is found,
**	return the IDRec's rsrcID field value. If no match is found, return
**	afpItemNotFound as the function result.
*/
static	OSErr	GetIconRsrcIDFromLocalID(BundleTypePtr theBundleType,
										 short iconLocalID,
										 short *iconRsrcID)
{
	OSErr		error;
	short		index;
	IDRecPtr	idIterator;
	
	error = afpItemNotFound;	/* default to not found */
	
	/* Find the rsrcID of the icon family type, given the localID */
	index = 0;
	idIterator = &theBundleType->idArray[0];
	*iconRsrcID = 0;
	
	while ( (index <= (theBundleType->count+1)) && (*iconRsrcID == 0) )
	{
		if ( idIterator->localID == iconLocalID )
		{
			*iconRsrcID = idIterator->rsrcID;
			error = noErr;
		}
		
		idIterator ++;
		index ++;
	}
	
	return ( error );
}

/*****************************************************************************/

/*
**	DTIconToResIcon
**
**	Map a Desktop Manager icon type to the corresponding resource type.
**	Return (OSType)0 if there is no corresponding resource type.
*/
pascal OSType	DTIconToResIcon(short iconType)
{
	OSType	resType;
	
	switch ( iconType )
	{
		case kLargeIcon:
			resType = large1BitMask;
			break;
		case kLarge4BitIcon:
			resType = large4BitData;
			break;
		case kLarge8BitIcon:
			resType = large8BitData;
			break;
		case kSmallIcon:
			resType = small1BitMask;
			break;
		case kSmall4BitIcon:
			resType = small4BitData;
			break;
		case kSmall8BitIcon:
			resType = small8BitData;
			break;
		default:
			resType = (OSType)0;
			break;
	}
	
	return ( resType );
}

/*****************************************************************************/

/*
**	GetIconFromDesktopFile
**
**	INPUT a pointer to a non-existent Handle, because we'll allocate one
**
**	search each BNDL resource for the right fileCreator and once we get it
**		find the 'FREF' type in BNDL
**		for each localID in the type, open the FREF resource
**			if the FREF is the desired fileType
**				get its icon localID
**				get the ICN# type in BNDL
**				get the icon resource number from the icon localID
**				get the icon resource type from the desktop mgr's iconType
**				get the icon of that type and number
*/
static	OSErr	GetIconFromDesktopFile(StringPtr volName,
									   short vRefNum,
									   short iconType,
									   OSType fileCreator,
									   OSType fileType,
									   Handle *iconHandle)
{
	OSErr			error;
	short			realVRefNum;
	Str255			desktopName;
	short			savedResFile;
	short			dfRefNum;
	BNDLRecHandle	theBndl = NULL;
	BundleTypePtr	theBundleType;
	short			iconLocalID;
	short			iconRsrcID;
	OSType			iconRsrcType;
	Handle			returnIconHandle;	
	char			bndlState;
	
	*iconHandle = NULL;
	
	error = DetermineVRefNum(volName, vRefNum, &realVRefNum);
	if ( error == noErr )
	{
		error = GetDesktopFileName(realVRefNum, desktopName);
		if ( error == noErr )
		{
			savedResFile = CurResFile();
		
			/*
			**	Open the 'Desktop' file in the root directory. (because
			**	opening the resource file could preload unwanted resources,
			**	bracket the call with SetResLoad(s))
			*/
			SetResLoad(false);
			dfRefNum = HOpenResFile(realVRefNum, fsRtDirID, desktopName, fsRdPerm);
			SetResLoad(true);
		
			if ( dfRefNum != -1 )
			{
				/*
				**	Find the BNDL resource with the specified creator.
				*/
				error = FindBundleGivenCreator(fileCreator, &theBndl);
				if ( error == noErr )
				{
					/* Lock the BNDL resource so it won't be purged when other resources are loaded */
					bndlState = HGetState((Handle)theBndl);
					HLock((Handle)theBndl);
					
					/* Find the 'FREF' BundleType record in the BNDL resource. */
					error = FindTypeInBundle(kFREFResType, theBndl, &theBundleType);
					if ( error == noErr )
					{
						/* Find the local ID in the 'FREF' resource with the specified fileType */
						error = GetLocalIDFromFREF(theBundleType, fileType, &iconLocalID);
						if ( error == noErr )
						{
							/* Find the 'ICN#' BundleType record in the BNDL resource. */
							error = FindTypeInBundle(kIconFamResType, theBndl, &theBundleType);
							if ( error == noErr )
							{
								/* Find the icon's resource ID in the 'ICN#' BundleType record */
								error = GetIconRsrcIDFromLocalID(theBundleType, iconLocalID, &iconRsrcID);
								if ( error == noErr )
								{
									/* Map Desktop Manager icon type to resource type */
									iconRsrcType = DTIconToResIcon(iconType);
									
									if ( iconRsrcType != (OSType)0 )
									{
										/* Load the icon */
										returnIconHandle = Get1Resource(iconRsrcType, iconRsrcID);
										if ( returnIconHandle != NULL )
										{
											/* Copy the resource handle, and return the copy */
											HandToHand(&returnIconHandle);
											if ( MemError() == noErr )
												*iconHandle = returnIconHandle;
											else
												error = afpItemNotFound;
										}
										else
											error = afpItemNotFound;
									}
								}
							}
						}
					}
					/* Restore the state of the BNDL resource */ 
					HSetState((Handle)theBndl, bndlState);
				}
				/* Restore the resource chain and close the Desktop file */
				UseResFile(savedResFile);
				CloseResFile(dfRefNum);
			}
			else
				error = ResError(); /* could not open Desktop file */
		}
		if ( (error != noErr) && (error != memFullErr) )
		{
			error = afpItemNotFound;	/* force an error we should return */
		}
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	DTGetIcon(StringPtr volName,
						  short vRefNum,
						  short iconType,
						  OSType fileCreator,
						  OSType fileType,
						  Handle *iconHandle)
{
	OSErr error;
	DTPBRec pb;
	short dtRefNum;
	Boolean newDTDatabase;
	Size bufferSize;
	
	*iconHandle = NULL;
	error = DTOpen(volName, vRefNum, &dtRefNum, &newDTDatabase);
	if ( error == noErr )
	{
		/* there was a desktop database and it's now open */
		
		if ( !newDTDatabase )	/* don't bother to look in a new (empty) database */
		{
			/* get the buffer size for the requested icon type */
			switch ( iconType )
			{
				case kLargeIcon:
					bufferSize = kLargeIconSize;
					break;
				case kLarge4BitIcon:
					bufferSize = kLarge4BitIconSize;
					break;
				case kLarge8BitIcon:
					bufferSize = kLarge8BitIconSize;
					break;
				case kSmallIcon:
					bufferSize = kSmallIconSize;
					break;
				case kSmall4BitIcon:
					bufferSize = kSmall4BitIconSize;
					break;
				case kSmall8BitIcon:
					bufferSize = kSmall8BitIconSize;
					break;
				default:
					iconType = 0;
					bufferSize = 0;
					break;
			}
			if ( bufferSize != 0 )
			{
				*iconHandle = NewHandle(bufferSize);
				if ( *iconHandle != NULL )
				{
					HLock(*iconHandle);
		
					pb.ioDTRefNum = dtRefNum;
					pb.ioTagInfo = 0;
					pb.ioDTBuffer = **iconHandle;
					pb.ioDTReqCount = bufferSize;
					pb.ioIconType = iconType;
					pb.ioFileCreator = fileCreator;
					pb.ioFileType = fileType;
					error = PBDTGetIconSync(&pb);
	
					HUnlock(*iconHandle);
					
					if ( error != noErr )
					{
						DisposeHandle(*iconHandle);	/* dispose of the allocated memory */
						*iconHandle = NULL;
					}
				}
				else
					error = memFullErr;	/* handle could not be allocated */
			}
			else
				error = paramErr;	/* unknown icon type requested */
		}
		else
			error = afpItemNotFound;	/* the desktop database was empty - nothing to return */
	}
	else
	{
		/* There is no desktop database - try the Desktop file */
		
		error = GetIconFromDesktopFile(volName, vRefNum, iconType,
										fileCreator, fileType, iconHandle);
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	DTSetComment(short vRefNum,
							 long dirID,
							 StringPtr name,
							 ConstStr255Param comment)
{
	DTPBRec pb;
	OSErr error;
	short dtRefNum;
	Boolean newDTDatabase;

	error = DTOpen(name, vRefNum, &dtRefNum, &newDTDatabase);
	if ( error == noErr )
	{
		pb.ioDTRefNum = dtRefNum;
		pb.ioNamePtr = name;
		pb.ioDirID = dirID;
		pb.ioDTBuffer = (Ptr)&comment[1];
		/* Truncate the comment to 200 characters just in case */
		/* some file system doesn't range check */
		if ( comment[0] <= 200 )
			pb.ioDTReqCount = comment[0];
		else
			pb.ioDTReqCount = 200;
		error = PBDTSetCommentSync(&pb);
	}
	return (error);
}

/*****************************************************************************/

pascal	OSErr	FSpDTSetComment(const FSSpec *spec,
							  ConstStr255Param comment)
{
	return (DTSetComment(spec->vRefNum, spec->parID, (StringPtr)spec->name, comment));
}

/*****************************************************************************/

/*
**	GetCommentID
**
**	Get the comment ID number for the Desktop file's 'FCMT' resource ID from
**	the file or folders fdComment (frComment) field.
*/
static	OSErr	GetCommentID(short vRefNum,
							 long dirID,
							 StringPtr name,
							 short *commentID)
{
	CInfoPBRec pb;
	OSErr error;

	error = GetCatInfoNoName(vRefNum, dirID, name, &pb);
	*commentID = pb.hFileInfo.ioFlXFndrInfo.fdComment;
	return ( error );
}

/*****************************************************************************/

/*
**	GetCommentFromDesktopFile
**
**	Get a file or directory's Finder comment field (if any) from the
**	Desktop file's 'FCMT' resources.
*/
static	OSErr	GetCommentFromDesktopFile(short vRefNum,
										  long dirID,
										  StringPtr name,
										  Str255 comment)
{
	OSErr error;
	short commentID;
	short realVRefNum;
	Str255 desktopName;
	short savedResFile;
	short dfRefNum;
	StringHandle commentHandle;
	
	/* Get the comment ID number */
	error = GetCommentID(vRefNum, dirID, name, &commentID);
	if ( error == noErr )
	{
		if ( commentID != 0 )	/* commentID == 0 means there's no comment */
		{
			error = DetermineVRefNum(name, vRefNum, &realVRefNum);
			if ( error == noErr )
			{
				error = GetDesktopFileName(realVRefNum, desktopName);
				if ( error == noErr )
				{
					savedResFile = CurResFile();
					/*
					**	Open the 'Desktop' file in the root directory. (because
					**	opening the resource file could preload unwanted resources,
					**	bracket the call with SetResLoad(s))
					*/
					SetResLoad(false);
					dfRefNum = HOpenResFile(realVRefNum, fsRtDirID, desktopName, fsRdPerm);
					SetResLoad(true);
					
					if ( dfRefNum != -1)
					{
						/* Get the comment resource */
						commentHandle = (StringHandle)Get1Resource(kFCMTResType,commentID);
						if ( commentHandle != NULL )
						{
							if ( GetHandleSize((Handle)commentHandle) > 0 )
								BlockMoveData(*commentHandle, comment, *commentHandle[0] + 1);
							else
								error = afpItemNotFound;	/* no comment available */
						}
						else
							error = afpItemNotFound;	/* no comment available */
						
						/* restore the resource chain and close the Desktop file */
						UseResFile(savedResFile);
						CloseResFile(dfRefNum);
					}
					else
						error = afpItemNotFound;
				}
				else
					error = afpItemNotFound;
			}
		}
		else
			error = afpItemNotFound;	/* no comment available */
	}
	
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	DTGetComment(short vRefNum,
							 long dirID,
							 StringPtr name,
							 Str255 comment)
{
	DTPBRec pb;
	OSErr error;
	short dtRefNum;
	Boolean newDTDatabase;

	if (comment != NULL)
	{
		comment[0] = 0;	/* return nothing by default */
		
		/* attempt to open the desktop database */
		error = DTOpen(name, vRefNum, &dtRefNum, &newDTDatabase);
		if ( error == noErr )
		{
			/* There was a desktop database and it's now open */
			
			if ( !newDTDatabase )
			{
				pb.ioDTRefNum = dtRefNum;
				pb.ioNamePtr = name;
				pb.ioDirID = dirID;
				pb.ioDTBuffer = (Ptr)&comment[1];
				error = PBDTGetCommentSync(&pb);
				if (error == noErr)
					comment[0] = (unsigned char)pb.ioDTActCount;
			}
		}
		else
		{
			/* There is no desktop database - try the Desktop file */
			error = GetCommentFromDesktopFile(vRefNum, dirID, name, comment);
			if ( error != noErr )
				error = afpItemNotFound;	/* return an expected error */
		}
	}
	else
		error = paramErr;
	
	return (error);
}

/*****************************************************************************/

pascal	OSErr	FSpDTGetComment(const FSSpec *spec,
							  Str255 comment)
{
	return (DTGetComment(spec->vRefNum, spec->parID, (StringPtr)spec->name, comment));
}

/*****************************************************************************/

pascal	OSErr	DTCopyComment(short srcVRefNum,
							  long srcDirID,
							  StringPtr srcName,
							  short dstVRefNum,
							  long dstDirID,
							  StringPtr dstName)
/* The destination volume must support the Desktop Manager for this to work */
{
	OSErr error;
	Str255 comment;

	error = DTGetComment(srcVRefNum, srcDirID, srcName, comment);
	if ( (error == noErr) && (comment[0] > 0) )
	{
		error = DTSetComment(dstVRefNum, dstDirID, dstName, comment);
	}
	return (error);
}

/*****************************************************************************/

pascal	OSErr	FSpDTCopyComment(const FSSpec *srcSpec,
							   const FSSpec *dstSpec)
/* The destination volume must support the Desktop Manager for this to work */
{
	return (DTCopyComment(srcSpec->vRefNum, srcSpec->parID, (StringPtr)srcSpec->name,
						dstSpec->vRefNum, dstSpec->parID, (StringPtr)dstSpec->name));
}

/*****************************************************************************/
