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

// ===========================================================================
// ufilemgr.cp
// Created by atotic, June 15th, 1994
// File utility routines for:
// Creating a temporary file name
// Bookkeeping of the temporary files
// ===========================================================================

// Front End
#include "ufilemgr.h"
#include "uprefd.h"
#include "BufferStream.h"
#include "uerrmgr.h"
#include "macutil.h"
#include "resgui.h"
#include "ulaunch.h"
#include "libmime.h"
#include "uerrmgr.h"
#include "FullPath.h"
#include "FileCopy.h"		//in MoreFiles

// XP
#ifndef _XP_H_
#include "xp.h"
#endif

#include "PascalString.h"

// MacOS
#include <AppleEvents.h>
#include <Folders.h>
#include <Errors.h>
#include <Finder.h>
//#include <UAEGizmos.h>

#define MAX_FILENAME_LEN		31
#define MAX_ALT_DIGIT_LEN		5
#define MAX_ALT_NAME_LEN		(MAX_FILENAME_LEN - (MAX_ALT_DIGIT_LEN + 1) )

/*****************************************************************************
 * class CFileMgr
 *****************************************************************************/

CFileMgr CFileMgr::sFileManager;		// The manager
unsigned int CFileMgr::sMungeNum = 1;

extern "C" OSErr FSSpecFromPathname_CWrapper(char * path, FSSpec * outSpec);

// ¥¥ constructors/destructors

// Tries to delete all the files.
// It will not be able to delete all the files, since users might have
// moved them, deleted them, etc.
CFileMgr::~CFileMgr()
{
	Boolean allDeleted = TRUE;
	LFile * aFile;
	for (int i =1; i <= fFiles.GetCount(); i++)		// Loop through
	{
		fFiles.FetchItemAt(i, &aFile);
		Try_ {
			FSSpec fileSpec;
			aFile->GetSpecifier(fileSpec);
			FSpDelete(&fileSpec);
			delete aFile;
		}
		Catch_(inErr)	{
			allDeleted = FALSE;
		} EndCatch_
	}
	fFiles.RemoveItemsAt(fFiles.GetCount(), 1);
}

// ¥¥ file management interface

// Register a file
void CFileMgr::RegisterFile(LFileBufferStream * inFile)
{
	fFiles.InsertItemsAt(1, LArray::index_Last, &inFile);
}

// Cancel file registration. Just deletes it from the queue
void CFileMgr::CancelRegister(LFileBufferStream * inFile)
{
	fFiles.Remove(&inFile);
}

// Cancels registration, and deletes the file from disk, and its file object
void CFileMgr::CancelAndDelete(LFileBufferStream * inFile)
{
	fFiles.Remove(&inFile);			// Remove it from the queue

	FSSpec fileSpec;
	inFile->GetSpecifier(fileSpec);
	OSErr err = FSpDelete(&fileSpec);	// Delete disk file
	delete inFile;						// Delete the object
}

// FindURL occurs when we have launched an external file
void CFileMgr::HandleFindURLEvent(
	const AppleEvent	&inAppleEvent,
	AppleEvent			&outAEReply,
	AEDesc				&/*outResult*/,
	long				/*inAENumber*/)
{
	FSSpec lookingFor;
	Size actualSize;
	DescType realType;

	OSErr err = ::AEGetParamPtr(&inAppleEvent, keyDirectObject,
					    	typeFSS, &realType,
							&lookingFor, 
							sizeof(lookingFor), &actualSize);
	ThrowIfOSErr_(err);
	// Have the file spec, now look for the same one
	LFileBufferStream *file;
	LArrayIterator iter(fFiles);
	while (iter.Next(&file))
	{
		FSSpec macSpec;
		file->GetSpecifier(macSpec);
		if ((macSpec.vRefNum == lookingFor.vRefNum) &&
			(macSpec.parID == lookingFor.parID) &&
			(CStr32(macSpec.name) == CStr32(lookingFor.name)))
		{
			char * url = file->GetURL();
			if (url)
			{
				err = ::AEPutParamPtr(&outAEReply, keyAEResult, typeChar, url, strlen(url));
				return;
			}
			else
				Throw_(errAEDescNotFound);
		}
	}
	Throw_(errAEDescNotFound);
}

/*
	Turning an URL into a filename
	Returns a file name that works on a Mac
	
	Requirements:
		Length < 32 characters
		No colons
		Name should look similar to URL (which is closer to the URN than the anchor)
	
	Input:
		URLs match this expression: <host>:<top><paths><name><crud>
		Top can be a /
		Paths end with a /
		Crud begins with a # or ?
		Host ends with a :
	
	Method:
		Start at the end of the paths - after last /
		Or at the end of the hostname - after first :
		Go until a # or ?

	Notes:
		This does not guarantee that the filename will work. We may still need to
		munge it later (if for example such a filename already existed)
*/

CStr31 CFileMgr::FileNameFromURL( const char* url )
{
	CStr255		newName;
	Byte		size = 0;
	
	if ( !url )
		return CStr31(::GetCString(FILE_NAME_NONE));

	const char* urlString = url;	
	
	// ¥ get the starting point
	const char* nameStart = strrchr( urlString, '/' );
	
	// ¥Êfilename starts *after* the colon
	if ( nameStart && ( strlen( nameStart ) > 1) )
		nameStart++;

	// ¥ no colon, must be simple path, go after the hostname
	else
	{
		nameStart = strchr( urlString, ':' );
		// ¥Êfilename starts *after* the colon
		if ( nameStart )
			nameStart++;
		// ¥Êno host, assume it's a simple local URL
		else 
			nameStart = urlString;
	}
	
	// ¥Êcopy it until we hit the crud (# or ?) or max out
	size = 0;		
	while ( size < MAX_FILENAME_LEN )
	{
		// 0 indexed
		char current = nameStart[ size ];
		if ( current == '#' || current == '?' || !current )
			break;
		// ¥Êtake out any : characters as well
		if ( current == ':' )
			current = '¥';
		
		size++;
		
		newName[ size ] = current;
		newName.Length() = size;
	}

	// If size was 0 then we have a problem. Make a temp filename.
	// A reasonable convention would be to have names like "Untitled 1", "Untitled 2", etc
	// since we really don't have any other information. We'll leave off the number
	// here and let the arbitrator add it on later. We don't have an english file kind
	// either, which the arbitrator might have, so this could get folded in later (just
	// have a "Untitled" hack in here now)
	if ( size == 0 )
 		newName = (char*)GetCString( UNTITLED_RESID );
	return newName;
}

// Creates a FSSpec of a file that does not exist, given the template
OSErr CFileMgr::UniqueFileSpec(const FSSpec& inSpec, const CStr31& genericName,
								FSSpec & outSpec)
{
	CStr255			goodName = genericName;
	OSErr			err;
	
	err = FSMakeFSSpec( inSpec.vRefNum, inSpec.parID, goodName, &outSpec );
	if (err == fnfErr)
		return noErr;
	if (err)
		return err;

	// If the filename exists (noErr) then we have a problem!
	
	// Argh. We can do this: filename.gif-2
	// Or this: #2 filename.gif
	// One messes with the extension (which some people care about) and the other
	// with the "proper" name.
		
#define USE_RANDOM 0
#if USE_RANDOM
	long index = 0;
#else
	static short	index = 1;
#endif
	CStr31			indexStr;
	Boolean			done = FALSE;
	CStr255			altName;
	
	do {
		index++;				// start with "Picture 2" after "Picture" exists
		if ( index > 999 )		// something's very wrong
			return ioErr;
		altName = goodName;
		if ( altName.Length() > MAX_ALT_NAME_LEN )
			altName.Length() = MAX_ALT_NAME_LEN;
#if USE_RANDOM
		long randomNum = abs(::Random())*9999/32767;
		NumToString( randomNum, indexStr );
#else
		NumToString( index, indexStr );
#endif
		altName += "-";
		altName += indexStr;
		err = FSMakeFSSpec( inSpec.vRefNum, inSpec.parID, altName, &outSpec );
		if ( err == fnfErr )
			return noErr;
	} while ( err == noErr );
	return noErr;
}

/*
	Turning an URL into a fileSpec of a new file.
	A file spec for non-existent file is created.
	
	Requirements:
		¥ Preserve filename as much as possible, including extension, if any (even though
		this doesn't quite make sense on the Mac).
		¥ Make the new filename look "nice". Provide as much information as possible to the
		user (they might want to keep the file) and don't use random numbers or the date.
		Well, the date might be OK. Something like "Picture 1", "Picture 2" might do,
		or "#2 madonna.gif".
		
	Method:
		For file name generation see FileNameFromURL
		If file with default name already exists, munge the name by appending
		it a number.
*/

OSErr CFileMgr::NewFileSpecFromURLStruct (const char * location,
							const FSSpec& inSpec,
							FSSpec &outSpec)
{
	CStr31 goodName = FileNameFromURL(location);
	return UniqueFileSpec( inSpec, goodName, outSpec );
}


/*
	FL_SetComment
*/

void CFileMgr::FileSetComment (const FSSpec& file, const CStr255& comment)
{
	// Set GetInfo box in Finder to the URL. Not really  necessary, but a neat hack
	Boolean hasDesktop;
	FSSpec tempSpec = CPrefs::GetFolderSpec(CPrefs::DownloadFolder);
	OSErr err = VolHasDesktopDB(tempSpec.vRefNum, hasDesktop);
	if (err || !hasDesktop)
		return;
	DTPBRec pb;
	pb.ioCompletion = NULL;
	pb.ioVRefNum = tempSpec.vRefNum;
	pb.ioNamePtr = NULL;
	err = PBDTGetPath(&pb);
	if (err)
		return;
	short refNum = pb.ioDTRefNum;
	pb.ioNamePtr = (StringPtr)&file.name;	// A pointer to a file or directory name.	
	char *ccomment = comment;
	pb.ioDTBuffer = ccomment;	
	pb.ioDTReqCount = strlen(ccomment);
	pb.ioDirID = file.parID;	// The parent directory of the file or directory.	
	err = ::PBDTSetCommentSync(&pb);
}

/*
	FL_VolumeHasDesktopDB
*/

OSErr CFileMgr::VolHasDesktopDB(short vRefNum, Boolean& hasDesktop)
{
	HParamBlockRec pb;
	GetVolParmsInfoBuffer info;

	pb.ioParam.ioCompletion = NULL;
	pb.ioParam.ioVRefNum = vRefNum;
	pb.ioParam.ioNamePtr = NULL;
	pb.ioParam.ioBuffer = (Ptr) & info;
	pb.ioParam.ioReqCount = sizeof(GetVolParmsInfoBuffer);

	OSErr err = ::PBHGetVolParmsSync(&pb);

	hasDesktop = (err == noErr) && ((info.vMAttrib & (1L << bHasDesktopMgr)) != 0);

	return err;
	
} // VolHasDesktopDB 

// From MacApp
// Get the vRefNum of an indexed on-line volume
OSErr CFileMgr::GetIndVolume(short index,
				   short& vRefNum)
{
	ParamBlockRec pb;
	OSErr err;

	pb.volumeParam.ioCompletion = NULL;
	pb.volumeParam.ioNamePtr = NULL;
	pb.volumeParam.ioVolIndex = index;

	err = PBGetVInfoSync(&pb);

	vRefNum = pb.volumeParam.ioVRefNum;
	return err;
} // GetIndVolume 

// Copied from MacApp
// Get the vRefNum of the system (boot) volume
OSErr CFileMgr::GetSysVolume(short& vRefNum)
{
	OSErr theErr;
	long dir;
	theErr = FindFolder(kOnSystemDisk, kSystemFolderType, false, &vRefNum, &dir);
	return theErr;
}

// Finds a folder inside a folder.
// The folder must be writeable
// Signals error if folder cannot be found
// Success: returns noErr and the location of the found folder in foundRefNum, foundDirID
// Failure: returns  PBGetCatInfo error, and undefined foundRefNum, foundDirID
OSErr CFileMgr::FindWFolderInFolder(short refNum, 		// Directory/folder to be searched
									long dirID,
									const CStr255& folderName,	// Name of the folder to search for
									short * outRefNum,// Location of the found folder
									long * outDirID)
{
	CInfoPBRec cipb;
	DirInfo	*dipb=(DirInfo *)&cipb;		// Typecast to what we need
	OSErr err;

	dipb->ioNamePtr 	= (unsigned char *) &folderName;
	dipb->ioFDirIndex 	= 0;
	dipb->ioVRefNum		= refNum;
	dipb->ioDrDirID 	= dirID;
	
	err = PBGetCatInfoSync(&cipb);
	
	if (err != noErr)
		return err;
		
	if ( ( dipb->ioFlAttrib & 0x0010 ) )// && !(cipb.ioACUser && 2))	// Is it a directory and writable?
	{
		*outRefNum 	= dipb->ioVRefNum;
		*outDirID 	= dipb->ioDrDirID;
		return noErr;
	}
	
	return fnfErr;
}	// FindWFolderInFolder

// Creates a folder named 'folderName' inside a folder.
// The errors returned are same as PBDirCreate
OSErr CFileMgr::CreateFolderInFolder(short refNum, 		// Parent directory/volume
									long dirID,
									const CStr255 &folderName,	// Name of the new folder
									short * outRefNum,	// Volume of the created folder
									long * outDirID)	// 
{
	HFileParam hpb;
	
	hpb.ioVRefNum = refNum;
	hpb.ioDirID = dirID;
	hpb.ioNamePtr = (StringPtr)&folderName;
	OSErr err = PBDirCreateSync((HParmBlkPtr)&hpb);
	if (err == noErr)	{
		*outRefNum = hpb.ioVRefNum;
		*outDirID = hpb.ioDirID;
	}	else	{
		*outRefNum = 0;
		*outDirID = 0;
	}
	return err;
}

// Creates a folder spec from folder id;
OSErr CFileMgr::FolderSpecFromFolderID(short vRefNum, long dirID, FSSpec& folderSpec)
{
	folderSpec.vRefNum = 0;	// Initialize them to 0
	folderSpec.parID = 0;
	CInfoPBRec cinfo;
	DirInfo	*dipb=(DirInfo *)&cinfo;
	dipb->ioNamePtr = (StringPtr)&folderSpec.name;
	dipb->ioVRefNum = vRefNum;
	dipb->ioFDirIndex = -1;
	dipb->ioDrDirID = dirID;
	OSErr err = PBGetCatInfoSync(&cinfo);
	
	if (err == noErr)
	{
		folderSpec.vRefNum = dipb->ioVRefNum;
		folderSpec.parID = dipb->ioDrParID;
	}
	return err;
}

//-----------------------------------
char* CFileMgr::PathNameFromFSSpec( const FSSpec& inSpec, Boolean wantLeafName )
// Returns a full pathname to the given file
// Returned value is allocated with XP_ALLOC, and must be freed with XP_FREE
// This is taken from FSpGetFullPath in MoreFiles, except that we need to tolerate
// fnfErr.
//-----------------------------------
{	
	char* result = nil;
	FSSpec tempSpec;
	OSErr err = noErr;
	
	short fullPathLength = 0;
	Handle fullPath = NULL;
	
	/* Make a copy of the input FSSpec that can be modified */
	BlockMoveData(&inSpec, &tempSpec, sizeof(FSSpec));
	
	if ( tempSpec.parID == fsRtParID )
	{
		/* The object is a volume */
		
		/* Add a colon to make it a full pathname */
		++tempSpec.name[0];
		tempSpec.name[tempSpec.name[0]] = ':';
		
		/* We're done */
		err = PtrToHand(&tempSpec.name[1], &fullPath, tempSpec.name[0]);
	}
	else
	{
		/* The object isn't a volume */
		
		CInfoPBRec	pb = { 0 };
		CStr63 dummyFileName("\pGrippy Lives!");

		/* Is the object a file or a directory? */
		pb.dirInfo.ioNamePtr = (! tempSpec.name[0] ) ? (StringPtr)dummyFileName : tempSpec.name;
		pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
		pb.dirInfo.ioDrDirID = tempSpec.parID;
		pb.dirInfo.ioFDirIndex = 0;
		err = PBGetCatInfoSync(&pb);
		if ( err == noErr || err == fnfErr)
		{
			// if the object is a directory, append a colon so full pathname ends with colon
			// Beware of the "illegal spec" case that Netscape uses (empty name string). In
			// this case, we don't want the colon.
			if ( err == noErr && tempSpec.name[0] && (pb.hFileInfo.ioFlAttrib & ioDirMask) != 0 )
			{
				++tempSpec.name[0];
				tempSpec.name[tempSpec.name[0]] = ':';
			}
			
			/* Put the object name in first */
			err = PtrToHand(&tempSpec.name[1], &fullPath, tempSpec.name[0]);
			if ( err == noErr )
			{
				/* Get the ancestor directory names */
				pb.dirInfo.ioNamePtr = tempSpec.name;
				pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
				pb.dirInfo.ioDrParID = tempSpec.parID;
				do	/* loop until we have an error or find the root directory */
				{
					pb.dirInfo.ioFDirIndex = -1;
					pb.dirInfo.ioDrDirID = pb.dirInfo.ioDrParID;
					err = PBGetCatInfoSync(&pb);
					if ( err == noErr )
					{
						/* Append colon to directory name */
						++tempSpec.name[0];
						tempSpec.name[tempSpec.name[0]] = ':';
						
						/* Add directory name to beginning of fullPath */
						(void) Munger(fullPath, 0, NULL, 0, &tempSpec.name[1], tempSpec.name[0]);
						err = MemError();
					}
				} while ( err == noErr && pb.dirInfo.ioDrDirID != fsRtDirID );
			}
		}
	}
	if ( err != noErr && err != fnfErr)
		goto Clean;

	fullPathLength = GetHandleSize(fullPath);
	err = noErr;	
	int allocSize = 1 + fullPathLength;
	// We only want the leaf name if it's the root directory or wantLeafName is true.
	if (inSpec.parID != fsRtParID && !wantLeafName)
		allocSize -= inSpec.name[0];
	result = (char*)XP_ALLOC(allocSize);
	if (!result)
		goto Clean;
	memcpy(result, *fullPath, allocSize - 1);
	result[ allocSize - 1 ] = 0;
Clean:
	if (fullPath)
		DisposeHandle(fullPath);
	Assert_(result); // OOPS! very bad.
	return result;
} // CFileMgr::PathNameFromFSSpec

// PathNameFromFSSpec + hex encoding
// pass this in to netlib when specifying filesToPost, etch
char* CFileMgr::EncodedPathNameFromFSSpec( const FSSpec& inSpec, Boolean wantLeafName )
{
	char* path = CFileMgr::PathNameFromFSSpec( inSpec, wantLeafName );
	path = CFileMgr::EncodeMacPath( path );
	return path;
}

// GetElement is a routine used by FSSpecFromPathname, routine adopted from ParseFullPathName.c on dev disk
// I am not sure how does it work.
static Boolean GetElement(StringPtr Result,char * PathNamePtr,short ElementNumber);
static Boolean GetElement(StringPtr Result,char *PathNamePtr,short ElementNumber)
{
	char 			*eStart, *eEnd;
	
	eStart = eEnd = PathNamePtr;
	while (ElementNumber) {					// Search for the element
		if (*eEnd == ':' || !(*eEnd)) {		//   if we see colon or a null , we're at the end of element
			--ElementNumber;				//   one down, n-1 to go
			if (ElementNumber == 1)			//   are we at the second to last element??
				eStart = eEnd + 1;			//		mark it.
		}
		if (!(*eEnd)) break;
		++eEnd;								//   always increment 
	}
	
	if (ElementNumber || (eEnd - eStart > 32) || (eEnd - eStart == 0))	// If n > 0 or the element is too big or there is no element
		return false; 													//  then croak.

	Result[0] = (char)(eEnd - eStart);		// Move the substring into the Result
	BlockMove ((Ptr) eStart, (Ptr) (Result + 1),Result[0]);			
		return true;
}

//-----------------------------------
OSErr CFileMgr::FSSpecFromPathname(char* inPathNamePtr, FSSpec* outSpec)
// FSSpecFromPathname reverses PathNameFromFSSpec.
// It returns a FSSpec given a c string which is a mac pathname.
//-----------------------------------
{
	// Simplify this routine to use FSMakeFSSpec if length < 255. Otherwise use the MoreFiles
	// routine FSpLocationFromFullPath, which allocates memory, to handle longer pathnames. 
	if (strlen(inPathNamePtr) < 255)
		return ::FSMakeFSSpec(0, 0, CStr255(inPathNamePtr), outSpec);
	return FSpLocationFromFullPath(strlen(inPathNamePtr), inPathNamePtr, outSpec);
} // CFileMgr::FSSpecFromPathname

// Changes the creator/file 
OSErr CFileMgr::SetFileTypeCreator(OSType creator, OSType type, const FSSpec * fileSpec)
{
	FInfo info;
	OSErr err = ::FSpGetFInfo (fileSpec, &info);
	if (err != noErr)
		return err;
	info.fdCreator = creator;
	info.fdType = type;
	err = ::FSpSetFInfo (fileSpec, &info);
	return err;
}

// Set or clear a Finder flag
OSErr CFileMgr::SetFileFinderFlag(const FSSpec& fileSpec, Uint16 flagMask, Uint8 value)
{
	FInfo info;
	OSErr err = ::FSpGetFInfo (&fileSpec, &info);
	if (err != noErr)
		return err;
		
	if (value)		//set the bit
		info.fdFlags |= flagMask;
	else 			//clear the bit
		info.fdFlags &= ~flagMask;
	
	err = ::FSpSetFInfo (&fileSpec, &info);
	return err;
}


//-----------------------------------
Boolean CFileMgr::IsFolder(const FSSpec& spec)
//-----------------------------------
{
	CStr31 name = spec.name;
	CInfoPBRec pb;
	DirInfo	*dipb=(DirInfo *)&pb;
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = spec.vRefNum;
	pb.hFileInfo.ioDirID = spec.parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	OSErr err = PBGetCatInfoSync(&pb);
	if (err != noErr)
		return FALSE;
	if (dipb->ioFlAttrib & 0x10)
		return TRUE;
	// else
		return FALSE;
}

//-----------------------------------
OSErr CFileMgr::FindAppOnVolume(OSType sig,
					  short vRefNum,
					  FSSpec& thefile)
// Copied from MacApp. It did not have any docs in MacApp eithere
// Ask vol's desktop db for application
//-----------------------------------
{
	DTPBRec pb;
	OSErr err;

	pb.ioCompletion = NULL;
	pb.ioVRefNum = vRefNum;
	pb.ioNamePtr = NULL;
	if ((err = PBDTGetPath(&pb)) != noErr)
		return err;									// Puts DT refnum into pb.ioDTRefNum
	short refNum = pb.ioDTRefNum;

	pb.ioCompletion = NULL;
	pb.ioDTRefNum = refNum;
	pb.ioIndex = 0;
	pb.ioFileCreator = sig;
	pb.ioNamePtr = (StringPtr) thefile.name;
	err = PBDTGetAPPLSync(&pb);						// Find it!

	if( err == fnfErr )
		err = afpItemNotFound;						// Bug in PBDTGetAPPL
	if( err )
		return err;									// Returns afpItemNotFound if app wasn't found.

	thefile.vRefNum = vRefNum;
	thefile.parID = pb.ioAPPLParID;
	return err;
}

//-----------------------------------
OSErr CFileMgr::FindApplication(OSType sig, FSSpec& file)	
// Finds application info given a file, based on file's creator
// Copied almost exactly from LaunchBySignature volume search
//-----------------------------------
{
	short sysVRefNum;

	OSErr err = CFileMgr::GetSysVolume(sysVRefNum);
	if (err)
		return err;									// Find boot volume

	short vRefNum = sysVRefNum;						// Start search with boot volume
	short index = 0;
	do	{
		if (index == 0 || vRefNum != sysVRefNum)		{
			Boolean hasDesktopDB;
			err = CFileMgr::VolHasDesktopDB(vRefNum, hasDesktopDB);
			if (err)
				return err;
			if (hasDesktopDB)			{
				// If volume has a desktop DB,
				err = FindAppOnVolume(sig, vRefNum, file);	// ask it to find app
				if (err == noErr)
					return err;
//				else if (err != afpItemNotFound) on broken file systems, the error returned might be spurious
//					return err;
			}
		}
		err = CFileMgr::GetIndVolume(++index, vRefNum);	// Else go to next volume
	} while (err != nsvErr);						// Keep going until we run out of vols
	if( err==nsvErr || err==afpItemNotFound )
		err= fnfErr;									// File not found on any volume
	return err;
}

//-----------------------------------
inline void  SwapSlashColon(char * s)
// Swaps ':' with '/'
//-----------------------------------
{
	while ( *s != 0)
	{
		if (*s == '/')
			*s++ = ':';
		else if (*s == ':')
			*s++ = '/';
		else
			*s++;
	}
}

//-----------------------------------
char* CFileMgr::EncodeMacPath( char* inPath, Boolean prependSlash )
//	Transforms Macintosh style path into Unix one
//	Method: Swap ':' and '/', hex escape the result
//-----------------------------------
{
	if (inPath == NULL)
		return NULL;
	int pathSize = XP_STRLEN(inPath);
	
	// XP code sometimes chokes if there's a final slash in the unix path.
	// Since correct mac paths to folders and volumes will end in ':', strip this
	// first.
	char* c = inPath + pathSize - 1;
	if (*c == ':')
	{
		*c = 0;
		pathSize--;
	}

	char * newPath = NULL;
	char * finalPath = NULL;
	
	if (prependSlash)
	{
		newPath = (char*) XP_ALLOC(pathSize + 2);
		newPath[0] = ':';	// It will be converted to '/'
		XP_MEMCPY(&newPath[1], inPath, pathSize + 1);
	}
	else
		newPath = XP_STRDUP(inPath);

	if (newPath)
	{
		SwapSlashColon( newPath );
		finalPath = NET_Escape(newPath, URL_PATH);
		XP_FREE(newPath);		
	}

	XP_FREE( inPath );
	return finalPath;
} // CFileMgr::EncodeMacPath

//-----------------------------------
char * CFileMgr::GetURLFromFileSpec(const FSSpec& inSpec)
//-----------------------------------
/* GetURLFromFileSpec generates a local file URL given a file spec.
 	Requirements:
		Unix-style file name (':' is replaced with '/')
		url looks like file:///<path>
		<path> is mac path, where all reserved characters (= | ; | / | # | ? | space)
		have been escaped. (except :).
		':' is then changed to '/' for compatibility with UNIX style file names.
	
	Input:
		Valid FSSpec
		
	Method:
		Generate a full path name
	Notes:
		This does not guarantee that the filename will work. We may still need to
		munge it later (if for example such a filename already existed)
*/
{
	char * path = PathNameFromFSSpec( inSpec, TRUE );
	char * unixPath = EncodeMacPath(path);
	char * finalPath = (char*)XP_ALLOC(strlen(unixPath) + 8 + 1);	//  file:///<path>0
	if ( finalPath == NULL )
		return NULL;
	finalPath[0] = 0;
	if ( unixPath == NULL )
		return NULL;
	strcat(finalPath, "file://");
	strcat(finalPath, unixPath);
	XP_FREE(unixPath);
	return finalPath;
}

//-----------------------------------
char*  CFileMgr::MacPathFromUnixPath(const char* unixPath)
//-----------------------------------
{
	// Relying on the fact that the unix path is always longer than the mac path:
	size_t len = XP_STRLEN(unixPath);
	char* result = (char*)XP_ALLOC(len + 2); // ... but allow for the initial colon in a partial name
	if (result)
	{
		char* dst = result;
		const char* src = unixPath;
		if (*src == '/')		 	// ¥ full path
			src++;
		else if (strchr(src, '/'))	// ¥ partial path, and not just a leaf name
			*dst++ = ':';
		XP_STRCPY(dst, src);
		NET_UnEscape(dst);	// Hex Decode
		SwapSlashColon(dst);
	}
	return result;
} // CFileMgr::MacPathFromUnixPath

//-----------------------------------
OSErr CFileMgr::FSSpecFromLocalUnixPath(
	const char * unixPath,
	FSSpec * inOutSpec,
	Boolean resolveAlias)
// File spec from URL. Reverses GetURLFromFileSpec 
// Its input is only the <path> part of the URL
// JRM 97/01/08 changed this so that if it's a partial path (doesn't start with '/'),
// then it is combined with inOutSpec's vRefNum and parID to form a new spec.
//-----------------------------------
{
	if (unixPath == NULL)
		return badFidErr;
	char* macPath = MacPathFromUnixPath(unixPath);
	if (!macPath)
		return memFullErr;

	OSErr err = noErr;
	if (*unixPath == '/' /*full path*/)
		err = FSSpecFromPathname(macPath, inOutSpec);
	else
		err = ::FSMakeFSSpec(inOutSpec->vRefNum, inOutSpec->parID, CStr255(macPath), inOutSpec);
	if (err == fnfErr)
		err = noErr;
	Boolean dummy, dummy2;	
	if (err == noErr && resolveAlias)	// Added 
		err = ::ResolveAliasFile(inOutSpec,TRUE,&dummy,&dummy2);
	XP_FREE(macPath);
	Assert_(err==noErr||err==fnfErr);
	return err;
} // CFileMgr::FSSpecFromLocalUnixPath

Boolean CFileMgr::FileExists( const FSSpec& fsSpec )
{	
	FSSpec		temp;
	OSErr		err;
	
	err = FSMakeFSSpec( fsSpec.vRefNum, fsSpec.parID, fsSpec.name, &temp );
	
	return ( err == noErr );
}

Boolean	CFileMgr::FileHasDataFork(const FSSpec& fsSpec )
{
	CStr31 name = fsSpec.name;
	CInfoPBRec pb;
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = fsSpec.vRefNum;
	pb.hFileInfo.ioDirID = fsSpec.parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	OSErr err = PBGetCatInfoSync(&pb);
	if (err != noErr)
		return FALSE;
	else if (pb.hFileInfo.ioFlLgLen <= 0)
		return FALSE;
	else
		return TRUE;
}
// Does the file have resource fork?
Boolean	CFileMgr::FileHasResourceFork(const FSSpec& fsSpec )
{
	CStr31 name = fsSpec.name;
	CInfoPBRec pb;
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = fsSpec.vRefNum;
	pb.hFileInfo.ioDirID = fsSpec.parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	OSErr err = PBGetCatInfoSync(&pb);
	if (err != noErr)
		return FALSE;
	else if (pb.hFileInfo.ioFlRLgLen <= 0)
		return FALSE;
	else
		return TRUE;
	
}

void CFileMgr::CopyFSSpec(const FSSpec & srcSpec, FSSpec & destSpec)
{
	destSpec.vRefNum = srcSpec.vRefNum;
	destSpec.parID = srcSpec.parID;
	*(CStr31*)&destSpec.name = srcSpec.name;
}

OSErr CFileMgr::GetFolderID(FSSpec& folderSpec, long& dirID)
{
	CInfoPBRec pb;
	pb.hFileInfo.ioNamePtr = folderSpec.name;
	pb.hFileInfo.ioVRefNum = folderSpec.vRefNum;
	pb.hFileInfo.ioDirID = folderSpec.parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	OSErr err = PBGetCatInfoSync(&pb);
	if (err == noErr)
		dirID = pb.dirInfo.ioDrDirID;
	return err;
}

OSErr CFileMgr::DeleteFolder(const FSSpec& folderSpec)
{
	CFileIter iter(folderSpec);
	FSSpec	nextFile;
	FInfo finderInfo;
	Boolean isFolder;
	OSErr err = noErr;
	OSErr storedErr = noErr;
	
	if (FileExists(folderSpec) == false)
		return noErr;
// Delete all the items in the folder
	while ( iter.Next(nextFile, finderInfo, isFolder) )
	{
		if (isFolder)
			err = DeleteFolder(nextFile);
		else
			err = ::FSpDelete(&nextFile);
		if (err != noErr)
			storedErr = err;
	}
// Delete the folder
	err = FSpDelete(&folderSpec);
	if (err != noErr)
		storedErr = err;
	return storedErr;
}

CFileIter::CFileIter(const FSSpec &folderSpec)
{	
	fDir = folderSpec;
	fIndex = 0;

	CInfoPBRec pb;
	pb.hFileInfo.ioNamePtr = (unsigned char*)folderSpec.name; // cast avoids warning
	pb.hFileInfo.ioVRefNum = folderSpec.vRefNum;
	pb.hFileInfo.ioDirID = folderSpec.parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	OSErr err = PBGetCatInfoSync(&pb);
	if (err == noErr)
	{
		fIndex = pb.dirInfo.ioDrNmFls;
		fDir.parID = pb.dirInfo.ioDrDirID;
	}
}

Boolean CFileIter::Next(FSSpec &nextFile, FInfo& finderInfo, Boolean& folder)
{
tryagain:
	if (fIndex <= 0)
		return FALSE;

	CInfoPBRec cipb;
	DirInfo	*dipb=(DirInfo *)&cipb;
	dipb->ioCompletion = NULL;
	dipb->ioFDirIndex = fIndex--;
	dipb->ioVRefNum = fDir.vRefNum;
	dipb->ioDrDirID = fDir.parID;
	dipb->ioNamePtr = (StringPtr)&nextFile.name;
	OSErr err = PBGetCatInfoSync (&cipb);
	if (err != noErr)
		goto tryagain;	// Go backwards, skip the directories
	if ((dipb->ioFlAttrib & 0x10) != 0)
		folder = TRUE;
	else
		folder = FALSE;
	nextFile.vRefNum = fDir.vRefNum;
	nextFile.parID = fDir.parID;
	Boolean dummy,wasAliased;
	err = ::ResolveAliasFile(&nextFile,TRUE,&dummy,&wasAliased);
	if ((err == noErr) && wasAliased)
	{	// Need to get info again
		CInfoPBRec pb;

		pb.hFileInfo.ioNamePtr = (StringPtr)&nextFile.name;
		pb.hFileInfo.ioVRefNum = nextFile.vRefNum;
		pb.hFileInfo.ioDirID = nextFile.parID;
		pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
		err = PBGetCatInfoSync(&pb);
		finderInfo = pb.hFileInfo.ioFlFndrInfo;
		if ((pb.hFileInfo.ioFlAttrib & 0x10) != 0)
			folder = TRUE;
		else
			folder = FALSE;
	}
	else
		finderInfo = cipb.hFileInfo.ioFlFndrInfo;
	return TRUE;
}


OSErr CFileMgr::UpdateFinderDisplay(const FSSpec& inSpec)
{
	AppleEvent theEvent;
	AliasHandle	theAlias = NULL;
	OSErr		theErr = noErr;
	
	try
		{
		theErr = ::NewAlias(NULL, &inSpec, &theAlias);
		ThrowIfOSErr_(theErr);

		FSSpec theFinderSpec;
		ProcessSerialNumber theFinderPSN;
		theErr = FindProcessBySignature('MACS', 'FNDR', theFinderPSN, &theFinderSpec);
		ThrowIfOSErr_(theErr);

		StHandleLocker theAliasLocker((Handle)theAlias);
		//LAEStream theEventStream('fndr', 'fupd', typeProcessSerialNumber, &theFinderPSN, sizeof( ProcessSerialNumber ));
		//theEventStream.WriteKeyDesc(keyDirectObject, typeAlias, *theAlias, ::GetHandleSize((Handle)theAlias));
		//theEventStream.Close(&theEvent);
		//UAppleEventsMgr::SendAppleEvent(theEvent);
		}
	catch(...)
		{
		if (theAlias != NULL)
			::DisposeHandle((Handle)theAlias);
		}
	
	return theErr;
}

void CFileMgr::MakeAliasFile(const FSSpec& aliasSpec, const FSSpec& target)
{
	AliasHandle alias;
	OSErr err = NewAlias( nil, &target, &alias );
	ThrowIfOSErr_(err);
	
	LFile aliasFile(aliasSpec);
	aliasFile.CreateNewFile('MACS', kContainerFolderAliasType);
	aliasFile.OpenResourceFork(fsRdWrPerm);

	AddResource((Handle) alias, 'alis', 0, nil);
		
	FInfo info;
	err = ::FSpGetFInfo(&aliasSpec, &info);
	ThrowIfOSErr_(err);
	
	info.fdFlags |= kIsAlias;
	err = ::FSpSetFInfo(&aliasSpec, &info);
	ThrowIfOSErr_(err);
}

OSErr CFileMgr::CopyFile(const FSSpec& srcSpec, const FSSpec &dstDirSpec, const CStr255 &copyName)
{

	return FSpFileCopy(&srcSpec, &dstDirSpec, (StringPtr)&copyName, nil, 0, true);

}


//-----------------------------------------------------------------------------
// More Utilities
//-----------------------------------------------------------------------------

void  WriteCString (LStream * s, const char * c)
{
	s->WriteData (c, strlen(c));
}

void 
WriteChar (LStream * s, char c)
{
	s->WriteData (&c, 1);
}

//
// fe_FileNameFromContext
//
// Suggest a name for this document based on its title or it's URL content type. If there
// are colons, be sure to swa[ them with /
//

void fe_FileNameFromContext( MWContext* context, const char* url, CStr31& defaultName )
{
#ifdef MOZ_MAIL_NEWS
	char*		urlString;
	
	urlString = MimeGuessURLContentName( context, url );
	if ( urlString )
	{
		SwapSlashColon( urlString );
		defaultName = urlString;
		XP_FREE( urlString );
	}
	else
#endif // MOZ_MAIL_NEWS
	{
		// If we have a context title and it's not an ftp URL
		if (context && context->title && context->title[0] &&
			strncasecomp(url, "ftp://", 6))
		{
			defaultName = context->title;
		}
		else
		{
			defaultName = CFileMgr::FileNameFromURL( url );
		}
	}
	
	// now make sure the resulting name doesn't have any colons.
	char buffer[50];
	short nextInsertPos = 0;
	for ( short loop = 1; loop <= defaultName.Length(); loop++ )
	{
		char currChar = defaultName[loop];
		if ( currChar != ':' ) 
		{
			buffer[nextInsertPos] = currChar;
			nextInsertPos++;
		}
	} // for each character
	buffer[nextInsertPos] = NULL;		// null terminate the new name
	defaultName = buffer;				// make the string w/out colons the default
	
} // fe_FileNameFromContext

OSErr FSSpecFromPathname_CWrapper(char * path, FSSpec * outSpec)
{
	return (CFileMgr::FSSpecFromPathname(path, outSpec));
}
