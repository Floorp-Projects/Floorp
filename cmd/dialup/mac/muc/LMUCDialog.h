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

#pragma once
#include <LDialogBox.h>
#include <LListBox.h>

#include <LGADialogBox.h>
#include "MUC.h"
#include "LPPPPopup.h"

#define	msg_SingleClick		424

class LSingleClickListBox: public LListBox
{
public:
	enum { class_ID = 'Lsng' };
	
	static void*	CreateSingleClickListBox( LStream* inStream );
					LSingleClickListBox( LStream* inStream );

	void			ClickSelf( const SMouseDownEvent& inMouseDown );
	Boolean			HandleKeyPress( const EventRecord& inKeyEvent );
	
	void			SetSingleClickMessage( MessageT inMessage );
protected:
	MessageT		mSingleClickMessage;
};

class LPPPListBox: public LSingleClickListBox
{
public:
	enum { class_ID = 'LPPP' };
	
	static void*	CreatePPPListBox( LStream* inStream );
					LPPPListBox( LStream* inStream );
				
	void			SetPPPFunction( TraversePPPListFunc p );
	void			UpdateList();
	
	void			SetToNamedItem( LStr255& name );

	virtual Boolean	FocusDraw( LPane* );
	virtual void	DrawSelf();
	virtual void	EnableSelf();
	virtual void	DisableSelf();
	
protected:	
	TraversePPPListFunc		mFunction;
};

class LMUCDialog: public LGADialogBox
{
public:
	enum { class_ID = 'Mccd' };

	static void*	CreateMUCDialogStream( LStream* inStream );
					LMUCDialog( LStream* inStream );

	void			SetPPPFunction( TraversePPPListFunc p );

	void			GetNewValue( LStr255& name );
	void			UpdateList();
	void			UpdateButtonState();

	virtual void	ListenToMessage( MessageT inMessage, void* ioParam );
	
protected:
	virtual void	FinishCreateSelf();
	LPPPListBox*	mListBox;
};

class LMUCEditDialog: public LGADialogBox
{
public:
	enum { class_ID = 'Mcce' };
	
	static void*	CreateMUCEditDialogStream( LStream* inStream );
					LMUCEditDialog( LStream* inStream );
					
	void			GetNewValues(	LStr255& outModemName,
									LStr255& outAccountName,
									LStr255& outLocationName );
									
	void			SetInitialValue( const LStr255& modem,
									const LStr255& account,
									const LStr255& location );
									
	void			UpdateLists();
	void			UpdateButtonState();
	
	virtual void	ListenToMessage( MessageT inMessage, void* ioParam );

protected:
	virtual void	FinishCreateSelf();
	
	LControl*		mDisabled;
	LPPPPopup*		mModemsList;
	LPPPPopup*		mLocationsList;
	LPPPPopup*		mAccountsList;
};
