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

#include "CBrowserWindow.h"


struct EditorCreationStruct {
	MWContext	*context;	// may be NULL
	URL_Struct	*existingURLstruct;
	Boolean		isEmptyPage;
};



/******************************************************************************
 * CEditorWindow is really just a CBrowserWindow with some changes for an editor window.
 ******************************************************************************/
class CEditorWindow: public CBrowserWindow
{	
public:
	enum {class_ID = 'edtw', res_ID = 10000};
	
	// ее Constructors

	static void 		RegisterViewTypes();	// Registers all its view types
	static CEditorWindow *MakeEditWindow( MWContext* old_context, URL_Struct* url );
	static CEditorWindow *CreateEditorWindowStage2( EditorCreationStruct *edtCreatePtr);
	static void 		MakeEditWindowFromBrowser( MWContext* mwcontext );

						CEditorWindow(LStream * inStream);
	virtual void 		FinishCreateSelf();
	virtual void		SetWindowContext(CBrowserContext* inContext);

	// ее Command handling
	virtual void		ListenToMessage( MessageT inMessage, void* ioParam );
	virtual	Boolean 	ObeyCommand( CommandT inCommand, void *ioParam );
	virtual void  		FindCommandStatus( CommandT inCommand,
							Boolean& outEnabled, Boolean& outUsesMark, 
							Char16& outMark,Str255 outName );
	virtual void		NoteDocTitleChanged( const char* title );
	virtual Boolean		AttemptQuitSelf( long inSaveOption );
	static CEditorWindow*	FindAndShow(Boolean inMakeNew = false);
	virtual void		SetPluginDoneClosing() { mPluginDoneClosing = true; }

protected:

	virtual ResIDT		GetStatusResID(void) const { return CEditorWindow::res_ID; }

	Boolean  			mPluginDoneClosing;
};
