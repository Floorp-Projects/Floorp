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

/* xp_file_mac.h
 * Mac-only xp_file functions
 */
 #include "xp_file_mac.h"

	/* macfe */
#include "resgui.h"
#include "ufilemgr.h"
#include "uprefd.h"

	/* utilities */
#include "MoreFilesExtras.h"
#include "FSpCompat.h"
#include "FileCopy.h"
#include "PascalString.h"
#include "Files.h"
#include <LString.h>
#include "cstring.h"

// validation flags for overloaded ConvertURLToSpec
enum ConvertURLValidationFlags {
	validateValFlag = 0x00000001,
	stripValFlag    = 0x00000002,
	appendValFlag   = 0x00000004,
	stripAndAppendValFlag = (stripValFlag | appendValFlag) };

// Prototypes
OSErr ConvertURLToSpec
	(const char* inName,
	FSSpec* outSpec,
	ResIDT resid = 0,
	Boolean validate = false);
OSErr ConvertURLToSpec(
	const char* inName,
	FSSpec* outSpec,
	const char* suffix = nil,
	ConvertURLValidationFlags validate = validateValFlag);

/**************************************************************************************
 * See xp_file.h for full documentation.  BE SURE TO READ THAT FILE.
 **************************************************************************************/
   
//-----------------------------------
OSErr ConvertURLToSpec(const char* inName, FSSpec* outSpec, ResIDT resid, Boolean validate)
// Returns a file spec given a name (URL form), and resid of an extension to add.
// If ResID is zero, just convert.
// If resid is nonzero and validate is true, check the input name and return an error
// 		if the name does NOT have the suffix.
// If resid is nonzero and validate is false, append the suffix to the input name
//-----------------------------------
{
	if (!inName)
		return bdNamErr;
#ifdef DEBUG
	if (XP_STRCHR(inName, ':'))
		XP_ASSERT(0);	// Are they passing us a Mac path? 
#endif

	OSErr err = CFileMgr::FSSpecFromLocalUnixPath(inName, outSpec);
	if (err && err != fnfErr) 	/* fnfErr is OK */
		return err;
	if (resid)
	{
		CStr255 extension;
		::GetIndString(extension, 300, resid);
		if (validate)
		{
			SInt32 suffixPosition = (XP_STRLEN(inName) - XP_STRLEN(extension));
			if (suffixPosition < 0)
				return bdNamErr;
			if (XP_STRCASECMP(extension, inName + suffixPosition) != 0)
				return bdNamErr;
		}
		else
			*(CStr63*)(outSpec->name) += extension;
	}
	return noErr;
} // ConvertURLToSpec
	
//-----------------------------------
OSErr ConvertURLToSpec(const char* inName, FSSpec* outSpec, const char* suffix,
			ConvertURLValidationFlags validate) 
// Returns a file spec given a name (URL form), and resid of an extension to add.
// If ResID is zero, just convert.
// If resid is nonzero and validate is true, check the input name and return an error
// 		if the name does NOT have the suffix.
// If resid is nonzero and validate is false, append the suffix to the input name
//-----------------------------------
{
	if (!inName)
		return bdNamErr;
#ifdef DEBUG
	if (XP_STRCHR(inName, ':'))
		XP_ASSERT(0);	// Are they passing us a Mac path? 
#endif

	if(validate & stripValFlag) {
		char *dot = XP_STRRCHR(inName, '.');
		if(dot) 
		{
			*dot = '\0';
		}
	}

	OSErr err = CFileMgr::FSSpecFromLocalUnixPath(inName, outSpec);
	if (err && err != fnfErr) 	/* fnfErr is OK */
		return err;
	if (validate & validateValFlag)
	{
		if (suffix)
		{
			CStr255 extension = suffix;
			SInt32 suffixPosition = (XP_STRLEN(inName) - XP_STRLEN(extension));
			if (suffixPosition < 0)
				return bdNamErr;
			if (XP_STRCASECMP(extension, inName + suffixPosition) != 0)
				return bdNamErr;
		}
	}
	if(validate & appendValFlag)
	{
		if (suffix)
		{
			CStr31 extension = suffix;
			CStr63& specName = *(CStr63*)(outSpec->name);
			/* If the total length is greater than 31, we want to 
			 * truncate the filename on the right-hand side by the 
			 * appropriate amount.
			 */
			if(specName.Length() + extension.Length() > 31)
			{
				specName.Length() = 31 - extension.Length();
			}
			specName += extension;
		} else {
			XP_ASSERT(0);
		}
	}
	
	return noErr;
} // ConvertURLToSpec

const char* CacheFilePrefix = "cache";

extern long	_fcreator, _ftype;	/* Creator and Type for files created by the open function */

//-----------------------------------
OSErr XP_FileSpec(const char *inName, XP_FileType type, FSSpec* outSpec)
// For the Mac, get the Mac specs first
//-----------------------------------
{
	CStr255 tempName(inName);
	OSErr err = noErr;
	_fcreator = emSignature;
	switch (type)	{
/* MAIN FOLDER */
		case xpUserPrefs:	/* Ignored by MacFE */
			err = fnfErr;
			break;
		case xpGlobalHistory:	/* Simple */
			_ftype = emGlobalHistory;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			GetIndString(outSpec->name, 300, globHistoryName);
			break;
		case xpImapRootDirectory:
			_ftype = emMailSubdirectory;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			*(CStr63*)(outSpec->name) = "\pIMAPmail";
			break;
		case xpImapServerDirectory:
			_ftype = emMailSubdirectory;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			*(CStr63*)(outSpec->name) = "\pIMAPmail";
			
			CInfoPBRec infoRecord;
			XP_MEMSET(&infoRecord, 0, sizeof(infoRecord));
			infoRecord.dirInfo.ioDrDirID = 0;
			infoRecord.dirInfo.ioVRefNum = outSpec->vRefNum;
			infoRecord.dirInfo.ioNamePtr = (unsigned char *) CFileMgr::PathNameFromFSSpec(*outSpec, TRUE); 
			if (infoRecord.dirInfo.ioNamePtr)
			{
				c2pstr((char *) infoRecord.dirInfo.ioNamePtr);
			
				OSErr getInfoErr = PBGetCatInfoSync(&infoRecord);
				if (getInfoErr == noErr)
				{
					/* any error here is handled by the caller */
					/* fnfErr is in fact ok for XP_Stat */
					FSMakeFSSpec(
						outSpec->vRefNum,
						infoRecord.dirInfo.ioDrDirID,
						tempName,
						outSpec);
				}
			}
			break;
		case xpBookmarks:
			_ftype = emTextType;
			err = ConvertURLToSpec(inName, outSpec, (ResIDT)0);
			break;
/* MISC */
		case xpTemporary:
			_ftype = emTextType;
			XP_ASSERT(tempName[0] != '/');
			*outSpec = CPrefs::GetTempFilePrototype();
			*(CStr63*)(outSpec->name) = tempName;
			break;
		case xpFileToPost:
			_ftype = emTextType;
			err = ConvertURLToSpec(inName, outSpec, (ResIDT)0);
			break;
		case xpURL:
		case xpExtCache:
			// This does not work if file name contains %. 
			// The problem is that netlib hex-decodes the file names before
			// passing them to XP_File, while other libraries do not
			// Windows has the same bug. atotic
			_ftype = emTextType;				
			err = CFileMgr::FSSpecFromLocalUnixPath( inName, outSpec);
			break;
		case xpMimeTypes:
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			GetIndString(outSpec->name, 300, mimeTypes);
			break;
		case xpHTTPCookie:
			_ftype = emMagicCookie;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			GetIndString(outSpec->name, 300, magicCookie);
			break;
		case xpJSCookieFilters:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			*(CStr63*)(outSpec->name) = ":cookies.js";	
			break;
		case xpProxyConfig:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			GetIndString(outSpec->name, 300, proxyConfig);
			break;
		case xpSocksConfig:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			GetIndString(outSpec->name, 300, socksConfig);
			break;		
		case xpSignature:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFolderSpec( CPrefs::SignatureFile );
			break;
		case xpHTMLAddressBook:
			_ftype = emTextType; // irrelevant
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			GetIndString(outSpec->name, 300, htmlAddressBook);
			break;
		case xpAddrBookNew:
		case xpAddrBook:
			_ftype = emAddressBookDB;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			*(CStr63*)(outSpec->name) = tempName;	
			break;		
		case xpRegistry:
			_ftype = emRegistry;
			err = FindFolder( kOnSystemDisk, kPreferencesFolderType, TRUE, &outSpec->vRefNum, &outSpec->parID);
			GetIndString(outSpec->name, 300, theRegistry);
			break;		

/* SECURITY */
		case xpKeyChain:	/* Should probably be the default name instead */
			_ftype = emKeyChain;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::SecurityFolder );
			*(CStr63*)(outSpec->name) = tempName;
			break;
		case xpCertDB:
			_ftype = emCertificates;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::SecurityFolder );
			GetIndString(tempName, 300, certDB);
			if (inName != NULL)
				tempName += inName;
			*(CStr63*)(outSpec->name) = tempName;	
			break;
		case xpCertDBNameIDX:
			_ftype = emCertIndex;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::SecurityFolder );
			GetIndString(outSpec->name, 300, certDBNameIDX);
			break;	
		case xpKeyDB:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::SecurityFolder );
			GetIndString(tempName, 300, keyDb);
			if (inName != NULL)
				tempName += inName;
			*(CStr63*)(outSpec->name) = tempName;	
			break;	
		case xpSecModuleDB:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::SecurityFolder );
			GetIndString(tempName, 300, secModuleDb);
			break;	
		case xpSignedAppletDB:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::SecurityFolder );
			GetIndString(tempName, 300, signedAppletDb);
			if (inName != NULL)
				tempName += inName;
			*(CStr63*)(outSpec->name) = tempName;	
			break;
		case xpCryptoPolicy:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::RequiredGutsFolder);
			GetIndString(outSpec->name, 300, cryptoPolicy);
			break;	
/* CACHE */
		case xpCache:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::DiskCacheFolder );
			*(CStr63*)(outSpec->name) = tempName;
			break;
		case xpCacheFAT:
			_ftype = emCacheFat;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::DiskCacheFolder );
			GetIndString(outSpec->name, 300, cacheLog);
			break;
		case xpExtCacheIndex:
			_ftype = emExtCache;			
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			GetIndString(outSpec->name, 300, extCacheFile);
			break;	
		case xpSARCache:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::SARCacheFolder );
			*(CStr63*)(outSpec->name) = tempName;
			break;
		case xpSARCacheIndex:
			_ftype = emExtCache;			
			*outSpec = CPrefs::GetFilePrototype( CPrefs::SARCacheFolder );
			GetIndString(outSpec->name, 300, sarCacheIndex);
			break;	
/* NEWS */
		case xpNewsRC:
		case xpSNewsRC:
			_ftype = emNewsrcFile;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::NewsFolder );
			if ( (inName == NULL) || (*inName == 0))
			{
				GetIndString( tempName, 300, newsrc );
				/* NET_RegisterNewsrcFile( tempName, inName, type == xpSNewsRC, FALSE ); */
			}
			else
			{
				tempName = NET_MapNewsrcHostToFilename( (char*)inName, type == xpSNewsRC, FALSE );
				if ( tempName.IsEmpty() )
				{
					// 3.0 -> 4.02 conversion hack:
					// The missing newsrc file from 3.0 probably is "newsrc"
					// If we're looking for a file that's not there, try it with "newsrc".
					// That's because if "foo" is the default host for 3.0, then these things
					// are true:
					// (1) When 4.0 is first launched, there is no is no entry in the host
					//		NewsFAT for foo, and hence no entry in the mapping table for foo.
					// (2) The entry that SHOULD be in the table for foo is "newsrc".
					// (3) The entry that 4.0 WOULD make, if the user hadn't run 3.0, is
					//		newsrc-foo.
					tempName = "newsrc";
					*(CStr63*)(outSpec->name) = tempName;
					FInfo fndrInfo;
					if (::FSpGetFInfo(outSpec, &fndrInfo) == fnfErr)
					{
						char * cstr = CStr255(inName);			// when we have <host:port>,
						char* macptr = XP_STRCHR(cstr, ':');	// ...replace ':' with '.'
						if (macptr) *macptr = '.';
	
						CFileMgr::UniqueFileSpec( *outSpec, cstr, *outSpec );
						tempName = outSpec->name;
					}
					NET_RegisterNewsrcFile( tempName, (char*)inName, type == xpSNewsRC, FALSE );
				}
			}
			*(CStr63*)(outSpec->name) = tempName;
			break;
		case xpNewsgroups:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::NewsFolder );
			tempName = NET_MapNewsrcHostToFilename((char*)inName, FALSE, TRUE );
			if ( tempName.IsEmpty() )
			{
				CStr63 newName( "groups-" );
				newName += inName;
				tempName = newName;
				CFileMgr::UniqueFileSpec( *outSpec, CStr31( tempName ), *outSpec );
				tempName = outSpec->name;
				NET_RegisterNewsrcFile( tempName, (char*)inName, FALSE, TRUE );
			}
			*(CStr63*)(outSpec->name) = tempName;
			break;
		case xpSNewsgroups:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::NewsFolder );
			tempName = NET_MapNewsrcHostToFilename((char*)inName, TRUE, TRUE );
			if ( tempName.IsEmpty() )
			{
				CStr63 newName( "sgroups-" );
				newName += inName;
				tempName = newName;
				CFileMgr::UniqueFileSpec( *outSpec, CStr31( tempName ), *outSpec );
				tempName = outSpec->name;
				NET_RegisterNewsrcFile( tempName, (char*)inName, TRUE, TRUE );
			}
			*(CStr63*)(outSpec->name) = tempName;
			break;
		case xpTemporaryNewsRC:
			_ftype = emNewsrcFile;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::NewsFolder );
			tempName = "temp newsrc";
			*(CStr63*)(outSpec->name) = tempName;
			break;
		case xpNewsrcFileMap:
			_ftype = emNewsrcDatabase;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::NewsFolder );
			GetIndString(outSpec->name, 300, newsFileMap);
			break;
		case xpXoverCache:
			_ftype = emNewsXoverCache;
			*outSpec = CPrefs::GetFilePrototype(CPrefs::NewsFolder);
//			Boolean isDirectory;
//			long dirID;
//			// Convert the outSpec of the news folder to a dir ID
//			err = FSpGetDirectoryID(outSpec, &dirID, &isDirectory);
			if (!err)
			{
//				Assert_(isDirectory);
//				outSpec->parID = dirID;
				// Name can contain a slash, so be prepared

				err = CFileMgr::FSSpecFromLocalUnixPath(inName, outSpec);

//				// name is now a native partial path
//				err = FSMakeFSSpec(
//						outSpec->vRefNum,
//						outSpec->parID,
//						tempName,
//						outSpec);
				if (err == fnfErr) err = noErr;
			}
			break;		
		case xpNewsSort:
			_ftype = emMailFilterRules;
			*outSpec = CPrefs::GetFilePrototype(CPrefs::NewsFolder);
			err = CFileMgr::FSSpecFromLocalUnixPath(inName, outSpec);
			if (err == fnfErr) err = noErr;
			break;
		case xpNewsHostDatabase:
			_ftype = emNewsHostDatabase;
			*outSpec = CPrefs::GetFilePrototype(CPrefs::NewsFolder);
			GetIndString(outSpec->name, 300, newsHostDatabase);
			break;

/* MAIL */
		case xpJSMailFilters:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype(CPrefs::MailFolder);
			if (inName && *inName)
			{
				// Make sure there are no colons in the server name
				char* cp = (char*)&tempName[1];
				for (int i = 1; i < tempName[0]; i++,cp++)
					if (*cp == ':') *cp = '.';
				*(CStr63*)(outSpec->name) += (":" + tempName + ".filters.js"); // in parent of mail folder.
			}
			else
				*(CStr63*)(outSpec->name) = ":filters.js"; // in parent of mail folder.	
			break;
		case xpMailSort: // normal filters
		{
			_ftype = emMailFilterRules;
			*outSpec = CPrefs::GetFilePrototype(CPrefs::MailFolder);
			if (inName && *inName)
			{
				// In 5.0, there are rules for each server.
				// Make sure there are no colons in the server name
				char* cp = (char*)&tempName[1];
				for (int i = 1; i < tempName[0]; i++,cp++)
					if (*cp == ':') *cp = '.';
				*(CStr63*)(outSpec->name) = (":" + tempName + " Rules"); // in parent of mail folder.
			}
			else
				GetIndString(outSpec->name, 300, mailFilterFile);// in parent of mail folder.
			break;
		}
		case xpMailFilterLog:
			_ftype = emMailFilterLog;
			*outSpec = CPrefs::GetFilePrototype(CPrefs::MailFolder);
			GetIndString(outSpec->name, 300, mailFilterLog);// in parent of mail folder.
			break;
		case xpMailFolder:
			_ftype = emTextType;
			err = ConvertURLToSpec(inName, outSpec, (ResIDT)0);
			break;
		case xpMailFolderSummary:
			_ftype = emMailFolderSummary;
			err = ConvertURLToSpec(inName, outSpec, mailBoxExt);
			break;
		case xpMailPopState:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype(CPrefs::MainFolder);
			GetIndString(outSpec->name, 300, mailPopState);
			break;
	    case xpMailSubdirectory:
			_ftype = emMailSubdirectory;
			err = ConvertURLToSpec(inName, outSpec, subdirExt);
			break;
	    case xpVCardFile:
			_ftype = emTextType;
			// We are required to check for a '.vcf'
			err = ConvertURLToSpec(inName, outSpec, vCardExt, true);
			break;
	    case xpLDIFFile:
			_ftype = emTextType;
			// We are required to check for a '.ldi', '.ldif'
			err = ConvertURLToSpec(inName, outSpec, ldifExt1, true);
			if (err == bdNamErr)
				err = ConvertURLToSpec(inName, outSpec, ldifExt2, true);
			if ( err == bdNamErr )
			{
				ConvertURLToSpec(inName, outSpec, 0, false);
				short refNum;
				err = ::FSpOpenDF( outSpec,  fsRdPerm, &refNum);
				FSClose( refNum );
				if (err == fnfErr)
					err = ConvertURLToSpec(inName, outSpec, ".ldif", stripAndAppendValFlag);
				else 
					err = bdNamErr;
			}
			break;
	    case xpPKCS12File:
			_ftype = emTextType;
			// We are required to check for a '.ldi', '.ldif'
			err = ConvertURLToSpec(inName, outSpec, ".p12", stripAndAppendValFlag);
			break;
		case xpFolderCache:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype(CPrefs::MainFolder);
			GetIndString(outSpec->name, 300, mailFolderCache);
			break;
		case xpLIClientDB:
			_ftype = emTextType;
			*outSpec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
			*(CStr63*)(outSpec->name) = "LIClientdb.dat";
			break;
		default:
			XP_ASSERT( false );		// Whoever added the enum, it is time to implement it on the Mac
			err = bdNamErr;
			break;
	}
	if ( (err == fnfErr)  )
		err = noErr;
	//XP_ASSERT( err == noErr);
	// Don't assert, sometimes normal flow of control involves an err here.
	return err;
} // XP_FileSpec

//-----------------------------------
char* xp_FileName( const char* name, XP_FileType type, char* buf, char* /* unixConfigBuf */ )
//-----------------------------------
{
	FSSpec		macSpec;
	OSErr		err = noErr;
	
	if ( ( type == xpCache ) && name)
	{
		char * cachePath = CPrefs::GetCachePath();
		if (cachePath == NULL)
			err = fnfErr;
		else
		{
			XP_STRCPY(buf, cachePath);
			XP_STRCAT(buf, name);
		}
	}

#ifdef _XP_TMP_FILENAME_FOR_LDAP_
	/* this is for address book to have a way of getting a temp file
	 * name for each ldap server, yet preserve the temp file name for
	 * later use on the same ldap server.  we expect name to be unique
	 * to each ldap server and the return would be a temp file name 
	 * associated with the ldap server - benjie
	 */
	else if ( ( type == xpAddrBook ) && name)
	{
		return FE_GetFileName(name, type);
	}
#endif

	else
	{
		char*		path = NULL;
	
		err = XP_FileSpec( name, type, &macSpec );

		if (err != noErr)
			return NULL;
			
		path = CFileMgr::PathNameFromFSSpec( macSpec, TRUE );
		
		if ( !path )
			return NULL;

		if ((strlen(path) > 1000) )
		{
			XP_ASSERT(FALSE);
			XP_FREE( path );
			return NULL;
		}

		XP_STRCPY( buf, path );
		XP_FREE( path );
	}
	if ( err != noErr )
		return NULL;
	return buf;
} // XP_FileSpec

//-----------------------------------
char* XP_TempDirName(void)
//-----------------------------------
{
	FSSpec spec = CPrefs::GetTempFilePrototype();
	char* path = CFileMgr::PathNameFromFSSpec(spec, TRUE);
	if (strlen(path) > 1000)
	{
		XP_ASSERT(FALSE);
		return NULL;
	}
	return path;
}

//-----------------------------------
char * xp_FilePlatformName(const char * name, char* path)
//-----------------------------------
{
	FSSpec spec;
	OSErr err;
	char * fullPath;

	if (name == NULL)
		return NULL;

	// Initialize spec, because FSSpecFromLocalUnixPath might use it.
	spec.vRefNum = 0;
	spec.parID = 0;
	err = CFileMgr::FSSpecFromLocalUnixPath(name, &spec);
	// ¥¥¥NOTE: rjc, pchen, and pinkerton changed the behavior of this so that it will no
	// longer return NULL when the file does not exist. If this is a problem, come tell us
	// because we need to change other code.
	if (err != noErr && err != fnfErr && err != dirNFErr )
		return NULL;
	fullPath = CFileMgr::PathNameFromFSSpec( spec, TRUE );
	if (fullPath && (XP_STRLEN(fullPath) < 300))
		XP_STRCPY(path, fullPath);
	else
		return NULL;
	if (fullPath)
		XP_FREE(fullPath);
	return path;
}


//-----------------------------------
char *XP_PlatformFileToURL(const char * inName)
//-----------------------------------
{
	char *retVal = NULL;
	const char *prefix = "file://";
	/* The inName parameter MUST be a native path, ie colons, spaces, etc.
	** This assert will help us fix all the various "doubly-encoded path"
	** bugs  - jrm 97/02/10 */
#ifdef DEBUG
	FSSpec tempSpec;
	OSErr err = ::FSMakeFSSpec(0, 0, CStr255(inName), &tempSpec);
	Boolean nativeMacPath = (err == noErr || err == fnfErr || err == dirNFErr);
	Assert_(nativeMacPath);
#endif
	char *duplicatedName = XP_STRDUP(inName);
	if (duplicatedName)
	{		/* YUCK! duplicatedName  deleted by CFileMgr::EncodeMacPath */
		char *xp_path = CFileMgr::EncodeMacPath(duplicatedName);	
		retVal = (char *) XP_ALLOC (XP_STRLEN(xp_path) + XP_STRLEN(prefix) + 1);
		if (retVal)
		{
			XP_STRCPY (retVal, prefix);
			XP_STRCAT (retVal, xp_path);
		}
		if (xp_path)
			XP_FREE(xp_path);
	}
	return retVal;
}

char *XP_PlatformPartialPathToXPPartialPath(const char *sourceStringWithSpaces)
{
	// convert spaces back to %20 quote char
	int numberOfSpaces = 0;
	const char *currentSpace   = XP_STRSTR(sourceStringWithSpaces, " ");
	while (currentSpace)
	{
		numberOfSpaces++;
		currentSpace = XP_STRSTR(currentSpace + 1, " ");
	}
	
	char *escapedReturnString = (char *) XP_ALLOC(XP_STRLEN(sourceStringWithSpaces) + (numberOfSpaces*2) + 1);
	if (escapedReturnString)
	{
		XP_STRCPY(escapedReturnString, sourceStringWithSpaces);
		if (numberOfSpaces)
		{
	    	char *currentSpace   = XP_STRSTR(escapedReturnString, " ");
	    	while (currentSpace)
	    	{
	    		XP_MEMMOVE(currentSpace+3,currentSpace+1,XP_STRLEN(currentSpace+1) + 1);
	    		*currentSpace++ = '%';
	    		*currentSpace++ = '2';
	    		*currentSpace++ = '0';
	    		currentSpace    = XP_STRSTR(currentSpace, " ");
	    	}
		}
	}
	
	return escapedReturnString;
}

/* Needs to deal with both CR, CRLF, and LF end-of-line */
extern char * XP_FileReadLine(char * dest, int32 bufferSize, XP_File file)
{
	char * retBuf = fgets(dest, bufferSize, file);
	if (retBuf == NULL)	/* EOF */
		return NULL;
	char * LFoccurence = (char *)strchr(retBuf, LF);
	if (LFoccurence != NULL)	/* We have hit LF before CR,  */
	{
		fseek(file, -(strlen(retBuf) - (LFoccurence  - retBuf))+1, SEEK_CUR);
		LFoccurence[1] = 0;
	}
	else	/* We have CR, check if the next character is LF */
	{
		int c;

		c = fgetc(file);

		if (c == EOF)
			;	/* Do nothing, end of file */
		else if (c == LF)	/* Swallow CR/LF */
		{
			int len = strlen(retBuf);
			if (len < bufferSize)	/* Append LF to our buffer if we can */
			{
				retBuf[len++] = LF;
				retBuf[len] = 0;
			}
		}
		else	/* No LF, just clean up the seek */
		{
			fseek(file, -1, SEEK_CUR);
		}
	}
	return retBuf;
}

static counter = 1;	/* temporary name suffix */

/* Returns a temp name given a path
 */
char * xp_TempName(XP_FileType type, const char * prefix, char* buf, char* /* buf2 */, unsigned int * /* count */)
{
	FSSpec tempSpec;
	CStr255 defaultName(prefix);
	OSErr err = noErr;

	switch (type)	{
		case xpTemporaryNewsRC:
			defaultName = "temp newsrc";
			break;
			break;
		case xpCache:
			defaultName = CacheFilePrefix;
			CStr255 dateString;
			::NumToString (::TickCount(), dateString);
			defaultName += dateString;
			break;
		case xpBookmarks:
			defaultName += ".bak";
			break;
		case xpJPEGFile:
			defaultName += ".jpeg";
			break;
		case xpMailFolder:	/* Ugly. MailFolder callers for temporary names pass in the full path */
		case xpMailFolderSummary:	/* Ugly. MailFolder callers for temporary names pass in the full path */
			{
				FSSpec temp;
				err = XP_FileSpec(prefix, xpMailFolder, &temp);
				if (err == noErr)
					defaultName = temp.name;
			}
		case xpFileToPost:	/* Temporary files to post return the full path */
			
		case xpTemporary:
		default:
			if ( (type == xpURL) && prefix ) 
			{
				if ( XP_STRRCHR(prefix, '/') )
				{
					XP_StatStruct st;
					if (XP_Stat (defaultName, &st, xpURL))
						XP_MakeDirectoryR (defaultName, xpURL);
					defaultName += "su";
				}
			}

			if (defaultName.IsEmpty())
				defaultName = "nstemp";
			CStr255 counterS;
			::NumToString(counter++, counterS);
			defaultName += counterS;	/* Need counter to guarantee uniqueness if called several times in a row */
			break;
	}
	if (type == xpFileToPost || type == xpMailFolder || type == xpMailFolderSummary || ((type == xpURL) && !prefix)) 
	{
		if (defaultName.Length() > 30)	// Someone has passed in something weird as a prefix
		{
			XP_ASSERT(false);
			defaultName.Delete(30, 1000);
		}
		err = XP_FileSpec(defaultName, xpTemporary, &tempSpec);
	}
	else
		err = XP_FileSpec(defaultName, type, &tempSpec);

	if (err && err != fnfErr)
		return NULL;

	FSSpec finalSpec;
	err = CFileMgr::UniqueFileSpec(tempSpec, tempSpec.name, finalSpec);
	if (err != noErr)
	{
		XP_ASSERT(FALSE);
		return NULL;
	}
	if ((type == xpFileToPost) || 
		(type == xpBookmarks) ||
		(type == xpMailFolder) || 
		(type == xpURL) ||
		(type == xpMailFolderSummary))
	/* These files needs full pathname */
	{
		char* tempPath = CFileMgr::PathNameFromFSSpec( finalSpec, TRUE );
		tempPath = CFileMgr::EncodeMacPath(tempPath);
		if (tempPath == NULL)
			return NULL;
		else if (XP_STRLEN(tempPath) > 500)	/* Buffer overflow check */
		{
			XP_FREE(tempPath);
			return NULL;
		}
			XP_STRCPY(buf, tempPath);
		XP_FREE(tempPath);
	}
	else
		XP_STRCPY(buf, CStr63(finalSpec.name));
	return buf;
} /* xp_TempName */



/* XP_OpenDir is not handling different directories yet. */
XP_Dir XP_OpenDir( const char* name, XP_FileType type )
{
	CInfoPBRec		pb;
	OSErr			err;
	DirInfo*		dipb;
	struct dirstruct *		dir = XP_NEW(struct dirstruct);

	if (dir == NULL)
		return NULL;

	XP_TRACE(( "opening file: %s", name ));
	dir->type = type;
	
	err = XP_FileSpec( name, type, &dir->dirSpecs );
	if ( err != noErr )
	{
		XP_DELETE(dir);
		return NULL;
	}

	dipb = (DirInfo*)&pb;

	pb.hFileInfo.ioNamePtr = dir->dirSpecs.name;
	pb.hFileInfo.ioVRefNum = dir->dirSpecs.vRefNum;
	pb.hFileInfo.ioDirID = dir->dirSpecs.parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */

	err = PBGetCatInfoSync( &pb );

	/* test that we have gotten a directory back, not a file */
	if ( (err != noErr ) || !( dipb->ioFlAttrib & 0x0010 ) )
	{
		XP_DELETE( dir );
		return NULL;
	}
	dir->dirSpecs.parID = pb.hFileInfo.ioDirID;
	dir->index = pb.dirInfo.ioDrNmFls;

	return (XP_Dir)dir;
}

void XP_CloseDir( XP_Dir dir )
{
	if ( dir )
		XP_DELETE(dir);
}

int XP_FileNumberOfFilesInDirectory(const char * dir_name, XP_FileType type)
{
	FSSpec spec;
	OSErr err = XP_FileSpec(dir_name, type, &spec);
	if ((err != noErr) && (err != fnfErr))
		goto loser;
	CInfoPBRec pb;
	pb.hFileInfo.ioNamePtr = spec.name;
	pb.hFileInfo.ioVRefNum = spec.vRefNum;
	pb.hFileInfo.ioDirID = spec.parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	err = PBGetCatInfoSync(&pb);
	if (err != noErr)
		goto loser;
	return pb.dirInfo.ioDrNmFls;
loser:
	return 10000;	/* ???? */
}

XP_DirEntryStruct * XP_ReadDir(XP_Dir dir)
{
tryagain:
	if (dir->index <= 0)
		return NULL;
	CInfoPBRec cipb;
	DirInfo	*dipb=(DirInfo *)&cipb;
	dipb->ioCompletion = NULL;
	dipb->ioFDirIndex = dir->index;
	dipb->ioVRefNum = dir->dirSpecs.vRefNum; /* Might need to use vRefNum, not sure*/
	dipb->ioDrDirID = dir->dirSpecs.parID;
	dipb->ioNamePtr = (StringPtr)&dir->dirent.d_name;
	OSErr err = PBGetCatInfoSync (&cipb);
	/* We are traversing the directory backwards */
	if (err != noErr)
	{
		if (dir->index > 1)
		{
			dir->index--;
			goto tryagain;
		}
		else
			return NULL;
	}
	p2cstr((StringPtr)&dir->dirent.d_name);

	/* Mail folders are treated in a special way */

	if ((dir->type == xpMailFolder) ||
		(dir->type == xpMailFolderSummary) ||
		(dir->type == xpMailSubdirectory) ) 
	{
		char * newName = XP_STRDUP(dir->dirent.d_name);
		newName = CFileMgr::EncodeMacPath(newName);
		if (newName)
		{
			XP_STRCPY(dir->dirent.d_name, newName);
			XP_FREE(newName);
		}
	}

	dir->index--;
	return &dir->dirent;
}

int XP_MakeDirectory(const char* name, XP_FileType type)
{
	FSSpec spec;
	OSErr err = XP_FileSpec(name, type, &spec);
	if ((err != noErr) && (err != fnfErr))	/* Should not happen */
		return -1;
	short refNum;
	long dirID;
	err = CFileMgr::CreateFolderInFolder(
				spec.vRefNum, spec.parID, spec.name, /* Name of the new folder */
				&refNum, &dirID);
	if (err != noErr)
		return -1;
	return 0;
}

/* Recursively create all the directories
 */
int XP_MakeDirectoryR(const char* name, XP_FileType type)
{
	char separator;
	int result = 0;
	XP_FILE_NATIVE_PATH finalNameNative = NULL;
	XP_FILE_URL_PATH finalName = NULL;
	char * dirPath = NULL;
	separator = '/';

	if ( type == xpURL)
		finalNameNative = CFileMgr::MacPathFromUnixPath(name);
	else
		finalNameNative = WH_FileName(name, type);
	
	if ( finalNameNative == NULL )
	{
		result  = -1;
		goto done;
	}
	finalName = XP_PlatformFileToURL(finalNameNative);
	if ( finalName )
	{
		char * currentEnd;
		int err = 0;
		XP_StatStruct s;
		dirPath = XP_STRDUP( &finalName[7] );	// Skip the file://
		if (dirPath == NULL)
		{
			result = -1;
			goto done;
		}

		currentEnd = XP_STRCHR(dirPath, separator);
		if (currentEnd)
			currentEnd = XP_STRCHR(&currentEnd[1], separator);
		/* Loop through every part of the directory path */
		while (currentEnd != 0)
		{
			char savedChar;
			savedChar = currentEnd[1];
			currentEnd[1] = 0;
			if ( XP_Stat(dirPath, &s, xpURL ) != 0)
				err = XP_MakeDirectory(dirPath, xpURL);
			if ( err != 0)
			{
				XP_ASSERT( FALSE );	/* Could not create the directory? */
				break;
			}
			currentEnd[1] = savedChar;
			currentEnd = XP_STRCHR( &currentEnd[1], separator);
		}
		if ( err == 0 )
		/* If the path is not terminated with / */
		{
			if ( dirPath[XP_STRLEN( dirPath) - 1] != separator )
				if ( XP_Stat(dirPath, &s, xpURL ) != 0)
					err = XP_MakeDirectory(dirPath, xpURL);
		}
		if ( 0 != err )
			result = err;
	}
	else
		result = -1;
done:
	XP_FREEIF(finalName);
	XP_FREEIF(finalNameNative);
	XP_FREEIF(dirPath);
	XP_ASSERT( result == 0 );	/* For debugging only */
	return result;
}

int XP_RemoveDirectory (const char* name, XP_FileType type)
{
	FSSpec spec;
	OSErr err = XP_FileSpec(name, type, &spec);
	if ((err != noErr) && (err != fnfErr))
		return -1;
	err = ::FSpDelete(&spec);
	if (err != noErr)
		return -1;
	return 0;
}

// Removes the directory and its contents.
int XP_RemoveDirectoryRecursive(const char *name, XP_FileType type)
{
    OSErr err;
    FSSpec dirSpec;
    
    err = XP_FileSpec(name, type, &dirSpec);
    if ((err != noErr) && (err != fnfErr))
            return -1;
    err = DeleteDirectory(dirSpec.vRefNum, dirSpec.parID, dirSpec.name);
    return (err == noErr) ? 0 : -1;
}

int XP_FileRename( const char* from, XP_FileType fromtype,
				   const char* to, XP_FileType totype )
{
	OSErr err = noErr;
	
	char* fromName = WH_FileName( from, fromtype );
	char* toName = WH_FileName( to, totype );
	
	if ( fromName && toName )
	{
		FSSpec toSpec;
		err = CFileMgr::FSSpecFromPathname( toName, &toSpec );
		if (err == noErr || err == fnfErr)
		{
			FSSpec fromSpec;
			err = CFileMgr::FSSpecFromPathname( fromName, &fromSpec );
			if (err == noErr || err == fnfErr)
			{
				
				if (fromSpec.vRefNum == toSpec.vRefNum)	
				/* Same volume */
				{
					/* But, er, is it the same file (jrm 97/08/20)? */
					if (fromSpec.parID == toSpec.parID
					&& *(CStr63*)fromSpec.name == *(CStr63*)toSpec.name)
						goto Cleanup; // YOW!  Don't delete it!
	
					/* Delete the file */
					::FSpDelete( &toSpec ); /* ignore error if not there */
					if (fromSpec.parID == toSpec.parID)
					{
						/* this is a flat node rename use ::FSpRename */
						err = ::FSpRename(&fromSpec, toSpec.name);
					}
					else
					{
						/* we are moving a file between dirs so use HMoveRenameCompat */
						err = ::HMoveRenameCompat(
							fromSpec.vRefNum,
							fromSpec.parID, (ConstStr255Param)fromSpec.name,
							toSpec.parID, nil, (StringPtr)toSpec.name);
					}
				}
				else	
				/* Different volumes */
				{
					FSSpec toDir;
					err = CFileMgr::FolderSpecFromFolderID(toSpec.vRefNum, toSpec.parID, toDir);
					if (err == noErr)
					{					
						::FSpDelete( &toSpec );
						err = FSpFileCopy( &fromSpec, &toDir, toSpec.name, NULL, 0, true );
						if ( err == noErr)
							::FSpDelete( &fromSpec );
					}
				}
			}
		}
			
	}
	else
		err = -1;
Cleanup:
	if ( fromName )
		XP_FREE( fromName );
	if ( toName )
		XP_FREE( toName );
	if (err != noErr)
		return -1;
	return 0;
}

int XP_FileRemove( const char* name, XP_FileType type )
{
	FSSpec		spec;
	OSErr		err;

	err = XP_FileSpec( name, type, &spec);

	if ( err == noErr )
	{
		err = ::FSpDelete( &spec );
		if ( type == xpNewsRC || type == xpNewsgroups )
	        NET_UnregisterNewsHost( name, FALSE );
		else if ( type == xpSNewsRC || type == xpSNewsgroups)
	        NET_UnregisterNewsHost( name, TRUE );
	}
	
	if ( err == noErr )
		return 0;
	return -1;
}

int XP_FileTruncate( const char* name, XP_FileType type, int32 len )
{
	FSSpec		spec;
	OSErr		err, err2;
	short refNum;

	if (len < 0)
		return EINVAL;

	err = XP_FileSpec( name, type, &spec);
	if (err != noErr)
		return EINVAL;

	err = FSpOpenDF(&spec, fsRdWrPerm, &refNum);
	if (err != noErr)
		return EACCES;
	
	err = SetEOF(refNum, len);
	
	err2 = FSClose(refNum);
	if ((err != noErr) || (err2 != noErr))
		return EIO;
	return 0;
}


int XP_FileDuplicateResourceFork( const char* oldFilePath, XP_FileType oldType,
									const char* newFilePath, XP_FileType newType )
{
	OSErr err;
	FSSpec oldfs, newfs;
	err = XP_FileSpec( oldFilePath, oldType, &oldfs);
	if (err != noErr)
		return EINVAL;

	err = XP_FileSpec( newFilePath, newType, &newfs);
	if (err != noErr)
		return EINVAL;

	short	oldFileRefNum = -1, newFileRefNum = -1;
	oldFileRefNum = FSpOpenResFileCompat( &oldfs, fsCurPerm );
	err = ResError();
	if ( err  || ( oldFileRefNum == -1 ) )
	{
		return EACCES;
	}
	
	// assume we need to create the new resource fork 
	// (may not be valid assumption for other uses of this function)
	FSpCreateResFileCompat( &newfs, emSignature, emTextType, smSystemScript );
	err = ResError();
	
	newFileRefNum = FSpOpenResFileCompat( &newfs, fsRdWrPerm );
	err = ResError();
	if ( err || ( newFileRefNum == -1 ) )
	{
		err = FSClose( oldFileRefNum );
		XP_ASSERT(0);
		return EACCES;
	}
	
	long buffsize = 1024;
	char buff[ 1024 ];
	err = CopyFork( oldFileRefNum, newFileRefNum, buff, buffsize);

	FSClose( oldFileRefNum );
	FSClose( newFileRefNum );
	
	if ( err )
		return EIO;
	
	return 0;
}

//======================================
// I put these stubs here.  They can go away when we move to MSL.
//======================================

PR_EXTERN(char*) PR_GetEnv(const char *var);
extern "C" {
	char* PR_GetEnv(const char *var);
	char* getenv(const char *var);
}

char* getenv(const char *var)
{
	return PR_GetEnv(var);
}

