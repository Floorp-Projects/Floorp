/*
**	Apple Macintosh Developer Technical Support
**
**	FSSpec compatibility functions.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		FSpCompat.h
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

#ifndef __FSPCOMPAT__
#define __FSPCOMPAT__

#include <Types.h>
#include <Files.h>

#include "PascalElim.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/

pascal	OSErr	FSMakeFSSpecCompat(short vRefNum,
								   long dirID,
								   ConstStr255Param fileName,
								   FSSpecPtr spec);
/*	¦ Initialize a FSSpec record.
	The FSMakeFSSpecCompat function fills in the fields of an FSSpec record.
	If the file system can't create the FSSpec, then the compatibility code
	creates a FSSpec that is exactly like an FSSpec except that spec.name
	for a file may not have the same capitalization as the file's catalog
	entry on the disk volume. That is because fileName is parsed to get the
	name instead of getting the name back from the file system. This works
	fine with System 6 where FSMakeSpec isn't available.
	
	vRefNum		input:	Volume specification.
	dirID		input:	Directory ID.
	fileName	input:	Pointer to object name, or nil when dirID specifies
						a directory that's the object.
	spec		output:	A file system specification to be filled in by
						FSMakeFSSpecCompat.
	
	Result Codes
		noErr				0		No error	
		nsvErr				-35		Volume doesnÕt exist	
		fnfErr				-43		File or directory does not exist
									(FSSpec is still valid)	
*/

/*****************************************************************************/

pascal	OSErr	FSpOpenDFCompat(const FSSpec *spec,
								char permission,
								short *refNum);
/*	¦ Open a file's data fork.
	The FSpOpenDFCompat function opens the data fork of the file specified
	by spec.
	Differences from FSpOpenDF: If FSpOpenDF isn't available,
	FSpOpenDFCompat uses PHBOpen because System 6 doesn't support PBHOpenDF.
	This means FSpOpenDFCompat could accidentally open a driver if the
	spec->name begins with a period.
	
	spec		input:	An FSSpec record specifying the file whose data
						fork is to be opened.
	permission	input:	A constant indicating the desired file access
						permissions.
	refNum		output:	A reference number of an access path to the file's
						data fork.
	
	Result Codes
		noErr				0		No error	
		nsvErr				-35		No such volume	
		ioErr				-36		I/O error	
		bdNamErr			-37		Bad filename	
		tmfoErr				-42		Too many files open	
		fnfErr				-43		File not found	
		opWrErr				-49		File already open for writing	
		permErr				-54		Attempt to open locked file for writing	
		dirNFErr			-120	Directory not found or incomplete pathname
		afpAccessDenied		-5000	User does not have the correct access to
									the file
	
	__________
	
	See also:	FSpOpenAware
*/

/*****************************************************************************/

pascal	OSErr	FSpOpenRFCompat(const FSSpec *spec,
								char permission,
								short *refNum);
/*	¦ Open a file's resource fork.
	The FSpOpenRFCompat function opens the resource fork of the file
	specified by spec.
	
	spec		input:	An FSSpec record specifying the file whose resource
						fork is to be opened.
	permission	input:	A constant indicating the desired file access
						permissions.
	refNum		output:	A reference number of an access path to the file's
						resource fork.
	
	Result Codes
		noErr				0		No error	
		nsvErr				-35		No such volume	
		ioErr				-36		I/O error	
		bdNamErr			-37		Bad filename	
		tmfoErr				-42		Too many files open	
		fnfErr				-43		File not found	
		opWrErr				-49		File already open for writing	
		permErr				-54		Attempt to open locked file for writing	
		dirNFErr			-120	Directory not found or incomplete pathname
		afpAccessDenied		-5000	User does not have the correct access to
									the file
	
	__________
	
	See also:	FSpOpenRFAware
*/


/*****************************************************************************/

pascal	OSErr	FSpCreateCompat(const FSSpec *spec,
								OSType creator,
								OSType fileType,
								ScriptCode scriptTag);
/*	¦ Create a new file.
	The FSpCreateCompat function creates a new file with the specified
	type, creator, and script code.
	Differences from FSpCreate: FSpCreateCompat correctly sets the
	fdScript in the file's FXInfo record to scriptTag if the problem
	isn't fixed in the File Manager code.
	
	spec		input:	An FSSpec record specifying the file to create.
	creator		input:	The creator of the new file.
	fileType	input	The file type of the new file.
	scriptCode	input:	The code of the script system in which the file
						name is to be displayed.
	
	Result Codes
		noErr				0		No error	
		dirFulErr			-33		File directory full	
		dskFulErr			-34		Disk is full	
		nsvErr				-35		No such volume	
		ioErr				-36		I/O error	
		bdNamErr			-37		Bad filename	
		fnfErr				-43		Directory not found or incomplete pathname	
		wPrErr				-44		Hardware volume lock	
		vLckdErr			-46		Software volume lock	
		dupFNErr			-48		Duplicate filename and version	
		dirNFErrdirNFErr	-120	Directory not found or incomplete pathname	
		afpAccessDenied		-5000	User does not have the correct access	
		afpObjectTypeErr	-5025	A directory exists with that name	
*/

/*****************************************************************************/

pascal	OSErr	FSpDirCreateCompat(const FSSpec *spec,
								   ScriptCode scriptTag,
								   long *createdDirID);
/*	¦ Create a new directory.
	The FSpDirCreateCompat function creates a new directory and returns the
	directory ID of the newDirectory.
	
	spec			input:	An FSSpec record specifying the directory to
							create.
	scriptCode		input:	The code of the script system in which the
							directory name is to be displayed.
	createdDirID	output:	The directory ID of the directory that was
							created.
	
	Result Codes
		noErr				0		No error	
		dirFulErr			-33		File directory full	
		dskFulErr			-34		Disk is full	
		nsvErr				-35		No such volume	
		ioErr				-36		I/O error	
		bdNamErr			-37		Bad filename	
		fnfErr				-43		Directory not found or incomplete pathname	
		wPrErr				-44		Hardware volume lock	
		vLckdErr			-46		Software volume lock	
		dupFNErr			-48		Duplicate filename and version	
		dirNFErrdirNFErr	-120	Directory not found or incomplete pathname	
		wrgVolTypErr		-123	Not an HFS volume	
		afpAccessDenied		-5000	User does not have the correct access	
*/

/*****************************************************************************/

pascal	OSErr	FSpDeleteCompat(const FSSpec *spec);
/*	¦ Delete a file or directory.
	The FSpDeleteCompat function deletes a file or directory.
	
	spec			input:	An FSSpec record specifying the file or 
							directory to delete.
	
	Result Codes
		noErr				0		No error	
		nsvErr				-35		No such volume	
		ioErr				-36		I/O error	
		bdNamErr			-37		Bad filename	
		fnfErr				-43		File not found	
		wPrErr				-44		Hardware volume lock	
		fLckdErr			-45		File is locked	
		vLckdErr			-46		Software volume lock	
		fBsyErr				-47		File busy, directory not empty, or
									working directory control block open	
		dirNFErrdirNFErr	-120	Directory not found or incomplete pathname	
		afpAccessDenied		-5000	User does not have the correct access	
*/

/*****************************************************************************/

pascal	OSErr	FSpGetFInfoCompat(const FSSpec *spec,
								  FInfo *fndrInfo);
/*	¦ Get the finder information for a file.
	The FSpGetFInfoCompat function gets the finder information for a file.

	spec		input:	An FSSpec record specifying the file.
	fndrInfo	output:	If the object is a file, then its FInfo.
	
	Result Codes
		noErr				0		No error	
		nsvErr				-35		No such volume	
		ioErr				-36		I/O error	
		bdNamErr			-37		Bad filename	
		fnfErr				-43		File not found	
		paramErr			-50		No default volume	
		dirNFErrdirNFErr	-120	Directory not found or incomplete pathname	
		afpAccessDenied		-5000	User does not have the correct access	
		afpObjectTypeErr	-5025	Directory not found or incomplete pathname	
	
	__________
	
	Also see:	FSpGetDInfo
*/

/*****************************************************************************/

pascal	OSErr	FSpSetFInfoCompat(const FSSpec *spec,
								  const FInfo *fndrInfo);
/*	¦ Set the finder information for a file.
	The FSpSetFInfoCompat function sets the finder information for a file.

	spec		input:	An FSSpec record specifying the file.
	fndrInfo	input:	The FInfo.
	
	Result Codes
		noErr				0		No error	
		nsvErr				-35		No such volume	
		ioErr				-36		I/O error	
		bdNamErr			-37		Bad filename	
		fnfErr				-43		File not found	
		wPrErr				-44		Hardware volume lock	
		fLckdErr			-45		File is locked	
		vLckdErr			-46		Software volume lock	
		dirNFErrdirNFErr	-120	Directory not found or incomplete pathname	
		afpAccessDenied		-5000	User does not have the correct access	
		afpObjectTypeErr	-5025	Object was a directory	
	
	__________
	
	Also see:	FSpSetDInfo
*/

/*****************************************************************************/

pascal	OSErr	FSpSetFLockCompat(const FSSpec *spec);
/*	¦ Lock a file.
	The FSpSetFLockCompat function locks a file.

	spec		input:	An FSSpec record specifying the file.
	
	Result Codes
		noErr				0		No error	
		nsvErr				-35		No such volume	
		ioErr				-36		I/O error	
		fnfErr				-43		File not found	
		wPrErr				-44		Hardware volume lock	
		vLckdErr			-46		Software volume lock	
		dirNFErrdirNFErr	-120	Directory not found or incomplete pathname	
		afpAccessDenied		-5000	User does not have the correct access to
									the file	
		afpObjectTypeErr	-5025	Folder locking not supported by volume	
*/

/*****************************************************************************/

pascal	OSErr	FSpRstFLockCompat(const FSSpec *spec);
/*	¦ Unlock a file.
	The FSpRstFLockCompat function unlocks a file.

	spec		input:	An FSSpec record specifying the file.
	
	Result Codes
		noErr				0		No error	
		nsvErr				-35		No such volume	
		ioErr				-36		I/O error	
		fnfErr				-43		File not found	
		wPrErr				-44		Hardware volume lock	
		vLckdErr			-46		Software volume lock	
		dirNFErrdirNFErr	-120	Directory not found or incomplete pathname	
		afpAccessDenied		-5000	User does not have the correct access to
									the file	
		afpObjectTypeErr	-5025	Folder locking not supported by volume	
*/

/*****************************************************************************/

pascal	OSErr	FSpRenameCompat(const FSSpec *spec,
								ConstStr255Param newName);
/*	¦ Rename a file or directory.
	The FSpRenameCompat function renames a file or directory.

	spec		input:	An FSSpec record specifying the file.
	newName		input:	The new name of the file or directory.
	
	Result Codes
		noErr				0		No error	
		dirFulErr			-33		File directory full	
		dskFulErr			-34		Volume is full	
		nsvErr				-35		No such volume	
		ioErr				-36		I/O error	
		bdNamErr			-37		Bad filename	
		fnfErr				-43		File not found	
		wPrErr				-44		Hardware volume lock	
		fLckdErr			-45		File is locked	
		vLckdErr			-46		Software volume lock	
		dupFNErr			-48		Duplicate filename and version	
		paramErr			-50		No default volume	
		fsRnErr				-59		Problem during rename	
		dirNFErrdirNFErr	-120	Directory not found or incomplete pathname	
		afpAccessDenied		-5000	User does not have the correct access to
									the file	
*/

/*****************************************************************************/

pascal	OSErr	FSpCatMoveCompat(const FSSpec *source,
								 const FSSpec *dest);
/*	¦ Move a file or directory to a different location on on the same volume.
	The FSpCatMoveCompat function moves a file or directory to a different
	location on on the same volume.

	source		input:	An FSSpec record specifying the file or directory.
	dest		input:	An FSSpec record specifying the name and location
						of the directory into which the source file or
						directory is to be moved.
	
	Result Codes
		noErr				0		No error	
		nsvErr				-35		No such volume	
		ioErr				-36		I/O error	
		bdNamErr			-37		Bad filename or attempt to move into
									a file	
		fnfErr				-43		File not found	
		wPrErr				-44		Hardware volume lock	
		fLckdErr			-45		Target directory is locked	
		vLckdErr			-46		Software volume lock	
		dupFNErr			-48		Duplicate filename and version	
		paramErr			-50		No default volume	
		badMovErr			-122	Attempt to move into offspring	
		wrgVolTypErr		-123	Not an HFS volume	
		afpAccessDenied		-5000	User does not have the correct access to
									the file	
*/

/*****************************************************************************/

pascal	OSErr	FSpExchangeFilesCompat(const FSSpec *source,
									   const FSSpec *dest);
/*	¦ Exchange the data stored in two files on the same volume.
	The FSpExchangeFilesCompat function swaps the data in two files by
	changing the information in the volume's catalog and, if the files
	are open, in the file control blocks.
	Differences from FSpExchangeFiles: Correctly exchanges files on volumes
	that don't support PBExchangeFiles. FSpExchangeFiles attempts to support
	volumes that don't support PBExchangeFiles, but in System 7, 7.0.1, 7.1,
	and 7 Pro, the compatibility code just doesn't work on volumes that
	don't support PBExchangeFiles (even though you may get a noErr result).
	System Update 3.0 and System 7.5 and later have the problems in
	FSpExchangeFiles corrected.
	
	Result Codes
		noErr				0		No error	
		nsvErr				-35		Volume not found	
		ioErr				-36		I/O error	
		fnfErr				-43		File not found	
		fLckdErr			-45		File is locked	
		vLckdErr			-46		Volume is locked or read-only	
		paramErr			-50		Function not supported by volume	
		volOfflinErr		-53		Volume is offline	
		wrgVolTypErr		-123	Not an HFS volume	
		diffVolErr			-1303	Files on different volumes	
		afpAccessDenied		-5000	User does not have the correct access	
		afpObjectTypeErr	-5025	Object is a directory, not a file	
		afpSameObjectErr	-5038	Source and destination files are the same	
*/

/*****************************************************************************/

pascal	short	FSpOpenResFileCompat(const FSSpec *spec,
									 SignedByte permission);
/*	¦ Open a file's resource file.
	The FSpOpenResFileCompat function opens the resource file specified
	by spec.
	
	spec			input:	An FSSpec record specifying the file whose
							resource file is to be opened.
	permission		input:	A constant indicating the desired file access
							permissions.
	function result	output:	A resource file reference number, or if there's
							an error -1.
	
	Result Codes
		noErr				0		No error
		nsvErr				Ð35		No such volume
		ioErr				Ð36		I/O error
		bdNamErr			Ð37		Bad filename or volume name (perhaps zero
									length)
		eofErr				Ð39		End of file
		tmfoErr				Ð42		Too many files open
		fnfErr				Ð43		File not found
		opWrErr				Ð49		File already open with write permission
		permErr				Ð54		Permissions error (on file open)
		extFSErr			Ð58		Volume belongs to an external file system
		memFullErr			Ð108	Not enough room in heap zone
		dirNFErr			Ð120	Directory not found
		mapReadErr			Ð199	Map inconsistent with operation
*/

/*****************************************************************************/

pascal	void	FSpCreateResFileCompat(const FSSpec *spec,
									   OSType creator,
									   OSType fileType,
									   ScriptCode scriptTag);
/*	¦ Create a resource file.
	The FSpCreateResFileCompat function creates a new resource file with
	the specified type, creator, and script code.
	Differences from FSpCreateResFile: FSpCreateResFileCompat correctly
	sets the fdScript in the file's FXInfo record to scriptTag if the
	problem isn't fixed in the File Manager code.
	
	spec		input:	An FSSpec record specifying the resource file to create.
	creator		input:	The creator of the new file.
	fileType	input	The file type of the new file.
	scriptCode	input:	The code of the script system in which the file
						name is to be displayed.
	
	Result Codes
		noErr				0		No error
		dirFulErr			Ð33		Directory full
		dskFulErr			Ð34		Disk full
		nsvErr				Ð35		No such volume
		ioErr				Ð36		I/O error
		bdNamErr			Ð37		Bad filename or volume name (perhaps zero
									length)
		tmfoErr				Ð42		Too many files open
		wPrErrw				Ð44		Disk is write-protected
		fLckdErr			Ð45		File is locked
*/

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#ifndef __COMPILINGMOREFILES
#undef pascal
#endif

#endif	/* __FSPCOMPAT__ */

