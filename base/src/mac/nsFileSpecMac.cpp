/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 
//	This file is included by nsFile.cp, and includes the Macintosh-specific
//	implementations.

#include "FullPath.h"
#include "FileCopy.h"
#include "nsEscape.h"

#include <Aliases.h>
#include <Folders.h>
#include <Errors.h>
#include <TextUtils.h>

const unsigned char* kAliasHavenFolderName = "\pnsAliasHaven";

//========================================================================================
namespace MacFileHelpers
//========================================================================================
{
	inline void						PLstrcpy(Str255 dst, ConstStr255Param src)
									{
										memcpy(dst, src, 1 + src[0]);
									}

	void			                PLstrcpy(Str255 dst, const char* src, int inMaxLen);

	void							SwapSlashColon(char * s);
	OSErr							FSSpecFromFullUnixPath(
										const char * unixPath,
										FSSpec& outSpec,
										Boolean resolveAlias);
	char*							MacPathFromUnixPath(const char* unixPath);
	char*							EncodeMacPath(
										char* inPath, // NOT const - gets clobbered
										Boolean prependSlash,
										Boolean doEscape );
	OSErr							FSSpecFromPathname(
										const char* inPathNamePtr,
										FSSpec& outSpec);
	char*							PathNameFromFSSpec(
										const FSSpec& inSpec,
										Boolean wantLeafName );
	OSErr							CreateFolderInFolder(
										short				refNum, 	// Parent directory/volume
										long				dirID,
										ConstStr255Param	folderName,	// Name of the new folder
										short&				outRefNum,	// Volume of the created folder
										long&				outDirID);	// 

	// Some routines to support an "alias haven" directory.  Aliases in this directory
	// are never resolved.  There is a ResolveAlias here that respects that.  This is
	// to support attaching of aliases in mail.
	void							EnsureAliasHaven();
	void							SetNoResolve(Boolean inResolve);
	bool							IsAliasSafe(const FSSpec& inSpec);
	OSErr							MakeAliasSafe(FSSpec& inOutSpec);
	OSErr							ResolveAliasFile(FSSpec& inOutSpec, Boolean& wasAliased);

	Boolean							sNoResolve = false;
	long							sAliasHavenDirID = 0;
	short							sAliasHavenVRefNum = 0;
} // namespace MacFileHelpers

//----------------------------------------------------------------------------------------
void MacFileHelpers::PLstrcpy(Str255 dst, const char* src, int inMax)
//========================================================================================
{
	int srcLength = strlen(src);
	NS_ASSERTION(srcLength <= inMax, "Oops, string is too long!");
	if (srcLength > inMax)
		srcLength = inMax;
	dst[0] = srcLength;
	memcpy(&dst[1], src, srcLength);
}

//-----------------------------------
void MacFileHelpers::SwapSlashColon(char * s)
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
} // MacFileHelpers::SwapSlashColon

//-----------------------------------
char* MacFileHelpers::EncodeMacPath(
	char* inPath, // NOT const, gets clobbered
	Boolean prependSlash,
	Boolean doEscape )
//	Transforms Macintosh style path into Unix one
//	Method: Swap ':' and '/', hex escape the result
//-----------------------------------
{
	if (inPath == NULL)
		return NULL;
	int pathSize = strlen(inPath);
	
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
		newPath = new char[pathSize + 2];
		newPath[0] = ':';	// It will be converted to '/'
		memcpy(&newPath[1], inPath, pathSize + 1);
	}
	else
	{
		newPath = new char[pathSize + 1];
		strcpy(newPath, inPath);
	}
	if (newPath)
	{
		SwapSlashColon( newPath );
		if (doEscape)
		{
			finalPath = nsEscape(newPath, url_Path);
			delete [] newPath;
		}
		else
			finalPath = newPath;
	}
	delete [] inPath;
	return finalPath;
} // MacFileHelpers::EncodeMacPath

//----------------------------------------------------------------------------------------
inline void MacFileHelpers::SetNoResolve(Boolean inResolve)
//----------------------------------------------------------------------------------------
{
	sNoResolve = inResolve;
} // MacFileHelpers::SetNoResolve

//----------------------------------------------------------------------------------------
OSErr MacFileHelpers::MakeAliasSafe(FSSpec& inOutSpec)
// Pass in the spec of an alias.  This copies the file to the safe haven folder, and
// returns the spec of the copy to the caller
//----------------------------------------------------------------------------------------
{
	EnsureAliasHaven();
	nsNativeFileSpec dstDirSpec(sAliasHavenVRefNum, sAliasHavenDirID, "\p");

	// Make sure its name is unique
	nsNativeFileSpec havenSpec(sAliasHavenVRefNum, sAliasHavenDirID, "\pG'day");
	if (havenSpec.Valid())
		havenSpec.MakeUnique(inOutSpec.name);
	// Copy the file into the haven directory
	if (havenSpec.Valid())
	{
		OSErr err = ::FSpFileCopy(
						&inOutSpec,
						dstDirSpec,
						havenSpec.GetLeafPName(),
						nil, 0, true);
		// Return the spec of the copy to the caller.
		if (err != noErr)
			return err;
		inOutSpec = havenSpec;
	}
	return noErr;
} // MacFileHelpers::MakeAliasSafe

//----------------------------------------------------------------------------------------
char* MacFileHelpers::MacPathFromUnixPath(const char* unixPath)
//----------------------------------------------------------------------------------------
{
	// Relying on the fact that the unix path is always longer than the mac path:
	size_t len = strlen(unixPath);
	char* result = new char[len + 2]; // ... but allow for the initial colon in a partial name
	if (result)
	{
		char* dst = result;
		const char* src = unixPath;
		if (*src == '/')		 	// ¥ full path
			src++;
		else if (strchr(src, '/'))	// ¥ partial path, and not just a leaf name
			*dst++ = ':';
		strcpy(dst, src);
		nsUnescape(dst);	// Hex Decode
		MacFileHelpers::SwapSlashColon(dst);
	}
	return result;
} // MacFileHelpers::MacPathFromUnixPath

//----------------------------------------------------------------------------------------
OSErr MacFileHelpers::FSSpecFromPathname(const char* inPathNamePtr, FSSpec& outSpec)
// FSSpecFromPathname reverses PathNameFromFSSpec.
// It returns a FSSpec given a c string which is a mac pathname.
//----------------------------------------------------------------------------------------
{
	OSErr err;
	// Simplify this routine to use FSMakeFSSpec if length < 255. Otherwise use the MoreFiles
	// routine FSpLocationFromFullPath, which allocates memory, to handle longer pathnames. 
	if (strlen(inPathNamePtr) < 255)
	{
		Str255 path;
		
		int pos = 0;
		while ( (path[++pos] = *inPathNamePtr++) != 0 )
			;
		path[0] = pos-1;	// save the length of the string (pos is the next open spot)
			
		err = ::FSMakeFSSpec(0, 0, path, &outSpec);
	}
	else
		err = FSpLocationFromFullPath(strlen(inPathNamePtr), inPathNamePtr, &outSpec);

	return err;
}

//----------------------------------------------------------------------------------------
OSErr MacFileHelpers::CreateFolderInFolder(
	short				refNum, 	// Parent directory/volume
	long				dirID,
	ConstStr255Param	folderName,	// Name of the new folder
	short&				outRefNum,	// Volume of the created folder
	long&				outDirID)	// 
// Creates a folder named 'folderName' inside a folder.
// The errors returned are same as PBDirCreate
//----------------------------------------------------------------------------------------
{
	HFileParam hpb;
	hpb.ioVRefNum = refNum;
	hpb.ioDirID = dirID;
	hpb.ioNamePtr = (StringPtr)&folderName;

	OSErr err = PBDirCreateSync((HParmBlkPtr)&hpb);
	if (err == noErr)
	{
		outRefNum = hpb.ioVRefNum;
		outDirID = hpb.ioDirID;
	}
	else
	{
		outRefNum = 0;
		outDirID = 0;
	}
	return err;
} // MacFileHelpers::CreateFolderInFolder

//----------------------------------------------------------------------------------------
void MacFileHelpers::EnsureAliasHaven()
//----------------------------------------------------------------------------------------
{
	// Alias Haven is a directory in which we never resolve aliases.
	if (sAliasHavenVRefNum != 0)
		return;

	
	FSSpec temp;
	if (FindFolder(0, kTemporaryFolderType, true, & temp.vRefNum, &temp.parID) == noErr)
	{
		CreateFolderInFolder(
			temp.vRefNum,	 			// Parent directory/volume
			temp.parID,
			kAliasHavenFolderName,		// Name of the new folder
			sAliasHavenVRefNum,		// Volume of the created folder
			sAliasHavenDirID);		
	}
} // MacFileHelpers::EnsureAliasHaven

//----------------------------------------------------------------------------------------
bool MacFileHelpers::IsAliasSafe(const FSSpec& inSpec)
// Returns true if the alias is in the alias haven directory, or if alias resolution
// has been turned off.
//----------------------------------------------------------------------------------------
{
	return sNoResolve
		|| (inSpec.parID == sAliasHavenDirID && inSpec.vRefNum == sAliasHavenVRefNum);
} // MacFileHelpers::IsAliasSafe

//----------------------------------------------------------------------------------------
OSErr MacFileHelpers::ResolveAliasFile(FSSpec& inOutSpec, Boolean& wasAliased)
//----------------------------------------------------------------------------------------
{
	wasAliased = false;
	if (IsAliasSafe(inOutSpec))
		return noErr;
	Boolean dummy;	
	return ::ResolveAliasFile(&inOutSpec, TRUE, &dummy, &wasAliased);
} // MacFileHelpers::ResolveAliasFile

//-----------------------------------
OSErr MacFileHelpers::FSSpecFromFullUnixPath(
	const char * unixPath,
	FSSpec& outSpec,
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
	NS_ASSERTION(*unixPath == '/' /*full path*/, "Not a Unix path!");
	err = FSSpecFromPathname(macPath, outSpec);
	if (err == fnfErr)
		err = noErr;
	Boolean dummy;	
	if (err == noErr && resolveAlias)	// Added 
		err = MacFileHelpers::ResolveAliasFile(outSpec, dummy);
	delete [] macPath;
	NS_ASSERTION(err==noErr||err==fnfErr||err==dirNFErr||err==nsvErr, "Not a path!");
	return err;
} // MacFileHelpers::FSSpecFromLocalUnixPath

//-----------------------------------
char* MacFileHelpers::PathNameFromFSSpec( const FSSpec& inSpec, Boolean wantLeafName )
// Returns a full pathname to the given file
// Returned value is allocated with new [], and must be freed with delete []
// This is taken from FSpGetFullPath in MoreFiles, except that we need to tolerate
// fnfErr.
//-----------------------------------
{
	char* result = nil;
	OSErr err = noErr;
	
	short fullPathLength = 0;
	Handle fullPath = NULL;
	
	FSSpec tempSpec = inSpec;
	if ( tempSpec.parID == fsRtParID )
	{
		/* The object is a volume */
		
		/* Add a colon to make it a full pathname */
		tempSpec.name[++tempSpec.name[0]] = ':';
		
		/* We're done */
		err = PtrToHand(&tempSpec.name[1], &fullPath, tempSpec.name[0]);
	}
	else
	{
		/* The object isn't a volume */
		
		CInfoPBRec	pb = { 0 };
		Str63 dummyFileName;
		MacFileHelpers::PLstrcpy(dummyFileName, "\pG'day!");

		/* Is the object a file or a directory? */
		pb.dirInfo.ioNamePtr = (! tempSpec.name[0]) ? (StringPtr)dummyFileName : tempSpec.name;
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
	result = new char[allocSize];
	if (!result)
		goto Clean;
	memcpy(result, *fullPath, allocSize - 1);
	result[ allocSize - 1 ] = 0;
Clean:
	if (fullPath)
		DisposeHandle(fullPath);
	NS_ASSERTION(result, "Out of memory"); // OOPS! very bad.
	return result;
} // MacFileHelpers::PathNameFromFSSpec

//========================================================================================
//					Macintosh nsNativeFileSpec implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec()
//----------------------------------------------------------------------------------------
:	mError(noErr)
{
	mSpec.name[0] = '\0';
}

//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec(const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
:	mSpec(inSpec.mSpec)
,	mError(inSpec.Error())
{
}

//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec(const char* inString)
//----------------------------------------------------------------------------------------
{
	mError = MacFileHelpers::FSSpecFromFullUnixPath(inString, mSpec, true);
	if (mError == fnfErr)
		mError = noErr;
} // nsNativeFileSpec::nsNativeFileSpec

//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec(
	short vRefNum,
	long parID,
	ConstStr255Param name)
//----------------------------------------------------------------------------------------
{
	mError = ::FSMakeFSSpec(vRefNum, parID, name, &mSpec);
	if (mError == fnfErr)
		mError = noErr;
}

//----------------------------------------------------------------------------------------
nsNativeFileSpec::nsNativeFileSpec(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	*this = inPath.GetNativeSpec();
}

//----------------------------------------------------------------------------------------
nsOutputFileStream& operator << (nsOutputFileStream& s, const nsNativeFileSpec& spec)
//----------------------------------------------------------------------------------------
{
	s << spec.mSpec.vRefNum << ", " << spec.mSpec.parID << ", \"";
	s.write((const char*)&spec.mSpec.name[1], spec.mSpec.name[0]);
	return s << "\"";	
} // nsOutputFileStream& operator << (nsOutputFileStream&, const nsNativeFileSpec&)

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
	mError = MacFileHelpers::FSSpecFromFullUnixPath(inString, mSpec, true);
} // nsNativeFileSpec::operator =

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator = (const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	mSpec = inSpec.mSpec;
	mError = inSpec.Error();
} // nsNativeFileSpec::operator =

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::operator = (const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	*this = inPath.GetNativeSpec();
} // nsNativeFileSpec::operator =

//----------------------------------------------------------------------------------------
bool nsNativeFileSpec::Exists() const
//----------------------------------------------------------------------------------------
{
	FSSpec temp;
	return ::FSMakeFSSpec(mSpec.vRefNum, mSpec.parID, mSpec.name, &temp) == noErr;
} // nsNativeFileSpec::operator =

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::SetLeafName(const char* inLeafName)
//----------------------------------------------------------------------------------------
{
	MacFileHelpers::PLstrcpy(mSpec.name, inLeafName, nsFileSpecHelpers::kMaxFilenameLength);
} // nsNativeFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
char* nsNativeFileSpec::GetLeafName() const
// Result needs to be delete[]ed.
//----------------------------------------------------------------------------------------
{
	char leaf[64];
	memcpy(leaf, &mSpec.name[1], mSpec.name[0]);
	leaf[mSpec.name[0]] = '\0';
	return nsFileSpecHelpers::StringDup(leaf);
} // nsNativeFileSpec::GetLeafName

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::MakeAliasSafe()
//----------------------------------------------------------------------------------------
{
	mError = MacFileHelpers::MakeAliasSafe(mSpec);
} // nsNativeFileSpec::MakeAliasSafe

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::MakeUnique(ConstStr255Param inSuggestedLeafName)
//----------------------------------------------------------------------------------------
{
	if (inSuggestedLeafName[0] > 0)
		MacFileHelpers::PLstrcpy(mSpec.name, inSuggestedLeafName);

	MakeUnique();
} // nsNativeFileSpec::MakeUnique

//----------------------------------------------------------------------------------------
void nsNativeFileSpec::ResolveAlias(bool& wasAliased)
//----------------------------------------------------------------------------------------
{
	Boolean wasAliased2;
	mError = MacFileHelpers::ResolveAliasFile(mSpec, wasAliased2);
	wasAliased = (wasAliased2 != false);
} // nsNativeFileSpec::ResolveAlias

//========================================================================================
//					Macintosh nsFilePath implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
:	mNativeFileSpec(inSpec)
{
	char * path = MacFileHelpers::PathNameFromFSSpec( inSpec.mSpec, TRUE );
	mPath = MacFileHelpers::EncodeMacPath(path, true, true);
}

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsNativeFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	delete [] mPath;
	char * path = MacFileHelpers::PathNameFromFSSpec( inSpec.mSpec, TRUE );
	mPath = MacFileHelpers::EncodeMacPath(path, true, true);
	mNativeFileSpec = inSpec;
} // nsFilePath::operator =
