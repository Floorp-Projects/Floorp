/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "MoreFilesExtras.h"

#include <Aliases.h>
#include <Folders.h>
#include <Errors.h>
#include <TextUtils.h>
#include <Processes.h>

const unsigned char* kAliasHavenFolderName = "\pnsAliasHaven";

//========================================================================================
namespace MacFileHelpers
//========================================================================================
{
	inline void						PLstrcpy(Str255 dst, ConstStr255Param src)
									{
										memcpy(dst, src, 1 + src[0]);
									}

	void			                PLstrcpy(Str255 dst, const char* src, int inMaxLen=255);
	void			                PLstrncpy(Str255 dst, const char* src, int inMaxLen);

	void							SwapSlashColon(char * s);
	OSErr							FSSpecFromUnixPath(
										const char * unixPath,
										FSSpec& ioSpec,
										Boolean hexDecode,
										Boolean resolveAlias,
										Boolean allowPartial = false,
										Boolean createDirs = false);
	char*							MacPathFromUnixPath(
										const char* unixPath,
										Boolean hexDecode);
	char*							EncodeMacPath(
										char* inPath, // NOT const - gets clobbered
										Boolean prependSlash,
										Boolean doEscape );
	OSErr							FSSpecFromPathname(
										const char* inPathNamePtr,
										FSSpec& ioSpec,
										Boolean inCreateDirs);
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
	PRBool							IsAliasSafe(const FSSpec& inSpec);
	OSErr							MakeAliasSafe(FSSpec& inOutSpec);
	OSErr							ResolveAliasFile(FSSpec& inOutSpec, Boolean& wasAliased);

	Boolean							sNoResolve = false;
	long							sAliasHavenDirID = 0;
	short							sAliasHavenVRefNum = 0;
} // namespace MacFileHelpers

//----------------------------------------------------------------------------------------
void MacFileHelpers::PLstrcpy(Str255 dst, const char* src, int inMax)
//----------------------------------------------------------------------------------------
{
	int srcLength = strlen(src);
	NS_ASSERTION(srcLength <= inMax, "Oops, string is too long!");
	if (srcLength > inMax)
		srcLength = inMax;
	dst[0] = srcLength;
	memcpy(&dst[1], src, srcLength);
}

//----------------------------------------------------------------------------------------
void MacFileHelpers::PLstrncpy(Str255 dst, const char* src, int inMax)
//----------------------------------------------------------------------------------------
{
	int srcLength = strlen(src);
	if (srcLength > inMax)
		srcLength = inMax;
	dst[0] = srcLength;
	memcpy(&dst[1], src, srcLength);
}

//-----------------------------------
void MacFileHelpers::SwapSlashColon(char * s)
//-----------------------------------

{
	while (*s)
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
	if (inPath == nsnull)
		return nsnull;
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

	char * newPath = nsnull;
	char * finalPath = nsnull;
	
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
	nsFileSpec dstDirSpec(sAliasHavenVRefNum, sAliasHavenDirID, "\p");

	// Make sure its name is unique
	nsFileSpec havenSpec(sAliasHavenVRefNum, sAliasHavenDirID, "\pG'day");
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
char* MacFileHelpers::MacPathFromUnixPath(const char* unixPath, Boolean hexDecode)
//----------------------------------------------------------------------------------------
{
	// Relying on the fact that the unix path is always longer than the mac path:
	size_t len = strlen(unixPath);
	char* result = new char[len + 2]; // ... but allow for the initial colon in a partial name
	if (result)
	{
		char* dst = result;
		const char* src = unixPath;
		if (*src == '/')		 	// * full path
			src++;
		else if (strchr(src, '/') && *src != '.')
		{
			// * partial path, and not just a leaf name. The '.' test is there because
			// the loop below will add sufficient colons in that case.
			*dst++ = ':';
		}
		// Copy src to dst, but watch out for .. and .
		char c = '/';
		do
		{
		    char cprev = c; // remember the previous char (initially /)
		    c = *src++;
		    if (c == '.' && cprev == '/')
		    {
			    char* dstSaved = dst;
			    // Special cases: "." and "..". Convert to ':' and '::'
		    	*dst++ = ':';  // . becomes :
		    	c = *src++;
		    	if (c == '.')
		    	{
		    	    *dst++ = ':'; // .. becomes ::
		    	    c = *src++;
		    	}
		    	if (c == '/')
		    	{
		    		// ../    becomes   :: so just skip the slash
		    		// ./     becomes   :  "  "    "    "   "
		    		src++;
		    	}
		    	else if (c)
		    	{
		    		// Oh. A file called ".foo" or "..foo"
		    		// Back up and just do the normal thing.
		    		src -= (dst - dstSaved);
		    		dst = dstSaved;
		    		// Since c is not '/', we won't do this stuff on the
		    		// next iteration.
		    	}
		    	continue;
		    }
		    *dst++ = c;
		} while (c);
		if (hexDecode)
			nsUnescape(result);	// Hex Decode
		MacFileHelpers::SwapSlashColon(result);
	}
	return result;
} // MacFileHelpers::MacPathFromUnixPath

//----------------------------------------------------------------------------------------
OSErr MacFileHelpers::FSSpecFromPathname(
	const char* inPathNamePtr,
	FSSpec& ioSpec, // used as in-parameter for a relative path.
	Boolean inCreateDirs)
// FSSpecFromPathname reverses PathNameFromFSSpec.
// It returns a FSSpec given a c string which is a mac pathname.
//----------------------------------------------------------------------------------------
{
	OSErr err;
	// Simplify this routine to use FSMakeFSSpec if length < 255. Otherwise use the MoreFiles
	// routine FSpLocationFromFullPath, which allocates memory, to handle longer pathnames. 
	
	size_t inLength = strlen(inPathNamePtr);
	bool isRelative = (strchr(inPathNamePtr, ':') == 0 || *inPathNamePtr == ':');
	if (inLength < 255)
	{
		Str255 ppath;
		MacFileHelpers::PLstrcpy(ppath, inPathNamePtr);
		if (isRelative)
		    err = ::FSMakeFSSpec(ioSpec.vRefNum, ioSpec.parID, ppath, &ioSpec);
		else
		    err = ::FSMakeFSSpec(0, 0, ppath, &ioSpec);
	}
	else if (!isRelative)
		err = FSpLocationFromFullPath(inLength, inPathNamePtr, &ioSpec);
    else
        err = bdNamErr;

	if ((err == dirNFErr || err == bdNamErr) && inCreateDirs)
	{
		const char* path = inPathNamePtr;
		ioSpec.vRefNum = 0;
		ioSpec.parID = 0;
		do {
			// Locate the colon that terminates the node.
			// But if we've a partial path (starting with a colon), find the second one.
			const char* nextColon = strchr(path + (*path == ':'), ':');
			// Well, if there are no more colons, point to the end of the string.
			if (!nextColon)
				nextColon = path + strlen(path);

			// Make a pascal string out of this node.  Include initial
			// and final colon, if any!
			Str255 ppath;
			MacFileHelpers::PLstrncpy(ppath, path, nextColon - path + 1);
			
			// Use this string as a relative path using the directory created
			// on the previous round (or directory 0,0 on the first round).
			err = ::FSMakeFSSpec(ioSpec.vRefNum, ioSpec.parID, ppath, &ioSpec);

			// If this was the leaf node, then we are done.
			if (!*nextColon)
				break;

			// If we got "file not found", then
			// we need to create a directory.
			if (err == fnfErr && *nextColon)
				err = FSpDirCreate(&ioSpec, smCurrentScript, &ioSpec.parID);
			// For some reason, this usually returns fnfErr, even though it works.
			if (err != noErr && err != fnfErr)
				return err;
			path = nextColon; // next round
		} while (1);
	}
	return err;
} // MacFileHelpers::FSSpecFromPathname

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
PRBool MacFileHelpers::IsAliasSafe(const FSSpec& inSpec)
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
OSErr MacFileHelpers::FSSpecFromUnixPath(
	const char * unixPath,
	FSSpec& ioSpec,
	Boolean hexDecode,
	Boolean resolveAlias,
	Boolean allowPartial,
	Boolean createDirs)
// File spec from URL. Reverses GetURLFromFileSpec 
// Its input is only the <path> part of the URL
// JRM 97/01/08 changed this so that if it's a partial path (doesn't start with '/'),
// then it is combined with inOutSpec's vRefNum and parID to form a new spec.
//-----------------------------------
{
	if (unixPath == nsnull)
		return badFidErr;
	char* macPath = MacPathFromUnixPath(unixPath, hexDecode);
	if (!macPath)
		return memFullErr;

	OSErr err = noErr;
	if (!allowPartial)
	{
		NS_ASSERTION(*unixPath == '/' /*full path*/, "Not a full Unix path!");
	}
	err = FSSpecFromPathname(macPath, ioSpec, createDirs);
	if (err == fnfErr)
		err = noErr;
	Boolean dummy;	
	if (err == noErr && resolveAlias)	// Added 
		err = MacFileHelpers::ResolveAliasFile(ioSpec, dummy);
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
	Handle fullPath = nsnull;
	
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
						(void) Munger(fullPath, 0, nsnull, 0, &tempSpec.name[1], tempSpec.name[0]);
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
//					Macintosh nsFileSpec implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec()
//----------------------------------------------------------------------------------------
:	mError(NS_OK)
{
	mSpec.name[0] = '\0';
}

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
:	mSpec(inSpec.mSpec)
,	mError(inSpec.Error())
{
}

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
{
	mSpec.vRefNum = 0;
	mSpec.parID = 0;
	// Convert unix (non-encoded) path to a spec.
	mError = NS_FILE_RESULT(
	    MacFileHelpers::FSSpecFromUnixPath(
	    	inString,
	    	mSpec, false, false, true, inCreateDirs));
	if (mError == NS_FILE_RESULT(fnfErr))
		mError = NS_OK;
} // nsFileSpec::nsFileSpec

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsString& inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
{
	mSpec.vRefNum = 0;
	mSpec.parID = 0;
	// Convert unix (non-encoded) path to a spec.
	mError = NS_FILE_RESULT(
	    MacFileHelpers::FSSpecFromUnixPath(
	    	nsAutoCString(inString),
	    	mSpec, false, false, true, inCreateDirs));
	if (mError == NS_FILE_RESULT(fnfErr))
		mError = NS_OK;
} // nsFileSpec::nsFileSpec

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(short vRefNum, long parID, ConstStr255Param name)
//----------------------------------------------------------------------------------------
{
	mError = NS_FILE_RESULT(::FSMakeFSSpec(vRefNum, parID, name, &mSpec));
	if (mError == NS_FILE_RESULT(fnfErr))
		mError = NS_OK;
}

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	*this = inPath.GetFileSpec();
}

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
	mPath.SetToEmpty();

	mSpec.vRefNum = 0;
	mSpec.parID = 0;
	// Convert unix (non-encoded) path to a spec.
	mError = NS_FILE_RESULT(
	    MacFileHelpers::FSSpecFromUnixPath(inString, mSpec, false, false, true));
	if (mError == NS_FILE_RESULT(fnfErr))
		mError = NS_OK;
} // nsFileSpec::operator =

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	mPath.SetToEmpty();
	mSpec.vRefNum = inSpec.mSpec.vRefNum;
	mSpec.parID = inSpec.mSpec.parID;
	memcpy(mSpec.name, inSpec.mSpec.name, inSpec.mSpec.name[0] + 1);
	mError = inSpec.Error();
} // nsFileSpec::operator =

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	*this = inPath.GetFileSpec();
} // nsFileSpec::operator =

#if DEBUG
//----------------------------------------------------------------------------------------
nsOutputStream& operator << (nsOutputStream& s, const nsFileSpec& spec)
//----------------------------------------------------------------------------------------
{
#if 0
	s << spec.mSpec.vRefNum << ", " << spec.mSpec.parID << ", \"";
	s.write((const char*)&spec.mSpec.name[1], spec.mSpec.name[0]);
	return s << "\"";	
#else
    return s << "\"" << spec.GetCString() << "\"";
#endif
} // nsOutputStream& operator << (nsOutputStream&, const nsFileSpec&)
#endif

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::Exists() const
//----------------------------------------------------------------------------------------
{
	FSSpec temp;
	return ::FSMakeFSSpec(mSpec.vRefNum, mSpec.parID, mSpec.name, &temp) == noErr;
} // nsFileSpec::Exists()

//----------------------------------------------------------------------------------------
void nsFileSpec::GetModDate(TimeStamp& outStamp) const
//----------------------------------------------------------------------------------------
{
	CInfoPBRec pb;
    if (GetCatInfo(pb) == noErr)
        outStamp = ((DirInfo*)&pb)->ioDrMdDat; // The mod date is in the same spot for files and dirs.
    else
        outStamp = 0;
} // nsFileSpec::GetModDate

//----------------------------------------------------------------------------------------
PRUint32 nsFileSpec::GetFileSize() const
//----------------------------------------------------------------------------------------
{
	CInfoPBRec pb;
    if (noErr == GetCatInfo(pb))
        return (PRUint32)((HFileInfo*)&pb)->ioFlLgLen;
    return 0;
} // nsFileSpec::GetFileSize

//----------------------------------------------------------------------------------------
void nsFileSpec::SetLeafName(const char* inLeafName)
// In leaf name can actually be a partial path...
//----------------------------------------------------------------------------------------
{
	mPath.SetToEmpty();

	// what about long relative paths?  Hmm?  We don't have a routine for this anywhere.
	Str255 partialPath;
	MacFileHelpers::PLstrcpy(partialPath, inLeafName);
	mError = NS_FILE_RESULT(
	    ::FSMakeFSSpec(mSpec.vRefNum, mSpec.parID, partialPath, &mSpec));
	if (mError == NS_FILE_RESULT(fnfErr))
		mError = NS_OK;
} // nsFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
char* nsFileSpec::GetLeafName() const
// Result needs to be delete[]ed.
//----------------------------------------------------------------------------------------
{
	char leaf[64];
	memcpy(leaf, &mSpec.name[1], mSpec.name[0]);
	leaf[mSpec.name[0]] = '\0';
	return PL_strdup(leaf);
} // nsFileSpec::GetLeafName

//----------------------------------------------------------------------------------------
void nsFileSpec::MakeAliasSafe()
//----------------------------------------------------------------------------------------
{
	mPath.SetToEmpty();
	mError = NS_FILE_RESULT(MacFileHelpers::MakeAliasSafe(mSpec));
} // nsFileSpec::MakeAliasSafe

//----------------------------------------------------------------------------------------
void nsFileSpec::MakeUnique(ConstStr255Param inSuggestedLeafName)
//----------------------------------------------------------------------------------------
{
	mPath.SetToEmpty();
	if (inSuggestedLeafName[0] > 0)
		MacFileHelpers::PLstrcpy(mSpec.name, inSuggestedLeafName);

	MakeUnique();
} // nsFileSpec::MakeUnique

//----------------------------------------------------------------------------------------
void nsFileSpec::ResolveAlias(PRBool& wasAliased)
//----------------------------------------------------------------------------------------
{
	Boolean wasAliased2;
	mError = NS_FILE_RESULT(MacFileHelpers::ResolveAliasFile(mSpec, wasAliased2));
	wasAliased = (wasAliased2 != false);
} // nsFileSpec::ResolveAlias

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsFile() const
//----------------------------------------------------------------------------------------
{
	long dirID;
	Boolean isDirectory;
	return (noErr == FSpGetDirectoryID(&mSpec, &dirID, &isDirectory) && !isDirectory);
} // nsFileSpec::IsFile

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsDirectory() const
//----------------------------------------------------------------------------------------
{
	long dirID;
	Boolean isDirectory;
	return (noErr == FSpGetDirectoryID(&mSpec, &dirID, &isDirectory) && isDirectory);
} // nsFileSpec::IsDirectory

//----------------------------------------------------------------------------------------
void nsFileSpec::GetParent(nsFileSpec& outSpec) const
//----------------------------------------------------------------------------------------
{
	if (NS_SUCCEEDED(mError))
		outSpec.mError
		    = NS_FILE_RESULT(::FSMakeFSSpec(mSpec.vRefNum, mSpec.parID, nsnull, outSpec));
} // nsFileSpec::GetParent

//----------------------------------------------------------------------------------------
void nsFileSpec::operator += (const char* inRelativePath)
//----------------------------------------------------------------------------------------
{
	long dirID;
	Boolean isDirectory;
	mError = NS_FILE_RESULT(::FSpGetDirectoryID(&mSpec, &dirID, &isDirectory));
	if (NS_SUCCEEDED(mError) && isDirectory)
	{
        mSpec.parID = dirID;
        // mSpec.vRefNum is already correct.
        // Convert unix path (which is unencoded) to a spec
        mError = NS_FILE_RESULT(
            MacFileHelpers::FSSpecFromUnixPath(
            	inRelativePath, mSpec, false, false, true, true));
        if (mError == NS_FILE_RESULT(fnfErr))
	        mError = NS_OK;
	}
} // nsFileSpec::operator +=

//----------------------------------------------------------------------------------------
void nsFileSpec::CreateDirectory(int /* unix mode */)
//----------------------------------------------------------------------------------------
{
	long ignoredDirID;
	mError = NS_FILE_RESULT(FSpDirCreate(&mSpec, smCurrentScript, &ignoredDirID));
} // nsFileSpec::CreateDirectory

//----------------------------------------------------------------------------------------
void nsFileSpec::Delete(PRBool inRecursive) const
//----------------------------------------------------------------------------------------
{
	nsresult& mutableError = const_cast<nsFileSpec*>(this)->mError;
	if (inRecursive)
	{
		// MoreFilesExtras
		mutableError = NS_FILE_RESULT(::DeleteDirectory(
					mSpec.vRefNum,
					mSpec.parID,
					const_cast<unsigned char*>(mSpec.name)));
	}
	else
		mutableError = NS_FILE_RESULT(FSpDelete(&mSpec));
} // nsFileSpec::Delete

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Rename(const char* inNewName)
//----------------------------------------------------------------------------------------
{
    if (strchr(inNewName, '/'))
        return -1; // no relative paths here!
    Str255 pName;
    MacFileHelpers::PLstrcpy(pName, inNewName);
    if (FSpRename(&mSpec, pName) != noErr)
        return -1;
    SetLeafName(inNewName);
    return 0;
} // nsFileSpec::Rename

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Copy(const nsFileSpec& newParentDir) const
//----------------------------------------------------------------------------------------
{
    // We can only copy into a directory, and (for now) can not copy entire directories

    if (!newParentDir.IsDirectory() || (IsDirectory() ) )
        return NS_FILE_FAILURE;

    
    nsresult result = NS_FILE_RESULT(::FSpFileCopy(   &mSpec,
                            &newParentDir.mSpec,
                            const_cast<StringPtr>(GetLeafPName()),
                            nsnull,
                            0,
                            true));

    return result;

} // nsFileSpec::Copy

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Move(const nsFileSpec& newParentDir) 
//----------------------------------------------------------------------------------------
{
    // We can only move into a directory
    
    if (!newParentDir.IsDirectory())
    	return NS_FILE_FAILURE;
 
    nsresult result = NS_FILE_RESULT(::FSpMoveRenameCompat(&mSpec,
                                    &newParentDir.mSpec,
                                    const_cast<StringPtr>(GetLeafPName())));

    if ( NS_SUCCEEDED(result) )
        *this = newParentDir + GetLeafName();
    
    return result;
} // nsFileSpec::Move

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Execute(const char* /*args - how can this be cross-platform?  problem! */ ) const
//----------------------------------------------------------------------------------------
{
    if (IsDirectory())
        return NS_FILE_FAILURE;

    LaunchParamBlockRec launchThis;
    launchThis.launchAppSpec = const_cast<FSSpec*>(&mSpec);
    launchThis.launchAppParameters = nsnull; // args;
    /* launch the thing */
    launchThis.launchBlockID    = extendedBlock;
    launchThis.launchEPBLength  = extendedBlockLen;
    launchThis.launchFileFlags  = nsnull;
    launchThis.launchControlFlags = launchContinue + launchNoFileFlags + launchUseMinimum;
    launchThis.launchControlFlags += launchDontSwitch;

    nsresult result = NS_FILE_RESULT(::LaunchApplication(&launchThis));
    return result;
  
} // nsFileSpec::Execute

//----------------------------------------------------------------------------------------
OSErr nsFileSpec::GetCatInfo(CInfoPBRec& outInfo) const
//----------------------------------------------------------------------------------------
{
	DirInfo	*dipb=(DirInfo *)&outInfo;
    dipb->ioCompletion = nsnull;
	dipb->ioFDirIndex = 0; // use dirID and name
	dipb->ioVRefNum = mSpec.vRefNum;
	dipb->ioDrDirID = mSpec.parID;
	dipb->ioNamePtr = const_cast<nsFileSpec*>(this)->mSpec.name;
	return PBGetCatInfoSync(&outInfo);
} // nsFileSpec::GetCatInfo()

//----------------------------------------------------------------------------------------
PRUint32 nsFileSpec::GetDiskSpaceAvailable() const
//----------------------------------------------------------------------------------------
{
	HVolumeParam	pb;
	pb.ioCompletion = NULL;
	pb.ioVolIndex = 0;
	pb.ioNamePtr = NULL;
	pb.ioVRefNum = mSpec.vRefNum;
	
	OSErr err = PBHGetVInfoSync( (HParmBlkPtr)&pb );
	
	if ( err == noErr )
		return pb.ioVFrBlk * pb.ioVAlBlkSiz;
	return ULONG_MAX;
} // nsFileSpec::GetDiskSpace()

//----------------------------------------------------------------------------------------
const char* nsFileSpec::GetCString() const
// This is the only conversion to const char* that is provided, and it allows the
// path to be "passed" to NSPR file routines.  This practice is VERY EVIL and should only
// be used to support legacy code.  Using it guarantees bugs on Macintosh. The string is
// cached and freed by the nsFileSpec destructor, so do not delete (or free) it.
//----------------------------------------------------------------------------------------
{
    if (mPath.IsEmpty())
    {
        const_cast<nsFileSpec*>(this)->mPath
            = MacFileHelpers::PathNameFromFSSpec(mSpec, true);
        if (mPath.IsEmpty())
            const_cast<nsFileSpec*>(this)->mError = NS_ERROR_OUT_OF_MEMORY;
    }
    return mPath;
}

//========================================================================================
//					Macintosh nsFilePath implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mFileSpec(inString, inCreateDirs)
{
    // Make canonical and absolute.
	char * path = MacFileHelpers::PathNameFromFSSpec( mFileSpec, true );
	path = MacFileHelpers::EncodeMacPath(path, true, false);
	mPath = path;
	delete [] path;
}

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsString& inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mFileSpec(nsAutoCString(inString), inCreateDirs)
{
    // Make canonical and absolute.
	char * path = MacFileHelpers::PathNameFromFSSpec( mFileSpec, true );
	path = MacFileHelpers::EncodeMacPath(path, true, false);
	mPath = path;
	delete [] path;
}

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	*this = inSpec;
}

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
	*this = inOther;
}

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	char * path = MacFileHelpers::PathNameFromFSSpec(inSpec, true);
	path = MacFileHelpers::EncodeMacPath(path, true, false);
	mPath = path;
	delete [] path;
	mFileSpec = inSpec;
} // nsFilePath::operator =

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
	char * path = MacFileHelpers::PathNameFromFSSpec(inOther.mFileSpec, true);
	path = MacFileHelpers::EncodeMacPath(path, true, false);
	mPath = path;
	delete [] path;
    mFileSpec = inOther.GetFileSpec();
}

//========================================================================================
//                                nsFileURL implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mURL(inString)
{    
    NS_ASSERTION(strstr(inString, kFileURLPrefix) == inString, "Not a URL!");
    mFileSpec.mError = NS_FILE_RESULT(MacFileHelpers::FSSpecFromUnixPath(
    	inString + kFileURLPrefixLength,
    	mFileSpec.mSpec,
    	true, // need to decode
    	false, // don't resolve alias
    	false, // must be a full path
    	inCreateDirs));
    if (mFileSpec.mError == NS_FILE_RESULT(fnfErr))
        mFileSpec.mError = NS_OK;
} // nsFileURL::nsFileURL

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsString& inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:    mURL(nsnull)
{
    nsAutoCString autostring(inString);
    const char* cstring = (const char*)autostring;
    mURL = cstring;
    NS_ASSERTION(strstr(cstring, kFileURLPrefix) == cstring, "Not a URL!");
    mFileSpec.mError = NS_FILE_RESULT(MacFileHelpers::FSSpecFromUnixPath(
    	cstring + kFileURLPrefixLength,
    	mFileSpec.mSpec,
    	true, // need to decode
    	false, // don't resolve alias
    	false, // must be a full path
    	inCreateDirs));
    if (mFileSpec.mError == NS_FILE_RESULT(fnfErr))
        mFileSpec.mError = NS_OK;
} // nsFileURL::nsFileURL

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
	*this = inOther.GetFileSpec();
} // nsFileURL::nsFileURL

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
	*this = inOther;
} // nsFileURL::nsFileURL

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
    *this = inOther.GetFileSpec();
} // nsFileURL::operator =

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
    mFileSpec  = inOther;
	char* path = MacFileHelpers::PathNameFromFSSpec( mFileSpec, true );
	char* encodedPath = MacFileHelpers::EncodeMacPath(path, true, true);
	nsSimpleCharString encodedURL(kFileURLPrefix);
	encodedURL += encodedPath;
	delete [] encodedPath;
    mURL = encodedURL;
	if (encodedURL[encodedURL.Length() - 1] != '/' && inOther.IsDirectory())
	    mURL += "/";
} // nsFileURL::operator =

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
    mURL = inString;
    NS_ASSERTION(strstr(inString, kFileURLPrefix) == inString, "Not a URL!");
    mFileSpec.mError = NS_FILE_RESULT(MacFileHelpers::FSSpecFromUnixPath(
    	inString + kFileURLPrefixLength,
    	mFileSpec.mSpec,
    	true, // need to decode
    	false, // don't resolve alias
    	false, // must be a full path
    	false)); // don't create dirs.
    if (mFileSpec.mError == NS_FILE_RESULT(fnfErr))
        mFileSpec.mError = NS_OK;
} // nsFileURL::operator =

//========================================================================================
//								nsDirectoryIterator
//========================================================================================

//----------------------------------------------------------------------------------------
nsDirectoryIterator::nsDirectoryIterator(
	const nsFileSpec& inDirectory
,	int inIterateDirection)
//----------------------------------------------------------------------------------------
	: mCurrent(inDirectory)
	, mExists(false)
	, mIndex(-1)
{
	CInfoPBRec pb;
    OSErr err = inDirectory.GetCatInfo(pb);
    
	// test that we have got a directory back, not a file
	DirInfo* dipb = (DirInfo*)&pb;
	if (err != noErr  || !( dipb->ioFlAttrib & 0x0010))
		return;
	// Sorry about this, there seems to be a bug in CWPro 4:
	FSSpec& currentSpec = mCurrent.nsFileSpec::operator FSSpec&();
	currentSpec.vRefNum = currentSpec.vRefNum;
	currentSpec.parID = dipb->ioDrDirID;
	mMaxIndex = pb.dirInfo.ioDrNmFls;
	if (inIterateDirection > 0)
	{
		mIndex = 0; // ready to increment
		++(*this); // the pre-increment operator
	}
	else
	{
		mIndex = mMaxIndex + 1; // ready to decrement
		--(*this); // the pre-decrement operator
	}
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
OSErr nsDirectoryIterator::SetToIndex()
//----------------------------------------------------------------------------------------
{
	CInfoPBRec cipb;
	DirInfo	*dipb=(DirInfo *)&cipb;
	Str255 objectName;
	dipb->ioCompletion = nsnull;
	dipb->ioFDirIndex = mIndex;
	// Sorry about this, there seems to be a bug in CWPro 4:
	FSSpec& currentSpec = mCurrent.nsFileSpec::operator FSSpec&();
	dipb->ioVRefNum = currentSpec.vRefNum; /* Might need to use vRefNum, not sure*/
	dipb->ioDrDirID = currentSpec.parID;
	dipb->ioNamePtr = objectName;
	OSErr err = PBGetCatInfoSync(&cipb);
	if (err == noErr)
		err = FSMakeFSSpec(currentSpec.vRefNum, currentSpec.parID, objectName, &currentSpec);
	mExists = err == noErr;
	return err;
} // nsDirectoryIterator::SetToIndex()

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator -- ()
//----------------------------------------------------------------------------------------
{
	mExists = false;
	while (--mIndex > 0)
	{
		OSErr err = SetToIndex();
		if (err == noErr)
			break;
	}
	return *this;
} // nsDirectoryIterator::operator --

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator ++ ()
//----------------------------------------------------------------------------------------
{
	mExists = false;
	if (mIndex >= 0) // probably trying to use a file as a directory!
		while (++mIndex <= mMaxIndex)
		{
			OSErr err = SetToIndex();
			if (err == noErr)
				break;
		}
	return *this;
} // nsDirectoryIterator::operator ++

