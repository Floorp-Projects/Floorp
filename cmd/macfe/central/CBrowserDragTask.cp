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

//	CBrowserDragTask.cp



#include "CBrowserDragTask.h"

#include "resgui.h"

// ---------------------------------------------------------------------------
//		¥ CBrowserDragTask
// ---------------------------------------------------------------------------

CBrowserDragTask::CBrowserDragTask(
	const EventRecord&		inEventRecord)
	:	super(inEventRecord)
{
}	

// ---------------------------------------------------------------------------
//		¥ ~CBrowserDragTask
// ---------------------------------------------------------------------------
											
CBrowserDragTask::~CBrowserDragTask()
{
}	


//
// AddFlavorBookmark
//
// This flavor is currently used to shuttle around url/title information, and is
// used by external applications (like DragThing) to get info about bookmarks/etc.
// The data format is plain text in the form of URL<cr>Title.
//
// This flavor may or may not contain data. The proxy icon, for example, would want to 
// include the data so that if it is dropped in the NavCenter, the NC could 
// determine if the drop was allowable based on the URL.
//
void
CBrowserDragTask::AddFlavorBookmark(ItemReference inItemRef, const char* inData)
{
	OSErr theErr = ::AddDragItemFlavor(
									mDragRef,
									inItemRef,
									emBookmarkDrag,
									inData,			
									inData ? strlen(inData) + 1 : 0,
									flavorSenderTranslated );
	ThrowIfOSErr_(theErr);
}							


//
// AddFlavorBookmarkFile
//
// This flavor is used for creating a bookmark file in the Finder instead of a clipping when
// icons are dragged to the desktop.
//
// The data will be fulfilled in a DoSendData proc.
// 
void
CBrowserDragTask::AddFlavorBookmarkFile(ItemReference inItemRef)
{	
	// Promise a file of type emBookmarkFile
	
	PromiseHFSFlavor promise;
	
	promise.fileType 		= emBookmarkFile;
	promise.fileCreator 	= emSignature;
	promise.fdFlags 		= 0;
	promise.promisedFlavor	= emBookmarkFileDrag;

	// Promise to create a file for the emBookmark flavor, where the actual
	// FSSpec is promised in the emBookmark flavor below
	
	OSErr theErr = ::AddDragItemFlavor(
									mDragRef,
									inItemRef,
									flavorTypePromiseHFS,
									&promise,
									sizeof(PromiseHFSFlavor),
									0);
	ThrowIfOSErr_(theErr);
	
	theErr = ::AddDragItemFlavor(
									mDragRef,
									inItemRef,
									emBookmarkFileDrag,
									nil,
									0,
									flavorNotSaved | flavorSenderTranslated);
	ThrowIfOSErr_(theErr);
}	


//
// AddFlavorURL
//
// This flavor is used to communicate the current URL with other applications, such
// as text editors, etc. It is basically the 'TEXT' flavor.
//
// No data is sent with this flavor, relying on a DoDragSendData() to get it out later.
// This prevents us from running into an odd problem where the CTheadView class wants to
// interpret the data as something that it isn't. This won't happen when no data is sent.
//
void
CBrowserDragTask::AddFlavorURL(ItemReference inItemRef)
{
	// TEXT flavor (drag an URL within Netscape). Set flavorSenderTranslated
	// so that the Finder *won't* try to put this in a clipping file.
	// We'd rather save the file itself.

	OSErr theErr = ::AddDragItemFlavor(
									mDragRef,
									inItemRef,
									'TEXT',
									nil,
									0,
									flavorSenderTranslated);
	ThrowIfOSErr_(theErr);
}	

// ---------------------------------------------------------------------------
//		¥ AddFlavors
// ---------------------------------------------------------------------------

void
CBrowserDragTask::AddFlavors( DragReference inDragRef )
{		
// NOTE: I'm passing |this| as the item ref because that's the way it was in the past
//			and i don't want to break anything.
	AddFlavorBookmark(static_cast<ItemReference>(this));
	AddFlavorBookmarkFile(static_cast<ItemReference>(this));
	AddFlavorURL(static_cast<ItemReference>(this));
}