/*
**	Apple Macintosh Developer Technical Support
**
**	IndexedSearch and the PBCatSearch compatibility function.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		Search.h
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

#ifndef __SEARCH__
#define __SEARCH__

#include <Types.h>
#include <Files.h>

#include "PascalElim.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/

pascal	OSErr	IndexedSearch(CSParamPtr pb,
							  long dirID);
/*	¦ Search in and below a directory.
	The IndexedSearch function performs an indexed search in and below the
	specified directory using the same parameters (in pb) as is passed to
	PBCatSearch. See Inside Macintosh: Files for a description of the
	parameter block.
	
	pb			input:	A CSParamPtr record specifying the volume to search
						and the search criteria.
				output:	Fields in the parameter block are returned indicating
						the number of matches found, the matches, and if the
						search ended with noErr, the CatPosition record that
						lets you resume a search where the last search left
						off.
	dirID		input:	The directory to search. If fsRtDirID is passed,
						the entire volume is searched.
	
	Note:	If you use a high-level debugger and use ioSearchTime to limit
			the length of time to run the search, you'll want to step over
			calls to IndexedSearch because it installs a Time Manager task.
			Most high-level debuggers don't deal gracefully with interrupt
			driven code.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		eofErr				-39		End of catalog found (this is normal!)
		paramErr			-50		Parameter block has invalid parameters
									(see source for VerifyUserPB) or
									invalid catPosition record was passed
		extFSErr			-58		External file system error - no file
									system claimed this call.
		memFullErr			-108	Memory could not be allocated in heap
		catChangedErr		-1304	Catalog has changed and catalog position
									record may be invalid
	
	__________
	
	See also:	PBCatSearch, PBCatSearchSyncCompat
*/

/*****************************************************************************/

pascal	OSErr	PBCatSearchSyncCompat(CSParamPtr paramBlock);
/*	¦ Search a volume using PBCatSearch or IndexedSearch.
	The PBCatSearchSyncCompat function uses PBCatSearch (if available) or
	IndexedSearch (if PBCatSearch is not available) to search a volume
	using a set of search criteria that you specify. It builds a list of all
	files or directories that meet your specifications.
	
	pb			input:	A CSParamPtr record specifying the volume to search
						and the search criteria.
				output:	Fields in the parameter block are returned indicating
						the number of matches found, the matches, and if the
						search ended with noErr, the CatPosition record that
						lets you resume a search where the last search left
						off.
	
	Note:	If you use a high-level debugger and use ioSearchTime to limit
			the length of time to run the search, you'll want to step over
			calls to PBCatSearchSyncCompat because it calls IndexedSearch
			which installs a Time Manager task. Most high-level debuggers
			don't deal gracefully with interrupt driven code.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		eofErr				-39		End of catalog found (this is normal!)
		paramErr			-50		Parameter block has invalid parameters
									(see source for VerifyUserPB) or
									invalid catPosition record was passed
		extFSErr			-58		External file system error - no file
									system claimed this call.
		memFullErr			-108	Memory could not be allocated in heap
		catChangedErr		-1304	Catalog has changed and catalog position
									record may be invalid
		afpCatalogChanged	-5037	Catalog has changed and search cannot
									be resumed
	
	__________
	
	See also:	PBCatSearch, IndexedSearch
*/

/*****************************************************************************/

pascal	OSErr	NameFileSearch(StringPtr volName,
							   short vRefNum,
							   ConstStr255Param fileName,
							   FSSpecPtr matches,
							   long reqMatchCount,
							   long *actMatchCount,
							   Boolean newSearch,
							   Boolean partial);
/*	¦ Search for files by file name with PBCatSearch.
	The NameFileSearch function searches for files with a specific file
	name on a volume that supports PBCatSearch.
	Note: A result of catChangedErr means the catalog has changed between
	searches, but the search can be continued with the possiblity that you
	may miss some matches or get duplicate matches.  For all other results
	(except for noErr), the search cannot be continued.

	volName			input:	A pointer to the name of a mounted volume
							or nil.
	vRefNum			input:	Volume specification.
	fileName		input:	The name of the file to search for.
	matches			input:	Pointer to array of FSSpec where the match list is
							returned.
	reqMatchCount	input:	Maximum number of matches to return	(the number of
							elements in the matches array).
	actMatchCount	output: The number of matches actually returned.
	newSearch		input:	If true, start a new search. If false and if
							vRefNum is the same as the last call to
							NameFileSearch, then start searching at the
							position where the last search left off.
	partial			input:	If the partial parameter is false, then only files
							that exactly match fileName will be found.  If the
							partial parameter is true, then all file names that
							contain fileName will be found.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		eofErr				-39		End of catalog found (this is normal!)
		paramErr			-50		Parameter block has invalid parameters
									(see source for VerifyUserPB) or
									invalid catPosition record was passed
		extFSErr			-58		External file system error - no file
									system claimed this call.
		memFullErr			-108	Memory could not be allocated in heap
		catChangedErr		-1304	Catalog has changed and catalog position
									record may be invalid
		afpCatalogChanged	-5037	Catalog has changed and search cannot
									be resumed
	
	__________
	
	Also see:	CreatorTypeFileSearch
*/

/*****************************************************************************/

pascal	OSErr	CreatorTypeFileSearch(StringPtr volName,
									  short vRefNum,
									  OSType creator,
									  OSType fileType,
									  FSSpecPtr matches,
									  long reqMatchCount,
									  long *actMatchCount,
									  Boolean newSearch);
/*	¦ Search for files by creator/fileType with PBCatSearch.
	The CreatorTypeFileSearch function searches for files with a specific
	creator or fileType on a volume that supports PBCatSearch.
	Note: A result of catChangedErr means the catalog has changed between
	searches, but the search can be continued with the possiblity that you
	may miss some matches or get duplicate matches.  For all other results
	(except for noErr), the search cannot be continued.

	volName			input:	A pointer to the name of a mounted volume
							or nil.
	vRefNum			input:	Volume specification.
	creator			input:	The creator type of the file to search for.
							To ignore the creator type, pass 0x00000000 in
							this field.
	fileType		input:	The file type of the file to search for.
							To ignore the file type, pass 0x00000000 in
							this field.
	matches			input:	Pointer to array of FSSpec where the match list is
							returned.
	reqMatchCount	input:	Maximum number of matches to return	(the number of
							elements in the matches array).
	actMatchCount	output: The number of matches actually returned.
	newSearch		input:	If true, start a new search. If false and if
							vRefNum is the same as the last call to
							CreatorTypeFileSearch, then start searching at the
							position where the last search left off.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		eofErr				-39		End of catalog found (this is normal!)
		paramErr			-50		Parameter block has invalid parameters
									(see source for VerifyUserPB) or
									invalid catPosition record was passed
		extFSErr			-58		External file system error - no file
									system claimed this call.
		memFullErr			-108	Memory could not be allocated in heap
		catChangedErr		-1304	Catalog has changed and catalog position
									record may be invalid
		afpCatalogChanged	-5037	Catalog has changed and search cannot
									be resumed
	
	__________
	
	Also see:	NameFileSearch
*/

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#ifndef __COMPILINGMOREFILES
#undef pascal
#endif

#endif	/* __SEARCH__ */
