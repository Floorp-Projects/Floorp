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

#include <CWASTEEdit.h>


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Wrapping Modes
//
//	Text can be wrapped in one of the following three ways:
//
//		wrapMode_WrapToFrame - Wrap to view frame.  This is similar to common
//			TE style wrapping.  
//
//		wrapMode_FixedPage - Don't wrap.  This would be like the metrowerks
//			code editor in which the page size is arbitrarily large in width
//			but no wrapping will occur when text flows outside the page bounds.
//
//		wrapMode_FixedPage - Text is wrapped to the width of the page.  This
//			would be like a word processor.
//
//		wrapMode_WrapWidestUnbreakable - The page size is determined by
//			the view frame size or the width of the widest unbreakable run.
//			This behaviour would be most like an HTML browser.  Text can
//			always be broken, but things like graphic images can not.
//			With text only, this behaves like the normal wrap to frame mode.
//
//	IMPORTANT NOTE:  if you are providing multiple views on a text engine,
//		and those views have different wrapping modes, the formatter will
//		reformat the text to the mode specified by the view EVERY TIME THE
//		VIEW IS FOCUSED.  This is probably not something you'd want to have
//		happening in your app.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


enum {
	wrapMode_WrapToFrame = 1,
	wrapMode_FixedPage,
	wrapMode_FixedPageWrap,
	wrapMode_WrapWidestUnbreakable
};

struct SSimpleTextParms {
	Uint16				attributes;
	Uint16				wrappingMode;
	ResIDT				traitsID;
	ResIDT				textID;
	Int32				pageWidth;
	Int32				pageHeight;
	Rect				margins;
};

class CSimpleTextView : public CWASTEEdit
	{
//		friend class LTextEditHandler;	//	this will be going away!

		private:
							typedef 			CWASTEEdit		Inherited;
							
		
														CSimpleTextView();	//	Must use parameters
		
		public:
		
			enum 
				{ 
				
					class_ID = 'Stxt' 
					
				};
			
														CSimpleTextView(				LStream			*inStream);
			
			virtual								~CSimpleTextView();
			
//		virtual void		BuildTextObjects(LModelObject *inSuperModel);
			
// ¥ Event Handler Overrides
//		virtual void			NoteOverNewThing(LManipulator* inThing);

			virtual Boolean				ObeyCommand(						CommandT 		inCommand, 
																										void				*ioParam);
																								
			virtual void					FindCommandStatus(			CommandT 		inCommand, 
																										Boolean			&outEnabled, 
																										Boolean			&outUsesMark, 
																										Char16			&outMark, 
																										Str255 			outName);

			static pascal void 		TSMPreUpdate( 					WEReference inWE);

			// ¥ Initialization
				
			virtual	void 					SetInitialTraits(ResIDT inTextTraitsID);
			virtual	void					SetInitialText(ResIDT inTextID);
			
			virtual Boolean					IsReadOnly();
			virtual void					SetReadOnly( const Boolean inFlag );
			
			virtual void					SetTabSelectsAll ( const Boolean inTabIntoFieldSelectsAll )
																								{
																									mTabIntoFieldSelectsAll = inTabIntoFieldSelectsAll;
																								};
			// Commands
					
			void									Save( const FSSpec	&inFileSpec );
					
		protected:

			virtual void					ClickSelf( const SMouseDownEvent &inMouseDown);
			virtual Boolean			 		HandleKeyPress( const EventRecord &	inKeyEvent);
			
			virtual void					BeTarget();
			virtual void					DontBeTarget();
		
			virtual	void					FinishCreateSelf(void);
			virtual void					Initialize();
			
//			SSimpleTextParms*			mTextParms;

			static Boolean								sInitialized;
			static Boolean								sHasTSM;
			static WETSMPreUpdateUPP			sPreUpdateUPP;
			static CSimpleTextView				*sTargetView;
			
		private:
			
			Boolean								mTabIntoFieldSelectsAll;
	};


	