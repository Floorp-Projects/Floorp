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

// ===========================================================================
// findw.cp
// Find dialog box
// Contains all the views/commands for the find dialog box
// Created by atotic, Aug 6th, 1994
// ===========================================================================

#include "findw.h"
#include "uapp.h"
#include "resgui.h"
#include "macutil.h"
//#include "VEditField.h"
#include "CTSMEditField.h"
#include <LStdControl.h>
#include <URegistrar.h>

#include "csid.h"
#include "libi18n.h"

#ifdef EDITOR
#include "edt.h"	// composer find/replace dialog
#include "CBrowserContext.h"
#endif




	// globals
CFindWindow* 		CFindWindow::sFindWindow			= NULL;	 
CHTMLView*			CFindWindow::sViewBeingSearched		= NULL;		// Who is being searched?
cstring				CFindWindow::sLastSearch;						// Last search string
Boolean				CFindWindow::sCaseless				= TRUE;		// Search is caseless?
Boolean				CFindWindow::sBackward				= FALSE;	// Search backward
Boolean				CFindWindow::sWrap					= FALSE;	// Wrap ?

Boolean				LMailNewsFindWindow::sFindInHeaders = FALSE;	// search message headers?
#ifdef EDITOR
cstring				CComposeFindWindow::sLastReplace;				// Last replacement string
#endif

CFindWindow::CFindWindow(LStream* inStream)
	: LDialogBox(inStream)
{
	sFindWindow = this;
}

CFindWindow::~CFindWindow()
{
	StoreDefaultWindowState(this);
	sFindWindow = NULL;
}

void CFindWindow::FinishCreateSelf()
{
	LDialogBox::FinishCreateSelf();

	RestoreDefaultWindowState(this);

	fSearchFor 			= (CTSMEditField*)sFindWindow->FindPaneByID('what');
	fCaseSensitive 		= (LStdCheckBox*)sFindWindow->FindPaneByID('case');
	fSearchBackwards 	= (LStdCheckBox*)sFindWindow->FindPaneByID('back');
	fWrapSearch 		= (LStdCheckBox*)sFindWindow->FindPaneByID('wrap');
}

	// Interface to the application
void CFindWindow::RegisterViewTypes()
{
	RegisterClass_(CFindWindow);
	#ifdef MOZ_MAIL_NEWS
	RegisterClass_(LMailNewsFindWindow);
	RegisterClass_(CComposeFindWindow);
	#endif
}

void CFindWindow::SetDialogValues()
{
	CStr255 searchString(sLastSearch);
	fSearchFor->SetDescriptor(searchString);
	fSearchFor->SelectAll();
	sFindWindow->SetLatentSub(fSearchFor);
	
	fCaseSensitive->SetValue(!sCaseless);
	fSearchBackwards->SetValue(sBackward);
	
	if (fWrapSearch != NULL)
		fWrapSearch->SetValue(sWrap);
}

void CFindWindow::SetTextTraitsID(ResIDT inTextTraitsID)
{
	fSearchFor->SetTextTraitsID(inTextTraitsID);
}

void CFindWindow::DoFind(CHTMLView* theHTMLView)
{
	sViewBeingSearched = theHTMLView;
	
	if (!sFindWindow)
	{
		theHTMLView->CreateFindWindow();
		ThrowIfNil_(sFindWindow);
	}
	
	if	(theHTMLView->GetWinCSID() != INTL_CharSetNameToID(INTL_ResourceCharSet()))
	{
		sFindWindow->SetTextTraitsID( CPrefs::GetTextFieldTextResIDs(theHTMLView->GetWinCSID()));
	}
	
	sFindWindow->SetDialogValues();
	sFindWindow->Show();
	sFindWindow->Select();
}

Boolean CFindWindow::CanFindAgain()
{
	return (sLastSearch.length() > 0);
}

void CFindWindow::GetDialogValues()
{
	CStr255 lookFor;
	fSearchFor->GetDescriptor(lookFor);
	sLastSearch = (char*)lookFor;
	sCaseless = (fCaseSensitive->GetValue() == 0);
	sBackward = fSearchBackwards->GetValue();
	
	sWrap = (fWrapSearch != NULL && fWrapSearch->GetValue());
}

void CFindWindow::ListenToMessage(MessageT inMessage, void* ioParam)
{
	switch( inMessage )
	{
		case msg_OK:
		{
			this->GetDialogValues();
			sViewBeingSearched->DoFind();
			ListenToMessage( cmd_Close, NULL );
			break;
		}
		
		
		case msg_Cancel:
		{
			LDialogBox::ListenToMessage( cmd_Close, ioParam );
			break;
		}
		
		default:
		{
			LDialogBox::ListenToMessage(inMessage, ioParam);
			break;
		}
	}
}


/* SavePlace, RestorePlace and DoSetBounds all override the window and subpane
   positioning behaviour used in FinishCreateSelf and the destructor, so that
   only the window position is saved and restored.  This is important to
   the internationalization folks, who want the PPob to control pane positions,
   rather than the preferences file. */
void CFindWindow::SavePlace (LStream */*outPlace*/)
{
	// don't!
}

void CFindWindow::RestorePlace (LStream */*inPlace*/)
{
	// don't
}

void CFindWindow::DoSetBounds (const Rect &inBounds) // bounds in global coords
{
	// refuse to accept a change in window size: we're a fixed dialog, after all.
	Rect	newBounds = UWindows::GetWindowContentRect(GetMacPort());

	newBounds.right += inBounds.left - newBounds.left;
	newBounds.left = inBounds.left;
	newBounds.bottom += inBounds.top - newBounds.top;
	newBounds.top = inBounds.top;

	super::DoSetBounds (newBounds);
}


void CFindWindow::FindCommandStatus(CommandT	/*inCommand*/,
									Boolean	& 	outEnabled, 
									Boolean & 	/*outUsesMark*/, 
									Char16  & 	/*outMark*/,
									Str255 		/*outName*/ )
{
	// we're a modal dialog
	outEnabled = false;
}
			
#ifdef MOZ_MAIL_NEWS

LMailNewsFindWindow::LMailNewsFindWindow( LStream* inStream )
:	CFindWindow( inStream )
{
}

void LMailNewsFindWindow::FinishCreateSelf()
{
	CFindWindow::FinishCreateSelf();
	fFindInHeaders = (LStdRadioButton*)this->FindPaneByID( 'Rhea' );
}

void LMailNewsFindWindow::GetDialogValues()
{
	CFindWindow::GetDialogValues();
	sFindInHeaders = ( fFindInHeaders->GetValue() != 0 );
}

# ifdef EDITOR
# pragma mark -

CComposeFindWindow::CComposeFindWindow( LStream *inStream ) : CFindWindow( inStream )
{
}


void CComposeFindWindow::FinishCreateSelf()
{
	CFindWindow::FinishCreateSelf();
	
	UReanimator::LinkListenerToControls( this, this, mPaneID );
	
	fReplaceWith = (CTSMEditField *)sFindWindow->FindPaneByID( 'newT' );
	XP_ASSERT( fReplaceWith != NULL );
}


void CComposeFindWindow::SetDialogValues()
{
	CFindWindow::SetDialogValues();
	
	CStr255 replaceString( sLastReplace );
	fReplaceWith->SetDescriptor( replaceString );
	fReplaceWith->SelectAll();
	sFindWindow->SetLatentSub( fReplaceWith );
}


void CComposeFindWindow::GetDialogValues()
{
	CFindWindow::GetDialogValues();
	
	CStr255 replaceString;
	fReplaceWith->GetDescriptor( replaceString );
	sLastReplace = (char*)replaceString;
}


void CComposeFindWindow::ListenToMessage(MessageT inMessage, void* ioParam)
{
	switch( inMessage )
	{
		case msg_OK:	/* find */
		{
			this->GetDialogValues();
			sViewBeingSearched->DoFind();
//			ListenToMessage( cmd_Close, NULL );
			break;
		}
		
		case 'Repl':
		case 'RepA':
		case 'RepN':
		{
			Str255 str;
			fReplaceWith->GetDescriptor( str );
			p2cstr( str );
			
			EDT_ReplaceText( (sViewBeingSearched->GetContext())->operator MWContext*(), (char *)str, 
							inMessage == 'RepA', sLastSearch, sCaseless, sBackward, sWrap );
			
			/* do we need to find the next one? */
			if ( inMessage == 'RepN' )
			{
				sViewBeingSearched->DoFind();
			}
			break;
		}
		
		case msg_Cancel:
		{
			LDialogBox::ListenToMessage( cmd_Close, ioParam );
			break;
		}
		
		default:
		{
			LDialogBox::ListenToMessage(inMessage, ioParam);
			break;
		}
	}
}


# endif // EDITOR
#endif // MOZ_MAIL_NEWS
