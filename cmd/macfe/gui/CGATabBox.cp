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


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CGATabBox.h"

#include <LGAPushButton.h>
#include <UGraphicsUtilities.h>
#include <UGAColorRamp.h>


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/

typedef struct {
	PaneIDT		tabPaneID;			// ID of the pane associated with the tab
	PaneIDT		latentCommanderID;	// Last active commander ID, 0 if none
} TabLatentCommanderT;


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/

static const Int16 cSelectedTabShiftH = 2;
static const Int16 cSelectedTabShiftV = 2;


/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/

// CGATabBoxTab

class CGATabBoxTab : public LGAPushButton {

public:

						enum { class_ID = 'TbBt' };
						CGATabBoxTab(LStream *inStream) :
									 LGAPushButton(inStream),
									 mSelected(false) {
						}
						
	void				RefreshHiddenBottom(void);

protected:

	virtual void		DrawSelf(void);
	virtual void		ClickSelf(const SMouseDownEvent &inMouseDown);
	virtual void		BroadcastValueMessage(void);
	
	virtual void		CalcTitleRect(Rect &outRect);
	virtual void		DrawButtonNormalColor(void);
	
	Boolean				IsSelectedTab(void) {
							return (((CGATabBox *) mSuperView)->GetCurrentTabID() == mPaneID);
						}

	// Instance variables
	
	Boolean				mSelected;
};



/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

#pragma mark -

/*======================================================================================
	Register PP classes associated with this tab box.
======================================================================================*/

void CGATabBox::RegisterTabBoxClasses(void) {

	RegisterClass_(CGATabBoxTab);
	RegisterClass_(CGATabBox);
}


/*======================================================================================
	Constructor.
======================================================================================*/

CGATabBox::CGATabBox(LStream *inStream) :
				  	 LGABox_fixes(inStream),
				  	 LListener(),
				  	 LBroadcaster(),
				  	 mCurrentTabID(0),
				  	 mCurrentPaneID(0),
				  	 mTabLatentCommanders(sizeof(TabLatentCommanderT)) {

	mHasBorder = true;
	mBorderStyle = borderStyleGA_EmbossedOneBorder;
	mTitlePosition = titlePositionGA_None;
	mTitle[0] = 0;
}


/*======================================================================================
	Set the currently selected tab.
======================================================================================*/

void CGATabBox::SetCurrentTabID(PaneIDT inTabID) {

	if ( mCurrentTabID == inTabID ) return;
	Boolean updatePort = false;
	
	CGATabBoxTab *lastTab = nil, *newTab = nil;
	LTabGroup *theTabGroup = nil;
	
	// Hide old view
	
	if ( mCurrentTabID != 0 ) {
		lastTab = GetTab(mCurrentTabID);
		if ( lastTab != nil ) {
			TabBoxCanChangeT canChangeRec = { this, true };
			BroadcastMessage(msg_TabViewCanChange, &canChangeRec);
			if ( canChangeRec.canChange ) {
				if ( lastTab->GetValueMessage() != 0 ) {
					LPane *thePane = FindPaneByID(lastTab->GetValueMessage());
					if ( thePane != nil ) {
						theTabGroup = FindTabGroup();
						StoreLatentTabCommander(dynamic_cast<LView *>(thePane));
						// Switch the window to the current commander so that a tab group
						// doesn't swap commanders as they are hidden
						if ( theTabGroup != nil ) {
							LCommander::SwitchTarget(LWindow::FetchWindowObject(GetMacPort()));
						}
						thePane->Hide();
					}
				}
				lastTab->SetTextTraitsID(GetTextTraitsID() + 1);
				lastTab->RefreshHiddenBottom();
			} else {
				return;	// Can't change
			}
		}
	}
	
	mCurrentTabID = inTabID;

	if ( mCurrentTabID != 0 ) {
		newTab = GetTab(mCurrentTabID);
		if ( newTab != nil ) {
			PaneIDT paneID = newTab->GetValueMessage();
			if ( paneID != 0 ) {
				LPane *thePane = FindPaneByID(paneID);
				if ( thePane != nil ) {
					thePane->Show();
					if ( !RestoreLatentTabCommander(dynamic_cast<LView *>(thePane)) &&
						 (theTabGroup != nil) ) {
						 
						//StEmptyVisRgn emptyRgn(GetMacPort());	// Hide any flicker
						LCommander::SwitchTarget(theTabGroup);
					}
				}
			}
			newTab->SetTextTraitsID(GetTextTraitsID());
			newTab->RefreshHiddenBottom();
			BroadcastMessage(msg_TabViewChanged, this);
		}
	}
	
	if ( (lastTab != nil) || (newTab != nil) ) {
		if ( lastTab != nil ) lastTab->Draw(nil);
		if ( newTab != nil ) newTab->Draw(nil);
		UpdatePort();
	}
}


/*======================================================================================
	Calculate the border rect, which will be offset by the bottom of the tabs.
======================================================================================*/

void CGATabBox::CalcBorderRect(Rect &outRect) {

	LGABox_fixes::CalcBorderRect(outRect);
	
	::InsetRect(&outRect, 1, 1);	// For border
	
	Rect tabRect;
	CalcCurrentTabRect(tabRect);

	outRect.top = tabRect.bottom - 4;
}


/*======================================================================================
	Calculate the current tab rect in local coordinates.
======================================================================================*/

void CGATabBox::CalcCurrentTabRect(Rect &outRect) {

	Assert_(mCurrentTabID != 0);

	if ( mCurrentTabID ) {
		CGATabBoxTab *theTab = GetTab(mCurrentTabID);
		Assert_(theTab != nil);
		if ( theTab != nil ) {
			theTab->CalcPortFrameRect(outRect);
			PortToLocalPoint(topLeft(outRect));
			PortToLocalPoint(botRight(outRect));
		}
	}
}


/*======================================================================================
	Finish creating the tab box.
======================================================================================*/

void CGATabBox::FinishCreateSelf(void) {

	LGABox_fixes::FinishCreateSelf();
	
	SetupTabs();
	
	SetCurrentTabID(GetUserCon());
}


/*======================================================================================
	Draw the box border.
======================================================================================*/

void CGATabBox::DrawBoxBorder(void) {

	StColorPenState::Normalize();

	Rect borderRect;
	CalcBorderRect(borderRect);
	::InsetRect(&borderRect, -1, -1);
	
	::FrameRect(&borderRect);

	LGABox_fixes::DrawBoxBorder();
}


/*======================================================================================
	Respond to tab messages. Only tabs will broadcast messages to this class, so that's
	all we need to be concerned with.
======================================================================================*/

void CGATabBox::ListenToMessage(MessageT inMessage, void */*ioParam*/) {

	SetCurrentTabID((PaneIDT) inMessage);
}


/*======================================================================================
	Called from FinishCreateSelf() to setup the tabs for the view.
======================================================================================*/

void CGATabBox::SetupTabs(void) {

	// Loop through all subpanes to find the relevant tabs
	
	LArrayIterator iterator(mSubPanes, LArrayIterator::from_Start);
	LPane *theSub;
	CGATabBoxTab *theTab = nil;
	Boolean foundTabs = false;
	while ( iterator.Next(&theSub) ) {
		theTab = dynamic_cast<CGATabBoxTab *>(theSub);
		if ( theTab != nil ) {
			if ( theTab->GetValueMessage() != 0 ) {
				PaneIDT paneID = theTab->GetValueMessage();
				LPane *thePane = FindPaneByID(paneID);
				if ( thePane != nil ) {
					thePane->Hide();
					LView *theView = dynamic_cast<LView *>(thePane);
					if ( theView != nil ) {
						// Initialize the last active commander for the tab's pane
						TabLatentCommanderT rec = { paneID, 0 };
						mTabLatentCommanders.InsertItemsAt(1, LArray::index_Last, &rec);
					}
				}
			}
			theTab->SetTextTraitsID(GetTextTraitsID() + 1);
			theTab->AddListener(this);
			foundTabs = true;
		}
	}
	Assert_(foundTabs);	// Should have found at least one tab!
}


/*======================================================================================
	A new tab view has just been activated. Try to find the last active commander and
	make it the currently active commander. Return true if a valid commander was actually
	found and stored.
======================================================================================*/

Boolean CGATabBox::StoreLatentTabCommander(LView *inTabView) {

	if ( inTabView == nil ) return false;
	
	const Int32 count = mTabLatentCommanders.GetCount();
	if ( count < 1 ) return false;
	
	PaneIDT paneID = inTabView->GetPaneID();
	Int32 index = 1;

	// Try to find the tab pane id in our list
	do {
		if ( ((TabLatentCommanderT *) mTabLatentCommanders.GetItemPtr(index))->tabPaneID == paneID ) {
			break;
		}
	} while ( ++index < count );
	
	paneID = 0;

	if ( index < count ) {
		LCommander *curTarget = LCommander::GetTarget();
		
		if ( curTarget != nil ) {
			LPane *thePane = dynamic_cast<LPane *>(curTarget);
			if ( (thePane != nil) && thePane->IsEnabled() && 
				 inTabView->FindPaneByID(thePane->GetPaneID()) ) {
				
				paneID = thePane->GetPaneID();	// We foud our commander!
			}
		}
		
		((TabLatentCommanderT *) mTabLatentCommanders.GetItemPtr(index))->latentCommanderID = paneID;
	}
	
	return (paneID != 0);
}


/*======================================================================================
	A new tab view has just been activated. Try to find the last active commander and
	make it the currently active commander. Return true if a valid commander was actually
	found and restored.
======================================================================================*/

Boolean CGATabBox::RestoreLatentTabCommander(LView *inTabView) {

	if ( inTabView == nil ) return false;
	
	Int32 count = mTabLatentCommanders.GetCount();
	if ( count < 1 ) return false;
	
	PaneIDT paneID = inTabView->GetPaneID();
	Int32 index = 1;

	// Try to find the tab pane id in our list
	do {
		if ( ((TabLatentCommanderT *) mTabLatentCommanders.GetItemPtr(index))->tabPaneID == paneID ) {
			break;
		}
	} while ( ++index < count );
	
	LCommander *theCommander = nil;

	if ( index < count ) {
		PaneIDT latentCommanderID = ((TabLatentCommanderT *) mTabLatentCommanders.GetItemPtr(index))->latentCommanderID;
		if ( latentCommanderID != 0 ) {
			// We found our view, now see if there was an active commander
			LPane *thePane = inTabView->FindPaneByID(latentCommanderID);
			if ( (thePane != nil) && thePane->IsEnabled() ) {
				theCommander = dynamic_cast<LCommander *>(thePane);
				if ( theCommander != nil ) {
					LCommander::SwitchTarget(theCommander);
				}
			}
		}
	}

	return (theCommander != nil);
}


/*======================================================================================
	Get the specified tab. Return nil if the tab cannot be found.
======================================================================================*/

CGATabBoxTab *CGATabBox::GetTab(PaneIDT inID) {

	LArrayIterator iterator(mSubPanes, LArrayIterator::from_Start);
	LPane *theSub;
	while ( iterator.Next(&theSub) ) {
		if ( theSub->GetPaneID() == inID ) {
			CGATabBoxTab *theTab = dynamic_cast<CGATabBoxTab *>(theSub);
			if ( theTab != nil ) return theTab;
		}
	}

	Assert_(false);	// Should have found tab!
	return nil;
}


/*======================================================================================
	Find the tab group for the window if it is currently active or nil if there is not 
	one.
======================================================================================*/

LTabGroup *CGATabBox::FindTabGroup(void) {

	LCommander *theCommander = LCommander::GetTarget();
	
	if ( !theCommander ) return nil;
	
	LCommander *windowCommander = dynamic_cast<LCommander *>(LWindow::FetchWindowObject(GetMacPort()));
	Assert_(windowCommander != nil);
	
	if ( windowCommander == theCommander ) return nil;
	
	LTabGroup *lastTabGroup = nil;
	
	do {
		LTabGroup *tabGroup = dynamic_cast<LTabGroup *>(theCommander);
		if ( tabGroup != nil ) {
			lastTabGroup = tabGroup;
		}
	} while ( ((theCommander = theCommander->GetSuperCommander()) != nil) &&
			  (theCommander != windowCommander) );

	return lastTabGroup;
}


#pragma mark -

/*======================================================================================
	Refresh just the bottom portion of the button.
======================================================================================*/

void CGATabBoxTab::RefreshHiddenBottom(void) {

	Rect refreshRect;
	if ( IsVisible() && CalcPortFrameRect(refreshRect) ) {
		refreshRect.top = refreshRect.bottom - 5;
		Rect superRevealed;
		mSuperView->GetRevealedRect(superRevealed);
		if ( ::SectRect(&refreshRect, &superRevealed, &refreshRect) ) {
			InvalPortRect(&refreshRect);
		}
	}
}


/*======================================================================================
	Draw the button except for the bottom edge.
======================================================================================*/

void CGATabBoxTab::DrawSelf(void) {

	Rect clipRect;
	CalcLocalFrameRect(clipRect);
	
	RgnHandle clipRgn = ::NewRgn();
	
	if ( IsSelectedTab() ) {
		RgnHandle tempRgn = ::NewRgn();
		clipRect.bottom -= 4;
		::RectRgn(clipRgn, &clipRect);
		::SetRect(&clipRect, clipRect.left + 1, clipRect.bottom, clipRect.right - 4, clipRect.bottom + 1);
		::RectRgn(tempRgn, &clipRect);
		::UnionRgn(clipRgn, tempRgn, clipRgn);
		::DisposeRgn(tempRgn);
	} else {
		clipRect.bottom -= 5;
		::RectRgn(clipRgn, &clipRect);
	}
	
	StClipRgnState saveClip(clipRgn);
	::DisposeRgn(clipRgn);

	LGAPushButton::DrawSelf();
}


/*======================================================================================
	Clicking atomatically selects.
======================================================================================*/

void CGATabBoxTab::ClickSelf(const SMouseDownEvent &/*inMouseDown*/) {

	BroadcastValueMessage();
}


/*======================================================================================
	Broadcast pane id instead of value message.
======================================================================================*/

void CGATabBoxTab::BroadcastValueMessage(void) {

	MessageT valueMsg = mValueMessage;
	BroadcastMessage(mPaneID, &mValueMessage);
}


/*======================================================================================
	Offset since hiding a portion of button.
======================================================================================*/

void CGATabBoxTab::CalcTitleRect(Rect &outRect) {

	LGAPushButton::CalcTitleRect(outRect);
	
	if ( IsSelectedTab() ) {
		::OffsetRect(&outRect, 0, -1);
	} else {
		::OffsetRect(&outRect, 0, -2);
	}
}


/*======================================================================================
	Draw darker if not selected. Direct copy/paste from LGAPushButton except for 
	IsSelectedTab() sections.
======================================================================================*/

void CGATabBoxTab::DrawButtonNormalColor(void) {

	StColorPenState	theColorPenState;
	theColorPenState.Normalize();
	
	Boolean isSelected = IsSelectedTab();
		
	Rect localFrame;
	CalcLocalFrameRect ( localFrame );
	
	::RGBForeColor (&UGAColorRamp::GetBlackColor());
	::FrameRoundRect (&localFrame, 8, 8);

	if ( isSelected ) {
		::RGBForeColor(&UGAColorRamp::GetColor(2));
	} else {
		::RGBForeColor(&UGAColorRamp::GetColor(3));
	}
	::InsetRect(&localFrame, 1, 1);
	::PaintRoundRect(&localFrame, 4, 4);
	::InsetRect(&localFrame, -1, -1);
	
	if ( isSelected ) {
		::RGBForeColor(&UGAColorRamp::GetWhiteColor());
	} else {
		::RGBForeColor(&UGAColorRamp::GetColor(1));
	}
	UGraphicsUtilities::TopLeftSide (&localFrame, 
						2, 					//	TOP
						2, 					//	LEFT
						3, 					// BOTTOM
						3 );					// RIGHT
	UGraphicsUtilities::PaintColorPixel ( 	localFrame.left + 3, localFrame.top + 3, &UGAColorRamp::GetWhiteColor());
	UGraphicsUtilities::PaintColorPixel ( 	localFrame.left + 1, localFrame.top + 2, &UGAColorRamp::GetColor(4));
	UGraphicsUtilities::PaintColorPixel ( 	localFrame.left + 2, localFrame.top + 1, &UGAColorRamp::GetColor(4));
	UGraphicsUtilities::PaintColorPixel ( 	localFrame.left + 1, localFrame.bottom - 3, &UGAColorRamp::GetColor(4));
	UGraphicsUtilities::PaintColorPixel ( 	localFrame.left + 2, localFrame.bottom - 2, &UGAColorRamp::GetColor(4));
	UGraphicsUtilities::PaintColorPixel ( 	localFrame.right - 3, localFrame.top + 1, &UGAColorRamp::GetColor(4));
	UGraphicsUtilities::PaintColorPixel ( 	localFrame.right - 2, localFrame.top + 2, &UGAColorRamp::GetColor(4));

	// ¥ SHADOW EDGES
	::RGBForeColor ( &UGAColorRamp::GetColor(8));
	::MoveTo ( localFrame.left + 3, localFrame.bottom - 2 );
	::LineTo ( localFrame.right - 3, localFrame.bottom - 2 );
	::MoveTo ( localFrame.right - 2, localFrame.bottom - 3 );
	::LineTo ( localFrame.right - 2, localFrame.top + 3 );
	::RGBForeColor ( &UGAColorRamp::GetColor(5));
	UGraphicsUtilities::BottomRightSide ( &localFrame, 
							3, 					//	TOP
							3, 					//	LEFT
							2, 					// BOTTOM
							2 );					// RIGHT
	UGraphicsUtilities::PaintColorPixel ( 	localFrame.right - 3, localFrame.bottom - 3, &UGAColorRamp::GetColor(8));
	UGraphicsUtilities::PaintColorPixel ( 	localFrame.right - 4, localFrame.bottom - 4, &UGAColorRamp::GetColor(5));
}


