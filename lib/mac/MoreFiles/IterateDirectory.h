/*
**	IterateDirectory: File Manager directory iterator routines.
**
**	by Jim Luther
**
**	File:		IterateDirectory.h
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

#ifndef __ITERATEDIRECTORY__
#define __ITERATEDIRECTORY__

#include <Types.h>
#include <Files.h>

#include "PascalElim.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/

typedef	pascal	void (*IterateFilterProcPtr) (const CInfoPBRec * const cpbPtr,
											  Boolean *quitFlag,
											  void *yourDataPtr);
/*	¦ Prototype for the IterateFilterProc function IterateDirectory calls.
	This is the prototype for the IterateFilterProc function which is
	called once for each file and directory found by IterateDirectory. The
	IterateFilterProc gets a pointer to the CInfoPBRec that IterateDirectory
	used to call PBGetCatInfo. The IterateFilterProc can use the read-only
	data in the CInfoPBRec for whatever it wants.
	
	If the IterateFilterProc wants to stop IterateDirectory, it can set
	quitFlag to true (quitFlag will be passed to the IterateFilterProc
	false).
	
	The yourDataPtr parameter can point to whatever data structure you might
	want to access from within the IterateFilterProc.

	cpbPtr		input:	A pointer to the CInfoPBRec that IterateDirectory
						used to call PBGetCatInfo. The CInfoPBRec and the
						data it points to must not be changed by your
						IterateFilterProc.
	quitFlag	output:	Your IterateFilterProc can set quitFlag to true
						if it wants to stop IterateDirectory.
	yourDataPtr	input:	A pointer to whatever data structure you might
						want to access from within the IterateFilterProc.
	
	__________
	
	Also see:	IterateDirectory, FSpIterateDirectory
*/

#define CallIterateFilterProc(userRoutine, cpbPtr, quitFlag, yourDataPtr) \
		(*(userRoutine))((cpbPtr), (quitFlag), (yourDataPtr))

/*****************************************************************************/

pascal	OSErr	IterateDirectory(short vRefNum,
								 long dirID,
								 StringPtr name,
								 unsigned short maxLevels,
								 IterateFilterProcPtr iterateFilter,
								 void *yourDataPtr);
/*	¦ Iterate (scan) through a directory's content.
	The IterateDirectory function performs a recursive iteration (scan) of
	the specified directory and calls your IterateFilterProc function once
	for each file and directory found.
	
	The maxLevels parameter lets you control how deep the recursion goes.
	If maxLevels is 1, IterateDirectory only scans the specified directory;
	if maxLevels is 2, IterateDirectory scans the specified directory and
	one subdirectory below the specified directory; etc. Set maxLevels to
	zero to scan all levels.
	
	The yourDataPtr parameter can point to whatever data structure you might
	want to access from within the IterateFilterProc.

	vRefNum			input:	Volume specification.
	dirID			input:	Directory ID.
	name			input:	Pointer to object name, or nil when dirID
							specifies a directory that's the object.
	maxLevels		input:	Maximum number of directory levels to scan or
							zero to scan all directory levels.
	iterateFilter	input:	A pointer to the routine you want called once
							for each file and directory found by
							IterateDirectory.
	yourDataPtr		input:	A pointer to whatever data structure you might
							want to access from within the IterateFilterProc.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		No such volume
		ioErr				-36		I/O error
		bdNamErr			-37		Bad filename
		fnfErr				-43		File not found
		paramErr			-50		No default volume or iterateFilter was NULL
		dirNFErr			-120	Directory not found or incomplete pathname
									or a file was passed instead of a directory
		afpAccessDenied		-5000	User does not have the correct access
		afpObjectTypeErr	-5025	Directory not found or incomplete pathname
		
	__________
	
	See also:	IterateFilterProcPtr, FSpIterateDirectory
*/

/*****************************************************************************/

pascal	OSErr	FSpIterateDirectory(const FSSpec *spec,
									unsigned short maxLevels,
									IterateFilterProcPtr iterateFilter,
									void *yourDataPtr);
/*	¦ Iterate (scan) through a directory's content.
	The FSpIterateDirectory function performs a recursive iteration (scan)
	of the specified directory and calls your IterateFilterProc function once
	for each file and directory found.
	
	The maxLevels parameter lets you control how deep the recursion goes.
	If maxLevels is 1, FSpIterateDirectory only scans the specified directory;
	if maxLevels is 2, FSpIterateDirectory scans the specified directory and
	one subdirectory below the specified directory; etc. Set maxLevels to
	zero to scan all levels.
	
	The yourDataPtr parameter can point to whatever data structure you might
	want to access from within the IterateFilterProc.

	spec			input:	An FSSpec record specifying the directory to scan.
	maxLevels		input:	Maximum number of directory levels to scan or
							zero to scan all directory levels.
	iterateFilter	input:	A pointer to the routine you want called once
							for each file and directory found by
							FSpIterateDirectory.
	yourDataPtr		input:	A pointer to whatever data structure you might
							want to access from within the IterateFilterProc.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		No such volume
		ioErr				-36		I/O error
		bdNamErr			-37		Bad filename
		fnfErr				-43		File not found
		paramErr			-50		No default volume or iterateFilter was NULL
		dirNFErr			-120	Directory not found or incomplete pathname
		afpAccessDenied		-5000	User does not have the correct access
		afpObjectTypeErr	-5025	Directory not found or incomplete pathname
		
	__________
	
	See also:	IterateFilterProcPtr, IterateDirectory
*/

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#ifndef __COMPILINGMOREFILES
#undef pascal
#endif

#endif	/* __ITERATEDIRECTORY__ */
