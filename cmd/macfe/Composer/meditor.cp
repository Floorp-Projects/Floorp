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

#include "meditor.h"	// HandleModalDialog
#include "StBlockingDialogHandler.h"
#include "CEditView.h"
#include "macgui.h"		// UGraphics::MakeLOColor
#include "CPaneEnabler.h"
#include "resgui.h"		// EDITDLG_AUTOSAVE
#include "edt.h"
#include "fe_proto.h"
#include "prefapi.h"	// PREF_GetBoolPref, PREF_GetIntPref
#include "shist.h"		// SHIST_GetCurrent
#include "uapp.h"		// CFrontApp
#include "CColorPopup.h"

extern "C" {
#include "xpgetstr.h"
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS
}

#include "CNSContext.h"	// ExtractHyperView


// takes pascal-strings
MessageT HandleModalDialog( int id, const unsigned char *prompt1, const unsigned char* prompt2)
{
	StPrepareForDialog		prepare;
	
	StBlockingDialogHandler handler( id, NULL );
	LDialogBox* dialog = (LDialogBox *)handler.GetDialog();
	if ( prompt1 )
	{
		LCaption *caption = (LCaption *)dialog->FindPaneByID( '^1  ' );
		if ( caption )
			caption->SetDescriptor( prompt1 );
	}
	if ( prompt2 )
	{
		LCaption *caption = (LCaption *)dialog->FindPaneByID( '^2  ' );
		if ( caption )
			caption->SetDescriptor( prompt2 );
	}
	
	MessageT message;
	do {
		message = handler.DoDialog();
	}
	while ( message == 0 );
	
	return message;
}

/* Set default colors, background from user Preferences via the Page Data structure 
*/
void FE_SetNewDocumentProperties(MWContext * pContext)
{
	if ( pContext && pContext->is_editor && pContext->bIsComposeWindow )
		return;

    EDT_PageData *pageData = EDT_NewPageData();

    if (pageData == NULL)  return;

	if (CPrefs::GetBoolean(CPrefs::EditorUseCustomColors )) {
	
		LO_Color EditorText = UGraphics::MakeLOColor(CPrefs::GetColor(CPrefs::EditorText));
		LO_Color EditorLink = UGraphics::MakeLOColor(CPrefs::GetColor(CPrefs::EditorLink));
		LO_Color EditorActiveLink = UGraphics::MakeLOColor(CPrefs::GetColor(CPrefs::EditorActiveLink));
		LO_Color EditorFollowedLink = UGraphics::MakeLOColor(CPrefs::GetColor(CPrefs::EditorFollowedLink));
		LO_Color EditorBackground = UGraphics::MakeLOColor(CPrefs::GetColor(CPrefs::EditorBackground));

		pageData->pColorText = &EditorText;
		pageData->pColorLink= &EditorLink;
		pageData->pColorActiveLink = &EditorActiveLink;
		pageData->pColorFollowedLink = &EditorFollowedLink;
		pageData->pColorBackground = &EditorBackground;
	
	} else {
	
		pageData->pColorText = NULL;			// I assume this is how we get the browser defaults...
		pageData->pColorLink=  NULL;
		pageData->pColorActiveLink =  NULL;
		pageData->pColorFollowedLink =  NULL;
		pageData->pColorBackground =  NULL;
		
	}

	Bool hasBackgroundImage;
	if ( ( PREF_GetBoolPref( "editor.use_background_image", &hasBackgroundImage ) == PREF_NOERROR )
		&& hasBackgroundImage )
	{
		pageData->pBackgroundImage = CPrefs::GetCharPtr(CPrefs::EditorBackgroundImage);
		if (pageData->pBackgroundImage && XP_STRLEN(pageData->pBackgroundImage) == 0)			// if there is really nothing there, skip it.
			pageData->pBackgroundImage = NULL;
	}
	else
		pageData->pBackgroundImage = NULL;
	    
	if ( pContext && pContext->title )
		pageData->pTitle = XP_STRDUP(pContext->title);
		

	EDT_SetPageData(pContext, pageData);
	
	pageData->pColorText = NULL;		//  don't free out lacal data!!!
	pageData->pColorLink=  NULL;
	pageData->pColorActiveLink =  NULL;
	pageData->pColorFollowedLink =  NULL;
	pageData->pColorBackground =  NULL;
	pageData->pBackgroundImage =  NULL;
	
	EDT_FreePageData(pageData);

    // Set Author name:

//	CStr255	EditorAuthor(CPrefs::GetString(CPrefs::EditorAuthor));
	
//	FE_UsersFullName();
	

    EDT_MetaData *metaData = EDT_NewMetaData();
    if (metaData == NULL) return;
    metaData->bHttpEquiv = FALSE;
    metaData->pName = XP_STRDUP("Author");
    metaData->pContent = XP_STRDUP(CPrefs::GetString(CPrefs::EditorAuthor));
    EDT_SetMetaData(pContext, metaData);     
    EDT_FreeMetaData(metaData);
}


/* 
 * Brings up a modal image load dialog and returns.  Calls
 *  EDT_ImageLoadCancel() if the cancel button is pressed
*/
void FE_ImageLoadDialog( MWContext * /* pContext */ )
{
}

/* 
 * called by the editor engine after the image has been loaded
*/
void FE_ImageLoadDialogDestroy( MWContext * /* pContext */ )
{
}

void FE_EditorDocumentLoaded( MWContext *pContext )
{
	if (pContext == NULL || !EDT_IS_EDITOR(pContext))
		return;
	
	CEditView *editView = (CEditView *)ExtractHyperView(pContext);

	int32 iSave;
	if ( pContext->bIsComposeWindow )
	{
		iSave = 0;	// auto-save
		
		CMailEditView *mailEditView = dynamic_cast<CMailEditView *>(editView);
		if ( mailEditView )
			mailEditView->InitMailCompose();
	}
	else
	{ 
		XP_Bool doAutoSave;
		PREF_GetBoolPref( "editor.auto_save", &doAutoSave );
		if ( doAutoSave )
			PREF_GetIntPref( "editor.auto_save_delay", &iSave );
		else
			iSave = 0;
	}
	
	EDT_SetAutoSavePeriod(pContext, iSave );
	
	// remember when the file was (last) modified
	// initializes date/time stamp for external editor warning
	EDT_IsFileModified(pContext);
	
	// We had disabled everything, now we have to enable it again. This happens automatically on activate, but we might not get an activate
	// if we don't have a dialog poping up (like if the user just creates a new document, there is no dialog...)

	// set this after calling InitMailCompose
	if ( editView )
	{
		editView->mEditorDoneLoading = true;
			
		// set color popup control to show correct default color (now that we have an mwcontext)
		editView->mColorPopup->InitializeCurrentColor();
	}
	
	InitCursor();
	(CFrontApp::GetApplication())->UpdateMenus();
}


Bool FE_CheckAndAutoSaveDocument(MWContext *pContext)
{
	if (pContext == NULL || !EDT_IS_EDITOR(pContext) || ExtractHyperView(pContext) == NULL )
		return FALSE;
	
	if ( pContext->bIsComposeWindow )
		return FALSE;
	
	CEditView *editView = (CEditView *)ExtractHyperView(pContext);
	if ( FrontWindow() != editView->GetMacPort() )
		return true;
	
	if (!EDT_DirtyFlag(pContext) && !EDT_IS_NEW_DOCUMENT(pContext))
		return TRUE;
	
	History_entry*	newEntry = SHIST_GetCurrent(&pContext->hist);
	CStr255 fileName;
	if ( newEntry && newEntry->address )
		fileName = newEntry->address;
				
	MessageT itemHit = HandleModalDialog(EDITDLG_AUTOSAVE, fileName, NULL );
	if (itemHit != ok)
		return FALSE;
		
	return ((CEditView *)ExtractHyperView(pContext))->SaveDocument();
}


void FE_FinishedSave( MWContext * /* pMWContext */, int /* status */, char * /* pDestURL */, int /* iFileNumber */ )
{
}

// in xp_file.h
// Create a backup filename for renaming current file before saving data
//    Input should be be URL file type "file:///..."
//    Caller must free the string with XP_FREE

/*
 * I don't know what the logic here should be, so I mostly copied this from the Windows code in:
 * src/ns/cmd/winfe/fegui.cpp#XP_BackupFileName()
 * (I didn't copy all the Windows code which deals with 8.3 filenames.)
 */

char * XP_BackupFileName( const char * szURL )
{
    // Must have "file:" URL type and at least 1 character after "///"
	if ( szURL == NULL || !NET_IsLocalFileURL((char*)szURL) || XP_STRLEN(szURL) <= 8 )
		return NULL;

    // Add extra space for '\0' and '.BAK', but subtract space for "file:///"

	char *szFileName = (char *)XP_ALLOC((XP_STRLEN(szURL)+1+4-7)*sizeof(char));
	if ( szFileName == NULL )
		return NULL;

	// Get filename but ignore "file:///"

//	{
//		char* filename = WH_FileName(szURL+7, xpURL);
//		if (!filename) return NULL;
//		XP_STRCPY(szFileName,filename);
//		XP_FREE(filename);
//	}
	XP_STRCPY(szFileName, szURL+7);

    // Add extension to the filename
    XP_STRCAT( szFileName, ".BAK" );

    return szFileName;
}

   

// If pszLocalName is not NULL, we return the full pathname
//   in local platform syntax, even if file is not found.
//   Caller must free this string.
// Returns TRUE if file already exists
//

/*
 * I don't know what the logic here should be, so I mostly copied this from the Windows code in:
 * src/ns/cmd/winfe/fegui.cpp#XP_ConvertUrlToLocalFile()
 * (I didn't copy all the Windows code which deals with 8.3 filenames.)
 */

// The results of this call are passed directly to functions like XP_Stat and XP_FileOpen.
// brade--use xpURL format
Bool XP_ConvertUrlToLocalFile(const char * szURL, char **pszLocalName)		// return TRUE if the file exists!! or return FALSE;
{
    // Default assumes no file found - no local filename
    
    Boolean bFileFound = FALSE;
	if ( pszLocalName )
		*pszLocalName = NULL;

	// if "file:///Untitled" fail to convert
	if ( szURL && XP_STRCMP( szURL, XP_GetString(XP_EDIT_NEW_DOC_NAME) ) == 0 )
		return bFileFound;

	// Must have "file:" URL type and at least 1 character after "///"
	if ( szURL == NULL || !NET_IsLocalFileURL((char*)szURL) || XP_STRLEN(szURL) <= 8 )
		return FALSE;


    // Extract file path from URL: e.g. "/c|/foo/file.html"

	char *szFileName = NET_ParseURL( szURL, GET_PATH_PART);
	if (szFileName == NULL)
		return FALSE;

	// NET_UnEscape(szFileName);  This will be done in WH_FileName, so don't unescape twice.

	// Test if file exists
    XP_StatStruct statinfo; 
	if ( -1 != XP_Stat(szFileName, &statinfo, xpURL)			// if the file exists
		&& statinfo.st_mode & S_IFREG )							// and its a normal file
			bFileFound = TRUE;									// We found it!

	if ( pszLocalName )
	{
		// Pass string to caller
		*pszLocalName = WH_FileName(szFileName, xpURL);
		if (szFileName)
			XP_FREE( szFileName );
	}
	else 
		XP_FREE(szFileName);

	return bFileFound;
}
