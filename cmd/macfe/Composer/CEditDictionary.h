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

#include "CTextTable.h"
#include <LGADialogBox.h>


class LGAPushButton;
class CLargeEditFieldBroadcast;
class ISpellChecker;


class CEditDictionary : public LGADialogBox {

	public:

		virtual 	void							ListenToMessage	(	MessageT inMessage, 
																	void*		ioParam );		
														
		virtual		void							FinishCreateSelf();
													CEditDictionary(LStream *inStream);
																
					void							SetISpellChecker(ISpellChecker *i);
					ISpellChecker					*GetISpellChecker();						
																
		enum	{ class_ID = 'CEdD', res_ID = 5298 };
		enum	{ AddButtonID	= 201 ,	WordID = 200, WordsTableID = 202, ReplaceButtonID = 203,
				RemoveButtonID = 204, HelpButtonID = 205, msg_LeaveDictionaryTable = 208 };
		enum	{ max_word_length = 50 };	// Max length of a word
		
	protected:
	
							void 							MakeDictionaryChangesPermanent();
		virtual		void 							AddNewWord();
		virtual		void  						RemoveSelectedWords();
		virtual		void							ReplaceSelectedWord();
		CLargeEditFieldBroadcast 		*mWordNameField;			// The field where user edits word
		LGAPushButton 							*mAddButton;					// Add new word button
		LGAPushButton								*mReplaceButton;
		LGAPushButton								*mRemoveButton;
		LGAPushButton								*mHelpButton;
		CTextTable									*mWordsView;					// The Personal Dictionary view
		
		int													mNumWords;
		
		ISpellChecker								*mISpellChecker;
};

class CEditDictionaryTable : public CTextTable {

	public:
	
			enum	{class_ID = 'CEDT' };
			virtual void		BeTarget();
			virtual void		DontBeTarget();
											CEditDictionaryTable(LStream *inStream) : CTextTable(inStream) { }
	
							void 		HiliteSelection(	Boolean	inActively, 	Boolean	 inHilite );
};