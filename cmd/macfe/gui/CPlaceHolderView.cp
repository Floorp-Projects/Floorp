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

#include "CPlaceHolderView.h"

/*
Overview:
---------
	A CPlaceHolderView allows you define a space where another view
	from the same window is moved to when the CPlaceHolderView becomes
	visible. When the CPlaceHolderView is hidden, the view is put back
	where it was.

	It is similar to the CIncludeView with the difference that the
	CIncludeView creates the included view from an external 'PPob' 
	while the CPlaceHolderView assumes that the included view is
	already somewhere in the same window.

Application:
------------
	It has been developed for the CSubscribeWindow which contains
	a TabSwitcher where each Tab panel displays the same list which
	can't be disposed when switching panels. The list is originally
	attached to the Window (under Constructor) and each Tab panel
	contains a CPlaceHolderView which refers to the list.

Limitation:
-----------
	It does not take care of the Commander chain. If the view that
	you plan to move around with different CPlaceHolderViews is also
	a Commander, you will have to manually link it where it fits
	in the Commander chain .
*/


//------------------------------------------------------------------------------------
//	CPlaceHolderView
//
//------------------------------------------------------------------------------------
CPlaceHolderView::CPlaceHolderView(LStream* inStream)
:	LView(inStream),
	mSaveSuperViewOfInstalledView(nil),
	mInstalledView(nil)
{
	*inStream >> mInstalledViewID;
}


//------------------------------------------------------------------------------------
//	~CPlaceHolderView
//
//------------------------------------------------------------------------------------
CPlaceHolderView::~CPlaceHolderView()
{
}


//------------------------------------------------------------------------------------
//	FinishCreateSelf
//
//------------------------------------------------------------------------------------
void CPlaceHolderView::FinishCreateSelf()
{
	LView::FinishCreateSelf();
	AcquirePane();
}


//------------------------------------------------------------------------------------
//	ShowSelf
//
//------------------------------------------------------------------------------------
void CPlaceHolderView::ShowSelf()
{
	AcquirePane();
}


//------------------------------------------------------------------------------------
//	HideSelf
//
//------------------------------------------------------------------------------------
void CPlaceHolderView::HideSelf()
{
	ReturnPane();
}


//------------------------------------------------------------------------------------
//	AcquirePane
//
//------------------------------------------------------------------------------------
void CPlaceHolderView::AcquirePane()
{
	if (mInstalledViewID == 0)		// no view to install
		return;

	if (mInstalledView != nil)		// view already installed
		return;

	// Starting from the top of the view hierarchy, we look
	// inside the window for a pane with the specified id
	LWindow *	myWindow = LWindow::FetchWindowObject(GetMacPort());
	mInstalledView = (LView *)myWindow->FindPaneByID(mInstalledViewID);

	// We don't want to re-install ourselves, though
	if (mInstalledView == (LView *)this)
		mInstalledView = nil;

	// Install the view
	Assert_(mInstalledView != nil);
	if (mInstalledView != nil)
	{
		mSaveSuperViewOfInstalledView = mInstalledView->GetSuperView();
		InstallPane(mInstalledView);
	}
}


//------------------------------------------------------------------------------------
//	ReturnPane
//
//------------------------------------------------------------------------------------
void CPlaceHolderView::ReturnPane()
{
	// Put back the included view to its original place before I disappear...
	if (mInstalledView)
	{
		// ... but check first if I'm still the owner of the view.
		if (mInstalledView->GetSuperView() == this)
		{
			if (mSaveSuperViewOfInstalledView)
				mInstalledView->PutInside(mSaveSuperViewOfInstalledView, false);
		}
		mInstalledView = nil;
	}
}

//------------------------------------------------------------------------------------
//	InstallPane
//
//------------------------------------------------------------------------------------
void CPlaceHolderView::InstallPane(LPane* inPane)
{
	// Set the Pane's binding to my binding
	SBooleanRect frameBinding;
	GetFrameBinding(frameBinding);
	inPane->SetFrameBinding(frameBinding);
	
	// Associate Pane with its superview: me
	inPane->PutInside(this, false);

	// Expand the Pane to my size
	this->ExpandSubPane(inPane, true, true);
}
