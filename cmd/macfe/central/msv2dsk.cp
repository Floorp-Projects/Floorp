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

// Macintosh front-end

#include "msv2dsk.h"

#include "earlmgr.h"
#include "macutil.h"
#include "uerrmgr.h"
#include "uprefd.h"
#include "ufilemgr.h"
#include "ulaunch.h"
#include "macutil.h"
#include "resgui.h"
#include "prefwutil.h"
#include "uapp.h"
#include "resae.h"

#include "CNSContext.h"
#include "CDownloadProgressWindow.h"

	// Netscape
#include "merrors.h"
#include "xp_thrmo.h"
#include "mkutils.h"
#include "glhist.h"
#include "xlate.h"
#include "prefapi.h"

#include "BufferStream.h"
#include "PascalString.h"

	// system
#include <StandardFile.h>
#include "merrors.h"



static Boolean	GetMacFileTypesFromMimeHeader(	const URL_Struct	*	fRequest, 
												OSType				*	fileCreator,
												OSType				*	fileType	 );
												
static OSType	TextToOSType(	const char*	text);
												


extern int MK_DISK_FULL;



extern CMimeMapper * CreateUnknownMimeTypeMapper (URL_Struct * request);
extern void GUI_PostAlertNoReply (const CStr255 & message);
extern OSErr GUI_AskForFileSpec (StandardFileReply & reply);

/*------------------=----------------------------------------------------------
	External function so everyone can start a progress load without having
	to include the full .h file
--------------------=--------------------------------------------------------*/

extern "C" void SaveAsCompletionProc( PrintSetup* p );
extern "C" void SaveAsCompletionProc( PrintSetup* p )
{
	XP_FileClose (p->out);
	CNSContext* theContext = (CNSContext*)p->url->fe_data;
	
// FIX ME!!! need to put this back in
//	if (theContext != NULL)
//		theContext->AllConnectionsComplete();
		
		
}

extern int MK_OUT_OF_MEMORY;

int PROGRESS_GetUrl(
	URL_Struct *,
	int /*format_out*/,
	const FSSpec * /*destination*/,
	Boolean /*delayed*/)
{
Assert_(false); // NO LONGER VALID
return 0;
}


/*-----------------------------------------------------------------------------
	When we decide to do something with an url (such as save it to disk, print
	it, mail it, etc), we sometimes have extra data which specializes the 
	action. Save-to-disk is the best example: a destination file. We need a 
	place to put it all.
	
	We have been putting this data in the context. This is a mistake because
	the context is not specific to any particular url action, and multiple 
	requests will cause a race condition.
	
	The url struct is where it belongs, since this object is created at that
	time (making it like MacApp's Command class). It has an fe_data field,
	but we need to put *lots* of data there. Argh.
	
	Things go to disk when we launch an external viewer (click -> download in
	normal context) or explicitly request a download (SAVE_AS, SAVE_TO_DISK,
	VIEW_SOURCE, and Drag&Drop).
	FO_PRESENT			auto-launch external viewer (by normal click).
	VIEW_SOURCE			auto-launch with view source application
	SAVE_TO_DISK		prompt for location and save to disk
	SAVE_AS				prompt for location and save to disk
	INTERNAL_IMAGE		display internally.
	Drag&Drop			Variant of SAVE_TO_DISK
	SAVE_ALL			Save page & images. Like SAVE_AS.
-----------------------------------------------------------------------------*/

class DownloadFilePipe: public Pipe
{
public:
					DownloadFilePipe (
							CNSContext* 			progressContext,
							URL_Struct*				request);
					~DownloadFilePipe ();
	OSErr			Open ();
	int				Write (const char *buffer, int len);
	void 			Complete ();
	void 			Abort (int reason);
	
	OSErr			GetFileSpec ();
	inline Boolean	TranslateLinefeeds ();
	Boolean			LaunchWhenDone ();
	CNSContext* 	mContext;
	
protected:
	URL_Struct * 	fRequest;
	NET_StreamClass * fInput;
// fFile logic gets complicated. There are two questions:
// - where to create the file
// - when should the file object be deleted
// - when should the file be deleted
// The answers depend upon the stream:
// 		case FO_PRESENT:
//		ask mapper for the registered viewer file spec.
//			if we get these specs, registered viewer will handle the physical file deletion
//				we need to delete the fFile object
//			else, do the same as 
//		case FO_VIEW_SOURCE:
// 		register with the app, do not delete fFile yourself
//		case FO_SAVE_AS:
// 		do not register, delete file object  - 

	LFileBufferStream *	fFile;
	Boolean			fDeleteOnDestroy;	// Should we delete the file object?
	int				fIntention;
	CMimeMapper * 	fHandler;
	Boolean			fHasDestination;
	FSSpec			fDestination;
	
	OSErr GetSilentFileSpec();
	OSErr GetUserFileSpec();
	
	friend NET_StreamClass * NewFilePipe (int format_out, void *registration, URL_Struct * request, MWContext *context);
};

static LArray DownloadList;	// Keeps track of all downloads, so that we can see if we are interrupting any

Boolean HasDownloads(MWContext * context)
{
	DownloadFilePipe * pipe;
	LArrayIterator iter(DownloadList);
	while (iter.Next(&pipe))
		{
		MWContext* theContext = *(pipe->mContext);
		if (theContext == context)
			return true;
			
		}
	return false;
}

//--------------------------------------------------------------------------------
// DownloadFilePipe
//--------------------------------------------------------------------------------

DownloadFilePipe::DownloadFilePipe (
	CNSContext* progressContext,
	URL_Struct* request)
{
	mContext = progressContext;
	mContext->AddUser(this);
	
	fRequest = request;
	fInput = nil;
	fFile = nil;
	fIntention = -1;
	
	fHandler = nil;
	fHasDestination = false;
	fDeleteOnDestroy = true;
	DownloadList.InsertItemsAt(1,1,&this);
}


DownloadFilePipe::~DownloadFilePipe ()
{
	mContext->RemoveUser(this);

	if (fFile && fDeleteOnDestroy)
	{
		CFileMgr::sFileManager.CancelRegister(fFile);
		delete fFile;
	}
	Int32 where = DownloadList.FetchIndexOf(&this);
	DownloadList.RemoveItemsAt(1,where);
}

OSErr DownloadFilePipe::GetSilentFileSpec()
{
	OSErr		err;
	CStr31		defaultName;
	FSSpec		defaultFolder;
	
	if ( fRequest->content_name )
		defaultName = fRequest->content_name;
	else
		 defaultName =  CFileMgr::FileNameFromURL( fRequest->address ) ;

	defaultFolder = CPrefs::GetFilePrototype( CPrefs::DownloadFolder );
	err = CFileMgr::UniqueFileSpec( defaultFolder, defaultName, fDestination );
	if ( err )
		return err;
	fDeleteOnDestroy = FALSE;
	fHasDestination = TRUE;
	return noErr;
}

// Get specs from the user
OSErr DownloadFilePipe::GetUserFileSpec()
{
	OSErr				err;
	CStr31				defaultName;
	StandardFileReply 	reply;

	if ( fRequest->content_name )
		defaultName = fRequest->content_name;
	else
		 defaultName =  CFileMgr::FileNameFromURL( fRequest->address ) ;

	(CStr63&)reply.sfFile.name = defaultName;
	err = GUI_AskForFileSpec( reply );

	if ( err )
		return err;

	fDestination = reply.sfFile;
	fHasDestination = TRUE;
	fDeleteOnDestroy = TRUE;	// do not delete automatically

	return noErr;
}

// Obtain the location of where the stream should be downloaded to
OSErr DownloadFilePipe::GetFileSpec ()
{
	OSErr err = noErr;
	if (fHasDestination)
		fDeleteOnDestroy = true;
	else
	{
// 		case FO_PRESENT:
//		ask mapper for the registered viewer file spec.
//			if we get these specs, registered viewer will handle the physical file deletion
//				we need to delete the fFile object
//			else, do the same as 
//		case FO_VIEW_SOURCE:
// 		register with the app, do not delete fFile yourself
//		case FO_SAVE_AS:
//			we should have the spec already, die otherwise

		switch (fIntention)	{
			case FO_PRESENT:
				
				
				if (fHandler)
				{
					// See if the helper app is already running and 
					// has registered with us at runtime.  If so,
					// give it a chance to tell us where to put the file.
					//
					err = fHandler->GetFileSpec(fRequest, fDestination);
				
					// tj, with mourey and timm:
					//
					// This wasn't guarded by fHandler but we're sure it should be.
					// We should only delete on destroy if the helper app has registered
					// with us at runtime and tells us where to save the file. Otherwise,
					// we need to prompt for the file name/location.
					//
					// This never used to be a problem because fHandler was never NULL.
					// Now with support for mac type/creator from the mime header, fHandler
					// may be NULL.
					
					 if (err == noErr)
					 {
					 	fDeleteOnDestroy = true;
					 	break;
					 }
				}
				
				if (LaunchWhenDone())
					err = GetSilentFileSpec();
				else
					err = GetUserFileSpec();
					
				break;
			case FO_SAVE_AS:
				err = GetUserFileSpec();
				break;
			case FO_VIEW_SOURCE:
				err = GetSilentFileSpec();
				break;
			default:
				Assert_(false);
		}
	}
	if (err == noErr)	// No error in creating the spec, make the stream
	{
		try
		{
			fFile = new LFileBufferStream (fDestination);
		}
		catch (OSErr err)
		{
			return err;
		}

		fFile->DoUseBuffer();
		if (fRequest && fRequest->address)
			fFile->SetURL(XP_STRDUP(fRequest->address));
		
		if (fDeleteOnDestroy == false)	// If we are not taking care of the file, file manager is
			CFileMgr::sFileManager.RegisterFile(fFile);
	}
	return err;
}

inline Boolean DownloadFilePipe::TranslateLinefeeds ()
{
	return (fIntention == FO_VIEW_SOURCE);
}

Boolean DownloadFilePipe::LaunchWhenDone ()
{
/*
	For view source we always want to launch the viewer.
	For saving HTML, don't launch it, just save.
	For drag&drop, we want to launch Stuffit (for FTP convenience) but not
		a text viewer, so text/html->false, the rest according to mime mapper
*/
	if (fIntention == FO_VIEW_SOURCE)
		return true;
	if (fIntention == FO_SAVE_AS)
		return false;
	if (strcmp(fRequest->content_type, "text/html") == 0) // LAM need entry for text/html!
		return false;
		
	if (fHandler != NULL)
		return fHandler->GetLoadAction() == CMimeMapper::Launch;
	else
		return false;
}


OSErr DownloadFilePipe::Open ()
{
/*
	If this is a drag&drop or SAVE_AS then we already have a filespec.
	If the helper application is set to "launch" then we automatically
	choose a filespec. Otherwise we have to ask.
*/
	OSErr 			err;
	cstring			appName 	= "\0";		// app that can open the attachment
	CMimeMapper * 	mapper		= nil;		// mime-type -> mac-type mapper
	OSType			fileType 	= 0;		// mac file type of the attachment
	OSType 			fileCreator	= 0;		// mac creator type of the attachment
	
	
	//
	// Figure out the appropriate file type and creator
	//
		
	if (fIntention == FO_VIEW_SOURCE || strcmp(fRequest->content_type,"text/html") == 0)
		mapper = CPrefs::sMimeTypes.FindMimeType (CMimeList::HTMLViewer);
	
	if (mapper == NULL)
	{
		//
		// If we have content of application/octet-stream, look for the mac file type
		// and creator in the URL struct (from the mime header).
		//
		// If we find valid mac file type/creator data, or if the content_type
		// isn't octet-stream, then we attempt content_type -> mime-type mapping
		//
		if ( 	(strcmp(fRequest->content_type, APPLICATION_OCTET_STREAM) != 0)	||
				!GetMacFileTypesFromMimeHeader(fRequest, &fileCreator, &fileType)	)
		{		
			mapper = CPrefs::sMimeTypes.FindMimeType (fRequest->content_type);
	
			if (mapper && mapper->GetLoadAction() == CMimeMapper::Unknown)
			{
				//
				// If they specified "Unknown" in the prefs, then we have an existing
				// mapper that tells us this fact.  To know how to handle the stream
				// this time, we need to ask them what to do and create a new mapper.
				//
				mapper = CreateUnknownMimeTypeMapper (fRequest);
				if (!mapper)
					return userCanceledErr;
			}
			else if (mapper == NULL)
			{
				if (!GetMacFileTypesFromMimeHeader(fRequest, &fileCreator, &fileType))
				{
					if (fIntention != FO_SAVE_AS)
					{
						mapper = CreateUnknownMimeTypeMapper (fRequest);
						if (!mapper)
							return userCanceledErr;
					}
					else
					{
						fileType = 'TEXT';
						fileCreator = emSignature;
					}
				}		
			}
		}
	}
	
	//
	// If fHandler is NULL, then fileType and fileCreator have been set
	// from the mime header via GetMacFileTypesFromMimeHeader.
	// 
	// When fHandler is NULL, appName will be empty because we're not
	// doing anything further to match the file to its application
	//
	fHandler = mapper;
	if (fHandler != NULL)
	{
		fileCreator	= fHandler->GetAppSig();
		fileType	= fHandler->GetDocType();
		appName		= fHandler->GetAppName();
	}
	
	// Additional little hack to make sure if we've got a text/html type file and we're supposed
	// to view it internally we set it to our creator code
	if (strcmp(fRequest->content_type,"text/html") == 0 &&
		mapper->GetLoadAction() == CMimeMapper::Internal)
	{
		fileType = 'TEXT';
		fileCreator = emSignature;
	}
	
	Assert_(fileCreator != (OSType) 0 && fileType != (OSType) 0);
	
	
	//
	// Figure out the appropriate file type and creator
	//
	
	err = GetFileSpec ();
	if (err)
		return err;	
	// Make a file object, file stream, and stream buffer.
	Try_
	{
		// Open an existing file before creating because for drag&drop we will have
		// created a Bookmark file as a placeholder. For all other cases, we have
		// a unique filespec.
		fFile->OpenDataFork (fsRdWrPerm);
		err = CFileMgr::SetFileTypeCreator(fileCreator, fileType, &fDestination);
		ThrowIfOSErr_ (err);
	}
	Catch_ (openErr)
	{
		if (openErr == fnfErr) {
			fFile->CreateNewDataFile (fileCreator, fileType, smSystemScript);
			fFile->OpenDataFork (fsRdWrPerm);
		}
		else
			Throw_ (openErr);
	}
	EndCatch_
	
	if (fFile && fRequest && fRequest->address)
	{
		FSSpec macSpec;
		fFile->GetSpecifier(macSpec);
		CFileMgr::FileSetComment (macSpec, CStr255(fRequest->address));
	}
	CStr255 message;
	StringHandle messageHdl = ::GetString(SAVE_QUOTE_RESID);
	
	if(messageHdl && *messageHdl) {
		::HLock((Handle) messageHdl);
		CopyString((unsigned char *) message, (const unsigned char *) *messageHdl);
		::HUnlock((Handle) messageHdl);
	}
	message += CStr255(fDestination.name);
	message += "\"";
	
	CContextProgress* theProgress = mContext->GetCurrentProgressStats();
	Assert_(theProgress != NULL);
	StSharer theShareLock(theProgress);

	theProgress->mAction = message;

	if (LaunchWhenDone()) {
	
		messageHdl = ::GetString(WILL_OPEN_WITH_RESID);
		if(messageHdl && *messageHdl) {
			::HLock((Handle) messageHdl);
			CopyString((unsigned char *) message, (const unsigned char *) *messageHdl);
			::HUnlock((Handle) messageHdl);
		}
		message += appName;
		message += GetCString(WILL_OPEN_TERM_RESID);
	}
	else {
		message = "";
		messageHdl = ::GetString(SAVE_AS_A_RESID);
		if(messageHdl && *messageHdl) {
			::HLock((Handle) messageHdl);
			CopyString((unsigned char *) message, (const unsigned char *) *messageHdl);
			::HUnlock((Handle) messageHdl);
		}
		message += appName;
		message += GetCString(FILE_RESID);
	}

	theProgress->mComment = message;
	mContext->UpdateCurrentProgressStats();
		
    if( fRequest->server_can_do_byteranges || fRequest->server_can_do_restart )
        fRequest->must_cache = TRUE;

	return noErr;
}

int
DownloadFilePipe::Write (const char *buffer, int len)
{
	int written = 0;
	Try_
	{
		if (TranslateLinefeeds()) {
			for (int i = 0; i < len; i++) {
				char c = buffer[i];
				if (c == LF)
					c = CR;
				fFile->WriteData (&c, 1);
			}
			written = len;
		}
		else
			written = fFile->WriteData (buffer, len);
	}
	Catch_ (writeErr)
	{
		CStr255 message = (const char*) GetCString(COULD_NOT_SAVE_RESID);
		message += CStr63(fDestination.name);
		if (writeErr == dskFulErr)
			message += CStr255((const char*)GetCString(DISK_FULL_RESID));
		else
			message += CStr255((const char*)GetCString(DISK_ERR_RESID));
		ErrorManager::PlainAlert(message);
		return MK_DISK_FULL;
	}
	EndCatch_
	
	return written;
}

//	Close the file
//	Launch the helper application if necessary
void DownloadFilePipe::Complete ()
{
	if (fRequest)
		GH_UpdateGlobalHistory(fRequest);
	if (fFile)
	Try_
	{
		fFile->CloseDataFork();
		CFileMgr::UpdateFinderDisplay(fDestination);
	}
	Catch_(inErr){}
	EndCatch_

    NET_RemoveURLFromCache(fRequest);
    NET_RemoveDiskCacheObjects(0);

	Try_
	{
	if (LaunchWhenDone() && fHandler && fFile)
	
//		fHandler->LaunchFile(fFile, fRequest, fCppContext->GetContextUniqueID());
// FIX ME!!! we need to implement the unique ID stuff in the CNSContext
		fHandler->LaunchFile(fFile, fRequest, 69);
	}
	Catch_(inErr){}
	EndCatch_
}

void DownloadFilePipe::Abort (int /*reason*/)
{
	if (fFile)
	{
		Try_
		{
			fFile->CloseDataFork();
		}
		Catch_(inErr)
		{}
		EndCatch_
		FSSpec fileSpec;
		fFile->GetSpecifier (fileSpec);
		FSpDelete (&fileSpec);
	}
	fDeleteOnDestroy = true;
}

/*-----------------------------------------------------------------------------
	C++ URL Streams
	
	Streams are responsible for reading data in to memory/disk
	They will also get "hinting" information and be responsible for
	ultimately deciding where to direct the data.
	
	display_converter and Pipe::Handle* are called from C code. They cannot
	pass any exceptions up.
-----------------------------------------------------------------------------*/

int Pipe::HandleProcess (NET_StreamClass *stream, const char *buffer, int32 buffLen)
{
	void *streamData=stream->data_object;
	volatile int result = -1;
	Try_ {
		Pipe *self = (Pipe*) streamData;
		result = self->Write (buffer, buffLen);
	}
	Catch_ (err) {
		XP_TRACE(("*** Caught an error (%i) in Pipe::HandleProcess", err));
		result = -1;
	}
	EndCatch_
	return result;
}

void Pipe::HandleComplete (NET_StreamClass *stream)
{
	void *streamData=stream->data_object;
	Try_ {
		Pipe *self = (Pipe*) streamData;
		self->Complete ();
		delete self;
	}
	Catch_ (err) {
		XP_TRACE(("*** Caught an error (%i) in Pipe::HandleComplete but can't return it", err));
	}
	EndCatch_
}

void Pipe::HandleAbort (NET_StreamClass *stream, int reason)
{
	void *streamData=stream->data_object;
	Try_ {
		XP_TRACE(("*** HandleAbort %i", reason));
		Pipe *self = (Pipe*) streamData;
		self->Abort (reason);
		delete self;
	}
	Catch_ (err) {
		XP_TRACE(("*** Caught an error (%i) in Pipe::HandleAbort but can't return it", err));
	}
	EndCatch_
}

unsigned int Pipe::HandleWriteReady (NET_StreamClass * /*stream*/)
{
	return 32765;
}

//
//
//

OSErr
Pipe::Open ()
{
	return noErr;
}

int
Pipe::Write (const char */*buffer*/, int buffLen)
{
	return buffLen;
}

void 
Pipe::Complete ()
{
}

void
Pipe::Abort (int /*reason*/)
{
}

//
// api
//

NET_StreamClass * 
Pipe::MakeStreamObject (char * name, MWContext * context)
{
	NET_StreamClass * stream = NET_NewStream (
			name, 
			Pipe::HandleProcess,
			Pipe::HandleComplete, 
			Pipe::HandleAbort, 
			Pipe::HandleWriteReady, 
			this, 
			context);
	return stream;
}

//
// Construction & Stuff
//

Pipe::Pipe ()
{
}

Pipe::~Pipe ()
{
}


CMimeMapper * CreateUnknownMimeTypeMapper(URL_Struct * request);
// After receiving the request for unknown mime type, this routine is called
// to create a mapper for an unknown MIME type
CMimeMapper* CreateUnknownMimeTypeMapper( URL_Struct* request )
{
	CStr255		fileName;
	CStr255		content_type;
	CStr31		newName;
	int			result;
	CMimeMapper*	mapper = NULL;
	
	if (!ErrorManager::TryToInteract(900))
		return NULL;
	// Notify the user with nice names
	newName = CFileMgr::FileNameFromURL( request->address );
	fileName = newName;
	content_type = NET_URLStruct_ContentType( request );
	
	::ParamText( newName, content_type, CStr255::sEmptyString, CStr255::sEmptyString );

	UDesktop::Deactivate();
	result = ::CautionAlert( ALRT_UnknownMimeType, NULL );
	UDesktop::Activate();
	
	// What does user want?
	switch ( result )
	{
		case ALRT_UnknownMimeType_Cancel:
			return NULL;
		case ALRT_UnknownMimeType_Save:
			mapper = CPrefs::CreateDefaultUnknownMapper( NET_URLStruct_ContentType(request), TRUE );
			mapper->SetLoadAction( CMimeMapper::Save );
			return mapper;
		case ALRT_UnknownMimeType_PickApp:
			StandardFileReply reply;
			CFilePicker::DoCustomGetFile( reply, CFilePicker::Applications, FALSE );
			if ( reply.sfGood )
				mapper = CPrefs::CreateDefaultAppMapper( reply.sfFile,NET_URLStruct_ContentType(request), TRUE );
			return mapper;
		case ALRT_UnknownMimeType_MoreInfo:
		{
			char* string;
			if (PREF_CopyConfigString("internal_url.more_info_plugin.url", &string) != PREF_NOERROR)
				return NULL;
				
			cstring url;
			url = (const char*) string;		// cstringÕs operator= expects unsigned char* to be a P string, so we have to cast
			url += "?";
			url += NET_URLStruct_ContentType( request );

			AppleEvent getURLEvent;
			UAppleEventsMgr::MakeAppleEvent( AE_url_suite, AE_url_getURL, getURLEvent );

			OSErr err = ::AEPutParamPtr( &getURLEvent, keyDirectObject, typeChar, url.data(), strlen(url.data()) );
			
			if ( err == noErr )
				err = ::AEPutParamPtr( &getURLEvent, AE_url_getURLname, typeChar, url.data(), strlen(url.data()) );

			if ( err == noErr )
				UAppleEventsMgr::SendAppleEvent (getURLEvent );

			return NULL;
		}
		default:
			return NULL;
	}
	return NULL;	
}

NET_StreamClass * NewFilePipe (
	int 				format_out,
	void *				/*registration*/,
	URL_Struct * 		request,
	MWContext*			context)
{
	CNSContext* theContext = ExtractNSContext(context);
	Assert_(theContext != NULL);

	GH_UpdateGlobalHistory(request);

	// 97-06-15 pkc -- We really should make a function to determine if a URL is a
	// mail attachment instead of duplicating this code.
	const char* urlAddress = request ? NET_URLStruct_Address(request) : nil;
	Boolean isMailMessage = false;
	if (urlAddress)
		{
		// check to see if this is a mail or news messages
		if (!strncasecomp (urlAddress, "mailbox:", 8) || !strncasecomp (urlAddress, "news:", 5))
			{
				{
				// this is a mail message
				isMailMessage = true;
				}
			}
		}

	// 97-06-15 pkc -- The bug is that when you close the browser
	// window for the mail message, the MimeObject* in the context is freed. That pointer is also
	// referenced somewhere else and used by libnet. Whoops. So, since we're trying to ship, don't
	// spawn a seperate progress context for mail messages.
	// 97-08-15 sdagley - Removing check for theContext->IsCloneRequired() as it won't be set to true
	// in frames which is the cause of bug #75288.  The alternative fix is to change the CNSContext constructors
	// to initialize mRequiresClone to true instead of the current default of false.
	if (/*theContext->IsCloneRequired() &&*/ !isMailMessage)
		{
		CNSContext* theProgressContext = NULL;
		CDownloadProgressWindow* theProgressWindow = NULL;
		try
			{
			theProgressContext = new CNSContext(*theContext);
			StSharer theShareLock(theProgressContext);
			
			// 97-06-12 pkc -- Move call to NET_SetNewContext here so that if it fails
			// (which can happen if mocha calls us) we still use context
			if(NET_SetNewContext(request, *theProgressContext, EarlManager::DispatchFinishLoadURL) == 0)
				theContext = theProgressContext;

			theProgressWindow = dynamic_cast<CDownloadProgressWindow*>(LWindow::CreateWindow(WIND_DownloadProgress, LCommander::GetTopCommander()));
			ThrowIfNULL_(theProgressWindow);
			theProgressWindow->Show();
			
//			theProgressWindow->SetWindowContext(theProgressContext);
			theProgressWindow->SetWindowContext(theContext);
			// the window will be shown upon progress initialization
			
			}
		catch(...)
			{
			delete theProgressWindow;
			return NULL;
			}
		}

	DownloadFilePipe* thePipe = NULL;
	NET_StreamClass* theStream = NULL;

	try
		{
		// 97-06-05 pkc -- if theContext doesn't have ContextProgress, make one now
		if (!theContext->GetContextProgress())
			theContext->EnsureContextProgress();
		thePipe = new DownloadFilePipe(theContext, request);

		thePipe->fIntention = CLEAR_CACHE_BIT(format_out);	
		if (request->fe_data) 
			{
			thePipe->fDestination = *(FSSpec*)request->fe_data;
			thePipe->fHasDestination = true;
			XP_FREE(request->fe_data);
			}

		OSErr theErr = thePipe->Open();
		ThrowIfOSErr_(theErr);

		theStream = thePipe->MakeStreamObject("Download File", *theContext);
		thePipe->fInput = theStream;
		}
	catch (...)
		{
		if (thePipe != NULL)
			thePipe->Abort (-1);
		delete thePipe;

		XP_DELETE(theStream);
		theStream = NULL;
		}

	return theStream;
}


//
// Snarf the mac file type and creator out of the mime header.
//
// Returns true if valid data is found, false if not.
//
static Boolean
GetMacFileTypesFromMimeHeader(	const URL_Struct	*	fRequest, 
								OSType				*	fileCreator,
								OSType				*	fileType	 )
{
	*fileCreator 	= 0;
	*fileType 		= 0;
	
	Assert_(fRequest != NULL);
	
	if (	fRequest->x_mac_creator == NULL	||
			fRequest->x_mac_type	== NULL		)
	{
		return false;
	}
		
	*fileCreator = TextToOSType(fRequest->x_mac_creator);
	*fileType	 = TextToOSType(fRequest->x_mac_type);
	
	return (*fileCreator != 0 && *fileType != 0);
}	


static OSType
TextToOSType(	const char*	text)
{
	OSType	result = 0;
	UInt32	len;
	
	//
	// If the text is 4-characters long, we treat it
	// as a raw OSType.
	//
	// If it is 8-characters, it is hex-encoded.
	//
	// If it's not 4 or 8, we return 0;
	//
	
	len = strlen(text);
	
	if (len == 4)
		result =  * ((OSType *) text);
	else
	if (len == 8)
		sscanf(text, "%lx", &result);
	
	return result;
}						




