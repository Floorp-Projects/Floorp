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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


// CAddressPickerWindow.h

#pragma once

#include "abcom.h"
#ifdef MOZ_NEWADDR
#include "LGADialogBox.h"
#include "CSaveWindowStatus.h"
#include "CComposeAddressTableView.h"
class CAddressBookTableView;
class CAddressBookController;
class CMailNewsContext;
static const UInt16 cAddressPickerWindowStatusVersion = 	0x006;
//------------------------------------------------------------------------------
//	¥	CAddressPickerWindow
//------------------------------------------------------------------------------
//
class CAddressPickerWindow : public LGADialogBox,  public CSaveWindowStatus
{
public:
	static void		DoPickerDialog(  CComposeAddressTableView* initialTable );
private:
	typedef 	LGADialogBox Inherited;
public:
	enum { class_ID = 'ApWn', pane_ID = class_ID, res_ID = 8990 };
	
	// Control IDs
	enum EButton { 		
		eOkayButton = 'OkBt',
		eCancelButton ='CnBt',
	 	eHelp ='help',
		eToButton = 'ToBt',
	 	eCcButton = 'CcBt',
	 	eBccButton = 'BcBt',
	 	ePropertiesButton = 'PrBt'
		} ;
	
						CAddressPickerWindow(LStream *inStream);
						~CAddressPickerWindow();
	void				Setup( CComposeAddressTableView* initialTable );
	
	virtual ResIDT		GetStatusResID() const { return res_ID; }
protected:
	virtual void		FinishCreateSelf();
	virtual	void 		ListenToMessage(MessageT inMessage, void *ioParam);
	void 				AddSelection( EAddressType inAddressType );
	
	virtual void		ReadWindowStatus(LStream *inStatusData);
	virtual void		WriteWindowStatus(LStream *outStatusData);
	virtual UInt16		GetValidStatusVersion() const { return cAddressPickerWindowStatusVersion; }

private:
	
	CComposeAddressTableView* mInitialTable;
	CComposeAddressTableView* mPickerAddresses;
	
	CAddressBookController*	mAddressBookController;
	CMailNewsContext*		mMailNewsContext;
	CProgressListener*		mProgressListener;
	EButton					mLastAction;			
};
#endif