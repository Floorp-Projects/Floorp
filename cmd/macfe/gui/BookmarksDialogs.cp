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

#include "BookmarksDialogs.h"
#include "PascalString.h"
#include "macutil.h"
#include "uapp.h"

CBookmarksFindDialog *CBookmarksFindDialog::sFindDialog = NULL;


//===========================================================
// CBookmarksFindDialog
//===========================================================
CBookmarksFindDialog::CBookmarksFindDialog( LStream* inStream ): LDialogBox( inStream )
{
	sFindDialog = this;
}

CBookmarksFindDialog::~CBookmarksFindDialog()
{
	sFindDialog = NULL;
}

void CBookmarksFindDialog::FinishCreateSelf()
{
	LDialogBox::FinishCreateSelf();
	fCheckName = (LStdCheckBox*)this->FindPaneByID( 'Cnam' );
	fCheckLocation = (LStdCheckBox*)this->FindPaneByID( 'Cloc' );
	fCheckDescription = (LStdCheckBox*)this->FindPaneByID( 'Cdes' );
	fMatchCase = (LStdCheckBox*)this->FindPaneByID( 'Ccas' );
	fWholeWord = (LStdCheckBox*)this->FindPaneByID( 'Cwor' );
	
	fFindText = (LEditField*)this->FindPaneByID( 'Efnd' );
}


void CBookmarksFindDialog::ListenToMessage( MessageT inMessage, void* ioParam )
{
	switch ( inMessage )
	{
		case msg_OK:
		{
#if 0
// THIS IS JUST LEFT HERE AS AN EXAMPLE OF HOW TO PULL THE RESULTS OUT OF
// THE CONTROLS. THIS USES THE DEPRICATED BOOKMARKS API AND WILL BE REWRITTEN.
			CStr255		tmp;
			
			fFindText->GetDescriptor( tmp );
			if ( fFindInfo->textToFind )
			{
				XP_FREE( fFindInfo->textToFind );
				fFindInfo->textToFind = NULL;
			}
			NET_SACopy( &fFindInfo->textToFind, tmp );
			fFindInfo->checkName = fCheckName->GetValue();
			fFindInfo->checkLocation = fCheckLocation->GetValue();
			fFindInfo->checkDescription = fCheckDescription->GetValue();
			fFindInfo->matchCase = fMatchCase->GetValue();
			fFindInfo->matchWholeWord = fWholeWord->GetValue();
			BM_DoFindBookmark( fContext, fFindInfo );
#endif
			ListenToMessage( cmd_Close, ioParam );
		}
		break;
		case msg_Cancel:
			ListenToMessage( cmd_Close, ioParam );
		break;
		default:
			LDialogBox::ListenToMessage( inMessage, ioParam );
		break;
	}
}
