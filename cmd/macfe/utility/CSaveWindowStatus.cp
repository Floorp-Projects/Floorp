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

/*======================================================================================
	AUTHOR:			Ted Morris <tmorris@netscape.com> - 08 NOV 96

	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CSaveWindowStatus.h"

#include "uprefd.h"
#include "macutil.h"


#pragma mark -
/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

#pragma mark -
/*======================================================================================
	Find the global bounds for the pane.
======================================================================================*/

void CSaveWindowStatus::GetPaneGlobalBounds(LPane *inPane, Rect *outBounds) {

	inPane->CalcPortFrameRect(*outBounds);
	inPane->PortToGlobalPoint(topLeft(*outBounds));
	inPane->PortToGlobalPoint(botRight(*outBounds));
}


/*======================================================================================
	Move this window to an alert position on the main screen.
======================================================================================*/

void CSaveWindowStatus::MoveWindowToAlertPosition(LWindow *inWindow) {

	Rect bounds;
	
	GetPaneGlobalBounds(inWindow, &bounds);
	
	GDHandle dominantDevice;
	LWindow *frontWindowP = UDesktop::FetchTopRegular();
	
	if ( frontWindowP ) {
		dominantDevice = UWindows::FindDominantDevice(
							UWindows::GetWindowStructureRect(frontWindowP->GetMacPort()));
	} else {
		dominantDevice = ::GetMainDevice();
	}
	
	Rect screenRect = (**dominantDevice).gdRect;
	if ( dominantDevice == ::GetMainDevice() ) {
		screenRect.top += GetMBarHeight();
	}

	::OffsetRect(&bounds, screenRect.left + ((screenRect.right - screenRect.left - (bounds.right - bounds.left)) / 2) - bounds.left,
						  screenRect.top + ((screenRect.bottom - screenRect.top - (bounds.bottom - bounds.top)) / 3) - bounds.top);
	
	inWindow->DoSetBounds(bounds);
}


/*======================================================================================
	Move this window to a new position on the main screen.
======================================================================================*/

void CSaveWindowStatus::MoveWindowTo(LWindow *inWindow, Point inGlobalTopLeft) {

	Rect bounds;
	GetPaneGlobalBounds(inWindow, &bounds);
	
	::OffsetRect(&bounds, inGlobalTopLeft.h - bounds.left, 
						  inGlobalTopLeft.v - bounds.top);
	
	VerifyWindowBounds(inWindow, &bounds);
	inWindow->DoSetBounds(bounds);
}


/*======================================================================================
	Verify the specified bounds against the window min/max sizes and the desktop
	bounds.
======================================================================================*/
static
inline
Int16
pin( const Int16& lo, const Int16& x, const Int16& hi )
	{
		return (x<lo) ? lo : ((hi<x) ? hi : x);
	}


void CSaveWindowStatus::VerifyWindowBounds(LWindow *inWindow, Rect *ioGlobalBounds)
	/*
		Make sure the entire window is on-screen [for roughly rectangular desktops].

		|ioGlobalBounds| is the proposed content rect of the window, in global coordinates.  It
		is not necessarily related to the current actual content rect of the window.
	*/
{
			/*
				First, calculate the rectangle within which the windows content rect must fit,
				for that window to be entirely on-screen.  Note: we explicitly don't handle the
				Ôfunny desktopÕ case.  This may be required.
			*/
		Int16 desktopMinTop, desktopMaxBottom, desktopMaxWidth;
		Int16 desktopMinLeft, desktopMaxRight, desktopMaxHeight;
		{
				// The bounding box of Ôthe gray regionÕ is the rectangle we want our window to fit entirely within.
			Rect desktopRect = (**::GetGrayRgn()).rgnBBox;

				// Calculate how much the windows structure region ÔpadsÕ its content region...
			Rect structureRect	= UWindows::GetWindowStructureRect(inWindow->GetMacPort());
			Rect contentRect		= UWindows::GetWindowContentRect(inWindow->GetMacPort());

			Int16	topPadding		= contentRect.top - structureRect.top;
			Int16 bottomPadding	= structureRect.bottom - contentRect.bottom;
			Int16	leftPadding		= contentRect.left - structureRect.left;
			Int16	rightPadding	= structureRect.right - contentRect.right;

				// ...and shrink |desktopRect| accordingly (so it is relative to our content rect, instead of our structure rect).
			desktopRect.top			+= cWindowDesktopMargin + topPadding;
			desktopRect.bottom	-= cWindowDesktopMargin + bottomPadding;
			desktopRect.left		+= cWindowDesktopMargin + leftPadding;
			desktopRect.right		-= cWindowDesktopMargin + rightPadding;

			desktopMinTop			= desktopRect.top;										// windows content rect top must not be above this this
			desktopMaxBottom	= desktopRect.bottom;									//	...nor its bottom below this
			desktopMaxHeight	= desktopMaxBottom - desktopMinTop;		//	...nor its height greater than this

			desktopMinLeft		= desktopRect.left;										//	...
			desktopMaxRight		= desktopRect.right;
			desktopMaxWidth		= desktopMaxRight - desktopMinLeft;
		}

			/*
				Second, calculate the minimum and maximum size of the window based on its own min/max settings
				and the limits imposed by the screen.
			*/
		Int16 minHeight, maxHeight, minWidth, maxWidth;
		{
			Rect windowLimits;
			inWindow->GetMinMaxSize(windowLimits);

			minHeight = pin(0,					windowLimits.top,			desktopMaxHeight);
			maxHeight = pin(minHeight,	windowLimits.bottom,	desktopMaxHeight);
			minWidth	= pin(0,					windowLimits.left,		desktopMaxWidth);
			maxWidth	= pin(minWidth,		windowLimits.right,		desktopMaxWidth);
		}

			/*
				Third, pin the windows size to its calculated limits.
			*/
		Int16 height	= pin(minHeight, ioGlobalBounds->bottom-ioGlobalBounds->top, maxHeight);
		Int16 width		= pin(minWidth, ioGlobalBounds->right-ioGlobalBounds->left, maxWidth);


			/*
				Fourth (and finally), now that we know its size, pin its location onto the screen, and
				stuff the calculated results back into |ioGlobalBounds|.
			*/
		ioGlobalBounds->top			= pin(desktopMinTop, ioGlobalBounds->top, desktopMaxBottom - height);
		ioGlobalBounds->left		= pin(desktopMinLeft, ioGlobalBounds->left, desktopMaxRight - width);

		ioGlobalBounds->bottom	= ioGlobalBounds->top + height;
		ioGlobalBounds->right		= ioGlobalBounds->left + width;
}


/*======================================================================================
	Better create window with saved status ID.
======================================================================================*/

LWindow *CSaveWindowStatus::CreateWindow(ResIDT inWindowID, LCommander *inSuperCommander) {

	LCommander::SetDefaultCommander(inSuperCommander);
	LAttachable::SetDefaultAttachable(nil);

	LWindow	*theWindow = (LWindow *) UReanimator::ReadObjects('PPob', inWindowID);
	
	try
	{
		FailNIL_(theWindow);
		theWindow->FinishCreate();
		if ( theWindow->HasAttribute(windAttr_ShowNew) ) {
			theWindow->Show();
		}
	}
	catch(...)
	{
		delete theWindow;
		throw;
	} 
	return theWindow;
}

/*======================================================================================
	Do basic initialization of the window. Should be called from the window's 
	FinishCreateSelf().
======================================================================================*/

void CSaveWindowStatus::FinishCreateWindow()
{
	Handle statusInfoH = CPrefs::ReadWindowData(GetStatusResID());
	Boolean doAdjustNIL = true;
	
	try {
	
		// Validate stored state info
		if ( statusInfoH ) {
			LHandleStream statusStream(statusInfoH);
			UInt16 bogusVersion, version;
			// As of 97/10/13, we are writing out zero as the first version data,
			// because Communicator 4.0x was checking version >= GetValidStatusVersion(),
			// and thus trying to decode future versions of the status, and crashing.
			statusStream >> bogusVersion; // and discard this zero.
			statusStream >> version;
			if ( bogusVersion == 0 && version == GetValidStatusVersion() ) {
				doAdjustNIL = false;
				ReadWindowStatus(&statusStream);
			}
		}
	}
	catch ( ExceptionCode inErr) {
		
		Assert_(false); // Just catch it, don't do anything!
	}
	catch ( OSErr inErr ) { }		// probably bad status version. Again, do nothing
	
	if ( doAdjustNIL ) ReadWindowStatus(nil);
}

void CSaveWindowStatus::FinishCreateWindow(CSaveWindowStatus* inTemplateWindow)
{
	ThrowIfNot_(inTemplateWindow->GetStatusResID() == GetStatusResID());
	ThrowIfNot_(inTemplateWindow->GetValidStatusVersion() == GetValidStatusVersion());
	LHandleStream statusStream;
	inTemplateWindow->WriteWindowStatus(&statusStream);
	statusStream.SetMarker(0, streamFrom_Start);
	this->ReadWindowStatus(&statusStream);
	// Stagger.
	Rect bounds;
	GetPaneGlobalBounds(mWindowSelf, &bounds);
	OffsetRect(&bounds, 20, 20);
	VerifyWindowBounds(mWindowSelf, &bounds);
	mWindowSelf->DoSetBounds(bounds);
}

/*======================================================================================
	Try to close a Window as a result of direct user action. Save window status. Should
	be called from the window's AttemptClose() or DoClose() method as follows:
	
	virtual void		AttemptClose() {
							AttemptCloseWindow();
							inherited::AttemptClose();
						}
	virtual void		DoClose() {
							AttemptCloseWindow();
							inherited::DoClose();
						}
======================================================================================*/

void CSaveWindowStatus::AttemptCloseWindow() {

	Assert_(mWindowSelf->GetSuperCommander() != nil);
	
	if ( mWindowSelf->GetSuperCommander()->AllowSubRemoval(mWindowSelf) ) {
		SaveStatusInfo();
	}
}


/*======================================================================================
	Adjust the window to stored preferences. This method should be overriden in subclasses to 
	read data from the stream as stored preferences. Make sure to call this method
	first.
======================================================================================*/

void CSaveWindowStatus::ReadWindowStatus(LStream *inStatusData)
{
	if ( inStatusData != nil )
	{
		mHasSavedStatus = true;			

		Rect bounds;
		*inStatusData >> bounds;
		VerifyWindowBounds(mWindowSelf, &bounds);
		mWindowSelf->DoSetBounds(bounds);
	}
	// Don't center the window in the default case, because not all clients
	// want this.
}


/*======================================================================================
	Get window stored preferences. This method should be overriden in subclasses to 
	write data to the stream that needs to be stored. Make sure to call this method
	first.
======================================================================================*/

void CSaveWindowStatus::WriteWindowStatus(LStream *outStatusData)
{
	mHasSavedStatus = true;			
	
	Rect bounds;
	GetPaneGlobalBounds(mWindowSelf, &bounds);

	*outStatusData << bounds;
}


/*======================================================================================
	Store window preferences.
======================================================================================*/

void CSaveWindowStatus::SaveStatusInfo()
{
	if ( !mCanSaveStatus ) return;
	
	try
	{
		LHandleStream statusStream;
		UInt16 bogusVersion = 0; // so that Communicator 4.0x will not use the data.
		statusStream << bogusVersion;
		statusStream << GetValidStatusVersion();
		
		WriteWindowStatus(&statusStream);
		
		CPrefs::WriteWindowData(statusStream.GetDataHandle(), GetStatusResID());
	}
	catch(...)
	{
		
		Assert_(false); // Just catch it, don't do anything!
	}
}


