/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// author: atotic
// See UTearOffPalette.h for usage instructions and design notes
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "UTearOffPalette.h"
#include "prtypes.h"
#include "CToolbarModeManager.h"
#include "CToolbarPatternBevelView.h"
#include "prefapi.h"
#include "uapp.h"

// PowerPlant
#include <LStream.h>
#include <URegions.h>
#include <LArray.h>
#include <LArrayIterator.h>

#define FLOAT_ID -19844	// Same define is in UFloatWindow.r
#define SIDE_FLOAT_ID -19845	// Same define is in UFloatWindow.r

#pragma mark CTearOffBar
#pragma mark -

// ---------------------------------------------------------------------------
//		¥ Constructor from stream
// ---------------------------------------------------------------------------
// Read in the data

CTearOffBar::CTearOffBar(LStream* inStream) : LView(inStream)
{
	*inStream >> fFloatingSubpaneID;
	*inStream >> fInPlaceSubpaneID;
	*inStream >> fAutoResize;
	*inStream >> fVisibility;
	*inStream >> fTitle;
	*inStream >> fSideTitle;
	*inStream >> fDisablePaletteWithWindow;

	fOriginalFrameSize = mFrameSize;
	fShown = false;
	fTransparentPane = 0;
	fAllowClickDrag = false;
}

// ---------------------------------------------------------------------------
//		¥ Destructor
// ---------------------------------------------------------------------------
// Unregister with the manager

CTearOffBar::~CTearOffBar()
{
	CTearOffManager::GetDefaultManager()->UnregisterBar(this);
}

// ---------------------------------------------------------------------------
//		¥ FinishCreateSelf
// ---------------------------------------------------------------------------
// Register with the manager

void CTearOffBar::FinishCreateSelf()
{
	CTearOffManager::GetDefaultManager()->RegisterBar(this);
}

// ---------------------------------------------------------------------------
//		¥ Click
// ---------------------------------------------------------------------------
// Slightly modified LView::Click
// We should handle clicks in the transparent pane
// We need this because the PPob view that holds the buttons that we put inside
// completely covers this view. Unless we make that view transparent, we'll never
// get any clicks

void CTearOffBar::Click( SMouseDownEvent	&inMouseDown )
{
									// Check if a SubPane of this View
									//   is hit
// Modified code
// Check if we have clicked on a 0 pane
	LPane	*clickedPane = FindDeepSubPaneContaining(inMouseDown.wherePort.h,
											inMouseDown.wherePort.v );
	if (clickedPane != nil && clickedPane->GetPaneID() == fTransparentPane)
	{
		LPane::Click(inMouseDown);
		return;
	}

// LView::Click code
	clickedPane = FindSubPaneHitBy(inMouseDown.wherePort.h,
											inMouseDown.wherePort.v);
											
	if	( clickedPane != nil )
	{		//  SubPane is hit, let it respond to the Click
		clickedPane->Click(inMouseDown);
		
	} else {						// No SubPane hit, or maybe pane 0. Inherited function
		LPane::Click(inMouseDown);	//   will process click on this View
	}
}

// ---------------------------------------------------------------------------
//		¥ ClickSelf
// ---------------------------------------------------------------------------
// Drag the outline of the bar out of the window
// Roughly copied out of UDesktop::DragDeskWindow

void CTearOffBar::ClickSelf(const SMouseDownEvent	&inMouseDown)
{
	if (::WaitMouseUp()) {				// Continue if mouse hasn't been
										//   released
										
		GrafPtr		savePort;			// Use ScreenPort for dragging
		GetPort(&savePort);
		GrafPtr		screenPort = UScreenPort::GetScreenPort();
		::SetPort(screenPort);
		StColorPenState::Normalize();

// Get surrounding rectangle in global port coordinates

		Rect frame;
		Point topLeft, botRight;
		topLeft.h = mFrameLocation.h;
		topLeft.v = mFrameLocation.v;
		botRight.h = topLeft.h + mFrameSize.width;
		botRight.v = topLeft.v + mFrameSize.height;
		PortToGlobalPoint(topLeft);
		PortToGlobalPoint(botRight);
		
		::SetRect( &frame, topLeft.h, topLeft.v, botRight.h, botRight.v );
		StRegion dragRgn(frame);
		
										// Let user drag a dotted outline
										//   of the Window
		Rect	dragRect = (**(GetGrayRgn())).rgnBBox;
		::InsetRect(&dragRect, 4, 4);
		
		Int32	dragDistance = ::DragGrayRgn(dragRgn, inMouseDown.macEvent.where,
											&dragRect, &dragRect,
											noConstraint, nil);
		::SetPort(savePort);
		
			// Check if the Window moved to a new, valid location.
			// DragGrayRgn returns the constant Drag_Aborted if the
			// user dragged outside the DragRect, which cancels
			// the drag operation. We also check for the case where
			// the drag finished at its original location (zero distance).
			
		Int16	horizDrag = LoWord(dragDistance);
		Int16	vertDrag = HiWord(dragDistance);
		
		if (dragDistance == 0x80008000) return;
			// 0x80008000 == Drag_Aborted from UFloatingDesktop
		Boolean realDrag = horizDrag != 0 || vertDrag != 0;
		if (fAllowClickDrag || realDrag)
		{
			 
				// A valid drag occurred. horizDrag and vertDrag are
				// distances from the current location. We need to
				// get the new location for the Window in global
				// coordinates. The bounding box of the Window's
				// content region gives the current location in
				// global coordinates. Add drag distances to this
				// to get the new location. 
			CTearOffManager::GetDefaultManager()->DragBar(this, 
													topLeft.h + horizDrag,
													topLeft.v + vertDrag,
													realDrag);
		}
	}
}

// ---------------------------------------------------------------------------
//		¥ AddListener
// ---------------------------------------------------------------------------
// Tell manager that we have a listener

void CTearOffBar::AddListener(LListener	*inListener)
{
	LBroadcaster::AddListener(inListener);
	CTearOffManager::GetDefaultManager()->RegisterListener(this, inListener);
}

// ---------------------------------------------------------------------------
//		¥ ListenToMessage 
// ---------------------------------------------------------------------------
// Tell TearOffManager that we got the message
void CTearOffBar::ListenToMessage(MessageT inMessage, void *ioParam)
{
	CTearOffManager::GetDefaultManager()->BarReceivedMessage(this, inMessage, ioParam);
}

// ---------------------------------------------------------------------------
//		¥ ListenToMessage 
// ---------------------------------------------------------------------------
// Tell TearOffManager that we got the message
void CTearOffBar::ActivateSelf()
{
	LView::ActivateSelf();
	CTearOffManager::GetDefaultManager()->BarActivated(this);
}

// ---------------------------------------------------------------------------
//		¥ ListenToMessage 
// ---------------------------------------------------------------------------
// Tell TearOffManager that we got the message
void CTearOffBar::DeactivateSelf()
{
	LView::DeactivateSelf();
	CTearOffManager::GetDefaultManager()->BarDeactivated(this);
}

// ---------------------------------------------------------------------------
//		¥ HidePalette 
// ---------------------------------------------------------------------------
// Delete all the subpanes.
// Manager will broadcast a message msg_FloatPalleteRemoved to your listener
void CTearOffBar::HidePalette()
{
	DeleteAllSubPanes();
	if (fAutoResize)
		ResizeFrameTo(0,0,TRUE);
	else
		Refresh();
	fShown = false;
}

// ---------------------------------------------------------------------------
//		¥ ShowPalette 
// ---------------------------------------------------------------------------
// Reads in the subview from PPob.
// Position the view properly

void CTearOffBar::ShowPalette()
{
	if (fShown)
		return;
	// setup commander (window), superview(this), and listener(this)
	
	SetDefaultView(this);
	LWindow * commander = LWindow::FetchWindowObject(GetMacPort());
	SignalIfNot_( commander );

	LView * view = UReanimator::CreateView(fInPlaceSubpaneID, this, commander);
	
	SignalIfNot_ ( view );	// We need that pane ID

	view->FinishCreate();
	fTransparentPane = view->GetPaneID();
	UReanimator::LinkListenerToControls(this, this, fInPlaceSubpaneID);
	UReanimator::LinkListenerToControls(CTearOffManager::GetDefaultManager(),
										this, fInPlaceSubpaneID);

	if (fAutoResize)
		ResizeFrameTo(fOriginalFrameSize.width, fOriginalFrameSize.height, TRUE);
#ifdef DEBUG
	if (fAutoResize)
	{
		SDimension16 s;
		view->GetFrameSize(s);
		// The container and the loaded view should be the same width/height???
		SignalIf_(s.width != fOriginalFrameSize.width);
		SignalIf_(s.height != fOriginalFrameSize.height);
	}	
#endif
	fShown = true;
}

#pragma mark -
#pragma mark CTearOffWindow
#pragma mark -
/************************************************************************
 * class CTearOffWindow
 *
 * just registers/unregisters itself with the 
 * You cannot subclass this class, because CTearOffManager creates all
 * palettes out of a single PPob.
 ************************************************************************/
#define SAVE_POSITION 1

#if SAVE_POSITION
#include "CSaveWindowStatus.h"
#endif // SAVE_POSITION

//======================================
class CTearOffWindow
		: public LWindow,
			public CToolbarModeChangeListener
#if SAVE_POSITION
		, public CSaveWindowStatus
#endif // SAVE_POSITION
//======================================
{
private:
	typedef LWindow Inherited;
	friend class CTearOffManager;
public:
	enum { class_ID = 'DRFW' };
		
// Constructors

							CTearOffWindow(LStream* inStream);

	virtual				~CTearOffWindow();

// Events
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam);

	virtual void HandleModeChange(Int8 inNewMode);
	virtual void DoSetZoom( Boolean );
	virtual void AdjustWindowSizeBy( SDimension16& size_change );

#if SAVE_POSITION
private:
	virtual ResIDT		GetStatusResID(void) const { return FLOAT_ID; }
	virtual UInt16		GetValidStatusVersion(void) const { return 0x0111; }
	virtual void		FinishCreateSelf();
	virtual void		AttemptClose();
	virtual void		DoClose();
#endif // SAVE_POSITION
private:
	UInt8				fVisibility;	// Inherited from the bar
};

//-----------------------------------
CTearOffWindow::CTearOffWindow(LStream* inStream)
//-----------------------------------
:	LWindow(inStream)
,	CSaveWindowStatus(this)
{
	PREF_SetBoolPref("taskbar.mac.is_open", true);
}

//-----------------------------------
CTearOffWindow::~CTearOffWindow()
//-----------------------------------
{
	// Notify the manager
	CTearOffManager::GetDefaultManager()->UnregisterWindow(this);

		// YIKES! Save this preference if the app _isn't_ quitting.  What an ugly hack!
	if ( CFrontApp::sApplication->GetState() == programState_ProcessingEvents )
		PREF_SetBoolPref("taskbar.mac.is_open", false);
}

#if SAVE_POSITION

//-----------------------------------
void CTearOffWindow::FinishCreateSelf()
//-----------------------------------
{
	Inherited::FinishCreateSelf();
	CSaveWindowStatus::FinishCreateWindow();
	
	SDimension16 zero_zero = {0,0};
	AdjustWindowSizeBy(zero_zero);
}

//-----------------------------------
void CTearOffWindow::AttemptClose()
//-----------------------------------
{
	AttemptCloseWindow();
	Inherited::AttemptClose();
}

//-----------------------------------
void CTearOffWindow::DoClose()
//-----------------------------------
{
	AttemptCloseWindow();
	Inherited::DoClose();
}

#endif // SAVE_POSITION


void CTearOffWindow::HandleModeChange( Int8 inNewMode )
	{
		SDimension16	size_change={ 0, 0 };
		if ( CToolbarButtonContainer* container_ptr = dynamic_cast<CToolbarButtonContainer*>(FindPaneByID('TBPv')) )
			container_ptr->HandleModeChange(inNewMode, size_change);
		AdjustWindowSizeBy(size_change);
	}


void CTearOffWindow::DoSetZoom( Boolean )
	{
		SDimension16	size_change={ 0, 0 };
		if ( CToolbarPatternBevelView* container_ptr = dynamic_cast<CToolbarPatternBevelView*>(FindPaneByID('TBPv')) )
			container_ptr->RotateArrangement(size_change);
		AdjustWindowSizeBy(size_change);

		SDimension16 frameSize;
		GetFrameSize(frameSize);
		PREF_SetBoolPref("taskbar.mac.is_vertical", XP_Bool(frameSize.height > frameSize.width));
	}

void CTearOffWindow::AdjustWindowSizeBy( SDimension16& size_change )
	{
			/*
				Always adjust the size (even if size_change is {0,0}) because the caller
				might be trying to ensure that the window is pinned to the screen.
			*/
		Rect frame_rect;
		CalcPortFrameRect(frame_rect);
		PortToGlobalPoint(topLeft(frame_rect));
		PortToGlobalPoint(botRight(frame_rect));

		frame_rect.right += size_change.width;
		frame_rect.bottom += size_change.height;

		VerifyWindowBounds(mWindowSelf, &frame_rect);

		DoSetBounds(frame_rect);
	}

// ---------------------------------------------------------------------------
//		¥ CTearOffWindow destructor
// ---------------------------------------------------------------------------
// Pass on to the manager
void CTearOffWindow::ListenToMessage(MessageT inMessage, void *ioParam)
{
	if ( inMessage == CToolbarModeManager::msg_ToolbarModeChange )
		HandleModeChange((Int8)ioParam);
	else
		CTearOffManager::GetDefaultManager()->WindowReceivedMessage(this, inMessage, ioParam);
}

/*****************************************************************************
 * class CTearOffManager
 *****************************************************************************/
#pragma mark -
#pragma mark CTearOffManager
#pragma mark -
CTearOffManager *  CTearOffManager::sDefaultFloatManager = NULL;

#pragma mark struct ButtonBarRecord
// holds information about a single button bar, stored in fPaletteBars list
struct ButtonBarRecord
{
	ResIDT				fID;	// ResID of the PPob loaded into the button bar.
	ResIDT				fWindowID;	// ResID of the PPob loaded into the floating window.
	CTearOffBar *	fBar;	// Pointer to the bar
	LListener * 		fListener;	// The listener for this bar
		
	ButtonBarRecord()
	{
		fID = -1;
		fWindowID = -1;
		fBar = NULL;
		fListener = NULL;
	}
	
	ButtonBarRecord(ResIDT id, ResIDT wID, CTearOffBar * bar, LListener * listener)
	{
		fID = id;
		fWindowID = wID;
		fBar = bar;
		fListener = listener;
	}
};

/****************************************************************************
 * struct FloatWindowRecord
 * used to keep a list of loaded windows
 ****************************************************************************/
#pragma mark struct FloatWindowRecord
struct FloatWindowRecord
{
	CTearOffWindow * fWindow;
	ResIDT				fID;		// Res ID of the PPob to be loaeded
	CTearOffBar * fActiveBar;

	FloatWindowRecord(CTearOffWindow * window, ResIDT resID, CTearOffBar * activeBar = NULL)
	{
		fWindow = window;
		fID = resID;
		fActiveBar = activeBar;
	}

};

// ---------------------------------------------------------------------------
//		¥ Compare the window records
// ---------------------------------------------------------------------------
// Wildcards can be used for fID and fWindow fields
// fListener field is not used.

#define WILDCARD -1

Int32 CTearOffManager::CPaletteWindowComparator::Compare(const void*		inItemOne,
									const void*		inItemTwo,
									Uint32			inSizeOne,
									Uint32			inSizeTwo) const
{
#pragma unused (inSizeOne, inSizeTwo)	// Cause we know what the items are

	FloatWindowRecord * item1 = (FloatWindowRecord *)inItemOne;
	FloatWindowRecord * item2 = (FloatWindowRecord *)inItemTwo;

	if ( item1->fID != item2->fID &&	// Different floaters
		 (item1->fID != WILDCARD) &&
		 (item2->fID != WILDCARD))	
		return (item1->fWindow - item2->fWindow);
	else							// Same floaters
		if ( item1->fWindow == item2->fWindow ||
			( item1->fWindow == (void*)WILDCARD ) ||
			( item2->fWindow == (void*)WILDCARD ) )
			return 0;
		else
			return item1->fWindow - item2->fWindow;
}

// ---------------------------------------------------------------------------
//		¥ Compare the bars
// ---------------------------------------------------------------------------
// Wildcards can be used for fID and fBar fields
// fListener field is not used.

Int32 CTearOffManager::CPaletteBarComparator::Compare(const void*		inItemOne,
									const void*		inItemTwo,
									Uint32			inSizeOne,
									Uint32			inSizeTwo) const
{
#pragma unused (inSizeOne, inSizeTwo)	// Cause we know what the items are

	ButtonBarRecord * item1 = (ButtonBarRecord *)inItemOne;
	ButtonBarRecord * item2 = (ButtonBarRecord *)inItemTwo;


	if ( item1->fID != item2->fID &&	// Different IDs
		 (item1->fID != WILDCARD) &&
		 (item2->fID != WILDCARD) )	
		return ( item1->fBar - item2->fBar );
	if (item1->fWindowID != item2->fWindowID &&	// Different floaters
		 (item1->fWindowID != WILDCARD) &&
		 (item2->fWindowID != WILDCARD) )
		return ( item1->fBar - item2->fBar );
	else							// Same floaters
		if ( item1->fBar == item2->fBar ||
			( item1->fBar == (void*)WILDCARD ) ||
			( item2->fBar == (void*)WILDCARD ) )
			return 0;
		else
			return item1->fBar - item2->fBar;
	return 0;	// Kill the warning
}

// ---------------------------------------------------------------------------
//		¥ CTearOffManager Constructor 
// ---------------------------------------------------------------------------
// sets the default manager

CTearOffManager::CTearOffManager(LCommander * defaultCommander)
{
	sDefaultFloatManager = this;
	
	// These lists cannot be sorted, because sometimes we do wildcard lookups
	// If you want to sort them, implement CompareKey better
	fPaletteBars = new LArray( sizeof (ButtonBarRecord), &fBarCompare, false );
	fPaletteWindows = new LArray( sizeof (FloatWindowRecord), &fWindowCompare, false );

	fDefaultCommander = defaultCommander;
}

// ---------------------------------------------------------------------------
//		¥ CTearOffManager destructor -- never called, here for completeness
// ---------------------------------------------------------------------------
// clean up
CTearOffManager::~CTearOffManager()
{
	delete fPaletteBars;
	delete fPaletteWindows;
}


void CTearOffManager::ListenToMessage(MessageT inMessage, void *ioParam)
{
	BroadcastMessage(inMessage, ioParam);
}


// ---------------------------------------------------------------------------
//		¥ RegisterBar 
// ---------------------------------------------------------------------------
// Insert the bar in the bar list. Hide or show it, depending on the window

void CTearOffManager::RegisterBar(CTearOffBar * inBar)
{
	// Insert it into the list
	ButtonBarRecord barRec( inBar->fInPlaceSubpaneID, inBar->fFloatingSubpaneID, inBar, NULL);
	fPaletteBars->InsertItemsAt(1, fPaletteBars->GetCount(), &barRec, sizeof( barRec ) );
	
	// Decide whether to hide or show the bar

	FloatWindowRecord fr( (CTearOffWindow *)WILDCARD, inBar->fFloatingSubpaneID );
	ArrayIndexT where = fPaletteWindows->FetchIndexOfKey( &fr );
	if ( where != LArray::index_Bad )
		inBar->HidePalette();
	else
		inBar->ShowPalette();
}

// ---------------------------------------------------------------------------
//		¥ UnregisterBar 
// ---------------------------------------------------------------------------
// Remove the bar from the bar list
// If there are any "left-over" floaters, remove them

void CTearOffManager::UnregisterBar(CTearOffBar * inBar)
{
	ButtonBarRecord bar( WILDCARD, WILDCARD, inBar, NULL);
	ArrayIndexT where = fPaletteBars->FetchIndexOfKey(&bar);
	if ( where != LArray::index_Bad )
		fPaletteBars->RemoveItemsAt( 1, where );
	else
		SignalCStr_("Unregistering a bar that was never registered?");

// If there are no other bars with the same ID,
// remove the floater window (floater might not exist)

	ButtonBarRecord brotherBar( inBar->fInPlaceSubpaneID, inBar->fFloatingSubpaneID,
								(CTearOffBar *)WILDCARD, NULL );
	
	where = fPaletteBars->FetchIndexOfKey(&brotherBar);
	if ( where == LArray::index_Bad )
	// Remove the floaters with this ID
	{
		FloatWindowRecord fw( (CTearOffWindow*) WILDCARD, inBar->fFloatingSubpaneID );
		ArrayIndexT where = fPaletteWindows->FetchIndexOfKey( &fw );
		if ( where != LArray::index_Bad )
		{
			fPaletteWindows->FetchItemAt( where, &fw );
			if (fw.fWindow->fVisibility != 0)
				delete fw.fWindow;	// this will automatically remove it from the list
		}
	}
}
	
// ---------------------------------------------------------------------------
//		¥ RegisterListener 
// ---------------------------------------------------------------------------
// Set the listener
// Broadcast the palette message to the window, depending on where the palette is

void CTearOffManager::RegisterListener(CTearOffBar * inBar, 
											LListener * listener)
{
	ButtonBarRecord bar( WILDCARD, WILDCARD, inBar, listener );
	ArrayIndexT where = fPaletteBars->FetchIndexOfKey(&bar);
	if (where != LArray::index_Bad)
	{
	// assign the listener
		fPaletteBars->FetchItemAt( where, &bar );
		bar.fListener = listener;
		fPaletteBars->AssignItemsAt( 1, where , &bar );
	
	// broadcast the palette message to the listener
		FloatWindowRecord fr( (CTearOffWindow *)WILDCARD, inBar->fFloatingSubpaneID );
		ArrayIndexT where = fPaletteWindows->FetchIndexOfKey( &fr );
		
		if ( where != LArray::index_Bad )
		{
		// We have a window to register
			fPaletteWindows->FetchItemAt( where, &fr );
			SetBarsView( inBar, fr.fWindow );
		}
		else
		// The bar holds the palette
		{
			SetBarsView( inBar, inBar );
		}
	}
	else
		SignalCStr_("Bar never registered?");
}
	
// ---------------------------------------------------------------------------
//		¥ BarReceivedMessage 
// ---------------------------------------------------------------------------
// Relay the message to its listeners

void CTearOffManager::BarReceivedMessage(CTearOffBar * inBar,
											MessageT inMessage,
											 void *ioParam)
{
	ButtonBarRecord bar( WILDCARD, WILDCARD, inBar, NULL );
	ArrayIndexT where = fPaletteBars->FetchIndexOfKey(&bar);
	
	if ( where != LArray::index_Bad )
	{
		fPaletteBars->FetchItemAt( where, &bar );
//		SignalIfNot_(bar.fListener);
		if (bar.fListener)
			bar.fListener->ListenToMessage( inMessage, ioParam );
	}
	else
		SignalCStr_("Bar never registered?");
}

// ---------------------------------------------------------------------------
//		¥ DragBar 
// ---------------------------------------------------------------------------
// Assert that the window exists
// move it to the new location

void CTearOffManager::DragBar( CTearOffBar * inBar,
								Int16 inH,
								Int16 inV,
								Boolean usePosition )
{
	try
	{
		CTearOffWindow * win = AssertBarWindowExists(inBar);

#if SAVE_POSITION
	if (usePosition) {
#endif
		Point topLeft;
		topLeft.h = inH;
		topLeft.v = inV;
		win->DoSetPosition(topLeft);
#if SAVE_POSITION
		}
#endif
		win->Show();
		
		SetBarsView( inBar, win );
	}
	catch(OSErr inErr)
	{
		SignalCStr_("Did you include UTearOffPalette.r in your project?");
		// No window, button bar stays where it was
	}
}

// ---------------------------------------------------------------------------
//		¥ BarActivated 
// ---------------------------------------------------------------------------
// Upon activation:
// Tell its float windowGets the floater window for the bar
// Tells listener what the current palette is
// 
void CTearOffManager::BarActivated( CTearOffBar * inBar)
{
// Find the floater window
	FloatWindowRecord fw( (CTearOffWindow *) WILDCARD, inBar->fFloatingSubpaneID );
	ArrayIndexT where = fPaletteWindows->FetchIndexOfKey( &fw );

// If we have a window, make this bar the active one
	if ( where != LArray::index_Bad )
	{
		fPaletteWindows->FetchItemAt( where, &fw );
		if ( fw.fActiveBar != inBar )
		{
			if ( fw.fActiveBar == NULL )	// If there was no view, we need to enable us
			{
				LPane * view;
				LArrayIterator iter( fw.fWindow->GetSubPanes());
				if ( iter.Next(&view) )	// We have disabled this on suspends
					view->Enable();
			}
			
			fw.fActiveBar = inBar;
			fPaletteWindows->AssignItemsAt( 1, where , &fw );
			fw.fWindow->Show();
			

			SetBarsView(inBar, fw.fWindow);
			
		}
	}
}

// ---------------------------------------------------------------------------
//		¥ BarDeactivated 
// ---------------------------------------------------------------------------
// Upon deactivation
// If we have a floating window, make it inactive and tell listener about it

void CTearOffManager::BarDeactivated( CTearOffBar * inBar)
{
	FloatWindowRecord fw( (CTearOffWindow *) WILDCARD, inBar->fFloatingSubpaneID );
	ArrayIndexT where = fPaletteWindows->FetchIndexOfKey( &fw );

	if ( where != LArray::index_Bad )
	{
		fPaletteWindows->FetchItemAt( where, &fw );
		if ( fw.fActiveBar == inBar )
		{
			if ( fw.fWindow->fVisibility == 2 )
				fw.fWindow->Hide();
			else
			{
				if (inBar->DisablePaletteWindow())
				{
					// Disable all the subpanes of our main subpane
					// Do not disable the main pane, because no one enables it
					LPane * view;
					LArrayIterator iter( fw.fWindow->GetSubPanes());
					if ( iter.Next(&view) )
						view->Disable();
				}
			}
			fw.fActiveBar = NULL;
			fPaletteWindows->AssignItemsAt( 1, where , &fw );
			RemoveBarsView(inBar, inBar->fTransparentPane);
		}
	}
}

// ---------------------------------------------------------------------------
//		¥ SetBarsView 
// ---------------------------------------------------------------------------
// Tell listener what its palette view is
// InView is either FBar, or FWindow. In either case, the palette is the first subview

void CTearOffManager::SetBarsView( CTearOffBar * inBar,
										LView * inView)
{
	SignalIfNot_( inView );

	// Get the bar record
	ButtonBarRecord br( WILDCARD, WILDCARD, inBar, (LListener*) WILDCARD );
	ArrayIndexT where = fPaletteBars->FetchIndexOfKey( &br );

	if ( where != LArray::index_Bad )
	{
		fPaletteBars->FetchItemAt( where, &br );
		if ( br.fListener )
		{
			// Palette is the first subview
			LView * topPaletteView;
			LArrayIterator iter( inView->GetSubPanes());
			if ( iter.Next(&topPaletteView) )
				br.fListener->ListenToMessage( msg_SetTearOffPalette, topPaletteView );
		}
	}
	else
		SignalCStr_("Where is the bar?");			
}

// ---------------------------------------------------------------------------
//		¥ RemoveBarsView 
// ---------------------------------------------------------------------------
// Similar to SetBarsView. Must get the top pane ID from somewhere 

void CTearOffManager::RemoveBarsView( CTearOffBar * inBar, PaneIDT inViewID)
{
	ButtonBarRecord br( WILDCARD, WILDCARD, inBar, (LListener*) WILDCARD );
	ArrayIndexT where = fPaletteBars->FetchIndexOfKey( &br );

	if ( where != LArray::index_Bad )
	{
		fPaletteBars->FetchItemAt( where, &br );
		if ( br.fListener )
			br.fListener->ListenToMessage( msg_RemoveTearOffPalette, (void*) inViewID );
	}
	else
		SignalCStr_("Where is the bar?");		
}

// ---------------------------------------------------------------------------
//		¥ AssertBarWindowExists 
// ---------------------------------------------------------------------------
// Create the window if it does not exist

CTearOffWindow * CTearOffManager::AssertBarWindowExists(CTearOffBar * inBar)
{
	return AssertBarWindowExists(
		inBar->fFloatingSubpaneID,
		inBar->IsFloaterOnSide(),
		inBar->fVisibility,
		inBar->fTitle
		);
}

// ---------------------------------------------------------------------------
//		¥ AssertBarWindowExists 
// ---------------------------------------------------------------------------
// Create the window if it does not exist

CTearOffWindow * CTearOffManager::AssertBarWindowExists(ResIDT inWinID,
														Boolean inSideways,
														UInt8 visibility,
														Str255 &title)
{
	FloatWindowRecord fw( (CTearOffWindow *) WILDCARD, inWinID );
	ArrayIndexT where = fPaletteWindows->FetchIndexOfKey( &fw );
	CTearOffWindow * newWindow = nil;
	if (where == LArray::index_Bad)
	{
	// Create a new window, load the palette into it
		ResIDT windowResID = inSideways ? SIDE_FLOAT_ID : FLOAT_ID;

		newWindow = (CTearOffWindow *) LWindow::CreateWindow(windowResID, fDefaultCommander);

		ThrowIfNil_(newWindow);
		newWindow->fVisibility = visibility;

		LView * view = UReanimator::CreateView(inWinID, newWindow, newWindow);
		ThrowIfNil_( view );
		view->FinishCreate();

		UReanimator::LinkListenerToControls(CTearOffManager::GetDefaultManager(),
											view, inWinID);
	// Resize the window
		Point corner = {0,0};
#if SAVE_POSITION
		newWindow->PortToGlobalPoint(corner);
#endif		
		SDimension16 viewSize;
		Rect winBounds;
		view->GetFrameSize(viewSize);
		::SetRect(&winBounds,
				corner.h, corner.v,
				corner.h + viewSize.width, corner.v + viewSize.height);
		newWindow->DoSetBounds( winBounds );
		newWindow->SetDescriptor(title);

		SBooleanRect boundToSuperview={ true, true, true, true };
		view->SetFrameBinding(boundToSuperview);

		Int32 toolbarStyle;
		if ( (PREF_GetIntPref("browser.chrome.toolbar_style", &toolbarStyle) == PREF_NOERROR)
			&& (toolbarStyle != CToolbarModeManager::defaultToolbarMode) )
			{
				((CTearOffWindow*)newWindow)->HandleModeChange(toolbarStyle);
			}

		XP_Bool is_vertical = true;
		if ( (PREF_GetBoolPref("taskbar.mac.is_vertical", &is_vertical) == PREF_NOERROR)
			&& !is_vertical )
			{
				((CTearOffWindow*)newWindow)->DoSetZoom(Boolean());
			}

	// Add the record to the list of managed floaters
		FloatWindowRecord fw(newWindow, inWinID, NULL);
		fPaletteWindows->InsertItemsAt( 1, fPaletteWindows->GetCount(), &fw, sizeof (fw) );

	// Hide the bars button palletes
		LArrayIterator iter(*fPaletteBars);
		ButtonBarRecord bar;
		while ( iter.Next(&bar) )
			if (bar.fWindowID == fw.fID)
			{
				bar.fBar->HidePalette();
				bar.fBar->fTransparentPane = view->GetPaneID();
				RemoveBarsView( bar.fBar, bar.fBar->fTransparentPane );
			}
	// Link up the new controls
		UReanimator::LinkListenerToControls( newWindow, newWindow, fw.fID );
	}
	else
	{
		fPaletteWindows->FetchItemAt( where, &fw );
		return fw.fWindow;
	}
	return newWindow;
} // CTearOffManager::AssertBarWindowExists

// ---------------------------------------------------------------------------
//		¥ UnregisterWindow 
// ---------------------------------------------------------------------------
// Remove the window from the list
// Show all the floaters

void CTearOffManager::UnregisterWindow(CTearOffWindow * window)
{
	// Remove the window from the list
	FloatWindowRecord fw( window, WILDCARD );
	ArrayIndexT where = fPaletteWindows->FetchIndexOfKey( &fw );

	if ( where != LArray::index_Bad )
	{
		fPaletteWindows->FetchItemAt( where, &fw );
		fPaletteWindows->RemoveItemsAt( 1, where );
	}
	else
		SignalCStr_("Removal of unregistered window?");
	
	// Tell all button bars to show themselves
	
	LArrayIterator iter(*fPaletteBars);
	ButtonBarRecord bar;
	while ( iter.Next(&bar) )
		if (bar.fWindowID == fw.fID)
			{
				bar.fBar->ShowPalette();
				SetBarsView(bar.fBar, bar.fBar);
			}
}

// ---------------------------------------------------------------------------
//		¥ CloseFloatingWindow 
// ---------------------------------------------------------------------------
// Closes the Floating Window

void CTearOffManager::CloseFloatingWindow(ResIDT inWinID)
{
	// find the window in the list
	FloatWindowRecord fw((CTearOffWindow *)WILDCARD, inWinID, (CTearOffBar *)WILDCARD );
	ArrayIndexT where = fPaletteWindows->FetchIndexOfKey( &fw );

	if ( where != LArray::index_Bad )
	{
		fPaletteWindows->FetchItemAt( where, &fw );
		fw.fWindow->ProcessCommand(cmd_Close);
	}
	else
	{
		SignalCStr_("close of unregistered window?");
	}
}

// ---------------------------------------------------------------------------
//		¥ OpenFloatingWindow 
// ---------------------------------------------------------------------------
// Opens the Floating Window

void CTearOffManager::OpenFloatingWindow(	ResIDT inWinID,
											Point /* inTopLeft */,
											Boolean inSideways,
											UInt8 visibility,
											Str255 &title)
{
	// find the window in the list
	FloatWindowRecord fw((CTearOffWindow *)WILDCARD, inWinID, (CTearOffBar *)WILDCARD );
	ArrayIndexT where = fPaletteWindows->FetchIndexOfKey( &fw );

	if (LArray::index_Bad == where)
	{
		try
		{
			CTearOffWindow * win = AssertBarWindowExists(inWinID, inSideways, visibility, title);

#if !SAVE_POSITION
			win->DoSetPosition(inTopLeft);
#endif
			win->Show();
		}
		catch(OSErr inErr)
		{
			SignalCStr_("Did you include UTearOffPalette.r in your project?");
			// No window, button bar stays where it was
		}
	}
	else
	{
		SignalCStr_("open of open window?");
	}
}


// ---------------------------------------------------------------------------
//		¥ IsFloatingWindow 
// ---------------------------------------------------------------------------
// Closes the Floating Window

Boolean CTearOffManager::IsFloatingWindow(ResIDT inWinID)
{
	// find the window in the list
	FloatWindowRecord fw((CTearOffWindow *)WILDCARD, inWinID, (CTearOffBar *)WILDCARD );
	ArrayIndexT where = fPaletteWindows->FetchIndexOfKey( &fw );

	return where != LArray::index_Bad;
}

// ---------------------------------------------------------------------------
//		¥ WindowReceivedMessage 
// ---------------------------------------------------------------------------
// Rebroadcast the message to the listener of the currently active bar
void CTearOffManager::WindowReceivedMessage(CTearOffWindow * inWin,
											MessageT inMessage,
											 void *ioParam)
{
	// Get the window record
	FloatWindowRecord fw( inWin, WILDCARD );
	ArrayIndexT where = fPaletteWindows->FetchIndexOfKey( &fw );

	if ( where != LArray::index_Bad )
	{
		fPaletteWindows->FetchItemAt( where, &fw );
		
		// If there is an active bar attached to this window
		// rebroadcast the message to its listener

		if (fw.fActiveBar)
		{
			ButtonBarRecord bar( WILDCARD, WILDCARD, fw.fActiveBar , NULL );
			where = fPaletteBars->FetchIndexOfKey(&bar);
			if ( where != LArray::index_Bad )
			{
				fPaletteBars->FetchItemAt( where, &bar );
				if ( bar.fListener != NULL )
					bar.fListener->ListenToMessage( inMessage, ioParam );
			}
		}
		
	}
	else
		SignalCStr_("Message from a window we do not know about?");
}

#pragma mark -
// ---------------------------------------------------------------------------
//		¥ InitUTearOffPalette [static]
// ---------------------------------------------------------------------------
// Registers classes, creates a single float manager
void InitUTearOffPalette(LCommander * defCommander)
{
	RegisterClass_( CTearOffBar);
	RegisterClass_( CTearOffWindow);
	new CTearOffManager(defCommander);
}

