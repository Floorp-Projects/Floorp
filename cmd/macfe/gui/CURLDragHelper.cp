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
// Implementaion of a class that contains code common to classes that have to do drag and
// drop for items w/ urls (bookmarks, etc).
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
				cstring urlAndTitle(inURL);
/*
				if (theElement->type == LO_IMAGE)
				{
					urlAndTitle += "\r[Image]";
				}
				else if (theElement->type == LO_TEXT)
*/
				{
					urlAndTitle += "\r";
					cstring title;
					
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
				::SetDragItemFlavorData(inDragRef, inItemRef, inFlavor, urlAndTitle, strlen(urlAndTitle) + 1, 0);
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
cstring 
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

