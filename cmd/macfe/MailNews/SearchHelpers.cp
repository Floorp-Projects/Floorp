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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// SearchHelpers.cp

/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#define DEBUGGER_ASSERTIONS

#include "SearchHelpers.h"
#include "CMailFolderButtonPopup.h"
#include "uprefd.h"
#include "uerrmgr.h"
#include "CDeviceLoop.h"
#include "LTableViewHeader.h"
#include "UGraphicGizmos.h"
#include "PascalString.h"

#include <LGAEditField.h>
#include <UGAColorRamp.h>
#include <LGAPushButton.h>
#include <URegions.h>
#include "UStClasses.h"
#include "CSearchManager.h"

// BE stuff

#define B3_SEARCH_API
#include "msg_srch.h"


#pragma mark -
/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/

static pascal void DoNothingStdRect(GrafVerb /*theVerb*/, Rect */*theRect*/) { }
static pascal void DoNothingStdRgn(GrafVerb /*theVerb*/, RgnHandle /*theRgn*/) { }
static pascal void ForgetRoutineDesc(UniversalProcPtr *routineDesc) {
#ifdef powerc	// Only do for power pc
	if ( *routineDesc )
	{
		DisposeRoutineDescriptor(*routineDesc);
		*routineDesc = nil;
	}
#else // powerc	// Only do for power pc
	#pragma unused(routineDesc)
#endif // powerc
}

static const Int16 cWindowDesktopMargin = 4;


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

CQDProcs USearchHelper::sStdProcs = { (QDTextUPP) -1L };

/*======================================================================================
	Register classes in this file.
======================================================================================*/

void USearchHelper::RegisterClasses()
{
	RegisterClass_(CFolderScopeGAPopup);
	RegisterClass_(CFolderMoveGAPopup);
	RegisterClass_(CSearchDateField);
}

/*======================================================================================
	Find the specified dialog control.
======================================================================================*/

LControl *USearchHelper::FindViewControl(LView *inView, PaneIDT inID)
{
	LControl *theControl = dynamic_cast<LControl *>(inView->FindPaneByID(inID));
	AssertFail_(theControl != nil);
	return theControl;
}

/*======================================================================================
	Find the specified view subpane.
======================================================================================*/

LPane *USearchHelper::FindViewSubpane(LView *inView, PaneIDT inID)
{
	LPane *thePane = inView->FindPaneByID(inID);
	AssertFail_(thePane != nil);
	
	return thePane;
}

/*======================================================================================
	Find the specified view subpane.
======================================================================================*/

LView *USearchHelper::FindViewSubview(LView *inView, PaneIDT inID)
{
	LView *theView = dynamic_cast<LView *>(inView->FindPaneByID(inID));
	AssertFail_(theView != nil);
	
	return theView;
}


/*======================================================================================
	Find the specified view edit field.
======================================================================================*/

LBroadcasterEditField *USearchHelper::FindViewEditField(LView *inView, PaneIDT inID)
{
	LBroadcasterEditField *theEditField = dynamic_cast<LBroadcasterEditField *>(inView->FindPaneByID(inID));
	AssertFail_(theEditField != nil);
	
	return theEditField;
}

/*======================================================================================
	Find the specified view popup menu.
======================================================================================*/

CFolderScopeGAPopup *USearchHelper::FindFolderScopePopup(LView *inView, PaneIDT inID)
{
	CFolderScopeGAPopup *thePopup = dynamic_cast<CFolderScopeGAPopup *>(inView->FindPaneByID(inID));
	AssertFail_(thePopup != nil);
	
	return thePopup;
}

CSearchPopupMenu *USearchHelper::FindSearchPopup(LView *inView, PaneIDT inID)
{
	CSearchPopupMenu *thePopup = dynamic_cast<CSearchPopupMenu *>(inView->FindPaneByID(inID));
	AssertFail_(thePopup != nil);
	
	return thePopup;
}

/*======================================================================================
	Find the specified date view.
======================================================================================*/

CSearchDateField *USearchHelper::FindViewDateField(LView *inView, PaneIDT inID)
{
	CSearchDateField *theDateField = dynamic_cast<CSearchDateField *>(inView->FindPaneByID(inID));
	AssertFail_(theDateField != nil);
	
	return theDateField;
}

/*======================================================================================
	Get the window associated with the specified pane.
======================================================================================*/

LWindow *USearchHelper::GetPaneWindow(LPane *inPane)
{
	LWindow *theWindow = LWindow::FetchWindowObject(inPane->GetMacPort());
	AssertFail_(theWindow != nil);

	return theWindow;
}

/*======================================================================================
	Find the first view subpane with the specified id if it is visible.
======================================================================================*/

LPane *USearchHelper::FindViewVisibleSubpane(LView *inView, PaneIDT inID)
{
	static LPane *sPane = nil;	// Reduce recursive requirements
	static LPane *sSub;
								
	if ( (inID == inView->GetPaneID()) && inView->IsVisible() )
	{
		sPane = inView;
	} else
	{
		// Search all subpanes
		LArrayIterator iterator(inView->GetSubPanes(), LArrayIterator::from_Start);
		while ( iterator.Next(&sSub) )
		{
			LView *theView = dynamic_cast<LView *>(sSub);
			if ( theView != nil )
			{
				sPane = FindViewVisibleSubpane(theView, inID);
			} else if ( (inID == sSub->GetPaneID()) && sSub->IsVisible() )
			{
				sPane = sSub;
				break;
			}
		}
	}
	
	return sPane;
}





/*======================================================================================
	Find the tab group for the view.
======================================================================================*/

CSearchTabGroup *USearchHelper::FindWindowTabGroup(LArray *inSubcommanders)
{
	LArrayIterator iterator(*inSubcommanders);
	LCommander *theSub;
	CSearchTabGroup *theTabGroup = nil;
		
	while ( (theTabGroup == nil) && iterator.Next(&theSub) )
	{
		theTabGroup = dynamic_cast<CSearchTabGroup *>(theSub);
	}
	
	AssertFail_(theTabGroup != nil);
	return theTabGroup;
}




/*======================================================================================
	Remove the size box area of the window from the visible region. This method should
	be called at the end of the window's DrawSelf() method.
======================================================================================*/

void USearchHelper::RemoveSizeBoxFromVisRgn(LWindow *inWindow)
{
	Rect sizeBox;
	RgnHandle visRgn = inWindow->GetMacPort()->visRgn;
	
	inWindow->CalcPortFrameRect(sizeBox);
	sizeBox.left = sizeBox.right - 15;
	sizeBox.top = sizeBox.bottom - 15;

	if ( ::RectInRgn(&sizeBox, visRgn) )
	{
		StRegion diffRgn(sizeBox);
		::DiffRgn(visRgn, diffRgn, visRgn);
	}
}


/*======================================================================================
	Refresh the specified port rect for the given pane.
======================================================================================*/

void USearchHelper::RefreshPortFrameRect(LPane *inPane, Rect *inPortRect)
{
	if ( !inPane->IsVisible() || (inPane->GetSuperView() == nil) ) return;
	
	Rect superRevealed;
	inPane->GetSuperView()->GetRevealedRect(superRevealed);
	if ( ::SectRect(inPortRect, &superRevealed, inPortRect) )
	{
		inPane->InvalPortRect(inPortRect);
	}
}


/*======================================================================================
	Refresh the specified local rect for the given pane.
======================================================================================*/

void USearchHelper::RefreshLocalFrameRect(LPane *inPane, Rect *inLocalRect)
{
	inPane->LocalToPortPoint(topLeft(*inLocalRect));
	inPane->LocalToPortPoint(botRight(*inLocalRect));
	
	RefreshPortFrameRect(inPane, inLocalRect);
}


/*======================================================================================
	Link the specified listener to the any and all pane broadcasters within the view 
	specified by inContainer.
======================================================================================*/

void USearchHelper::LinkListenerToBroadcasters(LView *inContainer, LListener *inListener)
{
	// First, check out the container itself
	
	if ( (dynamic_cast<LListener *>(inContainer)) != inListener )
	{
		LBroadcaster *broadcaster = dynamic_cast<LBroadcaster *>(inContainer);
		if ( broadcaster != nil ) broadcaster->AddListener(inListener);
	}
	
	// Iterate through all subviews
	
	LArrayIterator iterator(inContainer->GetSubPanes());
	
	LPane *pane;
	
	while ( iterator.Next(&pane) )
	{
		LView *view = dynamic_cast<LView *>(pane);
		if ( view != nil )
		{
			LinkListenerToBroadcasters(view, inListener);
		} else
		{
			LBroadcaster *broadcaster = dynamic_cast<LBroadcaster *>(pane);
			if ( broadcaster != nil ) broadcaster->AddListener(inListener);
		}
	}
}


/*======================================================================================
	Get the length of the specified edit field text.
======================================================================================*/

Int16 USearchHelper::GetEditFieldLength(LEditField *inEditField)
{
	
	return (**inEditField->GetMacTEH()).teLength;
}


/*======================================================================================
	Return a display name for the specified folder name.
======================================================================================*/

void USearchHelper::AssignUString(Int16 inStringIndex, CString& outString)
{
	::GetIndString(outString, 8600, inStringIndex);
}


/*======================================================================================
	Enable or disable the specified button without redrawing it!
======================================================================================*/

void USearchHelper::EnableDisablePane(LPane *inPane, Boolean inEnable, Boolean inRefresh)
{
	Boolean isEnabled = inPane->IsEnabled();
	Boolean isVisible = inPane->IsVisible();
	
	if ( isVisible && ((isEnabled && inEnable) || (!isEnabled && !inEnable)) ) return;
	
	if ( isVisible ) inPane->Hide();
	if ( inEnable )
	{
		inPane->Enable();
	} else
	{
		inPane->Disable();
	}
	if ( isVisible ) inPane->Show();
	if ( !inRefresh )
	{
		inPane->DontRefresh();
	}
}


/*======================================================================================
	Show or hide the specified pane.
======================================================================================*/

void USearchHelper::ShowHidePane(LPane *inPane, Boolean inDoShow)
{
	if ( inDoShow )
	{
		inPane->Show();
	} else
	{
		inPane->Hide();
	}
}


/*======================================================================================
	Handle a dialog Enter, Return, and Cancel keys. Return true if the key was
	handled, false otherwise.
======================================================================================*/

Boolean USearchHelper::HandleDialogKey(LView *inView, const EventRecord &inKeyEvent)
{
	Int16 theKey = inKeyEvent.message & charCodeMask;
	
	if ( ((theKey == char_Enter) || (theKey == char_Return)) )
	{
		
		USearchHelper::FindViewControl(inView, paneID_Okay)->SimulateHotSpotClick(kControlButtonPart);
		return true;
	} else if ( (((theKey == char_Escape) && ((inKeyEvent.message & keyCodeMask) == vkey_Escape)) ||
				 UKeyFilters::IsCmdPeriod(inKeyEvent)) )
{
	
		USearchHelper::FindViewControl(inView, paneID_Cancel)->SimulateHotSpotClick(kControlButtonPart);
		return true;
	}
	
	return false;
}


/*======================================================================================
	Set the descriptor for the pane if it is different from the current descriptor.
======================================================================================*/

void USearchHelper::SetPaneDescriptor(LPane *inPane, const CStr255& inDescriptor)
{
	CStr255 currentDesc;
	inPane->GetDescriptor(currentDesc);
	
	if ( currentDesc != inDescriptor )
	{
		inPane->SetDescriptor(inDescriptor);
	}
}


/*======================================================================================
	Set the default status of the specified button.
======================================================================================*/

void USearchHelper::SetDefaultButton(LView *inView, PaneIDT inID, Boolean inIsDefault)
{
	LControl *theControl = USearchHelper::FindViewControl(inView, inID);
	LGAPushButton *pushButton = dynamic_cast<LGAPushButton *>(theControl);
	Assert_(pushButton != nil);
	
	if ( pushButton != nil ) pushButton->SetDefaultButton(inIsDefault, true);
}


/*======================================================================================
	If the specified edit field is not already the target, select its text and make it 
	the current target.
======================================================================================*/

void USearchHelper::SelectEditField(LEditField *inEditField)
{
	if ( !inEditField->IsTarget() && inEditField->IsVisible() && 
		 LCommander::SwitchTarget(inEditField) ) {	// Become the current target
		{
			StExcludeVisibleRgn emptyRgn(inEditField);
			inEditField->SelectAll();	// Select all text
		}
		inEditField->Refresh();
	}
}


/*======================================================================================
	If the specified date view is not already the target, select its text and make it 
	the current target.
======================================================================================*/

void USearchHelper::SelectDateView(CDateView *inDateView)
{
	if ( !inDateView->ContainsTarget() && inDateView->IsVisible() ) {	// Become the current target
		{
			StExcludeVisibleRgn emptyRgn(inDateView);
			inDateView->Select();
		}
		inDateView->Refresh();
	}
}


/*======================================================================================
	Determine if the last clicked pane interacted with the user in some way (return true)
	or if the last click was just a dead click.
======================================================================================*/

Boolean USearchHelper::LastPaneWasActiveClickPane()
{
	LPane *lastPane = LPane::GetLastPaneClicked();
	
	if ( lastPane == nil ) return false;

	// Check for any control
	void *result = dynamic_cast<LControl *>(lastPane);
	
	if ( result == nil )
	{
		// Check for commander
		result = dynamic_cast<LCommander *>(lastPane);
	}

	return (result != nil);
}


/*======================================================================================
	Recalculate the bounds for the window based upon increment amounts for changing the
	window's size. Make sure that the new window size will not extend beyond the 
	desktop bounds. Note that this method does NOT check against the min/max size for
	the window, just as the LWindow::DoSetBounds() doesn't. The inSizeBoxLeft and
	inSizeBoxTop parameters represent the distance between the bottom, right of the 
	window and the top, left corner of the size box (always >= 0).
======================================================================================*/

void USearchHelper::CalcDiscreteWindowResizing(LWindow *inWindow, Rect *ioNewBounds, 
											   const Int16 inHIncrement, const Int16 inVIncrement, 
											   Boolean inCheckDesktop, Int16 inSizeBoxLeft,
											   Int16 inSizeBoxTop)
{
	Rect currentBounds, minMaxSizes;
	
	CSaveWindowStatus::GetPaneGlobalBounds(inWindow, &currentBounds);
	inWindow->GetMinMaxSize(minMaxSizes);

	Assert_((inSizeBoxLeft >= 0) && (inSizeBoxTop >= 0));
	const Int16 curHeight = currentBounds.bottom - currentBounds.top;
	const Int16 curWidth = currentBounds.right - currentBounds.left;
	const Int16 vResizeAmount = ioNewBounds->bottom - ioNewBounds->top - curHeight;
	const Int16 hResizeAmount = ioNewBounds->right - ioNewBounds->left - curWidth;
								
	if ( (vResizeAmount != 0) || (hResizeAmount != 0) )
	{
		Int16 vDelta = vResizeAmount % inVIncrement;
		Int16 hDelta = hResizeAmount % inHIncrement;
		Int16 vResize = vResizeAmount, hResize = hResizeAmount;
		
		if ( vDelta != 0 )
		{
			if ( vDelta < 0 ) vDelta = -vDelta;
			vDelta = ((vDelta > (inVIncrement>>1)) ? inVIncrement - vDelta : -vDelta);
			vResize += (vResizeAmount < 0) ? -vDelta : vDelta;
		}
		if ( hDelta != 0 )
		{
			if ( hDelta < 0 ) hDelta = -vDelta;
			hDelta = ((hDelta > (inVIncrement>>1)) ? inHIncrement - hDelta : -hDelta);
			hResize += (hResizeAmount < 0) ? -hDelta : hDelta;
		}
		
		AssertFail_(!(vResize % inVIncrement));	// Should be integral
		AssertFail_(!(hResize % inHIncrement));	// Should be integral

		if ( inCheckDesktop )
		{
			// Make sure we don't extend beyond the bounds of the desktop
			Point desktopBotRight = botRight((**(::GetGrayRgn())).rgnBBox);
			if ( (vResize > 0) && 
				 (currentBounds.bottom + vResize + cWindowDesktopMargin - inSizeBoxTop) > desktopBotRight.v )
			{
				vResize -= inVIncrement;
			}
			if ( (hResize > 0) && 
				 (currentBounds.right + hResize + cWindowDesktopMargin - inSizeBoxLeft) > desktopBotRight.h )
			{
				hResize -= inHIncrement;
			}
		}
		
		ioNewBounds->bottom = ioNewBounds->top + curHeight + vResize;
		ioNewBounds->right = ioNewBounds->left + curWidth + hResize;

		// Check against min/max sizes
		
		Int16 newHeight = ioNewBounds->bottom - ioNewBounds->top;
		Int16 newWidth = ioNewBounds->right - ioNewBounds->left;
		
		if ( newHeight < minMaxSizes.top )
		{
			ioNewBounds->bottom += inVIncrement;
		} else if ( newHeight > minMaxSizes.bottom )
		{
			ioNewBounds->bottom -= inVIncrement;
		}
		if ( newWidth < minMaxSizes.left )
		{
			ioNewBounds->right += inHIncrement;
		} else if ( newWidth > minMaxSizes.right )
		{
			ioNewBounds->right -= inHIncrement;
		}
	}
}

/*======================================================================================
	Scroll the views bit without redrawing the update region with the window's background
	color.
======================================================================================*/

void USearchHelper::ScrollViewBits(LView *inView, Int32 inLeftDelta, Int32 inTopDelta)
{
	if ( !inView->FocusDraw() ) return;
	
	if ( sStdProcs.textProc == ((QDTextUPP) -1L) )
	{
		SetStdCProcs(&sStdProcs);
		sStdProcs.rectProc = NewQDRectProc(DoNothingStdRect);
		sStdProcs.rgnProc = NewQDRgnProc(DoNothingStdRgn);
		if ( !sStdProcs.rectProc || !sStdProcs.rgnProc )
		{
			ForgetRoutineDesc((UniversalProcPtr *) &sStdProcs.rectProc);
			ForgetRoutineDesc((UniversalProcPtr *) &sStdProcs.rgnProc);
			sStdProcs.textProc = (QDTextUPP) -1L;
			FailOSErr_(memFullErr);
		}
	}

	Rect frame;
	Point offset;
	
	// Get Frame in Port coords
	inView->GetRevealedRect(frame);
	offset = topLeft(frame);
	inView->PortToLocalPoint(topLeft(frame));
	inView->PortToLocalPoint(botRight(frame));
	offset.h = offset.h - frame.left;
	offset.v = offset.v - frame.top;

	// Scroll Frame, clipping to the update region
	RgnHandle updateRgnH = NewRgn();
	
	GrafPtr currentPort = UQDGlobals::GetCurrentPort();

	if ( currentPort->grafProcs )
	{
		QDRectUPP saveStdRect = currentPort->grafProcs->rectProc;
		QDRgnUPP saveStdRgn = currentPort->grafProcs->rgnProc;
		currentPort->grafProcs->rectProc = sStdProcs.rectProc;
		currentPort->grafProcs->rgnProc = sStdProcs.rgnProc;
		::ScrollRect(&frame, -inLeftDelta, -inTopDelta, updateRgnH);
		currentPort->grafProcs->rectProc = saveStdRect;
		currentPort->grafProcs->rgnProc = saveStdRgn;
	} else
	{
		QDProcsPtr saveProcs = currentPort->grafProcs;
		currentPort->grafProcs = (QDProcsPtr) &sStdProcs;
		::ScrollRect(&frame, -inLeftDelta, -inTopDelta, updateRgnH);
		currentPort->grafProcs = saveProcs;
	}
		
	// Force redraw of update region
	::OffsetRgn(updateRgnH, offset.h, offset.v);
	inView->InvalPortRgn(updateRgnH);
	::DisposeRgn(updateRgnH);
}


/*======================================================================================
	KeyIsDown

	Determine whether or not the specified key is being pressed. Keys are specified by 
	hardware-specific key scan code (NOT the character). See PP_KeyCodes.h.
======================================================================================*/

Boolean USearchHelper::KeyIsDown(Int16 inKeyCode)
{
	KeyMap theKeys;

	::GetKeys(theKeys);

	return (::BitTst(((char *) &theKeys) + inKeyCode / 8, (long) 7 - (inKeyCode % 8)));
}


/*======================================================================================
	Show a generic exception alert.
======================================================================================*/

void USearchHelper::GenericExceptionAlert(OSErr inErr)
{
	CStr255 errorString;
	CStr31 numString;
	::NumToString(inErr, numString);
	USearchHelper::AssignUString(uStr_ExceptionErrorOccurred, errorString);
	::StringParamText(errorString, numString);
	::SetCursor(&qd.arrow);
	ErrorManager::PlainAlert((StringPtr) errorString);
	::HiliteMenu(0);
}


#ifdef NOT_YET
/*======================================================================================
	Return a display name for the specified folder name.
======================================================================================*/

LStr255 *USearchHelper::GetFolderDisplayName(const char *inFolderName, 
											 LStr255 *outFolderName)
{
	outFolderName->Assign(inFolderName);
	P2CStr(((StringPtr) *outFolderName));
	NET_UnEscape((char *) ((StringPtr) *outFolderName));
	C2PStr((char *) ((StringPtr) *outFolderName));
	return outFolderName;
}


#endif // NOT_YET


#pragma mark -

//-----------------------------------
CSearchPopupMenu::CSearchPopupMenu(LStream *inStream) :
	CGAIconPopup(inStream),	mCommands(sizeof(CommandT)),
	mLastPopulateID(0), mDefaultItem(1), mOldValue( 1 )
//-----------------------------------
{
}

//-----------------------------------
CommandT 
CSearchPopupMenu::GetOldItemCommand() const
//-----------------------------------
{
	return mOldValue;
}

//-----------------------------------
void CSearchPopupMenu::PopulateMenu(MSG_SearchMenuItem *inMenuItems, const Int16 inNumItems,
		CommandT inNewCommandIfInvalid, UInt32 inPopulateID)
//	Populate the menu with the specified items. The strings in inMenuItems have been
//	converted to pascal strings. The MSG_SearchMenuItem.attrib field is used for the 
//	menu command. inNewCommandIfInvalid specifies the new menu item to make current 
//	if the currently selected menu item becomes invalid. inPopulateID is a unique
//	non-zero identifier used for determining whether or not we actually need to repopulate
//	the menu. If inPopulateID is 0, the menu is always repopulated.
//-----------------------------------
{
	// We need to remember here, that since the menu is cached, it might not jive with
	// the command array (i.e. menu items might have been set in another instance of
	// this class). The mCommands array store data related to our state.
	
	MenuHandle menuH = GetMacMenuH();
	Int16 startNumMenuItems = ::CountMItems(menuH);
	Int16 startNumCommands = mCommands.GetCount();
	
//	if ( (inPopulateID != 0) && (inPopulateID == mLastPopulateID) )
//	{
//		AssertFail_(startNumMenuItems == inNumItems);
//		AssertFail_(startNumCommands == inNumItems);
//		return;	// Already populated!
//	}
	
	const Int16 curItem = GetValue();
	Boolean curItemChanged = (startNumCommands > 0) ? false : true;
	Boolean curItemTextChanged = (startNumCommands == 0) && (inNumItems > 0);
	mDefaultItem = 1;
	
	// Do we need to adjust the menu?
	if ( inNumItems < startNumMenuItems )
	{
		do
		{
			::DeleteMenuItem(menuH, startNumMenuItems--);
		} while ( startNumMenuItems != inNumItems );
	}
	// Do we need to adjust the command array?
	if ( inNumItems < startNumCommands )
	{
		Int16 deleteStartIndex = inNumItems + 1;
		if ( IsBetween(curItem, deleteStartIndex, startNumCommands) )
		{
			curItemChanged = true;
		}
		mCommands.RemoveItemsAt(LArray::index_Last, deleteStartIndex);
		startNumCommands = inNumItems;
	} else if ( inNumItems > startNumCommands )
	{
		mCommands.AdjustAllocation(inNumItems - startNumCommands);
	}
	
	// Add new items
	MSG_SearchMenuItem *curMenuItem = inMenuItems;
	
	for (Int32 i = 1; i <= inNumItems; ++i, ++curMenuItem)
	{
		CommandT command = curMenuItem->attrib;		
		if ( inNewCommandIfInvalid == command && curMenuItem->isEnabled)
		{
			mDefaultItem = i;
		}
		// Do we need to adjust the menu?
		if ( i <= startNumMenuItems )
		{
			// There is already an item in the menu
			CStr255 curMenuString; // Strings are not necessary 31 character file names
			::GetMenuItemText(menuH, i, curMenuString);
			if ( !::EqualString(curMenuString, (UInt8 *) curMenuItem->name, true, true) )
			{
				::SetMenuItemText(menuH, i, (UInt8 *) curMenuItem->name);
				if ( i == curItem )
				{
					curItemTextChanged = true;
				}
// the isEnabled flag is obsolete and basically random.
//				if ( curMenuItem->isEnabled )
//				{
//					::EnableItem(menuH, i);
//				} else
//				{
//					::DisableItem(menuH, i);
//				}
			}
		} else
		{
			::AppendMenu(menuH, (UInt8 *) curMenuItem->name);
			// curMenuItem->name can have meta character data in it
			// So this really is necessary
			::SetMenuItemText( menuH, i, (UInt8 *) curMenuItem->name);
// the isEnabled flag is obsolete and basically random.
//			if ( !curMenuItem->isEnabled )
//			{
//				::DisableItem(menuH, i);
//			}
		}
		// Do we need to adjust the commands?
		if ( i <= startNumCommands )
		{
			// There is already an item in the menu
			mCommands.AssignItemsAt(1, i, &command);
			if ( !curMenuItem->isEnabled && (i == curItem) )
			{
				curItemChanged = true;
			}
		} else
		{
			mCommands.InsertItemsAt(1, i, &command);
		}
	}
	
	if ( inNumItems != startNumCommands )
	{
		SetPopupMinMaxValues();
	}
	
	if ( curItemChanged )
	{
		mValue = mDefaultItem;
		RefreshMenu();
	} else if ( curItemTextChanged )
	{
		RefreshMenu();
	}
	
	mLastPopulateID = inPopulateID;
}


/*======================================================================================
	Get the command for the specified menu item. Pass 0 for inMenuItem to get the
	currently selected menu item. If the index is invalid, return cmd_InvalidMenuItem.
======================================================================================*/

CommandT CSearchPopupMenu::GetCurrentItemCommandNum(Int16 inMenuItem)
{
	ArrayIndexT index = (inMenuItem > 0) ? inMenuItem : mValue;
	CommandT command = cmd_InvalidMenuItem;
	
	// check if valid index
	if( mCommands.ValidIndex(index) )
		command = *((CommandT *) mCommands.GetItemPtr(index));
	else
		return mOldValue;
	
	if( command == 	cmd_InvalidMenuItem )
		return mOldValue;
	else
		return command;
		
//	return cmd_InvalidMenuItem;
}


/*======================================================================================
	Select the item with the specified command.
======================================================================================*/

void CSearchPopupMenu::SetCurrentItemByCommand(CommandT inMenuCommand)
{
	Int16 index = GetCommandItemIndex(inMenuCommand);
	if ( index && (GetValue() != index) )
	{
		SetValue(index);
	}
}



/*======================================================================================
	Refresh instead of drawing immediately. Also fixes bug in PowerPlant if an exception
	is raised when broadcasting the value message.
======================================================================================*/

void CSearchPopupMenu::SetValue(Int32 inValue)
{
	CommandT		menuCommand;
	bool 			success = mCommands.FetchItemAt( inValue, &menuCommand );
// ***	const Int16 	oldValue = GetValue();
		mOldValue = GetValue();	
				
//	if ( oldValue != inValue )
	if ( mOldValue != inValue )
	{
		RefreshMenu();
		MenuHandle menuH = GetMacMenuH();
		if( menuCommand == CSearchManager::eCustomizeSearchItem && success )
		{
			mValue = inValue;
			BroadcastValueMessage();
//			inValue = oldValue;
			inValue = mOldValue;
			FocusDraw();
		}
		if ( menuH ) SetupCurrentMenuItem(menuH, inValue);
		Try_
		{
			LControl::SetValue(inValue);
		}
		Catch_(inErr)
		{
			// Reset the old value, new value is unacceptable
//			if ( menuH ) SetupCurrentMenuItem(menuH, oldValue);
			if ( menuH ) SetupCurrentMenuItem(menuH, mOldValue);
//			mValue = oldValue;
			mValue = mOldValue;
			SetHiliteState(false);
			Throw_(inErr);
		}
		EndCatch_
	}
}


/*======================================================================================
	Get the index of the item with the specified command.
======================================================================================*/

Int16 CSearchPopupMenu::GetCommandItemIndex(CommandT inMenuCommand)
{
	ArrayIndexT index = mCommands.FetchIndexOf(&inMenuCommand);
	
	return ((index != LArray::index_Bad) ? index : 0);
}


/*======================================================================================
	Clear all items in the menu.
======================================================================================*/

void CSearchPopupMenu::ClearMenu(Int16 inStartItem)
{
	if ( GetNumCommands() < inStartItem ) return;
	
	mCommands.RemoveItemsAt(LArray::index_Last, inStartItem);
	MenuHandle menuH = GetMacMenuH();
	Int16 numMenuItems = ::CountMItems(menuH);
	while ( numMenuItems >= inStartItem )
	{
		::DeleteMenuItem(menuH, numMenuItems--);
	}
	SetPopupMinMaxValues();
}


/*======================================================================================
	Append the specified menu item to the menu.
======================================================================================*/

void CSearchPopupMenu::AppendMenuItemCommand(CommandT inCommand, ConstStr255Param inName, 
									  		 Boolean inIsEnabled)
{
	MenuHandle menuH = GetMacMenuH();
	Int16 numMenuItems = ::CountMItems(menuH);
	
	AssertFail_(numMenuItems == mCommands.GetCount());

	mCommands.InsertItemsAt(1, LArray::index_Last, &inCommand);
	::AppendMenu(menuH, inName);
	// I assume that we don't want Metacharacter data
	::SetMenuItemText( menuH, numMenuItems+1, inName );
	if ( !inIsEnabled )
	{
		::DisableItem(menuH, numMenuItems + 1);
	}
	SetPopupMinMaxValues();
}


/*======================================================================================
	Set the specified menu item in the menu.
======================================================================================*/

void CSearchPopupMenu::SetMenuItemCommand(CommandT inOldCommand, CommandT inNewCommand,
										  ConstStr255Param inName, Boolean inIsEnabled)
{
	Int16 index = GetCommandItemIndex(inOldCommand);
	if ( !index ) return;	// Not found!
	
	Boolean doRefresh = (GetValue() == index);
	
	MenuHandle menuH = GetMacMenuH();
	Int16 numMenuItems = ::CountMItems(menuH);
	
	AssertFail_(numMenuItems == mCommands.GetCount());

	mCommands.AssignItemsAt(1, index, &inNewCommand);
	
	if ( doRefresh )
	{
		Str255 tempString;
		::GetMenuItemText(menuH, index, tempString);
		doRefresh = !::EqualString(tempString, inName, true, true);
	}
	::SetMenuItemText(menuH, index, inName);
	
	if ( inIsEnabled )
	{
		::EnableItem(menuH, numMenuItems + 1);
	} else
	{
		::DisableItem(menuH, numMenuItems + 1);
	}
	if ( doRefresh )
	{
		RefreshMenu();
	}
}


/*======================================================================================
	Remove the specified menu item from the menu.
======================================================================================*/

void CSearchPopupMenu::RemoveMenuItemCommand(CommandT inRemoveCommand, 
											 CommandT inNewCommandIfInvalid)
{
	Int16 index = GetCommandItemIndex(inRemoveCommand);
	if ( !index ) return;	// Not found!
	
	Boolean doRefresh = (GetValue() == index);
	
	MenuHandle menuH = GetMacMenuH();
	Int16 numMenuItems = ::CountMItems(menuH);
	
	AssertFail_(numMenuItems == mCommands.GetCount());

	mCommands.RemoveItemsAt(1, index);
	::DeleteMenuItem(menuH, index);
	
	SetPopupMinMaxValues();
	
	if ( doRefresh )
	{
		index = GetCommandItemIndex(inNewCommandIfInvalid);
		if ( !index ) index = 1;
		mValue = index;
		RefreshMenu();
	}
}


/*======================================================================================
	Finish creating the search popup menu.
======================================================================================*/

void CSearchPopupMenu::FinishCreateSelf()
{
	Inherited::FinishCreateSelf();
	
	MenuHandle menuH = GetMacMenuH();
	Int16 numMenuItems = ::CountMItems(menuH);
	
	if ( numMenuItems > 0 )
	{
		AssertFail_(mCommands.GetCount() == 0);
		CommandT command = 0;
		mCommands.InsertItemsAt(numMenuItems, LArray::index_Last, &command);
	}
}


/*======================================================================================
	Send a reference to ourselves as the param.
======================================================================================*/

void CSearchPopupMenu::BroadcastValueMessage()
{
	BroadcastMessage(mValueMessage, this);
}


/*======================================================================================
	Don't do a double draw of the popup.
======================================================================================*/

Boolean CSearchPopupMenu::TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers)
{
	Boolean rtnVal = Inherited::TrackHotSpot(inHotSpot, inPoint, inModifiers);
	if ( rtnVal ) DontRefresh();
	
	return rtnVal;
}


#pragma mark -
/*======================================================================================
	Just update the text area. Don't allow more than mMaxChars to be set.
======================================================================================*/

void CSearchEditField::SetDescriptor(ConstStr255Param inDescriptor)
{
	{
		Str255 curDesc;
		GetDescriptor(curDesc);
		if ( ::EqualString(inDescriptor, curDesc, true, true) ) return;
	}

	Int16 length = -1;
	if ( inDescriptor[0] > mMaxChars )
	{
		length = inDescriptor[0];
		((UInt8 *) inDescriptor)[0] = mMaxChars;
	}
	
	::TESetText(inDescriptor + 1, inDescriptor[0], mTextEditH);
	
	if ( length >= 0 ) ((UInt8 *) inDescriptor)[0] = length;
	
	Draw(nil);

	BroadcastValueMessage();
}


/*======================================================================================
	Set the text for the field. This can be used for text that is greater than 255 in 
	length.
======================================================================================*/

void CSearchEditField::SetText(const char *inText)
{
	if( inText != NULL )
	{
		Int16 length = LString::CStringLength(inText);
		if ( length > mMaxChars ) length = mMaxChars;
		
		::TESetText(inText, length, mTextEditH);
	}
	Draw(nil);

	BroadcastValueMessage();
}


/*======================================================================================
	Get the text for the field. This can be used for text that is greater than 255 in 
	length. The returned string is null-terminated.
======================================================================================*/

char *CSearchEditField::GetText(char *outText)
{
	CharsHandle theText = ::TEGetText(mTextEditH);
	Int16 length = ::GetHandleSize(theText);
	Assert_(length <= mMaxChars);

	::BlockMoveData(*theText, outText, length);
	
	outText[length] = 0;
	
	return outText;
}


/*======================================================================================
	Just invalidate the right side of the pane.
======================================================================================*/

static const Int16 cRefreshEditMargin = 4;
void CSearchEditField::ResizeFrameBy(Int16 inWidthDelta, Int16 inHeightDelta, 
									 Boolean inRefresh)
{
	Assert_(!inHeightDelta);
	if ( !inWidthDelta ) return;
	
	Inherited::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	
	if ( inRefresh && IsVisible() )
	{
		Rect invalRect;
		CalcPortFrameRect(invalRect);
		::InsetRect(&invalRect, -1, -1);	// Make bigger for shadows
		if ( inWidthDelta < 0 )
		{
			invalRect.left = invalRect.right - cRefreshEditMargin;
			invalRect.right -= inWidthDelta;
		} else
		{
			invalRect.left = invalRect.right - inWidthDelta - cRefreshEditMargin;
		}
		InvalPortRect(&invalRect);
	}
}


/*======================================================================================
	Don't call LBroadcasterEditField::HandleKeyPress() since it intecepts Enter and 
	Return keys.
======================================================================================*/

Boolean CSearchEditField::HandleKeyPress(const EventRecord &inKeyEvent)
{
	return LBroadcasterEditField::LEditField_HandleKeyPress(inKeyEvent);
}


#pragma mark -
/*======================================================================================
	Only rotate the target if mCanRotate is true. Also, fix a bug in PowerPlant.
======================================================================================*/

void CSearchTabGroup::RotateTarget(Boolean inBackward)
{
	if ( (mCanRotate != nil) && !(*mCanRotate) ) return;
	
	LCommander *onDutySub = GetTarget();	//GetOnDutySub();
	Int32 pos = (onDutySub != nil) ? mSubCommanders.FetchIndexOf(onDutySub) :
									 mSubCommanders.GetCount() + 1;

	if ( (pos > 1) || (onDutySub != nil) ) {	
		Boolean switched = false;
		Int32 startingPos = pos;
		if ( startingPos > mSubCommanders.GetCount() )
		{
			startingPos = mSubCommanders.GetCount();
		} else if ( startingPos < 0 )
		{
			startingPos = 0;
		}
		LCommander *newTarget;		
		do
		{
			if (inBackward)
			{
				if (--pos <= 0)
									{
										// Wrap around to last cmdr
					pos = mSubCommanders.GetCount();
				}
				
			} else
			{
				if (++pos > mSubCommanders.GetCount())
				{
					pos = 1;			// Wrap around to first cmdr
				}
			}
										// Find SubCommander by position
										//   and see if it wants to become
										//   the new target
			mSubCommanders.FetchItemAt(pos, newTarget);
			switched = newTarget->ProcessCommand(msg_TabSelect);
	
		} while (!switched && (pos != startingPos));
			
		if (switched) {					// We found a willing subcommander
			SwitchTarget(newTarget);
		}
	}
}


#pragma mark -
/*======================================================================================
	Draw when disabled as grayish text.
======================================================================================*/

void CSearchCaption::DrawSelf()
{
	Rect frame;
	CalcLocalFrameRect(frame);
	
	Int16 just = UTextTraits::SetPortTextTraits(mTxtrID);
	
	RGBColor textColor;
	::GetForeColor(&textColor);
	
	ApplyForeAndBackColors();
	
	if ( IsEnabled() )
	{
		::RGBForeColor(&textColor);
		UTextDrawing::DrawWithJustification((Ptr) &mText[1], mText[0], frame, just);
	} else
	{
		StDeviceLoop theLoop(frame);
		Int16 depth;
		while ( theLoop.NextDepth(depth) )
		{
			if ( depth < 4 )
			{
				::TextMode(grayishTextOr);
			} else
			{
				textColor = UGAColorRamp::GetColor(7);
				::RGBForeColor(&textColor);
			}
			UTextDrawing::DrawWithJustification((Ptr) &mText[1], mText[0], frame, just);
		}
		::TextMode(srcOr);
		::ForeColor(blackColor);
	}
}



#pragma mark -

RgnHandle StSectClipRgnState::sClipRgn = nil;
Boolean StSectClipRgnState::sSetClip = false;

/*======================================================================================
	Clip to the intersection of the BOUNDS of the current clipping region. Don't create
	more than one of these objects at a time! If inDisplayRect is non-nil, the routine
	first checks to see if inDisplayRect is completely within inLocalRect AND the
	current clipping region, and if so, does nothing.
======================================================================================*/

StSectClipRgnState::StSectClipRgnState(const Rect *inLocalRect, const Rect *inDisplayRect)
{
	Assert_(!sSetClip);
	
	if ( inDisplayRect != nil )
	{
		if ( ((**UQDGlobals::GetCurrentPort()->clipRgn).rgnSize == sizeof(Region)) &&
			 RectEnclosesRect(&(**UQDGlobals::GetCurrentPort()->clipRgn).rgnBBox, inDisplayRect) &&
			 RectEnclosesRect(inLocalRect, inDisplayRect) )
		{
			
			return;	// All's well
		}
	}
	if ( sClipRgn == nil ) sClipRgn = ::NewRgn();
	if ( sClipRgn != nil )
	{
		::GetClip(sClipRgn);
		Rect newClip;
		::SectRect(inLocalRect, &(**sClipRgn).rgnBBox, &newClip);
		::ClipRect(&newClip);
		sSetClip = true;
	}
}


/*======================================================================================
	Restore original clip region.
======================================================================================*/

StSectClipRgnState::~StSectClipRgnState()
{
	
	if ( sSetClip )
	{
		Assert_(sClipRgn != nil);
		::SetClip(sClipRgn);
		if ( (**sClipRgn).rgnSize > 128 )
		{
			::EmptyRgn(sClipRgn);
		}
		sSetClip = false;
	}
}


/*======================================================================================
	Return true if inInsideRect is completely enclosed by inEnclosingRect.
======================================================================================*/

Boolean StSectClipRgnState::RectEnclosesRect(const Rect *inEnclosingRect, 
											 const Rect *inInsideRect)
{
	
	return ((inEnclosingRect->left <= inInsideRect->left) && 
			(inEnclosingRect->top <= inInsideRect->top) && 
			(inEnclosingRect->right >= inInsideRect->right) && 
			(inEnclosingRect->bottom >= inInsideRect->bottom));
}


#pragma mark -

#if 0
/*======================================================================================
	Determine if the specified cell is selected.
======================================================================================*/

Boolean CRowTableSelector::CellIsSelected(const STableCell &inCell) const
{
	if ( GetSelectedRowCount() > 0 )
{
	
		AssertFail_(GetCount() == GetSelectedRowCount());
		UInt32 id = GetRowUniqueID(inCell.row);
	
		if ( id != eInvalidID )
		{
			return (FetchIndexOf(&id) != LArray::index_Bad);
		} 
	}
		
	return false;
}


/*======================================================================================
	Select the specified cell.
======================================================================================*/

void CRowTableSelector::DoSelect(const TableIndexT inRow, Boolean inSelect, 
								 Boolean inHilite, Boolean inNotify)
{
	UInt32 id = GetRowUniqueID(inRow);
	if ( id == eInvalidID ) return;
	
	STableCell theCell(inRow, 1);
	const ArrayIndexT index = FetchIndexOf(&id);
	
	if ( (index != LArray::index_Bad) == inSelect ) return;
	
	if ( inSelect && !mAllowMultiple )
	{
		if ( mSelectedRowCount > 0 )
		{
			DoSelectAll(false, false);
			mSelectedRowCount = 0; // to be safe
		}
	}

	// Maintain the count
	if ( inSelect )
	{
		InsertItemsAt(1, LArray::index_First, &id);
		++mSelectedRowCount;
	}

	if ( inHilite )
	{
		mTableView->HiliteCell(theCell, inSelect);
		
		/*
		// hilite (or unhilite) the entire row
		UInt32 nCols, nRows;
			
		mTableView->GetTableSize(nRows, nCols);
			
		theCell.row = inRow;
		for (theCell.col = 1;  theCell.col <= nCols; theCell.col++)
		{
			mTableView->HiliteCell(theCell, inSelect);
		}
		*/
	}
	
	if ( !inSelect )
	{
		RemoveItemsAt(1, index);
		--mSelectedRowCount;
	}
				
	if ( inNotify ) mTableView->SelectionChanged();
}


/*======================================================================================
	Remove the specified cells from the table.
======================================================================================*/

void CRowTableSelector::RemoveRows(Uint32 inHowMany, TableIndexT inFromRow)
{
	Boolean notify = false;
	Assert_(mSelectedRowCount == mItemCount);
		// mSelectedRowCount is inherited from LTableRowSelector.
		// mItemCount is inherited from LArray.
		// They should always agree!
	if (mRowCount== 0)
	{
		AssertFail_(mRowCount >= mSelectedRowCount); // # selected <= # total rows
		TableIndexT cols;
		mTableView->GetTableSize(mRowCount, cols);
		return; // nothing to do.
	}
	if ( inHowMany >= mRowCount )
	{
		// Removing all rows from the table
		AssertFail_(mRowCount >= mSelectedRowCount); // # selected <= # total rows
		mRowCount = 0; // maintain the table count
		if (mItemCount > 0) // clear the array
			RemoveItemsAt(LArray::index_Last, LArray::index_First);
		notify = (mSelectedRowCount > 0);
		mSelectedRowCount = 0;
	}
	else
	{
		UInt32 curRow, nRows = mRowCount; // (table row count)
		if ( nRows > (inFromRow + inHowMany - 1) )
			nRows = (inFromRow + inHowMany - 1);
		for (curRow = inFromRow; curRow <= nRows; ++curRow)
		{
			UInt32 id = GetRowUniqueID(curRow);
			ArrayIndexT index = FetchIndexOf(&id);
			if ( index != LArray::index_Bad )
			{
				RemoveItemsAt(1, index);
				--mSelectedRowCount;
				notify = true;
			}
		}
		AssertFail_(mRowCount >= inHowMany);
		mRowCount -= inHowMany;
	}
	
	if ( notify ) mTableView->SelectionChanged();
}


/*======================================================================================
	Return the first selected cell's row, defined as the min row & col (closest to
  	top-left corner). Return 0 if no row is selected.
======================================================================================*/

TableIndexT CRowTableSelector::GetFirstSelectedRow() const
{
	Int32 count = GetCount();
	TableIndexT row = 0x7FFFFFFF;
	
	if ( count > 0 )
{
	
		Int32 curItem = 1;
		
		do
		{
			TableIndexT curRow = GetUniqueIDRow(*((UInt32 *) GetItemPtr(curItem)));
			if ( curRow < row ) row = curRow;
		} while ( ++curItem <= count );
	}
	
	return ((row == 0x7FFFFFFF) ? 0 : row);
}


/*======================================================================================
	Return an array of selected row indices with the base given by inBase. The caller
	must dispose of the returned memory using ::DisposePtr(). The function returns
	the number of elements in outIndices, and 0 if no elements are selected and
	nothing is allocated.
======================================================================================*/

TableIndexT CRowTableSelector::GetSelectedIndices(TableIndexT **outIndices, Int16 inBase) const
{
	TableIndexT count = GetCount();
	*outIndices = nil;
	
	if ( count > 0 )
{
	
		*outIndices = (TableIndexT *) XP_ALLOC(count * sizeof(TableIndexT));
		FailNIL_(*outIndices);
		TableIndexT curItem = 1;
		
		do
		{
			(*outIndices)[curItem - 1] = 
				GetUniqueIDRow(*((UInt32 *) GetItemPtr(curItem))) - 1 + inBase;
		} while ( ++curItem <= count );
	}
	
	return count;
}
 #endif

#pragma mark -
#if 0
/*======================================================================================
	Adjust the window to stored preferences.
======================================================================================*/

void CExTable::ReadSavedTableStatus(LStream *inStatusData)
{
	if ( inStatusData != nil )
	{
		
		AssertFail_(mTableHeader != nil);

		mTableHeader->ReadColumnState(inStatusData);
	}
}


/*======================================================================================
	Get window stored preferences.
======================================================================================*/

void CExTable::WriteSavedTableStatus(LStream *outStatusData)
{
	AssertFail_(mTableHeader != nil);

	mTableHeader->WriteColumnState(outStatusData);
}


/*======================================================================================
	Find status. Override it because of the overload in the inherited class.
======================================================================================*/

void CExTable::FindCommandStatus(CommandT inCommand, Boolean &outEnabled, Boolean &outUsesMark,
						   		 Char16 &outMark, Str255 outName)
{
	CStandardFlexTable::FindCommandStatus(inCommand, outEnabled, outUsesMark, 
										  outMark, outName);
}


/*======================================================================================
	Obey commands.
======================================================================================*/

Boolean CExTable::ObeyCommand(CommandT inCommand, void *ioParam)
{
	Boolean cmdHandled = true;

	switch (inCommand)
	{
		
		case msg_TabSelect:
			break;

		default:
			cmdHandled = CStandardFlexTable::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return cmdHandled;
}


/*======================================================================================
	Draw when disabled as grayish text.
======================================================================================*/

void CExTable::ScrollBits(Int32 inLeftDelta, Int32 inTopDelta)
{
	USearchHelper::ScrollViewBits(this, inLeftDelta, inTopDelta);
}


/*======================================================================================
	Draw the table.
======================================================================================*/

void CExTable::DrawSelf()
{
	UTextTraits::SetPortTextTraits(mTextTraitsID);

	RgnHandle localUpdateRgnH = GetLocalUpdateRgn();
	Rect updateRect = (**localUpdateRgnH).rgnBBox;
	::DisposeRgn(localUpdateRgnH);
	
	STableCell topLeftCell, botRightCell;
	FetchIntersectingCells(updateRect, topLeftCell, botRightCell);
	
	Boolean isActive = IsActive() && IsTarget(); 
	
	STableCell cell(1, 1);
	for (cell.row = topLeftCell.row; cell.row <= botRightCell.row; cell.row++)
	{
		DrawCellRow(cell, false, isActive);
	}

}


/*======================================================================================
	Activate the table.
======================================================================================*/

void CExTable::ActivateSelf()
{
	FocusDrawSelectedCells(true);
	// Do nothing
}


/*======================================================================================
	Deactivate the table.
======================================================================================*/

void CExTable::DeactivateSelf()
{
	// Do nothing
}


/*======================================================================================
	Become the current target.
======================================================================================*/

void CExTable::BeTarget()
{
	FocusDrawSelectedCells(true);
}


/*======================================================================================
	Don't be the current target.
======================================================================================*/

void CExTable::DontBeTarget()
{
	StValueChanger<UInt8> change(mFlags, mFlags | flag_IsLoosingTarget);
	FocusDrawSelectedCells(true);
}


/*======================================================================================
	Hilite the current selection.
======================================================================================*/


void CExTable::HiliteSelection(Boolean /* inActively */, Boolean  inHilite )
{
	if ( mFlags & flag_IsDrawingSelf ) return;
	FocusDrawSelectedCells(inHilite);
}


/*======================================================================================
	Hilite the current selection.
======================================================================================*/

void CExTable::HiliteCellInactively(const STableCell &inCell, Boolean inHilite)
{
	FocusDrawSelectedCells(inHilite, &inCell);
}


/*======================================================================================
	Hilite the current selection.
======================================================================================*/

void CExTable::HiliteCellActively(const STableCell &inCell, Boolean inHilite)
{
	FocusDrawSelectedCells(inHilite, &inCell);
}


/*======================================================================================
	Focus the table and draw the specified cell.
======================================================================================*/

void CExTable::FocusDrawSelectedCells(Boolean inHilite, const STableCell *inCell)
{
	TableIndexT startRow, endRow;
	
	if ( FocusExposed() && GetVisibleRowRange(&startRow, &endRow) )
	{
		STableCell selectedCell;
		if ( inCell == nil )
		{
			selectedCell.row = startRow;
			selectedCell.col = 1;
		} else
		{
			selectedCell = *inCell;
		}
		if ( IsValidCell(selectedCell) && (selectedCell.row >= startRow) && 
			 (selectedCell.row <= endRow) )
		{
			Boolean isActive = (DrawFullHiliting() && IsActive() && IsTarget());

			StValueChanger<UInt8> change(mFlags, inHilite ? mFlags : 
							mFlags | flag_DontDrawHiliting);
			UTextTraits::SetPortTextTraits(mTextTraitsID);
			StColorPenState::Normalize();
			do
			{
				if ( CellIsSelected(selectedCell) )
				{
					DrawCellRow(selectedCell, true, isActive);
				}
			} while ( (inCell == nil) && (++selectedCell.row <= endRow) );
		}
	}
}


/*======================================================================================
	Draw the table.
======================================================================================*/

void CExTable::DrawCellRow(STableCell &inCell, Boolean inErase, Boolean inActive)
{
	TableIndexT numRows, numCols;
	GetTableSize(numRows, numCols);
	
	Rect hiliteRect, cellRect;
	
	// Draw the hiliting
	
	Boolean drawHilited = CellIsSelected(inCell) && DrawHiliting();
	
	if ( drawHilited || inErase )
	{
		GetLocalCellRect(STableCell(inCell.row, 1), hiliteRect);
		GetLocalCellRect(STableCell(inCell.row, numCols), cellRect);
		
		hiliteRect.right = cellRect.right;
	}
	
	if ( drawHilited )
	{
		RGBColor hiliteColor;
		LMGetHiliteRGB(&hiliteColor);

		Int16 depth;
		CDeviceLoop deviceLoop(hiliteRect, depth);
	
		if ( depth ) do
		{
			mBlackAndWhiteHiliting = inActive && ((depth < 4) || ((hiliteColor.red == 0) && 
													 			  (hiliteColor.green == 0) && 
													 			  (hiliteColor.blue == 0)));
			if ( mBlackAndWhiteHiliting )
			{
				::ForeColor(blackColor);
			} else
			{
				::RGBForeColor(&hiliteColor);
			}
			if ( inActive )
			{
				::PaintRect(&hiliteRect);
			} else
			{
				if ( inErase )
				{
					::InsetRect(&hiliteRect, 1, 1);
					::EraseRect(&hiliteRect);
					::InsetRect(&hiliteRect, -1, -1);
				}
				::FrameRect(&hiliteRect);
			}
			
			::ForeColor(blackColor);
			
			for (inCell.col = 1; inCell.col <= numCols; inCell.col++)
			{
				GetLocalCellRect(inCell, cellRect);
				DrawCell(inCell, cellRect);
			}
		} while ( deviceLoop.NextDepth(depth) );
		
	} else
{
	
		mBlackAndWhiteHiliting = false;	// Doesn't matter here, we aren't drawing hilited
		
		if ( inErase ) ::EraseRect(&hiliteRect);

		// Draw the cell data
	
		for (inCell.col = 1; inCell.col <= numCols; inCell.col++)
		{
			GetLocalCellRect(inCell, cellRect);
			DrawCell(inCell, cellRect);
		}
	}
	
	inCell.col = numCols;
}


/*======================================================================================
	Draw hilited text in the table.
======================================================================================*/

void CExTable::PlaceHilitedTextInRect(const char *inText, Uint32 inTextLength, const Rect &inRect,
									  Int16 inHorizJustType, Int16 inVertJustType, 
									  const FontInfo *inFontInfo, Boolean inDoTruncate,
									  TruncCode inTruncWhere)
{
	if ( inTextLength > 0 )
	{
		RGBColor foreColor;
		::GetForeColor(&foreColor);
		Boolean drawBW = GetBlackAndWhiteHiliting();
		::ForeColor(drawBW ? whiteColor : blackColor);
		UGraphicGizmos::PlaceTextInRect(inText, inTextLength, inRect, inHorizJustType,
										inVertJustType, inFontInfo, inDoTruncate,
										inTruncWhere);
		::RGBForeColor(&foreColor);
	}
}


/*======================================================================================
	Get the range of visible rows in the table's frame.
======================================================================================*/

Boolean CExTable::GetVisibleRowRange(TableIndexT *inStartRow, TableIndexT *inEndRow)
{
	Rect visRect;
	GetRevealedRect(visRect);			// Check if Table is revealed
	
	if ( !::EmptyRect(&visRect) )
	{
		PortToLocalPoint(topLeft(visRect));
		PortToLocalPoint(botRight(visRect));
		STableCell topLeftCell, botRightCell;
		FetchIntersectingCells(visRect, topLeftCell, botRightCell);
		if ( IsValidCell(topLeftCell) && IsValidCell(botRightCell) )
		{
			*inStartRow = topLeftCell.row;
			*inEndRow = botRightCell.row;
			return true;
		}
	}
	
	return false;
}

void CExTable::ChangeStarting(MSG_Pane *inPane, MSG_NOTIFY_CODE inChangeCode,
							  TableIndexT inStartRow, SInt32 inRowCount)
{
	switch ( inChangeCode )
	{
		// Do this before the change occurs, because the code for removing
		// the rows queries the backend for the message key.  By the time the change
		// has finished, this call won't work.
		case MSG_NotifyInsertOrDelete:
			if ( inRowCount < 0 )
				RemoveRows(-inRowCount, inStartRow, true);
			break;
		default:
			Inherited::ChangeStarting(inPane, inChangeCode, inStartRow, inRowCount);
			break;
	}
}

/*======================================================================================
	Notification from the BE that something is finished changing in the display table.
======================================================================================*/

void CExTable::ChangeFinished(MSG_Pane */*inPane*/, MSG_NOTIFY_CODE inChangeCode,
							  TableIndexT inStartRow, SInt32 inRowCount)
{
	if ( (--mMysticPlane) > (kMysticUpdateThreshHold + 1) ) return;
	
	MsgPaneChanged();

	switch ( inChangeCode )
	{
		case MSG_NotifyInsertOrDelete:
			Assert_(inRowCount != 0);
			if ( inRowCount > 0 )
			{
				InsertRows(inRowCount, inStartRow - 1, nil, 0, true);
			} else
			{
				//RemoveRows(-inRowCount, inStartRow, true); See ChangeStarting();
			}
			break;

		case MSG_NotifyChanged:
			{
				STableCell topLeft, botRight;
				TableIndexT rows;

				topLeft.col = 1;
				topLeft.row = inStartRow;
				
				GetTableSize(rows, botRight.col);
				botRight.row = inStartRow + inRowCount - 1;
				RefreshCellRange(topLeft, botRight);
			}
			break;

		case MSG_NotifyScramble:
		case MSG_NotifyAll:
			if ( mMysticPlane < kMysticUpdateThreshHold )
			{
				SInt32 diff = GetBENumRows() - mRows;
				if ( diff > 0 )
				{
					InsertRows(diff, 1, nil, 0, false);
				} else if ( diff < 0 )
				{
					RemoveRows(-diff, 1, false);
				}
				Refresh();
			}
			break;

		default:
			break;
	}
}
#endif
