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

#include "spellchk.h"
#include "ntypes.h"		// MWContext

class CEditView;
class LEditField;
class CTextTable;
class CSimpleTextView;

// public definitions
void do_spellcheck( MWContext *mwcontext, CEditView *editView, CSimpleTextView *textView );
#define cmd_CheckSpelling					'ChSp'


// internal
const int SpellCheckerResource = 5109;
const int ChangeStringIndex = 1;
const int DoneStringIndex = 2;
const int NSDictionaryNameIndex = 3;
const int UserDictionaryNameIndex = 4;


// UI management class
class CMacSpellChecker
{
public:
	enum { class_ID = 'Spel', res_ID = 5299 };
	enum { pane_NewWord = 'NewW', pane_SuggestionList = 'AltW',
			msg_Change = 'Chng', msg_Change_All = 'CAll', 
			msg_Ignore = 'Ignr', msg_Ignore_All = 'IAll',
			msg_Add_Button = 'AddB', msg_Check = 'Chck',
			msg_Stop = 'Stop', msg_NewLanguage = 'Lang',
			msg_SelectionChanged = 'SelC', msg_EditDictionary = 'EdDc' };
	
				CMacSpellChecker( MWContext *context,
									CEditView *editView, CSimpleTextView *textView );
	
	char		*GetTextBuffer();
	void		GetSelection( int32 &selStart, int32 &selEnd );
	void		ReplaceHilitedText( char *newText, Boolean doAll );
	void		IgnoreHilitedText( Boolean doAll );
	void		SetNextMisspelledWord( char *textP, LEditField *typoField, CTextTable *t, LCommander *c );
	Boolean		GetNextMisspelledWord( Boolean doFirstWord );
	void		ClearReplacementWord( LEditField *newWord, CTextTable *table );
	void		GetAlternativesForWord( LEditField *newWord, CTextTable *table, LCommander *c );
	
	Boolean		StartProcessing( Boolean startOver );
	void		ShowDialog( char *textP );
	
	MWContext	*GetMWContext()	{ return mMWContext; };
	ISpellChecker *GetISpellChecker()	{ return mISpellChecker; };
	void		SetISpellChecker( ISpellChecker *i ) { mISpellChecker = i; };
	
	Boolean		isHTMLeditor()	{ return mEditView != NULL; };
	
private:
	ISpellChecker	*mISpellChecker;
	MWContext		*mMWContext;	// only if mEditView; ignored if mTextView
	Str255			mOrigMisspelledWord;
	
	// we should have one and only one of these-->evidence that this class is mis-designed
	CEditView		*mEditView;
	CSimpleTextView	*mTextView;
};
