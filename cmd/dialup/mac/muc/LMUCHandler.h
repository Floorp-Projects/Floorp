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
#include <LAttachment.h>
#include "LWinIniFile.h"
#include "MUC.h"
#include "LPPPPopup.h"

typedef OSErr (*TraversePPPListFunc)( Str255** list );

class LMUCHandler: public LListener
{
public:
						LMUCHandler();
	virtual				~LMUCHandler();
	
	ExceptionCode		EditProfile( FSSpec* profileName );
	ExceptionCode		SelectProfile( FSSpec* profileName, Boolean autoSelect );
	ExceptionCode		GetProfile( const FSSpec* profileName, FreePPPInfo* buffer );
	ExceptionCode		SetProfile( const FSSpec* profileName, const FreePPPInfo* buffer );
	ExceptionCode		ProfileSelectionChanged( const FSSpec* profileSpec );
	
	void				RegisterClasses();

	void				InitDialog( LDialogBox* dialog );
		
	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
protected:
	enum				{ kMUCStrings = 901 };
	enum				{ kConfigurationFileName = 1,
							kAccountString,
							kModemString };
							
	ExceptionCode		ConnectionExists();	
	ExceptionCode		FindName( TraversePPPListFunc p, const LStr255& value );

	void				ReadConfiguration();
	ExceptionCode		WriteConfiguration();

	ExceptionCode		HandleDialog(	TraversePPPListFunc inFunc,
										ResIDT inDlogID,
										LStr255& outListItemPicked );

	ExceptionCode		GetEntry( LStr255& outValue, const LStr255& inSectionName,
							const LStr255& inKeyName );
	ExceptionCode		SetupEntry( LStr255& inOutEntry,
							TraversePPPListFunc inFunc,
							ResIDT inDlogID );
	
	void				GetDefaultConfiguration();
	void				ClearConfiguration();
	void				SetCurrentConfiguration();
	void				UpdateConfiguration();
	
	void				GetDialConfigurationFile( const FSSpec& inPrefsFolder, FSSpec& configFile );
	void				PrettyPrint( short resNum, const LStr255& string, LStr255& buffer );
	
	FreePPPInfo				mConfiguration;
	Boolean				mDirty;
	LWinIniFile*		mIniFile;
	
	LStdCheckBox*		mAutoConfigCheck;
	LCaption*			mConfigSettings;
	LControl*			mAdvancedButton;
	LView*				mConfigHelp;
	LPPPPopup*			mLocationPopup;
	LCaption*			mAccountName;
	LCaption*			mModemName;
	Int32				mSizeToGrow;
	
	Boolean				mShowingHelp;
};
