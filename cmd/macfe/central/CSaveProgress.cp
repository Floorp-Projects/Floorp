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

#include "CSaveProgress.h"
#include "fe_proto.h"
#include "CBrowserContext.h"
#include "resgui.h"	// needed for EDITDLG_SAVE_PROGRESS
#include "uapp.h"
#include "edt.h"
#include "PascalString.h"	// CStr255
#include "proto.h"			// XP_InterruptContext
#include "macutil.h"		// TrySetCursor


#pragma mark CSaveProgress
void CSaveProgress::FinishCreateSelf()
{
	fFilenameText = (LCaption*)this->FindPaneByID( 'flnm' );
	LDialogBox::FinishCreateSelf();
}


void CSaveProgress::SetFilename(char *pFileName)
{
	if ( fFilenameText && pFileName )
		fFilenameText->SetDescriptor( CStr255(pFileName) );
}


void CSaveProgress::ListenToMessage( MessageT inMessage, void* ioParam )
{
	switch ( inMessage )
	{
		case msg_Cancel:
			if ( fContext )
			{
				TrySetCursor( watchCursor );
			
#ifdef EDITOR
				if ( EDT_IS_EDITOR( fContext ) )
					EDT_SaveCancel( fContext );
				else
#endif // EDITOR
					XP_InterruptContext( fContext );
				
				SetCursor( &qd.arrow );
			}
			break;
		
		default:
			LDialogBox::ListenToMessage( inMessage, ioParam );
		break;
	}
}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
#pragma mark --- FTP Upload Dialog ---
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// FIX ME -- find way to use new CDownloadProgressWindow
// Most likely, we'll duplicate the code from downloading


void FE_SaveDialogCreate( MWContext *pContext, int /*iFileCount*/, ED_SaveDialogType /*saveType*/ )
{
	try {
		CSaveProgress * newWindow = (CSaveProgress*)LWindow::CreateWindow( 
									EDITDLG_SAVE_PROGRESS, CFrontApp::GetApplication());
		if ( newWindow == NULL )
			return;
		
		UReanimator::LinkListenerToControls( newWindow, newWindow, EDITDLG_SAVE_PROGRESS );
		
		newWindow->SetContext( pContext );
		ExtractBrowserContext(pContext)->SetSaveDialog( newWindow );

		newWindow->Show();
	}
	
	catch (...)
	{
		ExtractBrowserContext(pContext)->SetSaveDialog( NULL );
	}
}

#ifdef EDITOR
void FE_SaveDialogSetFilename( MWContext *pContext, char *pFilename )
{
	char *better = FE_URLToLocalName( pFilename );
	if ( better )
	{
		if ( pContext && ExtractBrowserContext(pContext) && ExtractBrowserContext(pContext)->GetSaveDialog() )
			ExtractBrowserContext(pContext)->GetSaveDialog()->SetFilename( better );	
		
		XP_FREE( better );
	}
}
#endif // EDITOR

void FE_SaveDialogDestroy( MWContext *pContext, int /*status*/, char */*pFilename*/ )
{
	if ( pContext && ExtractBrowserContext(pContext) && ExtractBrowserContext(pContext)->GetSaveDialog() ) 
	{
		ExtractBrowserContext(pContext)->GetSaveDialog()->ListenToMessage( cmd_Close, NULL );
		ExtractBrowserContext(pContext)->SetSaveDialog( NULL );
	}
}

