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

#include "CEditorWindow.h"
#include "CBrowserContext.h"
#include "resgui.h"
#include "CFormattingToolBar.h"
#include "CFontMenuAttachment.h"
#include "CToolbarPopup.h"
#include "CPatternButtonPopupText.h"
#include "COpenRecentlyEditedPopup.h"
#include "CColorPopup.h"

#include "CEditDictionary.h"

#include "CTextTable.h"
#include "CTableKeySingleSelector.h"

	// macfe
#include "mversion.h"	// ID_STRINGS
#include "URobustCreateWindow.h"
#include "uprefd.h"
#include "uerrmgr.h"
#include "uapp.h"
#include "meditdlg.h"	// CTabbedDialog
#include "meditor.h"	// HandleModalDialog
#include "CEditView.h"

	// Netscape
#include "net.h"	// NET_cinfo_find_type
#include "proto.h"	// XP_IsContextBusy
#include "edt.h"
#include "shist.h"
#include "prefapi.h"	// PREF_GetBoolPref

#ifdef PROFILE
#pragma profile on
#endif

// pane id constants - for ShowOneDragBar
const PaneIDT Paragraph_Bar_PaneID = 'DBar';
const PaneIDT Character_Bar_PaneID = 'NBar';

const char* Pref_ShowParagraphBar = "editor.show_paragraph_toolbar";
const char* Pref_ShowCharacterBar = "editor.show_character_toolbar";

enum {  eParagraphBar,
		eCharacterBar	};

CEditorWindow::CEditorWindow( LStream* inStream )
	:CBrowserWindow(inStream)
{
	SetWindowType( WindowType_Editor );
}

 
static	// PRIVATE
CMediatedWindow* GetFrontWindowByType( OSType windowType )
{
	CMediatedWindow* theWindow = NULL;
	CWindowIterator iter(windowType);
	iter.Next(theWindow);
	return theWindow;
}

// given an mwcontext (non-busy, with a history), select the matching editor window
// or open an editor window based on that mwcontext
// if all else fails (and memory is not low), create an empty editor window
void CEditorWindow::MakeEditWindowFromBrowser( MWContext* mwcontext )
{
	if ( Memory_MemoryIsLow() )
		return;
	
	History_entry*	entry = NULL;
	if ( mwcontext && !XP_IsContextBusy( mwcontext ) )
		entry = SHIST_GetCurrent(&mwcontext->hist);
	
	if ( entry && entry->address)
	{
		// If there is already an editor window open for this url 
		// just switch to it and keep this browser window open.
		CMediatedWindow * win;
		CWindowIterator iter(WindowType_Editor);
		while (iter.Next(win))
		{
			CNSContext* curContext = ((CBrowserWindow *)win)->GetWindowContext();
			MWContext*	context;
			
			context = curContext ? curContext->operator MWContext*() : NULL;
			if (context && EDT_IS_EDITOR(context))
			{
				History_entry*	newEntry = SHIST_GetCurrent(&context->hist);
				if (newEntry && newEntry->address && !strcmp(newEntry->address, entry->address))
				{
					win->Show();
					win->Select();
					return;
				}
			}
		}
		
		if (CEditorWindow::MakeEditWindow(mwcontext, NULL) == NULL) 	// new window based on history of this window
			return;		// don't close this one on error.
	}
	else
	{
		CEditorWindow::MakeEditWindow(NULL, NULL);	// make a completely new window
	}
}


// EDT_PreOpenCallbackFn 
static void createEditorWindowCallback( XP_Bool userCanceled, char* pURL, void* hook )
{
	if ( !userCanceled )
	{
		EditorCreationStruct *edtCreatePtr = (EditorCreationStruct *)hook;
		
		if ( hook )
		{
			if ( edtCreatePtr->existingURLstruct && edtCreatePtr->existingURLstruct->address )
			{
				XP_FREE( edtCreatePtr->existingURLstruct->address );
				edtCreatePtr->existingURLstruct->address = XP_STRDUP( pURL );
			}
		}
		
		// substitute new URL
		CEditorWindow::CreateEditorWindowStage2( edtCreatePtr );
	}
}


CEditorWindow* CEditorWindow::MakeEditWindow( MWContext* old_context, URL_Struct* url )
{
	Boolean urlCameInNULL = (url == NULL);
	
	// if we don't have an URL, try to get it from the old_context's window history
	if (url == NULL && old_context != NULL)
	{
		CBrowserContext *browserContext = (CBrowserContext *)old_context->fe.newContext;
		History_entry*	entry = browserContext->GetCurrentHistoryEntry();	// Take the last instead of the first history entry.
		if ( entry )
		{
			url = SHIST_CreateURLStructFromHistoryEntry( old_context, entry );
			if ( url )
			{
				url->force_reload = NET_NORMAL_RELOAD;

				XP_MEMSET( &url->savedData, 0, sizeof( SHIST_SavedData ) );
			}
		}
		// if we don't have a history entry, we're kind of screwed-->just load a blank page
	}

	// we want to open a new blank edit window
	// or we are still having troubles... fall back to our old tried and true blank page
	if (url == NULL)
		url = NET_CreateURLStruct ("about:editfilenew", NET_NORMAL_RELOAD );
				// FIX THIS!!! the above line should use "XP_GetString(XP_EDIT_NEW_DOC_NAME)"

	// time of reckoning. We really, really need an URL and address at this point...
	// I don't know what an url without an address is anyway...
	if (url == NULL || url->address == NULL)
		return NULL;
	
	// now make sure that the url is a valid type to edit.
	NET_cinfo *cinfo = NET_cinfo_find_type (url->address);
	
	if (cinfo == NULL || cinfo->type == NULL ||
		  (strcasecomp (cinfo->type, TEXT_HTML)
		&& strcasecomp (cinfo->type, UNKNOWN_CONTENT_TYPE)
		&& strcasecomp (cinfo->type, TEXT_PLAIN)))
	{
		ErrorManager::PlainAlert( NOT_HTML );
		return NULL;
	}

	EditorCreationStruct *edtCreatePtr = (EditorCreationStruct *)XP_ALLOC( sizeof( EditorCreationStruct ) );
	if ( edtCreatePtr )
	{
		edtCreatePtr->context = old_context;
		edtCreatePtr->existingURLstruct = url;
		edtCreatePtr->isEmptyPage = old_context == NULL && urlCameInNULL;
	}
		
	if ( urlCameInNULL && old_context == NULL )
	{
		return CreateEditorWindowStage2( edtCreatePtr );
	}
	else
	{
		EDT_PreOpen( old_context, url->address, createEditorWindowCallback, edtCreatePtr );
		return NULL;
	}
}


CEditorWindow *CEditorWindow::CreateEditorWindowStage2( EditorCreationStruct *edtCreatePtr )
{
	// now we can create an editor window since we don't already have one for this url.
	
	/* instead of just calling LWindow::CreateWindow(), we do it ourselves by hand here 
	 * so that we can set the window bounds before we call FinishCreate().
	 */
	
	if ( edtCreatePtr == NULL )
		return NULL;
	
	CEditorWindow* newWindow = NULL;
	SetDefaultCommander(CFrontApp::GetApplication());
	try {
		OSErr	error;
		URobustCreateWindow::ReadObjects( 'PPob', CEditorWindow::res_ID, &newWindow, &error );
		Assert_(newWindow);
		FailOSErr_(error);
		if (newWindow == NULL)
		{
			XP_FREE( edtCreatePtr );
			return NULL;
		}

		newWindow->FinishCreate();
		if (newWindow->HasAttribute(windAttr_ShowNew))
			newWindow->Show();

		UReanimator::LinkListenerToControls( newWindow, newWindow, CEditorWindow::res_ID );
	}
	catch(...)
	{
		if ( newWindow )
			delete newWindow;
		
		XP_FREE( edtCreatePtr );
		
		return NULL;
	}
	
	Boolean hasURLstruct = edtCreatePtr->existingURLstruct && edtCreatePtr->existingURLstruct->address;
	CBrowserContext *nscontext = new CBrowserContext();
	newWindow->SetWindowContext( nscontext );
	
	CURLDispatcher::DispatchURL( edtCreatePtr->existingURLstruct, nscontext, false, false, CEditorWindow::res_ID );
	
	if ( edtCreatePtr->context )
	{
		nscontext->InitHistoryFromContext( (CBrowserContext *)edtCreatePtr->context->fe.newContext );
	}
	else
	{
		// the url will eventually be freed when the load is complete.
	}

	// set window title here (esp. for "open")
	if ( !edtCreatePtr->isEmptyPage
	&& edtCreatePtr->existingURLstruct && edtCreatePtr->existingURLstruct->address )
	{
		char *pSlash = strrchr( edtCreatePtr->existingURLstruct->address, '/' );
		if ( pSlash )
			pSlash += 1;	// move past '/'
		newWindow->NoteDocTitleChanged( pSlash );
	}
	
	XP_FREE( edtCreatePtr );
	
	return newWindow;
}

		
void CEditorWindow::SetWindowContext(CBrowserContext* inContext)
{
	if ( inContext )
	{
		MWContext *context;
		context = inContext->operator MWContext*();
		if ( context )
		{
			context->is_editor = true;
			NET_CheckForTimeBomb( context );
		}
	}

	CBrowserWindow::SetWindowContext( inContext );
}


void CEditorWindow::NoteDocTitleChanged( const char* inNewTitle )
{
	// there is one bogus set-title from layout that we want to skip...
	// We are hard coding "editfilenew" here because it is already hardcoded 
	// a million other places...
	if ( inNewTitle && XP_STRCMP( inNewTitle, "editfilenew" ) == 0 )
		return;

	CNSContext *theContext = GetWindowContext();
	
	char *baseName = LO_GetBaseURL( theContext->operator MWContext*() );	// don't free this...
	// strip out username and password so user doesn't see them in window title
	char *location = NULL;
	if ( baseName )
		NET_ParseUploadURL( baseName, &location, NULL, NULL );
	CStr255	csBaseURL(location);
	// if this page has a local "file:" url, then just show the file name (skip the url and directory crap.)
	
	if ( location && NET_IsLocalFileURL(location) )
	{
		char *localName = NULL;
		XP_ConvertUrlToLocalFile( location, &localName );
		
		if (localName)
		{
#if 0
			char *pSlash = strrchr(localName, '/');
			if (pSlash)
			{		// is there is a slash, move everything AFTER the last slash to the front
				pSlash++;
				XP_STRCPY(localName, pSlash);
			}
#endif

			csBaseURL = localName;
			XP_FREE(localName);
		}
	}
	
	CStr255 	netscapeTitle;
	CStr255		subTitle;
	
	::GetIndString( netscapeTitle, ID_STRINGS, APPNAME_STRING_INDEX );
	::GetIndString( subTitle, WINDOW_TITLES_RESID, 3 );
	
	netscapeTitle += " ";
	netscapeTitle += subTitle;
	netscapeTitle += " - [";
	
	// set up page title manually; rather than rely on XP string passed in
	EDT_PageData * pageData = EDT_GetPageData( theContext->operator MWContext*() );
	if ( pageData && pageData->pTitle && pageData->pTitle[0] )
	{
		netscapeTitle += pageData->pTitle;
		if (csBaseURL.Length())
			netscapeTitle += " : ";
	}
	
	if ( pageData )
		EDT_FreePageData( pageData );
	
	// add file path to end
	netscapeTitle += csBaseURL;
	netscapeTitle += "]";

	SetDescriptor( netscapeTitle );

	if ( location )
		XP_FREE( location );
}


void CEditorWindow::RegisterViewTypes()
{
	// Registers all its view types
	RegisterClass_( CEditorWindow);
	RegisterClass_( CEditView);
	RegisterClass_( MultipleSelectionSingleColumn);	// newer, better class?
	RegisterClass_( CTarget);
	RegisterClass_( CLineProp);
	RegisterClass_( CFormattingToolBar);
	
	RegisterClass_( CToolbarPopup);					// newer, better class?
	RegisterClass_( CIconToolbarPopup);				// newer, better class?
	RegisterClass_( CColorPopup);
	RegisterClass_( CFontMenuPopup );
	
	RegisterClass_( CChameleonCaption);				// newer, better class?
	RegisterClass_( CChameleonView);				// newer, better class?
	CTabbedDialog::RegisterViewTypes();
	
	RegisterClass_( CTextTable);					// newer, better class?
	RegisterClass_( CTableKeySingleSelector);		// newer, better class?

	RegisterClass_( CPatternButtonPopupText);		// newer, better class?
	RegisterClass_( LOffscreenView);
	RegisterClass_( COpenRecentlyEditedPopup );
	
	RegisterClass_( CEditDictionary);
	RegisterClass_( CEditDictionaryTable);	
}


void CEditorWindow::FinishCreateSelf()
{
	CBrowserWindow::FinishCreateSelf();

	// Show/hide toolbars based on preference settings
	XP_Bool	value;
	PREF_GetBoolPref(Pref_ShowParagraphBar, &value);
	mToolbarShown[eParagraphBar] = value;
	ShowOneDragBar(Paragraph_Bar_PaneID, value);
	PREF_GetBoolPref(Pref_ShowCharacterBar, &value);
	mToolbarShown[eCharacterBar] = value;
	ShowOneDragBar(Character_Bar_PaneID, value);
}


void CEditorWindow::ListenToMessage( MessageT inMessage, void* ioParam )
{
	switch (inMessage)
	{
		case msg_NSCDocTitleChanged:
			NoteDocTitleChanged((const char*)ioParam);
			break;
	
		default:
		{
			if ( inMessage == 'Para' )
			{
				switch  (*(long*)ioParam)
				{
					case 1:		inMessage = cmd_Format_Paragraph_Normal;		break;
					case 2:		inMessage = cmd_Format_Paragraph_Head1;			break;
					case 3:		inMessage = cmd_Format_Paragraph_Head2;			break;
					case 4:		inMessage = cmd_Format_Paragraph_Head3;			break;
					case 5:		inMessage = cmd_Format_Paragraph_Head4;			break;
					case 6:		inMessage = cmd_Format_Paragraph_Head5;			break;
					case 7:		inMessage = cmd_Format_Paragraph_Head6;			break;
					case 8:		inMessage = cmd_Format_Paragraph_Address;		break;
					case 9:		inMessage = cmd_Format_Paragraph_Formatted;		break;
					case 10:	inMessage = cmd_Format_Paragraph_List_Item;		break;
					case 11:	inMessage = cmd_Format_Paragraph_Desc_Title;	break;
					case 12:	inMessage = cmd_Format_Paragraph_Desc_Text;		break;
				}
			}
			else if ( inMessage == 'Size' )
			{
				switch  (*(long*)ioParam)
				{
					case 1:		inMessage = cmd_Format_Font_Size_N2;	break;
					case 2:		inMessage = cmd_Format_Font_Size_N1;	break;
					case 3:		inMessage = cmd_Format_Font_Size_0;		break;
					case 4:		inMessage = cmd_Format_Font_Size_P1;	break;
					case 5:		inMessage = cmd_Format_Font_Size_P2;	break;
					case 6:		inMessage = cmd_Format_Font_Size_P3;	break;
					case 7:		inMessage = cmd_Format_Font_Size_P4;	break;
				}
			}
			else if ( inMessage == 'Algn' )
			{
				switch  (*(long*)ioParam)
				{
					case 1:		inMessage = cmd_JustifyLeft;	break;
					case 2:		inMessage = cmd_JustifyCenter;	break;
					case 3:		inMessage = cmd_JustifyRight;	break;
				}
			}

			// GetHTMLView() guaranteed not to fail
			GetHTMLView()->ObeyCommand( inMessage, ioParam );
			break;
		}
	}
}


// EDT_PreCloseCallbackFn 
static void closeEditorWindowCallback( void* hook )
{
	CEditorWindow *editorWindow = (CEditorWindow *)hook;
	if ( editorWindow )
		editorWindow->SetPluginDoneClosing();
}


Boolean CEditorWindow::ObeyCommand( CommandT inCommand, void *ioParam )
{
	switch ( inCommand )
	{
		case cmd_NewWindowEditor:
			CEditorWindow::MakeEditWindow( NULL, NULL );
			break;
		
		case cmd_ViewSource:
			// Delegate this to the view.
			if ( ((CEditView *)GetHTMLView())->IsDoneLoading() )
				GetHTMLView()->ObeyCommand(inCommand, ioParam);
			break;
				
		case cmd_Toggle_Paragraph_Toolbar:
			ToggleDragBar(Paragraph_Bar_PaneID, eParagraphBar, Pref_ShowParagraphBar);
			break;
		
		case cmd_Toggle_Character_Toolbar:
			ToggleDragBar(Character_Bar_PaneID, eCharacterBar, Pref_ShowCharacterBar);
			break;

		case cmd_Close:
		case cmd_Quit:				// we'll just intercept these and then send them on to the default case
			MWContext *mwcontext = GetWindowContext()->operator MWContext*();
			History_entry*	newEntry = SHIST_GetCurrent(&mwcontext->hist);
			CStr255 fileName;
			if ( newEntry && newEntry->address )
				fileName = newEntry->address;
				
			if ( ((CEditView *)GetHTMLView())->IsDoneLoading() && EDT_DirtyFlag( *GetWindowContext() ) )
			{
				Select(); // This helps during a quit or "close all"
				
				MessageT itemHit = HandleModalDialog( EDITDLG_SAVE_BEFORE_QUIT, fileName, NULL );
				if (itemHit == cancel)
					return true;
				
				if (itemHit == ok)
				{				// save
					if ( !((CEditView *)GetHTMLView())->SaveDocument() )
						return true;			
				}
				EDT_SetDirtyFlag( mwcontext, false );		// we have to do this or else when we quit, we will be asked twice to save
			}
			
			// need to let this work asynchronously; make our own internal loop
			if ( ((CEditView *)GetHTMLView())->IsDoneLoading() && newEntry && newEntry->address )
			{
				mPluginDoneClosing = false;
				EDT_PreClose( mwcontext, newEntry->address, closeEditorWindowCallback, this );
				
				do
				{
					CFrontApp::GetApplication()->ProcessNextEvent();
				} while ( !mPluginDoneClosing );
			}

		// fall through

		default:
		{
			return CBrowserWindow::ObeyCommand (inCommand, ioParam);
		}
	}
	return TRUE;
}


void CEditorWindow::FindCommandStatus( CommandT inCommand,
	Boolean& outEnabled, Boolean& outUsesMark, Char16& outMark,
	Str255 outName )
{
	short index;
	outUsesMark = FALSE;
	outEnabled = false;

	switch ( inCommand )
	{
		case cmd_ViewSource:
			// Delegate this to the view.
			if ( ((CEditView *)GetHTMLView())->IsDoneLoading() )
				GetHTMLView()->FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			break;

		case cmd_Toggle_Character_Toolbar:
			outEnabled = ((CEditView *)GetHTMLView())->IsDoneLoading();
			index = mToolbarShown[eCharacterBar] ? CEditView::EDITOR_MENU_HIDE_COMP_TOOLBAR 
											: CEditView::EDITOR_MENU_SHOW_COMP_TOOLBAR;
			::GetIndString( outName, CEditView::STRPOUND_EDITOR_MENUS, index );
			break;

		case cmd_Toggle_Paragraph_Toolbar:
			outEnabled = ((CEditView *)GetHTMLView())->IsDoneLoading();
			index = mToolbarShown[eParagraphBar] ? CEditView::EDITOR_MENU_HIDE_FORMAT_TOOLBAR 
											: CEditView::EDITOR_MENU_SHOW_FORMAT_TOOLBAR;
			::GetIndString( outName, CEditView::STRPOUND_EDITOR_MENUS, index );
			break;

		default:
			CBrowserWindow::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName );
	}
}


// Called when we are trying to quit the application
Boolean	CEditorWindow::AttemptQuitSelf( long inSaveOption )
{
	MWContext *mwcontext = GetWindowContext()->operator MWContext*();
	if ( EDT_DirtyFlag( mwcontext ) )
	{
		History_entry*	newEntry = SHIST_GetCurrent(&mwcontext->hist);
		CStr255 fileName;
		if ( newEntry && newEntry->address )
			fileName = newEntry->address;

		if ( kAEAsk == inSaveOption )
		{
			MessageT itemHit = HandleModalDialog( EDITDLG_SAVE_BEFORE_QUIT, fileName, NULL );
			if ( cancel == itemHit )
				return false;
		}
		
		// save
		if ( !((CEditView *)GetHTMLView())->SaveDocument() ) 
			return false;			
	}
	
	return true;
}

#ifdef PROFILE
#pragma profile off
#endif
