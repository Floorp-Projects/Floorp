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

// umimemap.cp
// CMimeMapper class and CMimeList
// Created by atotic, June 6th, 1994
// ===========================================================================

// Front End
#include "umimemap.h"
#include "macutil.h"
#include "resae.h"
#include "uapp.h"
#include "CAppleEventHandler.h"
#include "ulaunch.h"
#include "ufilemgr.h"
#include "uprefd.h"
#include "resgui.h"
#include "BufferStream.h"

// Netscape
#include "client.h"
#include "net.h"
// stdc
#include <String.h>

#include "uerrmgr.h"
#include "prefapi.h"

//----------------------------------------------------------------------
// class CMimeMapper
//----------------------------------------------------------------------

	// ¥¥ Constructors/destructors

// Initialize all the variables to defaults
// Every initializer calls it
void CMimeMapper::InitMapper()
{
	fTemporary = false;			// This is only a temporary mapper
	fRegisteredViewer = false;	// This is a registered viewer, use all the special codes
	fTempAppSig = '????';		// Application
	fTempFileType = '????';		// File signature
	fFromOldPrefs = false;		// Was this mapper from pre-plugin prefs?
	fLatentPlugin = false;		// Was the plug-in disabled because itÕs missing?
	fLoadAction = CMimeMapper::Unknown;
	fBasePref = nil;			// Corresponding XP pref branch name
	fIsLocked = false;			// was this locked by Mission Control (disables edit and delete)
	fNumChildrenFound = 0;
	fFileFlags  = 0;
}


Boolean
CMimeMapper::NetscapeCanHandle(const CStr255& mimeType)
{
	return ((mimeType == IMAGE_GIF) ||
			(mimeType == IMAGE_JPG) ||
			(mimeType == IMAGE_XBM) ||
			(mimeType == IMAGE_PNG) ||
			(mimeType == APPLICATION_BINHEX) ||
			(mimeType == HTML_VIEWER_APPLICATION_MIME_TYPE) ||
			(mimeType == APPLICATION_PRE_ENCRYPTED) ||
			(mimeType == TEXT_PLAIN));
}


// ¥¥ XP Prefs
//
// CMimeMapper is technically now redundant because all the information 
// it contains is also reflected in xp prefs.  Keeping duplicate
// structures is not ideal but it's minimal-impact.
//
// The xp pref routines are:
//  - CreateMapperFor: creates a mapper corresponding to a specific
//    xp mime pref (e.g. mime.image_gif)
//  - ReadMimePref: converts an xp pref to a mapper (called from
//	  CreateMapperFor, and when xp prefs are updated by auto-config).
//	- WriteMimePref: converts a mapper to its xp pref representation
//	  (called when initializing 3.0-format preferences, after editing
//	  a mime type from its pref UI, and when registering plug-ins).

static const char* Pref_MimeType = ".mimetype";
static const char* Pref_AppName = ".mac_appname";
static const char* Pref_AppSig = ".mac_appsig";
static const char* Pref_FileType = ".mac_filetype";
static const char* Pref_Extension = ".extension";
static const char* Pref_PluginName = ".mac_plugin_name";
static const char* Pref_Description = ".description";
static const char* Pref_LoadAction = ".load_action";
static const char* Pref_LatentPlugin = ".latent_plug_in";
static const char* Pref_FileFlags	= ".file_flags";

// CreateMapperFor converts an xp pref name into a mimetype mapper.
// Finds an existing mapper or creates a new one, and
// populates its fields from user pref values.
CMimeMapper* CMimeMapper::CreateMapperFor( const char* basepref, Boolean newPrefFormat )
{
	CMimeMapper* mapper = CPrefs::sMimeTypes.FindBasePref(basepref);

	if (mapper) {
		mapper->ReadMimePrefs();
		return NULL;	// already exists; caller doesn't use it.
	}
	else {
		mapper = new CMimeMapper(basepref);
		// FromOldPrefs triggers plug-ins to install themselves as preferred viewers
		mapper->fFromOldPrefs = !newPrefFormat;
		
		// Throw out this mime mapper if we didn't find any useful info about it in prefs
		if (mapper->GetNumChildrenFound() <= 0)
		{
			delete mapper;
			return NULL;
		}
		else	
			return mapper;
	}
}

// CreateMapperForRes converts a 3.0-format mime resource
// into both a mimetype mapper and xp prefs.
CMimeMapper* CMimeMapper::CreateMapperForRes(Handle res)
{
	SInt8 flags = ::HGetState(res);
	::HNoPurge(res);
	
	CMimeMapper* mapper = new CMimeMapper( res );
	::HSetState(res, flags);

	if (mapper->fMimeType == HTML_VIEWER_APPLICATION_MIME_TYPE) {
		mapper->SetLoadAction(CPrefs::sViewSourceInline ? Internal : Launch);
	}

	mapper->WriteMimePrefs(false);			// convert to xp format

	mapper->SetDefaultDescription();

	return mapper;
}


// Constructor to build mapper from xp prefs
CMimeMapper::CMimeMapper( const char* basepref )
{
	InitMapper();
	
	fBasePref = XP_STRDUP(basepref);
	if (strlen(fBasePref) > 200)
		fBasePref[200] = '\0';
	ReadMimePrefs();
	
	SetDefaultDescription();
}

// Copies an xp mimetype pref into the mapper
void CMimeMapper::ReadMimePrefs()
{
	int err;
	int size = 256,
		actualSize;
	char value[256];

	fAppSig = 0;

	fNumChildrenFound = 0;	// keep track of how many mapper prefs we successfully read in from prefs file

	err = PREF_GetCharPref( CPrefs::Concat(fBasePref, Pref_AppSig), value, &size );
	if (PREF_NOERROR == err)
	{
		fNumChildrenFound++;
	
		// values stored as binary (Base64) will always be longer than 4 bytes
		// so if the string is 4 bytes long (or shorter), it's a straight character string
		actualSize = strlen(value);
		if (actualSize <= sizeof(OSType))
			::BlockMoveData(value, (Ptr) &fAppSig, actualSize );
		else
		{
			err = PREF_GetBinaryPref ( CPrefs::Concat( fBasePref, Pref_AppSig ), value, &size);
			if (PREF_NOERROR == err)
				::BlockMoveData( value, (Ptr) &fAppSig, sizeof(OSType) );
		}
	}

	fFileType = 0;
	err = PREF_GetCharPref( CPrefs::Concat(fBasePref, Pref_FileType), value, &size );
	if (PREF_NOERROR == err)
	{
		// see notes above for fAppSig
		fNumChildrenFound++;
		
		actualSize = strlen(value);
		if (actualSize <= sizeof(OSType))
			::BlockMoveData( value, (Ptr) &fFileType, actualSize );
		else
		{
			err = PREF_GetBinaryPref ( CPrefs::Concat( fBasePref, Pref_FileType ), value, &size);
			if (PREF_NOERROR == err)
				::BlockMoveData( value, (Ptr) &fFileType, sizeof(OSType) );
		}
	}
	
	err = PREF_GetCharPref( CPrefs::Concat(fBasePref, Pref_AppName), value, &size );
	if (PREF_NOERROR == err)
	{
		fNumChildrenFound++;
		fAppName = value;
	}
	
	err = PREF_GetCharPref( CPrefs::Concat(fBasePref, Pref_MimeType), value, &size );
	if (PREF_NOERROR == err)
	{
		fNumChildrenFound++;
		
		fMimeType = value;

		if (PREF_PrefIsLocked(CPrefs::Concat(fBasePref,Pref_MimeType)))		//for mission control - this is a convienient place to check this
			fIsLocked = true;
	}
	
	err = PREF_GetCharPref( CPrefs::Concat(fBasePref, Pref_Extension), value, &size );
	if (PREF_NOERROR == err)
	{
		fNumChildrenFound++;
		fExtensions = value;
	}
	err = PREF_GetCharPref( CPrefs::Concat(fBasePref, Pref_PluginName), value, &size );
	if (PREF_NOERROR == err)
	{
		fNumChildrenFound++;
		fPluginName = value;
	}
	
	err = PREF_GetCharPref( CPrefs::Concat(fBasePref, Pref_Description), value, &size );
	if (PREF_NOERROR == err)
	{
		fNumChildrenFound++;
		fDescription = value;
	}
	int32 intvalue;
	err = PREF_GetIntPref( CPrefs::Concat(fBasePref, Pref_LoadAction), &intvalue );
	if (PREF_NOERROR == err)
	{
		fNumChildrenFound++;
		fLoadAction = (LoadAction) intvalue;
	}
	err = PREF_GetIntPref( CPrefs::Concat(fBasePref, Pref_FileFlags), &intvalue );
	if (PREF_NOERROR == err)
	{
		fFileFlags =  intvalue;
	}
	
	XP_Bool boolvalue;
	err = PREF_GetBoolPref( CPrefs::Concat(fBasePref, Pref_LatentPlugin), &boolvalue );
	if (PREF_NOERROR == err)
	{
		fNumChildrenFound++;
		fLatentPlugin = (Boolean) boolvalue;
	}

}

// -- For 3.0 format compatability --
// Initializes off a resource handle of one of the mime types. 
// See pref.r for resource definition
// The memory layout of the resource is very important.
#define NAME_LENGTH_OFFSET 9
CMimeMapper::CMimeMapper( Handle resRecord )
{
	ThrowIfNil_(resRecord);
	Size size = ::GetHandleSize(resRecord);
	InitMapper();

	char 	temp[256];
	Ptr		resPtr = *resRecord;
	Byte	lengthByte = 0;
	long	offset = 0;
	
	ThrowIf_(size < 9);
	::BlockMoveData( resPtr, (Ptr)&fAppSig, 4 );
	::BlockMoveData( resPtr + 4, (Ptr)&fFileType, 4 );
	uint32 loadAction = *((unsigned char*) (resPtr + 8));
	fLoadAction = (LoadAction) loadAction;

	offset = NAME_LENGTH_OFFSET;
	ThrowIf_(offset > size);
	// ¥ read the application name
	lengthByte = resPtr[ offset ];
	ThrowIf_(lengthByte > 255);
	::BlockMoveData( &resPtr[ offset + 1 ], (Ptr)temp, lengthByte );

	temp[ lengthByte ] = 0;

	fAppName = temp;

	// ¥ read in the MIME type
	offset += ( lengthByte + 1 );
	ThrowIf_(offset > size);
	lengthByte = resPtr[ offset ];
	::BlockMoveData( &resPtr[ offset + 1 ], temp, lengthByte );

	temp[ lengthByte ] = 0;

	fMimeType = temp;

	// ¥ read in the extensions string
	offset += ( lengthByte + 1 );
	ThrowIf_(offset > size);
	lengthByte = resPtr[ offset ];
	ThrowIf_(lengthByte > 255);
	::BlockMove ( &resPtr[ offset + 1 ], temp, lengthByte );
	
	temp[ lengthByte ] = 0;
	
	fExtensions = temp;
		
	// ¥ read in the plug-in name string
	// Since the plug-in name doesn't exist in 2.0-vintage prefs resources, make
	// sure to check that there is still data left in the handle before proceeding.
	offset += ( lengthByte + 1 );
	if (offset < size)
	{
		lengthByte = resPtr[ offset ];
		ThrowIf_(lengthByte > 255);
		::BlockMove ( &resPtr[ offset + 1 ], temp, lengthByte );
		temp[ lengthByte ] = 0;
		fPluginName = temp;
		
		// Now read the description string (it too could be non-existant)
		offset += ( lengthByte + 1 );
		if (offset < size)
		{
			lengthByte = resPtr[ offset ];
			ThrowIf_(lengthByte > 255);
			::BlockMove ( &resPtr[ offset + 1 ], temp, lengthByte );
			temp[ lengthByte ] = 0;
			fDescription = temp;
			
			// Last, read the "latent plug-in" flag (if it exists)
			offset += ( lengthByte + 1 );
			if (offset < size)
			{
				Boolean latentPlugin = *((unsigned char*) (resPtr + offset));
				XP_ASSERT(latentPlugin == 0 || latentPlugin == 1);
				fLatentPlugin = latentPlugin;
			}
		}
	}
	else
	{
		fPluginName = "";
		fFromOldPrefs = true;				// No plug-in info in these prefs
	}
}

//
// For old prefs that don't have a description stored in the handle,
// look up the default description in netlib, defaulting to just the
// MIME type itself if it can't be found.  This description will then
// get written out to the prefs so it can be edited and saved by the user.
//
void CMimeMapper::SetDefaultDescription()
{
	if (fMimeType == HTML_VIEWER_APPLICATION_MIME_TYPE)
		return;

	if (fNumChildrenFound <= 0 && fMimeType == CStr255::sEmptyString)	// this means the pref did not even come with a mime type, so we ignore it. 
		return;

	CStr255 description;
	char* mimetype = (char*) fMimeType;
    NET_cdataStruct* cdata = NULL;
	XP_List* list_ptr = cinfo_MasterListPointer();
	while ((cdata = (NET_cdataStruct*) XP_ListNextObject(list_ptr)) != NULL)
		if (strcasecomp(mimetype, cdata->ci.type) == 0) 
			break;

	if (cdata && cdata->ci.desc)
		description = cdata->ci.desc;
	else
		description = fMimeType;
	
	if (fDescription == CStr255::sEmptyString) {
		fDescription = description;
		CPrefs::SetModified();
	}
	
	PREF_SetDefaultCharPref( CPrefs::Concat(fBasePref, Pref_Description), description );
}


// Duplicate
CMimeMapper::CMimeMapper( const CMimeMapper& clone )
{
	InitMapper();
	fMimeType = clone.fMimeType;
	fAppName = clone.fAppName;
	fAppSig = clone.fAppSig;
	fFileType = clone.fFileType;
	fLoadAction = clone.fLoadAction;
	fExtensions = clone.fExtensions;
	
	fTemporary = clone.fTemporary;		
	fRegisteredViewer = clone.fRegisteredViewer;
	fTempAppSig = clone.fTempAppSig;			// Application
	fTempFileType = clone.fTempFileType;		// File signature
	
	fPluginName = clone.fPluginName;
	fFromOldPrefs = clone.fFromOldPrefs;
	fDescription = clone.fDescription;
	fLatentPlugin = clone.fLatentPlugin;
	fFileFlags = clone.fFileFlags;
	fBasePref = clone.fBasePref ? XP_STRDUP(clone.fBasePref) : nil;
	
	fIsLocked = clone.fIsLocked;
	
	fNumChildrenFound = clone.fNumChildrenFound;
}

// Copies values from the mapper to xp user pref tree.
void CMimeMapper::WriteMimePrefs( Boolean )
{
	if (fBasePref == nil) {
		// Convert mime type to a pref-name string with 
		// non-alpha chars replaced with underscores
		char typecopy[256];
		strcpy(typecopy, "mime.");
		int start = strlen(typecopy);
		strncat(typecopy, (char*) fMimeType, 200);
		for (int i = start; i < strlen(typecopy); i++)
			if (!isalnum(typecopy[i]))
				typecopy[i] = '_';
		
		// Mimetype may be blank, so pick an arbitrary name
		if (fMimeType.Length() == 0)
			strcat(typecopy, "unknown_type_999");
		
		fBasePref = XP_STRDUP(typecopy);
	}
	
	char appsig[sizeof(OSType)+1], filetype[sizeof(OSType)+1];
	appsig[sizeof(OSType)] = '\0';
	::BlockMoveData( (Ptr) &fAppSig, appsig, sizeof(OSType) );
	filetype[sizeof(OSType)] = '\0';
	::BlockMoveData( (Ptr) &fFileType, filetype, sizeof(OSType) );
	
	PREF_SetCharPref( CPrefs::Concat(fBasePref, Pref_Extension), fExtensions );
	fNumChildrenFound++;	// since we are creating children prefs, we should keep this record updated
	if (fDescription.Length() > 0)
	{
		PREF_SetCharPref( CPrefs::Concat(fBasePref, Pref_Description), fDescription );
		fNumChildrenFound++;
	}
	PREF_SetCharPref( CPrefs::Concat(fBasePref, Pref_MimeType), fMimeType );
	PREF_SetCharPref( CPrefs::Concat(fBasePref, Pref_AppName), fAppName );
	PREF_SetCharPref( CPrefs::Concat(fBasePref, Pref_PluginName), fPluginName );
	fNumChildrenFound += 3;
	// store appsig and filetype as 4-byte character strings, if possible.
	// otherwise, store as binary.
	if (PrintableChars( &appsig, sizeof(OSType)))
		PREF_SetCharPref( CPrefs::Concat(fBasePref, Pref_AppSig), appsig );
	else
		PREF_SetBinaryPref( CPrefs::Concat(fBasePref, Pref_AppSig), appsig, sizeof(OSType));
	if (PrintableChars( &filetype, sizeof(OSType)))
		PREF_SetCharPref( CPrefs::Concat(fBasePref, Pref_FileType), filetype );		
	else
		PREF_SetBinaryPref( CPrefs::Concat(fBasePref, Pref_FileType), filetype, sizeof(OSType));
	PREF_SetIntPref( CPrefs::Concat(fBasePref, Pref_LoadAction), fLoadAction );
	PREF_SetBoolPref( CPrefs::Concat(fBasePref, Pref_LatentPlugin), (XP_Bool) fLatentPlugin );
	PREF_SetIntPref ( CPrefs::Concat(fBasePref, Pref_FileFlags ), fFileFlags );
	fNumChildrenFound += 4;
}

CMimeMapper::CMimeMapper(
	LoadAction loadAction,
	const CStr255&	mimeType,
	const CStr255&	appName,
	const CStr255&	extensions,
	OSType			appSignature,
	OSType			fileType )
{
	fLoadAction = loadAction;
	fMimeType = mimeType;
	fAppName = appName;
	fAppSig = appSignature;
	fFileType = fileType;
	fExtensions = extensions;
	
	fTemporary = false;
	fRegisteredViewer = false;
	fPluginName = "";
	fDescription = "";
	fLatentPlugin = false;
	fBasePref = nil;
}

// Disposes of all allocated structures
CMimeMapper::~CMimeMapper()
{
	if (fBasePref)
		free (fBasePref);
}

CMimeMapper & CMimeMapper::operator= (const CMimeMapper& mapper)
{
	fMimeType = mapper.fMimeType;	
	fAppName = mapper.fAppName;
	fAppSig = mapper.fAppSig;
	fFileType = mapper.fFileType;
	fLoadAction = mapper.fLoadAction;
	fPluginName = mapper.fPluginName;
	fFromOldPrefs = mapper.fFromOldPrefs;
	fDescription = mapper.fDescription;
	fLatentPlugin = mapper.fLatentPlugin;
	fFileFlags  = mapper.fFileFlags;
	return *this;
}

void CMimeMapper::SetAppName(const CStr255& newName) 
{
	fAppName = newName;
}

void CMimeMapper::SetMimeType(const CStr255& newType) 
{
	fMimeType = newType;
}

void CMimeMapper::SetAppSig(OSType newSig) 
{
	fAppSig = newSig;
}

void CMimeMapper::SetAppSig(FSSpec& appSpec) 
{
	FInfo finderInfo;
	OSErr err = FSpGetFInfo(&appSpec, &finderInfo );	
	ThrowIfOSErr_(err);
	fAppSig = finderInfo.fdCreator;
}

void CMimeMapper::SetExtensions(const CStr255& newExtensions)
{
	fExtensions = newExtensions;
	SyncNetlib();
}

void CMimeMapper::SetDocType(OSType newType) 
{
	fFileType = newType;
}	

void CMimeMapper::SetLoadAction(LoadAction newAction)
{
	//
	// If the user explicitly changed the load action,
	// cancel the latent plug-in setting so their choice
	// is maintained persistently.
	//
	fLatentPlugin = false;
		
	//
	// If the user explicitly changed the load action,
	// cancel the registration of an external viewer.
	//
	fRegisteredViewer = false;
	
	fLoadAction = newAction;
	SyncNetlib();
}

void CMimeMapper::SetLatentPlugin()
{
	fLatentPlugin = true;
	fLoadAction = CMimeMapper::Unknown;
}


void CMimeMapper::RegisterViewer(OSType tempAppSig, OSType tempFileType)
{
	fTempAppSig = tempAppSig;		// Application
	fTempFileType = tempFileType;		// File signature
	fRegisteredViewer = TRUE;
	CFrontApp::RegisterMimeType(this);
}

void CMimeMapper::UnregisterViewer()
{
	fRegisteredViewer = FALSE;
	CFrontApp::RegisterMimeType(this);
}

// Typical Spy apple event creation routine.
// It creates the event, with two arguments, URL and the MIME type.
// URL type is stored as attribute urlAttribute, MIME type as AE_spy_viewDocFile_mime
OSErr	CMimeMapper::MakeSpyEvent(AppleEvent & event, 
								URL_Struct * request, 
								AEEventID id,
								AEKeyword urlAttribute)
{
	OSErr err;
	Try_
	{
		AEAddressDesc targetApp;
		
		// Specify the application, and create the event
		{
		err = ::AECreateDesc(typeApplSignature, &fTempAppSig, 
								 sizeof(fTempAppSig), &targetApp);
		ThrowIfOSErr_(err);
		OSErr err = ::AECreateAppleEvent(AE_spy_send_suite, id,
									&targetApp,
									kAutoGenerateReturnID,
									kAnyTransactionID,
									&event);
		::AEDisposeDesc(&targetApp);
		}
		// URL
		{
			AEDesc urlDesc;
			err = ::AECreateDesc(typeChar, request->address, strlen(request->address), &urlDesc);
			ThrowIfOSErr_(err);
			
			err = ::AEPutParamDesc(&event, urlAttribute, &urlDesc);
			ThrowIfOSErr_(err);
			::AEDisposeDesc(&urlDesc);
		}
		// MIME type
		if (request->content_type)
		{
			AEDesc mimeDesc;
			err = ::AECreateDesc(typeChar, request->content_type, strlen(request->content_type), &mimeDesc);
			ThrowIfOSErr_(err);
			err = ::AEPutParamDesc(&event, AE_spy_queryViewer_mime, &mimeDesc);			
			::AEDisposeDesc(&mimeDesc);
		}
	}
	Catch_(inErr)
	{	return inErr;
	}
	EndCatch_
	return noErr;
}

/* Returns whether the contents of inBuffer are all visible, printable characters.
   Actually, that's a misnomer ... it returns whether the contents are printable
   on a Macintosh, which we define to be "not control characters."  This is intended
   for determining whether a binary buffer can be stored as a text string, or needs
   to be encoded. */
Boolean CMimeMapper::PrintableChars(const void *inBuffer, Int32 inLength)
{
	unsigned char	*buffer = (unsigned char *) inBuffer,
					*bufferEnd = buffer + inLength;

	while (buffer < bufferEnd)
	{
		if (*buffer <= 0x20)
			break;
		buffer++;
	}
	return buffer == bufferEnd;
}

// if we have a registered viewer, query it for a file spec
OSErr	CMimeMapper::GetFileSpec(URL_Struct * request, 
								FSSpec & destination)
{
	OSErr err = noErr;
	if (!fRegisteredViewer)
		return fnfErr;
	else
	Try_
	{	
		AppleEvent queryViewerEvent;

		err = MakeSpyEvent(queryViewerEvent, request,AE_spy_queryViewer,
						keyDirectObject); // URL is the direct object
		// 	Send the event
		AppleEvent	reply;
		err = ::AESend(&queryViewerEvent,&reply,kAEWaitReply,kAEHighPriority,600,nil, nil);
		ThrowIfOSErr_(err);
		// Handle the reply. We want to get out the transaction ID
		{
			DescType realType;
			Size actualSize;
			if (!MoreExtractFromAEDesc::DisplayErrorReply(reply))
			{
				err = ::AEGetParamPtr(&reply, keyAEResult, typeFSS, 
						 &realType, &destination, sizeof(destination), 
						&actualSize);
				ThrowIfOSErr_(err);
			}
			else
				Throw_(fnfErr);
			::AEDisposeDesc(&reply);
		}
	}
	Catch_(inErr)
	{
		return inErr;
	}	EndCatch_
	return noErr;
}

// If we have a registered viewer, use ViewDocFile
// if anything fails, or we have no viewer, use ordinary launch sequence
OSErr CMimeMapper::LaunchFile(LFileBufferStream * inFile, URL_Struct * request, Int32 windowID)
{
	OSErr err;
	if (fRegisteredViewer)
	{
		Try_
		{
			AppleEvent viewDocEvent;
			// AppleEvent(fileSpec, URL, MIME, windowID)
			// Create the event, with MIME and URL arguments
			err = MakeSpyEvent(viewDocEvent, request,AE_spy_viewDocFile,
							AE_spy_viewDocFile_url); // URL is the direct object
			ThrowIfOSErr_(err);
			// Make the file spec
			FSSpec fileSpec;
			inFile->GetSpecifier(fileSpec);
			err = ::AEPutParamPtr(&viewDocEvent,keyDirectObject,typeFSS,&fileSpec,sizeof(fileSpec));
			ThrowIfOSErr_(err);
			// URL
			if (request->address)
			{
				err = ::AEPutParamPtr(&viewDocEvent,AE_spy_viewDocFile_url, typeChar, request->address,strlen(request->address));
				ThrowIfOSErr_(err);
			}
			// MIME type
			{
				StAEDescriptor mimeDesc((StringPtr)fMimeType);
				err = ::AEPutParamDesc(&viewDocEvent,AE_spy_viewDocFile_mime,&mimeDesc.mDesc);
				ThrowIfOSErr_(err);
			}
			// WindowID
			{
			StAEDescriptor windowDesc(windowID);
			err = ::AEPutParamDesc(&viewDocEvent,AE_spy_viewDocFile_wind, &windowDesc.mDesc);
			ThrowIfOSErr_(err);
			}
			// Send the event
			AppleEvent	reply;
			err = ::AESend(&viewDocEvent,&reply,kAEWaitReply,kAEHighPriority,60,nil, nil);
			if (err != errAETimeout)
				ThrowIfOSErr_(err);
			// If we got an error code back, launch as usual
			if (reply.descriptorType != typeNull)
			{
				DescType realType;
				Size actualSize;
				long	errNumber;
				err = ::AEGetParamPtr(&reply, keyErrorNumber, typeLongInteger, 
							&realType, &errNumber, sizeof(errNumber), 
							&actualSize);
				if ((err == noErr) && (errNumber != noErr))
					Throw_(errNumber);
			}
		}
		Catch_(inErr)
		{
			// Launch through ViewDoc failed, use our original creator and launch as usual
			FSSpec fileSpec;
			inFile->GetSpecifier(fileSpec);
			err = CFileMgr::SetFileTypeCreator(fAppSig, fFileType, &fileSpec);
			::LaunchFile(inFile);
		}
		EndCatch_
	}
	else
		::LaunchFile(inFile);
	return noErr;
}


void CMimeMapper::SyncNetlib()
{
	CFrontApp::RegisterMimeType(this);
}

//----------------------------------------------------------------------
// class CMimeList
//----------------------------------------------------------------------

	// ¥¥ Constructors/destructors

CMimeList::CMimeList() : LArray()
{
}

CMimeList::~CMimeList()
{
}

	// ¥¥ÊUtility functions
// Overrode this on the assumption that every time you add a MIME mapper
// to this list, you want to sync up the netlib
ArrayIndexT CMimeList::InsertItemsAt(
	Uint32			inCount,
	ArrayIndexT		inAtIndex,
	const void		*inItem,
	Uint32			inItemSize)

{
	ArrayIndexT result = LArray::InsertItemsAt(inCount,inAtIndex,inItem, inItemSize);
	CMimeMapper ** mapperPtr = (CMimeMapper **)inItem;
	(*mapperPtr)->SyncNetlib();
	return result;
}

// Deletes all the items in the list
void CMimeList::DeleteAll(Boolean)
{
	CMimeMapper*		oldMap;
	for ( short i = 1; i <= mItemCount; i++ )
	{
		FetchItemAt( i, &oldMap );
		delete oldMap;
	}
	RemoveItemsAt( mItemCount,1);
}

// Finds a CMimeMapper with a given XP pref branch name.
CMimeMapper* CMimeList::FindBasePref( const char* basepref )
{
	CMimeMapper* foundMap = NULL;
	for ( Int32 i = mItemCount; i >= 1; i-- )
	{
		CMimeMapper* oldMap;
		FetchItemAt( i, &oldMap );
		if ( oldMap->GetBasePref() != NULL &&
			 XP_STRCMP(oldMap->GetBasePref(), basepref) == 0 )
		{
			foundMap = oldMap;
			break;
		}
	}
	return foundMap;
}


// Finds a CMimeMapper with a given mimeType. NULL if none
// Search is linear. Might want to make it faster
CMimeMapper* CMimeList::FindMimeType( char* mimeType )
{
	if ( mimeType == NULL )
		return NULL;

	CMimeMapper* foundMap = NULL;
	for ( Int32 i = 1; i <= mItemCount; i++ )
	{
		CMimeMapper* oldMap;
		FetchItemAt( i, &oldMap );
		// MIME types are defined to be case insensitive
		if ( XP_STRCASECMP (oldMap->GetMimeName(), mimeType) == 0 )
		{
			foundMap = oldMap;
			break;
		}
	}
	return foundMap;
}

// Find a "built-in" mime type, which were separate in 3.0
// but are integrated into the 4.0 mimetype list.
CMimeMapper* CMimeList::FindMimeType(BuiltIn mimeBuiltin)
{
	char* mimetype = nil;
	switch (mimeBuiltin) {
		case HTMLViewer:
			mimetype = HTML_VIEWER_APPLICATION_MIME_TYPE;
			break;
		case Telnet:
			mimetype = TELNET_APPLICATION_MIME_TYPE;
			break;
		case Tn3270:
			mimetype = TN3270_VIEWER_APPLICATION_MIME_TYPE;
			break;
	}
	return FindMimeType(mimetype);
}


// FindMimeType finds a Mime mapper that
// matches this file's type and creator
// Algorithm is inexact
// TEXT files are not typed	because netlib figures out the 
// HTML files, and hqx ones have the extension
// Look for exact match.
// If not found, look for inexact one

// I don't like the above algorithm. There is no way of telling what type of match occured. In
// cases other than exact or FileType  match the results are unsatisfactory. 
// New behavoir is to return NULL and let the caller call CMimeList::FindCreatorFindCreator if
// needed. Provides a way to hand off to IC if needed.
CMimeMapper* CMimeList::FindMimeType(const FSSpec& inFileSpec)
{
	FInfo		fndrInfo;
	
	OSErr err = FSpGetFInfo( &inFileSpec, &fndrInfo );
	if ((err != noErr) || (fndrInfo.fdType == 'TEXT'))
		return NULL;

	CMimeMapper* fileTypeMatch = NULL;

	// Find the cinfo (which gives us the MIME type) for this file name
	NET_cinfo* cinfo = NET_cinfo_find_type((CStr255)inFileSpec.name);

	for (Int32 i = 1; i <= mItemCount; i++)
	{
		CMimeMapper* map;
		FetchItemAt(i, &map);
		
		//
		// If this MIME type is configured for a plug-in, see if it matches
		// the MIME type of the file (based on the file extension -> MIME
		// type mapping).  If not, try for an exact or partial match based
		// on type and/or creator.  bing: If we decide to support Mac file
		// types for plug-ins, we should check the file type here.
		//
		if (map->GetLoadAction() == CMimeMapper::Plugin)
		{
			if (cinfo && (map->GetMimeName() == cinfo->type))
				return map;
		}
		else if ((map->GetAppSig() == fndrInfo.fdCreator) &&
				 (map->GetDocType() == fndrInfo.fdType) && 
				 !(map->GetFileFlags()  &ICmap_not_outgoing_mask) )
			return map;
		else 
		{
			#if 0
				if (map->GetAppSig() == fndrInfo.fdCreator)
					creatorMatch = map;
			#endif
			if (map->GetDocType() == fndrInfo.fdType && !(map->GetFileFlags()  &ICmap_not_outgoing_mask) )
				fileTypeMatch = map;
		}
	}

	return fileTypeMatch;	
}


// FindCreator finds a CMimeMapper whose application signature
// matches appSig
CMimeMapper* CMimeList::FindCreator(OSType appSig)
{
	CMimeMapper * foundMap = NULL;
	for (Int32 i = 1; i <= mItemCount; i++)
	{
		CMimeMapper* oldMap;
		FetchItemAt(i, &oldMap);
		if (oldMap->GetAppSig() == appSig)
		{
			foundMap = oldMap;
			break;
		}
	}
	return foundMap;
}

// ------------------------- Apple Event handling -------------------------------

// HandleRegisterViewerEvent
// registers a viewer.
// If the MIME type does not exist, we create one on the fly
void CMimeList::HandleRegisterViewerEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&/*outResult*/,
									long				/*inAENumber*/)
{
	OSType	appSign;	//	app signature
	OSType	fileType = 'TEXT';	//	file type to be created
	volatile char *	mimeType = NULL;	// MIME type
	OSType	realType;
	Size	realSize;
	CMimeMapper * mapper;
	OSErr err;
	Try_ {
		{	// Get the application signature. Required
			err = ::AEGetParamPtr(&inAppleEvent,keyDirectObject,typeApplSignature,
									&realType, &appSign, sizeof(appSign), &realSize);
			ThrowIfOSErr_(err);
		}
		{	// Get the MIME type. Required
			AEDesc mimeDesc;
			err = ::AEGetParamDesc(&inAppleEvent,AE_spy_registerViewer_mime,typeWildCard,&mimeDesc);
			ThrowIfOSErr_(err);		
			MoreExtractFromAEDesc::TheCString(mimeDesc, mimeType);
			::AEDisposeDesc(&mimeDesc);
			if (mimeType == nil)
				ThrowOSErr_(errAEWrongDataType);
		}
	
		mapper = FindMimeType(mimeType);
		if (mapper)
			fileType = mapper->GetDocType();
		{	// Get the file type. If none is specified, it defaults to already registered type, or 'TEXT'
			AEDesc fileTypeDesc;
			err = ::AEGetParamDesc(&inAppleEvent,AE_spy_registerViewer_ftyp,typeWildCard,&fileTypeDesc);
			if (err == noErr)
			{
				Try_	{
					UExtractFromAEDesc::TheType(fileTypeDesc, fileType);
				} Catch_(inErr)
				{} EndCatch_
			}
			::AEDisposeDesc(&fileTypeDesc);
		}
		// We have all the arguments, set up the new mapper if necessary
		if (mapper == NULL)
		{
			mapper = new CMimeMapper(CMimeMapper::Launch, 
								mimeType,
								"-",	// I10L - this string is never seen by a user
								CStr255::sEmptyString,
								appSign,
								fileType);
			ThrowIfNil_(mapper);
			mapper->SetTemporary(TRUE);
			InsertItemsAt( 1, LArray::index_Last, &mapper);
		}
		
		// If the type is being handled by a plug-in, donÕt let the AE override
		Boolean result = false;
		if (mapper->GetLoadAction() != CMimeMapper::Plugin)
		{
			mapper->RegisterViewer(appSign, fileType);
			result = true;
		}
		
		// Reply success
		// Create the reply, it will automatically be stuck into the outgoing AE by PP
		{
			StAEDescriptor booleanDesc(result);
			err = ::AEPutKeyDesc(&outAEReply,keyAEResult,&booleanDesc.mDesc);   /* IM VI chap. 6 pg 91 */
		}
	}
	Catch_(inErr)
	{
		MoreExtractFromAEDesc::MakeErrorReturn(outAEReply,
					(unsigned char *)GetCString(REG_EVENT_ERR_RESID) , inErr);
	}	EndCatch_
	// Dispose
	if (mimeType) XP_FREE(mimeType);	
}

void CMimeList::HandleUnregisterViewerEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&/*outResult*/,
									long				/*inAENumber*/)
{
	OSType	appSign;	//	app signature
	OSType	fileType = 'TEXT';	//	file type to be created
	volatile char*	mimeType = NULL;	// MIME type

	CMimeMapper * mapper = NULL;
	OSErr err;
	Try_
	{
		{	// Get the application signature. Required
			AEDesc appSignDesc;
			err = ::AEGetParamDesc(&inAppleEvent,keyDirectObject,typeWildCard,&appSignDesc);
			ThrowIfOSErr_(err);
			UExtractFromAEDesc::TheType(appSignDesc, appSign);
			::AEDisposeDesc(&appSignDesc);
		}
		{	// Get the MIME type. Required
			AEDesc mimeDesc;
			err = ::AEGetParamDesc(&inAppleEvent,AE_spy_unregisterViewer_mime,typeWildCard,&mimeDesc);
			ThrowIfOSErr_(err);		
			MoreExtractFromAEDesc::TheCString(mimeDesc, mimeType);
			::AEDisposeDesc(&mimeDesc);
			if (mimeType == nil)
				ThrowOSErr_(errAEWrongDataType);
		}
		
		mapper = FindMimeType(mimeType);
		
		if (mapper == NULL || mapper->IsViewerRegistered() == false)
		{
			//
			// If the type isnÕt found, or wasnÕt registered
			// to an external viewer, return an error.
			//
			MoreExtractFromAEDesc::MakeErrorReturn(outAEReply, 
				(unsigned char *)GetCString(APP_NOT_REG_RESID), errAEDescNotFound);
		}
		else
		{
			//
			// Otherwise unregister the viewer and remove the 
			// temporary mapper if necessary.
			//
			mapper->UnregisterViewer();
			if (mapper->IsTemporary())
			{
				Remove(&mapper);
				delete mapper;
			}
		}
	}
	Catch_(inErr)
	{
		MoreExtractFromAEDesc::MakeErrorReturn(outAEReply, 
				(unsigned char *)GetCString(UNREG_EVENT_ERR_RESID), inErr);
	}
	EndCatch_
	if (mimeType) XP_FREE(mimeType);
}

