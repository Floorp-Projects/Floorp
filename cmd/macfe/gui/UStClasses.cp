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
// File: UStClasses.cp
//
// This file contains a couple of stack-based save/restore classes
// 

#include "UStClasses.h"


//======================================================================================
#pragma mark * StSetResAttrs

//
// 	Constructor. Set the resource handle attributes to the specified attributes. If
//	inAddAttrs is true, add the specified attributes to the current attributes; otherwise
//	reset the attributes completely.
//
StSetResAttrs::StSetResAttrs(Handle inResourceH, short inResAttrs, Boolean inAddAttrs) {

	Assert_(inResourceH != nil);
	
	mResourceH = nil;
	
	mSavedAttrs = ::GetResAttrs(inResourceH);
	ThrowIfResError_();
	::SetResAttrs(inResourceH, inAddAttrs ? (mSavedAttrs | inResAttrs) : inResAttrs);
	ThrowIfResError_();
	mResourceH = inResourceH;
}


//
// Destructor
//
StSetResAttrs::~StSetResAttrs(void) {

	if ( mResourceH ) {
		::SetResAttrs(mResourceH, mSavedAttrs);
		OSErr theErr = ::ResError();
		Assert_(theErr == noErr);
	}
}

//======================================================================================
#pragma mark * StExcludeVisibleRgn

//
// Constructor.
//
// Exclude the specified port rectangle from the current visible region.
//
StExcludeVisibleRgn::StExcludeVisibleRgn(LPane *inPane) {

	mGrafPtr = nil;
	
	Assert_(inPane != nil);
	FailNIL_(mSaveVisRgn = ::NewRgn());
	
	Rect portFrame;
	GrafPtr port = inPane->GetMacPort();
	inPane->CalcPortFrameRect(portFrame);
	StRegion tempRgn(portFrame);
	::CopyRgn(port->visRgn, mSaveVisRgn);
	::DiffRgn(port->visRgn, tempRgn, port->visRgn);
	
	mGrafPtr = port;
	mSavePortOrigin = topLeft(mGrafPtr->portRect);
}


//
// Destructor.
//
StExcludeVisibleRgn::~StExcludeVisibleRgn(void) {

	if ( mGrafPtr != nil ) {
		Point deltaOrigin = topLeft(mGrafPtr->portRect);
		deltaOrigin.h -= mSavePortOrigin.h;
		deltaOrigin.v -= mSavePortOrigin.v;
		if ( (deltaOrigin.h != 0) || (deltaOrigin.v != 0) ) {
			::OffsetRgn(mSaveVisRgn, deltaOrigin.h, deltaOrigin.v);
		}
		RgnHandle tempRgn = mGrafPtr->visRgn;
		mGrafPtr->visRgn = mSaveVisRgn;
		::DisposeRgn(tempRgn);
	}
}
