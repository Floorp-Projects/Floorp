/*
**	Apple Macintosh Developer Technical Support
**
**	IndexedSearch and the PBCatSearch compatibility function.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		Search.c
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
 *
 * To avoid potential conflicts with other files named search renamed to
 * MoreFilesSearch.c
 */


#include <MacTypes.h>
#include <Gestalt.h>
#include <Timer.h>
#include <MacErrors.h>
#include <Memory.h>
#include <Files.h>
#include <TextUtils.h>

#define	__COMPILINGMOREFILES

#include "MoreFiles.h"
#include "MoreFilesExtras.h"
#include "MoreFilesSearch.h"

/*****************************************************************************/

enum
{
	/* Number of LevelRecs to add each time the searchStack is grown */
	/* 20 levels is probably more than reasonable for most volumes. */
	/* If more are needed, they are allocated 20 levels at a time. */
	kAdditionalLevelRecs = 20
};

/*****************************************************************************/

/*
**	LevelRecs are used to store the directory ID and index whenever
**	IndexedSearch needs to either scan a sub-directory, or return control
**	to the caller because the call has timed out or the number of
**	matches requested has been found. LevelRecs are stored in an array
**	used as a stack.
*/
struct	LevelRec
{
	long	dirModDate;	/* for detecting most (but not all) catalog changes */
	long	dirID;
	short	index;
};
typedef struct LevelRec LevelRec;
typedef LevelRec *LevelRecPtr, **LevelRecHandle;


/*
**	SearchPositionRec is my version of a CatPositionRec. It holds the
**	information I need to resuming searching.
*/
#if PRAGMA_STRUCT_ALIGN
#pragma options align=mac68k
#endif
struct SearchPositionRec
{
	long			initialize;		/* Goofy checksum of volume information used to make */
									/* sure we're resuming a search on the same volume. */
	unsigned short	stackDepth;		/* Current depth on searchStack. */
	short			priv[11];		/* For future use... */
};
#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif
typedef struct SearchPositionRec SearchPositionRec;
typedef SearchPositionRec *SearchPositionRecPtr;


/*
**	ExtendedTMTask is a TMTask record extended to hold the timer flag.
*/
#if PRAGMA_STRUCT_ALIGN
#pragma options align=mac68k
#endif
struct ExtendedTMTask
{
	TMTask			theTask;
	Boolean			stopSearch;		/* the Time Mgr task will set stopSearch to */
									/* true when the timer expires */
};
#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif
typedef struct ExtendedTMTask ExtendedTMTask;
typedef ExtendedTMTask *ExtendedTMTaskPtr;

/*****************************************************************************/

static	OSErr	CheckVol(ConstStr255Param pathname,
						 short vRefNum,
						 short *realVRefNum,
						 long *volID);

static	OSErr	CheckStack(unsigned short stackDepth,
						   LevelRecHandle searchStack,
						   Size *searchStackSize);

static	OSErr	VerifyUserPB(CSParamPtr userPB,
							 Boolean *includeFiles,
							 Boolean *includeDirs,
							 Boolean *includeNames);

static	Boolean	IsSubString(ConstStr255Param aStringPtr,
							ConstStr255Param subStringPtr);

static	Boolean	CompareMasked(const long *data1,
							  const long *data2,
							  const long *mask,
							  short longsToCompare);

static	void	CheckForMatches(CInfoPBPtr cPB,
								CSParamPtr userPB,
								const Str63 matchName,
								Boolean includeFiles,
								Boolean includeDirs);

#if	__WANTPASCALELIMINATION
#undef	pascal
#endif

#if TARGET_RT_MAC_CFM || defined(XP_MACOSX)

static	pascal	void	TimeOutTask(TMTaskPtr tmTaskPtr);

#else

static	pascal	TMTaskPtr	GetTMTaskPtr(void);

static	void	TimeOutTask(void);

#endif

#if	__WANTPASCALELIMINATION
#define	pascal	
#endif

static	long	GetDirModDate(short vRefNum,
							  long dirID);

/*****************************************************************************/

/*
**	CheckVol gets the volume's real vRefNum and builds a volID. The volID
**	is used to help insure that calls to resume searching with IndexedSearch
**	are to the same volume as the last call to IndexedSearch.
*/
static	OSErr	CheckVol(ConstStr255Param pathname,
						 short vRefNum,
						 short *realVRefNum,
						 long *volID)
{
	HParamBlockRec pb;
	OSErr error;

	error = GetVolumeInfoNoName(pathname, vRefNum, &pb);
	if ( error == noErr )
	{
		/* Return the real vRefNum */
		*realVRefNum = pb.volumeParam.ioVRefNum;

		/* Add together a bunch of things that aren't supposed to change on */
		/* a mounted volume that's being searched and that should come up with */
		/* a fairly unique number */
		*volID = pb.volumeParam.ioVCrDate +
				 pb.volumeParam.ioVRefNum +
				 pb.volumeParam.ioVNmAlBlks +
				 pb.volumeParam.ioVAlBlkSiz +
				 pb.volumeParam.ioVFSID;
	}
	return ( error );
}

/*****************************************************************************/

/*
**	CheckStack checks the size of the search stack (array) to see if there's
**	room to push another LevelRec. If not, CheckStack grows the stack by
**	another kAdditionalLevelRecs elements.
*/
static	OSErr	CheckStack(unsigned short stackDepth,
						   LevelRecHandle searchStack,
						   Size *searchStackSize)
{
	OSErr	result;
	
	if ( (*searchStackSize / sizeof(LevelRec)) == (stackDepth + 1) )
	{
		/* Time to grow stack */
		SetHandleSize((Handle)searchStack, *searchStackSize + (kAdditionalLevelRecs * sizeof(LevelRec)));
		result = MemError();	/* should be noErr */
		*searchStackSize = GetHandleSize((Handle)searchStack);
	}
	else
	{
		result = noErr;
	}
	
	return ( result );
}

/*****************************************************************************/

/*
**	VerifyUserPB makes sure the parameter block passed to IndexedSearch has
**	valid parameters. By making this check once, we don't have to worry about
**	things like NULL pointers, strings being too long, etc.
**	VerifyUserPB also determines if the search includes files and/or
**	directories, and determines if a full or partial name search was requested.
*/
static	OSErr	VerifyUserPB(CSParamPtr userPB,
							 Boolean *includeFiles,
							 Boolean *includeDirs,
							 Boolean *includeNames)
{
	CInfoPBPtr	searchInfo1;
	CInfoPBPtr	searchInfo2;
	
	searchInfo1 = userPB->ioSearchInfo1;
	searchInfo2 = userPB->ioSearchInfo2;
	
	/* ioMatchPtr cannot be NULL */
	if ( userPB->ioMatchPtr == NULL )
	{
		goto ParamErrExit;
	}
	
	/* ioSearchInfo1 cannot be NULL */
	if ( searchInfo1 == NULL )
	{
		goto ParamErrExit;
	}
	
	/* If any bits except partialName, fullName, or negate are set, then */
	/* ioSearchInfo2 cannot be NULL because information in ioSearchInfo2 is required  */
	if ( ((userPB->ioSearchBits & ~(fsSBPartialName | fsSBFullName | fsSBNegate)) != 0) &&
		 ( searchInfo2 == NULL ))
	{
		goto ParamErrExit;
	}
	
	*includeFiles = false;
	*includeDirs = false;
	*includeNames = false;
	
	if ( (userPB->ioSearchBits & (fsSBPartialName | fsSBFullName)) != 0 )
	{
		/* If any kind of name matching is requested, then ioNamePtr in */
		/* ioSearchInfo1 cannot be NULL or a zero-length string */
		if ( (searchInfo1->hFileInfo.ioNamePtr == NULL) ||
			 (searchInfo1->hFileInfo.ioNamePtr[0] == 0) ||
			 (searchInfo1->hFileInfo.ioNamePtr[0] > (sizeof(Str63) - 1)) )
		{
			goto ParamErrExit;
		}
		
		*includeNames = true;
	}
	
	if ( (userPB->ioSearchBits & fsSBFlAttrib) != 0 )
	{
		/* The only attributes you can search on are the directory flag */
		/* and the locked flag. */
		if ( (searchInfo2->hFileInfo.ioFlAttrib & ~(ioDirMask | 0x01)) != 0 )
		{
			goto ParamErrExit;
		}
		
		/* interested in the directory bit? */
		if ( (searchInfo2->hFileInfo.ioFlAttrib & ioDirMask) != 0 )
		{
			/* yes, so do they want just directories or just files? */
			if ( (searchInfo1->hFileInfo.ioFlAttrib & ioDirMask) != 0 )
			{
				*includeDirs = true;
			}
			else
			{
				*includeFiles = true;
			}
		}
		else
		{
			/* no interest in directory bit - get both files and directories */
			*includeDirs = true;
			*includeFiles = true;
		}
	}
	else
	{
		/* no attribute checking - get both files and directories */
		*includeDirs = true;
		*includeFiles = true;
	}
	
	/* If directories are included in the search, */
	/* then the locked attribute cannot be requested. */
	if ( *includeDirs &&
		 ((userPB->ioSearchBits & fsSBFlAttrib) != 0) &&
		 ((searchInfo2->hFileInfo.ioFlAttrib & 0x01) != 0) )
	{
		goto ParamErrExit;
	}
	
	/* If files are included in the search, then there cannot be */
	/* a search on the number of files. */
	if ( *includeFiles &&
		 ((userPB->ioSearchBits & fsSBDrNmFls) != 0) )
	{
		goto ParamErrExit;
	}
	
	/* If directories are included in the search, then there cannot */
	/* be a search on file lengths. */
	if ( *includeDirs &&
		 ((userPB->ioSearchBits & (fsSBFlLgLen | fsSBFlPyLen | fsSBFlRLgLen | fsSBFlRPyLen)) != 0) )
	{
		goto ParamErrExit;
	}
	
	return ( noErr );
		 
ParamErrExit:
	return ( paramErr );
}

/*****************************************************************************/

/*
**	IsSubString checks to see if a string is a substring of another string.
**	Both input strings have already been converted to all uppercase using
**	UprString (the same non-international call the File Manager uses).
*/
static	Boolean	IsSubString(ConstStr255Param aStringPtr,
							ConstStr255Param subStringPtr)
{
	short	strLength;		/* length of string */
	short	subStrLength;	/* length of subString */
	Boolean	found;			/* result of test */
	short	index;			/* current index into string */
	
	found = false;
	strLength = aStringPtr[0];
	subStrLength = subStringPtr[0];
		
	if ( subStrLength <= strLength)
	{
		register short	count;			/* search counter */
		register short	strIndex;		/* running index into string */
		register short	subStrIndex;	/* running index into subString */
		
		/* start looking at first character */
		index = 1;
		
		/* continue looking until remaining string is shorter than substring */
		count = strLength - subStrLength + 1;
		
		do
		{
			strIndex = index;	/* start string index at index */
			subStrIndex = 1;	/* start subString index at 1 */
			
			while ( !found && (aStringPtr[strIndex] == subStringPtr[subStrIndex]) )
			{
				if ( subStrIndex == subStrLength )
				{
					/* all characters in subString were found */
					found = true;
				}
				else
				{
					/* check next character of substring against next character of string */
					++subStrIndex;
					++strIndex;
				}
			}
			
			if ( !found )
			{
				/* start substring search again at next string character */
				++index;
				--count;
			}
		} while ( count != 0 && (!found) );
	}
	
	return ( found );
}

/*****************************************************************************/

/*
**	CompareMasked does a bitwise comparison with mask on 1 or more longs.
**	data1 and data2 are first exclusive-ORed together resulting with bits set
**	where they are different. That value is then ANDed with the mask resulting
**	with bits set if the test fails. true is returned if the tests pass.
*/
static	Boolean	CompareMasked(const long *data1,
							  const long *data2,
							  const long *mask,
							  short longsToCompare)
{
	Boolean	result = true;
	
	while ( (longsToCompare != 0) && (result == true) )
	{
		/* (*data1 ^ *data2) = bits that are different, so... */
		/* ((*data1 ^ *data2) & *mask) = bits that are different that we're interested in */
		
		if ( ((*data1 ^ *data2) & *mask) != 0 )
			result = false;
		
		++data1;
		++data2;
		++mask;
		--longsToCompare;
	}
	
	return ( result );
}

/*****************************************************************************/

/*
**	Check for matches compares the search criteria in userPB to the file
**	system object in cPB. If there's a match, then the information in cPB is
**	is added to the match array and the actual match count is incremented.
*/
static	void	CheckForMatches(CInfoPBPtr cPB,
								CSParamPtr userPB,
								const Str63 matchName,
								Boolean includeFiles,
								Boolean includeDirs)
{
	long		searchBits;
	CInfoPBPtr	searchInfo1;
	CInfoPBPtr	searchInfo2;
	Str63		itemName;		/* copy of object's name for partial name matching */
	Boolean		foundMatch;
	
	foundMatch = false;			/* default to no match */
	
	searchBits = userPB->ioSearchBits;
	searchInfo1 = userPB->ioSearchInfo1;
	searchInfo2 = userPB->ioSearchInfo2;
	
	/* Into the if statements that go on forever... */
	
	if ( (cPB->hFileInfo.ioFlAttrib & ioDirMask) == 0 )
	{
		if (!includeFiles)
		{
			goto Failed;
		}
	}
	else
	{
		if (!includeDirs)
		{
			goto Failed;
		}
	}
	
	if ( (searchBits & fsSBPartialName) != 0 )
	{
		if ( (cPB->hFileInfo.ioNamePtr[0] > 0) &&
			 (cPB->hFileInfo.ioNamePtr[0] <= (sizeof(Str63) - 1)) )
		{
			/* Make uppercase copy of object name */
			BlockMoveData(cPB->hFileInfo.ioNamePtr,
							itemName,
							cPB->hFileInfo.ioNamePtr[0] + 1);
			/* Use the same non-international call the File Manager uses */
			UpperString(itemName, true);
		}
		else
		{
			goto Failed;
		}
		
		{
			if ( !IsSubString(itemName, matchName) )
			{
				goto Failed;
			}
			else if ( searchBits == fsSBPartialName )
			{
				/* optimize for name matching only since it is most common way to search */
				goto Hit;
			}
		}
	}
	
	if ( (searchBits & fsSBFullName) != 0 )
	{
		/* Use the same non-international call the File Manager uses */
		if ( !EqualString(cPB->hFileInfo.ioNamePtr, matchName, false, true) )
		{
			goto Failed;
		}
		else if ( searchBits == fsSBFullName )
		{
			/* optimize for name matching only since it is most common way to search */
			goto Hit;
		}
	}
	
	if ( (searchBits & fsSBFlParID) != 0 )
	{
		if ( ((unsigned long)(cPB->hFileInfo.ioFlParID) < (unsigned long)(searchInfo1->hFileInfo.ioFlParID)) ||
			 ((unsigned long)(cPB->hFileInfo.ioFlParID) > (unsigned long)(searchInfo2->hFileInfo.ioFlParID)) )
		{
			goto Failed;
		}
	}
	
	if ( (searchBits & fsSBFlAttrib) != 0 )
	{
		if ( ((cPB->hFileInfo.ioFlAttrib ^ searchInfo1->hFileInfo.ioFlAttrib) &
			  searchInfo2->hFileInfo.ioFlAttrib) != 0 )
		{
			goto Failed;
		}
	}
	
	if ( (searchBits & fsSBDrNmFls) != 0 )
	{
		if ( ((unsigned long)(cPB->dirInfo.ioDrNmFls) < (unsigned long)(searchInfo1->dirInfo.ioDrNmFls)) ||
			 ((unsigned long)(cPB->dirInfo.ioDrNmFls) > (unsigned long)(searchInfo2->dirInfo.ioDrNmFls)) )
		{
			goto Failed;
		}
	}

	if ( (searchBits & fsSBFlFndrInfo) != 0 )	/* fsSBFlFndrInfo is same as fsSBDrUsrWds */
	{
		if ( !CompareMasked((long *)&(cPB->hFileInfo.ioFlFndrInfo),
							(long *)&(searchInfo1->hFileInfo.ioFlFndrInfo),
							(long *)&(searchInfo2->hFileInfo.ioFlFndrInfo),
							sizeof(FInfo) / sizeof(long)) )
		{
			goto Failed;
		}
	}
	
	if ( (searchBits & fsSBFlXFndrInfo) != 0 )	/* fsSBFlXFndrInfo is same as fsSBDrFndrInfo */
	{
		if ( !CompareMasked((long *)&(cPB->hFileInfo.ioFlXFndrInfo),
							(long *)&(searchInfo1->hFileInfo.ioFlXFndrInfo),
							(long *)&(searchInfo2->hFileInfo.ioFlXFndrInfo),
							sizeof(FXInfo) / sizeof(long)) )
		{
			goto Failed;
		}
	}
	
	if ( (searchBits & fsSBFlLgLen) != 0 )
	{
		if ( ((unsigned long)(cPB->hFileInfo.ioFlLgLen) < (unsigned long)(searchInfo1->hFileInfo.ioFlLgLen)) ||
			 ((unsigned long)(cPB->hFileInfo.ioFlLgLen) > (unsigned long)(searchInfo2->hFileInfo.ioFlLgLen)) )
		{
			goto Failed;
		}
	}

	if ( (searchBits & fsSBFlPyLen) != 0 )
	{
		if ( ((unsigned long)(cPB->hFileInfo.ioFlPyLen) < (unsigned long)(searchInfo1->hFileInfo.ioFlPyLen)) ||
			 ((unsigned long)(cPB->hFileInfo.ioFlPyLen) > (unsigned long)(searchInfo2->hFileInfo.ioFlPyLen)) )
		{
			goto Failed;
		}
	}

	if ( (searchBits & fsSBFlRLgLen) != 0 )
	{
		if ( ((unsigned long)(cPB->hFileInfo.ioFlRLgLen) < (unsigned long)(searchInfo1->hFileInfo.ioFlRLgLen)) ||
			 ((unsigned long)(cPB->hFileInfo.ioFlRLgLen) > (unsigned long)(searchInfo2->hFileInfo.ioFlRLgLen)) )
		{
			goto Failed;
		}
	}

	if ( (searchBits & fsSBFlRPyLen) != 0 )
	{
		if ( ((unsigned long)(cPB->hFileInfo.ioFlRPyLen) < (unsigned long)(searchInfo1->hFileInfo.ioFlRPyLen)) ||
			 ((unsigned long)(cPB->hFileInfo.ioFlRPyLen) > (unsigned long)(searchInfo2->hFileInfo.ioFlRPyLen)) )
		{
			goto Failed;
		}
	}

	if ( (searchBits & fsSBFlCrDat) != 0 )	/* fsSBFlCrDat is same as fsSBDrCrDat */
	{
		if ( ((unsigned long)(cPB->hFileInfo.ioFlCrDat) < (unsigned long)(searchInfo1->hFileInfo.ioFlCrDat)) ||
			 ((unsigned long)(cPB->hFileInfo.ioFlCrDat) > (unsigned long)(searchInfo2->hFileInfo.ioFlCrDat)) )
		{
			goto Failed;
		}
	}

	if ( (searchBits & fsSBFlMdDat) != 0 )	/* fsSBFlMdDat is same as fsSBDrMdDat */
	{
		if ( ((unsigned long)(cPB->hFileInfo.ioFlMdDat) < (unsigned long)(searchInfo1->hFileInfo.ioFlMdDat)) ||
			 ((unsigned long)(cPB->hFileInfo.ioFlMdDat) > (unsigned long)(searchInfo2->hFileInfo.ioFlMdDat)) )
		{
			goto Failed;
		}
	}

	if ( (searchBits & fsSBFlBkDat) != 0 )	/* fsSBFlBkDat is same as fsSBDrBkDat */
	{
		if ( ((unsigned long)(cPB->hFileInfo.ioFlBkDat) < (unsigned long)(searchInfo1->hFileInfo.ioFlBkDat)) ||
			 ((unsigned long)(cPB->hFileInfo.ioFlBkDat) > (unsigned long)(searchInfo2->hFileInfo.ioFlBkDat)) )
		{
			goto Failed;
		}
	}

	/* Hey, we passed all of the tests! */
	
Hit:
	foundMatch = true;

/* foundMatch is false if code jumps to Failed */
Failed:
	/* Do we reverse our findings? */
	if ( (searchBits & fsSBNegate) != 0 )
	{
		foundMatch = !foundMatch;	/* matches are not, not matches are */
	}
	
	if ( foundMatch )
	{

		/* Move the match into the match buffer */
		userPB->ioMatchPtr[userPB->ioActMatchCount].vRefNum = cPB->hFileInfo.ioVRefNum;
		userPB->ioMatchPtr[userPB->ioActMatchCount].parID = cPB->hFileInfo.ioFlParID;
		if ( cPB->hFileInfo.ioNamePtr[0] > 63 )
		{
			cPB->hFileInfo.ioNamePtr[0] = 63;
		}
		BlockMoveData(cPB->hFileInfo.ioNamePtr,
					  userPB->ioMatchPtr[userPB->ioActMatchCount].name,
					  cPB->hFileInfo.ioNamePtr[0] + 1);
		
		/* increment the actual count */
		++(userPB->ioActMatchCount);
	}
}

/*****************************************************************************/

/*
**	TimeOutTask is executed when the timer goes off. It simply sets the
**	stopSearch field to true. After each object is found and possibly added
**	to the matches buffer, stopSearch is checked to see if the search should
**	continue.
*/

#if	__WANTPASCALELIMINATION
#undef	pascal
#endif

#if TARGET_RT_MAC_CFM || defined(XP_MACOSX)

static	pascal	void	TimeOutTask(TMTaskPtr tmTaskPtr)
{
	((ExtendedTMTaskPtr)tmTaskPtr)->stopSearch = true;
}

#else

static	pascal	TMTaskPtr	GetTMTaskPtr(void)
	ONEWORDINLINE(0x2e89);	/* MOVE.L A1,(SP) */
	
static	void	TimeOutTask(void)
{
	((ExtendedTMTaskPtr)GetTMTaskPtr())->stopSearch = true;
}

#endif

#if	__WANTPASCALELIMINATION
#define	pascal	
#endif

/*****************************************************************************/

/*
**	GetDirModDate returns the modification date of a directory. If there is
**	an error getting the modification date, -1 is returned to indicate
**	something went wrong.
*/
static	long	GetDirModDate(short vRefNum,
							  long dirID)
{
	CInfoPBRec pb;
	Str31 tempName;
	long modDate;

	/* Protection against File Sharing problem */
	tempName[0] = 0;
	pb.dirInfo.ioNamePtr = tempName;
	pb.dirInfo.ioVRefNum = vRefNum;
	pb.dirInfo.ioDrDirID = dirID;
	pb.dirInfo.ioFDirIndex = -1;	/* use ioDrDirID */
	
	if ( PBGetCatInfoSync(&pb) == noErr )
	{
		modDate = pb.dirInfo.ioDrMdDat;
	}
	else
	{
		modDate = -1;
	}
	
	return ( modDate );
}

/*****************************************************************************/

pascal	OSErr	IndexedSearch(CSParamPtr pb,
							  long dirID)
{
#if !TARGET_CARBON
/* NSCP - pinkerton
 * some of the timer stuff is mismatched for carbon. We don't use it anyway.
 */
	static LevelRecHandle	searchStack = NULL;		/* static handle to LevelRec stack */
	static Size				searchStackSize = 0;	/* size of static handle */
	SearchPositionRecPtr	catPosition;
	long					modDate;
	short					index;
	ExtendedTMTask			timerTask;
	OSErr					result;
	short					realVRefNum;
	Str63					itemName;
	CInfoPBRec				cPB;
	long					tempLong;
	Boolean					includeFiles;
	Boolean					includeDirs;
	Boolean					includeNames;
	Str63					upperName;
	
	timerTask.stopSearch = false;	/* don't stop yet! */
	
	/* If request has a timeout, install a Time Manager task. */
	if ( pb->ioSearchTime != 0 )
	{
		/* Start timer */
		timerTask.theTask.tmAddr = NewTimerProc(TimeOutTask);
		InsTime((QElemPtr)&(timerTask.theTask));
		PrimeTime((QElemPtr)&(timerTask.theTask), pb->ioSearchTime);
	}
	
	/* Check the parameter block passed for things that we don't want to assume */
	/* are OK later in the code. For example, make sure pointers to data structures */
	/* and buffers are not NULL.  And while we're in there, see if the request */
	/* specified searching for files, directories, or both, and see if the search */
	/* was by full or partial name. */
	result = VerifyUserPB(pb, &includeFiles, &includeDirs, &includeNames);
	if ( result == noErr )
	{
		pb->ioActMatchCount = 0;	/* no matches yet */
	
		if ( includeNames )
		{
			/* The search includes seach by full or partial name. */
			/* Make an upper case copy of the match string to pass to */
			/* CheckForMatches. */
			BlockMoveData(pb->ioSearchInfo1->hFileInfo.ioNamePtr,
							upperName,
							pb->ioSearchInfo1->hFileInfo.ioNamePtr[0] + 1);
			/* Use the same non-international call the File Manager uses */
			UpperString(upperName, true);
		}
		
		/* Prevent casting to my type throughout code */
		catPosition = (SearchPositionRecPtr)&pb->ioCatPosition;
		
		/* Create searchStack first time called */
		if ( searchStack == NULL )
		{
			searchStack = (LevelRecHandle)NewHandle(kAdditionalLevelRecs * sizeof(LevelRec));
		}
		
		/* Make sure searchStack really exists */
		if ( searchStack != NULL )
		{
			searchStackSize = GetHandleSize((Handle)searchStack);
			
			/* See if the search is a new search or a resumed search. */
			if ( catPosition->initialize == 0 )
			{
				/* New search. */
				
				/* Get the real vRefNum and fill in catPosition->initialize. */ 
				result = CheckVol(pb->ioNamePtr, pb->ioVRefNum, &realVRefNum, &catPosition->initialize);
				if ( result == noErr )
				{
					/* clear searchStack */
					catPosition->stackDepth = 0;
					
					/* use dirID parameter passed and... */
					index = -1;	/* start with the passed directory itself! */
				}
			}
			else
			{
				/* We're resuming a search. */
	
				/* Get the real vRefNum and make sure catPosition->initialize is valid. */ 
				result = CheckVol(pb->ioNamePtr, pb->ioVRefNum, &realVRefNum, &tempLong);
				if ( result == noErr )
				{
					/* Make sure the resumed search is to the same volume! */
					if ( catPosition->initialize == tempLong )
					{
						/* For resume, catPosition->stackDepth > 0 */
						if ( catPosition->stackDepth > 0 )
						{
							/* Position catPosition->stackDepth to access last saved level */
							--(catPosition->stackDepth);
			
							/* Get the dirID and index for the next item */
							dirID = (*searchStack)[catPosition->stackDepth].dirID;
							index = (*searchStack)[catPosition->stackDepth].index;
							
							/* Check the dir's mod date against the saved mode date on our "stack" */
							modDate = GetDirModDate(realVRefNum, dirID);
							if ( modDate != (*searchStack)[catPosition->stackDepth].dirModDate )
							{
								result = catChangedErr;
							}
						}
						else
						{
							/* Invalid catPosition record was passed */
							result = paramErr;
						}
					}
					else
					{
						/* The volume is not the same */
						result = catChangedErr;
					}
				}
			}
			
			if ( result == noErr )
			{
				/* ioNamePtr and ioVRefNum only need to be set up once. */
				cPB.hFileInfo.ioNamePtr = itemName;
				cPB.hFileInfo.ioVRefNum = realVRefNum;
				
				/*
				**	Here's the loop that:
				**		Finds the next item on the volume.
				**		If noErr, calls the code to check for matches and add matches
				**			to the match buffer.
				**		Sets up dirID and index for to find the next item on the volume.
				**
				**	The looping ends when:
				**		(a) an unexpected error is returned by PBGetCatInfo. All that
				**			is expected is noErr and fnfErr (after the last item in a
				**			directory is found).
				**		(b) the caller specified a timeout and our Time Manager task
				**			has fired.
				**		(c) the number of matches requested by the caller has been found.
				**		(d) the last item on the volume was found.
				*/
				do
				{
					/* get the next item */
					cPB.hFileInfo.ioFDirIndex = index;
					cPB.hFileInfo.ioDirID = dirID;
					result = PBGetCatInfoSync(&cPB);
					if ( index != -1 )
					{
						if ( result == noErr )
						{
							/* We found something */
		
							CheckForMatches(&cPB, pb, upperName, includeFiles, includeDirs);
							
							++index;
							if ( (cPB.dirInfo.ioFlAttrib & ioDirMask) != 0 )
							{
								/* It's a directory */
								
								result = CheckStack(catPosition->stackDepth, searchStack, &searchStackSize);
								if ( result == noErr )
								{
									/* Save the current state on the searchStack */
									/* when we come back, this is where we'll start */
									(*searchStack)[catPosition->stackDepth].dirID = dirID;
									(*searchStack)[catPosition->stackDepth].index = index;
									(*searchStack)[catPosition->stackDepth].dirModDate = GetDirModDate(realVRefNum, dirID);
									
									/* position catPosition->stackDepth for next saved level */
									++(catPosition->stackDepth);
									
									/* The next item to get is the 1st item in the child directory */
									dirID = cPB.dirInfo.ioDrDirID;
									index = 1;
								}
							}
							/* else do nothing for files */
						}
						else
						{
							/* End of directory found (or we had some error and that */
							/* means we have to drop out of this directory). */
							/* Restore last thing put on stack and */
							/* see if we need to continue or quit. */
							if ( catPosition->stackDepth > 0 )
							{
								/* position catPosition->stackDepth to access last saved level */
								--(catPosition->stackDepth);
								
								dirID = (*searchStack)[catPosition->stackDepth].dirID;
								index = (*searchStack)[catPosition->stackDepth].index;
								
								/* Check the dir's mod date against the saved mode date on our "stack" */
								modDate = GetDirModDate(realVRefNum, dirID);
								if ( modDate != (*searchStack)[catPosition->stackDepth].dirModDate )
								{
									result = catChangedErr;
								}
								else
								{
									/* Going back to ancestor directory. */
									/* Clear error so we can continue. */
									result = noErr;
								}
							}
							else
							{
								/* We hit the bottom of the stack, so we'll let the */
								/* the eofErr drop us out of the loop. */
								result = eofErr;
							}
						}
					}
					else
					{
						/* Special case for index == -1; that means that we're starting */
						/* a new search and so the first item to check is the directory */
						/* passed to us. */
						if ( result == noErr )
						{
							/* We found something */
		
							CheckForMatches(&cPB, pb, upperName, includeFiles, includeDirs);
							
							/* Now, set the index to 1 and then we're ready to look inside */
							/* the passed directory. */
							index = 1;
						}
					}
				} while ( (!timerTask.stopSearch) &&	/* timer hasn't fired */
						  (result == noErr) &&			/* no unexpected errors */
						  (pb->ioReqMatchCount > pb->ioActMatchCount) ); /* we haven't found our limit */
				
				/* Did we drop out of the loop because of timeout or */
				/* ioReqMatchCount was found? */
				if ( result == noErr )
				{
					result = CheckStack(catPosition->stackDepth, searchStack, &searchStackSize);
					if ( result == noErr )
					{
						/* Either there was a timeout or ioReqMatchCount was reached. */
						/* Save the dirID and index for the next time we're called. */
						
						(*searchStack)[catPosition->stackDepth].dirID = dirID;
						(*searchStack)[catPosition->stackDepth].index = index;
						(*searchStack)[catPosition->stackDepth].dirModDate = GetDirModDate(realVRefNum, dirID);
						
						/* position catPosition->stackDepth for next saved level */
						
						++(catPosition->stackDepth);
					}
				}
			}
		}
		else
		{
			/* searchStack Handle could not be allocated */
			result = memFullErr;
		}
	}
	
	if ( pb->ioSearchTime != 0 )
	{
		/* Stop Time Manager task here if it was installed */
		RmvTime((QElemPtr)&(timerTask.theTask));
		DisposeRoutineDescriptor(timerTask.theTask.tmAddr);
	}
	
	return ( result );
#else
  return paramErr;
#endif
}

/*****************************************************************************/

pascal OSErr PBCatSearchSyncCompat(CSParamPtr paramBlock)
{
	static Boolean			fullExtFSDispatchingtested = false;
	static Boolean			hasFullExtFSDispatching = false;
	OSErr 					result;
	Boolean					supportsCatSearch;
	long					response;
	GetVolParmsInfoBuffer	volParmsInfo;
	long					infoSize;
	
	result = noErr;

	/* See if File Manager will pass CatSearch requests to external file systems */
	/* we'll store the results in a static variable so we don't have to call Gestalt */
	/* everytime we're called. */
	if ( !fullExtFSDispatchingtested )
	{
		fullExtFSDispatchingtested = true;
		if ( Gestalt(gestaltFSAttr, &response) == noErr )
		{
			hasFullExtFSDispatching = ((response & (1L << gestaltFullExtFSDispatching)) != 0);
		}
	}
	
	/* CatSearch is a per volume attribute, so we have to check each time we're */
	/* called to see if it is available on the volume specified. */
	supportsCatSearch = false;
	if ( hasFullExtFSDispatching )
	{
		infoSize = sizeof(GetVolParmsInfoBuffer);
		result = HGetVolParms(paramBlock->ioNamePtr, paramBlock->ioVRefNum,
							&volParmsInfo, &infoSize);
		if ( result == noErr )
		{
			supportsCatSearch = hasCatSearch(volParmsInfo);
		}
	}
	
	/* noErr or paramErr is OK here. */
	/* paramErr just means that GetVolParms isn't supported by this volume */
	if ( (result == noErr) || (result == paramErr) )
	{
		if ( supportsCatSearch )
		{
			/* Volume supports CatSearch so use it. */
			/* CatSearch is faster than an indexed search. */
			result = PBCatSearchSync(paramBlock);
		}
		else
		{
			/* Volume doesn't support CatSearch so */
			/* search using IndexedSearch from root directory. */
			result = IndexedSearch(paramBlock, fsRtDirID);
		}
	}
	
	return ( result );
}

/*****************************************************************************/

pascal	OSErr	NameFileSearch(ConstStr255Param volName,
							   short vRefNum,
							   ConstStr255Param fileName,
							   FSSpecPtr matches,
							   long reqMatchCount,
							   long *actMatchCount,
							   Boolean newSearch,
							   Boolean partial)
{
	CInfoPBRec		searchInfo1, searchInfo2;
	HParamBlockRec	pb;
	OSErr			error;
	static CatPositionRec catPosition;
	static short	lastVRefNum = 0;
	
	/* get the real volume reference number */
	error = DetermineVRefNum(volName, vRefNum, &vRefNum);
	if ( error != noErr )
		return ( error );
	
	pb.csParam.ioNamePtr = NULL;
	pb.csParam.ioVRefNum = vRefNum;
	pb.csParam.ioMatchPtr = matches;
	pb.csParam.ioReqMatchCount = reqMatchCount;
	if ( partial )	/* tell CatSearch what we're looking for: */
	{
		pb.csParam.ioSearchBits = fsSBPartialName + fsSBFlAttrib;	/* partial name file matches or */
	}
	else
	{
		pb.csParam.ioSearchBits =  fsSBFullName + fsSBFlAttrib;		/* full name file matches */
	}
	pb.csParam.ioSearchInfo1 = &searchInfo1;
	pb.csParam.ioSearchInfo2 = &searchInfo2;
	pb.csParam.ioSearchTime = 0;
	if ( (newSearch) ||				/* If caller specified new search */
		 (lastVRefNum != vRefNum) )	/* or if last search was to another volume, */
	{
		catPosition.initialize = 0;	/* then search from beginning of catalog */
	}
	pb.csParam.ioCatPosition = catPosition;
	pb.csParam.ioOptBuffer = GetTempBuffer(0x00004000, &pb.csParam.ioOptBufSize);

	/* search for fileName */
	searchInfo1.hFileInfo.ioNamePtr = (StringPtr)fileName;
	searchInfo2.hFileInfo.ioNamePtr = NULL;
	
	/* only match files (not directories) */
	searchInfo1.hFileInfo.ioFlAttrib = 0x00;
	searchInfo2.hFileInfo.ioFlAttrib = ioDirMask;

	error = PBCatSearchSyncCompat((CSParamPtr)&pb);
	
	if ( (error == noErr) ||							/* If no errors or the end of catalog was */
		 (error == eofErr) )							/* found, then the call was successful so */
	{
		*actMatchCount = pb.csParam.ioActMatchCount;	/* return the match count */
	}
	else
	{
		*actMatchCount = 0;							/* else no matches found */
	}
	
	if ( (error == noErr) ||						/* If no errors */
		 (error == catChangedErr) )					/* or there was a change in the catalog */
	{
		catPosition = pb.csParam.ioCatPosition;
		lastVRefNum = vRefNum;
			/* we can probably start the next search where we stopped this time */
	}
	else
	{
		catPosition.initialize = 0;
			/* start the next search from beginning of catalog */
	}
	
	if ( pb.csParam.ioOptBuffer != NULL )
	{
		DisposePtr(pb.csParam.ioOptBuffer);
	}
		
	return ( error );
}

/*****************************************************************************/

pascal	OSErr	CreatorTypeFileSearch(ConstStr255Param volName,
									  short vRefNum,
									  OSType creator,
									  OSType fileType,
									  FSSpecPtr matches,
									  long reqMatchCount,
									  long *actMatchCount,
									  Boolean newSearch)
{
	CInfoPBRec		searchInfo1, searchInfo2;
	HParamBlockRec	pb;
	OSErr			error;
	static CatPositionRec catPosition;
	static short	lastVRefNum = 0;
	
	/* get the real volume reference number */
	error = DetermineVRefNum(volName, vRefNum, &vRefNum);
	if ( error != noErr )
		return ( error );
	
	pb.csParam.ioNamePtr = NULL;
	pb.csParam.ioVRefNum = vRefNum;
	pb.csParam.ioMatchPtr = matches;
	pb.csParam.ioReqMatchCount = reqMatchCount;
	pb.csParam.ioSearchBits = fsSBFlAttrib + fsSBFlFndrInfo;	/* Looking for finder info file matches */
	pb.csParam.ioSearchInfo1 = &searchInfo1;
	pb.csParam.ioSearchInfo2 = &searchInfo2;
	pb.csParam.ioSearchTime = 0;
	if ( (newSearch) ||				/* If caller specified new search */
		 (lastVRefNum != vRefNum) )	/* or if last search was to another volume, */
	{
		catPosition.initialize = 0;	/* then search from beginning of catalog */
	}
	pb.csParam.ioCatPosition = catPosition;
	pb.csParam.ioOptBuffer = GetTempBuffer(0x00004000, &pb.csParam.ioOptBufSize);

	/* no fileName */
	searchInfo1.hFileInfo.ioNamePtr = NULL;
	searchInfo2.hFileInfo.ioNamePtr = NULL;
	
	/* only match files (not directories) */
	searchInfo1.hFileInfo.ioFlAttrib = 0x00;
	searchInfo2.hFileInfo.ioFlAttrib = ioDirMask;
	
	/* search for creator; if creator = 0x00000000, ignore creator */
	searchInfo1.hFileInfo.ioFlFndrInfo.fdCreator = creator;
	if ( creator == (OSType)0x00000000 )
	{
		searchInfo2.hFileInfo.ioFlFndrInfo.fdCreator = (OSType)0x00000000;
	}
	else
	{
		searchInfo2.hFileInfo.ioFlFndrInfo.fdCreator = (OSType)0xffffffff;
	}
	
	/* search for fileType; if fileType = 0x00000000, ignore fileType */
	searchInfo1.hFileInfo.ioFlFndrInfo.fdType = fileType;
	if ( fileType == (OSType)0x00000000 )
	{
		searchInfo2.hFileInfo.ioFlFndrInfo.fdType = (OSType)0x00000000;
	}
	else
	{
		searchInfo2.hFileInfo.ioFlFndrInfo.fdType = (OSType)0xffffffff;
	}
	
	/* zero all other FInfo fields */
	searchInfo1.hFileInfo.ioFlFndrInfo.fdFlags = 0;
	searchInfo1.hFileInfo.ioFlFndrInfo.fdLocation.v = 0;
	searchInfo1.hFileInfo.ioFlFndrInfo.fdLocation.h = 0;
	searchInfo1.hFileInfo.ioFlFndrInfo.fdFldr = 0;
	
	searchInfo2.hFileInfo.ioFlFndrInfo.fdFlags = 0;
	searchInfo2.hFileInfo.ioFlFndrInfo.fdLocation.v = 0;
	searchInfo2.hFileInfo.ioFlFndrInfo.fdLocation.h = 0;
	searchInfo2.hFileInfo.ioFlFndrInfo.fdFldr = 0;

	error = PBCatSearchSyncCompat((CSParamPtr)&pb);
	
	if ( (error == noErr) ||							/* If no errors or the end of catalog was */
		 (error == eofErr) )							/* found, then the call was successful so */
	{
		*actMatchCount = pb.csParam.ioActMatchCount;	/* return the match count */
	}
	else
	{
		*actMatchCount = 0;							/* else no matches found */
	}
	
	if ( (error == noErr) ||						/* If no errors */
		 (error == catChangedErr) )					/* or there was a change in the catalog */
	{
		catPosition = pb.csParam.ioCatPosition;
		lastVRefNum = vRefNum;
			/* we can probably start the next search where we stopped this time */
	}
	else
	{
		catPosition.initialize = 0;
			/* start the next search from beginning of catalog */
	}
	
	if ( pb.csParam.ioOptBuffer != NULL )
	{
		DisposePtr(pb.csParam.ioOptBuffer);
	}
		
	return ( error );
}

/*****************************************************************************/
