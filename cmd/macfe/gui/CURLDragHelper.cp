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

//
// Mike Pinkerton
// Netscape Communications
//
// A collection of classes for helping out with drag and drop. See header file
// for more information regarding usage.
//

#include "CURLDragHelper.h"
#include "CNetscapeWindow.h"
#include "libi18n.h"
#include "shist.h"
#include "resgui.h"
#include "ufilemgr.h"
#include "mkgeturl.h"
#include "CURLDispatcher.h"
#include "macutil.h"
#include "FSpCompat.h"


//
// DoDragSendData
//
// call after a drop. Handles sending the data or creating files.
//
void
CURLDragHelper :: DoDragSendData( const char* inURL, char* inTitle, FlavorType inFlavor, 
									ItemReference inItemRef, DragReference inDragRef )
{
	OSErr err = noErr;
	
	if (!inURL)
		return;
	
	switch (inFlavor)
	{
		case 'TEXT':
			{
				// Just send the URL text

				err = ::SetDragItemFlavorData(
												inDragRef,
												inItemRef,
												inFlavor,
												inURL,
												strlen(inURL),
												0);
				if (err != noErr)
					goto Cleanup;
			}
			break;
		
		case emBookmarkFileDrag:
			{
					// Get the target drop location
				AEDesc dropLocation;
				
				err = ::GetDropLocation(inDragRef, &dropLocation);
				if (err != noErr)
				{
					goto Cleanup;
				}
				
					// Get the directory ID and volume reference number from the drop location
				SInt16 	volume;
				SInt32 	directory;
				
				err = GetDropLocationDirectory(&dropLocation, &directory, &volume);
				//ThrowIfOSErr_(err);	
			
					// Ok, this is a hack, and here's why:  This flavor type is sent with the
					// FlavorFlag 'flavorSenderTranslated' which means that this send data routine
					// will get called whenever someone accepts this flavor.  The problem is that 
					// it is also called whenever someone calls GetFlavorDataSize().  This routine
					// assumes that the drop location is something HFS related, but it's perfectly
					// valid for something to query the data size, and not be a HFS derivative (like
					// the text widget for example).
					// So, if the coercion to HFS thingy fails, then we just punt to the textual
					// representation.
				if (err == errAECoercionFail)
				{
					::SetDragItemFlavorData(inDragRef, inItemRef,
							inFlavor, inURL, strlen(inURL), 0);
					err = noErr;
					goto Cleanup;
				}
		
				if (err != noErr)
				{
					goto Cleanup;
				}

					// Combine with the unique name to make an FSSpec to the new file
				FSSpec		prototypeFilespec;
				FSSpec		locationSpec;
				prototypeFilespec.vRefNum = volume;
				prototypeFilespec.parID = directory;
				err = CFileMgr::NewFileSpecFromURLStruct(inURL, prototypeFilespec, locationSpec);
				if (err && err != fnfErr) // need a unique name, so we want fnfErr!
					goto Cleanup;
				
				// Clean up the title for use as a file name, and stuff it in the spec...
				do
				{
					char* colonPos = XP_STRCHR(inTitle, ':');
					if (!colonPos)
						break;
					*colonPos = '/';
					
				} while (1);
				if (strlen(inTitle) > 31)
					inTitle[31] = '\0';
				*(CStr31*)(locationSpec.name) = inTitle;

				// Set the flavor data to our emBookmarkFileDrag flavor with an FSSpec to the new file.
				err = ::SetDragItemFlavorData(inDragRef, inItemRef, inFlavor, &locationSpec, sizeof(FSSpec), 0);
				if (err)
					goto Cleanup;
#define SAVE_SOURCE 0	
#if 0
				if (theElement->type == LO_IMAGE)
				{
					URL_Struct*		request;
					cstring			urlString;
					
					urlString = GetURLFromImageElement(mContext, (LO_ImageStruct*) theElement);
					if (urlString==NULL) break;
					request = NET_CreateURLStruct(urlString, NET_DONT_RELOAD);
					XP_MEMSET(&request->savedData, 0, sizeof(SHIST_SavedData));
					
					CURLDispatcher::DispatchToStorage(request, locationSpec);
					// XP_FREE(urlString);
					break;
				}
				else
#elif SAVE_SOURCE
// jrm 97/08/13. Currently turned off...
				{
					URL_Struct* request = NET_CreateURLStruct(inURL, NET_DONT_RELOAD);
					XP_MEMSET(&request->savedData, 0, sizeof(SHIST_SavedData));
					CURLDispatcher::DispatchToStorage(request, locationSpec);
					break;
				}
#else
// ... in favor of
				{
					// create file (same as dragging a bookmark).
					err = FSpCreateCompat(&locationSpec, emSignature, 'URL ', smSystemScript );
					if ( err )
						goto Cleanup;
					
					// open file
					short refNum;
					err = ::FSpOpenDFCompat( &locationSpec, fsRdWrPerm, &refNum );
					if ( err )
						goto Cleanup;
					
					// write out URL
					long actualSizeWritten = strlen(inURL);
					err = ::FSWrite(refNum, &actualSizeWritten, inURL);
					if ( err )
						goto Cleanup;
					const char foo = 0x0D;
					actualSizeWritten = sizeof(foo); 
					err = ::FSWrite(refNum, &actualSizeWritten, &foo);
					if ( err )
						goto Cleanup;

					// close file
					::FSClose( refNum );
					break;
				}
#endif
				
				// еее both blocks of the if/then/else above call break, so does this get called?  96-12-17 deeje
				CFileMgr::UpdateFinderDisplay(locationSpec);
			}
			break;
			
		case emBookmarkDrag:
			{
				string urlAndTitle(inURL);
/*
				if (theElement->type == LO_IMAGE)
				{
					urlAndTitle += "\r[Image]";
				}
				else if (theElement->type == LO_TEXT)
*/
				{
					urlAndTitle += "\r";
					string title;
					
					if (inTitle)
					{
						urlAndTitle += inTitle;
					}
					else
					{
						urlAndTitle += inURL;
					}
				}			
				
				// send url<CR>title with the null terminator
				::SetDragItemFlavorData(inDragRef, inItemRef, inFlavor, urlAndTitle.c_str(), urlAndTitle.length() + 1, 0);
			}
			break;	
				
		default:
			{
				err = cantGetFlavorErr;
			}
			break;
	}		
Cleanup:
	if (err)
		Throw_(err); // caught by PP handler
		
} // CURLDragHelper :: DoDragSendData


//
// MakeIconTextValid
//
// Use this to make sure that the caption for a dragged item is not too long, and if it
// is, middle truncates it to avoid the visual turdfest
//
string 
CURLDragHelper :: MakeIconTextValid ( const char* inText )
{
	const Uint8 kMaxTitleLength = 50;
	char finalText[100];
	const char* result = nil;
	
	if ( strlen(inText) > kMaxTitleLength ) {
		INTL_MidTruncateString ( 0, inText, finalText, kMaxTitleLength );
		result = finalText;
	}
	else
		result = inText;
		
	return result;
	
} // MakeIconTextValid


#pragma mark -


//
// CURLDragMixin constructor
//
// Fill in the vector of acceptable drag flavors in this order:
//	- proxy icon (title & url)
//	- TEXT from some other app (should contain a URL, but doesn't have to)
//	- file from desktop
//
CURLDragMixin :: CURLDragMixin ( )
	: mAcceptableFlavors(4)
{
	static const FlavorType checkFor[] = { emBookmarkDrag, 'TEXT', flavorTypeHFS, flavorTypePromiseHFS };
	AcceptedFlavors().assign(checkFor, &checkFor[sizeof(checkFor) - 1]);
	
} // constructor


//
// ReceiveDragItem
//
// Called for each item dropped on the table. Pass it off the the backend, etc to do the
// right thing for the kind dropped here.
//
void
CURLDragMixin :: ReceiveDragItem ( DragReference inDragRef, DragAttributes /*inDragAttrs*/,
											ItemReference inItemRef, Rect & /*inItemBounds*/ )
{
	try {
		FlavorType useFlavor;
		FindBestFlavor ( inDragRef, inItemRef, useFlavor );
		Size theDataSize = 0;
		switch ( useFlavor ) {
		
			case emBookmarkDrag:
			{
				OSErr err = ::GetFlavorDataSize(inDragRef, inItemRef, emBookmarkDrag, &theDataSize);
				if ( err && err != badDragFlavorErr )
					throw(err);
				else if ( theDataSize ) {
					vector<char> urlAndTitle ( theDataSize + 1 );
					try {
						ThrowIfOSErr_( ::GetFlavorData( inDragRef, inItemRef, emBookmarkDrag,
														urlAndTitle.begin(), &theDataSize, 0 ) );
						urlAndTitle[theDataSize] = NULL;
						char* title = find(urlAndTitle.begin(), urlAndTitle.end(), '\r');
						if ( title != urlAndTitle.end() ) {
							title[0] = NULL;
							title++;
							char* url = urlAndTitle.begin();
							HandleDropOfPageProxy ( url, title );
						}
						else
							throw(0);
					}
					catch ( ... ) { 
						DebugStr ( "\pError getting flavor data for proxy drag" );
					}
				}
			}
			break;
				
			case flavorTypeHFS:
			case flavorTypePromiseHFS:
			{
				HFSFlavor theData;
				Boolean ignore1, ignore2;
				
				::GetFlavorDataSize(inDragRef, inItemRef, flavorTypeHFS, &theDataSize);
				OSErr anError = ::GetFlavorData(inDragRef, inItemRef, flavorTypeHFS, &theData, &theDataSize, nil);
				if ( anError == badDragFlavorErr )
					ThrowIfOSErr_( ::GetHFSFlavorFromPromise (inDragRef, inItemRef, &theData, true) );
				
				// if there's an error resolving the alias, the local file url will refer to the alias itself.
				::ResolveAliasFile(&theData.fileSpec, true, &ignore1, &ignore2);
				char* localURL = CFileMgr::GetURLFromFileSpec(theData.fileSpec);
				Assert_(localURL != nil);
				if ( localURL ) {
					HandleDropOfLocalFile ( localURL, CStr255(theData.fileSpec.name), theData );
					XP_FREE(localURL);
				}
			}
			break;
				
			case 'TEXT':
			{
				OSErr err = ::GetFlavorDataSize(inDragRef, inItemRef, 'TEXT', &theDataSize);
				if ( err && err != badDragFlavorErr )
					throw(err);
				else if ( theDataSize ) {
					vector<char> textData ( theDataSize + 1 );
					try {
						ThrowIfOSErr_( ::GetFlavorData( inDragRef, inItemRef, 'TEXT',
														textData.begin(), &theDataSize, 0 ) );
						HandleDropOfText ( textData.begin() );
					}
					catch ( ... ) {
						DebugStr("\pError getting flavor data for TEXT");
					}
				}
			}
			break;
			
			default:
				throw(-1);
				break;
		
		} // case of best flavor
	}
	catch ( ... ) {
		DebugStr ( "\pCan't find the flavor we want; g" );
	}
					
} // ReceiveDragItem


//
// FindBestFlavor
//
// Checks the item being dropped and returns the best flavor that we support. It does
// this by searching through the list of acceptable flavors, returning the first one
// supported by the drag item. Since the acceptable flavors are ordered by preference,
// the first one found is the best one.
//
bool
CURLDragMixin :: FindBestFlavor ( DragReference inDragRef, ItemReference inItemRef,
										FlavorType & oFlavor )
{
	// a function object which implements the body of the find_if() loop below. Returns
	// true if the flavor given is present in the drag item
	class FindBestFlavor_imp
	{
	public:
		FindBestFlavor_imp ( DragReference inDragRef, ItemReference inItemRef )
			: mDragRef(inDragRef), mItemRef(inItemRef) { } ;			
		bool operator() ( const FlavorType & inType )
		{
			FlavorFlags ignore;
			if ( ::GetFlavorFlags(mDragRef, mItemRef, inType, &ignore) == noErr )
				return true;
			return false;
		}
 	private:
 		DragReference mDragRef;
 		ItemReference mItemRef;
 	};
 	
	FlavorType* result = find_if ( AcceptedFlavors().begin(), AcceptedFlavors().end(), 
										FindBestFlavor_imp(inDragRef, inItemRef) );
	if ( result != AcceptedFlavors().end() ) {
		oFlavor = *result;
		return true;
	}
	return false;
		
} // FindBestFlavor


#pragma mark -


//
// CHTAwareURLDragMixin constructor
//
// As the name implies, this version of the URLDragMixin class knows about HT, and thus
// HT_Resources. Add the HT_Resource drag flavor to the front of the acceptable flavors
// list, because we want to use that first if it is present over all other drag flavors
//
CHTAwareURLDragMixin :: CHTAwareURLDragMixin ( )
{
	AcceptedFlavors().insert ( AcceptedFlavors().begin(), 1, (FlavorType)emHTNodeDrag );

} // constructor


//
// ReceiveDragItem
//
// Overridden to handle the HT_Resource drag flavor before all the others
//
void
CHTAwareURLDragMixin :: ReceiveDragItem ( DragReference inDragRef, DragAttributes inDragAttrs,
											ItemReference inItemRef, Rect & inItemBounds )
{
	try {
		FlavorType useFlavor;
		FindBestFlavor ( inDragRef, inItemRef, useFlavor );
		Size theDataSize = 0;
		switch ( useFlavor ) {
		
			case emHTNodeDrag:
			{
				HT_Resource draggedNode = NULL;
				HT_Resource node = NULL;
				theDataSize = sizeof(void*);
				ThrowIfOSErr_(::GetFlavorData( inDragRef, inItemRef, emHTNodeDrag, &node, &theDataSize, 0 ));
				HandleDropOfHTResource ( node );
			}
			break;
	
			default:
				CURLDragMixin::ReceiveDragItem(inDragRef, inDragAttrs, inItemRef, inItemBounds);
				break;
		
		} // switch on best flavor
	}
	catch ( ... ) {
		DebugStr ( "\pCan't find the flavor we want; g" );	
	}

} // ReceiveDragItem