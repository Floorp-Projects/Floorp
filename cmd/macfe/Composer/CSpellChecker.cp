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

#include "CSpellChecker.h"

#include "CTextTable.h"
#include "StBlockingDialogHandler.h"
#include "URobustCreateWindow.h"
#include "CSimpleTextView.h"
#include "LGAEditField.h"
#include "LGAPushButton.h"
#include "LGAPopup.h"
#include "CEditDictionary.h"
#include "UMenuUtils.h"

// need to include "structs.h" before "edt.h" because "edt.h" is missing
// some of the includes it needs
#include "structs.h"
#include "edt.h"
#include "proto.h"		// LO_GetSelectionText
#include "prefapi.h"	// PREF_SetIntPref
#include "uerrmgr.h"	// ErrorManager
#include "resgui.h"		// error stringNums
#include "macgui.h"		// StPrepareForDialog
#include "ufilemgr.h"	// CFileMgr

// shared library stuff from NSPR
#include "prlink.h"


/*	The difference between calling the spell checker lib from a DLL or from
	a static linked lib depends on whether we're generating CFM code
	(and on whether the static lib was included in the application project) */
#if GENERATINGCFM
#define USE_DYNAMIC_SC_LIB
#else
#undef USE_DYNAMIC_SC_LIB
#endif

// local prototypes
static void exit_spellchecker( PRLibrary *lib, ISpellChecker *pSpellChecker, CMacSpellChecker *macSpellChecker );


// function typedefs
typedef ISpellChecker*(*sc_create_func)(); 
typedef void (*sc_destroy_func)(ISpellChecker *); 



CMacSpellChecker::CMacSpellChecker( MWContext *context, CEditView *editView, CSimpleTextView *textView )
{
	mMWContext = context;
	mEditView = editView;
	mTextView = textView;
	mISpellChecker = NULL;
}


void CMacSpellChecker::ReplaceHilitedText( char *newText, Boolean doAll )
{
	if ( mEditView )
	{
		char *oldWord = (char *)LO_GetSelectionText( GetMWContext() );
		if ( oldWord == NULL )
			return;
		
		EDT_ReplaceMisspelledWord( GetMWContext(), oldWord, newText, doAll );
		XP_FREE( oldWord );
	}
	else if ( mTextView )
	{
		GetISpellChecker()->ReplaceMisspelledWord( newText, doAll );

		/* assume the misspelled word is selected so that InsertPtr will replace it */
		mTextView->InsertPtr( newText, XP_STRLEN(newText), NULL, NULL, false, true );
	}
}


void CMacSpellChecker::IgnoreHilitedText( Boolean doAll )
{
	if ( mEditView )
	{
		char *oldWord = (char *)LO_GetSelectionText( GetMWContext() );
		if ( oldWord == NULL )
			return;
		
		EDT_IgnoreMisspelledWord( GetMWContext(), oldWord, doAll );
		XP_FREE( oldWord );
	}
	else if ( mTextView && doAll )
	{
		SInt32	selStart 	= 0;
		SInt32	selEnd		= 0;
		SInt32	selLen		= 0;
		
		mTextView->GetSelection( &selStart, &selEnd );
		
		selLen = selEnd - selStart;
		
		if ( selLen > 0 )
		{
			Handle	theText = mTextView->GetTextHandle();
			XP_ASSERT( theText != NULL );
			
			char *textP = (char *)XP_ALLOC( selLen + 1 );
			XP_ASSERT( textP );
			if ( textP )
			{
				::BlockMoveData( ((char *)*theText) + selStart, textP, selLen );
				textP[ selLen ] = 0;
				
				GetISpellChecker()->IgnoreWord( textP );
				XP_FREE( textP );
			}
		}
	}
		// assume we don't need to do anything for ignoring just this occurrence in plain text
}


typedef struct dictionaryInfo
{
	int	langCode;
	int	dialectCode;
	int	menuItemNumber;
} dictionaryInfo;

static void AddDictionaryMenuItem( MenuHandle languagePopup, dictionaryInfo *dictInfoP )
{
	// get string for lang code and dialectcode
	int stringIndex = 4;
	switch ( dictInfoP->langCode )
	{
		case L_ENGLISH:
			switch ( dictInfoP->dialectCode )
			{
				case D_US_ENGLISH:	stringIndex = 5;	break;
				case D_UK_ENGLISH:	stringIndex = 6;	break;
				case D_AUS_ENGLISH:	stringIndex = 7;	break;
				default:			stringIndex = 8;	break;
			}
			break;
		
		case L_CATALAN:	stringIndex = 9;	break;
		case L_HUNGARIAN: stringIndex = 10;	break;
		
		case L_GERMAN:
			switch ( dictInfoP->dialectCode )
			{
				default:			stringIndex = 11;	break;
				case D_SCHARFES:	stringIndex = 12;	break;
				case D_DOPPEL:		stringIndex = 13;	break;
			}
			break;
		
		case L_SWEDISH:	stringIndex = 14;	break;
		case L_SPANISH:	stringIndex = 15;	break;
		case L_ITALIAN:	stringIndex = 16;	break;
		case L_DANISH:	stringIndex = 17;	break;
		case L_DUTCH:	stringIndex = 18;	break;
			
		case L_PORTUGUESE:
			switch ( dictInfoP->dialectCode )
			{
				default:
				case D_EUROPEAN:	stringIndex = 20;	break;
				case D_BRAZILIAN:	stringIndex = 21;	break;
			}
			break;
		
		case L_NORWEGIAN:
			switch ( dictInfoP->dialectCode )
			{
				default:
				case D_BOKMAL:	stringIndex = 22;	break;
				case D_NYNORSK:	stringIndex = 23;	break;
			}
			break;
		
		case L_FINNISH:	stringIndex = 24;	break;
		case L_GREEK:	stringIndex = 25;	break;
		case L_AFRIKAANS: stringIndex = 26;	break;
		case L_POLISH:	stringIndex = 27;	break;
		case L_CZECH:	stringIndex = 28;	break;
		case L_FRENCH:	stringIndex = 29;	break;
		case L_RUSSIAN:	stringIndex = 30;	break;
	}
	
	Str255	str;
	::GetIndString( str, SpellCheckerResource, stringIndex );
	if ( str[ 0 ] == 0 )
		dictInfoP->menuItemNumber = 0;
	else
	{
		UMenuUtils::AppendMenuItem( languagePopup, str, true );
		dictInfoP->menuItemNumber = ::CountMItems( languagePopup );
	}
}


static int FindDefaultMenuItem( int langcode, int dialectcode, dictionaryInfo *dictInfoP, int maxIndex )
{
	int i;
	for (i = 0; i < maxIndex; i++ )
	{
		if ( langcode == dictInfoP[ i ].langCode && dialectcode == dictInfoP[ i ].dialectCode )
			return dictInfoP[ i ].menuItemNumber;
	}
	
	return 1;
}

static int FindDictionaryInfoIndexForMenu( LGAPopup *languagePopup, dictionaryInfo *dictInfoP, int maxIndex )
{
	int menuSelection = languagePopup->GetValue();
	if ( menuSelection == 0 )
		return 0;
	
	int i;
	for (i = 0; i < maxIndex; i++ )
	{
		if ( menuSelection == dictInfoP[ i ].menuItemNumber )
			return i;
	}
	
	return 0;
}

void CMacSpellChecker::ShowDialog( char *textP )
{
	Boolean continueChecking = StartProcessing( false );
	if ( continueChecking )
		continueChecking = GetNextMisspelledWord( true );
	
		Str255	s;
		LGAPushButton *btn;
		StPrepareForDialog prepare;
		StBlockingDialogHandler handler( res_ID, NULL );
		LDialogBox* dialog = (LDialogBox *)handler.GetDialog();
		
		CTextTable *listview = (CTextTable *)dialog->FindPaneByID( pane_SuggestionList );
		if ( listview == NULL )
			return;
		
		PenState penState;
		::GetPenState( &penState );
		listview->AddAttachment( new LColorEraseAttachment( &penState, NULL, NULL, true ) );
		
		listview->FocusDraw();
		listview->AddListener( &handler );
		dialog->FocusDraw();
		
		LGAEditField *editField = (LGAEditField *)dialog->FindPaneByID( pane_NewWord );
		if ( editField == NULL )
			return;
		
		LGAPushButton *changeBtn = (LGAPushButton *)dialog->FindPaneByID( msg_Change );
		if ( changeBtn == NULL )
			return;
		
		LGAPopup *languagePopup = (LGAPopup *)dialog->FindPaneByID( msg_NewLanguage );
		if ( languagePopup == NULL )
			return;
		languagePopup->LoadPopupMenuH();
		
		int curDictionary, CurrLangCode, CurrDialectCode, menuLangCode, menuDialectCode;
		int numDictionaries = GetISpellChecker()->GetNumOfDictionaries();
		
		dictionaryInfo *dictInfoP = (dictionaryInfo *)XP_ALLOC( numDictionaries * sizeof( dictionaryInfo ) );
		if ( dictInfoP == NULL )
			return;
		
		for ( curDictionary = 0; curDictionary < numDictionaries; curDictionary++ )
		{
			if ( GetISpellChecker()->GetDictionaryLanguage( curDictionary, CurrLangCode, CurrDialectCode ) )
				break;
			
			dictInfoP[ curDictionary ].langCode = CurrLangCode;
			dictInfoP[ curDictionary ].dialectCode = CurrDialectCode;
			dictInfoP[ curDictionary ].menuItemNumber = curDictionary + 1;
			AddDictionaryMenuItem( languagePopup->GetMacMenuH(), &dictInfoP[ curDictionary ] );
		}
		
		GetISpellChecker()->GetCurrentLanguage( CurrLangCode, CurrDialectCode );
		
		// the following line resets the menu so its width gets adjusted and max value set
		languagePopup->SetMacMenuH( languagePopup->GetMacMenuH() );
		languagePopup->SetValue( FindDefaultMenuItem( CurrLangCode, CurrDialectCode, dictInfoP, numDictionaries ) );
		
		if ( continueChecking )
			SetNextMisspelledWord( textP, editField, listview, dialog );

		Boolean isEditFieldOriginal, doNextWord = false, doStartOver = false;
		MessageT message;
		do {
			LCommander *target = dialog->GetTarget();
			
			// if we're done checking we won't update buttons anymore
			if ( continueChecking )
			{
				editField->GetDescriptor( s );
				isEditFieldOriginal = ( XP_STRNCMP( (char *)s, 
								(char *)mOrigMisspelledWord, mOrigMisspelledWord[0] + 1 ) == 0 );
				p2cstr( s );
				char *indexp = XP_STRCHR( (char *)s, ' ' );
				Boolean containsNoSpaces = indexp == NULL || indexp[0] == 0;
				// set state of buttons (enable/disable); disable when editField is target and matches origword
				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Change_All );
				if ( target == editField && isEditFieldOriginal )
				{
					// string hasn't changed so disable "replace" and "replace all"
					changeBtn->Disable();
					if ( btn )
						btn->Disable();
				}
				else
				{
					changeBtn->Enable();
					if ( btn )
						btn->Enable();
				}
				
				// we shouldn't be able to "learn" something that is already in the dictionary (in sugg. list)
				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Add_Button );
				if ( btn )
				{
					// can't learn if there's a space in the string!
#ifdef LEARN_BUTTON_WORKS_AS_SPECD
					if ( target == editField && containsNoSpaces )
#else
					if ( containsNoSpaces )
#endif
						btn->Enable();
					else
						btn->Disable();
				}
				
				// we shouldn't be able to "check" unless the editfield is the target
				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Check );
				if ( btn )
				{
					// can't check if multiple words (contains a space)
					if ( target == editField && containsNoSpaces )
						btn->Enable();
					else
						btn->Disable();
				}
				
				if ( changeBtn->GetValueMessage() != msg_Change )
				{
					changeBtn->SetValueMessage( msg_Change );
					::GetIndString( s, SpellCheckerResource, ChangeStringIndex );
					changeBtn->SetDescriptor( s );
				}

				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Ignore );
				if ( btn )
					btn->Enable();
				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Ignore_All );
				if ( btn )
					btn->Enable();
				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Stop );
				if ( btn )
					btn->Enable();
				
				editField->Enable();
//				listview->Enable();
			}
			else // done checking
			{
				doStartOver = false;
				dialog->FocusDraw();
				
				if ( changeBtn->GetValueMessage() != msg_Stop )
				{
					changeBtn->SetValueMessage( msg_Stop );
					::GetIndString( s, SpellCheckerResource, DoneStringIndex );
					changeBtn->SetDescriptor( s );
				}
				changeBtn->Enable();
				
				// disable all of the other buttons
				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Change_All );
				if ( btn )
					btn->Disable();
				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Ignore );
				if ( btn )
					btn->Disable();
				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Ignore_All );
				if ( btn )
					btn->Disable();
				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Add_Button );
				if ( btn )
					btn->Disable();
				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Check );
				if ( btn )
					btn->Disable();
				
				btn = (LGAPushButton *)dialog->FindPaneByID( msg_Stop );
				if ( btn )
					btn->Disable();
				
				editField->Disable();	// don't allow anymore typing
				listview->Disable();
			}
			
			if ( doNextWord )
			{
				doNextWord = false;
				dialog->FocusDraw();
			
				ClearReplacementWord( editField, listview );
				if ( continueChecking )
					continueChecking = GetNextMisspelledWord( doStartOver );
				if ( continueChecking )
					SetNextMisspelledWord( NULL, editField, listview, dialog );
			}
			
			message = handler.DoDialog();
			switch ( message )
			{
				case msg_Add_Button:
					editField->GetDescriptor( s );
					p2cstr( s );
					int statusError = GetISpellChecker()->AddWordToPersonalDictionary( (char *)s );
					if ( statusError )
						SysBeep(0);
					else
						ReplaceHilitedText( (char *)s, true );
					doNextWord = true;
					break;
				
				case msg_Ignore:
				case msg_Ignore_All:
					IgnoreHilitedText( message == msg_Ignore_All );
					doNextWord = true;
					break;
				
				case msg_Change:
				case msg_Change_All:
					if ( target && target == editField )
						editField->GetDescriptor( s );
					else if ( target && target == listview )
					{
						STableCell	c(0, 1);
						c = listview->GetFirstSelectedCell();
						Uint32	len = sizeof( s );
						listview->GetCellData( c, s, len );
					}
					else
						s[ 0 ] = 0;
					
					if ( s[ 0 ] != 0 )
					{
						p2cstr( s );
						ReplaceHilitedText( (char *)s, message == msg_Change_All );
					}
				
					doNextWord = true;
					break;
				
				case msg_SelectionChanged:
					break;
				
				case msg_Check:
					GetAlternativesForWord( editField, listview, dialog );
					break;
				
				case msg_NewLanguage:
					// get current menu choice; convert to langcode/dialectcode
					int index = FindDictionaryInfoIndexForMenu( languagePopup, dictInfoP, numDictionaries );
					menuLangCode = dictInfoP[ index ].langCode;
					menuDialectCode = dictInfoP[ index ].dialectCode;
					if ( CurrLangCode != menuLangCode || menuDialectCode != CurrDialectCode )	// actually changed
					{
						CurrLangCode = menuLangCode;
						CurrDialectCode = menuDialectCode;
				        PREF_SetIntPref( "SpellChecker.DefaultLanguage", menuLangCode );
				        PREF_SetIntPref( "SpellChecker.DefaultDialect", menuDialectCode );
				        
						if ( GetISpellChecker()->SetCurrentLanguage( menuLangCode, menuDialectCode ) == 0 )
						{
							doNextWord = true;	// set flag to get next misspelled word and enable/disable
							doStartOver = true;
							continueChecking = StartProcessing( true );	// continue if any misspelled words
						}
					}
					break;
					
				case msg_EditDictionary:
					CEditDictionary *dictEditor = dynamic_cast<CEditDictionary *>
							(URobustCreateWindow::CreateWindow( CEditDictionary::res_ID, LCommander::GetTopCommander() ));
					if ( dictEditor )
						dictEditor->SetISpellChecker( GetISpellChecker() );
					break;
				}
		} while ( message != msg_Stop );

		if ( mEditView )
			// clear the TF_SPELL mask for any words that might still have it 
			EDT_IgnoreMisspelledWord( mMWContext, NULL, true );
}


char *CMacSpellChecker::GetTextBuffer()
{
	char *retVal;
	
	if ( mEditView )
		retVal = EDT_GetPositionalText( mMWContext );
	else if ( mTextView )
	{
		SInt32 theSize = mTextView->GetTextLength();
		if ( theSize == 0 )
			return NULL;
	
		retVal = (char *)XP_ALLOC( theSize + 1 );
		XP_ASSERT( retVal );
		if ( retVal )
		{
			Handle textH = mTextView->GetTextHandle();
			::BlockMoveData( *textH, retVal, theSize );
			retVal[ theSize ] = 0;
		}
	}
	
	return retVal;
}


void CMacSpellChecker::GetSelection(int32 &selStart, int32 &selEnd)
{
	if (mEditView)
	{
		char *pSelection = (char *)LO_GetSelectionText( GetMWContext() );
		if ( pSelection != NULL )
		{
			XP_FREE( pSelection );
			EDT_GetSelectionOffsets( GetMWContext(), &selStart, &selEnd );
		}
	}
	else
	{
		long	localStart, localEnd;
		
		mTextView->GetSelection( &localStart, &localEnd );
		// bug fix
		// only return the selection if it is not empty.
		if (localStart != localEnd)
		{
			selStart = localStart;
			selEnd = localEnd;
		}
	}
}


Boolean CMacSpellChecker::StartProcessing( Boolean startOver )
{
	char *stringToCheck;
	char *misspelledWord = NULL;
	
	stringToCheck = GetTextBuffer();
	Boolean retVal = stringToCheck != NULL;
	
	if ( stringToCheck )
	{
		if ( mEditView )
			EDT_SetRefresh( mMWContext, false );

		if ( startOver )
			GetISpellChecker()->SetNewBuf( stringToCheck, true );
		else
		{
			// setting buffer the first time
			// let the spellchecker know the current selection
			int32 selStart = 0, selEnd = 0;
			GetSelection( selStart, selEnd );
			GetISpellChecker()->SetBuf( stringToCheck, selStart, selEnd );
		}
		
		XP_HUGE_FREE( stringToCheck );

		if ( mEditView )
		{
			// clear the TF_SPELL mask for any words that might still have it 
			EDT_IgnoreMisspelledWord( mMWContext, NULL, true );
			
			// mark misspelled words
			EDT_CharacterData *pData = EDT_NewCharacterData();
			if ( pData )
			{
				pData->mask = TF_SPELL;
				pData->values = TF_SPELL;
				
				unsigned long StartPos = 0, len = 0;
				while ( GetISpellChecker()->GetNextMisspelledWord( StartPos, len ) == 0 )
					EDT_SetCharacterDataAtOffset( GetMWContext(), pData, StartPos, len );
				
				EDT_FreeCharacterData( pData );
			}

			// set cursor position at the beginning so that
			// EDT_SelectNextMisspelledWord will start at beginning
			EDT_BeginOfDocument( mMWContext, false );
			EDT_SetRefresh( mMWContext, true );
		}
	}
	
	return retVal;
}


Boolean CMacSpellChecker::GetNextMisspelledWord( Boolean startOver )
{
	if ( isHTMLeditor() )
	{
		if ( startOver )
			return EDT_SelectFirstMisspelledWord( GetMWContext() );
		else
			return EDT_SelectNextMisspelledWord( GetMWContext() );
	}
	else
	{
		unsigned long StartPos = 0, Len = 0;
		if ( GetISpellChecker()->GetNextMisspelledWord( StartPos, Len ) == 0 )
		{
			mTextView->FocusDraw();
			mTextView->SetSelection( StartPos, StartPos + Len );
			
			return true;
		}
		else	// no more misspelled words
			return false;
	}
	
	return false;
}


void CMacSpellChecker::SetNextMisspelledWord( char *textP, LEditField *typoField, 
											CTextTable *table, LCommander *commander )
{
	Boolean	doFree = false;

	if ( textP == NULL )
	{
		if ( isHTMLeditor() )
		{
			textP = (char *)LO_GetSelectionText( GetMWContext() );
			doFree = ( textP != NULL );
		}
		else
		{
			SInt32	selStart = 0;
			SInt32	selEnd   = 0;
			SInt32 	selLen	 = 0;
			
			mTextView->FocusDraw();
			mTextView->GetSelection( &selStart, &selEnd );
			selLen = selEnd - selStart;
			
			if ( selLen > 0 )
			{
				textP = (char *)XP_ALLOC( selLen + 1 );
				XP_ASSERT( textP != NULL );
				
				doFree = ( textP != NULL );
				
				if ( textP )
				{
					Handle textH = mTextView->GetTextHandle();
					::BlockMoveData( (*textH) + selStart, textP, selLen );
					textP[ selLen ] = 0;
				}
			}
		}
	}
	
	// convert c-string to pascal-string
	CStr255 str( textP );
	if ( typoField )
	{
		typoField->FocusDraw();
		typoField->SetDescriptor( str );
		typoField->SelectAll();
		// re-set misspelledword member
		typoField->GetDescriptor( mOrigMisspelledWord );
	}

	if ( table )
	{
		table->FocusDraw();
	
		STableCell	cell(0, 1);
		int numRows = GetISpellChecker()->GetNumAlternatives( textP );
		if ( numRows > 0 )
		{
			table->Enable();
			table->InsertRows( numRows, 0, nil, 0, Refresh_Yes );
		
			int i, len;
			for ( i = 1; i <= numRows; i++ )
			{
				char liststr[256];
				if ( GetISpellChecker()->GetAlternative( i - 1, liststr, 255 ) == 0 )
				{
					len = XP_STRLEN( (char *)liststr );
					if ( len > 0 )
					{
						cell.row = i;
						CStr255 pascalstring( liststr );
						table->SetCellData( cell, pascalstring, len + 1 );
					}
				}
			}

			cell.row = 1;
			table->SelectCell( cell );
			if ( commander )
				commander->SwitchTarget( table );
		}
		else
		{
			table->InsertRows( 1, 0, nil, 0, Refresh_Yes );
			
			Str255 noneFoundString;
			noneFoundString[0] = 0;
			::GetIndString( noneFoundString, SpellCheckerResource, 31 );
			cell.row = 1;
			table->SetCellData( cell, noneFoundString, noneFoundString[0] + 1 );
			
			table->Disable();
			
			if ( commander && typoField )
				commander->SwitchTarget( typoField );
		}
	}
	
	if ( doFree )
		XP_FREE( textP );
}


void CMacSpellChecker::ClearReplacementWord( LEditField *newWord, CTextTable *table )
{
	if ( newWord )
		newWord->SetDescriptor( CStr255::sEmptyString );
	
	if ( table )
	{
		TableIndexT rows = 0, cols = 0;
		table->GetTableSize( rows, cols );
		if ( rows )
		{
			table->FocusDraw();
			table->RemoveRows( rows, 0, Refresh_Yes );
		}
	}
}


// this function is used only for "Check" button
void CMacSpellChecker::GetAlternativesForWord( LEditField *newWord, CTextTable *table, LCommander *commander )
{
	Str255	s;
	if ( newWord )
		newWord->GetDescriptor( s );
	p2cstr( s );
	
	if ( table )
	{
		table->FocusDraw();
	
		// clear previous choices (if any)
		TableIndexT rows = 0, cols = 0;
		table->GetTableSize( rows, cols );
		if ( rows )
			table->RemoveRows( rows, 0, Refresh_Yes );
		
		// add new list of choices (if any)
		STableCell cell(0, 1);
		if ( GetISpellChecker()->CheckWord( (char *)s ) )
		{
			// word spelled correctly; no need to get alternatives
			table->InsertRows( 1, 0, nil, 0, Refresh_Yes );
			
			Str255 spelledRightString;
			spelledRightString[0] = 0;
			::GetIndString( spelledRightString, SpellCheckerResource, 32 );
			cell.row = 1;
			table->SetCellData( cell, spelledRightString, spelledRightString[0] + 1 );
			table->Disable();
			
			if ( commander && newWord )
				commander->SwitchTarget( newWord );
			
			return;
		}
		
		int numrows = GetISpellChecker()->GetNumAlternatives( (char *)s );
		if ( numrows > 0 )
		{
			table->Enable();
			table->InsertRows( numrows, 0, nil, 0, Refresh_Yes );
		
			char liststr[256];
			int i, len;
			for ( i = 1; i <= numrows; i++ )
			{
				if ( GetISpellChecker()->GetAlternative( i - 1, liststr, sizeof(s) ) == 0 )
				{
					len = XP_STRLEN( liststr );
					if ( len > 0 )
					{
						cell.row = i;
						CStr255 pascalstring( liststr );
						table->SetCellData( cell, pascalstring, len + 1 );
					}
				}
			}
			
			cell.row = 1;
			table->SelectCell( cell );
			if ( commander )
				commander->SwitchTarget( table );
		}
		else
		{
			table->InsertRows( 1, 0, nil, 0, Refresh_Yes );
			
			Str255 noneFoundString;
			noneFoundString[0] = 0;
			::GetIndString( noneFoundString, SpellCheckerResource, 31 );
			cell.row = 1;
			table->SetCellData( cell, noneFoundString, noneFoundString[0] + 1 );
			table->Disable();
			
			if ( commander && newWord )
				commander->SwitchTarget( newWord );
		}
	}
}


#pragma mark -

// returns success (true) or failure (false)
static Boolean LocateDictionaries( FSSpec *mainDictionary, FSSpec *personalDictionary )
{
	FSSpec appFolder, userPrefsFolder, goodFS;
	Str255	str;
	OSErr	err;
	
	// assume main dictionary and netscape dictionary are next to each other
	// get that netscape dictionary FSSpec
	appFolder = CPrefs::GetFilePrototype( CPrefs::RequiredGutsFolder );		// CPrefs::NetscapeFolder
	// get netscape dictionary name
	::GetIndString( str, SpellCheckerResource, NSDictionaryNameIndex );
	if ( str[ 0 ] == 0 )
		return false;
	if ( str[ 0 ] > MAX_MAC_FILENAME )
		str[ 0 ] = MAX_MAC_FILENAME;
	
	strncpy( (char *)&appFolder.name, (char *)str, str[0] + 1 );
	err = FSMakeFSSpec( appFolder.vRefNum, appFolder.parID, appFolder.name, &goodFS );
	if ( err == noErr )
		memcpy(mainDictionary, &goodFS, sizeof(FSSpec));
	else
		return false;

	// check for custom dictionary (may or may not be present)
	// assume in User Profile folder
    Boolean isValidPath = false;
    char *prefMacPath = NULL;
    PREF_CopyCharPref("SpellChecker.PersonalDictionary", &prefMacPath); 
	if ( prefMacPath && *prefMacPath )
	{
		err = CFileMgr::FSSpecFromPathname( prefMacPath, &goodFS );
		if ( err == noErr )
			isValidPath = true;
	}
	
	XP_FREEIF( prefMacPath );
	
    if ( ! isValidPath )
    {
		userPrefsFolder = CPrefs::GetFilePrototype( CPrefs::MainFolder );
		// get user dictionary name
		::GetIndString( str, SpellCheckerResource, UserDictionaryNameIndex );
		if ( str[ 0 ] == 0 )
			return false;
		if ( str[ 0 ] > MAX_MAC_FILENAME )
			str[ 0 ] = MAX_MAC_FILENAME;
		
		strncpy( (char *)&userPrefsFolder.name, (char *)str, str[0] + 1 );
		err = FSMakeFSSpec( userPrefsFolder.vRefNum, userPrefsFolder.parID, userPrefsFolder.name, &goodFS );
	}
	
	memcpy(personalDictionary, &goodFS, sizeof(FSSpec));
	if ( err == fnfErr )
		return true;
	if ( err != noErr )
		return false;
	
	return true;
}

#ifndef USE_DYNAMIC_SC_LIB
/*	The direct interface to PR_FindSymbol("SC_Create") and PR_FindSymbol("SC_Destroy"):
	These prototypes can't be found in any header file; they're defined in an unreleased source file.
	It's not _too_ unsafe to define them separately here, because the interface understood
	for these functions is the one which uses PR_FindSymbol, which naturally allows
	no prototypes anyway.
*/
extern "C" ISpellChecker * SCAPI SC_Create();
extern "C" void SCAPI SC_Destroy(ISpellChecker *pSpellChecker);
#endif

void do_spellcheck( MWContext *mwcontext, CEditView *editView, CSimpleTextView *textView )
{
	if ( editView == NULL && textView == NULL )
		return;
	
	// editor requires a context
	if ( mwcontext == NULL && editView != NULL)
		return;
	
	// locate dictionaries
	FSSpec mainDictionary, personalDictionary;
	try {
		if ( ! LocateDictionaries( &mainDictionary, &personalDictionary ) )
		{
			// display message:  unable to locate dictionary!
			ErrorManager::PlainAlert( NO_DICTIONARY_FOUND );
			return;
		}
	}
	catch(...)
	{
		// display message:  unable to locate dictionary!
		ErrorManager::PlainAlert( NO_DICTIONARY_FOUND );
		return;
	}
	
#ifdef USE_DYNAMIC_SC_LIB
	const char *libPath = PR_GetLibraryPath();
	PR_SetLibraryPath( "/usr/local/netscape/" );
	
	// load library
	char *libname = "NSSpellChecker";	// internal string within SHLB
	PRLibrary *spellCheckerLib = PR_LoadLibrary( libname );
	
	// set path back to original path (don't have ourselves in list)
	PR_SetLibraryPath( libPath );

	if ( spellCheckerLib == NULL )
	{
		// failed!
		ErrorManager::PlainAlert( NO_SPELL_SHLB_FOUND );
		return;
	}
#else
	PRLibrary *spellCheckerLib = NULL;
#endif
		
	ISpellChecker *pSpellChecker = NULL;
	CMacSpellChecker *macSpellChecker = NULL;
	
	try	{
#ifdef USE_DYNAMIC_SC_LIB
		sc_create_func sc_createProc;
#ifndef NSPR20
		sc_createProc = (sc_create_func)PR_FindSymbol( "SC_Create", spellCheckerLib );
#else
		sc_createProc = (sc_create_func)PR_FindSymbol( spellCheckerLib, "SC_Create" );
#endif
		if ( sc_createProc == NULL )
		{
			ErrorManager::PlainAlert( NO_SPELL_SHLB_FOUND );
			exit_spellchecker( spellCheckerLib, pSpellChecker, macSpellChecker );
			return;
		}
		
		pSpellChecker = sc_createProc();
#else
		pSpellChecker = SC_Create();
#endif
		if ( pSpellChecker )
		{
			int	err;
			
			macSpellChecker = new CMacSpellChecker( mwcontext, editView, textView );

		    int32 Language = 0;
		    int32 Dialect = 0;
		    PREF_GetIntPref( "SpellChecker.DefaultLanguage", &Language );
		    PREF_GetIntPref( "SpellChecker.DefaultDialect", &Dialect );

			err = pSpellChecker->Initialize( Language, Dialect, 
									(char *)&mainDictionary, 
									(char *)&personalDictionary );
			if ( err == noErr )
			{
				macSpellChecker->SetISpellChecker( pSpellChecker );
				macSpellChecker->ShowDialog( NULL );
			}
		}
	}
	catch(...)
	{
		exit_spellchecker( spellCheckerLib, pSpellChecker, macSpellChecker );
		throw;
	}

	exit_spellchecker( spellCheckerLib, pSpellChecker, macSpellChecker );
}


static void exit_spellchecker( PRLibrary *lib, ISpellChecker *pSpellChecker, CMacSpellChecker *macSpellChecker )
{
	if ( pSpellChecker )
	{
#ifdef USE_DYNAMIC_SC_LIB
		sc_destroy_func sc_destroyProc;
#ifndef NSPR20
		sc_destroyProc = (sc_destroy_func)PR_FindSymbol( "SC_Destroy", lib );
#else
		sc_destroyProc = (sc_destroy_func)PR_FindSymbol( lib, "SC_Destroy" );
#endif
		if ( sc_destroyProc != NULL )
			sc_destroyProc( pSpellChecker );
#else
		SC_Destroy( pSpellChecker );
#endif
	}
	
	if ( macSpellChecker )
		delete macSpellChecker;
	macSpellChecker = NULL;
	
	// unload library
	if ( lib )
		int err = PR_UnloadLibrary( lib );
}
