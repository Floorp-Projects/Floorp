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
// umimemap.h
// MIME to Mac application/filetype mappings
// Created by atotic, June 13th, 1994
// ===========================================================================

#include <LArray.h>
#include "AppleEvents.h"
#include "PascalString.h"

class LFileBufferStream;
class CPrefs;

typedef struct URL_Struct_ URL_Struct;

/*****************************************************************************
 * class CMimeMapper
 * Maps a mime type to record describing what external application to use
 * with this mime type, and how to launch it.
 * Mappings are stored in a preference file, as resources of type MIME
 *****************************************************************************/

class CMimeMapper {
public:
		// All these must be non-zero because of a presumed bug in back end
	enum LoadAction { Save = 0, Launch = 1, Internal = 2, Unknown = 3, Plugin = 4};
	
	// •• Constructors/destructors
	void		InitMapper();
	CMimeMapper(Handle resRecord);	// Takes in a resource of type 'MIME'
	CMimeMapper(const CMimeMapper& clone);	// Duplicate
	CMimeMapper(LoadAction loadAction, const CStr255& mimeType,
		const CStr255& appName, const CStr255& extensions, 
		OSType appSignature, OSType fileType );
	~CMimeMapper();

	static	Boolean	NetscapeCanHandle(CStr255& mimeType);

	// •• XP prefs
	CMimeMapper( const char* basepref );
	static CMimeMapper*	CreateMapperFor(const char* basepref, Boolean newPrefFormat = true);
	static CMimeMapper*	CreateMapperForRes(Handle res);
	
	void		ReadMimePrefs();
	void		WriteMimePrefs(Boolean isDefault = false);


	// •• Access
	
		// Set functions are used by app setup box
	void SetAppName(const CStr255& newName);
	void SetMimeType(const CStr255& newType);
	void SetExtensions(const CStr255& newExtensions);
	void SetPluginName(const CStr255& newName)			{ fPluginName = newName; }
	void SetDescription(const CStr255& newDesc)			{ fDescription = newDesc; }
	void SetAppSig(OSType newSig);
	void SetAppSig(FSSpec& appSpec);	// Set the signature, given the file spec. Will THROW on error
	void SetDocType(OSType newType);
	void SetLoadAction(LoadAction newAction);
	void SetLatentPlugin();
	void SetDefaultDescription();
	
		// External viewers
	void SetTemporary(Boolean isTemporary)	{fTemporary = isTemporary;}
	void RegisterViewer(OSType tempAppSig, OSType tempFileType);
	void UnregisterViewer();
	Boolean IsViewerRegistered() const { return fRegisteredViewer; }

		// Get functions
	CStr255 GetAppName() {return fAppName;};
	CStr255 GetMimeName() {return fMimeType;};
	CStr255 GetExtensions() {return fExtensions;};
	CStr255 GetPluginName() { return fPluginName; }
	CStr255 GetDescription() { return fDescription; }
	OSType GetAppSig() {return fRegisteredViewer ? fTempAppSig : fAppSig;};
	OSType GetDocType() {return fRegisteredViewer ? fTempFileType :fFileType;};
	LoadAction GetLoadAction() {return fRegisteredViewer ? Launch : fLoadAction;};	
	Boolean IsTemporary() {return fTemporary;}
	Boolean FromOldPrefs() { return fFromOldPrefs; }
	Boolean LatentPlugin() { return fLatentPlugin; }
	char* GetBasePref() { return fBasePref; }
	
	// •• operators
	CMimeMapper & operator = (const CMimeMapper& mapper);
	
	// •• utility
	// Get the spec for this MIME mapper
	OSErr	GetFileSpec(URL_Struct * request, 
						FSSpec & destination);
	OSErr	LaunchFile(LFileBufferStream * fFile,	// File to launch
						URL_Struct * request,	// URL associated with the file
						Int32 windowID);		// ID of the parent window
	void SyncNetlib();
private:
	OSErr	MakeSpyEvent(AppleEvent & event, 
						URL_Struct * request,
						AEEventID id,
						AEKeyword urlAttribute);

	Boolean	PrintableChars(const void *inBuffer, Int32 inLength);
	
	// Basic mapper information
	CStr255		fMimeType;			// MIME type
	CStr255		fAppName;			// Application's name
	CStr255		fExtensions;		// Extensions for this file type
	OSType		fAppSig;			// Application signature
	OSType		fFileType;			// Mac file type
	LoadAction	fLoadAction;		// What to do after launch
	// Information about the registered viewers
	Boolean		fTemporary;			// This is only a temporary mapper
	Boolean		fRegisteredViewer;	// This is a registered viewer, use all the special codes
	OSType		fTempAppSig;		// Application
	OSType		fTempFileType;		// File signature
	CStr255		fPluginName;		// Plug-in's name
	CStr255		fDescription;		// Description of the type
	Boolean		fFromOldPrefs;		// Was this mapper from pre-plugin prefs?
	Boolean		fLatentPlugin;		// Was the plug-in disabled only because it’s missing?
	char*		fBasePref;			// Corresponding XP pref branch name
};



/*****************************************************************************
 * class CMimeList
 * Holds a list of CMimeMappers. Has utility functions for mime list manipulation
 * Also handles registration of special viewers
 *****************************************************************************/
 
class CMimeList : public LArray {
friend class CPrefs;
public:
	enum BuiltIn { HTMLViewer = 0, Telnet = 1, Tn3270 = 2, Last};
	
	// •• Constructors/destructors
						CMimeList();
	virtual				~CMimeList();
	
	// Overrode this on the assumption that every time you add a MIME mapper
	// to this list, you want to sync up the netlib
	virtual ArrayIndexT	InsertItemsAt(
							Uint32			inCount,
							ArrayIndexT		inAtIndex,
							const void		*inItem,
							Uint32			inItemSize = 0);

	// •• Utility functions
	// Deletes all the items in the list. Can also delete static MIME mappers
	void				DeleteAll(Boolean unused = false);
 	
 	// Returns mimemapper with given mime type
 	//	NULL if none
 	CMimeMapper*		FindMimeType( char* mimeType );
	CMimeMapper*		FindMimeType( BuiltIn mimeBuiltin );
	CMimeMapper*		FindMimeType( const FSSpec& file );
 	
 	// Returns mimemapper with given application signature
	CMimeMapper* 		FindCreator( OSType appSig );	// NULL if none

	// Returns mimemapper with given XP pref branch name
	CMimeMapper*		FindBasePref( const char* basepref );

	// •• Apple event handling
	void 				HandleRegisterViewerEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	void 				HandleUnregisterViewerEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);

};
