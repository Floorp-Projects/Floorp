/*
**	Apple Macintosh Developer Technical Support
**
**	FSSpec compatibility functions.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		FSpCompat.c
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
**	If building application 68K code, set GENERATENODATA to 0 for faster code.
**	If building stand-alone 68K code, set GENERATENODATA to 1 so globals
**		(static variables) are not used.
*/
#define	GENERATENODATA 0

#include <Types.h>
#include <Errors.h>
#include <LowMem.h>
#include <Gestalt.h>
#include <Resources.h>
#include <Script.h>

#define	__COMPILINGMOREFILES

#include "MoreFilesExtras.h"
#include "FSpCompat.h"

/*****************************************************************************/

/* local constants */

enum {
	gestaltBugFixAttrsTwo					= 'bugy',
	gestaltFSpExchangeFilesCompatibilityFix	= 26,
	gestaltBugFixAttrsThree					= 'bugx',
	gestaltFSpCreateScriptSupportFix		= 1
};

/*****************************************************************************/

/* static prototypes */


#if !SystemSevenOrLater
static	Boolean	FSHasFSSpecCalls(void);

static	Boolean	QTHasFSSpecCalls(void);
#endif	/* !SystemSevenOrLater */

#if !SystemSevenFiveOrLater
static	Boolean	HasFSpExchangeFilesCompatibilityFix(void);
#endif	/* !SystemSevenFiveOrLater */

static	Boolean	HasFSpCreateScriptSupportFix(void);

#if !SystemSevenFiveOrLater
static	OSErr	GenerateUniqueName(short volume,
								   long *startSeed,
								   long dir1,
								   long dir2,
								   StringPtr uniqueName);
#endif	/* !SystemSevenFiveOrLater */

/*****************************************************************************/

/* FSHasFSSpecCalls returns true if the file system provides FSSpec calls. */

#if !SystemSevenOrLater
static	Boolean	FSHasFSSpecCalls(void)
{
	long			response;
#if !GENERATENODATA
	static Boolean	tested = false;
	static Boolean	result = false;
#else
	Boolean	result = false;
#endif
	
#if !GENERATENODATA
	if ( !tested )
	{
		tested = true;
#endif
		if ( Gestalt(gestaltFSAttr, &response) == noErr )
		{
			result = ((response & (1L << gestaltHasFSSpecCalls)) != 0);
		}
#if !GENERATENODATA
	}
#endif
	return ( result );
}
#endif	/* !SystemSevenOrLater */

/*****************************************************************************/

/* QTHasFSSpecCalls returns true if QuickTime provides FSSpec calls */
/* except for FSpExchangeFiles. */

#if !SystemSevenOrLater
static	Boolean	QTHasFSSpecCalls(void)
{
	long			response;
#if !GENERATENODATA
	static Boolean	tested = false;
	static Boolean	result = false;
#else
	Boolean	result = false;
#endif
	
#if !GENERATENODATA
	if ( !tested )
	{
		tested = true;
#endif
		result = (Gestalt(gestaltQuickTimeVersion, &response) == noErr);
#if !GENERATENODATA
	}
#endif
	return ( result );
}
#endif	/* !SystemSevenOrLater */

/*****************************************************************************/

/* HasFSpExchangeFilesCompatibilityFix returns true if FSpExchangeFiles */
/* compatibility code has been fixed in system software. */
/* This was fixed by System Update 3.0, so if SystemSevenFiveOrLater */
/* is true, then we know the fix is in. */

#if !SystemSevenFiveOrLater
static	Boolean	HasFSpExchangeFilesCompatibilityFix(void)
{
	long			response;
#if !GENERATENODATA
	static Boolean	tested = false;
	static Boolean	result = false;
#else	/* !GENERATENODATA */
	Boolean	result = false;
#endif	/* !GENERATENODATA */
	
#if !GENERATENODATA
	if ( !tested )
	{
		tested = true;
#endif	/* !GENERATENODATA */
		if ( Gestalt(gestaltBugFixAttrsTwo, &response) == noErr )
		{
			result = ((response & (1L << gestaltFSpExchangeFilesCompatibilityFix)) != 0);
		}
#if !GENERATENODATA
	}
#endif	/* !GENERATENODATA */
	return ( result );
}
#endif	/* !SystemSevenFiveOrLater */

/*****************************************************************************/

/* HasFSpCreateScriptSupportFix returns true if FSpCreate and */
/* FSpCreateResFile have been fixed in system software to correctly set */
/* the scriptCode in the volume's catalog. */
/* This was fixed by System 7.5 Update 1.0 */

static	Boolean	HasFSpCreateScriptSupportFix(void)
{
	long			response;
#if !GENERATENODATA
	static Boolean	tested = false;
	static Boolean	result = false;
#else
	Boolean	result = false;
#endif	/* !GENERATENODATA */
	
#if !GENERATENODATA
	if ( !tested )
	{
		tested = true;
#endif	/* !GENERATENODATA */
		if ( Gestalt(gestaltBugFixAttrsThree, &response) == noErr )
		{
			result = ((response & (1L << gestaltFSpCreateScriptSupportFix)) != 0);
		}
#if !GENERATENODATA
	}
#endif	/* !GENERATENODATA */
	return ( result );
}

/*****************************************************************************/

/*
**	File Manager FSp calls
*/

/*****************************************************************************/

pascal	OSErr	FSMakeFSSpecCompat(short vRefNum,
								   long dirID,
								   ConstStr255Param fileName,
								   FSSpecPtr spec)
{
	OSErr	result;
	
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		/* Let the file system create the FSSpec if it can since it does the job */
		/* much more efficiently than I can. */
		result = FSMakeFSSpec(vRefNum, dirID, fileName, spec);
		/* Fix a bug in Macintosh PC Exchange's MakeFSSpec code where 0 is */
		/* returned in the parID field when making an FSSpec to the volume's */
		/* root directory by passing a full pathname in MakeFSSpec's */
		/* fileName parameter. */
		if ( (result == noErr) && (spec->parID == 0) )
			spec->parID = fsRtParID;
	}
#if !SystemSevenOrLater
	else
	{
		Boolean	isDirectory;
		
		result = GetObjectLocation(vRefNum, dirID, (StringPtr)fileName,
									&(spec->vRefNum), &(spec->parID), spec->name,
									&isDirectory);
	}
#endif	/* !SystemSevenOrLater */
	return ( result );
}

/*****************************************************************************/

pascal	OSErr	FSpOpenDFCompat(const FSSpec *spec,
								char permission,
								short *refNum)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		return ( FSpOpenDF(spec, permission, refNum) );
	}
#if !SystemSevenOrLater
	else
	{
		OSErr			result;
		HParamBlockRec	pb;
		
		pb.ioParam.ioVRefNum = spec->vRefNum;
		pb.fileParam.ioDirID = spec->parID;
		pb.ioParam.ioNamePtr = (StringPtr) &(spec->name);
		pb.ioParam.ioVersNum = 0;
		pb.ioParam.ioPermssn = permission;
		pb.ioParam.ioMisc = NULL;
		result = PBHOpenSync(&pb);	/* OpenDF not supported by System 6, so use Open */
		*refNum = pb.ioParam.ioRefNum;
		return ( result );
	}
#endif	/* !SystemSevenOrLater */
}

/*****************************************************************************/

pascal	OSErr	FSpOpenRFCompat(const FSSpec *spec,
								char permission,
								short *refNum)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		return ( FSpOpenRF(spec, permission, refNum) );
	}
#if !SystemSevenOrLater
	else
	{
		OSErr			result;
		HParamBlockRec	pb;
		
		pb.ioParam.ioVRefNum = spec->vRefNum;
		pb.fileParam.ioDirID = spec->parID;
		pb.ioParam.ioNamePtr = (StringPtr) &(spec->name);
		pb.ioParam.ioVersNum = 0;
		pb.ioParam.ioPermssn = permission;
		pb.ioParam.ioMisc = NULL;
		result = PBHOpenRFSync(&pb);
		*refNum = pb.ioParam.ioRefNum;
		return ( result );
	}
#endif	/* !SystemSevenOrLater */
}

/*****************************************************************************/

pascal	OSErr	FSpCreateCompat(const FSSpec *spec,
								OSType creator,
								OSType fileType,
								ScriptCode scriptTag)
{
	OSErr			result;
	UniversalFMPB	pb;
	
	/* There's no conditional to test for, so this test must be made */
	if ( HasFSpCreateScriptSupportFix() )
	{
		return ( FSpCreate(spec, creator, fileType, scriptTag) );
	}
	/*	If FSpCreate isn't called, this code will be executed */
	pb.hPB.fileParam.ioVRefNum = spec->vRefNum;
	pb.hPB.fileParam.ioDirID = spec->parID;
	pb.hPB.fileParam.ioNamePtr = (StringPtr) &(spec->name);
	pb.hPB.fileParam.ioFVersNum = 0;
	result = PBHCreateSync(&(pb.hPB));
	if ( result == noErr )
	{
		/* get info on created item */
		pb.ciPB.hFileInfo.ioFDirIndex = 0;
		result = PBGetCatInfoSync(&(pb.ciPB));
		if ( result == noErr )
		{
			/* Set fdScript in FXInfo */
			/* The negative script constants (smSystemScript, smCurrentScript, and smAllScripts) */
			/* don't make sense on disk, so only use scriptTag if scriptTag >= smRoman */
			/* (smRoman is 0). fdScript is valid if high bit is set (see IM-6, page 9-38) */
			pb.ciPB.hFileInfo.ioFlXFndrInfo.fdScript = (scriptTag >= smRoman) ?
														((char)scriptTag | (char)0x80) :
														(smRoman);
			/* Set creator/fileType */
			pb.ciPB.hFileInfo.ioFlFndrInfo.fdCreator = creator;
			pb.ciPB.hFileInfo.ioFlFndrInfo.fdType = fileType;
			/* Restore ioDirID field in pb which was changed by PBGetCatInfo */
			pb.ciPB.hFileInfo.ioDirID = spec->parID;
			result = PBSetCatInfoSync(&(pb.ciPB));
		}
	}
	return ( result );
}

/*****************************************************************************/

pascal	OSErr	FSpDirCreateCompat(const FSSpec *spec,
								   ScriptCode scriptTag,
								   long *createdDirID)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		return ( FSpDirCreate(spec, scriptTag, createdDirID) );
	}
#if !SystemSevenOrLater
	else
	{
		OSErr			result;
		UniversalFMPB	pb;
		
		pb.hPB.fileParam.ioVRefNum = spec->vRefNum;
		pb.hPB.fileParam.ioDirID = spec->parID;
		pb.hPB.fileParam.ioNamePtr = (StringPtr) &(spec->name);
		result = PBDirCreateSync(&(pb.hPB));
		*createdDirID = pb.hPB.fileParam.ioDirID;
		if ( result == noErr )
		{
			/* get info on created item */
			pb.ciPB.dirInfo.ioFDirIndex = 0;
			pb.ciPB.dirInfo.ioDrDirID = spec->parID;
			result = PBGetCatInfoSync(&(pb.ciPB));
			if ( result == noErr )
			{
				/* Set frScript in DXInfo */
				/* The negative script constants (smSystemScript, smCurrentScript, and smAllScripts) */
				/* don't make sense on disk, so only use scriptTag if scriptTag >= smRoman */
				/* (smRoman is 0). frScript is valid if high bit is set (see IM-6, page 9-38) */
				pb.ciPB.dirInfo.ioDrFndrInfo.frScript = (scriptTag >= smRoman) ?
															((char)scriptTag | (char)0x80) :
															(smRoman);
				/* Restore ioDirID field in pb which was changed by PBGetCatInfo */
				pb.ciPB.dirInfo.ioDrDirID = spec->parID;			
				result = PBSetCatInfoSync(&(pb.ciPB));
			}
		}
		return ( result );
	}
#endif	/* !SystemSevenOrLater */
}

/*****************************************************************************/

pascal	OSErr	FSpDeleteCompat(const FSSpec *spec)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		return ( FSpDelete(spec) );
	}
#if !SystemSevenOrLater
	else
	{
		HParamBlockRec	pb;
		
		pb.ioParam.ioVRefNum = spec->vRefNum;
		pb.fileParam.ioDirID = spec->parID;
		pb.ioParam.ioNamePtr = (StringPtr) &(spec->name);
		pb.ioParam.ioVersNum = 0;
		return ( PBHDeleteSync(&pb) );
	}
#endif	/* !SystemSevenOrLater */
}

/*****************************************************************************/

pascal	OSErr	FSpGetFInfoCompat(const FSSpec *spec,
								  FInfo *fndrInfo)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		return ( FSpGetFInfo(spec, fndrInfo) );
	}
#if !SystemSevenOrLater
	else
	{
		OSErr			result;
		HParamBlockRec	pb;
		
		pb.fileParam.ioVRefNum = spec->vRefNum;
		pb.fileParam.ioDirID = spec->parID;
		pb.fileParam.ioNamePtr = (StringPtr) &(spec->name);
		pb.fileParam.ioFVersNum = 0;
		pb.fileParam.ioFDirIndex = 0;
		result = PBHGetFInfoSync(&pb);
		*fndrInfo = pb.fileParam.ioFlFndrInfo;
		return ( result );
	}
#endif	/* !SystemSevenOrLater */
}

/*****************************************************************************/

pascal	OSErr	FSpSetFInfoCompat(const FSSpec *spec,
								  const FInfo *fndrInfo)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		return ( FSpSetFInfo(spec, fndrInfo) );
	}
#if !SystemSevenOrLater
	else
	{
		OSErr			result;
		HParamBlockRec	pb;
		
		pb.fileParam.ioVRefNum = spec->vRefNum;
		pb.fileParam.ioDirID = spec->parID;
		pb.fileParam.ioNamePtr = (StringPtr) &(spec->name);
		pb.fileParam.ioFVersNum = 0;
		pb.fileParam.ioFDirIndex = 0;
		result = PBHGetFInfoSync(&pb);
		if ( result == noErr )
		{
			pb.fileParam.ioFlFndrInfo = *fndrInfo;
			pb.fileParam.ioDirID = spec->parID;
			result = PBHSetFInfoSync(&pb);
		}
		return ( result );
	}
#endif	/* !SystemSevenOrLater */
}

/*****************************************************************************/

pascal	OSErr	FSpSetFLockCompat(const FSSpec *spec)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		return ( FSpSetFLock(spec) );
	}
#if !SystemSevenOrLater
	else
	{
		HParamBlockRec	pb;
		
		pb.fileParam.ioVRefNum = spec->vRefNum;
		pb.fileParam.ioDirID = spec->parID;
		pb.fileParam.ioNamePtr = (StringPtr) &(spec->name);
		pb.fileParam.ioFVersNum = 0;
		return ( PBHSetFLockSync(&pb) );
	}
#endif	/* !SystemSevenOrLater */
}

/*****************************************************************************/

pascal	OSErr	FSpRstFLockCompat(const FSSpec *spec)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		return ( FSpRstFLock(spec) );
	}
#if !SystemSevenOrLater
	else
	{
		HParamBlockRec	pb;
		
		pb.fileParam.ioVRefNum = spec->vRefNum;
		pb.fileParam.ioDirID = spec->parID;
		pb.fileParam.ioNamePtr = (StringPtr) &(spec->name);
		pb.fileParam.ioFVersNum = 0;
		return ( PBHRstFLockSync(&pb) );
	}
#endif	/* !SystemSevenOrLater */
}

/*****************************************************************************/

pascal	OSErr	FSpRenameCompat(const FSSpec *spec,
								ConstStr255Param newName)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		return ( FSpRename(spec, newName) );
	}
#if !SystemSevenOrLater
	else
	{
		HParamBlockRec	pb;
		
		pb.ioParam.ioVRefNum = spec->vRefNum;
		pb.fileParam.ioDirID = spec->parID;
		pb.ioParam.ioNamePtr = (StringPtr) &(spec->name);
		pb.ioParam.ioVersNum = 0;
		pb.ioParam.ioMisc = (Ptr) newName;
		return ( PBHRenameSync(&pb) );
	}
#endif	/* !SystemSevenOrLater */
}

/*****************************************************************************/

pascal	OSErr	FSpCatMoveCompat(const FSSpec *source,
								 const FSSpec *dest)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		return ( FSpCatMove(source, dest) );
	}
#if !SystemSevenOrLater
	else
	{
		CMovePBRec	pb;
		
		/* source and destination volume must be the same */
		if ( source->vRefNum != dest->vRefNum )
			return ( paramErr );
		
		pb.ioNamePtr = (StringPtr) &(source->name);
		pb.ioVRefNum = source->vRefNum;
		pb.ioDirID = source->parID;
		pb.ioNewDirID = dest->parID;
		pb.ioNewName = (StringPtr) &(dest->name);
		return ( PBCatMoveSync(&pb) );
	}
#endif	/* !SystemSevenOrLater */
}

/*****************************************************************************/

/* GenerateUniqueName generates a name that is unique in both dir1 and dir2 */
/* on the specified volume. Ripped off from Feldman's code. */

#if !SystemSevenFiveOrLater
static	OSErr	GenerateUniqueName(short volume,
								   long *startSeed,
								   long dir1,
								   long dir2,
								   StringPtr uniqueName)
{
	OSErr			error = noErr;
	long			i;
	CInfoPBRec		cinfo;
	unsigned char	hexStr[16];
	
	for ( i = 0; i < 16; ++i )
	{
		if ( i < 10 )
			hexStr[i] = 0x30 + i;
		else
			hexStr[i] = 0x37 + i;
	}
	
	cinfo.hFileInfo.ioVRefNum = volume;
	cinfo.hFileInfo.ioFDirIndex = 0;
	cinfo.hFileInfo.ioNamePtr = uniqueName;

	while ( error != fnfErr )
	{
		(*startSeed)++;		
		cinfo.hFileInfo.ioNamePtr[0] = 8;
		for ( i = 1; i <= 8; i++ )
		{
			cinfo.hFileInfo.ioNamePtr[i] = hexStr[((*startSeed >> ((8-i)*4)) & 0xf)];
		}
		cinfo.hFileInfo.ioDirID = dir1;
		error = fnfErr;
		for ( i = 1; i <= 2; i++ )
		{
			error = error & PBGetCatInfoSync(&cinfo);
			cinfo.hFileInfo.ioDirID = dir2;
			if ( (error != fnfErr) && (error != noErr) )
				return ( error );
		}
	}
	return ( noErr );
}
#endif	/* !SystemSevenFiveOrLater */

/*****************************************************************************/

pascal	OSErr	FSpExchangeFilesCompat(const FSSpec *source,
									   const FSSpec *dest)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
#if !SystemSevenFiveOrLater
		if ( HasFSpExchangeFilesCompatibilityFix() )
#endif	/* !SystemSevenFiveOrLater */
		{
			return ( FSpExchangeFiles(source, dest) );
		}
	}
#if !SystemSevenFiveOrLater
	/*	If FSpExchangeFiles isn't called, this code will be executed */
	{
		HParamBlockRec			pb;
		CInfoPBRec				catInfoSource, catInfoDest;
		OSErr					result, result2;
		Str31					unique1, unique2;
		StringPtr				unique1Ptr, unique2Ptr, swapola;
		GetVolParmsInfoBuffer	volInfo;
		long					theSeed, temp;
		
		/* Make sure the source and destination are on the same volume */
		if ( source->vRefNum != dest->vRefNum )
		{
			result = diffVolErr;
			goto errorExit3;
		}
		
		/* Try PBExchangeFiles first since it preserves the file ID reference */
		pb.fidParam.ioNamePtr = (StringPtr) &(source->name);
		pb.fidParam.ioVRefNum = source->vRefNum;
		pb.fidParam.ioDestNamePtr = (StringPtr) &(dest->name);
		pb.fidParam.ioDestDirID = dest->parID;
		pb.fidParam.ioSrcDirID = source->parID;
	
		result = PBExchangeFilesSync(&pb);
	
		/* Note: The compatibility case won't work for files with *Btree control blocks. */
		/* Right now the only *Btree files are created by the system. */
		if ( result != noErr )
		{
			pb.ioParam.ioNamePtr = NULL;
			pb.ioParam.ioBuffer = (Ptr) &volInfo;
			pb.ioParam.ioReqCount = sizeof(volInfo);
			result2 = PBHGetVolParmsSync(&pb);
			
			/* continue if volume has no fileID support (or no GetVolParms support) */
			if ( (result2 == noErr) && hasFileIDs(volInfo) )
				goto errorExit3;
	
			/* Get the catalog information for each file */
			/* and make sure both files are *really* files */
			catInfoSource.hFileInfo.ioVRefNum = source->vRefNum;
			catInfoSource.hFileInfo.ioFDirIndex = 0;
			catInfoSource.hFileInfo.ioNamePtr = (StringPtr) &(source->name);
			catInfoSource.hFileInfo.ioDirID = source->parID;
			catInfoSource.hFileInfo.ioACUser = 0; /* ioACUser used to be filler2 */
			result = PBGetCatInfoSync(&catInfoSource);
			if ( result != noErr )
				goto errorExit3;
			if ( (catInfoSource.hFileInfo.ioFlAttrib & ioDirMask) != 0 )
			{
				result = notAFileErr;
				goto errorExit3;
			}
			
			catInfoDest.hFileInfo.ioVRefNum = dest->vRefNum;
			catInfoDest.hFileInfo.ioFDirIndex = 0;
			catInfoDest.hFileInfo.ioNamePtr = (StringPtr) &(dest->name);
			catInfoDest.hFileInfo.ioDirID = dest->parID;
			catInfoDest.hFileInfo.ioACUser = 0; /* ioACUser used to be filler2 */
			result = PBGetCatInfoSync(&catInfoDest);
			if ( result != noErr )
				goto errorExit3;
			if ( (catInfoDest.hFileInfo.ioFlAttrib & ioDirMask) != 0 )
			{
				result = notAFileErr;
				goto errorExit3;
			}
			
			/* generate 2 filenames that are unique in both directories */
			theSeed = 0x64666A6C;	/* a fine unlikely filename */
			unique1Ptr = (StringPtr)&unique1;
			unique2Ptr = (StringPtr)&unique2;
			
			result = GenerateUniqueName(source->vRefNum, &theSeed, source->parID, dest->parID, unique1Ptr);
			if ( result != noErr )
				goto errorExit3;
	
			GenerateUniqueName(source->vRefNum, &theSeed, source->parID, dest->parID, unique2Ptr);
			if ( result != noErr )
				goto errorExit3;
	
			/* rename source to unique1 */
			pb.fileParam.ioNamePtr = (StringPtr) &(source->name);
			pb.ioParam.ioMisc = (Ptr) unique1Ptr;
			pb.ioParam.ioVersNum = 0;
			result = PBHRenameSync(&pb);
			if ( result != noErr )
				goto errorExit3;
			
			/* rename dest to unique2 */
			pb.ioParam.ioMisc = (Ptr) unique2Ptr;
			pb.ioParam.ioVersNum = 0;
			pb.fileParam.ioNamePtr = (StringPtr) &(dest->name);
			pb.fileParam.ioDirID = dest->parID;
			result = PBHRenameSync(&pb);
			if ( result != noErr )
				goto errorExit2;	/* back out gracefully by renaming unique1 back to source */
				
			/* If files are not in same directory, swap their locations */
			if ( source->parID != dest->parID )
			{
				/* move source file to dest directory */
				pb.copyParam.ioNamePtr = unique1Ptr;
				pb.copyParam.ioNewName = NULL;
				pb.copyParam.ioNewDirID = dest->parID;
				pb.copyParam.ioDirID = source->parID;
				result = PBCatMoveSync((CMovePBPtr) &pb);
				if ( result != noErr )
					goto errorExit1;	/* back out gracefully by renaming both files to original names */
				
				/* move dest file to source directory */
				pb.copyParam.ioNamePtr = unique2Ptr;
				pb.copyParam.ioNewDirID = source->parID;
				pb.copyParam.ioDirID = dest->parID;
				result = PBCatMoveSync((CMovePBPtr) &pb);
				if ( result != noErr)
				{
					/* life is very bad.  We'll at least try to move source back */
					pb.copyParam.ioNamePtr = unique1Ptr;
					pb.copyParam.ioNewName = NULL;
					pb.copyParam.ioNewDirID = source->parID;
					pb.copyParam.ioDirID = dest->parID;
					(void) PBCatMoveSync((CMovePBPtr) &pb);	/* ignore errors */
					goto errorExit1;	/* back out gracefully by renaming both files to original names */
				}
			}
			
			/* Make unique1Ptr point to file in source->parID */
			/* and unique2Ptr point to file in dest->parID */
			/* This lets us fall through to the rename code below */
			swapola = unique1Ptr;
			unique1Ptr = unique2Ptr;
			unique2Ptr = swapola;
	
			/* At this point, the files are in their new locations (if they were moved) */
			/* Source is named Unique1 (name pointed to by unique2Ptr) and is in dest->parID */
			/* Dest is named Unique2 (name pointed to by unique1Ptr) and is in source->parID */
			/* Need to swap attributes except mod date and swap names */
	
			/* swap the catalog info by re-aiming the CInfoPB's */
			catInfoSource.hFileInfo.ioNamePtr = unique1Ptr;
			catInfoDest.hFileInfo.ioNamePtr = unique2Ptr;
			
			catInfoSource.hFileInfo.ioDirID = source->parID;
			catInfoDest.hFileInfo.ioDirID = dest->parID;
			
			/* Swap the original mod dates with each file */
			temp = catInfoSource.hFileInfo.ioFlMdDat;
			catInfoSource.hFileInfo.ioFlMdDat = catInfoDest.hFileInfo.ioFlMdDat;
			catInfoDest.hFileInfo.ioFlMdDat = temp;
			
			/* Here's the swap (ignore errors) */
			(void) PBSetCatInfoSync(&catInfoSource); 
			(void) PBSetCatInfoSync(&catInfoDest);
			
			/* rename unique2 back to dest */
errorExit1:
			pb.ioParam.ioMisc = (Ptr) &(dest->name);
			pb.ioParam.ioVersNum = 0;
			pb.fileParam.ioNamePtr = unique2Ptr;
			pb.fileParam.ioDirID = dest->parID;
			(void) PBHRenameSync(&pb);	/* ignore errors */
	
			/* rename unique1 back to source */
errorExit2:
			pb.ioParam.ioMisc = (Ptr) &(source->name);
			pb.ioParam.ioVersNum = 0;
			pb.fileParam.ioNamePtr = unique1Ptr;
			pb.fileParam.ioDirID = source->parID;
			(void) PBHRenameSync(&pb); /* ignore errors */
		}
errorExit3: { /* null statement */ }
		return ( result );
	}
#endif	/* !SystemSevenFiveOrLater */
}

/*****************************************************************************/

/* 
**	Resource Manager FSp calls
*/

/*****************************************************************************/

pascal	short	FSpOpenResFileCompat(const FSSpec *spec,
									 SignedByte permission)
{
#if !SystemSevenOrLater
	if ( FSHasFSSpecCalls() || QTHasFSSpecCalls() )
#endif	/* !SystemSevenOrLater */
	{
		return ( FSpOpenResFile(spec, permission) );
	}
#if !SystemSevenOrLater
	else
	{
		return ( HOpenResFile(spec->vRefNum, spec->parID, spec->name, permission) );
	}
#endif	/* !SystemSevenOrLater */
}

/*****************************************************************************/

pascal	void	FSpCreateResFileCompat(const FSSpec *spec,
									   OSType creator,
									   OSType fileType,
									   ScriptCode scriptTag)
{	
	if ( HasFSpCreateScriptSupportFix() )
	{
		FSpCreateResFile(spec, creator, fileType, scriptTag);
		return;
	}
	/*	If FSpCreateResFile isn't called, this code will be executed */
	{
		OSErr			result;
		CInfoPBRec		pb;
		
		HCreateResFile(spec->vRefNum, spec->parID, spec->name);
		if ( ResError() == noErr )
		{
			/* get info on created item */
			pb.hFileInfo.ioVRefNum = spec->vRefNum;
			pb.hFileInfo.ioDirID = spec->parID;
			pb.hFileInfo.ioNamePtr = (StringPtr) &(spec->name);
			pb.hFileInfo.ioFDirIndex = 0;
			result = PBGetCatInfoSync(&pb);
			if ( result == noErr )
			{
				/* Set fdScript in FXInfo */
				/* The negative script constants (smSystemScript, smCurrentScript, and smAllScripts) */
				/* don't make sense on disk, so only use scriptTag if scriptTag >= smRoman */
				/* (smRoman is 0). fdScript is valid if high bit is set (see IM-6, page 9-38) */
				pb.hFileInfo.ioFlXFndrInfo.fdScript = (scriptTag >= smRoman) ?
														((char)scriptTag | (char)0x80) :
														(smRoman);
				/* Set creator/fileType */
				pb.hFileInfo.ioFlFndrInfo.fdCreator = creator;
				pb.hFileInfo.ioFlFndrInfo.fdType = fileType;
				
				/* Restore ioDirID field in pb which was changed by PBGetCatInfo */
				pb.hFileInfo.ioDirID = spec->parID;
				result = PBSetCatInfoSync(&pb);
			}
			/* Set ResErr low memory global to result */
			LMSetResErr(result);
		}
		return;
	}
}

/*****************************************************************************/
