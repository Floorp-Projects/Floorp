/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#pragma once
// ===========================================================================
// ufilemgr.h
// File manager utility routines.
// File manager class that takes care of temporary file management
// Created by atotic, June 15th, 1994
// ===========================================================================

#include <LArray.h>
#include <LFileStream.h>
#include "PascalString.h"

class LFile;
struct CStr255;
class cstring;
typedef struct URL_Struct_ URL_Struct;
class LFileBufferStream;
typedef struct MWContext_ MWContext;

struct AEDesc;
typedef AEDesc AEDescList;
typedef AEDescList AERecord;
typedef AERecord AppleEvent;


/*****************************************************************************
 * class CFileMgr
 * does general file routines. It provides many file utility functions, (as static)
 * and keeps track of temporary files (throught sFileManager). 
 * It deletes temporary files as necessary. (disk overflow, on quit).
 * To register a temporary file, call RegisterFile
 * If registered file is deleted, call CancelRegister
 * All registered files are deleted when application quits.
 * Only one instance of the manager is created. (sFileManager global)
 * THIS CLASS IS GETTING UNMANAGEABLE. SPLIT IT
 *****************************************************************************/
class CFileMgr;

class CFileMgr	{
	LArray fFiles;						// Registered files
public:
	static CFileMgr sFileManager;		// The manager
	static unsigned int sMungeNum;		// Number used to automatically generate a unique file name
// ¥¥ constructors/destructors
	virtual ~CFileMgr();

// ¥¥ file management interface
	
	void RegisterFile(LFileBufferStream * inFile);		// Register a file
	void CancelRegister(LFileBufferStream * inFile );	// Cancel file registration.
	void CancelAndDelete(LFileBufferStream * inFile );	// Cancels registration, and deletes the file.

	void HandleFindURLEvent(const AppleEvent	&inAppleEvent,
							AppleEvent			&outAEReply,
							AEDesc				&outResult,
							long				inAENumber);
// ¥¥ utility functions, all static
// ¥¥ MacOS
	// Generates file spec for a file that does not exist (not created yet)
	// Used to create a new unique temporary files
	static OSErr UniqueFileSpec(const FSSpec& inSpec, const CStr31& genericName,
								FSSpec & outSpec);
	// File spec for non-existent file. Name is based on URL
	static OSErr NewFileSpecFromURLStruct (const char * location, 
									const FSSpec& inSpec,
									FSSpec &outSpec);
	// Does a volume have a desktop database
	static OSErr	VolHasDesktopDB (short vRefNum, Boolean& hasDesktop);
	// Set file comment
	static void 	FileSetComment (const FSSpec& file, const CStr255& comment);
	// Get system volume
	static OSErr 	GetSysVolume(short& vRefNum);
	// Get volume by number (used for iteration, returns nsvErr when there are no more volumes)
	static OSErr	GetIndVolume(short index,  short& vRefNum);
	// Finds a writable folder inside a folder
	static OSErr FindWFolderInFolder(short refNum, 		// Directory/folder to be searched
									long dirID,
									const CStr255 &folderName,	// Name of the folder to search for
									short * outRefNum,// Location of the found folder
									long * outDirID);
	// Creates a folder inside a folder
	static OSErr CreateFolderInFolder(short refNum, 		// Parent directory/volume
									long dirID,
									const CStr255 &folderName,	// Name of the new folder
									short * outRefNum,	// Location of the created folder
									long * outDirID);
	// Creates a folder spec from folder id;
	static OSErr FolderSpecFromFolderID(short vRefNum, long dirID, FSSpec& folderSpec);
	// Returns a full pathname to the given file
	static char * PathNameFromFSSpec(const FSSpec& inSpec, Boolean wantLeafName );
	// PathNameFromFSSpec + hex encoding
	static char * EncodedPathNameFromFSSpec(const FSSpec& inSpec, Boolean wantLeafName );	
	// Returns file spec from full pathname
	static OSErr FSSpecFromPathname(char * PathNamePtr, FSSpec* outSpec);
	// Changes the creator/file 
	static OSErr SetFileTypeCreator(OSType creator, OSType type, const FSSpec * fileSpec);
	// Set or clear a finder flag for the file (e.g. custom icon bit)
	static OSErr SetFileFinderFlag(const FSSpec& fileSpec, Uint16 flagMask, Uint8 value);
	// Is this FSSpec a folder?
	static Boolean IsFolder(const FSSpec& spec);
	// Force the Finder to refresh a window
	static OSErr   UpdateFinderDisplay(const FSSpec& spec);
	// Creates an alias file with a given name/location pointing to the specified target
	static void    MakeAliasFile(const FSSpec& aliasSpec, const FSSpec& target);
	// Copy a file
	static OSErr   CopyFile(const FSSpec& srcSpec, const FSSpec &dstDirSpec, const CStr255 &copyName);
	
// ¥¥ Application searching

	// Finds an application given a its sig. Returns err if app is not found
	// Copied almost exactly from LaunchBySignature volume search in MacApp
	static OSErr FindApplication(OSType sig, FSSpec& file);
	// Finds app on given volume
	static OSErr FindAppOnVolume(OSType sig, short vRefNum, FSSpec& thefile);

// ¥¥ÊWWW utilities
	// Given a URL, it returns a 'reasonable' file name
	static CStr31 	FileNameFromURL (const char * url);
	// Returns a URL corresponding to the file spec. (See code for more info)
	static char * 	GetURLFromFileSpec(const FSSpec &inSpec);
	// Encodes the Macintosh full path with hex escapes
	static char * 	EncodeMacPath(char * path, Boolean prependSlash = true);
	// Decodes the Macintosh full path from one encoded with EncodeMacPath
	static char * 	DecodeMacPath(char * path);
	// File spec from URL. Reverses GetURLFromFileSpec
	static OSErr 	FSSpecFromLocalUnixPath(const char * url, FSSpec * outSpec, Boolean resolveAlias = true);
	// Convert a unix path into a mac one.  Called by FSSpecFromLocalUnixPath().
	static char* MacPathFromUnixPath(const char* unixPath);
	
// Misc file utilities

	// Does the file exist?
	static Boolean	FileExists( const FSSpec& fsSpec );
	// Does the file have data fork?
	static Boolean	FileHasDataFork(const FSSpec& fsSpec );
	// Does the file have resource fork?
	static Boolean	FileHasResourceFork(const FSSpec& fsSpec );
	// Just does an FSSpec copy, because we cannot do assignments with them
	static void		CopyFSSpec(const FSSpec & srcSpec, FSSpec & destSpec);
	// Given a folder FSSpec, returns a folder ID
	static OSErr	GetFolderID(FSSpec& folderSpec, long& dirID);
	// Recursively, deletes a folder
	// if noErr, deletion was successful
	// otherwise, returns the last encountered error when deleting
	static OSErr	DeleteFolder(const FSSpec& folderSpec);
};

// Iterates through all the files inside the folder.
// Does not deal with deletions during iteration
class CFileIter	{
public:
	CFileIter(const FSSpec &folderSpec);
	~CFileIter(){};
	Boolean Next(FSSpec &nextFile);
	Boolean Next(FSSpec &nextFile, FInfo& finderInfo, Boolean& folder);
private:
	FSSpec fDir;
	short 	fIndex;	// Index for looping
};

//-----------------------------------------------------------------------------
// More Utilities
//-----------------------------------------------------------------------------

void
WriteCString (LStream * s, const char * c);

void
WriteChar (LStream * s, char c);

void fe_FileNameFromContext( MWContext* context, const char* url, CStr31& defaultName );
