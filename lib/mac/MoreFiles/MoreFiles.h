/*
**	Apple Macintosh Developer Technical Support
**
**	The long lost high-level and FSSpec File Manager functions.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		MoreFiles.h
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

#ifndef __MOREFILES__
#define __MOREFILES__

#include <Types.h>
#include <Files.h>

#include "PascalElim.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/

pascal	OSErr	HGetVolParms(StringPtr volName,
							 short vRefNum,
							 GetVolParmsInfoBuffer *volParmsInfo,
							 long *infoSize);
/*	¶ Determine the characteristics of a volume.
	The HGetVolParms function returns information about the characteristics
	of a volume. A result of paramErr usually just means the volume doesn't
	support PBHGetVolParms and the feature you were going to check
	for isn't available.

	volName			input:	A pointer to the name of a mounted volume
							or nil.
	vRefNum			input:	Volume specification.
	volParmsInfo	input:	Pointer to GetVolParmsInfoBuffer where the
							volume attributes information is returned.
					output:	Atributes information.
	infoSize		input:	Size of buffer pointed to by volParmsInfo.
					output: Size of data actually returned.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		paramErr			-50		Volume doesn't support this function
	
	__________
	
	Also see the macros for checking attribute bits in MoreFilesExtras.h
*/

/*****************************************************************************/

pascal	OSErr	HCreateMinimum(short vRefNum,
							   long dirID,
							   ConstStr255Param fileName);
/*	¶ Create a new file with no creator or file type.
	The HCreateMinimum function creates a new file without attempting to set
	the creator and file type of the new file.  This function is needed to
	create a file in an AppleShare "drop box" where the user can make
	changes, but cannot see folder or files.
	
	vRefNum		input:	Volume specification.
	dirID		input:	Directory ID.
	fileName	input:	The name of the new file.
	
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
	
	__________
	
	Also see:	FSpCreateMinimum
*/

/*****************************************************************************/

pascal	OSErr	FSpCreateMinimum(const FSSpec *spec);
/*	¶ Create a new file with no creator or file type.
	The FSpCreateMinimum function creates a new file without attempting to set 
	the the creator and file type of the new file.  This function is needed to
	create a file in an AppleShare "dropbox" where the user can make
	changes, but cannot see folder or files. 
	
	spec		input:	An FSSpec record specifying the file to create.
	
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
	
	__________
	
	Also see:	HCreateMinimum
*/

/*****************************************************************************/

pascal	OSErr	ExchangeFiles(short vRefNum,
							  long srcDirID,
							  ConstStr255Param srcName,
							  long dstDirID,
							  ConstStr255Param dstName);
/*	¶ Exchange the data stored in two files on the same volume.
	The ExchangeFiles function swaps the data in two files on the same
	volume by changing some of the information in the volume catalog and,
	if the files are open, in the file control blocks.

	vRefNum		input:	Volume specification.
	srcDirID	input:	Source directory ID.
	srcName		input:	Source file name.
	dstDirID	input:	Destination directory ID.
	dstName		input:	Destination file name.
	
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
		afpSameObjectErr	-5038	Source and destination are the same

	__________
	
	Also see:	FSpExchangeFilesCompat
*/

/*****************************************************************************/

pascal	OSErr	ResolveFileIDRef(StringPtr volName,
								 short vRefNum,
								 long fileID,
								 long *parID,
								 StringPtr fileName);
/*	¶ Retrieve the location of the file with the specified file ID reference.
	The ResolveFileIDRef function returns the filename and parent directory ID
	of the file with the specified file ID reference.
	
	volName	input:	A pointer to the name of a mounted volume
					or nil.
	vRefNum	input:	Volume specification.
	fileID	input:	The file ID reference.
	parID	output:	The parent directory ID of the file.
	name	input:	Points to a buffer (minimum Str63) where the filename
					is to be returned or must be nil.
			output:	The filename.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		fnfErr				-43		File not found
		paramErr			-50		Function not supported by volume
		volOfflinErr		-53		Volume is offline
		extFSErr			-58		External file system error - no file
									system claimed this call.
		wrgVolTypErr		-123	Not an HFS volume
		fidNotFoundErr		-1300	File ID reference not found
		notAFileErr			-1302	Specified file is a directory
		afpAccessDenied		-5000	User does not have the correct access
		afpObjectTypeErr	-5025	Specified file is a directory
		afpIDNotFound		-5034	File ID reference not found
		afpBadIDErr			-5039	File ID reference not found
	
	__________
	
	Also see:	FSpResolveFileIDRef, CreateFileIDRef, FSpCreateFileIDRef,
				DeleteFileIDRef
*/

/*****************************************************************************/

pascal	OSErr	FSpResolveFileIDRef(StringPtr volName,
									short vRefNum,
									long fileID,
									FSSpecPtr spec);
/*	¶ Retrieve the location of the file with the specified file ID reference.
	The FSpResolveFileIDRef function fills in an FSSpec with the location
	of the file with the specified file ID reference.
	
	volName	input:	A pointer to the name of a mounted volume
					or nil.
	vRefNum	input:	Volume specification.
	fileID	input:	The file ID reference.
	spec	input:	A pointer to a FSSpec record.
			output:	A file system specification to be filled in by
					FSpResolveFileIDRef.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		fnfErr				-43		File not found
		paramErr			-50		Function not supported by volume or
									no default volume
		volOfflinErr		-53		Volume is offline
		extFSErr			-58		External file system error - no file
									system claimed this call.
		wrgVolTypErr		-123	Not an HFS volume
		fidNotFoundErr		-1300	File ID reference not found
		notAFileErr			-1302	Specified file is a directory
		afpAccessDenied		-5000	User does not have the correct access
		afpObjectTypeErr	-5025	Specified file is a directory
		afpIDNotFound		-5034	File ID reference not found
		afpBadIDErr			-5039	File ID reference not found
	
	__________
	
	Also see:	ResolveFileIDRef, CreateFileIDRef, FSpCreateFileIDRef,
				DeleteFileIDRef
*/

/*****************************************************************************/

pascal	OSErr	CreateFileIDRef(short vRefNum,
								long parID,
								ConstStr255Param fileName,
								long *fileID);
/*	¶ Establish a file ID reference for a file.
	The CreateFileIDRef function creates a file ID reference for the
	specified file, or if a file ID reference already exists, supplies
	the file ID reference and returns the result code fidExists.

	vRefNum		input:	Volume specification.
	parID		input:	Directory ID.
	fileName	input:	The name of the file.
	fileID		output:	The file ID reference.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		fnfErr				-43		File not found
		wPrErr				-44		Hardware volume lock
		vLckdErr			-46		Software volume lock
		paramErr			-50		Function not supported by volume
		volOfflinErr		-53		Volume is offline
		extFSErr			-58		External file system error - no file
									system claimed this call.
		wrgVolTypErr		-123	Not an HFS volume
		fidExists			-1301	File ID reference already exists
		notAFileErrn		-1302	Specified file is a directory
		afpAccessDenied		-5000	User does not have the correct access
		afpObjectTypeErr	-5025	Specified file is a directory
		afpIDExists			-5035	File ID reference already exists
	
	__________
	
	Also see:	FSpResolveFileIDRef, ResolveFileIDRef, FSpCreateFileIDRef,
				DeleteFileIDRef
*/

/*****************************************************************************/

pascal	OSErr	FSpCreateFileIDRef(const FSSpec *spec,
								   long *fileID);
/*	¶ Establish a file ID reference for a file.
	The FSpCreateFileIDRef function creates a file ID reference for the
	specified file, or if a file ID reference already exists, supplies
	the file ID reference and returns the result code fidExists.

	spec		input:	An FSSpec record specifying the file.
	fileID		output:	The file ID reference.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		fnfErr				-43		File not found
		wPrErr				-44		Hardware volume lock
		vLckdErr			-46		Software volume lock
		paramErr			-50		Function not supported by volume
		volOfflinErr		-53		Volume is offline
		extFSErr			-58		External file system error - no file
									system claimed this call.
		wrgVolTypErr		-123	Not an HFS volume
		fidExists			-1301	File ID reference already exists
		notAFileErrn		-1302	Specified file is a directory
		afpAccessDenied		-5000	User does not have the correct access
		afpObjectTypeErr	-5025	Specified file is a directory
		afpIDExists			-5035	File ID reference already exists
	
	__________
	
	Also see:	FSpResolveFileIDRef, ResolveFileIDRef, CreateFileIDRef,
				DeleteFileIDRef
*/

/*****************************************************************************/

pascal	OSErr	DeleteFileIDRef(StringPtr volName,
								short vRefNum,
								long fileID);
/*	¶ Delete a file ID reference.
	The DeleteFileIDRef function deletes a file ID reference.

	volName	input:	A pointer to the name of a mounted volume
					or nil.
	vRefNum	input:	Volume specification.
	fileID	input:	The file ID reference.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		fnfErr				-43		File not found
		wPrErr				-44		Hardware volume lock
		vLckdErr			-46		Software volume lock
		paramErr			-50		Function not supported by volume
		volOfflinErr		-53		Volume is offline
		extFSErr			-58		External file system error - no file
									system claimed this call.
		wrgVolTypErr		-123	Function is not supported by volume
		fidNotFoundErr		-1300	File ID reference not found
		afpAccessDenied		-5000	User does not have the correct access
		afpObjectTypeErr	-5025	Specified file is a directory
		afpIDNotFound		-5034	File ID reference not found
	
	__________
	
	Also see:	FSpResolveFileIDRef, ResolveFileIDRef, CreateFileIDRef,
				FSpCreateFileIDRef
*/

/*****************************************************************************/

pascal	OSErr	FlushFile(short refNum);
/*	¶ Write the contents of a file's access path buffer (the fork data).
	The FlushFile function writes the contents of a file's access path
	buffer (the fork data) to the volume. Note: some of the file's catalog
	information stored on the volume may not be correct until FlushVol
	is called.

	refNum	input:	The file reference number of an open file.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr 				-36		I/O error
		fnOpnErr			-38		File not open
		fnfErr				-43		File not found
		rfNumErr			-51		Bad reference number
		extFSErr			-58		External file system error - no file
									system claimed this call.
*/

/*****************************************************************************/

pascal	OSErr	LockRange(short refNum,
						  long rangeLength,
						  long rangeStart);
/*	¶ Lock a portion of a file.
	The LockRange function locks (denies access to) a portion of a file
	that was opened with shared read/write permission.

	refNum		input:	The file reference number of an open file.
	rangeLength	input:	The number of bytes in the range.
	rangeStart	input:	The starting byte in the range to lock.
	
	Result Codes
		noErr				0		No error
		ioErr				-36		I/O error
		fnOpnErr			-38		File not open
		eofErr				-39		Logical end-of-file reached
		fLckdErr			-45		File is locked by another user
		paramErr			-50		Negative ioReqCount
		rfNumErr			-51		Bad reference number
		extFSErr			-58		External file system error - no file
									system claimed this call.
		volGoneErr			-124	Server volume has been disconnected
		afpNoMoreLocks		-5015	No more ranges can be locked
		afpRangeOverlap		-5021	Part of range is already locked

	__________
	
	Also see:	UnlockRange
*/

/*****************************************************************************/

pascal	OSErr	UnlockRange(short refNum,
							long rangeLength,
							long rangeStart);
/*	¶ Unlock a previously locked range.
	The UnlockRange function unlocks (allows access to) a previously locked
	portion of a file that was opened with shared read/write permission.

	refNum		input:	The file reference number of an open file.
	rangeLength	input:	The number of bytes in the range.
	rangeStart	input:	The starting byte in the range to unlock.
	
	Result Codes
		noErr				0		No error
		ioErr				-36		I/O error
		fnOpnErr			-38		File not open
		eofErr				-39		Logical end-of-file reached
		paramErr			-50		Negative ioReqCount
		rfNumErr			-51		Bad reference number
		extFSErr			-58		External file system error - no file
									system claimed this call.
		volGoneErr			-124	Server volume has been disconnected
		afpRangeNotLocked	-5020	Specified range was not locked

	__________
	
	Also see:	LockRange
*/

/*****************************************************************************/

pascal	OSErr	GetForeignPrivs(short vRefNum,
								long dirID,
								StringPtr name,
								void *foreignPrivBuffer,
								long *foreignPrivSize,
								long *foreignPrivInfo1,
								long *foreignPrivInfo2,
								long *foreignPrivInfo3,
								long *foreignPrivInfo4);
/*	¶ Retrieve the native access-control information.
	The GetForeignPrivs function retrieves the native access-control
	information for a file or directory stored on a volume managed by
	a foreign file system.
	
	vRefNum				input:	Volume specification.
	dirID				input:	Directory ID.
	name				input:	Pointer to object name, or nil when dirID
								specifies a directory that's the object.
	foreignPrivBuffer	input:	Pointer to buffer where the privilege
								information is returned.
						output:	Privilege information.
	foreignPrivSize		input:	Size of buffer pointed to by
								foreignPrivBuffer.
						output: Amount of buffer actually used.
	foreignPrivInfo1	output:	Information specific to privilege model.
	foreignPrivInfo2	output:	Information specific to privilege model.
	foreignPrivInfo3	output:	Information specific to privilege model.
	foreignPrivInfo4	output:	Information specific to privilege model.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		paramErr			-50		Volume is HFS or MFS (that is, it has
									no foreign privilege model), or foreign
									volume does not support these calls
	
	__________
	
	Also see:	FSpGetForeignPrivs, SetForeignPrivs, FSpSetForeignPrivs
*/

/*****************************************************************************/

pascal	OSErr	FSpGetForeignPrivs(const FSSpec *spec,
								   void *foreignPrivBuffer,
								   long *foreignPrivSize,
								   long *foreignPrivInfo1,
								   long *foreignPrivInfo2,
								   long *foreignPrivInfo3,
								   long *foreignPrivInfo4);
/*	¶ Retrieve the native access-control information.
	The FSpGetForeignPrivs function retrieves the native access-control
	information for a file or directory stored on a volume managed by
	a foreign file system.
	
	spec				input:	An FSSpec record specifying the object.
	foreignPrivBuffer	input:	Pointer to buffer where the privilege
								information is returned.
						output:	Privilege information.
	foreignPrivSize		input:	Size of buffer pointed to by
								foreignPrivBuffer.
						output: Amount of buffer actually used.
	foreignPrivInfo1	output:	Information specific to privilege model.
	foreignPrivInfo2	output:	Information specific to privilege model.
	foreignPrivInfo3	output:	Information specific to privilege model.
	foreignPrivInfo4	output:	Information specific to privilege model.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		paramErr			-50		Volume is HFS or MFS (that is, it has
									no foreign privilege model), or foreign
									volume does not support these calls
	
	__________
	
	Also see:	GetForeignPrivs, SetForeignPrivs, FSpSetForeignPrivs
*/

/*****************************************************************************/

pascal	OSErr	SetForeignPrivs(short vRefNum,
								long dirID,
								StringPtr name,
								void *foreignPrivBuffer,
								long *foreignPrivSize,
								long foreignPrivInfo1,
								long foreignPrivInfo2,
								long foreignPrivInfo3,
								long foreignPrivInfo4);
/*	¶ Change the native access-control information.
	The SetForeignPrivs function changes the native access-control
	information for a file or directory stored on a volume managed by
	a foreign file system.
	
	vRefNum				input:	Volume specification.
	dirID				input:	Directory ID.
	name				input:	Pointer to object name, or nil when dirID
								specifies a directory that's the object.
	foreignPrivBuffer	input:	Pointer to privilege information buffer.
	foreignPrivSize		input:	Size of buffer pointed to by
								foreignPrivBuffer.
						output: Amount of buffer actually used.
	foreignPrivInfo1	input:	Information specific to privilege model.
	foreignPrivInfo2	input:	Information specific to privilege model.
	foreignPrivInfo3	input:	Information specific to privilege model.
	foreignPrivInfo4	input:	Information specific to privilege model.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		paramErr			-50		Volume is HFS or MFS (that is, it has
									no foreign privilege model), or foreign
									volume does not support these calls
	
	__________
	
	Also see:	GetForeignPrivs, FSpGetForeignPrivs, FSpSetForeignPrivs
*/

/*****************************************************************************/

pascal	OSErr	FSpSetForeignPrivs(const FSSpec *spec,
								   void *foreignPrivBuffer,
								   long *foreignPrivSize,
								   long foreignPrivInfo1,
								   long foreignPrivInfo2,
								   long foreignPrivInfo3,
								   long foreignPrivInfo4);
/*	¶ Change the native access-control information.
	The FSpSetForeignPrivs function changes the native access-control
	information for a file or directory stored on a volume managed by
	a foreign file system.
	
	spec				input:	An FSSpec record specifying the object.
	foreignPrivBuffer	input:	Pointer to privilege information buffer.
	foreignPrivSize		input:	Size of buffer pointed to by
								foreignPrivBuffer.
						output: Amount of buffer actually used.
	foreignPrivInfo1	input:	Information specific to privilege model.
	foreignPrivInfo2	input:	Information specific to privilege model.
	foreignPrivInfo3	input:	Information specific to privilege model.
	foreignPrivInfo4	input:	Information specific to privilege model.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		paramErr			-50		Volume is HFS or MFS (that is, it has
									no foreign privilege model), or foreign
									volume does not support these calls
	
	__________
	
	Also see:	GetForeignPrivs, FSpGetForeignPrivs, SetForeignPrivs
*/

/*****************************************************************************/

pascal	OSErr	HGetLogInInfo(StringPtr volName,
							  short vRefNum,
							  short *loginMethod,
							  StringPtr userName);
/*	¶ Get the login method and user name used to log on to a shared volume.
	The HGetLogInInfo function retrieves the login method and user name
	used to log on to a particular shared volume.
	
	volName		input:	A pointer to the name of a mounted volume
						or nil.
	vRefNum		input:	The volume reference number.
	loginMethod	output:	The login method used (kNoUserAuthentication,
						kPassword, kEncryptPassword, or
						kTwoWayEncryptPassword).
	userName	input:	Points to a buffer (minimum Str31) where the user
						name is to be returned or must be nil.
				output:	The user name.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Specified volume doesn’t exist
		paramErr			-50		Function not supported by volume
	
	__________
	
	Also see:	HGetDirAccess, FSpGetDirAccess, HSetDirAccess,
				FSpSetDirAccess, HMapName, HMapID
*/

/*****************************************************************************/

pascal	OSErr	HGetDirAccess(short vRefNum,
							  long dirID,
							  StringPtr name,
							  long *ownerID,
							  long *groupID,
							  long *accessRights);
/*	¶ Get a directory's access control information on a shared volume.
	The HGetDirAccess function retrieves the directory access control
	information for a directory on a shared volume.
	
	vRefNum			input:	Volume specification.
	dirID			input:	Directory ID.
	name			input:	Pointer to directory name, or nil if dirID
							specifies the directory.
	ownerID			output:	The directory's owner ID.
	groupID			output:	The directory's group ID or
							0 if no group affiliation.
	accessRights	output:	The directory's access rights.
	
	Result Codes
		noErr				0		No error
		fnfErr				-43		Directory not found
		paramErr			-50		Function not supported by volume
		afpAccessDenied		-5000	User does not have the correct access
									to the directory
	
	__________
	
	Also see:	HGetLogInInfo, FSpGetDirAccess, HSetDirAccess,
				FSpSetDirAccess, HMapName, HMapID
*/

/*****************************************************************************/

pascal	OSErr	FSpGetDirAccess(const FSSpec *spec,
								long *ownerID,
								long *groupID,
								long *accessRights);
/*	¶ Get a directory's access control information on a shared volume.
	The FSpGetDirAccess function retrieves the directory access control
	information for a directory on a shared volume.
	
	spec			input:	An FSSpec record specifying the directory.
	ownerID			output:	The directory's owner ID.
	groupID			output:	The directory's group ID or
							0 if no group affiliation.
	accessRights	output:	The directory's access rights.
	
	Result Codes
		noErr				0		No error
		fnfErr				-43		Directory not found
		paramErr			-50		Function not supported by volume
		afpAccessDenied		-5000	User does not have the correct access
									to the directory
	
	__________
	
	Also see:	HGetLogInInfo, HGetDirAccess, HSetDirAccess,
				FSpSetDirAccess, HMapName, HMapID
*/

/*****************************************************************************/

pascal	OSErr	HSetDirAccess(short vRefNum,
							  long dirID,
							  StringPtr name,
							  long ownerID,
							  long groupID,
							  long accessRights);
/*	¶ Set a directory's access control information on a shared volume.
	The HSetDirAccess function changes the directory access control
	information for a directory on a shared volume. You must own a directory
	to change its access control information.
	
	vRefNum			input:	Volume specification.
	dirID			input:	Directory ID.
	name			input:	Pointer to directory name, or nil if dirID
							specifies the directory.
	ownerID			input:	The directory's owner ID.
	groupID			input:	The directory's group ID or
							0 if no group affiliation.
	accessRights	input:	The directory's access rights.
	
	Result Codes
		noErr				0		No error
		fnfErr				-43		Directory not found
		vLckdErr			-46		Volume is locked or read-only
		paramErr			-50		Parameter error
		afpAccessDenied		-5000	User does not have the correct access
									to the directory
		afpObjectTypeErr	-5025	Object is a file, not a directory
	
	__________
	
	Also see:	HGetLogInInfo, HGetDirAccess, FSpGetDirAccess,
				FSpSetDirAccess, HMapName, HMapID
*/

/*****************************************************************************/

pascal	OSErr	FSpSetDirAccess(const FSSpec *spec,
								long ownerID,
								long groupID,
								long accessRights);
/*	¶ Set a directory's access control information on a shared volume.
	The FSpSetDirAccess function changes the directory access control
	information for a directory on a shared volume. You must own a directory
	to change its access control information.
	
	spec			input:	An FSSpec record specifying the directory.
	ownerID			input:	The directory's owner ID.
	groupID			input:	The directory's group ID or
							0 if no group affiliation.
	accessRights	input:	The directory's access rights.
	
	Result Codes
		noErr				0		No error
		fnfErr				-43		Directory not found
		vLckdErr			-46		Volume is locked or read-only
		paramErr			-50		Parameter error
		afpAccessDenied		-5000	User does not have the correct access
									to the directory
		afpObjectTypeErr	-5025	Object is a file, not a directory
	
	__________
	
	Also see:	HGetLogInInfo, HGetDirAccess, FSpGetDirAccess, HSetDirAccess,
				HMapName, HMapID
*/

/*****************************************************************************/

pascal	OSErr	HMapID(StringPtr volName,
					   short vRefNum,
					   long ugID,
					   short objType,
					   StringPtr name);
/*	¶ Map a user or group ID to a user or group name.
	The HMapID function determines the name of a user or group if you know
	the user or group ID.
	
	volName		input:	A pointer to the name of a mounted volume
						or nil.
	vRefNum		input:	Volume specification.
	objType		input:	The mapping function code: 1 if you're mapping a
						user ID to a user name or 2 if you're mapping a
						group ID to a group name.
	name		input:	Points to a buffer (minimum Str31) where the user
						or group name is to be returned or must be nil.
				output:	The user or group name.
	
	Result Codes
		noErr				0		No error
		fnfErr				-43		Unrecognizable owner or group name
		paramErr			-50		Function not supported by volume
	
	__________
	
	Also see:	HGetLogInInfo, HGetDirAccess, FSpGetDirAccess, HSetDirAccess,
				FSpSetDirAccess, HMapName
*/

/*****************************************************************************/

pascal	OSErr	HMapName(StringPtr volName,
						 short vRefNum,
						 ConstStr255Param name,
						 short objType,
						 long *ugID);
/*	¶ Map a user or group name to a user or group ID.
	The HMapName function determines the user or group ID if you know the
	user or group name.
	
	volName		input:	A pointer to the name of a mounted volume
						or nil.
	vRefNum		input:	Volume specification.
	name		input:	The user or group name.
	objType		input:	The mapping function code: 3 if you're mapping a
						user name to a user ID or 4 if you're mapping a
						group name to a group ID.
	ugID		output:	The user or group ID.

	Result Codes
		noErr				0		No error
		fnfErr				-43		Unrecognizable owner or group name
		paramErr			-50		Function not supported by volume
	
	__________
	
	Also see:	HGetLogInInfo, HGetDirAccess, FSpGetDirAccess, HSetDirAccess,
				FSpSetDirAccess, HMapID
*/

/*****************************************************************************/

pascal	OSErr	HCopyFile(short srcVRefNum,
						  long srcDirID,
						  ConstStr255Param srcName,
						  short dstVRefNum,
						  long dstDirID,
						  StringPtr dstPathname,
						  StringPtr copyName);
/*	¶ Duplicate a file on a file server and optionally to rename it.
	The HCopyFile function duplicates a file and optionally to renames it.
	The source and destination volumes must be on the same file server.
	This function instructs the server to copy the file.
	
	srcVRefNum	input:	Source volume specification.
	srcDirID	input:	Source directory ID.
	srcName		input:	Source file name.
	dstVRefNum	input:	Destination volume specification.
	dstDirID	input:	Destination directory ID.
	dstPathname	input:	Pointer to destination directory name, or
						nil when dstDirID specifies a directory.
	copyName	input:	Points to the new file name if the file is to be
						renamed or nil if the file isn't to be renamed.
	
	Result Codes
		noErr				0		No error
		dskFulErr			-34		Destination volume is full
		fnfErr				-43		Source file not found, or destination
									directory does not exist
		vLckdErr	 		-46		Destination volume is read-only
		fBsyErr	 			-47		The source or destination file could
									not be opened with the correct access
									modes
		dupFNErr			-48		Destination file already exists
		paramErr			-50		Function not supported by volume
		wrgVolTypErr		-123	Function not supported by volume
		afpAccessDenied		-5000	The user does not have the right to
									read the source or write to the
									destination
		afpDenyConflict		-5006	The source or destination file could
									not be opened with the correct access
									modes
		afpObjectTypeErr	-5025	Source is a directory
	
	__________
	
	Also see:	FSpCopyFile, FileCopy, FSpFileCopy
*/

/*****************************************************************************/

pascal	OSErr	FSpCopyFile(const FSSpec *srcSpec,
							const FSSpec *dstSpec,
							StringPtr copyName);
/*	¶ Duplicate a file on a file server and optionally to rename it.
	The FSpCopyFile function duplicates a file and optionally to renames it.
	The source and destination volumes must be on the same file server.
	This function instructs the server to copy the file.
	
	srcSpec		input:	An FSSpec record specifying the source file.
	dstSpec		input:	An FSSpec record specifying the destination
						directory.
	copyName	input:	Points to the new file name if the file is to be
						renamed or nil if the file isn't to be renamed.
	
	Result Codes
		noErr				0		No error
		dskFulErr			-34		Destination volume is full
		fnfErr				-43		Source file not found, or destination
									directory does not exist
		vLckdErr	 		-46		Destination volume is read-only
		fBsyErr	 			-47		The source or destination file could
									not be opened with the correct access
									modes
		dupFNErr			-48		Destination file already exists
		paramErr			-50		Function not supported by volume
		wrgVolTypErr		-123	Function not supported by volume
		afpAccessDenied		-5000	The user does not have the right to
									read the source or write to the
									destination
		afpDenyConflict		-5006	The source or destination file could
									not be opened with the correct access
									modes
		afpObjectTypeErr	-5025	Source is a directory
	
	__________
	
	Also see:	HCopyFile, FileCopy, FSpFileCopy
*/

/*****************************************************************************/

pascal	OSErr	HMoveRename(short vRefNum,
							long srcDirID,
							ConstStr255Param srcName,
							long dstDirID,
							StringPtr dstpathName,
							StringPtr copyName);
/*	¶ Move a file or directory on a file server and optionally to rename it.
	The HMoveRename function moves a file or directory and optionally
	renames it. The source and destination locations must be on the same
	shared volume.
	
	vRefNum		input:	Volume specification.
	srcDirID	input:	Source directory ID.
	srcName		input:	The source object name.
	dstDirID	input:	Destination directory ID.
	dstName		input:	Pointer to destination directory name, or
						nil when dstDirID specifies a directory.
	copyName	input:	Points to the new name if the object is to be
						renamed or nil if the object isn't to be renamed.
	
	Result Codes
		noErr				0		No error
		fnfErr				-43		Source file or directory not found
		fLckdErr			-45		File is locked
		vLckdErr			-46		Destination volume is read-only
		dupFNErr			-48		Destination already exists
		paramErr			-50		Function not supported by volume
		badMovErr			-122	Attempted to move directory into
									offspring
		afpAccessDenied		-5000	The user does not have the right to
									move the file  or directory
	
	__________
	
	Also see:	FSpMoveRename, HMoveRenameCompat, FSpMoveRenameCompat
*/

/*****************************************************************************/

pascal	OSErr	FSpMoveRename(const FSSpec *srcSpec,
							  const FSSpec *dstSpec,
							  StringPtr copyName);
/*	¶ Move a file or directory on a file server and optionally to rename it.
	The FSpMoveRename function moves a file or directory and optionally
	renames it. The source and destination locations must be on the same
	shared volume.
	
	srcSpec		input:	An FSSpec record specifying the source object.
	dstSpec		input:	An FSSpec record specifying the destination
						directory.
	copyName	input:	Points to the new name if the object is to be
						renamed or nil if the object isn't to be renamed.
	
	Result Codes
		noErr				0		No error
		fnfErr				-43		Source file or directory not found
		fLckdErr			-45		File is locked
		vLckdErr			-46		Destination volume is read-only
		dupFNErr			-48		Destination already exists
		paramErr			-50		Function not supported by volume
		badMovErr			-122	Attempted to move directory into
									offspring
		afpAccessDenied		-5000	The user does not have the right to
									move the file  or directory
	
	__________
	
	Also see:	HMoveRename, HMoveRenameCompat, FSpMoveRenameCompat
*/

/*****************************************************************************/

pascal	OSErr	GetVolMountInfoSize(StringPtr volName,
									short vRefNum,
									short *size);
/*	¶ Get the size of a volume mounting information record.
	The GetVolMountInfoSize function determines the how much space the
	program needs to allocate for a volume mounting information record.
	
	volName		input:	A pointer to the name of a mounted volume
						or nil.
	vRefNum		input:	Volume specification.
	size		output:	The space needed (in bytes) of the volume mounting
						information record.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		paramErr			-50		Parameter error
		extFSErr			-58		External file system error - no file
									system claimed this call.

	__________
	
	Also see:	GetVolMountInfo, VolumeMount BuildAFPVolMountInfo,
				RetrieveAFPVolMountInfo
*/

/*****************************************************************************/

pascal	OSErr	GetVolMountInfo(StringPtr volName,
								short vRefNum,
								void *volMountInfo);
/*	¶ Retrieve a volume mounting information record.
	The GetVolMountInfo function retrieves a volume mounting information
	record containing all the information needed to mount the volume,
	except for passwords.
	
	volName			input:	A pointer to the name of a mounted volume
							or nil.
	vRefNum			input:	Volume specification.
	volMountInfo	output:	Points to a volume mounting information
							record where the mounting information is to
							be returned.
	
	Result Codes
		noErr				0		No error	
		nsvErr				-35		Volume not found	
		paramErr			-50		Parameter error	
		extFSErr			-58		External file system error - no file
									system claimed this call.

	__________
	
	Also see:	GetVolMountInfoSize, VolumeMount, BuildAFPVolMountInfo,
				RetrieveAFPVolMountInfo
*/

/*****************************************************************************/

pascal	OSErr	VolumeMount(void *volMountInfo,
							short *vRefNum);
/*	¶ Mount a volume using a volume mounting information record.
	The VolumeMount function mounts a volume using a volume mounting
	information record.
	
	volMountInfo	input:	Points to a volume mounting information record.
	vRefNum			output:	A volume reference number.
	
	Result Codes
		noErr				0		No error
		notOpenErr			-28		AppleTalk is not open
		nsvErr				-35		Volume not found
		paramErr			-50		Parameter error; typically, zone, server,
									and volume name combination is not valid
									or not complete, or the user name is not
									recognized
		extFSErr			-58		External file system error - no file
									system claimed this call.
		memFullErr			-108	Not enough memory to create a new volume
									control block for mounting the volume
		afpBadUAM			-5002	User authentication method is unknown
		afpBadVersNum		-5003	Workstation is using an AFP version that
									the server doesn’t recognize
		afpNoServer			-5016	Server is not responding
		afpUserNotAuth		-5023	User authentication failed (usually,
									password  is not correct)
		afpPwdExpired		-5042	Password has expired on server
		afpBadDirIDType		-5060	Not a fixed directory ID volume
		afpCantMountMoreSrvrs -5061	Maximum number of volumes has been
									mounted
		afpAlreadyMounted	-5062	Volume already mounted
		afpSameNodeErr		-5063	Attempt to log on to a server running
									on the same machine

	__________
	
	Also see:	GetVolMountInfoSize, GetVolMountInfo, BuildAFPVolMountInfo,
				RetrieveAFPVolMountInfo
*/

/*****************************************************************************/

pascal	OSErr	Share(short vRefNum,
					  long dirID,
					  StringPtr name);
/*	¶ Establish a local volume or directory as a share point.
	The Share function establishes a local volume or directory as a
	share point.

	vRefNum			input:	Volume specification.
	dirID			input:	Directory ID.
	name			input:	Pointer to directory name, or nil if dirID
							specifies the directory.
	
	Result Codes
		noErr				0		No error	
		tmfoErr				-42		Too many share points	
		fnfErr				-43		File not found	
		dupFNErr			-48		Already a share point with this name	
		paramErr			-50		Function not supported by volume	
		dirNFErrdirNFErr	-120	Directory not found	
		afpAccessDenied		-5000	This directory cannot be shared	
		afpObjectTypeErr	-5025	Object was a file, not a directory	
		afpContainsSharedErr -5033	The directory contains a share point	
		afpInsideSharedErr	-5043	The directory is inside a shared directory	

	__________
	
	Also see:	FSpShare, Unshare, FSpUnshare
*/

/*****************************************************************************/

pascal	OSErr	FSpShare(const FSSpec *spec);
/*	¶ Establish a local volume or directory as a share point.
	The FSpShare function establishes a local volume or directory as a
	share point.

	spec	input:	An FSSpec record specifying the share point.
	
	Result Codes
		noErr				0		No error	
		tmfoErr				-42		Too many share points	
		fnfErr				-43		File not found	
		dupFNErr			-48		Already a share point with this name	
		paramErr			-50		Function not supported by volume	
		dirNFErrdirNFErr	-120	Directory not found	
		afpAccessDenied		-5000	This directory cannot be shared	
		afpObjectTypeErr	-5025	Object was a file, not a directory	
		afpContainsSharedErr -5033	The directory contains a share point	
		afpInsideSharedErr	-5043	The directory is inside a shared directory	

	__________
	
	Also see:	Share, Unshare, FSpUnshare
*/

/*****************************************************************************/

pascal	OSErr	Unshare(short vRefNum,
						long dirID,
						StringPtr name);
/*	¶ Remove a share point.
	The Unshare function removes a share point.

	vRefNum			input:	Volume specification.
	dirID			input:	Directory ID.
	name			input:	Pointer to directory name, or nil if dirID
							specifies the directory.
	
	Result Codes
		noErr				0		No error	
		fnfErr				-43		File not found	
		paramErr			-50		Function not supported by volume	
		dirNFErrdirNFErr	-120	Directory not found	
		afpObjectTypeErr	-5025	Object was a file, not a directory; or,
									this directory is not a share point	

	__________
	
	Also see:	Share, FSpShare, FSpUnshare
*/

/*****************************************************************************/

pascal	OSErr	FSpUnshare(const FSSpec *spec);
/*	¶ Remove a share point.
	The FSpUnshare function removes a share point.

	spec	input:	An FSSpec record specifying the share point.
	
	Result Codes
		noErr				0		No error	
		fnfErr				-43		File not found	
		paramErr			-50		Function not supported by volume	
		dirNFErrdirNFErr	-120	Directory not found	
		afpObjectTypeErr	-5025	Object was a file, not a directory; or,
									this directory is not a share point	

	__________
	
	Also see:	Share, FSpShare, Unshare
*/

/*****************************************************************************/

pascal	OSErr	GetUGEntry(short objType,
						   StringPtr objName,
						   long *objID);
/*	¶ Retrieve a user or group entry from the local file server.
	The GetUGEntry function retrieves user or group entries from the
	local file server.

	objType		input:	The object type: -1 = group; 0 = user
	objName		input:	Points to a buffer (minimum Str31) where the user
						or group name is to be returned or must be nil.
				output:	The user or group name.
	objID		input:	O to get the first user or group. If the entry objID
						last returned by GetUGEntry is passed, then user or
						group whose alphabetically next in the list of entries
						is returned.
				output:	The user or group ID.
	
	Result Codes
		noErr				0		No error	
		fnfErr				-43		No more users or groups	
		paramErr			-50		Function not supported; or, ioObjID is
									negative	

	__________
	
	Also see:	GetUGEntries
*/

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#ifndef __COMPILINGMOREFILES
#undef pascal
#endif

#endif	/* __MOREFILES__ */
