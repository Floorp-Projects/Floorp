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
// findw.h
// Find dialog box.
// Contains find dialog box declarations.
// Minimal interface in the header file, most of the class definitions are in
// .cp file.
// Created by atotic, June 6th, 1994
// ===========================================================================


#pragma once
#include <LDialogBox.h>
#include <LStream.h>
#include <LApplication.h>
#include "cstring.h"
#include "CHTMLView.h"

class CTSMEditField;

/*****************************************************************************
 * class CFindWindow
 * Find dialog box
 *****************************************************************************/
 
class CFindWindow: public LDialogBox
{
	private:
		typedef LDialogBox super;
	public:
		enum				{ class_ID = 'find' };
		
			// constructor
							CFindWindow(LStream* inStream);
		virtual				~CFindWindow();
		virtual void		FinishCreateSelf();
		
		virtual void		GetDialogValues();
		virtual void		SetDialogValues();
		virtual void 		SetTextTraitsID(ResIDT	inTextTraitsID);
		
		static void			DoFind(CHTMLView* theHTMLView);	
		static Boolean		CanFindAgain();
		
			// Interface to the application
		static void			RegisterViewTypes();

			// events & commands
		virtual void		FindCommandStatus(CommandT inCommand,
												Boolean& outEnabled, 
												Boolean& outUsesMark,
												Char16& outMark,
												Str255 outName);
		virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
		virtual void		SavePlace (LStream *outPlace);
		virtual void		RestorePlace (LStream *inPlace);
		virtual void		DoSetBounds (const Rect &inBounds);

		static CHTMLView*	sViewBeingSearched;
		static CFindWindow* sFindWindow;		// Global holds current find window
		static cstring		sLastSearch;		// Last search string
		static Boolean		sCaseless;			// Search is caseless?
		static Boolean		sBackward;			// Search backward
		static Boolean		sWrap;				// Wrap to beginning of doc

	protected:
		CTSMEditField*		fSearchFor;
		LStdCheckBox*		fCaseSensitive;
		LStdCheckBox*		fSearchBackwards;
		LStdCheckBox*		fWrapSearch;

	private:
};

class LMailNewsFindWindow: public CFindWindow
{
public:
	enum				{ class_ID = 'Dfmn' };
	
						LMailNewsFindWindow( LStream* inStream );
	
	virtual void		FinishCreateSelf();
	virtual void		GetDialogValues();
	
	static Boolean		sFindInHeaders;

protected:
	LStdRadioButton*	fFindInHeaders;
};


/*****************************************************************************
 * class CComposeFindWindow
 * handles Composer's Find/Replace dialog
 *****************************************************************************/

class CComposeFindWindow: public CFindWindow
{
	public:
		enum				{ class_ID = 'Efnd' };
		
							CComposeFindWindow( LStream* inStream );
		virtual void		FinishCreateSelf();
		virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
		virtual void		SetDialogValues();
		virtual void 		GetDialogValues();

		static cstring		sLastReplace;		// Last search string

	protected:
		CTSMEditField*		fReplaceWith;
};

/*****************************************************************************
 * class CLoadURLBox
 * The only thing it does is sets the fGetURLWindow of uapp to NULL on deletion
 *****************************************************************************/
class CLoadURLBox: public LDialogBox
{
public:
						CLoadURLBox( LStream* inStream );
	virtual				~CLoadURLBox();
};




