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

#include <PP_Types.h>

#pragma once

#include "CPrefsDialog.h"
#include "prefwutil.h"
#include "uprefd.h"
#include "UMailFolderMenus.h"
#include "CValidEditField.h"

#include <LTextColumn.h>

class LWindow;
class LView;
class MPreferenceBase;

//======================================
#pragma mark
class CPrefsMediator
// The base class of all the panes that go inside the preferences window. This has to be
// an LCommander so that it can handle receiving messages from a dialog put up in front of
// it. 

// NOTE: Since it is an LCommander, PowerPlant will clean it up automatically
// when the prefs dialog goes away (since it is a subcommander of the dialog). Do NOT
// explicitly delete any class derived from this base class.

// This is not an abstract class.  In fact, it is the mediator you get if you don't register
// your own.  It relies on the controls all being smart and doing their own thing, i.e., all
// controls in the pane are ones defined in, or inheriting from, those in PrefControls.cp.

// The only reasons to define your own mediator would be (1) custom controls that are not
// prefapi-aware, or (2) a need to co-ordinate different controls.  e.g., when this checkbox is
// on, I must enable these other controls, or (3) you have a button that does something special,
// such as display an extra dialog.

// Concerning reason (2), you don't even need code for this, since the CSlaveEnabler attachment
// can be added in constructor and will handle dimming/undimming in preference to a "master"
// checkbox or radio button.
//======================================
	:	public LListener
	,	public LCommander
{
	public:

	enum
	{
		eCommitPrefs = 12100,
		eCancelPrefs = 12200
	};
		enum			{ refNum_Undefined	= -1 };
						CPrefsMediator(PaneIDT inMainPaneID);
		virtual			~CPrefsMediator();

		static	void	RegisterViewClasses();
		static	Boolean	CanSwitch(LCommander *inNewTarget = nil);
		static	void	SelectFirstEnabledEditField();

		virtual	void	StepAside();
		static	void	UseIC(Boolean useIC) {sUseIC = useIC;};
		static	Boolean	UseIC() {return sUseIC;};

		virtual	void	LoadPanes();
		virtual	void	StepForward(LCommander *defaultSub);
		PaneIDT			GetMainPaneID() const { return mMainPaneID; }
		virtual	void	LoadMainPane();
		virtual	void	LoadPrefs();
		virtual	void	WritePrefs();
		virtual void	Canceled();
		virtual	void	UpdateFromIC() {};
		virtual void	ListenToMessage(
							MessageT		inMessage,
							void			*ioParam);

		static	void	SetStatics(	LView *panel,
									LWindow *window,
									CPrefsDialog::Selection_T selection)
							{
								sPanel = panel;
								sWindow = window;
								sInitialSelection = selection;
							}

		LWindow*		GetWindow() const { return (LWindow*)mMainPane->GetMacPort(); }
		virtual void	ActivateHelp();
		LPane*			FindPaneByID(PaneIDT inPaneID) const
							{ return mMainPane->FindPaneByID(inPaneID); }
		
	protected:
			// setting controls with prefs

		void	SetEditFieldsWithICPref(ConstStr255Param prefName,
										PaneIDT editFieldID,
										Boolean	stripPortNumber = false,
										PaneIDT	portFieldID = 0,
										Boolean	portLocked = false,
										long	defaultPort = 0);
										// If stripPortNumber is false then portFieldID and
										// portLocked are ignored.
										// If stripPortNumber is true then the first
										// colon and any chars following it are stripped.
										// If portFieldID is non-zero and portLocked is
										// false, then the port value is set.
		int		SetFontSizePopupWithIntPref(const char *prefName,
											PaneIDT popupID,
											Boolean &locked,
											int defaultValue = 12);
		void	SetFolderPopupWithPref(	const char *prefName,
										PaneIDT popupID,
										Boolean &locked,
										const char *defaultValue = "");

//		int SetEditFieldWithAliasPref(	const char *prefName,
//										PaneIDT editFieldID,
//										Alias &alias,
//										Boolean &locked);

			// setting controls with SFGetFile

		Boolean	SetEditFieldWithLocalFileURL(PaneIDT editFieldID, Boolean locked = false);
				// returns true iff the user selects a file (as opposed to canceling)

			// setting prefs with controls
		void	SetIntPrefWithFontSizePopup(
										const char *prefName,
										PaneIDT popupID,
										Boolean locked = false);
		void	SetPrefWithFolderPopup(
										const char *prefName,
										PaneIDT popupID,
										Boolean locked = false);

		MPreferenceBase*				GetPreferenceObject(PaneIDT inPaneID) const;
		Boolean	PaneHasLockedPref(PaneIDT inPaneID) const;
		void	ReadDefaultPref(PaneIDT inPaneID);
		void	ChangePrefName(PaneIDT inPaneID, const char* inNewName);
		void	SetValidationFunction(
										PaneIDT inPaneID,
										CValidEditField::ValidationFunc inFunc) const;


		static	LWindow	*					sWindow;
		static	LView *						sPanel;
		static	CPrefsDialog::Selection_T	sInitialSelection;

		Boolean								mNeedsPrefs;

		LView*								mMainPane;
		PaneIDT								mMainPaneID;
		LCommander*							mLatentSub;
	private:

		static			Boolean				sUseIC;
}; // class CPrefsMediator

//======================================
#pragma mark
class CDragOrderTextList : public LTextColumn
//======================================
{
	public:
		enum
		{
			class_ID = 'DOTL',
			eHandClosedCursorID = 29202,
			msg_ChangedOrder = 'ROrd'
		};

							CDragOrderTextList(LStream *inStream) :
											  LTextColumn(inStream),
											  mOrderLocked(false)
											  {}
		virtual				~CDragOrderTextList() {}
		virtual void		ClickCell(const STableCell &inCell, const SMouseDownEvent &inMouseDown);
				void		LockOrder(Boolean lock = true) {mOrderLocked = lock;}
				Boolean		IsLocked() {return mOrderLocked;}
	protected:
		virtual void		MoveRow(TableIndexT inCurrRow, TableIndexT inNewRow);
		Boolean	mOrderLocked;
}; // class CDragOrderTextList

enum
{
	eSelectionChanged = 'selc',
	eDoubleClick = '2clk'
};
