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

#include "CEditDictionary.h"
#include "CLargeEditField.h"	// msg_EditField2
#include "LGAPushButton.h"
#include "resgui.h"				// msg_Help
#include "macutil.h"			// ShowHelp
#include "CSpellChecker.h"		// ISpellChecker

// need to include "ntypes.h" before "xp_help.h" because 
// "xp_help.h" doesn't include definition of MWContext though it should
#include "ntypes.h"
#include "xp_help.h"			// HELP_SPELL_CHECK

// If user has changed the word field, figure out the length and enable/disable appropriately
void		CEditDictionary::ListenToMessage	(	MessageT inMessage, 
														void*		ioParam )	{
														
	Assert_(mWordNameField);
	Assert_(mAddButton);
	
	// This happens when user types
	if (inMessage == msg_EditField2) {
		if ((**(mWordNameField->GetMacTEH())).teLength) {
			mAddButton->Enable();
		} else {
			mAddButton->Disable();
		}
	
	// User clicked in words table	
	} else if (inMessage == WordsTableID) {
	
		Boolean atleastoneselected = false;
		// Must have at least one cell selected for remove to be valid
		STableCell selchecker(0,0);
		if (mWordsView->GetNextSelectedCell(selchecker)) {
			atleastoneselected = true;
			mRemoveButton->Enable();
		} else {
			mRemoveButton->Disable();
		}
		// Replace is enabled if there is a valid word in New Word Edit Field
		// And exactly one word is selected
		if ((**(mWordNameField->GetMacTEH())).teLength && atleastoneselected &&
		 !mWordsView->GetNextSelectedCell(selchecker) ) {
			mReplaceButton->Enable();
		} else {
			mReplaceButton->Disable();
		}
	
	// User left words table
	} else if (inMessage == msg_LeaveDictionaryTable) {
		mRemoveButton->Disable();
		mReplaceButton->Disable();
	
	// If user clicks on Remove Button
	} else if (inMessage == RemoveButtonID) {
		RemoveSelectedWords();
	
	// If user clicks on Replace Button
	} else if (inMessage == ReplaceButtonID) {
		ReplaceSelectedWord();
		
	// If user clicks on OK Button
	}	else if (inMessage == msg_OK) {
		MakeDictionaryChangesPermanent();
		AttemptClose();
		
	// If user clicks on Cancel Button
	} else if (inMessage == msg_Cancel) {
		AttemptClose();
	
	// User clicks on Help Button	
	} else if (inMessage == msg_Help) {
		ShowHelp(HELP_SPELL_CHECK);
		
	// This is the case when something important happens (selection, deselection, etc.)
	// to the New Word edit field.  Check status of word
	} else if (inMessage == WordID) {
		if ((**(mWordNameField->GetMacTEH())).teLength) {
			mAddButton->Enable();
		} else {
			mAddButton->Disable();
		}
		
	// Add word to dictionary	
	} else if (inMessage == AddButtonID) {
		AddNewWord();
	} else {
		LGADialogBox::ListenToMessage(inMessage, ioParam);
	}			
	
}

//-------------------------------------------------------------------------------------
// CEditDictionary::MakeDictionaryChangesPermanent
// Take words in view and set in dictionary
//-------------------------------------------------------------------------------------
void CEditDictionary::MakeDictionaryChangesPermanent() {

	STableCell 	currCellLoc(1,1);
	Str255			currCellData;
	int					maxsize = 255;
	
	mISpellChecker->ResetPersonalDictionary();
	for (currCellLoc.row = 1; currCellLoc.row <= mNumWords; currCellLoc.row++) {
		maxsize = 255;
		mWordsView->GetCellData(currCellLoc, currCellData, maxsize);
		mISpellChecker->AddWordToPersonalDictionary(p2cstr(currCellData));
	}
}


//-------------------------------------------------------------------------------------
// CEditDictionary::RemoveSelectedWord
// Remove words that are currently selected
//-------------------------------------------------------------------------------------
void  CEditDictionary::RemoveSelectedWords() {

	STableCell currCellLoc(0,0);
	int rowstodelete = 0;
		
	while (mWordsView->GetNextSelectedCell(currCellLoc)) {
		rowstodelete++;
	}
	currCellLoc = mWordsView->GetFirstSelectedCell();
	mWordsView->RemoveRows(rowstodelete, currCellLoc.row, true);
	mNumWords-=rowstodelete;
}

//-------------------------------------------------------------------------------------
// CEditDictionary::ReplaceSelectedWord
// Remove currently selected word with word in New Word box
//-------------------------------------------------------------------------------------
void  CEditDictionary::ReplaceSelectedWord() {

	STableCell currCellLoc;
	Str255 theWord;
	
	mWordNameField->GetDescriptor(theWord);
	currCellLoc = mWordsView->GetFirstSelectedCell();
	if (mWordsView->IsValidCell(currCellLoc)) {
		mWordsView->SetCellData(currCellLoc, theWord, theWord[0] + 1);
		mWordsView->RefreshCell(currCellLoc);
	}
}


//-------------------------------------------------------------------------------------
// CEditDictionary::AddNewWord
// Add the word in "New Word" edit field to personal dictionary
//-------------------------------------------------------------------------------------
void  CEditDictionary::AddNewWord()	{

	Str255 theWord;
	Str255 currWord;
	Assert_(mWordNameField);
	Assert_(mISpellChecker);
	
	STableCell cell(1,1);
	Uint32 maxbytes;

	mWordNameField->GetDescriptor(theWord);
	
	/*
	Check to make sure word is not already in the list.  If it is, then highlight that word
	and return so we don't insert.
	*/
	for (cell.row = 1; cell.row <= mNumWords; cell.row++) {
		maxbytes = 255;
		mWordsView->GetCellData(cell, currWord, maxbytes);
		if (currWord[0] == theWord[0] && 
		 !strncmp(reinterpret_cast<const char *>(&(currWord[1])), reinterpret_cast<const char *>(&(theWord[1])), theWord[0]) ) {		// If word already exists exit
				mWordsView->UnselectAllCells();
				mWordsView->SelectCell(cell);
				return;
		}
	}
	
	mWordsView->InsertRows(1, mNumWords, theWord, theWord[0] + 1, Refresh_Yes);
	mNumWords++;
	cell.row = mNumWords;
	mWordsView->UnselectAllCells();
	mWordsView->SelectCell(cell);
}


//-------------------------------------------------------------------------------------
// CEditDictionary::FinishCreateSelf
// Setup object references
//-------------------------------------------------------------------------------------
void	CEditDictionary::FinishCreateSelf ()	{

	mAddButton = dynamic_cast<LGAPushButton *>(FindPaneByID(AddButtonID));
	mReplaceButton = dynamic_cast<LGAPushButton *>(FindPaneByID(ReplaceButtonID));
	mRemoveButton = dynamic_cast<LGAPushButton *>(FindPaneByID(RemoveButtonID));
	mHelpButton = dynamic_cast<LGAPushButton *>(FindPaneByID(HelpButtonID));
	mWordNameField = dynamic_cast<CLargeEditFieldBroadcast *>(FindPaneByID(WordID));
	mWordsView = dynamic_cast<CTextTable *>(FindPaneByID( WordsTableID ));
	
	ThrowIfNil_(mAddButton);
	ThrowIfNil_(mReplaceButton);
	ThrowIfNil_(mRemoveButton);
	ThrowIfNil_(mHelpButton);
	ThrowIfNil_(mWordNameField);
	ThrowIfNil_(mWordsView);
		
	LGADialogBox::FinishCreateSelf();

	PenState penState;
	::GetPenState( &penState );
	mWordsView->AddAttachment( new LColorEraseAttachment( &penState, NULL, NULL, true ) );
	mAddButton->Disable();		// Add button should be originally disabled
	mReplaceButton->Disable();
	mRemoveButton->Disable();
	
	mAddButton->AddListener( this );
	mReplaceButton->AddListener( this );
	mRemoveButton->AddListener( this );
	mWordNameField->AddListener( this );
	mWordsView->AddListener( this );
	mHelpButton->AddListener( this );
}

// Constructor

CEditDictionary::CEditDictionary(LStream *inStream)	: mNumWords(0), 
		mWordNameField(NULL), mISpellChecker(NULL), mAddButton(NULL), mWordsView(NULL), 
		mHelpButton(NULL), LGADialogBox (inStream) {

}

//-------------------------------------------------------------------------------------
// CEditDictionary::SetISPellChecker
// Sets personal dictionary and inserts into GUI
//-------------------------------------------------------------------------------------

void CEditDictionary::SetISpellChecker(ISpellChecker *i)	{

	STableCell	cell(0, 1);
	int unusedNumber;
	char word_buffer[max_word_length];
	int curr_word_length;
	
	mISpellChecker = i;
	cell.row = 0;
	
	if ((mISpellChecker->GetFirstPersonalDictionaryWord(word_buffer, max_word_length) >= 0) && mWordsView) {
		mWordsView->FocusDraw();
		do	{
			mNumWords++;
			cell.row++;
			curr_word_length = strlen(word_buffer);
			CStr255 pascalstring( word_buffer );
			mWordsView->InsertRows(1, cell.row-1, NULL ,0, Refresh_Yes);
			mWordsView->SetCellData(cell, pascalstring, curr_word_length + 1);
			// Insert the words into dialog box here!
		}	while	(mISpellChecker->GetNextPersonalDictionaryWord(word_buffer, max_word_length) >= 0);
	}
	
	mWordsView->GetTableSize(mNumWords, unusedNumber);
}

//-------------------------------------------------------------------------------------
// CEditDictionary::GetISPellChecker
// Get the Personal Dictionary
//-------------------------------------------------------------------------------------
ISpellChecker *CEditDictionary::GetISpellChecker()	{

	return mISpellChecker;
}

//-------------------------------------------------------------------------------------
// CEditDictionaryTable::BeTarget()
// Broadcast messages when we become target so we can setup buttons
//-------------------------------------------------------------------------------------
void CEditDictionaryTable::BeTarget() {

	CTextTable::BeTarget();
	BroadcastMessage(CEditDictionary::WordsTableID, NULL);
}

//-------------------------------------------------------------------------------------
// CEditDictionaryTable::DontBeTarget()
// Broadcast messages when we lose target so we can setup buttons
//-------------------------------------------------------------------------------------
void CEditDictionaryTable::DontBeTarget() {

	CTextTable::DontBeTarget();
	BroadcastMessage(CEditDictionary::msg_LeaveDictionaryTable, NULL);
}

//-------------------------------------------------------------------------------------
// HiliteSelection
// Setup back/front colors.  Otherwise we sometimes get the de-highlighting
// wrong when switching selection
//-------------------------------------------------------------------------------------
void CEditDictionaryTable::HiliteSelection(	Boolean	inActively, 	Boolean	 inHilite )
{
	if (inActively) {
		StColorPenState::Normalize();
	}
	LTableView::HiliteSelection( 	inActively, inHilite );
}
