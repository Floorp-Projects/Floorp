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

#include "Netscape_Constants.h"
#include "CBrowserWindow.h"

#include "CAutoPtrXP.h"

#include "CBevelView.h"
#include "CApplicationEventAttachment.h"
#include "CHTMLView.h"
#include "CBrowserContext.h"
#include "CHistoryMenu.h"
#include "mimages.h"
#include "mversion.h"
#include "CURLEditField.h"
#include "CURLCaption.h"
#include "CProxyPane.h"

//#include "CEditView.h"				// need for SetWindowContext, using CEditView::class_ID
#include "CEditorWindow.h"
// need these for dynamic window modification - mjc
#include "CDragBar.h"				// need for GetChromeInfo
#include "CDragBarContainer.h"		// need for SetChromeInfo, using CDragBarContainer::class_ID
#include "CHyperScroller.h" // need for SetChromeInfo
#include "CPaneEnabler.h"

#include "CSpinningN.h"
#include "CBrowserSecurityButton.h"
#include "CMiniSecurityButton.h"

#include "CRDFCoordinator.h"

	// stuff added by deeje
#include "macutil.h"
#include "xp.h"
#include "uerrmgr.h"	// GetPString prototype
#include "shist.h"
#include "resgui.h"
#include "CURLDispatcher.h"
#include "prefapi.h"
#include "CPrefsDialog.h"
#include "xp_ncent.h"	// for XP_SetLastActiveContext


#include "libi18n.h"
#include "ufilemgr.h"

	// stuff for AppleEvent support
#include "CAppleEventHandler.h"
#include "resae.h"
#include "uapp.h"

#include "CMochaHacks.h" // for SendMoveEvent, SendResizeEvent, and RemoveDependents 1997-02-26 mjc

#include "RandomFrontEndCrap.h" // for IsSpecialBrowserWindow

#include <UReanimator.h>
#include <UMemoryMgr.h>
#include <Sound.h>

// pane id constants - for SetChromeInfo and GetChromeInfo - mjc
const PaneIDT HTML_Container_PaneID = 'HtCt';
const PaneIDT Button_Bar_PaneID = 'NBar';
const PaneIDT Location_Bar_PaneID = 'LBar';
const PaneIDT Directory_Bar_PaneID = 'DBar';
const PaneIDT PersonalToolbar_PaneID = 'PBar';
const PaneIDT SWatch_View_PaneID = 'SwBv';
const PaneIDT Hyper_Scroller_PaneID = 1005;
const PaneIDT Status_Bar_PaneID = 'StBv';
const PaneIDT CoBrandLogo_PaneID = 'Bnet';

const char* Pref_ShowToolbar = "browser.chrome.show_toolbar";
const char* Pref_ShowLocationBar = "browser.chrome.show_url_bar";
const char* Pref_ShowPersonalToolbar = "browser.chrome.show_personal_toolbar";	// is this right?


enum {  eNavigationBar,
		eLocationBar,
		eStatusBar,
		ePersonalToolbar };
				
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CBrowserWindow::CBrowserWindow(LStream* inStream)
	:	CNetscapeWindow(inStream, WindowType_Browser),
		CSaveWindowStatus(this)
{
	mProgressListener = NULL;
	mContext = NULL;
	mAlwaysOnBottom = false; // javascript alwaysLowered window property - mjc
	mZLocked = false;
	mMenubarMode = BrWn_Menubar_Default;
	mCommandsDisabled = false;
	fCloseNotifier = MakeNoProcessPSN();
	mSupportsPageServices = false;
	mAllowSubviewPopups = true;
	mIsRootDocInfo = false;
	mIsViewSource = false;
	mIsHTMLHelp = false;
	mHTMLView = nil;
	mNavCenterParent = nil;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CBrowserWindow::~CBrowserWindow()
{
	delete mProgressListener;
	if (mContext != NULL) CMochaHacks::RemoveDependents(*mContext); // close windows declared as dependents in Javascript - 1997-02-26 mjc
	SetWindowContext(NULL);
	
	RemoveAllAttachments();
		// Kludgy, but prevents crash in LUndoer caused by view being destroyed before
		// attachments.  This happens if a form element which is a text field exists.

	// there is no need to save the state of the selector widget or the nav center shelf
	// because the prefs are always correctly updated on a state change.
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserWindow::FinishCreateSelf(void)
{
	CNetscapeWindow::FinishCreateSelf();
	
	mHTMLView = dynamic_cast<CHTMLView*>(FindPaneByID(CHTMLView::pane_ID));
	ThrowIfNil_(mHTMLView);
	
	mNavCenterParent = dynamic_cast<CRDFCoordinator*>(FindPaneByID(CRDFCoordinator::pane_ID));
		
	if (HasAttribute(windAttr_Regular) && HasAttribute(windAttr_Resizable))
	{
		FinishCreateWindow();
		
		Rect structureBounds = UWindows::GetWindowStructureRect(mMacWindowP);
		
		GDHandle windowDevice = UWindows::FindDominantDevice(structureBounds);
		GDHandle mainDevice = ::GetMainDevice();
		
		Rect staggerBounds;
		
		if (windowDevice && (windowDevice != mainDevice))
			staggerBounds = (*windowDevice)->gdRect;
		else
		{
			staggerBounds = (*mainDevice)->gdRect;
			staggerBounds.top += LMGetMBarHeight();
		}
		::InsetRect(&staggerBounds, 4, 4);	//	Same as zoom limit
		
		Rect contentBounds = UWindows::GetWindowContentRect(mMacWindowP);
		
		Rect windowBorder;
		windowBorder.top = contentBounds.top - structureBounds.top;
		windowBorder.left = contentBounds.left - structureBounds.left;
		windowBorder.bottom = structureBounds.bottom - contentBounds.bottom;
		windowBorder.right = structureBounds.right - contentBounds.right;
		
		Point maxTopLeft;
		maxTopLeft.v = staggerBounds.bottom
			- (windowBorder.top + mMinMaxSize.top + windowBorder.bottom);
		maxTopLeft.h = staggerBounds.right
			- (windowBorder.left + mMinMaxSize.left + windowBorder.right);
		
		UInt16 vStaggerOffset = windowBorder.top;
		
		if (vStaggerOffset > 12)
			vStaggerOffset -= 4;	//	Tweak it up
		
		const int maxStaggerPositionsCount = 10;
		Point staggerPositions[maxStaggerPositionsCount];
		UInt16 usedStaggerPositions[maxStaggerPositionsCount];
		int staggerPositionsCount;
		
		for (	staggerPositionsCount = 0;
				staggerPositionsCount < maxStaggerPositionsCount;
				++staggerPositionsCount)
		{
			staggerPositions[staggerPositionsCount].v
				= staggerBounds.top + (vStaggerOffset * staggerPositionsCount);
			staggerPositions[staggerPositionsCount].h
				= staggerBounds.left + (4 * staggerPositionsCount);
			
			if ((staggerPositions[staggerPositionsCount].v > maxTopLeft.v)
				|| (staggerPositions[staggerPositionsCount].h > maxTopLeft.h))
				break;
			
			usedStaggerPositions[staggerPositionsCount] = 0;
		}
		unsigned int windowCount = 0;
		CMediatedWindow *foundWindow = NULL;
		CWindowIterator windowIterator(WindowType_Browser);
		
		for (windowIterator.Next(foundWindow); foundWindow; windowIterator.Next(foundWindow))
		{
			CBrowserWindow *browserWindow = dynamic_cast<CBrowserWindow*>(foundWindow);
			
			if (browserWindow && (browserWindow != this)
				&& browserWindow->HasAttribute(windAttr_Regular))
			{
				++windowCount;
				Rect bounds = UWindows::GetWindowStructureRect(browserWindow->mMacWindowP);
				
				for (int index = 0; index < staggerPositionsCount; ++index)
				{
					Boolean matchTop = (bounds.top == staggerPositions[index].v);
					Boolean matchLeft = (bounds.left == staggerPositions[index].h);
					
					if ((matchTop) && (matchLeft))
						usedStaggerPositions[index] += 1;
					
					if ((matchTop) || (matchLeft))
						break;
				}
			}
		}
		Point structureSize;
		structureSize.v = structureBounds.bottom - structureBounds.top;
		structureSize.h = structureBounds.right - structureBounds.left;
		
		if (windowCount)
		{
			Boolean foundStaggerPosition = false;
			
			for (UInt16 minCount = 0; (minCount < 100) && !foundStaggerPosition; ++minCount)
			{
				for (int index = 0; index < staggerPositionsCount; ++index)
				{
					if (usedStaggerPositions[index] == minCount)
					{
						structureBounds.top = staggerPositions[index].v;
						structureBounds.left = staggerPositions[index].h;
						foundStaggerPosition = true;
						break;
					}
				}
			}
			if (!foundStaggerPosition)
			{
				structureBounds.top = staggerBounds.top;
				structureBounds.left = staggerBounds.left;
			}
			structureBounds.bottom = structureBounds.top + structureSize.v;
			structureBounds.right = structureBounds.left + structureSize.h;
		}
		if ((structureBounds.top > maxTopLeft.v) || (structureBounds.left > maxTopLeft.h))
		{
			structureBounds.top = staggerBounds.top;
			structureBounds.left = staggerBounds.left;
			structureBounds.bottom = structureBounds.top + structureSize.v;
			structureBounds.right = structureBounds.left + structureSize.h;
		}
		if (structureBounds.bottom > staggerBounds.bottom)
			structureBounds.bottom = staggerBounds.bottom;
		
		if (structureBounds.right > staggerBounds.right)
			structureBounds.right = staggerBounds.right;
		
		contentBounds.top = structureBounds.top + windowBorder.top;
		contentBounds.left = structureBounds.left + windowBorder.left;
		contentBounds.bottom = structureBounds.bottom - windowBorder.bottom;
		contentBounds.right = structureBounds.right - windowBorder.right;
		
		DoSetBounds(contentBounds);
	}
	UReanimator::LinkListenerToControls(this, this, mPaneID);
	
	// Show/hide toolbars based on preference settings
	XP_Bool	value;
	PREF_GetBoolPref(Pref_ShowToolbar, &value);
	mToolbarShown[eNavigationBar] = value;
	ShowOneDragBar(Button_Bar_PaneID, value);
	PREF_GetBoolPref(Pref_ShowLocationBar, &value);
	mToolbarShown[eLocationBar] = value;
	ShowOneDragBar(Location_Bar_PaneID, value);
	PREF_GetBoolPref(Pref_ShowPersonalToolbar, &value);
	mToolbarShown[ePersonalToolbar] = value;
	ShowOneDragBar(PersonalToolbar_PaneID, value);
	mToolbarShown[eStatusBar] = TRUE; // no pref for status bar

	// Handle NavCenter pane. There are two separate prefs here: one to control showing/hiding the
	// selector widget, the other for the shelf itself. Recall that Composer doesn't have a nav center...
	if ( mNavCenterParent ) {
		if ( PREF_GetBoolPref(CRDFCoordinator::Pref_ShowNavCenterSelector, &value) == PREF_ERROR )
			value = true;
		mNavCenterParent->NavCenterSelector().SetShelfState ( value );

		// don't worry about showing the shelf if the selector is not visible. Otherwise do
		// what the pref says.
		if ( value ) {
			// if the pref is missing, close it by default. If the pref exists, use whatever is
			// stored there. There is no need to set the current view because HT handles that.
			if ( PREF_GetBoolPref(CRDFCoordinator::Pref_ShowNavCenterShelf, &value) == PREF_ERROR )
				value = false;
			mNavCenterParent->NavCenterShelf().SetShelfState ( value );
		} // if selector visible
		
	} // if there is a navcenter
			
	// Delete the Admin Kit co-brand button if a
	// custom animation has not been installed
	if ( !CPrefs::HasCoBrand() ) {
		LPane* button = FindPaneByID(CoBrandLogo_PaneID);
		if (button) {
			RemoveSubPane(button);
			delete button;
		}
	}

	/*
		// the HTML view handles many of the buttons in the tool bar
	LListener*		theHTMLView = (LListener*) FindPaneByID(CHTMLView::pane_ID);
	if (theHTMLView != nil)
	{
		UReanimator::LinkListenerToControls(theHTMLView, this, mPaneID);
	}
	*/
	
	// Make the tab group the latent sub commander
	
	CURLEditField* theURLEditField = dynamic_cast<CURLEditField*>(FindPaneByID(CURLEditField::class_ID));
	if (theURLEditField)
	{
		// Find the tab group which is a super commander of the edit field and make it
		// the latent sub commander which should rotate the target to the first target.
		
		LCommander* theCommander	= theURLEditField->GetSuperCommander();
		LTabGroup*	theTabGroup		= nil;
		while (theCommander && !(theTabGroup = dynamic_cast<LTabGroup*>(theCommander)))
		{
			theCommander = theCommander->GetSuperCommander();
		}
		
		if (theTabGroup)
		{
			SetLatentSub(theTabGroup);
		}
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// NOTE: This will handle registering/unregistering the embedded navCenter
//       for sitemap info. When the context is cleared (by passing NULL), 
//       it will unregister so we don't need to do it elsewhere.
void CBrowserWindow::SetWindowContext(CBrowserContext* inContext)
{
	if (mContext != NULL) {
		if ( mNavCenterParent )
			mNavCenterParent->UnregisterNavCenter();
		mContext->RemoveUser(this);
	}
	
	mContext = inContext;
	
	if (mContext != NULL)
	{
		mContext->SetRequiresClone(true);
		mContext->AddListener(this);
		try {
			// Let there be a progress listener, placed in my firmament,
			// which shall listen to the context
			mProgressListener = new CProgressListener(this, inContext);
		} catch (...) {
			mProgressListener = NULL;
		}
		mContext->AddUser(this);
		// now hook up CProxyPane to listen to context
		CProxyPane* browserPageProxy = dynamic_cast<CProxyPane*>(FindPaneByID(CProxyPane::class_ID));
		if (browserPageProxy)
			mContext->AddListener(browserPageProxy);
		// now hook up CURLCaption to listen to context
		CURLCaption* urlCaption = dynamic_cast<CURLCaption*>(FindPaneByID(CURLCaption::class_ID));
		if (urlCaption)
			mContext->AddListener(urlCaption);
		// now hook up CURLEditField to listen to context
		CURLEditField* urlField = dynamic_cast<CURLEditField*>(FindPaneByID(CURLEditField::class_ID));
		if (urlField)
			{
			mContext->AddListener(urlField);
			urlField->AddListener(this);
			if (urlCaption)
				urlField->AddListener(urlCaption);
			}
		CSpinningN* theN = dynamic_cast<CSpinningN*>(FindPaneByID(CSpinningN::class_ID));
		if (theN)
			mContext->AddListener(theN);

		CBrowserSecurityButton* securityButton =
			dynamic_cast<CBrowserSecurityButton*>(FindPaneByID(CBrowserSecurityButton::class_ID));
		if (securityButton)
			mContext->AddListener(securityButton);

		CMiniSecurityButton* miniSecurityButton =
			dynamic_cast<CMiniSecurityButton*>(FindPaneByID(CMiniSecurityButton::class_ID));
		if (miniSecurityButton)
			mContext->AddListener(miniSecurityButton);
		
		// setup navCenter for sitemaps
		if ( mNavCenterParent && !HasAttribute(windAttr_Floating))
			mNavCenterParent->RegisterNavCenter ( *inContext );

	}
		
	GetHTMLView()->SetContext(mContext);
	
	// This call links up the model object hierarchy for any potential
	// sub model that gets created within the scope of the html view.
	GetHTMLView()->SetFormElemBaseModel(this);
	
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CNSContext* CBrowserWindow::GetWindowContext() const
{
	return mContext;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserWindow::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
{
	outUsesMark = false;
	
	if (mIsHTMLHelp && UDesktop::FrontWindowIsModal() && inCommand != cmd_About)
	{
		outEnabled = false;
		return;
	}
	// Floating browser windows whose commands have been disabled should only disable the
	// commands that apply to the floating window.
	if (mCommandsDisabled)
		if (HasAttribute(windAttr_Floating))
		{
			if (	
				inCommand == cmd_GoBack ||
				inCommand == cmd_GoForward ||
				inCommand == cmd_Home ||
				inCommand == cmd_Close ||
				inCommand == cmd_ToggleToolbar ||
				inCommand == cmd_ToggleURLField ||
				inCommand == cmd_Reload ||
				inCommand == cmd_LoadImages ||
				inCommand == cmd_Stop
#ifdef EDITOR
				||
				inCommand == cmd_EditFrameSet ||
				inCommand == cmd_EditDocument
#endif // EDITOR
				)
			{
				outEnabled = false;
				return;
			}
		}
		// Commands may be disabled from javascript. The motivation for this is to prevent
		// users from loading new content into windows that have a special purpose (i.e. Constellation).
		// Don't disable Apple Menu Items or Quit menu item.
		// NOTE: it appears that the Apple Menu Items will be disabled unless I enable
		// the About command. Assume for now that menubar hiding will be implemented, so the
		// user won't be able to access the About menu item.
		else if (
				inCommand != cmd_Quit &&
				inCommand != cmd_SecurityInfo &&
				inCommand != cmd_About)
			{
				outEnabled = false;
				return;
			}

	// 
	// Handle the remainder of commands
	//
	switch (inCommand)
	{
		// the bevel button for the branding icon asks us if this command should be enabled
		// so that the button can be enabled. We always want this button enabled, so return 
		// true.
		case LOGO_BUTTON:
			outEnabled = true;
			break;
			
		case cmd_DocumentInfo:
		case cmd_ViewSource:
			if (mContext && (mIsRootDocInfo || mIsViewSource || mIsHTMLHelp))
			{
				outEnabled = false;
				break;
			}
		case cmd_PageSetup:
		case cmd_Print:
		case cmd_PrintOne:
		case cmd_AddToBookmarks:
		case cmd_SaveAs:
		case cmd_SecurityInfo:
		case cmd_FTPUpload:
		case cmd_Find:
		case cmd_MailDocument:
		case cmd_LoadImages:
		case cmd_Reload:
		{
			// Delegate this to the view.
			GetHTMLView()->FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			break;
		}
		case cmd_GoForward:
			if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey))
			{
				LString::CopyPStr(::GetPString(MENU_FORWARD_ONE_HOST), outName);
			}
			else
			{
				LString::CopyPStr(::GetPString(MENU_FORWARD), outName);
			}

			if (mContext != NULL)
			{
				outEnabled = mContext->CanGoForward() && !(mIsRootDocInfo || mIsViewSource || mIsHTMLHelp);
				
			}
			break;
		case cmd_GoBack:
			if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey))
			{
				LString::CopyPStr(::GetPString(MENU_BACK_ONE_HOST), outName);
			}
			else
			{
				LString::CopyPStr(::GetPString(MENU_BACK), outName);
			}
		
			if (mContext != NULL)
			{
				outEnabled = mContext->CanGoBack() && !(mIsRootDocInfo || mIsViewSource || mIsHTMLHelp);
				
			}
			break;
		case cmd_Home:
			outEnabled = (mContext) ? !(mIsRootDocInfo || mIsViewSource || mIsHTMLHelp) : true;
			break;

		case cmd_ToggleToolbar:
			outEnabled = (mContext) ? !(mIsRootDocInfo || mIsViewSource || mIsHTMLHelp || PREF_PrefIsLocked(Pref_ShowToolbar)) : true;
			::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, (mToolbarShown[eNavigationBar] ? HIDE_NAVIGATION_TOOLBAR_STRING : SHOW_NAVIGATION_TOOLBAR_STRING));
			break;
		
		case cmd_ToggleURLField:
			outEnabled = (mContext) ? !(mIsRootDocInfo || mIsViewSource || mIsHTMLHelp || PREF_PrefIsLocked(Pref_ShowLocationBar)) : true;
			::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, (mToolbarShown[eLocationBar] ? HIDE_LOCATION_TOOLBAR_STRING : SHOW_LOCATION_TOOLBAR_STRING));
			break;

		case cmd_TogglePersonalToolbar:
			outEnabled = (mContext) ? !(mIsRootDocInfo || mIsViewSource || mIsHTMLHelp || PREF_PrefIsLocked(Pref_ShowPersonalToolbar)) : true;
			::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, (mToolbarShown[ePersonalToolbar] ? HIDE_PERSONAL_TOOLBAR_STRING : SHOW_PERSONAL_TOOLBAR_STRING));
			break;
		
		case cmd_NCToggle:
			outEnabled = (mContext && mNavCenterParent) ? !(mIsRootDocInfo || mIsViewSource || mIsHTMLHelp || PREF_PrefIsLocked(CRDFCoordinator::Pref_ShowNavCenterSelector)) : true;
			Uint32 strID = SHOW_NAVCENTER_STRING;
			if ( mNavCenterParent && mNavCenterParent->NavCenterSelector().IsShelfOpen() )
				strID = HIDE_NAVCENTER_STRING;
			::GetIndString(outName, BROWSER_MENU_TOGGLE_STRINGS_ID, strID);
			break;
			
		case cmd_NetSearch:
			outEnabled = (mContext) ? !(mIsRootDocInfo || mIsViewSource || mIsHTMLHelp) : true;
			break;
		
		case cmd_Stop:
		{
			// Default name is STOP_LOADING_INDEX
				
			::GetIndString(outName, STOP_STRING_LIST, STOP_LOADING_INDEX );
			
			if (mContext)
			{
				if (XP_IsContextStoppable((MWContext*) (*mContext)))
				{
					outEnabled = true;
				}
				else if (mContext->IsContextLooping())
				{
					outEnabled = true;
				
					::GetIndString(outName, STOP_STRING_LIST, STOP_ANIMATIONS_INDEX );
				}
			}
			break;
		}
		
#ifdef EDITOR
		case cmd_EditFrameSet:
		case cmd_EditDocument:
		{
			if ((mContext != nil) && !(XP_IsContextBusy(*mContext)) && !Memory_MemoryIsLow())
				outEnabled = !(mIsRootDocInfo || mIsViewSource || mIsHTMLHelp);
			break;
		}
#endif // EDITOR
		
		case cmd_PageServices:
			outEnabled = mSupportsPageServices;
			break;
		
		default:
		{
			if(inCommand >= ENCODING_BASE && inCommand < ENCODING_CEILING)
			{
				// Delegate this to the view.
				GetHTMLView()->FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			}
			else
				CNetscapeWindow::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
		}
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Revised to handle back, forward, home, reload through AppleEvents - 1997-02-26 mjc
Boolean	CBrowserWindow::ObeyCommand(
	CommandT			inCommand,
	void				*ioParam)
{
	LCommander	*target;
	Boolean		cmdHandled = false;
	// check for synthetic commands from history items
	if (CHistoryMenu::IsHistoryMenuSyntheticCommandID(inCommand) && mContext != NULL)
	{
		// Compute correct history list index.
		// Menu item number - last non-dynamic menu item number is
		// index to history entry starting from the *END* of history list --
		// because we list history entries in menu bar starting with the most
		// recent history entry at the top which is at the end of the history list in the
		// MWContext. Therefore, compute correct history list index before calling
		// CNSContext::LoadHistoryEntry
		Int32 histIndex = mContext->GetHistoryListCount() -
						  (CHistoryMenu::GetFirstSyntheticCommandID() - inCommand);
		// histIndex should now be the one-based index to the history entry we want
		// starting from the beginning of the list.
		mContext->LoadHistoryEntry(histIndex);
		cmdHandled = true;
	}
	else
	{
		switch (inCommand)
		{
			case cmd_Print:
			case cmd_PrintOne:
				// if the current target happens to be a subview of our main HTMLView, let it
				// handle the command.  else, hand it to our main HTML view.
				target = LCommander::GetTarget();
				while (target && target != GetHTMLView())
					target = target->GetSuperCommander();
				if (target == GetHTMLView())
					LCommander::GetTarget()->ObeyCommand (inCommand, ioParam);
				else
					GetHTMLView()->ObeyCommand (inCommand, ioParam);
				break;
			case cmd_ViewSource:
			case cmd_Find:
			case cmd_AddToBookmarks:
			case cmd_SaveAs:
			case cmd_SecurityInfo:
			case cmd_DocumentInfo:
			case cmd_MailDocument:
			case cmd_LoadImages:
			case cmd_FTPUpload:
				{
					// Delegate this to the view.
					GetHTMLView()->ObeyCommand(inCommand, ioParam);
				}
				break;

#if 0			
			// The forward and back buttons are LBevelButtons, which means they will
			// broadcast when the user clicks in them. |ioParam| will be the value of
			// the popup menu, or 0 if the user clicked the button.
			case cmd_GoForward:
				if ( ioParam > 0 ) {
				
				
				}
					else {
					if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey))
					{
						if (mContext)
						{
							mContext->GoForwardOneHost();
						}
					}
					else
					{
						SendAEGo(kAENext);
					}
					cmdHandled = true;
				}
				break;
				
			case cmd_GoBack:
				if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey))
				{
					if (mContext)
					{
						mContext->GoBackOneHost();
					}
				}
				else
				{
					SendAEGo(kAEPrevious);
				}
				cmdHandled = true;
				break;
#endif
				
			case cmd_Home:
				SendAEGo(AE_www_go_home);
				
				// ¥¥¥ what the hell is this?	deeje 97-03-06
				// cmdHandled = CNetscapeWindow::ObeyCommand(inCommand, ioParam);
				break;
				
			case cmd_NetSearch:
				HandleNetSearchCommand();
				break;
							
			case cmd_ToggleToolbar:
				ToggleDragBar(Button_Bar_PaneID, eNavigationBar, Pref_ShowToolbar);
				cmdHandled = true;
				break;
			
			case cmd_ToggleURLField:
				ToggleDragBar(Location_Bar_PaneID, eLocationBar, Pref_ShowLocationBar);
				cmdHandled = true;
				break;

			case cmd_TogglePersonalToolbar:
				ToggleDragBar(PersonalToolbar_PaneID, ePersonalToolbar, Pref_ShowPersonalToolbar);
				cmdHandled = true;
				break;
				
			case cmd_NCToggle:
				// Note that this allows the user to hide the selector while the shelf is 
				// still open. This isn't the end of the world, at least, because the closebox
				// is there so the user can still figure out how to get rid of it and
				// close it if they want.
				if ( mNavCenterParent )
					mNavCenterParent->NavCenterSelector().ToggleShelf();
				cmdHandled = true;
				break;
								
			case cmd_Reload:
			{
				if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey) ||
					CApplicationEventAttachment::CurrentEventHasModifiers(shiftKey))
				{
					SendAEGo(AE_www_super_reload);
				}
				else
				{
					SendAEGo(AE_www_go_again);
				}
				cmdHandled = true;
				break;
			}
			
			case cmd_Stop:
			{
				TrySetCursor(watchCursor);
				XP_InterruptContext(*mContext);
				SetCursor( &qd.arrow );
				cmdHandled = true;
				break;
			}
			
#ifdef EDITOR
			case cmd_EditFrameSet:
			case cmd_EditDocument:
				MWContext *frontWindowMWContext;
				frontWindowMWContext = XP_GetNonGridContext( GetWindowContext()->operator MWContext*() );
				CEditorWindow::MakeEditWindowFromBrowser( frontWindowMWContext );
				cmdHandled = true;
			break;
#endif // EDITOR

			case cmd_SaveDefaultCharset:
			{
				Int32 default_csid = GetDefaultCSID();
				CPrefs::SetLong(default_csid, CPrefs::DefaultCharSetID);
			}
			break;
						
			case cmd_PageServices:
			{
				char* url = SHIST_GetCurrentPageServicesURL(*mContext);
				if (url)
				{
					URL_Struct* newURL = NET_CreateURLStruct(url, NET_DONT_RELOAD);
					if (newURL)
						mContext->SwitchLoadURL(newURL, FO_CACHE_AND_PRESENT);
				}
			}
			break;

			default:
			{
				if ( inCommand > ENCODING_BASE && inCommand < ENCODING_CEILING )
				{
					// Delegate this to the view.
					GetHTMLView()->SetDefaultCSID(CPrefs::CmdNumToDocCsid(inCommand));
					cmdHandled = true;
				}
				else
					cmdHandled = CNetscapeWindow::ObeyCommand(inCommand, ioParam);
				break;
			}
			
		} // case of which command
	} // else not a history command

	return cmdHandled;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserWindow::ListenToMessage(MessageT inMessage, void* ioParam)
{
	switch (inMessage)
		{
		case msg_NSCDocTitleChanged:
			NoteDocTitleChanged((const char*)ioParam);
			break;
		
		case msg_NSCLayoutNewDocument:
			NoteBeginLayout();
			break;
		
		case msg_NSCFinishedLayout:
			NoteFinishedLayout();
			break;
		
		case msg_NSCAllConnectionsComplete:
			NoteAllConnectionsComplete();
			break;
		
		case msg_UserSubmittedURL:
			// user hit enter or return in URL edit field
			CStr255* urlString = (CStr255*)ioParam;
			if (urlString && mContext)
			{
				if (!urlString->IsEmpty())
					SendAEGetURL(*urlString);
			}
			break;
		
		case cmd_Home:
		case cmd_Reload:
		case cmd_Stop:
		case cmd_LoadImages:
		case cmd_Print:
		case cmd_CoBrandLogo:
		case cmd_NetSearch:
		case cmd_SecurityInfo:
		case LOGO_BUTTON:
			ObeyCommand(inMessage, ioParam);
			break;
		}
}





void CBrowserWindow::AttemptClose()
{
	if (HasProcess(fCloseNotifier))	// If someone has registered for window closing, notify them
	Try_
	{
		AEAddressDesc 	closeApp;
		AppleEvent	 	closeEvent;
		Int32			aWindowID = mContext->GetContextUniqueID();
		
		OSErr err = ::AECreateDesc(typeProcessSerialNumber, &fCloseNotifier, 
							 sizeof(fCloseNotifier), &closeApp);
		ThrowIfOSErr_(err);
		err = ::AECreateAppleEvent(AE_spy_send_suite, AE_spy_winClosed,
									&closeApp,
									kAutoGenerateReturnID,
									kAnyTransactionID,
									&closeEvent);
		ThrowIfOSErr_(err);
		
			// windowID is the direct object
		err = ::AEPutParamPtr(&closeEvent, keyDirectObject,
					typeLongInteger, &aWindowID, sizeof(aWindowID));
		ThrowIfOSErr_(err);
		
		// Are we quitting is another parameter
		Boolean quitting = (CFrontApp::GetApplication()->GetState() == programState_Quitting);
		err =::AEPutParamPtr(&closeEvent, AE_spy_winClosedExiting, typeBoolean,
				&quitting, sizeof(quitting));
		ThrowIfOSErr_(err);
		
			// send the event
		AppleEvent reply;
		::AESend(&closeEvent,&reply,kAENoReply,kAENormalPriority,0,nil, nil);
	}
	Catch_(inErr){}
	EndCatch_

	AttemptCloseWindow();
	CMediatedWindow::AttemptClose();
}


// modified so z-locked window will not automatically be selected - mjc
void
CBrowserWindow::Select()
{
	if (!mZLocked)
	{
		SendSelfAE(kAEMiscStandards, kAESelect, false);
		UDesktop::SelectDeskWindow(this);
	}
}


//
// ActivateSelf
//
// When this window comes to the front, we want to make sure that RDF knows about this window's
// context so that navCenter windows can display the site map info correctly for this window.
//
// Make sure not to do this for floating windows or for editor windows.
//
void
CBrowserWindow::ActivateSelf ( )
{
	super::ActivateSelf();
	if ( mNavCenterParent && !HasAttribute(windAttr_Floating) )
		XP_SetLastActiveContext ( *GetWindowContext() );
}


// ClickInDrag modified so z-locked window will not automatically be selected - mjc
void	
CBrowserWindow::ClickInDrag(const EventRecord &inMacEvent)
{
	if (mZLocked) 
	{
		EventRecord filterMacEvent = inMacEvent;
		filterMacEvent.modifiers = filterMacEvent.modifiers | cmdKey; // command-key modifier disables select
		super::ClickInDrag(filterMacEvent);
	}
	else super::ClickInDrag(inMacEvent);
}

// Allow us to restrict user from growing the window by setting the Resizable attribute,
// even if there is a grow box.
void			
CBrowserWindow::ClickInGrow(const EventRecord &inMacEvent)
{
	if (HasAttribute(windAttr_Resizable))
		super::ClickInGrow(inMacEvent);
}

// Return the window for the top-level context of the one passed in.
CBrowserWindow*	
CBrowserWindow::WindowForContext(CBrowserContext* inContext)
{
	if (inContext == NULL) return NULL;
	
	CMediatedWindow* theIterWindow = NULL;
	DataIDT windowType = WindowType_Browser;
	CWindowIterator theWindowIterator(windowType);
	
	CBrowserContext* theTopContext = inContext->GetTopContext();
	
	while (theWindowIterator.Next(theIterWindow))
		{
		CBrowserWindow* theBrowserWindow = dynamic_cast<CBrowserWindow*>(theIterWindow);
		CNSContext* theCurrentContext = theBrowserWindow->GetWindowContext();
		if (theBrowserWindow->GetWindowContext() == theTopContext)
			{
				return theBrowserWindow;
			break;
			}
		}
	return NULL;
}

// Retrieve a window with the given Window ID
//		In : ID of window
//		Out: Pointer returned.		(ct hull)
//  GetWindowByID
// return the window for the context.
CBrowserWindow*	
CBrowserWindow::GetWindowByID(const Int32 windowID)
{
	CMediatedWindow* theIterWindow = NULL;
	DataIDT windowType = WindowType_Browser;
	CWindowIterator theWindowIterator(windowType);
	while (theWindowIterator.Next(theIterWindow))
		{
		CBrowserWindow* theBrowserWindow = dynamic_cast<CBrowserWindow*>(theIterWindow);
		CNSContext* theCurrentContext = theBrowserWindow->GetWindowContext();
		if (theCurrentContext->GetContextUniqueID() == windowID)
			{
				return theBrowserWindow;
			break;
			}
		}
	return NULL;
}

Boolean
CBrowserWindow::IsRestrictedTarget(void)
{
	return mContext->IsRestrictedTarget();
}

// override to send javascript move, resize, and zoom events
void	
CBrowserWindow::DoSetPosition(Point inPosition)
{
	super::DoSetPosition(inPosition);

	if (mContext != NULL) 
	{
		Rect bounds;
		bounds = UWindows::GetWindowStructureRect(GetMacPort());
		CMochaHacks::SendMoveEvent(*mContext, bounds.left, bounds.top - ::GetMBarHeight());
	}
}

void	
CBrowserWindow::DoSetBounds(const Rect &inBounds)
{
									// inBounds is in global coordinates
									
									// Early exit if the bounds have not
									//   really changed.
	Rect bounds = UWindows::GetWindowContentRect(GetMacPort());
	
	if (::EqualRect(&bounds, &inBounds))
		return;
	
	Boolean didMove = (bounds.top != inBounds.top) || (bounds.left != inBounds.left);
	
									// Set size and location of Toolbox
									//   WindowRecord
	::SizeWindow(mMacWindowP, inBounds.right - inBounds.left,
				inBounds.bottom - inBounds.top, false);
	::MoveWindow(mMacWindowP, inBounds.left, inBounds.top, false);
	
									// Set our Frame
	ResizeFrameTo(inBounds.right - inBounds.left,
				inBounds.bottom - inBounds.top, true);

	SDimension16	frameSize;		// For Windows, Image is always the
	GetFrameSize(frameSize);		//   same size as its Frame
	ResizeImageTo(frameSize.width, frameSize.height, false);
	
									// Changing Bounds establishes a
									//   new User state for zooming
	CalcPortFrameRect(mUserBounds);
	PortToGlobalPoint(topLeft(mUserBounds));
	PortToGlobalPoint(botRight(mUserBounds));
	mMoveOnlyUserZoom = false;


	if (mContext != NULL) 
	{
		Rect structureBounds = UWindows::GetWindowStructureRect(GetMacPort());
		if (didMove)
			CMochaHacks::SendMoveEvent(*mContext, structureBounds.left, structureBounds.top - ::GetMBarHeight());
		//CMochaHacks::SendResizeEvent(*mContext, structureBounds.right - structureBounds.left, structureBounds.bottom - structureBounds.top);
	}
}

void	
CBrowserWindow::DoSetZoom(Boolean inZoomToStdState)
{
	if (!HasAttribute(windAttr_Zoomable)) {
		ThrowOSErr_(errAENotModifiable);
	}

	Rect	currBounds = UWindows::GetWindowContentRect(mMacWindowP);
	Rect	zoomBounds;

	if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey))
	{
		
		StValueChanger<Int16> changeStandardWidth(mStandardSize.width, max_Int16); 
		StValueChanger<Int16> changeStandardHeight(mStandardSize.height, max_Int16); 

		CalcStandardBounds(zoomBounds);
	
	
										// Changing bounds via option-zoom
										//   establishes a new User state
										//   for zooming
		mUserBounds = zoomBounds;
	}
	else
	{
		if (inZoomToStdState) {			// Zoom to Standard state
			if (CalcStandardBounds(zoomBounds)) {
				return;					// Already at Standard state
			}
			
		} else {						// Zoom to User state
			zoomBounds = mUserBounds;
			
			if (mMoveOnlyUserZoom) {	// Special case for zooming a Window
										//   that is at standard size, but
										//   is partially offscreen
				zoomBounds.right = zoomBounds.left +
									(currBounds.right - currBounds.left);
				zoomBounds.bottom = zoomBounds.top +
									(currBounds.bottom - currBounds.top);
			}
		}
	}
	
	Int16	zoomWidth = zoomBounds.right - zoomBounds.left;
	Int16	zoomHeight = zoomBounds.bottom - zoomBounds.top;
	mMoveOnlyUserZoom = false;
	
		// To avoid unnecessary redraws, we check to see if the
		// current and zoom states are either the same size
		// or at the same location
		
	if ( ((currBounds.right - currBounds.left) == zoomWidth) &&
		 ((currBounds.bottom - currBounds.top) == zoomHeight) ) {
									// Same size, just move
		::MoveWindow(mMacWindowP, zoomBounds.left, zoomBounds.top, false);
		mMoveOnlyUserZoom = true;
	
	} else if (::EqualPt(topLeft(currBounds), topLeft(zoomBounds))) {
									// Same location, just resize
		::SizeWindow(mMacWindowP, zoomWidth, zoomHeight, false);
		ResizeFrameTo(zoomWidth, zoomHeight, true);
		
	} else {						// Different size and location
									// Zoom appropriately
		FocusDraw();
		ApplyForeAndBackColors();
		::EraseRect(&mMacWindowP->portRect);
		if (inZoomToStdState) {
			SetWindowStandardState(mMacWindowP, &zoomBounds);
			::ZoomWindow(mMacWindowP, inZoomOut, false);
		} else {
			SetWindowUserState(mMacWindowP, &zoomBounds);
			::ZoomWindow(mMacWindowP, inZoomIn, false);
		}
		ResizeFrameTo(zoomWidth, zoomHeight, false);
	}
	
	if (mContext != NULL) 
	{
		Rect bounds;
		bounds = UWindows::GetWindowStructureRect(GetMacPort());
		CMochaHacks::SendMoveEvent(*mContext, bounds.left, bounds.top - ::GetMBarHeight());
		//CMochaHacks::SendResizeEvent(*mContext, bounds.right - bounds.left, bounds.bottom - bounds.top);
	}
}

// ---------------------------------------------------------------------------
//		¥ CalcStandardBoundsForScreen
// ---------------------------------------------------------------------------

void
CBrowserWindow::CalcStandardBoundsForScreen(
	const Rect&	inScreenBounds,
	Rect&		outStdBounds) const
{
	CHTMLView*			theHTMLView = nil;
	CBrowserContext*	theTopContext = nil;
	
	theTopContext = dynamic_cast<CBrowserContext*>(mContext->GetTopContext());
	if (theTopContext)
	{
		theHTMLView = ::ExtractHyperView((MWContext*)(*theTopContext));
	}

	StValueChanger<Int16> saveStandardWidth(const_cast<CBrowserWindow*>(this)->mStandardSize.width, max_Int16); 
	StValueChanger<Int16> saveStandardHeight(const_cast<CBrowserWindow*>(this)->mStandardSize.height, max_Int16); 

	CHTMLView::CalcStandardSizeForWindowForScreen(
										theHTMLView,
										*this,
										inScreenBounds,
										const_cast<CBrowserWindow*>(this)->mStandardSize);
	
	// Calculate standard bounds given standard size
	
	super::CalcStandardBoundsForScreen(inScreenBounds, outStdBounds);
}

// dynamically show/hide the status bar - mjc
void			
CBrowserWindow::ShowStatus(Chrome* inChromeInfo)
{
	LPane* 			theSWatchStuff = FindPaneByID(SWatch_View_PaneID);
	LPane*			theStatusBar = FindPaneByID(Status_Bar_PaneID);
	
	// ¥ show or hide status bar
	if (theStatusBar != nil && theSWatchStuff != nil)
	{
		if (!inChromeInfo->show_bottom_status_bar) // status=no
		{
			// FIXME grow box draws correctly?
			if (mToolbarShown[eStatusBar])
			{
				SDimension16		theStatusSize = {0, 0};
				theStatusBar->GetFrameSize(theStatusSize);
				theStatusBar->Hide();
				theSWatchStuff->ResizeFrameBy(0, theStatusSize.height, false);
				mToolbarShown[eStatusBar] = false;
				// 97-05-12 pkc -- have CHyperScroller handle grow icon if we're
				// resizeable
				// 97-06-05 pkc -- hide size box because it looks bad
				ClearAttribute(windAttr_SizeBox);
				if (inChromeInfo->allow_resize)
				{
					CHyperScroller* scroller = dynamic_cast<CHyperScroller*>(FindPaneByID(Hyper_Scroller_PaneID));
					if (scroller)
						scroller->SetHandleGrowIconManually(true);
				}
			}
		}
		else if (!mToolbarShown[eStatusBar]) // status=yes
		{
			// we want to show status bar, but it's currently hidden
			SDimension16		theStatusSize = {0, 0};
			theStatusBar->Show();
			theStatusBar->GetFrameSize(theStatusSize);
			theSWatchStuff->ResizeFrameBy(0, -theStatusSize.height, false);
			mToolbarShown[eStatusBar] = true;
			// 97-05-12 pkc -- tell CHyperScroller not to handle grow icon if we're
			// resizeable
			if (inChromeInfo->allow_resize)
			{
				CHyperScroller* scroller = dynamic_cast<CHyperScroller*>(FindPaneByID(Hyper_Scroller_PaneID));
				if (scroller)
				{
					SetAttribute(windAttr_SizeBox);
					scroller->SetHandleGrowIconManually(false);
				}
				// make sure we turn on bevelling of grow icon area
				SetSystemGrowIconBevel();
			}
			else
				ClearAttribute(windAttr_SizeBox);			
		}
		else if (!inChromeInfo->allow_resize)
		{
			// we want to show status bar, it's already showing, and we don't
			// want it resizeable
			ClearAttribute(windAttr_SizeBox);
			// also clear bevelling of grow icon area
			ClearSystemGrowIconBevel();
		}
	}
}
	
// dynamically show/hide the drag bars - mjc (without changing pref value)
// modified to synchronize with menu items by using ShowOneDragBar and mToolbarShown - 1997-02-27 mjc
void			
CBrowserWindow::ShowDragBars(Boolean inShowNavBar, Boolean inShowLocBar, Boolean inShowPersToolbar)
{
	ShowOneDragBar(Button_Bar_PaneID, inShowNavBar);
	mToolbarShown[eNavigationBar] = inShowNavBar;

	ShowOneDragBar(Location_Bar_PaneID, inShowLocBar);
	mToolbarShown[eLocationBar] = inShowLocBar;
	
	ShowOneDragBar(PersonalToolbar_PaneID, inShowPersToolbar);
	mToolbarShown[ePersonalToolbar] = inShowPersToolbar;	
}

void
CBrowserWindow::ClearSystemGrowIconBevel()
{
	CBevelView* statusArea = dynamic_cast<CBevelView*>(FindPaneByID('StBv'));
	if (statusArea)
		statusArea->DontBevelGrowIcon();
}

void
CBrowserWindow::SetSystemGrowIconBevel()
{
	CBevelView* statusArea = dynamic_cast<CBevelView*>(FindPaneByID('StBv'));
	if (statusArea)
		statusArea->BevelGrowIcon();
}

// set up the window based on the chrome structure
// this may be called more than once.
void CBrowserWindow::SetChromeInfo(Chrome* inChrome, Boolean inNotifyMenuBarModeChanged, Boolean inFirstTime)
{
	LPane*		theHTMLContainer = FindPaneByID(HTML_Container_PaneID);
	LPane* theSWatchStuff = FindPaneByID(SWatch_View_PaneID);
	CHyperScroller* theScroller = (CHyperScroller*)FindPaneByID(Hyper_Scroller_PaneID);

	if (inChrome != nil)
	{
		Assert_(mMacWindowP != NULL);
		Rect		windowBounds = {0, 0, 0, 0};
		Rect		contentBounds  = {0, 0, 0, 0};
		Rect		newWindowBounds = {0, 0, 0, 0};
		Rect		portRect = mMacWindowP->portRect;
		short 		titlebarHeight, deltaWidth, deltaHeight;
		
		FocusDraw();
	
		// ¥ reposition bars based on chrome attributes. The "directory button" is really
		// the personal toolbar, though no one has thought to update the chrome structure yet.
		ShowDragBars(inChrome->show_button_bar, inChrome->show_url_bar, inChrome->show_directory_buttons);
		
		// ¥ show or hide scrollbars
		if (theScroller != nil)
		{
			if (inChrome->show_scrollbar)
				GetHTMLView()->ResetScrollMode();
			else GetHTMLView()->SetScrollMode(LO_SCROLL_NEVER);
		}
		
		//¥¥¥ This should be reflected in the Chrome data structure, but since I'm too scared to
		// touch it, we can hack it so that if the chrome says not to have a personal toolbar, then
		// we probably don't want the NavCenter showing up. Don't let this temporary setting of
		// the shelf states affect the global preference, however.
		if ( mNavCenterParent ) {
			if ( !inChrome->show_directory_buttons ) {
				mNavCenterParent->NavCenterShelf().SetShelfState(CShelf::kClosed, false);
				mNavCenterParent->NavCenterSelector().SetShelfState(CShelf::kClosed, false);
			}
			else
				mNavCenterParent->NavCenterSelector().SetShelfState(CShelf::kOpen, false);
		}
		ShowStatus(inChrome);
		
		// w_hint and h_hint are inner dimensions (html view).
		// outw_hint and outh_hint are outer dimensions (window structure).
		// l_hint and t_hint are position of window structure.
		// location_is_chrome will always be set for positioning so that {0,0} 
		// positioning is possible (i.e. we know the chrome record is not just zeroed out).
		// ALERT: positioning applies menubar height regardless of multiple monitor situation.
		
		windowBounds = UWindows::GetWindowStructureRect(GetMacPort());
		contentBounds = UWindows::GetWindowContentRect(GetMacPort());
		// if the window is windowshaded then correct the windowBounds because height of content is zero.
		if (IsVisible() && HasAttribute(windAttr_TitleBar) && ::EmptyRgn(((WindowPeek)GetMacPort())->contRgn))
		{
			short lostHeight = portRect.bottom - portRect.top; // portRect is immune to windowshading
			windowBounds.bottom = windowBounds.bottom + lostHeight;
			contentBounds.bottom = contentBounds.bottom + lostHeight;
		}
		deltaWidth = (windowBounds.right - windowBounds.left) - (contentBounds.right - contentBounds.left); // difference of width of structure and content
		deltaHeight = (windowBounds.bottom - windowBounds.top) - (contentBounds.bottom - contentBounds.top); // difference of height of structure and content
		titlebarHeight = contentBounds.top - windowBounds.top;
		
		// convert newWindowBounds to the content rect suitable for passing to DoSetBounds().
		if ((inChrome->outw_hint && inChrome->outh_hint) ||
			(inChrome->w_hint && inChrome->h_hint) ||
			(inChrome->location_is_chrome)) // reposition is intended
		{
			RgnHandle desktopRgnH = ::GetGrayRgn(); // max size of window is size of desktop region
			short maxWidth = MAX((**desktopRgnH).rgnBBox.right - (**desktopRgnH).rgnBBox.left, 100);
			short maxHeight = MAX((**desktopRgnH).rgnBBox.bottom - (**desktopRgnH).rgnBBox.top, 100);

			if (inChrome->location_is_chrome) { // position if specified
				newWindowBounds.left = inChrome->l_hint + contentBounds.left - windowBounds.left;
				newWindowBounds.top = inChrome->t_hint + titlebarHeight + ::GetMBarHeight();
			}
			else {
				newWindowBounds.left = contentBounds.left;
				newWindowBounds.top = contentBounds.top;
			}

			// set outer dimensions if specified or inner dimensions. Prevent a non-resizable
			// window from being resized unless it's the first time we're setting chrome info.
			if ((inChrome->outw_hint > 0) && 
				(inChrome->outh_hint > 0) && 
				(inChrome->allow_resize || inFirstTime)) // outer dimensions both > 0
			{
				// pin the window dimensions specified in the chrome to the maximum dimensions
				inChrome->outw_hint = MIN(maxWidth, inChrome->outw_hint);
				inChrome->outh_hint = MIN(maxHeight, inChrome->outh_hint);
				newWindowBounds.right = MIN(SHRT_MAX, newWindowBounds.left + inChrome->outw_hint - deltaWidth);
				newWindowBounds.bottom = MIN(SHRT_MAX, newWindowBounds.top + inChrome->outh_hint - deltaHeight);
			}
			else if ((inChrome->w_hint > 0) && 
						(inChrome->h_hint > 0) && 
						(inChrome->allow_resize || inFirstTime)) // inner dimensions both > 0
			{
				SDimension16 theInnerSize;
				/* if (theScroller != nil) theScroller->GetFrameSize(theInnerSize);
				else */ GetHTMLView()->GetFrameSize(theInnerSize);
				// pin the window dimensions specified in the chrome to the maximum dimensions
				inChrome->w_hint = MIN(maxWidth, inChrome->w_hint);
				inChrome->h_hint = MIN(maxHeight, inChrome->h_hint);
				newWindowBounds.right = MIN(SHRT_MAX, newWindowBounds.left + portRect.right - portRect.left + (inChrome->w_hint - theInnerSize.width));
				newWindowBounds.bottom = MIN(SHRT_MAX, newWindowBounds.top + portRect.bottom - portRect.top + (inChrome->h_hint - theInnerSize.height));	
			}
			else // no dimensions specified, so get default dimensions from port rect.
			{
				newWindowBounds.right = newWindowBounds.left + portRect.right - portRect.left;
				newWindowBounds.bottom = newWindowBounds.top + portRect.bottom - portRect.top;
			}
			if (!::EqualRect(&newWindowBounds, &contentBounds)) 
				DoSetBounds(newWindowBounds);
		}
		
		// ¥ set window attributes
		// the layout's window attributes don't necessarily specify floating so set it here.
		// z-lock means that the window cannot move in front of other windows in its layer.
		ClearAttribute(windAttr_Floating); // clear all the layer bits before we set again - 1997-02-28 mjc
		ClearAttribute(windAttr_Regular);
		ClearAttribute(windAttr_Modal);
		if (inChrome->topmost) // alwaysRaised=yes				
			SetAttribute(windAttr_Floating); // javascript under windows doesn't hide a topmost window so we won't either
		else SetAttribute(windAttr_Regular);
		
		if (inChrome->z_lock)
		{
			mZLocked = true;
			ClearAttribute(windAttr_DelaySelect);
			// z-locked window must be able to get the select click because it isn't selectable.
			SetAttribute(windAttr_GetSelectClick); 
		}
		else 
		{
			mZLocked = false;
			SetAttribute(windAttr_DelaySelect);
		}
		
		if (inChrome->bottommost && !inChrome->topmost)
		{
			// a bottommost window has to be in the regular layer.
			// if there is more than one regular window, we'll have to send it to the back.
			if (CWindowMediator::GetWindowMediator()->CountOpenWindows(
															WindowType_Browser,
															regularLayerType) > 1)
			{
				if (UDesktop::FetchTopRegular() == this)
					Deactivate();
				::SendBehind(GetMacPort(), NULL); // send to the back
				// The following hack is based on UDesktop::Resume() and fixes a case where the
				// window just sent to the back would be activated, when there is a floating
				// window present.
				LWindow* theTopWindow = UDesktop::FetchTopRegular();
				if (theTopWindow != NULL)
				{
					theTopWindow->Activate();
					LMSetCurActivate(nil); 	// Suppress any Activate event
				}
			}
			mAlwaysOnBottom = true;
			mZLocked = true; // bottommost implies z-lock
			ClearAttribute(windAttr_DelaySelect); // see z_lock for explanation
			SetAttribute(windAttr_GetSelectClick); 
		}
		else
		{
			mAlwaysOnBottom = false;
			if (mZLocked == false) SetAttribute(windAttr_DelaySelect);			
		}
		
		if (inChrome->hide_title_bar)
		{ 
			// a titlebar-less window must get the click that selected it because there is 
			// little feedback that window is selected.
			SetAttribute(windAttr_GetSelectClick);
		}
		else if (!HasAttribute(windAttr_Floating))
		{
			SetAttribute(windAttr_Targetable); // window which has titlebar and is non-floating, is targetable.
		}
		
		// 97-05-08 pkc -- set windAttr_Modal flag appropriately
		if (inChrome->type == MWContextDialog)
		{
			AllowSubviewPopups (false);
			if (!mContext->IsSpecialBrowserContext())
				SetAttribute(windAttr_Modal);
		}
		
		mIsRootDocInfo = mContext->IsRootDocInfoContext();
		mIsViewSource = mContext->IsViewSourceContext();
		
		// ¥¥¥ TODO: if we have the right security we should be able to set menubar mode to BrWn_Menubar_None
		if (inChrome->show_menu || mIsRootDocInfo || mIsViewSource)
			mMenubarMode = BrWn_Menubar_Default;
		else
			mMenubarMode = BrWn_Menubar_Minimal;
		
		// ¥ Broadcast that the menubar mode has changed
		if (inNotifyMenuBarModeChanged)
			NoteWindowMenubarModeChanged();

		mIsHTMLHelp = (inChrome->type == MWContextHTMLHelp);
		mCommandsDisabled = inChrome->disable_commands && !mIsHTMLHelp;

		if (inChrome->allow_resize && !HasAttribute(windAttr_Resizable))
		{
			Str255 desc;
			SetAttribute(windAttr_Resizable);
			if (!HasAttribute(windAttr_Floating))
			{
				((WindowPeek)GetMacPort())->spareFlag = true; 	// enable zoom box
				GetDescriptor(desc); 							// force titlebar update
				SetDescriptor(desc);
			}
		} else if (!inChrome->allow_resize && HasAttribute(windAttr_Resizable))
		{
			Str255 desc;
			ClearAttribute(windAttr_Resizable);
			if (!HasAttribute(windAttr_Floating))
			{
				((WindowPeek)GetMacPort())->spareFlag = false; 	// disable zoom box
				GetDescriptor(desc); 							// force titlebar update
				SetDescriptor(desc);
			}
		}
		
		Refresh();
	}
	else
	{
		// reset the window... but how?
	}
}

// Set the chrome structure based on window state - mjc
void CBrowserWindow::GetChromeInfo(Chrome* inChrome)
{
	// ¥ controls
	// modified to modified to synchronize with menu items by using mToolbarShown - 1997-02-27 mjc
	// The directory button bar is really the personal toolbar, though the chrome struct has yet to
	// be updated to reflect this change.
	inChrome->show_button_bar        	= mToolbarShown[eNavigationBar];
	inChrome->show_url_bar           	= mToolbarShown[eLocationBar];
	inChrome->show_directory_buttons 	= mToolbarShown[ePersonalToolbar];
	inChrome->show_bottom_status_bar 	= mToolbarShown[eStatusBar];
	inChrome->show_menu              	= mMenubarMode == BrWn_Menubar_Default;
	inChrome->show_security_bar      	= FALSE; // ¥¥¥ always false in javascript?
	
	inChrome->hide_title_bar			= !HasAttribute(windAttr_TitleBar);

	// ¥¥¥ NEED SOMETHING HERE FOR THE NAV CENTER
	
	// ¥ position and size use window structure bounds
	Rect windowBounds = UWindows::GetWindowStructureRect(GetMacPort());
	// if the window is windowshaded then correct the windowBounds because height of content is zero.
	if (IsVisible() && HasAttribute(windAttr_TitleBar) && ::EmptyRgn(((WindowPeek)GetMacPort())->contRgn))
		windowBounds.bottom = windowBounds.bottom + mMacWindowP->portRect.bottom - mMacWindowP->portRect.top;
	
	inChrome->outw_hint					= windowBounds.right - windowBounds.left;
	inChrome->outh_hint					= windowBounds.bottom - windowBounds.top;
	inChrome->l_hint				  	= windowBounds.left;
	inChrome->t_hint				  	= windowBounds.top - ::GetMBarHeight();
	inChrome->location_is_chrome		= TRUE; // set to insure these coordinates are used.
	
	// ¥ window properties
	inChrome->topmost		  			= this->HasAttribute(windAttr_Floating);
	//inChrome->bottommost      		= 
	inChrome->z_lock					= mZLocked;
	
	inChrome->is_modal               	= FALSE; // always FALSE in javascript
	inChrome->allow_resize           	= HasAttribute(windAttr_Resizable);
	inChrome->allow_close            	= TRUE; // always TRUE in javascript
	inChrome->disable_commands			= mCommandsDisabled;
	//inChrome->copy_history           	= FALSE;	/* XXX need data tainting */

	// default values if there is no scroller of HTMLView
	inChrome->w_hint					= inChrome->outw_hint;
	inChrome->h_hint					= inChrome->outh_hint;
	inChrome->show_scrollbar        	= FALSE;
	
	CHyperScroller* theScroller = (CHyperScroller*)FindPaneByID(Hyper_Scroller_PaneID);
	
	if (theScroller == NULL) return;
	
	SDimension16	theInnerSize;
	/* if (theScroller != nil) theScroller->GetFrameSize(theInnerSize);
	else */ GetHTMLView()->GetFrameSize(theInnerSize);
	
	inChrome->w_hint					= theInnerSize.width;
	inChrome->h_hint					= theInnerSize.height;
	
	inChrome->show_scrollbar        	= (theScroller != nil) && (GetHTMLView()->GetScrollMode() != LO_SCROLL_NEVER);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
		
void CBrowserWindow::NoteDocTitleChanged(const char* inNewTitle)
{
	CStr255 netscapeTitle;
	// Get Window title prefix
	::GetIndString( netscapeTitle, ID_STRINGS, APPNAME_STRING_INDEX );
	netscapeTitle += ": ";
	// append title
	netscapeTitle += inNewTitle;
	SetDescriptor( netscapeTitle );
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserWindow::NoteBeginLayout(void)
{
//	
//	if (!is_grid)	// If we are laying out main new document, adjust the URLs
//	{
//		CStr31 newTitle = CFileMgr::FileNameFromURL (request->address);
//		SetDocTitle( newTitle );
//		
//		if ( fTopWrapper )
//			fTopWrapper->SetLayingOutURLCaption( CStr255( NET_URLStruct_Address( request ) ) , request->is_netsite);	
//		fHasTitle = FALSE;
//	}	
//	this->UpdateStatusBar();

	// ¥¥¥ TELL NAVCENTER ABOUT THE CHANGE?

	// 1997-05-22 brade -- context can be NULL
	if (mContext)
	{
		// 1997-05-02 pkc -- hack to select doc info window if we're laying it out.
		if (mContext->IsRootDocInfoContext())
			Select();
		// 1997-05-09 pkc -- set mSupportsPageServices
		mSupportsPageServices = mContext->SupportsPageServices();
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
		
void CBrowserWindow::NoteFinishedLayout(void)
{
	// Do we need to do anything??
	// ¥ TELL NAVCENTER ABOUT THE CHANGE?
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CBrowserWindow::NoteAllConnectionsComplete(void)
{
//	NetscapeContext::AllConnectionsComplete();
//
//	HideProgressBar();
//
//	if (fProgress)
//		fProgress->SetValue(0);
//
//  	this->SetMessage( CStr255( *GetString(DOCUMENT_DONE_RESID)) );
//	this->UpdateStatusBar();
//	this->StopSpinIcon();

//	CleanupImagesInTempMemory();
//	fHyperView->RepaginateIfNeeded();		// This is needed for grids. The main view gets
			// done before its subviews are done, so repagination never happens
	
	// CPaneEnabler::UpdatePanes();		// moved this to CHTMLView.cp  deeje 97-02-10
}


CBrowserWindow* CBrowserWindow::MakeNewBrowserWindow(Boolean inShow, Boolean inSelect)
{	
	ThrowIf_(Memory_MemoryIsLow());

	CBrowserWindow* result = nil;

	try
	{
		CBrowserContext* theContext = new CBrowserContext(MWContextBrowser);
		StSharer theShareLock(theContext);
		
		result = dynamic_cast<CBrowserWindow*>(LWindow::CreateWindow(res_ID, LCommander::GetTopCommander()));
		ThrowIfNil_(result);
		result->SetWindowContext(theContext);	
		
		if (inShow)
			result->Show();
			
		if (inSelect)
			result->Select();
	}
	catch(...)
	{
	}
	
	return result;
}

CBrowserWindow* CBrowserWindow::FindAndShow(Boolean inMakeNew)
{	
	ThrowIf_(Memory_MemoryIsLow());

	CBrowserWindow*		result = NULL;
	CWindowIterator		iter(WindowType_Browser);
	CMediatedWindow*	window;
	
	iter.Next(window);

	if (window)
	{
		result = dynamic_cast<CBrowserWindow*>(window);
	}
	else if (inMakeNew)
	{
		try
		{
//			ThrowIfNil_(CURLDispatcher::GetURLDispatcher());
//			CURLDispatcher::GetURLDispatcher()->DispatchToView(nil, nil, FO_CACHE_AND_PRESENT, true);
			result = CURLDispatcher::CreateNewBrowserWindow();
		}
		catch(...)
		{
		}
		
//		iter.Next(window);
//
//		if (window)
//		{
//			result = dynamic_cast<CBrowserWindow*>(window);
//		}
	}
	else
	{
		CWindowMediator* mediator = CWindowMediator::GetWindowMediator();
		CMediatedWindow* topWindow = mediator->FetchTopWindow(WindowType_Browser, dontCareLayerType, false);
		result = dynamic_cast<CBrowserWindow*>(topWindow);
	}

	return result;
}

CBrowserWindow* CBrowserWindow::FindAndPrepareEmpty(Boolean inMakeNew)
{
/*
	CWindowIterator iter(WindowType_Browser);
	CMediatedWindow* window;
	CBrowserWindow* result = NULL;
	iter.Next(window);
*/
	CMediatedWindow* window;
	CBrowserWindow* result = NULL;
	// Find the topmost regular browser window which is not a restricted target
	window = CWindowMediator::GetWindowMediator()->FetchTopWindow(WindowType_Browser, regularLayerType, false);

	if (window)
		result = dynamic_cast<CBrowserWindow*>(window);
	else if (inMakeNew)
	{
		result = MakeNewBrowserWindow(kShow, kSelect);
	}
	return result;
}

Boolean CBrowserWindow::HandleKeyPress(const EventRecord &inKeyEvent)
{
	Char16		theChar = inKeyEvent.message & charCodeMask;
	short 		modifiers = inKeyEvent.modifiers & (cmdKey | shiftKey | optionKey | controlKey);

	// Command-Key Events
	if ( ( modifiers & cmdKey ) == cmdKey )
	{
		switch ( theChar & charCodeMask )
		{
			case char_LeftArrow:
				ObeyCommand( cmd_GoBack );
			return true;

			case char_RightArrow:
				ObeyCommand( cmd_GoForward );
			return true;
		}
	}
	
	// Passing on the scrolling keys
	
	// ¥¥¥ tj - only when modifiers are 0, because
	// we the hyperview may have already passed the key
	// up to us if there are  modifiers 
	//	
	
	if ( modifiers == 0 && GetHTMLView())
	{
		if ((theChar == char_UpArrow) ||
			(theChar == char_DownArrow) ||
			(theChar == char_PageUp) ||
			(theChar == char_PageDown) ||
			(theChar == ' ') ||
			(theChar == char_Home) ||
			(theChar == char_End))
				return GetHTMLView()->HandleKeyPress( inKeyEvent );
	}
	
	return CNetscapeWindow::HandleKeyPress(inKeyEvent);
}

//
// WriteWindowStatus
//
// Write out status of the toolbars and nav center
//
void CBrowserWindow :: WriteWindowStatus ( LStream *outStatusData )
{
	CSaveWindowStatus::WriteWindowStatus(outStatusData);
	CDragBarContainer* dragContainer = dynamic_cast<CDragBarContainer*>(FindPaneByID('DbCt'));
	if (dragContainer && outStatusData)
		dragContainer->SavePlace(outStatusData);
		
	if ( mNavCenterParent )
		mNavCenterParent->SavePlace(outStatusData);

} // CBrowserWindow :: WriteWindowStatus


//
// ReadWindowStatus
//
// Read in status of toolbars and nav center
//
void CBrowserWindow :: ReadWindowStatus ( LStream *inStatusData )
{
	CSaveWindowStatus::ReadWindowStatus(inStatusData);
	CDragBarContainer* dragContainer = dynamic_cast<CDragBarContainer*>(FindPaneByID('DbCt'));
	if (dragContainer && inStatusData)
		dragContainer->RestorePlace(inStatusData);
	
	if ( mNavCenterParent )
		mNavCenterParent->RestorePlace(inStatusData);
	
} // ReadWindowStatus


#pragma mark -- I18N Support --
Int16 	CBrowserWindow::DefaultCSIDForNewWindow()
{
	Int16		default_csid;
	CHTMLView	*mainView = GetHTMLView();

	/* mainView might be zero if the window is still being created (haven't executed
	   FinishCreateSelf).  (note that in that case, we could still fetch mainView by
	   calling FindPaneByID, but again in that case, the pane's context hasn't been set up
	   and the view simply returns a 0, so we just skip that step and do the same) */
	default_csid = mainView ? mainView->DefaultCSIDForNewWindow() : 0;
			
	// Get it from mContext
	if((0 == default_csid) && mContext)
		default_csid = mContext->GetDefaultCSID();
		
	return default_csid;
}

Int16 	CBrowserWindow::GetDefaultCSID() const
{
	if(mContext)
		return mContext->GetDefaultCSID();
	return 0;
}
void 	CBrowserWindow::SetDefaultCSID(Int16 default_csid)
{
	if(mContext)
		mContext->SetDefaultCSID(default_csid);
}
#pragma mark -- AppleEvent Support --

// pkc 2/17/97
// This code copied from mcontext.cp
// If the event action is long, dispatches it to the proper handler
// short actions are handled in this loop
void CBrowserWindow::HandleAppleEvent(const AppleEvent		&inAppleEvent,
									  AppleEvent			&outAEReply,
									  AEDesc				&outResult,
									  long					inAENumber)
{
	switch (inAENumber) {	
		case AE_Go:
			{
			StAEDescriptor	directionSpec;
			OSErr err = ::AEGetParamDesc(&inAppleEvent,AE_www_go_direction,typeWildCard,&directionSpec.mDesc);
			ThrowIfOSErr_(err);
			OSType	direction;
			UExtractFromAEDesc::TheEnum(directionSpec.mDesc, direction);
			DoAEGo((Int32)direction);
			}
			break;
		case AE_GetURL:
			HandleGetURLEvent(inAppleEvent, outAEReply, outResult);
			break;
		case AE_OpenURL:
			HandleOpenURLEvent(inAppleEvent, outAEReply, outResult);
			break;
		case AE_RegisterWinClose:
			Try_
			{
				OSType appSignature;
				Size 	realSize;
				OSType	realType;
	
				OSErr err = ::AEGetParamPtr(&inAppleEvent, keyDirectObject, typeApplSignature, &realType,
							&appSignature, sizeof(appSignature), &realSize);
				if (err == noErr)	// No parameters, extract the signature from the Apple Event
					fCloseNotifier = GetPSNBySig(appSignature);
				else
					fCloseNotifier = MoreExtractFromAEDesc::ExtractAESender(inAppleEvent);
				break;
			}
			Catch_(inErr)
			{
				fCloseNotifier = MakeNoProcessPSN();
			}
			EndCatch_
			break;
		case AE_UnregisterWinClose:
			fCloseNotifier = MakeNoProcessPSN();
			break;
		default:
			LWindow::HandleAppleEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
	}
}


void CBrowserWindow::GetAEProperty(DescType inProperty,
							const AEDesc	&inRequestedType,
							AEDesc			&outPropertyDesc) const
{
	switch (inProperty)
	{
		case AE_www_typeWindowURL:			// Current URL
			{
				if (!GetWindowContext())
				{
					ThrowOSErr_(errAEDescNotFound);
				}
				
				History_entry* theCurrentHistoryEntry = GetWindowContext()->GetCurrentHistoryEntry();
				if (!theCurrentHistoryEntry || !theCurrentHistoryEntry->address)
				{
					ThrowOSErr_(errAEDescNotFound);
				}
				
				OSErr err = ::AECreateDesc(typeChar,
											(Ptr) theCurrentHistoryEntry->address,
											strlen(theCurrentHistoryEntry->address),
											&outPropertyDesc);
				ThrowIfOSErr_(err);
			}
			break;
			
		case AE_www_typeWindowBusy:			// is the window busy?
			{
				if ( !GetWindowContext() )
				{
					ThrowOSErr_(errAEDescNotFound);
				}
			
				Uint32 busy = XP_IsContextBusy ( *GetWindowContext() );
				OSErr err = ::AECreateDesc(typeLongInteger,
											&busy,
											sizeof(Uint32),
											&outPropertyDesc);
				ThrowIfOSErr_(err);
				
			}
			break;
			
		case AE_www_typeWindowID:			// get the window ID
			{
				if ( !GetWindowContext() )
				{
					ThrowOSErr_(errAEDescNotFound);
				}
			
				Uint32 id = GetWindowContext()->GetContextUniqueID();
				OSErr err = ::AECreateDesc(typeLongInteger,
											& id,
											sizeof(Uint32),
											&outPropertyDesc);
				ThrowIfOSErr_(err);
				
			}
			break;
		
			
		default:
			super::GetAEProperty(inProperty, inRequestedType, outPropertyDesc);
			break;
	}
}




// Sends an GetURL event
void CBrowserWindow::SendAEGetURL(const char* url)
{
	OSErr anError = noErr;
	try
	{
		AppleEvent	getURLEvent;
		UAppleEventsMgr::MakeAppleEvent(AE_url_suite, AE_url_getURL,
											getURLEvent);
		
			// URL
		anError = ::AEPutParamPtr(&getURLEvent,
									keyDirectObject,
									typeChar,
									url,
									strlen(url));
		ThrowIfOSErr_(anError);
		
			// This window
		StAEDescriptor	modelSpec;
		LWindow::MakeSpecifier(modelSpec.mDesc);
		anError = ::AEPutParamDesc(&getURLEvent,
									AE_www_typeWindow,
									&modelSpec.mDesc);
		ThrowIfOSErr_(anError);
		
	/*
			// Refererer
		if (strlen(refererer) > 0)
		{
			anError = ::AEPutParamPtr(&getURLEvent,
										AE_url_getURLrefererer,
										typeChar,
										refererer,
										strlen(refererer));
			ThrowIfOSErr_(anError);
		}
		
			// Window name
		if (strlen(winName) > 0)
		{
			anError = ::AEPutParamPtr(&getURLEvent,
										AE_url_getURLname,
										typeChar,
										winName,
										strlen(winName));
			ThrowIfOSErr_(anError);
		}
		
			// Optional load-to-disk parameter
		if (loadToDisk)
		{
			anError = ::AEPutParamPtr(&getURLEvent,
										AE_url_getURLdestination,
										typeNull,
										NULL,
										0);
			ThrowIfOSErr_(anError);
		}
	*/
					
		UAppleEventsMgr::SendAppleEvent(getURLEvent);
	}	
	catch(...)
	{
		URL_Struct* theURL = NET_CreateURLStruct(url, NET_DONT_RELOAD);
		mContext->SwitchLoadURL(theURL, FO_CACHE_AND_PRESENT);
	}
}





// Simplify the navigation events to only a single event, Go <direction>
// Still incomplete, since I have not figured out how to deal with history
// in an easy way.
// right now, the possible constants are:
//	kAENext
//	kAEPrevious
//	AE_www_go_again
//	AE_www_super_reload
void CBrowserWindow::SendAEGo( OSType direction )
{
	OSErr		anError = noErr;
	
	try 
	{
		AppleEvent	goEvent;
		UAppleEventsMgr::MakeAppleEvent(AE_www_suite, AE_www_go, goEvent);

		// ¥Êthis window
		StAEDescriptor		modelSpec;
		LWindow::MakeSpecifier(modelSpec.mDesc);
		anError = ::AEPutParamDesc(&goEvent, keyDirectObject, &modelSpec.mDesc);
		ThrowIfOSErr_(anError);

		StAEDescriptor		directionSpec(typeEnumerated, &direction, sizeof(direction));
		anError = ::AEPutParamDesc(&goEvent, AE_www_go_direction, &directionSpec.mDesc);
		ThrowIfOSErr_(anError);

		UAppleEventsMgr::SendAppleEvent(goEvent);
	}	
	catch(...)
	{
		DoAEGo(direction);
	}	
}





// updated to send an event to javascript, also checks for null context - 1997-02-26 mjc
// Handles the navigation events
void CBrowserWindow::DoAEGo( OSType direction )
{
	switch ( direction )
	{
		case kAENext:	// going back in history
			if (mContext != NULL)
			{
				mContext->GoForward();
				CMochaHacks::SendForwardEvent(*mContext);
			}
			break;
		
		case kAEPrevious:
			if (mContext != NULL)
			{
				mContext->GoBack();
				CMochaHacks::SendBackEvent(*mContext);
			}
			break;
		
		case AE_www_go_again:
			if (mContext != NULL)
				mContext->LoadHistoryEntry(CBrowserContext::index_Reload);
			break;
		
		case AE_www_super_reload:
			if (mContext != NULL)
				mContext->LoadHistoryEntry(CBrowserContext::index_Reload, true);
			break;
		
		case AE_www_go_home:
		{
			URL_Struct* homeURL =
				NET_CreateURLStruct(CPrefs::GetString(CPrefs::HomePage), NET_DONT_RELOAD);
			if (mContext != NULL)
				mContext->SwitchLoadURL(homeURL, FO_CACHE_AND_PRESENT);
		}
		break;
		
		default:
			SysBeep( 1 );
		break;
	}
}





void CBrowserWindow::HandleGetURLEvent(const AppleEvent		&inAppleEvent,
									   AppleEvent			&outAEReply,
									   AEDesc				&/*outResult*/,
									   CBrowserWindow		*inBrowserWindow)
{	
	FSSpec fileSpec;
	Boolean hasFileSpec = FALSE;
	Boolean doLoadToDisk = FALSE;
	DescType realType;
	Size actualSize;
	OSErr err;
	char * url = NULL;
	char * refererer = NULL;
	char * winName = NULL;

#ifdef DEBUG
	StValueChanger<EDebugAction> changeDebugThrow(gDebugThrow, debugAction_Nothing);
#endif
	MoreExtractFromAEDesc::GetCString(inAppleEvent, keyDirectObject, url);

	// Extract the referer, if possible
	Try_
	{
		MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_url_getURLrefererer, refererer);
	}
	Catch_(inErr){}
	EndCatch_
	Try_
	{
		MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_url_getURLname, winName);
	}
	Catch_(inErr){}
	EndCatch_
	// See if we are doing load-to-disk.
	// If the descriptor is of typeNull, we should load to disk
	// but we have to file spec
	{
		StAEDescriptor fileDesc;
		err = ::AEGetParamDesc(&inAppleEvent,AE_url_getURLdestination,typeWildCard,&fileDesc.mDesc);
		if (err == noErr)
		{
			if (fileDesc.mDesc.descriptorType == typeNull)
				doLoadToDisk = TRUE;
			else
			{
				err = ::AEGetParamPtr(&inAppleEvent, AE_url_getURLdestination,
							    	typeFSS, &realType,
									&fileSpec, 
									sizeof(FSSpec), &actualSize);
				if (err == errAECoercionFail)
				{
					char * filePath = NULL;
					Try_
					{
						MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_url_getURLdestination, filePath);
						err = CFileMgr::FSSpecFromPathname(filePath, &fileSpec);
						if (err == fnfErr)
							err = noErr;
						ThrowIfOSErr_(err);
					}
					Catch_(inErr)
					{
						err = fnfErr;
					}
					EndCatch_
					if (filePath)
						XP_FREE(filePath);
				}
				hasFileSpec = (err == noErr);
				doLoadToDisk = TRUE;
			}
		}
	}
	
	URL_Struct * request = NET_CreateURLStruct (url, NET_DONT_RELOAD);
	if (winName)
		request->window_target = XP_STRDUP(winName);
	ThrowIfNil_(request);
	request->referer = refererer;

	if (hasFileSpec && doLoadToDisk)
	{
		CURLDispatcher::DispatchToStorage(request, fileSpec, FO_SAVE_AS, true);
	}
	else if (!hasFileSpec && !doLoadToDisk)
	{
		if (inBrowserWindow)
		{
			ThrowIfNil_(inBrowserWindow->mContext);
			
			long windowIndex = inBrowserWindow->mContext->GetContextUniqueID();
			// StartLoadingURL(request, FO_CACHE_AND_PRESENT);
			// mContext->SwitchLoadURL(request, FO_CACHE_AND_PRESENT);
			CURLDispatcher::DispatchURL(request, inBrowserWindow->mContext, true);
			
			if (outAEReply.descriptorType != typeNull)
				err = ::AEPutParamPtr (&outAEReply, keyAEResult, typeLongInteger, &windowIndex, sizeof(windowIndex));
		}
		else
		{
			if (CFrontApp::GetApplication()->HasProperlyStartedUp())
			{
				// Get a window - but don't show it

				CBrowserWindow*	win = CBrowserWindow::MakeNewBrowserWindow(kDontShow);
				ThrowIfNil_(win);

				ThrowIfNil_(win->mContext);
				long windowIndex = win->mContext->GetContextUniqueID();
				
				CURLDispatcher::DispatchURL(request, win->mContext, true);
				
				if (outAEReply.descriptorType != typeNull)
					err = ::AEPutParamPtr (&outAEReply, keyAEResult, typeLongInteger, &windowIndex, sizeof(windowIndex));
			}
			else
			{
				// MGY 5/22: If there are multiple profiles and the event is handled before the user selects a profile,
				// then prefs have not been loaded and we will crash trying to restore the position of the browser window.
				// To fix it right, we will have to delay the positioning of the window until it is shown. The mechanism
				// by which the position of windows is restored is not flexible enough to allow this and to make it so now
				// would be too intrusive of a change at this late date. So we just return 0 for the window ID and the
				// window will be created when the delayed URL is handled.
			  
				CURLDispatcher::DispatchURL(request, nil, true);

				long windowIndex = 0;
				if (outAEReply.descriptorType != typeNull)
					err = ::AEPutParamPtr (&outAEReply, keyAEResult, typeLongInteger, &windowIndex, sizeof(windowIndex));
			}
		}
	}
	XP_FREE (url);
	if (winName)
		XP_FREE(winName);
}

//--------------------------------------------------------------------------------
// Spyglass AppleEvent suite
//--------------------------------------------------------------------------------

void CBrowserWindow::HandleOpenURLEvent(const AppleEvent	&inAppleEvent,
										AppleEvent			&outAEReply,
										AEDesc				&/*outResult*/,
									    CBrowserWindow		*inBrowserWindow)
{
	// The use of "volatile" in  the following lines
	//		(a) generated compiler warnings, because they are passed to routines that
	//			are not declared with correspondingly volatile parameters.
	//		(b) do not appear to need to be volatile in the sense given (If they were
	//			declared as volatile char*, that would be another thing...
	char * /*volatile*/ url = NULL;
	char * /*volatile*/ formData = NULL;	// Do not free this
	char * /*volatile*/ formHeader = NULL;	// form headers (MIME type)
	ProcessSerialNumber psn;
	FSSpec fileSpec;
	Boolean hasFileSpec = FALSE;
	Boolean hasFormData = FALSE;
	Boolean hasPSN = FALSE;
	Boolean forceReload = FALSE;
	OSErr volatile err;
	Size actualSize;
	DescType realType;
	
	// Get the url
	MoreExtractFromAEDesc::GetCString(inAppleEvent, keyDirectObject, url);

	{	// See if we are doing load-to-disk
		err = ::AEGetParamPtr(&inAppleEvent, AE_spy_openURL_into,
					    	typeFSS, &realType,
							&fileSpec, 
							sizeof(fileSpec), &actualSize);
		if (err == noErr)
			hasFileSpec = TRUE;
	}
	// Flags
	Try_
	{
		long flags;
		err = ::AEGetParamPtr(&inAppleEvent, AE_spy_openURL_flag, typeLongInteger,
						 &realType, &flags, sizeof(flags), &actualSize);
		ThrowIfOSErr_(err);
		if ((flags & 0x1) || (flags & 0x2))
			forceReload = TRUE;
	}
	Catch_(inErr){}
	EndCatch_
	// Form data?
	Try_
	{
		MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_spy_openURL_post, formData);
		hasFormData = TRUE;
	}
	Catch_(inErr){}
	EndCatch_
	// If we have form data, get the form MIME type
	if (formData)	
	Try_
	{
		char * newFormHeader = NULL;
		MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_spy_openURL_mime, formHeader);
		StrAllocCat(formHeader, CRLF);
	}
	Catch_(inErr)
	{
		StrAllocCopy(formHeader, "Content-type: application/x-www-form-urlencoded");
		StrAllocCat(formHeader, CRLF);
	}
	EndCatch_
	if (formData && formHeader)
	{
		char buffer[64];
		XP_SPRINTF(buffer, "Content-length: %ld%s", strlen(formData), CRLF);
		StrAllocCat(formHeader, buffer);
	}
	{	// Progress application
		err = ::AEGetParamPtr(&inAppleEvent, AE_spy_openURL_prog,
					    	typeProcessSerialNumber, &realType,
							&psn, 
							sizeof(psn), &actualSize);
		if (err == noErr)
			hasPSN = TRUE;
	}
	
	// create a Netlib geturl thread
	URL_Struct * request = NET_CreateURLStruct (url, NET_DONT_RELOAD);
	ThrowIfNil_(request);
	
	if (forceReload)
		request->force_reload = NET_NORMAL_RELOAD;
	else
		request->force_reload = NET_DONT_RELOAD;
	if (hasFormData)
	{
		request->method = 1;
		request->post_data = formData;
		request->post_data_size = strlen(formData);
		request->post_headers = formHeader;
	}
	
		// service non-interactive request from a different application
	
	/*
		¥¥¥ what do we do with this in dobgert, since SetProgressApp no longer exists?
	if (hasPSN)
		mContext->SetProgressApp(psn);
	*/
	
	if (hasFileSpec)
	{
		CURLDispatcher::DispatchToStorage(request, fileSpec, FO_SAVE_AS, true);
	}
	else
	{
		if (inBrowserWindow)
		{
			ThrowIfNil_(inBrowserWindow->mContext);
			
			long windowID = inBrowserWindow->mContext->GetContextUniqueID();
			/*
				¥¥¥ what is this?  deeje 97-03-07
			if (!fHyperView->CanLoad())
				windowID = 0;
			*/
			if (outAEReply.descriptorType != typeNull)
			{
				StAEDescriptor	windowIDDesc(windowID);
				err = ::AEPutParamDesc(&outAEReply, keyAEResult, &windowIDDesc.mDesc);
			}
			
			CURLDispatcher::DispatchURL(request, inBrowserWindow->mContext, true);
		}
		else
		{
			if (CFrontApp::GetApplication()->HasProperlyStartedUp())
			{
				// Get a window - but don't show it

				CBrowserWindow*	win = CBrowserWindow::MakeNewBrowserWindow(kDontShow);
				ThrowIfNil_(win);

				ThrowIfNil_(win->mContext);
				long windowIndex = win->mContext->GetContextUniqueID();
				
				CURLDispatcher::DispatchURL(request, win->mContext, true);
				
				if (outAEReply.descriptorType != typeNull)
					err = ::AEPutParamPtr (&outAEReply, keyAEResult, typeLongInteger, &windowIndex, sizeof(windowIndex));
			}
			else
			{
				// MGY 5/22: If there are multiple profiles and the event is handled before the user selects a profile,
				// then prefs have not been loaded and we will crash trying to restore the position of the browser window.
				// To fix it right, we will have to delay the positioning of the window until it is shown. The mechanism
				// by which the position of windows is restored is not flexible enough to allow this and to make it so now
				// would be too intrusive of a change at this late date. So we just return 0 for the window ID and the
				// window will be created when the delayed URL is handled.
			  
				CURLDispatcher::DispatchURL(request, nil, true);

				long windowIndex = 0;
				if (outAEReply.descriptorType != typeNull)
					err = ::AEPutParamPtr (&outAEReply, keyAEResult, typeLongInteger, &windowIndex, sizeof(windowIndex));
			}
		}
	}

	if (url)
		XP_FREE (url);
}

// override to send javascript move events when the user performs the action
// because ClickInDrag sends an AppleEvent which doesn't come back to the app.
void	
CBrowserWindow::SendAESetPosition(Point inPosition, Boolean inExecuteAE)
{
	CNetscapeWindow::SendAESetPosition(inPosition, inExecuteAE);
	if (mContext != NULL)
	{
		Rect bounds;
		bounds = UWindows::GetWindowStructureRect(GetMacPort());
		CMochaHacks::SendMoveEvent(*mContext, bounds.left, bounds.top - ::GetMBarHeight());
	}
}

void CBrowserWindow::DoDefaultPrefs()
{
	CPrefsDialog::EditPrefs(CPrefsDialog::eExpandBrowser);
}



#if 0
History_entry* CBrowserWindow::GetBookmarksEntry()
{
	return mContext->GetCurrentHistoryEntry();
}
#endif // 0


#pragma mark -- Helper functions for ObeyCommand --

// ---------------------------------------------------------------------------
//		¥ HandleNetSearchCommand
// ---------------------------------------------------------------------------

void
CBrowserWindow::HandleNetSearchCommand()
{
	CAutoPtrXP<char>	url;
	char*				tempURL = 0;
	int					rc;

	rc = PREF_CopyConfigString("internal_url.net_search.url", &tempURL);
	url.reset(tempURL);
	
	if (rc == PREF_NOERROR)
	{
		CFrontApp& theApp = dynamic_cast<CFrontApp&>(CApplicationEventAttachment::GetApplication());
		
		theApp.DoGetURL(url.get());
	}
}
