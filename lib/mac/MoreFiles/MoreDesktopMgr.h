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
**	File:	MoreDesktopMgr.h
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

#ifndef __MOREDESKTOPMGR__
#define __MOREDESKTOPMGR__

#include <Types.h>
#include <Files.h>

#include "PascalElim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Rather than duplicate this case statement I am making this function public */
pascal OSType	DTIconToResIcon(short iconType);
/*****************************************************************************/

pascal	OSErr	DTOpen(StringPtr volName,
					   short vRefNum,
					   short *dtRefNum,
					   Boolean *newDTDatabase);
/*	¶ Open a volume's desktop database and return the desktop database refNum.
	The DTOpen function opens a volume's desktop database. It returns
	the reference number of the desktop database and indicates if the
	desktop database was created as a result of this call (if it was created,
	then it is empty).

	volName			input:	A pointer to the name of a mounted volume
							or nil.
	vRefNum			input:	Volume specification.
	dtRefNum		output:	The reference number of Desktop Manager's
							desktop database on the specified volume.
	newDTDatabase	output:	true if the desktop database was created as a
							result of this call and thus empty.
							false if the desktop database was already created,
							or if it could not be determined if it was already
							created.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		paramErr			-50		Volume doesn't support this function
		extFSErr			-58		External file system error - no file
									system claimed this call.
		desktopDamagedErr	-1305	The desktop database has become corrupted - 
									the Finder will fix this, but if your
									application is not running with the
									Finder, use PBDTReset or PBDTDelete
*/

/*****************************************************************************/

pascal	OSErr	DTGetAPPL(StringPtr volName,
						  short vRefNum,
						  OSType creator,
						  short *applVRefNum,
						  long *applParID,
						  Str255 applName);
/*	¶ Find an application on a volume that can open a file with a given creator.
	The DTGetAPPL function finds an application (file type 'APPL') with
	the specified creator on the specified volume. It first tries to get
	the application mapping from the desktop database. If that fails, then
	it tries to find an application with the specified creator using
	the File Manager's CatSearch routine. If that fails, then it tries to
	find an application in the Desktop file.

	volName		input:	A pointer to the name of a mounted volume
						or nil.
	vRefNum		input:	Volume specification.
	creator		input:	The file's creator type.
	applVRefNum	output:	The volume reference number of the volume the
						application is on.
	applParID	output:	The parent directory ID of the application.
	applName	output:	The name of the application.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		paramErr			-50		No default volume
		rfNumErr			-51		Reference number invalid
		extFSErr			-58		External file system error - no file
									system claimed this call
		desktopDamagedErr	-1305	The desktop database has become corrupted - 
									the Finder will fix this, but if your
									application is not running with the
									Finder, use PBDTReset or PBDTDelete
		afpItemNotFound		-5012	Information not found
	
	__________
	
	Also see:	FSpDTGetAPPL
*/

/*****************************************************************************/

pascal	OSErr	FSpDTGetAPPL(StringPtr volName,
							 short vRefNum,
							 OSType creator,
							 FSSpec *spec);
/*	¶ Find an application on a volume that can open a file with a given creator.
	The FSpDTGetAPPL function finds an application (file type 'APPL') with
	the specified creator on the specified volume. It first tries to get
	the application mapping from the desktop database. If that fails, then
	it tries to find an application with the specified creator using
	the File Manager's CatSearch routine. If that fails, then it tries to
	find an application in the Desktop file.

	volName		input:	A pointer to the name of a mounted volume
						or nil.
	vRefNum		input:	Volume specification.
	creator		input:	The file's creator type.
	spec		output:	FSSpec record containing the application name and
						location.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		paramErr			-50		No default volume
		rfNumErr			-51		Reference number invalid
		extFSErr			-58		External file system error - no file
									system claimed this call
		desktopDamagedErr	-1305	The desktop database has become corrupted - 
									the Finder will fix this, but if your
									application is not running with the
									Finder, use PBDTReset or PBDTDelete
		afpItemNotFound		-5012	Information not found
	
	__________
	
	Also see:	DTGetAPPL
*/

/*****************************************************************************/

pascal	OSErr	DTGetIcon(StringPtr volName,
						  short vRefNum,
						  short iconType,
						  OSType fileCreator,
						  OSType fileType,
						  Handle *iconHandle);
/*	¶ Get an icon from the desktop database or Desktop file.
	The DTGetIcon function retrieves the specified icon and returns it in
	a newly created handle. The icon is retrieves from the Desktop Manager
	or if the Desktop Manager is not available, from the Finder's Desktop
	file. Your program is responsible for disposing of the handle when it is
	done using the icon.

	volName		input:	A pointer to the name of a mounted volume
						or nil.
	vRefNum		input:	Volume specification.
	iconType	input:	The icon type as defined in Files.h. Valid values are:
							kLargeIcon
							kLarge4BitIcon
							kLarge8BitIcon
							kSmallIcon
							kSmall4BitIcon
							kSmall8BitIcon
	fileCreator	input:	The icon's creator type.
	fileType	input:	The icon's file type.
	iconHandle	output:	A Handle containing the newly created icon.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		paramErr			-50		Volume doesn't support this function
		rfNumErr			-51		Reference number invalid
		extFSErr			-58		External file system error - no file
									system claimed this call
		memFullErr			-108	iconHandle could not be allocated
		desktopDamagedErr	-1305	The desktop database has become corrupted - 
									the Finder will fix this, but if your
									application is not running with the
									Finder, use PBDTReset or PBDTDelete
		afpItemNotFound		-5012	Information not found
*/

/*****************************************************************************/

pascal	OSErr	DTSetComment(short vRefNum,
							 long dirID,
							 StringPtr name,
							 ConstStr255Param comment);
/*	¶ Set a file or directory's Finder comment field.
	The DTSetComment function sets a file or directory's Finder comment
	field. The volume must support the Desktop Manager because you only
	have read access to the Desktop file.

	vRefNum	input:	Volume specification.
	dirID	input:	Directory ID.
	name	input:	Pointer to object name, or nil when dirID
					specifies a directory that's the object.
	comment	input:	The comment to add. Comments are limited to 200 characters;
					longer comments are truncated.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		fnfErr				–43		File or directory doesn’t exist
		paramErr			-50		Volume doesn't support this function
		wPrErr				–44		Volume is locked through hardware
		vLckdErr			–46		Volume is locked through software
		rfNumErr			–51		Reference number invalid
		extFSErr			-58		External file system error - no file
									system claimed this call.
		desktopDamagedErr	-1305	The desktop database has become corrupted - 
									the Finder will fix this, but if your
									application is not running with the
									Finder, use PBDTReset or PBDTDelete
	
	__________
	
	Also see:	DTCopyComment, FSpDTCopyComment, FSpDTSetComment, DTGetComment,
				FSpDTGetComment
*/

/*****************************************************************************/

pascal	OSErr	FSpDTSetComment(const FSSpec *spec,
								ConstStr255Param comment);
/*	¶ Set a file or directory's Finder comment field.
	The FSpDTSetComment function sets a file or directory's Finder comment
	field. The volume must support the Desktop Manager because you only
	have read access to the Desktop file.

	spec	input:	An FSSpec record specifying the file or directory.
	comment	input:	The comment to add. Comments are limited to 200 characters;
					longer comments are truncated.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		fnfErr				–43		File or directory doesn’t exist
		wPrErr				–44		Volume is locked through hardware
		vLckdErr			–46		Volume is locked through software
		rfNumErr			–51		Reference number invalid
		paramErr			-50		Volume doesn't support this function
		extFSErr			-58		External file system error - no file
									system claimed this call.
		desktopDamagedErr	-1305	The desktop database has become corrupted - 
									the Finder will fix this, but if your
									application is not running with the
									Finder, use PBDTReset or PBDTDelete
	
	__________
	
	Also see:	DTCopyComment, FSpDTCopyComment, DTSetComment, DTGetComment,
				FSpDTGetComment
*/

/*****************************************************************************/

pascal	OSErr	DTGetComment(short vRefNum,
							 long dirID,
							 StringPtr name,
							 Str255 comment);
/*	¶ Get a file or directory's Finder comment field (if any).
	The DTGetComment function gets a file or directory's Finder comment
	field (if any) from the Desktop Manager or if the Desktop Manager is
	not available, from the Finder's Desktop file.

	vRefNum	input:	Volume specification.
	dirID	input:	Directory ID.
	name	input:	Pointer to object name, or nil when dirID
					specifies a directory that's the object.
	comment	output:	A Str255 where the comment is to be returned.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		paramErr			-50		Volume doesn't support this function
		rfNumErr			–51		Reference number invalid
		extFSErr			-58		External file system error - no file
									system claimed this call.
		desktopDamagedErr	-1305	The desktop database has become corrupted - 
									the Finder will fix this, but if your
									application is not running with the
									Finder, use PBDTReset or PBDTDelete
		afpItemNotFound		-5012	Information not found
		
	__________
	
	Also see:	DTCopyComment, FSpDTCopyComment, DTSetComment, FSpDTSetComment,
				FSpDTGetComment
*/

/*****************************************************************************/

pascal	OSErr	FSpDTGetComment(const FSSpec *spec,
								Str255 comment);
/*	¶ Get a file or directory's Finder comment field (if any).
	The FSpDTGetComment function gets a file or directory's Finder comment
	field (if any) from the Desktop Manager or if the Desktop Manager is
	not available, from the Finder's Desktop file.

	spec	input:	An FSSpec record specifying the file or directory.
	comment	output:	A Str255 where the comment is to be returned.

	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		paramErr			-50		Volume doesn't support this function
		rfNumErr			–51		Reference number invalid
		extFSErr			-58		External file system error - no file
									system claimed this call.
		desktopDamagedErr	-1305	The desktop database has become corrupted - 
									the Finder will fix this, but if your
									application is not running with the
									Finder, use PBDTReset or PBDTDelete
		afpItemNotFound		-5012	Information not found
		
	__________
	
	Also see:	DTCopyComment, FSpDTCopyComment, DTSetComment, FSpDTSetComment,
				DTGetComment
*/

/*****************************************************************************/

pascal	OSErr	DTCopyComment(short srcVRefNum,
							  long srcDirID,
							  StringPtr srcName,
							  short dstVRefNum,
							  long dstDirID,
							  StringPtr dstName);
/*	¶ Copy the file or folder comment from the source to the destination object.
	The DTCopyComment function copies the file or folder comment from the
	source to the destination object.  The destination volume must support
	the Desktop Manager because you only have read access to the Desktop file.
	
	srcVRefNum	input:	Source volume specification.
	srcDirID	input:	Source directory ID.
	srcName		input:	Pointer to source object name, or nil when srcDirID
						specifies a directory that's the object.
	dstVRefNum	input:	Destination volume specification.
	dstDirID	input:	Destination directory ID.
	dstName		input:	Pointer to destination object name, or nil when
						dstDirID specifies a directory that's the object.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		fnfErr				–43		File or directory doesn’t exist
		wPrErr				–44		Volume is locked through hardware
		vLckdErr			–46		Volume is locked through software
		paramErr			-50		Volume doesn't support this function
		rfNumErr			–51		Reference number invalid
		paramErr			-50		Volume doesn't support this function
		extFSErr			-58		External file system error - no file
									system claimed this call.
		desktopDamagedErr	-1305	The desktop database has become corrupted - 
									the Finder will fix this, but if your
									application is not running with the
									Finder, use PBDTReset or PBDTDelete
		afpItemNotFound		-5012	Information not found
		
	__________
	
	Also see:	FSpDTCopyComment, DTSetComment, FSpDTSetComment, DTGetComment,
				FSpDTGetComment
*/

/*****************************************************************************/

pascal	OSErr	FSpDTCopyComment(const FSSpec *srcSpec,
								 const FSSpec *dstSpec);
/*	¶ Copy the desktop database comment from the source to the destination object.
	The FSpDTCopyComment function copies the desktop database comment from
	the source to the destination object.  Both the source and the
	destination volumes must support the Desktop Manager.
	
	srcSpec		input:	An FSSpec record specifying the source object.
	dstSpec		input:	An FSSpec record specifying the destination object.
	
	Result Codes
		noErr				0		No error
		nsvErr				-35		Volume not found
		ioErr				-36		I/O error
		fnfErr				–43		File or directory doesn’t exist
		wPrErr				–44		Volume is locked through hardware
		vLckdErr			–46		Volume is locked through software
		paramErr			-50		Volume doesn't support this function
		rfNumErr			–51		Reference number invalid
		paramErr			-50		Volume doesn't support this function
		extFSErr			-58		External file system error - no file
									system claimed this call.
		desktopDamagedErr	-1305	The desktop database has become corrupted - 
									the Finder will fix this, but if your
									application is not running with the
									Finder, use PBDTReset or PBDTDelete
		afpItemNotFound		-5012	Information not found
		
	__________
	
	Also see:	DTCopyComment, DTSetComment, FSpDTSetComment, DTGetComment,
				FSpDTGetComment
*/

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#ifndef __COMPILINGMOREFILES
#undef pascal
#endif

#endif	/* __MOREDESKTOPMGR__ */
