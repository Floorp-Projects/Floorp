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

#include "CBrowserView.h"

	// need these for various routine calls
#include "CHTMLClickrecord.h"
#include "msgcom.h"
#include "proto.h"
#include "RandomFrontEndCrap.h"
#include "CBrowserContext.h"
#include "CBookmarksAttachment.h"
#include "ufilemgr.h"
#include "CURLDispatcher.h"
#include "CEditorWindow.h"	// cmd_EditFrame
#include "secnav.h"
#include "ntypes.h"
#include "shist.h"
#include "macutil.h"
#include "CViewUtils.h"
#include "CApplicationEventAttachment.h"
#include "uerrmgr.h"	// GetPString prototype
#include "CTargetFramer.h"

extern "C" {
#include "layout.h"
}

	// ¥¥¥ I don't want to include this, but I need the ID of the popup menu
#include "resgui.h"

#include "CMochaHacks.h"

class CBrowserViewDoDragReceiveMochaCallback;

void MochaDragDrop(MWContext* inContext, CBrowserViewDoDragReceiveMochaCallback * cb, Point inDragMouse, short inDragModifiers );
Boolean HasFTPUpload(CBrowserContext * context);


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
		
CBrowserView::CBrowserView(LStream* inStream)
	: CHTMLView(inStream)
{
}

CBrowserView::~CBrowserView()
{
}

void		
CBrowserView::MoveBy(
								Int32				inHorizDelta,
								Int32				inVertDelta,
								Boolean				inRefresh)
{
	Inherited::MoveBy(inHorizDelta, inVertDelta, inRefresh);
	if (mContext != NULL && (inHorizDelta || inVertDelta))
	{
		SPoint32 location;
		GetFrameLocation(location);
		CMochaHacks::SendMoveEvent(*mContext, location.h, location.v);
	}
}

/*	BeTarget and DontBeTarget enable and disable the drawing of the selection.
	This has the effect of not drawing the text as selected when the view is
	not currently the target.  Using LO_ClearSelection instead would have
	the same effect, but the selection wouldn't be recovered when the target
	is given back to the view (as in, when its window is deactivated then
	reactivated).
*/

// ---------------------------------------------------------------------------
//		¥ BeTarget
// ---------------------------------------------------------------------------

void
CBrowserView::BeTarget() {

	LO_HighlightSelection (*mContext, TRUE);
	ExecuteAttachments(CTargetFramer::msg_BecomingTarget, this);
}

// ---------------------------------------------------------------------------
//		¥ DontBeTarget
// ---------------------------------------------------------------------------

void
CBrowserView::DontBeTarget() {

//	LO_ClearSelection (*mContext);
	LO_HighlightSelection (*mContext, FALSE);
	ExecuteAttachments(CTargetFramer::msg_ResigningTarget, this);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- COMMANDER OVERRIDES ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserView::FindCommandStatus(CommandT inCommand,
									Boolean	&outEnabled,
									Boolean	&outUsesMark,
									Char16	&outMark,
									Str255	outName)
{
	if (::StillDown())
	{
		if (FindCommandStatusForContextMenu(inCommand, outEnabled,outUsesMark, outMark, outName))
			return;
	}
	MWContext *mwcontext;
	outUsesMark = false;
	
	CBrowserWindow	*browserWindow = dynamic_cast<CBrowserWindow *>(CViewUtils::GetTopMostSuperView(this));
	Boolean isHTMLHelp		= browserWindow ? browserWindow->IsHTMLHelp() : false;

// Restricted targets are Netcaster things, right?
#ifdef MOZ_NETCAST
	// Disable commands which don't apply to restricted targets, at the request of javascript.
	// No special distinction needed for floaters here.
	if (browserWindow && 
		browserWindow->CommandsAreDisabled() &&
		inCommand == cmd_EditFrame)
	{
		outEnabled = false;
		return;
	}
#endif // MOZ_NETCAST
	switch (inCommand)
	{
#ifdef EDITOR
		case cmd_EditFrame:
			mwcontext = *GetContext();
			if (mwcontext && !(XP_IsContextBusy(mwcontext)) && !Memory_MemoryIsLow())
			{
				// we must have a frameset if this context has a non-grid parent
				if ( XP_GetNonGridContext( mwcontext ) != mwcontext )
					outEnabled = true && !isHTMLHelp;
			}
			break;
#endif // EDITOR

//		case cmd_SecurityInfo:
//		case cmd_DocumentInfo:
//			outEnabled = true;
//			break;
			
		case cmd_FTPUpload:
			outEnabled = HasFTPUpload( GetContext() ) && !isHTMLHelp;
			break;

		default:
			Inherited::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			break;
	}
}

Boolean	CBrowserView::ObeyCommand(CommandT inCommand, void* ioParam)
{
	Boolean cmdHandled = false;
	
	switch (inCommand)
	{
#ifdef EDITOR
		case cmd_EditFrame:
			CEditorWindow::MakeEditWindowFromBrowser( *GetContext() );
			break;
#endif // EDITOR
		
//		case cmd_SecurityInfo:
//			mwcontext = *GetContext();
//			if (mwcontext)
//			{
//				URL_Struct* url =
//					SHIST_CreateURLStructFromHistoryEntry(
//						mwcontext,
//						GetContext()->GetCurrentHistoryEntry()
//						);
//				if (url)
//					SECNAV_SecurityAdvisor(mwcontext,url);
//			}
//			break;

		case cmd_FTPUpload:
			{
				// NOTE - 	the backend currently only supports uploading the data fork
				//			of a file.  A planned feature of 5.0 is to support MacBinary
				//			encoding during the upload process.  To this end the routine
				//			UStdDialogs::AskUploadAsDataOrMacBin has been created to prompt
				//			for the desired upload mode in place of the call to
				//			StandardGetFile below.
				FSSpec spec;
				History_entry * he = GetContext()->GetCurrentHistoryEntry();
				char * fileURL  = CFileMgr::EncodedPathNameFromFSSpec(spec, TRUE);
				OSType myTypes[3];
				StandardFileReply myReply;
				UDesktop::Deactivate();
				StandardGetFile(NULL, -1, myTypes, &myReply);
				UDesktop::Activate();
				if (myReply.sfGood)
				{
					URL_Struct * request = NET_CreateURLStruct(he->address, NET_SUPER_RELOAD);
					char ** newFileList = (char **) XP_ALLOC(2 * sizeof(char*));
					ThrowIfNil_(request);
					ThrowIfNil_(newFileList);
					request->method = URL_POST_METHOD;

					newFileList[0] = CFileMgr::EncodedPathNameFromFSSpec(myReply.sfFile, TRUE);
					ThrowIfNil_(newFileList[0]);
					newFileList[1] = NULL;
					request->files_to_post = newFileList;
					GetContext()->SwitchLoadURL( request, FO_CACHE_AND_PRESENT );
				}
			}
			break;

//		case cmd_DocumentInfo:
//			URL_Struct* aboutURL = NET_CreateURLStruct( "about:document", NET_DONT_RELOAD );
//			if ( aboutURL )
//				CURLDispatcher::GetURLDispatcher()->DispatchToView(mContext, aboutURL, FO_CACHE_AND_PRESENT, false, 1010, false);
//			break;
		
		default:
			cmdHandled = Inherited::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return cmdHandled;
}



// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- CONTEXTUAL POPUP ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CBrowserView::FindCommandStatusForContextMenu(
	CommandT inCommand,
	Boolean	&outEnabled,
	Boolean	&/*outUsesMark*/,
	Char16	&/*outMark*/,
	Str255	/*outName*/)
{
	// We come here only if a CContextMenuAttachment is installed. (This is the new way).
	// Return true if we want to bypass standard status processing.
	if (!mCurrentClickRecord)
		return false;
	CHTMLClickRecord& cr = *mCurrentClickRecord; // for brevity.
	Boolean isAnchor = FALSE;
	Boolean isImage = FALSE;
	if (cr.mElement)
	{
		isAnchor = cr.IsAnchor();
		isImage = cr.mElement->type == LO_IMAGE
				&& NET_URL_Type(mContext->GetCurrentURL()) != FTP_TYPE_URL;
	}
	
	CBrowserWindow	*browserWindow = dynamic_cast<CBrowserWindow *>(CViewUtils::GetTopMostSuperView(this));
	Boolean isHTMLHelp = browserWindow ? browserWindow->IsHTMLHelp() : false;
	// We're overloading command disabling to handle menu items too.
	Boolean commandsAreDisabled = browserWindow ? browserWindow->CommandsAreDisabled() : false;

	switch (inCommand)
	{
		case cmd_NEW_WINDOW_WITH_FRAME:
			outEnabled = ((MWContext (*mContext)).is_grid_cell
								&& (mContext->GetCurrentHistoryEntry() != NULL)) && !isHTMLHelp;
			return true;
		case cmd_OPEN_LINK:
			outEnabled = isAnchor && !commandsAreDisabled;
			return true;
		case cmd_AddToBookmarks:
			if (isAnchor)
				outEnabled = !isHTMLHelp;
			return true; // for now, only have a "add bookmark for this link" command.
//			return isAnchor; // we don't know unless it's an anchor
		case cmd_NEW_WINDOW:
#ifdef MOZ_MAIL_NEWS
			Boolean newWindowProhibited = ::MSG_NewWindowProhibited(*mContext, cr.mClickURL);
#else
			// MSG_NewWindowProhibited doesn't exist in a non-mail/news world so just say
			// we don't prohibit creating a new window
			Boolean newWindowProhibited = false;
#endif // MOZ_MAIL_NEWS
			outEnabled = !newWindowProhibited && isAnchor && !isHTMLHelp;
			return true;
		case cmd_SAVE_LINK_AS:
			outEnabled = isAnchor && !IsMailToLink(cr.mClickURL) && !isHTMLHelp;
			return true; // we don't know unless it's an anchor
		case cmd_COPY_LINK_LOC:
			outEnabled = (isAnchor && mCurrentClickRecord->mClickURL.length() > 0) && !isHTMLHelp;
			return true;
		case cmd_VIEW_IMAGE:
			outEnabled = isImage && !isHTMLHelp && !commandsAreDisabled;
			return true;
		case cmd_SAVE_IMAGE_AS:
		case cmd_COPY_IMAGE:
		case cmd_COPY_IMAGE_LOC:
			outEnabled = isImage && !isHTMLHelp;
			return true;
		case cmd_LOAD_IMAGE:
			outEnabled = isImage && !commandsAreDisabled;
			return true;
	}
	return false; // we don't know about it
} // CBrowserView::FindCommandStatusForContextMenu


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- DRAG AND DROP ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Drag and drop is now reflected into javascript. On drag drop, the browser view
// packages the URLs received into a new event and sends it to javascript with a
// callback. If javascript lets us have the drop, the callback dispatches the URLs.



Boolean CBrowserView::DragIsAcceptable(DragReference inDragRef)
{
	Boolean			doAccept = false;
	DragAttributes	theAttributes = {0};
	
	(void) GetDragAttributes(inDragRef, &theAttributes);
	doAccept = !(theAttributes & kDragInsideSenderWindow);
	
	if (doAccept)
	{
		doAccept = Inherited::DragIsAcceptable(inDragRef);
	}
	
	return doAccept;
}



Boolean CBrowserView::ItemIsAcceptable(DragReference inDragRef, ItemReference inItemRef)
{
	FlavorFlags		theFlags;
	Boolean			doAccept = false;
	
	doAccept |= (::GetFlavorFlags(inDragRef, inItemRef, 'TEXT', &theFlags) == noErr);
	doAccept |= (::GetFlavorFlags(inDragRef, inItemRef, flavorTypeHFS, &theFlags) == noErr);
	 
	return doAccept;
}



/*
void CBrowserView::ReceiveDragItem(DragReference inDragRef,
								DragAttributes inDragAttrs,
								ItemReference inItemRef,
								Rect& inItemBounds)			// In local coordinates
{
	FlavorFlags		theFlags;
	OSErr			anError = noErr;
	URL_Struct*		request = nil;
	
	anError = ::GetFlavorFlags(inDragRef, inItemRef, 'TEXT', &theFlags);
	if (anError == noErr)
	{
		long		theDataSize = 0;
		char*		theData = nil;
		
		::GetFlavorDataSize(inDragRef, inItemRef, 'TEXT', &theDataSize);
		if (theDataSize > 255)				// ¥¥¥Êare there any limits to the URL string length?
		{
			theDataSize = 255;
		}
		
		theData = (char*) ::NewPtr(theDataSize+1);
		// assert(theData != nil);
		
		::GetFlavorData(inDragRef, inItemRef, 'TEXT', theData, &theDataSize, nil);
		theData[theDataSize] = 0;
		
		request = NET_CreateURLStruct(theData, NET_DONT_RELOAD);
		// assert(request != nil);
		
		::DisposePtr(theData);
	}

	anError = ::GetFlavorFlags(inDragRef, inItemRef, flavorTypeHFS, &theFlags);
	if (anError == noErr)
	{
		long		theDataSize = 0;
		HFSFlavor	theData;
		char*		localURL;
		
		::GetFlavorDataSize(inDragRef, inItemRef, flavorTypeHFS, &theDataSize);
		::GetFlavorData(inDragRef, inItemRef, flavorTypeHFS, &theData, &theDataSize, nil);
		localURL = CFileMgr::GetURLFromFileSpec(theData.fileSpec);
		// assert(localURL != nil);
		
		request = NET_CreateURLStruct(localURL, NET_DONT_RELOAD);
		// assert(request != nil);
		
		XP_FREE(localURL);
	}
	
	if (request != nil)
	{
		XP_MEMSET(&request->savedData, 0, sizeof(SHIST_SavedData));
		CURLDispatcher::GetURLDispatcher()->DispatchToView(mContext, request, FO_CACHE_AND_PRESENT, false);
	}
}
*/

/*
 * CBrowserViewDoDragReceiveMochaCallback
 * callback for DoDragReceive
 */
class CBrowserViewDoDragReceiveMochaCallback : public CMochaEventCallback
{
public:
	CBrowserViewDoDragReceiveMochaCallback( CBrowserView * view, XP_List* inRequestList, char** inURLArray)
		: fBrowserView(view), fRequestList(inRequestList), fURLArray(inURLArray) {}

	virtual void Complete(MWContext *, 
				LO_Element * , int32 /*type*/, ETEventStatus status)
	{
		if (status == EVENT_OK)
		{
			fBrowserView->DoDragReceiveCallback(fRequestList );
		}
		else if (status == EVENT_PANIC || status == EVENT_CANCEL)
		{
			// free the URL_Structs we created in DoDragReceive since they weren't dispatched.
			for (int i = 1; i <= XP_ListCount(fRequestList); i++)
			{
				URL_Struct* request = (URL_Struct *)XP_ListGetObjectNum(fRequestList, i);
				NET_FreeURLStruct(request);
			}
		}
		XP_ListDestroy(fRequestList);
		XP_FREE(fURLArray);
	}
	
	int LengthURLArray() { return XP_ListCount(fRequestList); }
	
	char** GetURLArray() { return fURLArray; }
	
private:
	CBrowserView * fBrowserView;
	XP_List* fRequestList;
	char** fURLArray;
};

// This is handled through a callback from javascript now.
// Creates a list of the URL_Structs to process in the callback because the DragReference
// will no longer be valid.
void			
CBrowserView::DoDragReceive(DragReference	inDragRef)
{
	// This gets freed in the callback.
	XP_List* requestList = XP_ListNew();
	if ( !requestList ) return;
	
	FlavorFlags		theFlags;
	OSErr			anError = noErr;
	URL_Struct*		request = nil;
	
	Uint16	itemCount;				// Number of Items in Drag
	::CountDragItems(inDragRef, &itemCount);
	
	for (Uint16 item = 1; item <= itemCount; item++) 
	{
		ItemReference	itemRef;
		::GetDragItemReferenceNumber(inDragRef, item, &itemRef);
		
		// First check if this has a TEXT flavor. If so, this is either a URL dragged from 
		// another app or a text clipping dragged from the desktop. Either way, we need
		// to guarantee that we don't try below to read the drop as an HFSFlavor
		// (which a clipping also has).
		anError = ::GetFlavorFlags(inDragRef, itemRef, 'TEXT', &theFlags);
		if (anError == noErr)
		{
			long		theDataSize = 0;
			char*		theData = nil;
			
			::GetFlavorDataSize(inDragRef, itemRef, 'TEXT', &theDataSize);
			if (theDataSize > 255)				// ¥¥¥Êare there any limits to the URL string length?
				theDataSize = 255;
			
			theData = (char*) ::NewPtr(theDataSize+1);
			// assert(theData != nil);
			
			if (theData != NULL)
			{
				::GetFlavorData(inDragRef, itemRef, 'TEXT', theData, &theDataSize, nil);
				theData[theDataSize] = 0;
			
				request = NET_CreateURLStruct(theData, NET_DONT_RELOAD);
				if (request != NULL) XP_ListAddObjectToEnd(requestList, request);
				// assert(request != nil);
			
				::DisposePtr(theData);
			}
			
		} // if has TEXT flavor
		else
		{
			// if this item was not a clipping or dragged text, then process it as an HTML
			// file
			long		theDataSize = 0;
			HFSFlavor	theData;
			char*		localURL;
			Boolean		ignore1, ignore2;
			
			::GetFlavorDataSize(inDragRef, itemRef, flavorTypeHFS, &theDataSize);
			anError = ::GetFlavorData(inDragRef, itemRef, flavorTypeHFS, &theData, &theDataSize, nil);
			if (anError == badDragFlavorErr)
				anError = GetHFSFlavorFromPromise (inDragRef, itemRef, &theData, true);
			
			if (anError == noErr)
			{
				if (HasFTPUpload(GetContext()))
				{
					History_entry * he = GetContext()->GetCurrentHistoryEntry();

					if (!request)
					{
						request = NET_CreateURLStruct(he->address, NET_SUPER_RELOAD);
						request->method = URL_POST_METHOD;
					}
					
					QueueFTPUpload(theData.fileSpec, request);
				}
				else
				{
					// if there's an error resolving the alias, the local file url will refer to the alias itself.
					::ResolveAliasFile(&theData.fileSpec, true, &ignore1, &ignore2);
					localURL = CFileMgr::GetURLFromFileSpec(theData.fileSpec);
					// assert(localURL != nil);
					
					if (localURL != NULL)
					{
						request = NET_CreateURLStruct(localURL, NET_DONT_RELOAD);
						if (request != NULL) XP_ListAddObjectToEnd(requestList, request);
						// assert(request != nil);
						
						XP_FREE(localURL);
					}
				}

			} // if we are sure this is a file.
			
		} // if not a clipping or text
		
	} // for each drag item

	// After all that, do we have any files to post?
	if (HasFTPUpload(GetContext()) && request->files_to_post)
		XP_ListAddObjectToEnd(requestList, request);

	if (requestList != NULL)
	{
		int numItems = XP_ListCount(requestList);
		int size = sizeof(char*) * (numItems + 1);
		char ** urlArray = (char**)XP_ALLOC(size);
		XP_ASSERT(urlArray);
		
		int i;
		for (i = 0; i < numItems; i++)
		{
			// the URL_Struct already has the url allocated so use that.
			URL_Struct* request = (URL_Struct *)XP_ListGetObjectNum(requestList, i+1); // XP_Lists are 1-indexed
			urlArray[i] = request->address;
		}
		urlArray[i] = 0;
		
		Point dragMouse;
		short dragModifiers;
		
		::GetDragMouse(inDragRef, &dragMouse, NULL);
		::GetDragModifiers(inDragRef, &dragModifiers, NULL, NULL);
		MochaDragDrop(*mContext,  
						new CBrowserViewDoDragReceiveMochaCallback(this, requestList, urlArray),
						dragMouse,
						dragModifiers);
						
	} // if requestList has things to process

} // CBrowserView :: DoDragReceive


// Dispatch the urls collected in DoDragReceive.
// This is executed if javascript lets us have the event.
void			
CBrowserView::DoDragReceiveCallback(XP_List* inRequestList)
{
	for (int i = 1; i <= XP_ListCount(inRequestList); i++)
	{
		URL_Struct* request = (URL_Struct *)XP_ListGetObjectNum(inRequestList, i);
		if (request != nil)
		{
			XP_MEMSET(&request->savedData, 0, sizeof(SHIST_SavedData));
			//CURLDispatcher::GetURLDispatcher()->DispatchToView(mContext->GetTopContext(mContext),
					//request, FO_CACHE_AND_PRESENT, false);
			
			// We need this to be delayed so that we can confirm ftp drag & drop since we can't confirm
			//inside the drag handler
//			CURLDispatcher::GetURLDispatcher()->DispatchToView(mContext->GetTopContext(mContext),
					//request, FO_CACHE_AND_PRESENT, false, 1010, true);
			CURLDispatcher::DispatchURL(request, mContext->GetTopContext(), true);
		}
	}
}

// Mocha allows drag receive to be cancelled, so send a dragdrop message.
void 
MochaDragDrop(MWContext* inContext, CBrowserViewDoDragReceiveMochaCallback * cb, Point inDragMouse, short inDragModifiers)
{
	JSEvent* event = XP_NEW_ZAP(JSEvent);
	
	if (event != NULL)
	{	
		// event's data field is an array of urls.
		
		event->type = EVENT_DRAGDROP;
		event->data = (void *)cb->GetURLArray();
		event->dataSize = cb->LengthURLArray();
		event->screenx = inDragMouse.h;
		event->screeny = inDragMouse.v - ::GetMBarHeight();
		event->modifiers = CMochaHacks::MochaModifiers(inDragModifiers);
		ET_SendEvent( inContext, 0, event, CMochaEventCallback::MochaCallback, cb );
	}
}


// True if we might be able to do FTP upload.
Boolean HasFTPUpload(CBrowserContext * context)
{
	History_entry * he = context->GetCurrentHistoryEntry();
	return (he &&
			!strncasecomp(he->address, "ftp", 3) &&
			he->address[strlen(he->address)-1] == '/' );
}


// Adds file for FTP file upload	
void CBrowserView::QueueFTPUpload(const FSSpec & spec, URL_Struct* request)
{
	FSSpec theResolvedSpec = spec;
	Boolean isFolder;
	Boolean wasAliased;

	// If we drop the whole HD, allow bailout
	if (CmdPeriod())	
		return;

	// If it's an alias to a folder - skip it!
	if (::ResolveAliasFile(&theResolvedSpec, TRUE, &isFolder, &wasAliased) == noErr && isFolder && wasAliased)
		return;
	
	// If it is a folder, add it recursively
	if (CFileMgr::IsFolder(spec))
	{
		FSSpec newSpec;
		FInfo 	dummy;
		Boolean dummy2;
		CFileIter	iter((FSSpec&)spec);
		while (iter.Next(newSpec, dummy, dummy2))
			QueueFTPUpload(newSpec, request);
		return;
	}
	
	// We want to use the resolved alias spec here so we get the right file name
	char * fileURL  = CFileMgr::EncodedPathNameFromFSSpec(theResolvedSpec, TRUE);
 		
 	// Add file argument to the URL struct (queued up as the fLatentRequest
	
		// Duplicate the current file list
		int newFilePos = 0;
		char ** newFileList;
		if (request->files_to_post)
			while (request->files_to_post[newFilePos] != NULL)
				newFilePos++;

		newFileList = (char **) XP_ALLOC((newFilePos+2) * sizeof(char*));
		ThrowIfNil_(newFileList);

		for (int i = 0; i < newFilePos; i++)
			newFileList[i] = request->files_to_post[i];
		
		// Add the new member
		newFileList[newFilePos] = fileURL;
		newFileList[newFilePos + 1] = NULL;
		
		if (request->files_to_post)
			XP_FREE(request->files_to_post);
			
		request->files_to_post = newFileList;
		
}
