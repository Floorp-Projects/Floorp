/*
**	IterateDirectory: File Manager directory iterator routines.
**
**	by Jim Luther
**
**	File:		IterateDirectory.c
**
**	Copyright © 1995-1996 Jim Luther
**	All rights reserved.
**
**	You may incorporate this sample code into your applications without
**	restriction, though the sample code has been provided "AS IS" and the
**	responsibility for its operation is 100% yours.
**
**	IterateDirectory is designed to drop into the MoreFiles sample code
**	library I wrote while in Apple Developer Technical Support
*/

#include <Types.h>
#include <Errors.h>
#include <Files.h>

#define	__COMPILINGMOREFILES

#include "MoreFilesExtras.h"
#include "IterateDirectory.h"

/*
**	Type definitions
*/

/* The IterateGlobals structure is used to minimize the amount of
** stack space used when recursively calling IterateDirectoryLevel
** and to hold global information that might be needed at any time.
*/
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif
struct IterateGlobals
{
	IterateFilterProcPtr	iterateFilter;	/* pointer to IterateFilterProc */
	CInfoPBRec				cPB;			/* the parameter block used for PBGetCatInfo calls */
	Str63					itemName;		/* the name of the current item */
	OSErr					result;			/* temporary holder of results - saves 2 bytes of stack each level */
	Boolean					quitFlag;		/* set to true if filter wants to kill interation */
	unsigned short			maxLevels;		/* Maximum levels to iterate through */
	unsigned short			currentLevel;	/* The current level IterateLevel is on */
	void					*yourDataPtr;	/* A pointer to caller data the filter may need to access */
};
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

typedef struct IterateGlobals IterateGlobals;
typedef IterateGlobals *IterateGlobalsPtr;

/*****************************************************************************/

/*	Static Prototype */

static	void	IterateDirectoryLevel(long dirID,
									  IterateGlobalsPtr theGlobals);

/*****************************************************************************/

/*
**	Functions
*/

static	void	IterateDirectoryLevel(long dirID,
									  IterateGlobalsPtr theGlobals)
{
	if ( (theGlobals->maxLevels == 0) ||						/* if maxLevels is zero, we aren't checking levels */
		 (theGlobals->currentLevel < theGlobals->maxLevels) )	/* if currentLevel < maxLevels, look at this level */
	{
		short index = 1;
		
		++theGlobals->currentLevel;	/* go to next level */
		
		do
		{	/* Isn't C great... What I'd give for a "WITH theGlobals DO" about now... */
		
			/* Get next source item at the current directory level */
			
			theGlobals->cPB.dirInfo.ioFDirIndex = index;
			theGlobals->cPB.dirInfo.ioDrDirID = dirID;
			theGlobals->result = PBGetCatInfoSync((CInfoPBPtr)&theGlobals->cPB);		
	
			if ( theGlobals->result == noErr )
			{
				/* Call the IterateFilterProc */
				CallIterateFilterProc(theGlobals->iterateFilter, &theGlobals->cPB, &theGlobals->quitFlag, theGlobals->yourDataPtr);
				
				/* Is it a directory? */
				if ( (theGlobals->cPB.hFileInfo.ioFlAttrib & ioDirMask) != 0 )
				{
					/* We have a directory */
					if ( !theGlobals->quitFlag )
					{
						/* Dive again if the IterateFilterProc didn't say "quit" */
						IterateDirectoryLevel(theGlobals->cPB.dirInfo.ioDrDirID, theGlobals);
					}
				}
			}
			
			++index; /* prepare to get next item */
		} while ( (theGlobals->result == noErr) && (!theGlobals->quitFlag) ); /* time to fall back a level? */
		
		if ( theGlobals->result == fnfErr )	/* fnfErr is OK - it only means we hit the end of this level */
			theGlobals->result = noErr;
			
		--theGlobals->currentLevel;	/* return to previous level as we leave */
	}
}

/*****************************************************************************/

pascal	OSErr	IterateDirectory(short vRefNum,
								 long dirID,
								 StringPtr name,
								 unsigned short maxLevels,
								 IterateFilterProcPtr iterateFilter,
								 void *yourDataPtr)
{
	IterateGlobals	theGlobals;
	OSErr			result;
	long			theDirID;
	short			theVRefNum;
	Boolean			isDirectory;
	
	/* Make sure there is a IterateFilter */
	if ( iterateFilter != NULL )
	{
		/* Get the real directory ID and make sure it is a directory */
		result = GetDirectoryID(vRefNum, dirID, name, &theDirID, &isDirectory);
		if ( result == noErr )
		{
			if ( isDirectory == true )
			{
				/* Get the real vRefNum */
				result = DetermineVRefNum(name, vRefNum, &theVRefNum);
				if ( result == noErr )
				{
					/* Set up the globals we need to access from the recursive routine. */
					theGlobals.iterateFilter = iterateFilter;
					theGlobals.cPB.hFileInfo.ioNamePtr = (StringPtr)&theGlobals.itemName;
					theGlobals.cPB.hFileInfo.ioVRefNum = theVRefNum;
					theGlobals.itemName[0] = 0;
					theGlobals.result = noErr;
					theGlobals.quitFlag = false;
					theGlobals.maxLevels = maxLevels;
					theGlobals.currentLevel = 0;	/* start at level 0 */
					theGlobals.yourDataPtr = yourDataPtr;
				
					/* Here we go into recursion land... */
					IterateDirectoryLevel(theDirID, &theGlobals);
					
					result = theGlobals.result;	/* set the result */
				}
			}
			else
			{
				result = dirNFErr;	/* a file was passed instead of a directory */
			}
		}
	}
	else
	{
		result = paramErr;	/* iterateFilter was NULL */
	}
	
	return ( result );
}

/*****************************************************************************/

pascal	OSErr	FSpIterateDirectory(const FSSpec *spec,
									unsigned short maxLevels,
									IterateFilterProcPtr iterateFilter,
									void *yourDataPtr)
{
	return ( IterateDirectory(spec->vRefNum, spec->parID, (StringPtr)spec->name,
						maxLevels, iterateFilter, yourDataPtr) );
}

/*****************************************************************************/
