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


#include "CIncludeView.h"

//-----------------------------------
CIncludeView::CIncludeView(LStream* inStream)
//-----------------------------------
:	LPane(inStream)
{
	*inStream >> mInstalledViewID;

	if ( mInstalledViewID == 0 ) {	// No view to install.
		//Assert_(mInstalledViewID != 0);
		return;
	}
	
	// We need to create the pane immediately so that it is added to the superview in the order
	// specified in Constructor.

	// If the installed view ID is negative, we take the absolute value of it and install
	// the created view directly. Otherwise, we install the first subpane of the created
	// view.
	
	const Boolean installViewDirectly = (mInstalledViewID < 0);
	
	// Store default commander and view, do we need to do this?
	LCommander *defaultCommander = LCommander::GetDefaultCommander();
	LView *defaultView = LPane::GetDefaultView();

	//
	// Because of the way that the new stream classes work in CWPro1 PP, we had
	// to make a change to how we read in the included object. Previously, we
	// could just call UReanimator::ReadObjects to do this. However, the new
	// classes change the way things work to use a static counter variable to assign
	// a key for each object in an internal hash table. This would normally be ok, but
	// this class forces a reset of that counter (using ReadObjects() does that) and
	// that messes up all the internal bookkeeping for the reanimator. 
	//
	// To get around that, we have duplicated the code in UReanimator::ReadObjects
	// with the exception of the line that resets the counter. (mcmullen & pinkerton)
	//
	LView *newView = nil;
	{
		StResource objectRes('PPob', installViewDirectly ? 
									-mInstalledViewID : mInstalledViewID);
		HLockHi(objectRes.mResourceH);
		
		LDataStream objectStream(*objectRes.mResourceH,
										GetHandleSize(objectRes.mResourceH));
		
		// sObjectCount = 0; // THIS IS THE BAD THING WE DON'T WANT TO HAPPEN
		
		Int16	ppobVersion;
		objectStream.ReadBlock(&ppobVersion, sizeof(Int16));
		
		SignalIf_(ppobVersion != 2);
										
		newView = (LView *) UReanimator::ObjectsFromStream(&objectStream);
	}

	// Restore default commander and view, do we need to do this?
	LCommander::SetDefaultCommander(defaultCommander);
	LPane::SetDefaultView(defaultView);

	Assert_(newView != nil);
	if ( newView == nil ) {
		mInstalledViewID = 0;	// So that we don't delete ourselves in FinishCreateSelf()
		return;	// Get outta here!
	}
	
	Try_ {
		
		if ( installViewDirectly ) {
		
			InstallPane(newView);
			
		} else {
			
			// The pane we want should be the lone sibling of the root of the installed view's
			// hierarchy.  Why?  Because constructor will only allow the root of the
			// hierarchy to be an LView or an LWindow, and we want to allow all types of
			// panes.  So here we dig out the real pane that we want to install.
			
			LPane *subPane = nil;
			newView->GetSubPanes().FetchItemAt(1, subPane);
			Assert_(subPane != nil);
			
			if ( subPane != nil ) {

				InstallPane(subPane);
				//subPane->FinishCreate();
				// NOT!  Superview will call this, because it's added to the iterator.
			}
			
			delete newView; // old parent of the installed view.  An empty shell, now.
		}
		
	}
	Catch_(inErr) {
		delete newView;	// Make sure we delete the view!
		Throw_(inErr);
	}
	EndCatch_
}

//-----------------------------------
CIncludeView::~CIncludeView()
//-----------------------------------
{
}

//-----------------------------------
void CIncludeView::InstallPane(LPane* inPane)
// This matches LPane::InitPane.  Unfortunately, this may go out of date
// when InitPane changes.
//-----------------------------------
{
	inPane->SetPaneID(mPaneID);
	mPaneID = 0;	// Set our id to 0 so that we are not confused with the installed pane

	inPane->SetUserCon(mUserCon);
	
	if ( mVisible == triState_Off ) {
		inPane->Hide();
	} else {
		inPane->Show();
	}
	
	if ( mEnabled == triState_Off ) {
		inPane->Disable();
	} else {
		inPane->Enable();
	}

	SDimension16 frameSize;
	GetFrameSize(frameSize);
	inPane->ResizeFrameTo(frameSize.width, frameSize.height, false);
	SPoint32 location;
	inPane->GetFrameLocation(location);
	inPane->MoveBy(mFrameLocation.h - location.h, mFrameLocation.v - location.v, false);

	SBooleanRect frameBinding;
	GetFrameBinding(frameBinding);
	inPane->SetFrameBinding(frameBinding);
	
	// Associate Pane with its SuperView
	LView *theSuperView = mSuperView;
	Assert_(theSuperView);	
	inPane->PutInside(theSuperView, false);

	// Negative width and/or height means to expand the
	// Pane to the size of the SuperView
		
	if (theSuperView != nil)
	{
		Boolean	expandHoriz = (mFrameSize.width < 0);
		Boolean	expandVert = (mFrameSize.height < 0);
		if (expandHoriz || expandVert)
			theSuperView->ExpandSubPane(inPane, expandHoriz, expandVert);
	}
} // CIncludeView::InstallPane

//-----------------------------------
void CIncludeView::FinishCreateSelf()
//-----------------------------------
{
	inherited::FinishCreateSelf();

	if ( mInstalledViewID != 0 ) {
		delete this; // removes this from superview in ~LPane
	}
	
} // CIncludeView::FinishCreateSelf
