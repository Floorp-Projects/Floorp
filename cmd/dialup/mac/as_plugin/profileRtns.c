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

#include "pluginIncludes.h"

extern OSErr getProfileDirectory( FSSpecPtr theFSSpecPtr )
{
	AliasHandle		aliasH;
	Boolean			wasChanged;
	CInfoPBRec		cBlock;
	OSErr			err = paramErr;
	short			profileID, refNum, saveResFile;
	
	if ( theFSSpecPtr )
	{
		if ( !( err = findNetscapeUserProfileDatabase( theFSSpecPtr ) ) )
		{
			saveResFile = CurResFile();
			if ( ( refNum = FSpOpenResFile( theFSSpecPtr, fsRdWrPerm ) ) != kResFileNotOpened )
			{
				UseResFile( refNum );
				if ( !( err = findCurrentUserProfileID( refNum, &profileID ) ) )
				{
					if ( aliasH = (AliasHandle)Get1Resource( rAliasType, profileID ) )
					{
						HNoPurge( (Handle)aliasH );
						if ( !( err = ResolveAlias( NULL, aliasH, theFSSpecPtr, &wasChanged ) ) )
						{
							bzero( (char*)&cBlock, sizeof( cBlock ) );
							cBlock.hFileInfo.ioCompletion = NULL;
							cBlock.hFileInfo.ioNamePtr = (StringPtr)theFSSpecPtr->name;
							cBlock.hFileInfo.ioVRefNum = theFSSpecPtr->vRefNum;
							cBlock.hFileInfo.ioDirID = theFSSpecPtr->parID;
							cBlock.hFileInfo.ioFDirIndex = 0;
							if ( !( err = PBGetCatInfoSync( &cBlock ) ) )
							{
								// XXX should verify that its a directory (i.e. not a file)

								theFSSpecPtr->vRefNum = cBlock.hFileInfo.ioVRefNum;
								theFSSpecPtr->parID = cBlock.hFileInfo.ioDirID;
								theFSSpecPtr->name[ 0 ] = 0;
							}
							else
							{
								SETUP_PLUGIN_ERROR( "\p getProfileDirectory: PBGetCatInfoSync error;g", err );
							}
						}
					}
				}
				CloseResFile( refNum );
				}
			UseResFile( saveResFile );
		}
	}
	return err;
}



extern JRI_PUBLIC_API(struct java_lang_String*) native_SetupPlugin_SECURE_0005fGetCurrentProfileDirectory(
	JRIEnv* env, struct SetupPlugin* self)
{
	AliasHandle		aliasH;
	Boolean			wasChanged;
	FSSpec			userProfileFSSpec, currentProfileFSSpec;
	Handle			h;
	OSErr			err;
	java_lang_String*	profileDir = NULL;
	short			profileID, refNum, saveResFile;

	SETUP_PLUGIN_TRACE( "\p native_SetupPlugin_GetCurrentProfileDirectory entered" );

	useCursor( watchCursor );

	if ( !( err = findNetscapeUserProfileDatabase( &userProfileFSSpec ) ) )
	{
		saveResFile = CurResFile();
		if ( ( refNum = FSpOpenResFile( &userProfileFSSpec, fsRdWrPerm ) ) != kResFileNotOpened )
		{
			UseResFile( refNum );
			if ( !( err = findCurrentUserProfileID( refNum, &profileID ) ) )
			{
				if ( aliasH = (AliasHandle)Get1Resource( rAliasType, profileID ) )
				{
					HNoPurge( (Handle)aliasH );
					if ( !( err = ResolveAlias( NULL, aliasH, &currentProfileFSSpec, &wasChanged ) ) )
					{
						if ( h = pathFromFSSpec( &currentProfileFSSpec ) )
						{
							HLock( h );
//							profileDir=JRI_NewStringUTF(env, (char *)(*h), (unsigned)GetHandleSize(h));
							profileDir = cStr2javaLangString( env, (char*)(*h), (unsigned)GetHandleSize( h ) );
							DisposeHandle( h );
						}
					}
				}
			}
			CloseResFile( refNum );
		}
		else
			SETUP_PLUGIN_ERROR( "\p GetCurrentProfileDirectory: FSpOpenResFile error;g", ResError() );
		UseResFile( saveResFile );
	}
	else
		SETUP_PLUGIN_ERROR( "\p GetCurrentProfileDirectory: findNetscapeUserProfileDatabase error;g", err );

	SETUP_PLUGIN_TRACE( "\p native_SetupPlugin_GetCurrentProfileDirectory exiting" );
	return profileDir;
}



extern JRI_PUBLIC_API(struct java_lang_String *)
native_SetupPlugin_SECURE_0005fGetCurrentProfileName(JRIEnv* env, 
				struct SetupPlugin* self)
{
		FSSpec			userProfileFSSpec;
		OSErr			err;
		StringHandle		strH;
		java_lang_String	*profileName=NULL;
		short			profileID,refNum,saveResFile;

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_GetCurrentProfileName entered");

	useCursor(watchCursor);

	if (!(err=findNetscapeUserProfileDatabase(&userProfileFSSpec)))	{
		saveResFile = CurResFile();
		if ((refNum=FSpOpenResFile(&userProfileFSSpec, fsRdWrPerm)) != kResFileNotOpened)	{
			UseResFile(refNum);
			if (!(err=findCurrentUserProfileID(refNum, &profileID)))	{
				if (strH=GetString(profileID))	{
					HLock((Handle)strH);
//					profileName=JRI_NewStringUTF(env, (char *)&((*strH)[1]), (unsigned)((*strH)[0]));
					profileName=cStr2javaLangString(env, (char *)&((*strH)[1]), (unsigned)((*strH)[0]));
					ReleaseResource((Handle)strH);
					}
				}
			CloseResFile(refNum);
			}
		UseResFile(saveResFile);
		}
	else	{
		SETUP_PLUGIN_ERROR("\p GetCurrentProfileName: findNetscapeUserProfileDatabase error;g", err);
		}

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_GetCurrentProfileName exiting");
	return(profileName);
}



extern JRI_PUBLIC_API(void)
native_SetupPlugin_SECURE_0005fSetCurrentProfileName(JRIEnv* env, 
				struct SetupPlugin* self, 
				struct java_lang_String *profileName)
{
		FSSpec			userProfileFSSpec;
		OSErr			err;
		StringHandle		strH;
		const char		*profileStr;
		short			profileID,refNum,saveResFile;
		int			len;

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_SetCurrentProfileName entered");

	if (profileName==NULL)		return;
	profileStr = javaLangString2Cstr( env, profileName );
	if (profileStr==NULL)		return;
	len=strlen(profileStr);
	if (len==0)	return;

	useCursor(watchCursor);

	if (!(err=findNetscapeUserProfileDatabase(&userProfileFSSpec)))	{
		saveResFile = CurResFile();
		if ((refNum=FSpOpenResFile(&userProfileFSSpec, fsRdWrPerm)) != kResFileNotOpened)	{
			UseResFile(refNum);
			if (!(err=findCurrentUserProfileID(refNum, &profileID)))	{
				if (strH=GetString(profileID))	{
					HNoPurge((Handle)strH);
					HUnlock((Handle)strH);
					SetHandleSize((Handle)strH,1L+len);
					if (GetHandleSize((Handle)strH) == 1L+len)	{
						HLock((Handle)strH);
						(*strH)[0] = (unsigned)len;
						BlockMove(profileStr,&(*strH)[1],len);
						ChangedResource((Handle)strH);
						WriteResource((Handle)strH);
						}
					}
				}
			CloseResFile(refNum);
			}
		UseResFile(saveResFile);
		}
	else	{
		SETUP_PLUGIN_ERROR("\p SetCurrentProfileName: findNetscapeUserProfileDatabase error;g", err);
		}

SETUP_PLUGIN_TRACE("\p native_SetupPlugin_SetCurrentProfileName exiting");
}



OSErr findNetscapeUserProfileDatabase( FSSpecPtr theFSSpecPtr )
{
	OSErr			err;
	short			vRefNum;
	long			dirID;

	if ( !( err = FindFolder( kOnSystemDisk, kPreferencesFolderType, kDontCreateFolder, &vRefNum, &dirID ) ) )
	{
		err = FSMakeFSSpec( vRefNum, dirID, USERPROFILEDATABASE_NAME_POSTB2, theFSSpecPtr );
		
		// Note: FSMakeFSSpec can return fnfErr (documented) or dirNFErr (not documented) if file/dir/dir-in-path doesn't exist
		
		if ( ( err==fnfErr ) || ( err==dirNFErr ) )
		{
			SETUP_PLUGIN_INFO_STR( "\p findNetscapeUserProfileDatabase: B2 fallback for User Profile database location", NULL );
			err = FSMakeFSSpec( vRefNum, dirID, USERPROFILEDATABASE_NAME_B2, theFSSpecPtr );
		}
	}
	return err;
}

OSErr findCurrentUserProfileID( short refNum, short* profileID )
{
	Handle			h;
	OSErr			err = paramErr;
	ResType			theType;
	Str255			resName;
	short			saveResFile, theCount;

	saveResFile = CurResFile();
	UseResFile( refNum );
	if ( h = Get1Resource( USERPROFILE_RESTYPE, USERPROFILE_RESID ) )
	{
		if ( GetHandleSize( h ) == sizeof( short ) )
		{
			BlockMove( *h, profileID, (long)sizeof( short ) );
			err = noErr;
		}
		else
		{
			SETUP_PLUGIN_ERROR( "\p findCurrentUserProfileID: user profile resource is incorrect size error;g", err );
		}
	}
	else
	{
		if ( ( theCount = Count1Resources( rAliasType ) ) == 1 )
		{
			if ( h = Get1IndResource( rAliasType, 1 ) )
			{
				HNoPurge( h );
				GetResInfo( h, profileID, &theType, resName );
				err = noErr;
			}
		}
	}
	UseResFile( saveResFile );
	return err;
}



Handle pathFromFSSpec( FSSpecPtr theFSSpecPtr )
{
	CInfoPBRec		cBlock;
	Handle			h = NULL;
	OSErr			err;
	Str255			name = { 0 };
	long			initialLen = 0L, namelen, hlen;
	
	if ( theFSSpecPtr == NULL )
		return NULL;

	// start with cBlock.dirInfo.ioDrDirID
	bzero( (char*)&cBlock, sizeof( cBlock ) );
	cBlock.dirInfo.ioCompletion = NULL;
	cBlock.dirInfo.ioNamePtr = theFSSpecPtr->name;
	cBlock.dirInfo.ioVRefNum = theFSSpecPtr->vRefNum;
	cBlock.dirInfo.ioDrDirID = theFSSpecPtr->parID;
	cBlock.dirInfo.ioFDirIndex = 0;
	if ( err = PBGetCatInfoSync( &cBlock ) )
	{
		SETUP_PLUGIN_ERROR( "\p pathFromFSSpec: PBGetCatInfoSync error;g", err );
		return NULL;
	}

	
	// ¥ the FSSpec is pointing to a file, so copy the name
	if ( !( cBlock.dirInfo.ioFlAttrib & ioDirMask ) )
	{
		SETUP_PLUGIN_INFO_STR( "\p PBGetCatInfoSync: file specified", NULL );
		initialLen = (unsigned)theFSSpecPtr->name[ 0 ] + 1;
		cBlock.dirInfo.ioDrDirID = theFSSpecPtr->parID;
	}
	// ¥Êthe FSSpec is a directory, we'll fetch the name in a sec, but copy in the
	//	following "/" first
	else
	{
		initialLen = 1;
		SETUP_PLUGIN_INFO_STR( "\p PBGetCatInfoSync: folder specified", NULL );
	}
	
	h = NewHandle( initialLen );
	if ( h == NULL )
	{
		SETUP_PLUGIN_ERROR( "\p pathFromFSSpec: NewHandle error;g", MemError() );
		return NULL;
	}

	HNoPurge( h );
	
	(*h)[ 0 ] = '/';

	if ( initialLen > 1 )
		// ¥Êcopy the file name into the handle
		BlockMove( &theFSSpecPtr->name[ 1 ], &(*h)[ 1 ],(unsigned long)( theFSSpecPtr->name[ 0 ] ) );
				

	// ¥ start with cBlock.dirInfo.ioDrDirID
	cBlock.dirInfo.ioNamePtr = (StringPtr)name;
	cBlock.dirInfo.ioFDirIndex = -1;

	while( !( err = PBGetCatInfoSync( &cBlock ) ) )
	{

		SETUP_PLUGIN_INFO_STR( "\p pathFromFSSpec: parent is;g", cBlock.dirInfo.ioNamePtr );

		PtoCstr( name );
		namelen = strlen( (void*)name );
		hlen = GetHandleSize( h );
		HUnlock( h );
		SetHandleSize( h, namelen + hlen + 1 );
		if ( GetHandleSize( h ) != ( namelen + hlen + 1 ) )
		{
			err = ( err = MemError() ) ? err : -1;
			SETUP_PLUGIN_ERROR( "\p pathFromFSSpec: SetHandleSize error;g", err );
			break;
		}
		HLock( h );
		
		// prepend name and a colon onto handle
		if ( hlen )
			BlockMove( &(*h)[ 1 ], &(*h)[ namelen + 2 ], hlen );
		BlockMove( name, &(*h)[ 1 ], namelen );
		(*h)[ namelen + 1 ] = '/';

		// stop when at root, else move up to parent directory
		if ( cBlock.dirInfo.ioDrDirID == 2L )
			break;
	
		cBlock.dirInfo.ioDrDirID=cBlock.dirInfo.ioDrParID;
	}

	if ( err )
	{
		SETUP_PLUGIN_ERROR( "\p pathFromFSSpec: possible PBGetCatInfoSync error;g", err );
		if ( h )
		{
			DisposeHandle( h );
			h = NULL;
		}
	}
	return h;
}
