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

#include <vector>

#include "CEditView.h"
#include "CHTMLView.h"			// ::SafeSetCursor

#include "resgui.h"
#include "mprint.h"
#include "meditor.h"			// HandleModalDialog
#include "meditdlg.h"			// CEditDialog
#include "CHTMLClickrecord.h"
#include "CBrowserContext.h"	// load url
#include "CPaneEnabler.h"		// CPaneEnabler::UpdatePanes()
#include "CPatternButtonPopup.h"
#include "CSpellChecker.h"		// cmd_CheckSpelling
#include "CComposerDragTask.h"
#include "CColorPopup.h"
#include "CFontMenuAttachment.h"	// CFontMenuPopup

#include "TSMProxy.h"
#include "LTSMSupport.h"

#include "macgui.h"				// UGraphics::MakeRGBColor
#include "ufilemgr.h"			// CFileMgr::GetURLFromFileSpec
#include "uerrmgr.h"			// ErrorManager::PlainAlert
#include "edt.h"
#include "net.h"				// FO_CACHE_AND_PRESENT
#include "proto.h"				// XP_IsContextBusy()
#include "CURLDispatcher.h"		// CURLDispatcher::DispatchURL
#include "pa_tags.h"			// tags such as P_HEADER_5
#include "ulaunch.h"			// StartDocInApp
#include "shist.h"				// SHIST_GetCurrent
#include "prefapi.h"			// PREF_GetBoolPref, PREF_GetIntPref
#include "CPrefsDialog.h"	// CPrefsDialog::EditPrefs and related
#include "CRecentEditMenuAttachment.h"	// cmd for FindCommandStatus

#include "FSpCompat.h"
#include "MoreFilesExtras.h"

#include "uintl.h"

// get string constants
#define WANT_ENUM_STRING_IDS
#include "allxpstr.h"
#undef WANT_ENUM_STRING_IDS

#include <LGAPopup.h>

#ifdef PROFILE
#include <profiler.h>
#endif

#include <Script.h>


#define FLUSH_JAPANESE_TEXT	if ( mProxy ) mProxy->FlushInput();

#pragma mark -- local --
// ** LOCAL functions
static Boolean 
sect_rect_long(SPoint32 frameLocation, SDimension16 frameSize, SPoint32 *updateUL,  SPoint32 *updateLR)
{
	if (updateUL->h >= frameLocation.h + frameSize.width ||			//	are we overlapping at all?
		updateLR->h <= frameLocation.h ||
		updateUL->v >= frameLocation.v + frameSize.height ||
		updateLR->v <= frameLocation.v)
	{
		*updateLR = *updateUL;										//	no,
		return false;												//	return.
	}
	
	if ( updateUL->h < frameLocation.h )
		updateUL->h = frameLocation.h;
	if ( updateLR->h > frameLocation.h + frameSize.width )
		updateLR->h = frameLocation.h + frameSize.width;
	if ( updateUL->v < frameLocation.v )
		updateUL->v = frameLocation.v;
	if ( updateLR->v > frameLocation.v + frameSize.height )
		updateLR->v = frameLocation.v + frameSize.height;
	
	return (updateLR->h != updateUL->h && updateLR->v != updateUL->v );		// is this test redundant; do we always return true??
}


// utility function for menu string/command swapping
// if "outName" is NULL it will actually set the menu text
// otherwise the menu item's text will be returned in "outName"
static
void SetMenuCommandAndString( CommandT oldCommand, CommandT newCommand, 
								ResIDT newStringResourceID, ResIDT newStringIndex, 
								Str255 outName )
{
	ResIDT		theMenuID;
	MenuHandle	theMenuH;
	Int16		theItem;
	LMenuBar	*theMenuBar = LMenuBar::GetCurrentMenuBar();
	
	if ( theMenuBar == NULL )
		return;		// something must be pretty screwed up!
	
	// Locate oldCommand
	theMenuBar->FindMenuItem( oldCommand, theMenuID, theMenuH, theItem );
	if ( theItem != 0 )
	{				// Replace with newCommand
		LMenu	*theMenu = theMenuBar->FetchMenu( theMenuID );
		if ( newCommand )
			theMenu->SetCommand( theItem, newCommand );
		
		if ( newStringResourceID )
		{
			Str255	newMenuString;
			::GetIndString( newMenuString, newStringResourceID, newStringIndex );
			if ( newMenuString[ 0 ] )
			{
				if ( outName )
				{
					outName[ 0 ] = 0;
					XP_STRCPY( (char *)outName, (char *)newMenuString );
				}
				else
					SetMenuItemText( theMenuH, theItem, newMenuString );
			}
		}
	}
}

#pragma mark -- CComposerAwareURLDragMixin

//
// CComposerAwareURLDragMixin constructor
//
// As the name implies, this version of the URLDragMixin class knows about composer.
// Add the composer native drag flavor to the front of the acceptable flavors
// list, because we want to use that first if it is present over all other drag flavors
//
CComposerAwareURLDragMixin :: CComposerAwareURLDragMixin ( )
{
	AcceptedFlavors().insert ( AcceptedFlavors().begin(), 1, (FlavorType)emComposerNativeDrag );

} // constructor


//
// ReceiveDragItem
//
// Overridden to handle the composer native drag flavor before all the others
//
void
CComposerAwareURLDragMixin :: ReceiveDragItem ( DragReference inDragRef, DragAttributes inDragAttrs,
												ItemReference inItemRef, Rect & inItemBounds,
												SPoint32 & inMouseLoc )
{
	try {
		FlavorType useFlavor;
		FindBestFlavor ( inDragRef, inItemRef, useFlavor );
		Size theDataSize = 0;
		
		switch ( useFlavor ) {
		
			case emComposerNativeDrag:
			{				
				SInt16 mods, mouseDownModifiers, mouseUpModifiers;
				OSErr err = ::GetDragModifiers( inDragRef, &mods, &mouseDownModifiers, &mouseUpModifiers );
				if (err != noErr)
					mouseDownModifiers = 0;
				bool doCopy = ( (mods & optionKey) == optionKey ) || ( (mouseDownModifiers & optionKey) == optionKey );

				err = ::GetFlavorDataSize( inDragRef, inItemRef, emComposerNativeDrag, &theDataSize );
				ThrowIfOSErr_( err );
	
				vector<char> datap ( theDataSize + 1 );
				if ( noErr == ::GetFlavorData( inDragRef, inItemRef, emComposerNativeDrag, datap.begin(), &theDataSize, 0 )
					&& ( theDataSize > 0 ) ) {
					datap[ theDataSize ] = 0;
					HandleDropOfComposerFlavor ( datap.begin(), doCopy, inMouseLoc );
				}
			}
			break;
	
			default:
				CHTAwareURLDragMixin::ReceiveDragItem(inDragRef, inDragAttrs, inItemRef, inItemBounds);
				break;
		
		} // switch on best flavor
	}
	catch ( ... ) {
		DebugStr ( "\pCan't find the flavor we want; g" );	
	}

} // ReceiveDragItem


#pragma mark -- CEditView --
CEditView::CEditView(LStream * inStream) : CHTMLView(inStream),
	mEditorDoneLoading( false ),
	mUseCharFormattingCache( false ),
	mDragData( nil ),
	mHoldUpdates( nil )
{
	// Editable views always have scrollbars, though they might be disabled.
	SetScrollMode( LO_SCROLL_YES, true );

	mLastBlink = 0;
	mHideCaret = false;
	mCaretDrawn = false;
	mCaretActive = false;
	mCaretX = 0;
	mCaretYLow = 0;
	mCaretYHigh = 0;
	mDisplayParagraphMarks = false;

	mProxy = NULL;
	Int32 theResult;
	::Gestalt( gestaltTSMgrAttr, &theResult );
	
	// Failure just means that this edit view won't support TSM
	OSErr theErr = ::Gestalt( gestaltTSMgrVersion, &theResult );
	if ( theErr == noErr && theResult >= 1 )
	{
		try
			{
				if ( LTSMSupport::HasTextServices() )
					mProxy = new HTMLInlineTSMProxy( *this );
			}
		catch (...)
			{
			}
	}

	mOldLastElementOver = -1;
	mOldPoint.h = -1;
	mOldPoint.v = -1;
}

CEditView::~CEditView()
{
	SetContext( NULL );	
	delete mProxy;
}

void CEditView::FinishCreateSelf(void)
{
	LView *view = GetSuperView();
	while (view->GetSuperView())
		view = view->GetSuperView();

	mParagraphToolbarPopup = (LGAPopup *)view->FindPaneByID( cmd_Paragraph_Hierarchical_Menu );
	mSizeToolbarPopup = (LGAPopup *)view->FindPaneByID( cmd_Font_Size_Hierarchical_Menu );
	mAlignToolbarPopup = (CPatternButtonPopup *)view->FindPaneByID( cmd_Align_Hierarchical_Menu );
	mFontToolbarPopup = (CFontMenuPopup *)view->FindPaneByID( 'Font' );
	mColorPopup = (CColorPopup *)view->FindPaneByID( 'Colr' );

	CHTMLView::FinishCreateSelf();
}

void CEditView::LayoutNewDocument( URL_Struct* inURL, Int32* inWidth, Int32* inHeight,
									Int32* inMarginWidth, Int32* inMarginHeight )
{
	/*
		The inherited |LayoutNewDocument| arbitrarily sets the scroll mode to LO_SCROLL_AUTO,
		which is wrong for editable views --- they always show their scrollbars.
	*/

	/*inherited*/ CHTMLView::LayoutNewDocument( inURL, inWidth, inHeight, inMarginWidth, inMarginHeight );
	SetScrollMode( LO_SCROLL_YES, true );
}

void CEditView::SetContext( CBrowserContext* inNewContext )
{
	// if we are setting this to a NULL context *and* we had a previous context...
	// assume we are shutting down window and destroy edit buffer
	if ( GetContext() && inNewContext == NULL )
		EDT_DestroyEditBuffer( *GetContext() );

	// remove and add ourselves (the editor) as a user of the context
	if ( GetContext() )
		GetContext()->RemoveUser( this );
	if ( inNewContext )
		inNewContext->AddUser( this );

	inherited::SetContext( inNewContext );
	
	if ( mProxy )
		mProxy->SetContext( *(inNewContext) );

}

Boolean	CEditView::CanUseCharFormatting()
{
	if ( ! mUseCharFormattingCache )
	{
		ED_ElementType elementType = EDT_GetCurrentElementType( *GetContext() );
		mIsCharFormatting = elementType == ED_ELEMENT_TEXT || elementType == ED_ELEMENT_SELECTION
							|| elementType == ED_ELEMENT_CELL || elementType == ED_ELEMENT_ROW
							|| elementType == ED_ELEMENT_COL; 
	}

	return mIsCharFormatting;
}

Bool CEditView::PtInSelectedRegion( SPoint32 cpPoint )
{
    Bool bPtInRegion = false;
    Int32 right, left, top, bottom;

    if ( EDT_IS_EDITOR( ((MWContext *)*GetContext()) ) && EDT_IsSelected( *GetContext() ) )
    {
    	int32 start_selection, end_selection;
	    LO_Element * start_element = NULL;
	    LO_Element * end_element = NULL;
	    CL_Layer *layer = NULL;
	    // Start the search from the current selection location	
	    LO_GetSelectionEndpoints( *GetContext(), 
	                         &start_element, 
	                         &end_element, 
	                         &start_selection, 
	                         &end_selection,
	                         &layer);
        
        if ( start_element == NULL )
            return false;
        
        // Do.. Until would be nice!
        for ( LO_Any_struct * element = (LO_Any_struct *)start_element; ;
				element = (LO_Any_struct *)(element->next) )
		{
			// Linefeed rect is from end of text to right ledge, so lets ignore it
			if ( element->type != LO_LINEFEED )
			{
				if ( element->type == LO_TEXT && 
                     (element == (LO_Any_struct*)start_element || 
                      element == (LO_Any_struct*)end_element) )
				{
					// With 1st and last text elements, we need to
					// account for character offsets from start or end of selection

					LO_TextStruct *text = (LO_TextStruct*)element;
            		left = element->x + element->x_offset;
                    top = element->y + element->y_offset;
                    right = left + element->width;
                    bottom = top + element->height;
                    bPtInRegion = cpPoint.h > left &&
                                  cpPoint.h < right &&
                                  cpPoint.v > top &&
                                  cpPoint.v < bottom;
                } 
                else
                {
                    // Get the rect surrounding selected element,
                    left = element->x + element->x_offset;
                    top = element->y + element->y_offset;
                    right = left + element->width;
                    bottom = top + element->height;
                    bPtInRegion = cpPoint.h > left &&
                                  cpPoint.h < right &&
                                  cpPoint.v > top &&
                                  cpPoint.v < bottom;
                }
            }
            // We're done if we are in a rect or finished with last element

			if ( bPtInRegion || element == (LO_Any_struct*)end_element || element->next == NULL )
			{
				break;
			}
        }
    }
    return bPtInRegion;
}

// used before editSource, reload, browse
Bool CEditView::VerifySaveUpToDate()
{
	if ( !EDT_DirtyFlag( *GetContext() ) && !EDT_IS_NEW_DOCUMENT( ((MWContext *)*GetContext()) ))
		return true;
	
	History_entry*	newEntry = SHIST_GetCurrent( &((MWContext *)*GetContext())->hist );
	CStr255 fileName;
	if ( newEntry && newEntry->address )
		fileName = newEntry->address;
				
	MessageT itemHit = HandleModalDialog( EDITDLG_SAVE_BEFORE_BROWSE, fileName,  NULL );
	if (itemHit != ok)
		return false;
	
	return SaveDocument();
}


// Call this to check if we need to force saving file
//   Conditions are New, unsaved document and when editing a remote file
// Returns true for all cases except CANCEL by the user in any dialog

Bool CEditView::SaveDocumentAs()
{
	History_entry * hist_ent = SHIST_GetCurrent( &((MWContext *)*GetContext())->hist );
	if ( hist_ent && hist_ent->address )
	{
		if ( XP_STRNCMP( hist_ent->address, "file", 4 ) != 0 && CPrefs::GetBoolean( CPrefs::ShowCopyright ) )
			{								// preferences
                // Show a "Hint" dialog to warn user about copyright 
                //   of downloaded documents or images

    			if ( HandleModalDialog( EDITDLG_COPYRIGHT_WARNING , NULL, NULL ) == 2 )
    			{
    				// set preference
    				CPrefs::SetBoolean( false, CPrefs::ShowCopyright );
    			}
            }

        if ( hist_ent->title && XP_STRCMP( hist_ent->title, hist_ent->address ) == 0 )
        {
            // If there is no title, it will be set to URL address
            ///  in CFE_FinishedLayout. It makes no sense to
            //  use this title when changing the filename, so destroy it
            XP_FREE( hist_ent->title );
            hist_ent->title = NULL;
            if ( ((MWContext *)*GetContext())->title )
            {
                XP_FREE( ((MWContext *)*GetContext())->title );
                ((MWContext *)*GetContext())->title = NULL;
            }
        }

		// Try to suggest a name if not a new doc or local directory
		CStr31 pSuggested;
		fe_FileNameFromContext( *GetContext(), hist_ent->address, pSuggested );
		
		char viewableName[32];
		strcpy( viewableName, pSuggested );
		NET_UnEscape(viewableName);	// Hex Decode
		pSuggested = viewableName;
	    
	    StandardFileReply sfReply;
		::UDesktop::Deactivate();
		::StandardPutFile( NULL, pSuggested, &sfReply );
		::UDesktop::Activate();

		if ( !sfReply.sfGood )
			return false;

  		char *pLocalName = CFileMgr::GetURLFromFileSpec( sfReply.sfFile );
		if ( pLocalName )
        {
			int16	saveCsid = (GetContext())->GetWinCSID();

			if (hist_ent->title == NULL)
			{
				CStr31 newName( sfReply.sfFile.name );
				StrAllocCopy( ((MWContext *)*GetContext())->title, newName );
				SHIST_SetTitleOfCurrentDoc( *GetContext() );
			}

			Bool bKeepImagesWithDoc = CPrefs::GetBoolean( CPrefs::PublishKeepImages ); 				// preferences
			Bool bAutoAdjustLinks = CPrefs::GetBoolean( CPrefs::PublishMaintainLinks );
	
			// ignore result
			EDT_SaveFile( *GetContext(), hist_ent->address,             // Source
                                         pLocalName,                    // Destination
                                         true,                          // Do SaveAs
                                         bKeepImagesWithDoc, bAutoAdjustLinks );

			(GetContext())->SetWinCSID(saveCsid);			
			
			XP_Bool doAutoSave;
			int32 iSave;
			PREF_GetBoolPref( "editor.auto_save", &doAutoSave );
			if ( doAutoSave )
				PREF_GetIntPref( "editor.auto_save_delay", &iSave );
			else
				iSave = 0;
			EDT_SetAutoSavePeriod( *GetContext(), iSave );
 			
            XP_FREE( pLocalName );
            return true;
        }

        // Clear the flag!
	}
    return true;
}


Bool CEditView::SaveDocument()
{
	History_entry * hist_ent = SHIST_GetCurrent(&(((MWContext *)*GetContext())->hist));
	if ( hist_ent && hist_ent->address )
	{
		char *szLocalFile = NULL;

		if ( !EDT_IS_NEW_DOCUMENT( ((MWContext *)*GetContext()) )
		&& XP_ConvertUrlToLocalFile( hist_ent->address, &szLocalFile ) )
		{
			Bool bKeepImagesWithDoc = CPrefs::GetBoolean( CPrefs::PublishKeepImages );
			Bool bAutoAdjustLinks = CPrefs::GetBoolean( CPrefs::PublishMaintainLinks );
		
			ED_FileError result = EDT_SaveFile( *GetContext(), hist_ent->address,	// Source
							            		hist_ent->address,  // Destination
							            		false,  // Not doing SaveAs
												bKeepImagesWithDoc, bAutoAdjustLinks );

			if ( result == ED_ERROR_NONE )
			{
				XP_Bool doAutoSave;
				int32 iSave;
				PREF_GetBoolPref( "editor.auto_save", &doAutoSave );
				if ( doAutoSave )
					PREF_GetIntPref( "editor.auto_save_delay", &iSave );
				else
					iSave = 0;
				EDT_SetAutoSavePeriod( *GetContext(), iSave );
            }
	
            if ( szLocalFile )
            	XP_FREE( szLocalFile );

            return true;
        }
        else
        {
            // Any URL that is NOT a file must do SaveAs to rename to a local file
            // (With current model, this only happens with new document,
            //  editing remote URLs immediately forces SaveAs locally)
            Boolean returnValue;
            returnValue = SaveDocumentAs();
            
            if ( returnValue )	// successfully saved
            	EDT_SetDirtyFlag( *GetContext(), false );

            return returnValue;
        }
	}
	
    return true;
}

URL_Struct *CEditView::GetURLForPrinting( Boolean& outSuppressURLCaption, MWContext *printingContext )
{
	// for the editor, we don't want to require saving the file if it's dirty or if
	// it has never been saved SO we save it to a temp file and we'll print that…
	MWContext *ourContext = *GetContext();
	if ( EDT_IS_EDITOR( ourContext ) 
	&& ( EDT_DirtyFlag( ourContext ) || EDT_IS_NEW_DOCUMENT( ourContext ) ) )
	{
		EDT_SaveToTempFile( *GetContext(), EditorPrintingCallback, printingContext );
		return NULL;
	}
	else
		return CHTMLView::GetURLForPrinting( outSuppressURLCaption, printingContext );
} 

void CEditView::DrawCaret( Boolean doErase )
{
	if ( mHideCaret )
		return;
	
	FocusDraw();
	
	// only draw caret if we are erasing it or if this view is the target of the window
	if ( IsTarget() || doErase )
	{
		StColorPenState theState;
		::PenNormal();
		::PenMode(srcXor);
		::MoveTo(mCaretX, mCaretYLow);
		::LineTo(mCaretX, mCaretYHigh - 1);		// only want to turn on pixels down to mCaretYHigh, not beyond

		mLastBlink = TickCount();
		mCaretDrawn = !mCaretDrawn;
	}
}


void CEditView::EraseCaret()
{
	if ( mCaretDrawn )
		DrawCaret( true );		// erase it if it's on the screen so we don't leave behind carets
}


void CEditView::PlaceCaret( int32 caretXi, int32 caretYLowi, int32 caretYHighi )
{
	if ( mCaretActive )
		RemoveCaret();			// did we forget to remove the last caret?! assert here?
	
	mCaretActive = false;
	
/*	This function takes the real position of a layout element,
	and returns the local position and whether it is visible.
	If it's not visible, the local position is not valid.
*/
	Point			portOrigin = {0, 0};
	SPoint32		frameLocation;
	SDimension16	frameSize;
	SPoint32		imageLocation;
	
	PortToLocalPoint( portOrigin );
	GetFrameLocation( frameLocation );
	GetFrameSize( frameSize );
	GetImageLocation( imageLocation );
	
	// convert local rectangle to image rectangle
	long realFrameLeft 		= frameLocation.h - imageLocation.h; // frameLocation.h - portOrigin.h - imageLocation.h;
	long realFrameRight 	= realFrameLeft + frameSize.width;
	long realFrameTop 		= frameLocation.v - imageLocation.v; // frameLocation.v - portOrigin.v - imageLocation.v;
	long realFrameBottom 	= realFrameTop + frameSize.height;
		
	// is it visible at all? If not, return
	if (realFrameRight <= caretXi || realFrameLeft >= caretXi 
			|| realFrameBottom <= caretYLowi ||	realFrameTop >= caretYHighi)
		return;
	
	mCaretActive = true;
	
	mCaretX = caretXi + portOrigin.h + imageLocation.h;
	mCaretYLow = caretYLowi + portOrigin.v + imageLocation.v;
	mCaretYHigh = mCaretYLow + caretYHighi - caretYLowi;
	
	DrawCaret( false );
	StartIdling();
}


void CEditView::RemoveCaret()
{
	mCaretActive = false;
	EraseCaret();
	StopIdling();
}


void CEditView::DisplayGenericCaret( MWContext *context, LO_Element * pLoAny, 
										ED_CaretObjectPosition caretPos )
{
	if ( !(FocusDraw()) )
		return;
	
	int32 xVal, yVal, yValHigh;

	GetCaretPosition( context, pLoAny, caretPos, &xVal, &yVal, &yValHigh );
	
	if ( context->is_editor )
		PlaceCaret( xVal, yVal, yValHigh );	
}


// forces a super-reload (not from cache)
void CEditView::DoReload( void )
{
	Try_	
	{
		History_entry * history = SHIST_GetCurrent( &((MWContext *)*GetContext())->hist );
		ThrowIfNil_(history);
		
		SPoint32	location;
		LO_Element*	element;
			
		// We need to set the location before creating the new URL struct
		GetScrollPosition( location );
	#ifdef LAYERS
		element = LO_XYToNearestElement( *GetContext(), location.h, location.v, NULL );
	#else
		element = LO_XYToNearestElement( *GetContext(), location.h, location.v );
	#endif // LAYERS
		if ( element )
			SHIST_SetPositionOfCurrentDoc( &((MWContext *)*GetContext())->hist, element->lo_any.ele_id );
		
		URL_Struct * url = SHIST_CreateURLStructFromHistoryEntry( *GetContext(), history );
		ThrowIfNil_(url);
		
		url->force_reload = NET_SUPER_RELOAD;
		mContext->ImmediateLoadURL( url, FO_CACHE_AND_PRESENT );
	}
	Catch_(inErr)
	{}EndCatch_
}

void CEditView::NoteEditorRepagination( void )
{
	SetFontInfo();

	LO_InvalidateFontData( (MWContext *)(*GetContext()) );
	EDT_RefreshLayout( (MWContext *)(*GetContext()) );
	AdjustScrollBars();
}


void CEditView::ActivateSelf()
{
	if ( mCaretActive )
		StartIdling();						// don't forget to restart our caret
	
    if ( EDT_IsFileModified( *GetContext() ))
    {
		History_entry*	newEntry = SHIST_GetCurrent( &((MWContext *)*GetContext())->hist );
		CStr255 fileName;
		if ( newEntry && newEntry->address )
			fileName = newEntry->address;
				
		if ( HandleModalDialog( EDITDLG_FILE_MODIFIED, fileName, NULL ) == ok )
			DoReload();
    }
}

void CEditView::GetDefaultBackgroundColor( LO_Color* outColor ) const
{
	// if we're not done loading the editor yet, 
	// the default color should be the same as the browser
	if ( !mEditorDoneLoading )
	{
		uint8 red, green, blue;
		int result = PREF_GetColorPref( "editor.background_color", &red, &green, &blue );
		if ( PREF_NOERROR == result )
		{
			outColor->red = red;
			outColor->green = green;
			outColor->blue = blue;
		}
		else
			CHTMLView::GetDefaultBackgroundColor( outColor );
	}
}


void CEditView:: DeactivateSelf()
{
	FocusDraw();
	
/*	if (midocID)
	{
		OSErr	err = ::FixTSMDocument(midocID);
		ThrowIfOSErr_(err);
	}
*/
	if ( mCaretActive )
	{
		StopIdling();						// don't forget to stop caret
		EraseCaret();						// oh yeah, and get rid of it if it is on the screen so we don't leave a caret
	}
}

// XXX CALLBACK

void CEditView::SetDocPosition( int inLocation, Int32 inX, Int32 inY, Boolean inScrollEvenIfVisible )
{
	// Make sure any pending updates are handled at correct origin
	UpdatePort();
	CHTMLView::SetDocPosition( inLocation, inX, inY, inScrollEvenIfVisible );
}

/*
If we are hilighted as the pane being dragged "into", we want to first hide the hilite
before drawing then draw it back.  This is because we have an offscreen view which 
messes up the drag hilite-ing.
*/
void CEditView::DrawSelf()
{
	EraseCaret();							// remove caret if it is showing so we don't leave it behind
	
	if ( mIsHilited )
		::HideDragHilite(mDragRef);
	
	CHTMLView::DrawSelf();
	
	if ( mIsHilited )
		HiliteDropArea(mDragRef);
}


/*
Keep track of the drag reference number so we can re-hilite the drag area manually in DrawSelf.
Also, do our superclass's LDropArea::EnterDropArea behavior
*/
void CEditView::EnterDropArea( DragReference inDragRef, Boolean inDragHasLeftSender )
{
	if ( inDragHasLeftSender )
		mDragRef = inDragRef;

	LDropArea::EnterDropArea( inDragRef, inDragHasLeftSender );
}

// This method gets called when window changes size.
// Make sure we DO NOT call mContext->Repaginate() like CHTMLView does
void CEditView::AdaptToSuperFrameSize( Int32 inSurrWidthDelta,
									Int32 inSurrHeightDelta, Boolean inRefresh )
{
	LView::AdaptToSuperFrameSize( inSurrWidthDelta, inSurrHeightDelta, inRefresh );
	if (mContext)
		EDT_RefreshLayout( *GetContext() );
}


#if 0
void CEditView::SetDocDimension( int /* ledge */, Int32 inWidth, Int32 inHeight )		// this overrides the SetDocDimension which does cacheing.
{
	SDimension32 theImageSize;
	GetImageSize(theImageSize);
	
	Int32 theWidthDelta = inWidth - theImageSize.width;
	Int32 theHeightDelta = inHeight - theImageSize.height;

	if ((abs(theWidthDelta) > 200) || (abs(theHeightDelta) > 200))
		{
		mCachedImageSizeDelta.width = 0;
		mCachedImageSizeDelta.height = 0;
		SetDocDimensionSelf(theWidthDelta, theHeightDelta);
		}
	else
		{
		mCachedImageSizeDelta.width = theWidthDelta;
		mCachedImageSizeDelta.height = theHeightDelta;
		}
}
#endif


void CEditView::GetDocAndWindowPosition( SPoint32 &frameLocation, SPoint32 &imageLocation, SDimension16 &frameSize )
{
	// Make sure we're all talking about the same thing.
	FocusDraw();

	// Get the frame and image rectangles...
	GetFrameLocation( frameLocation );
	GetImageLocation( imageLocation );
	GetFrameSize( frameSize );
}


void CEditView::CreateFindWindow()
{
	LWindow::CreateWindow( 5290, LCommander::GetTopCommander() );
}


void CEditView::DisplaySubtext(
	int 					inLocation,
	LO_TextStruct*			inText,
	Int32 					inStartPos,
	Int32					inEndPos,
	XP_Bool 				inNeedBG)
{
	if ( mHoldUpdates )
		return;
		
	inherited::DisplaySubtext(inLocation, inText, inStartPos, inEndPos, inNeedBG);
}

void CEditView::EraseBackground(
	int						inLocation,
	Int32					inX,
	Int32					inY,
	Uint32					inWidth,
	Uint32					inHeight,
	LO_Color*				inColor)
{
	if (mHoldUpdates)
		return;
		
	inherited::EraseBackground(inLocation, inX, inY, inWidth, inHeight, inColor);
}


void CEditView::DocumentChanged( int32 iStartY, int32 iHeight )
{
	SPoint32		frameLocation, imageLocation, updateUL, updateLR;
	SDimension16	frameSize;
	SDimension32	imageSize;
	
	// document may have grown but not enough to trigger SetDocDimension so force resize
	FlushPendingDocResize();
	
	if ( mHoldUpdates )
	{
		mHoldUpdates->DocumentChanged( iStartY, iHeight );
		return;
	}
		
	// Make sure we're all talking about the same thing.
	FocusDraw();

	// Get the frame rectangle in port coordinates
	GetFrameLocation( frameLocation );			// wimpy 16 bit coordinates
	GetFrameSize( frameSize );
	
	// Get the image rectangle in port coordinates
	GetImageLocation( imageLocation );			// massive 32 bit coordinates
	GetImageSize( imageSize );
	
	// convert the frame rectangle to image coordinates					// massive 32 bit coordinates
	frameLocation.h = frameLocation.h - imageLocation.h;
	frameLocation.v = frameLocation.v - imageLocation.v;
		
	// Get the region to update in image coordinates.
	updateUL.h = 0;		// sorry to say, the horizontal will be ignored later and set to the whole frame width
	updateUL.v = iStartY;
	
	// we want to update all the way to right side of the frame (no matter how narrow or wide the image might be..)
	updateLR.h = frameLocation.h + frameSize.width;		// sorry to say, the horizontal will be ignored later and set to the whole frame width
		
	// if iHeight is -1, then we want to invalidate to the end of the image or the frame (whichever is lower)
	// however, we are going to clip to the frame later anyway, so just invalidate to the end of the frame.
	if (iHeight == -1)
		updateLR.v = frameLocation.v + frameSize.height;
	else
		updateLR.v = iStartY + iHeight;

	// Intersect the update rectangle with the frame rectangle and make sure they overlap (in image coordinates)
	if ( sect_rect_long( frameLocation, frameSize, &updateUL, &updateLR ) )
	{
		Rect	updateRect;
		Point	ul, lr;
		
		// get the frame rectange in port coordinates again.
		CalcPortFrameRect( updateRect );
		
		// Convert from image to local to port coordinates... we don't have to worry about the conversion because we've already been clipped to the frame
		ImageToLocalPoint( updateUL, ul );
		ImageToLocalPoint( updateLR, lr );
		LocalToPortPoint( ul );
		LocalToPortPoint( lr );
		
		updateRect.top = ul.v;
		updateRect.bottom = lr.v;
		
		EraseCaret();			// erase it if it's on the screen so we don't show the caret. This should be covered by DrawSelf...
// There is a problem with the last line because Idling is still on and we may just redraw the caret before the InvalRect updates the screen.
// The next two lines are a hack. They cause the updated region to be redrawn immediately,
// this is necessary because EDT_Paste and other functions may cause an update event and then a scroll
// before allowing the updated region to be redrawn (in its old location).
// In other words, when the invalid text is scrolled out from under the update region, it is never updated.
		InvalPortRect( &updateRect );
		UpdatePort();
	}
}


void CEditView::SpendTime( const EventRecord& inMacEvent )
{
	if ( mCaretActive )
		if ( TickCount() > mLastBlink + ::LMGetCaretTime() )
			DrawCaret( false );
	
	CHTMLView::SpendTime( inMacEvent );
}

void CEditView::BeTarget()
{
	FocusDraw();
	
	inherited::BeTarget();
	
	if ( mProxy )
		mProxy->Activate();

}

void CEditView::DontBeTarget()
{
	FocusDraw();
	
	try
		{

			if ( mProxy )
				mProxy->Deactivate();

		}
	catch (...)
		{
		}

	inherited::DontBeTarget();
}


/*
IsPastable shows whether or not a character can be pasted in.  Note that right now Tab cannot be
pasted in, even though it is handled in EDT_PasteText().  This is because EDT_PasteText()'s
tab implementation is broken, inserting only one space.

Note: Methods HandleKeyPress and IsPastable are dependant on each other because IsPastable
returns false for all keystrokes that HandleKeyPress handles other than the default 
EDT_KeyDown.  Specifically, in HandleKeyPress there is a switch statement based on
"theChar & charCodeMask".  If additional case statements are added to this switch which
make it inappropriate to call EDT_KeyDown we may have to add an additional case statement
in IsPastable which returns false.

*/
Boolean CEditView::IsPastable(Char16 theChar) 
{
	switch ( theChar & charCodeMask ) 
	{
		case char_LeftArrow:
		case char_RightArrow:
		case char_UpArrow:
		case char_DownArrow:
		case char_Backspace:
		case char_FwdDelete:
		case char_Return:
		case char_Enter:
		case char_Tab:
		case char_Home:
		case char_End:
		case char_PageUp:
		case char_PageDown:
			return false;
		default:
			return true;
	}
}


/*
This function looks ahead into the Event Manager queue.  It finds all of the keyDown and keyUp events.
The keyUp events it ignores.  The keyDown events, if they are "normal" keystrokes, puts into the 
string keys_in_q.  This is then used by HandleKeyPress so that since it's already handling a key press,
why not handle the rest also.  

Unfortunately, this is a bit of a hack.  Note especially that ::EventAvail() is used to check to make sure
that the event fits criteria and then ::GetNextEvent() gets the event.  If inbetween calling these two
calls the Event Queue's structure is changed, this could cause inappropriate behavior.
*/
int CEditView::FindQueuedKeys(char *keys_in_q) 
{
  	int curr_q_num;
  	EventRecord currEvent, eventToRemove;
	
	curr_q_num = 0;
	
	/* Keep getting events while:
	1) We have room in our buffer
	2) There are events in Event Manager Queue
	3) Either we get a keyUp, or we get a key down that can be pasted	
	*/ 


	Boolean foundAndClearedEvent;
	while ( (curr_q_num < (MAX_Q_SIZE-2)) && ::EventAvail( everyEvent, &currEvent ) )
	{
		if ( (currEvent.what == keyDown) 	// its a backspace-keydown
		&& !((cmdKey | optionKey | controlKey) & currEvent.modifiers) // with no modKeys except maybe shift
		&& ( IsPastable( static_cast<Char16>(currEvent.message & 0xFFFF) )) )
		{
        	keys_in_q[curr_q_num + 1] = static_cast<char>(currEvent.message & 0xFF);
			++curr_q_num;
		}
		else if ( currEvent.what != keyUp ) // its _not_ a keyup; bail
			break; 							// keyups don't stop us, everything else does
		
		foundAndClearedEvent = ::GetNextEvent( keyDownMask | keyUpMask, &eventToRemove );
		XP_ASSERT( foundAndClearedEvent );
		if ( !foundAndClearedEvent )		// something bad must have happened; bail!
			break;
	}
					
	return curr_q_num;												
}

/*
Note: Methods HandleKeyPress and IsPastable are dependant on each other because IsPastable
returns false for all keystrokes that HandleKeyPress handles other than the default 
EDT_KeyDown.  Specifically, in HandleKeyPress there is a switch statement based on
"theChar & charCodeMask".  If additional case statements are added to this switch which
make it inappropriate to call EDT_KeyDown we may have to add an additional case statement
in IsPastable which returns false.
*/
Boolean CEditView::HandleKeyPress( const EventRecord& inKeyEvent )
{
	int lNumQKeys;
	char keys_to_type[MAX_Q_SIZE];
	
	if ( !IsDoneLoading() )
		return true;
	
#ifdef PROFILE
//	ProfilerSetStatus( true );
#endif
	
	::ObscureCursor();
				
	Char16		theChar = inKeyEvent.message & charCodeMask;
	short 		modifiers = inKeyEvent.modifiers & (cmdKey | shiftKey | optionKey | controlKey);
	URL_Struct *request = nil;
	
	Boolean		handled = false;
	Boolean		isNormalKeyPress = false;	// whether or not to update buttons
	Boolean		shiftKeyPressed = ((modifiers & shiftKey) == shiftKey);
	
	if ( (modifiers & cmdKey) == 0)	{		// we don't do command keys. But we do everything else...
		switch ( theChar )
		{
		
	// • Navigation Keys •
	
			case char_LeftArrow:
				if (modifiers & optionKey)
					EDT_PreviousWord( *GetContext(), shiftKeyPressed );
				else
					EDT_PreviousChar( *GetContext(), shiftKeyPressed );
				handled = true;
				break;

			case char_RightArrow:
				if (modifiers & optionKey)
					EDT_NextWord( *GetContext(), shiftKeyPressed );
				else
					EDT_NextChar( *GetContext(), shiftKeyPressed );
				handled = true;
				break;

			case char_UpArrow:
				if (modifiers & optionKey)
					EDT_PageUp( *GetContext(), shiftKeyPressed );
				else
					EDT_Up( *GetContext(), shiftKeyPressed );
				handled = true;
				break;

			case char_DownArrow:
				if (modifiers & optionKey)
					EDT_PageDown( *GetContext(), shiftKeyPressed );
				else
					EDT_Down( *GetContext(), shiftKeyPressed );
				handled = true;
				break;

			
			case 0xCA:			// space !!
				if (modifiers & optionKey)
				{
					EDT_InsertNonbreakingSpace( *GetContext() );
				}
				else
				{
					EDT_KeyDown( *GetContext(), theChar, 0, 0 );
				}
				handled = true;
				break;

			
	// • Deletion Keys •

			case char_Backspace:
				// if no existing selection, look for consecutive backspaces
				if ( !EDT_IsSelected( *GetContext() ) )
				{
  					EventRecord currEvent, eventToRemove;
  					int count = 1;	// include current keypress in count
  					Boolean foundAndClearedEvent;

					while ( ::EventAvail( everyEvent, &currEvent ) )
					{
						if ( (currEvent.what == keyDown) 	// its a backspace-keydown
						&& !((cmdKey | optionKey | controlKey) & currEvent.modifiers) // with no modKeys except maybe shift
						&& ( (static_cast<Uchar>(currEvent.message & charCodeMask)) == char_Backspace ) )
							++count;
						else if ( currEvent.what != keyUp ) // its _not_ a keyup; bail
							break; 							// keyups don't stop us, everything else does
						
						foundAndClearedEvent = ::GetNextEvent( keyDownMask | keyUpMask, &eventToRemove );
						XP_ASSERT( foundAndClearedEvent );
						if ( !foundAndClearedEvent )		// something bad must have happened; bail!
							break;
					}
					
					if ( count > 1 )	// more than just current event?
					{
						while ( count > 0 )
						{
							EDT_PreviousChar( *GetContext(), true );
							--count;
						}
					}
				}

				EDT_DeletePreviousChar( *GetContext() );
				handled = true;
				break;

			case char_FwdDelete:
				EDT_DeleteChar( *GetContext() );
				handled = true;
				break;


	// • Action Keys •

			case char_Return:
				if ( (modifiers & optionKey) || (modifiers & shiftKey) )
				{
					if ( EDT_GetCurrentElementType( *GetContext() ) == ED_ELEMENT_TEXT )
						EDT_InsertBreak( *GetContext(), ED_BREAK_NORMAL );
				}
				else
				{
					EDT_ReturnKey( *GetContext() );
				}
				handled = true;
				break;

			case char_Enter:
				if (modifiers & optionKey)
				{
					EDT_ReturnKey( *GetContext() );
				} 
				else
				{
					if ( EDT_GetCurrentElementType( *GetContext() ) == ED_ELEMENT_TEXT )
						EDT_InsertBreak( *GetContext(), ED_BREAK_NORMAL );
				}			
				handled = true;
				break;

			
			case char_Tab:
				EDT_TabKey( *GetContext(), !shiftKeyPressed, modifiers & optionKey );
#if 0
				if (modifiers & optionKey)
					EDT_Outdent( *GetContext() );
				else
					EDT_Indent( *GetContext() );
#endif
				handled = true;
				break;

			case char_Home:
				EDT_BeginOfDocument( *GetContext(), shiftKeyPressed );
				handled = true;
				break;
			
			case char_End:
				EDT_EndOfDocument( *GetContext(), shiftKeyPressed );
				handled = true;
				break;
			
			case char_PageUp:
				EDT_PageUp( *GetContext(), shiftKeyPressed );
				handled = true;
				break;
			
			case char_PageDown:
				EDT_PageDown( *GetContext(), shiftKeyPressed );
				handled = true;
				break;
						
			default:
				// normal if nothing was selected (no character deletions) and file already marked dirty
				isNormalKeyPress = !EDT_IsSelected( *GetContext() ) && EDT_DirtyFlag( *GetContext() );
				
				lNumQKeys = FindQueuedKeys(keys_to_type);
				if (lNumQKeys > 0) 
				{
					keys_to_type[0] = theChar;
					keys_to_type[lNumQKeys + 1] = '\0';
					if ( (GetWinCSID() == CS_UTF8) || (GetWinCSID() == CS_UTF7) )
					{
						INTL_Encoding_ID scriptCSID = ScriptToEncoding( ::GetScriptManagerVariable( smKeyScript ) );
			            unsigned char* unicodestring = INTL_ConvertLineWithoutAutoDetect( scriptCSID, CS_UTF8, (unsigned char *)keys_to_type, lNumQKeys);
			            if ( unicodestring )
			            {
			                EDT_InsertText( *GetContext(), (char *)unicodestring ); 
			                XP_FREE( unicodestring );
			            }
					}
					else
						EDT_InsertText( *GetContext(), keys_to_type );
				}
				else 
				{
					if ( (GetWinCSID() == CS_UTF8) || (GetWinCSID() == CS_UTF7) )
					{
					    unsigned char characterCode[2];
			            characterCode[0] = (char) theChar;
			            characterCode[1] = '\0';
						INTL_Encoding_ID scriptCSID = ScriptToEncoding( ::GetScriptManagerVariable( smKeyScript ) );
			            unsigned char* t_unicodestring = INTL_ConvertLineWithoutAutoDetect( scriptCSID, CS_UTF8, characterCode, 1);
			            if ( t_unicodestring )
						{
							EDT_InsertText( *GetContext(), (char *)t_unicodestring ); 
							XP_FREE( t_unicodestring );
			            }
					}
					else
						EDT_KeyDown( *GetContext(), theChar, 0, 0 );
				}
				handled = true;
				break;
		}
	} 
	else		// command key
	{
		switch ( theChar )
		{
	// • Navigation Keys •
			case char_LeftArrow:
				EDT_BeginOfLine( *GetContext(), shiftKeyPressed );
				handled = true;
				break;

			case char_RightArrow:
				EDT_EndOfLine( *GetContext(), shiftKeyPressed );
				handled = true;
				break;

			case char_UpArrow:
				EDT_BeginOfDocument( *GetContext(), shiftKeyPressed );
				handled = true;
				break;

			case char_DownArrow:
				EDT_EndOfDocument( *GetContext(), shiftKeyPressed );
				handled = true;
				break;
		}
	}

	// update toolbar on navigation moves and when there is a selection
	if ( handled && !isNormalKeyPress )
	{
		StUseCharFormattingCache stCharFormat( *this );
		
		// force menu items to update (checkMarks and text (Undo/Redo) could change)
		LCommander::SetUpdateCommandStatus( true );
//		CPaneEnabler::UpdatePanes();
	}
	
	if ( !handled )
		handled = CHTMLView::HandleKeyPress(inKeyEvent);
	
#ifdef PROFILE
//	ProfilerSetStatus( false );
#endif

	return handled;
}

static
long GetAlignmentMenuItemNum( MWContext *mwcontext, Boolean& outEnabled )
{
	if ( mwcontext == NULL )
	{
		outEnabled = false;
		return 0;
	}
	
	ED_ElementType elementtype;
	elementtype = EDT_GetCurrentElementType( mwcontext );

	outEnabled = ( elementtype != ED_ELEMENT_UNKNOWN_TAG );
	if ( outEnabled )
	{
		ED_Alignment alignment;
		long itemNum;

		if ( elementtype == ED_ELEMENT_HRULE )
		{
			EDT_HorizRuleData *h_data;
			h_data = EDT_GetHorizRuleData( mwcontext );
			if ( h_data )
			{
				alignment = h_data->align;
				EDT_FreeHorizRuleData( h_data );
			}
			else
				alignment = ED_ALIGN_DEFAULT;
		}
		else	/* For Images, Text, or selection, this will do all: */
			alignment = EDT_GetParagraphAlign( mwcontext );

		switch( alignment )
		{
			case ED_ALIGN_DEFAULT:
			case ED_ALIGN_LEFT:		itemNum = 1;	break;
			case ED_ALIGN_ABSCENTER:
			case ED_ALIGN_CENTER:	itemNum = 2;	break;
			case ED_ALIGN_RIGHT:	itemNum = 3;	break;
			default:
				itemNum = 0;
		}
		
		Assert_(itemNum <= 3 || itemNum >= 0);
		
		outEnabled = ( itemNum != 0 );
		return itemNum;
	}
	
	return 0;
}

static
long GetFormatParagraphPopupItem( MWContext *mwcontext, Boolean& outEnabled, Char16& outMark )
{
	if ( mwcontext == NULL )
	{
		outEnabled = false;
		return 0;
	}
	
	long itemNum;
	TagType paragraph_type = EDT_GetParagraphFormatting( mwcontext );
	switch( paragraph_type )
	{
		case P_NSDT:
		case P_UNKNOWN:
		case P_PARAGRAPH:	itemNum = 1;	break;
		case P_HEADER_1:	itemNum = 2;	break;
		case P_HEADER_2:	itemNum = 3;	break;
		case P_HEADER_3:	itemNum = 4;	break;
		case P_HEADER_4:	itemNum = 5;	break;
		case P_HEADER_5:	itemNum = 6;	break;
		case P_HEADER_6:	itemNum = 7;	break;
		case P_ADDRESS:		itemNum = 8;	break;
		case P_PREFORMAT:	itemNum = 9;	break;
		case P_LIST_ITEM:	itemNum = 10;	break;
		case P_DESC_TITLE:	itemNum = 11;	break;
		case P_DESC_TEXT:	itemNum = 12;	break;
		default:
			itemNum = 0;
	}
		
	outMark = checkMark;
	XP_ASSERT( itemNum <= 12 && itemNum >= 0 );
	
	outEnabled = ( itemNum != 0 );

	return itemNum;
}

void CEditView::FindCommandStatus( CommandT inCommand, Boolean& outEnabled, 
						Boolean& outUsesMark, Char16& outMark, Str255 outName )
{
	outUsesMark = false;
	outEnabled = false;
	EDT_CharacterData* better;			// used by a bunch of cases.
	short index;
	Boolean	hasBroadcasting;
	
	if ( ::StillDown() && IsDoneLoading() )
	{
		if ( FindCommandStatusForContextMenu( inCommand, outEnabled,outUsesMark, outMark, outName ) )
			return;
	}
	
	// if we haven't finished initializing yet very few commands should be enabled
	switch ( inCommand )
	{
		case cmd_AddToBookmarks:
			if ( !IsDoneLoading() )
				return;
	
			// don't add history entries == "file:///Untitled" (unsaved editor windows)
			if ( !mContext )
				break;
			History_entry *histentry = mContext->GetCurrentHistoryEntry();
			if ( histentry && histentry->address )
			{
				if ( 0 != XP_STRCMP( histentry->address, XP_GetString(XP_EDIT_NEW_DOC_NAME) ) )
					outEnabled = true;
			}
			break;
		
		case cmd_CheckSpelling:
		case cmd_Print:
		case cmd_ViewSource:
		case cmd_Publish:
		case cmd_Refresh:
		case cmd_Format_Document:
		case cmd_Format_PageTitle:
		case cmd_Format_FontHierMenu:
			outEnabled = IsDoneLoading();
			break;
		
		case 'Font':
			outUsesMark = outEnabled = IsDoneLoading() && CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better && better->pFontFace 
				&& mFontToolbarPopup && mFontToolbarPopup->IsEnabled() )
				{
					outMark = checkMark;
					
					// turn off broadcasting so we don't apply it here!
					hasBroadcasting = mFontToolbarPopup->IsBroadcasting();
					if ( hasBroadcasting )
						mFontToolbarPopup->StopBroadcasting();
					
					// get font menu item
					int		menuItemNum;
					Str255	fontItemString;
					LMenu *ppmenu = mFontToolbarPopup->GetMenu();
					MenuHandle menuh = ppmenu ? ppmenu->GetMacMenuH() : NULL;
					for ( menuItemNum = 0; menuItemNum < ::CountMenuItems( menuh ); menuItemNum++ )
					{
						fontItemString[ 0 ] = 0;
						::GetMenuItemText ( menuh, menuItemNum, fontItemString );
						p2cstr( fontItemString );
						if ( XP_STRLEN((char *)fontItemString) > 0 
						&& XP_STRSTR( better->pFontFace, (char *)fontItemString ) != NULL )
							break;
					}

					mFontToolbarPopup->SetValue( menuItemNum );
					
					// resume broadcasting
					if ( hasBroadcasting )
						mFontToolbarPopup->StartBroadcasting();
				}
				if ( better )
					EDT_FreeCharacterData( better );
			}
			break;
		
		case cmd_Font_Size_Hierarchical_Menu:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better && mSizeToolbarPopup && mSizeToolbarPopup->IsEnabled() )
				{
					outMark = checkMark;
					XP_ASSERT( better->iSize <= 8 || better->iSize >= 1 );
					
					// turn off broadcasting so we don't apply it here!
					hasBroadcasting = mSizeToolbarPopup->IsBroadcasting();
					if ( hasBroadcasting )
						mSizeToolbarPopup->StopBroadcasting();
					
					mSizeToolbarPopup->SetValue( better->iSize );	// iSize is the menu itemNum not the size
					
					// resume broadcasting
					if ( hasBroadcasting )
						mSizeToolbarPopup->StartBroadcasting();
				}
				if ( better )
					EDT_FreeCharacterData( better );
			}
			break;

		case cmd_Align_Hierarchical_Menu:
			if ( !IsDoneLoading() )
				return;
	
			long alignItemNum;
			alignItemNum = GetAlignmentMenuItemNum( *GetContext(), outEnabled );
			if ( outEnabled && mAlignToolbarPopup && mAlignToolbarPopup->IsEnabled() )
			{
				// turn off broadcasting so we don't apply it here!
				hasBroadcasting = mAlignToolbarPopup->IsBroadcasting();
				if ( hasBroadcasting )
					mAlignToolbarPopup->StopBroadcasting();
					
				mAlignToolbarPopup->SetValue( alignItemNum );

				// resume broadcasting
				if ( hasBroadcasting )
					mAlignToolbarPopup->StartBroadcasting();
			}
			break;
			
		case cmd_Paragraph_Hierarchical_Menu:
			if ( !IsDoneLoading() )
				return;
	
			long formatItemNum;
			formatItemNum = GetFormatParagraphPopupItem( *GetContext(), outEnabled, outMark );
			if ( outEnabled && mParagraphToolbarPopup && mParagraphToolbarPopup->IsEnabled() )
			{
				hasBroadcasting = mParagraphToolbarPopup->IsBroadcasting();
				if ( hasBroadcasting )
					mParagraphToolbarPopup->StopBroadcasting();
					
				mParagraphToolbarPopup->SetValue( formatItemNum );

				// resume broadcasting
				if ( hasBroadcasting )
					mParagraphToolbarPopup->StartBroadcasting();
			}
			break;
		
		case cmd_Reload:
			outEnabled = IsDoneLoading() && !EDT_IS_NEW_DOCUMENT( ((MWContext *)*GetContext()) );
			break;
		
		case cmd_EditSource:
			{
			if ( !IsDoneLoading() )
				return;
	
			// only enable "Edit Source" if it's a local file (so we can make an FSSpec; possibly also backend reasons)
			History_entry *histentry = mContext->GetCurrentHistoryEntry();
			outEnabled = histentry && histentry->address && ( 0 == XP_STRNCMP( histentry->address, "file", 4 ) );
			}
			break;
		
		case cmd_Remove_Links:
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = EDT_SelectionContainsLink( *GetContext() );
			break;
		
#if 0
		case cmd_DisplayTables:
			outUsesMark = outEnabled = true;
			outMark = EDT_GetDisplayTables( *GetContext() ) ? checkMark : 0;
			break;
#endif
		
		case cmd_Format_Target:
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = ED_ELEMENT_TARGET == EDT_GetCurrentElementType( *GetContext() );
			break;
		
		case cmd_Format_Unknown_Tag:
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = ED_ELEMENT_UNKNOWN_TAG == EDT_GetCurrentElementType( *GetContext() );
			break;
		
		case cmd_Format_DefaultFontColor:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					if ( (better->mask & TF_FONT_COLOR) && !(better->values & TF_FONT_COLOR) )
						outMark = checkMark;
					if ( !(better->mask & TF_FONT_COLOR) )
						outMark = dashMark;
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
//Table support
		case cmd_Delete_Table:
		case cmd_Select_Table:
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = EDT_IsInsertPointInTable( *GetContext() );
			break;
		
		case cmd_Format_Table:
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = ( EDT_IsInsertPointInTableCell( *GetContext() )
				|| EDT_IsInsertPointInTableRow( *GetContext() )
				|| EDT_IsInsertPointInTable( *GetContext() ) );
			break;
		
		case cmd_Insert_Row:
		case cmd_Insert_Cell:
		case cmd_Format_Row:
		case cmd_Delete_Row:
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = EDT_IsInsertPointInTableRow( *GetContext() );
			break;
		
		case cmd_Insert_Col:
		case cmd_Delete_Col:
		case cmd_Delete_Cell:
		case cmd_Format_Cell:
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = EDT_IsInsertPointInTableCell( *GetContext() );
			break;
		
#if 0
		case cmd_DisplayTableBoundaries:
			outEnabled = true;
			Boolean areTableBordersVisible;
			areTableBordersVisible = EDT_GetDisplayTables( *GetContext() );
			index = (areTableBordersVisible) ? EDITOR_MENU_HIDE_TABLE_BORDERS : EDITOR_MENU_SHOW_TABLE_BORDERS;
			::GetIndString( outName, STRPOUND_EDITOR_MENUS, index );
			break;
#endif
		
		case cmd_DisplayParagraphMarks:
			outEnabled = true;
			index = mDisplayParagraphMarks ? EDITOR_MENU_HIDE_PARA_SYMBOLS 
											: EDITOR_MENU_SHOW_PARA_SYMBOLS;
			::GetIndString( outName, STRPOUND_EDITOR_MENUS, index );
			break;
		
		case cmd_Cut:
		case cmd_Clear:
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = LO_HaveSelection( *GetContext() )
						&& EDT_CanCut( *GetContext(), true ) == EDT_COP_OK;
			break;
		
		case cmd_Copy:
			outEnabled = IsDoneLoading() && LO_HaveSelection( *GetContext() )
						&& EDT_CanCopy( *GetContext(), true ) == EDT_COP_OK;
			break;
		
		case cmd_Paste:
			if ( !IsDoneLoading() )
				return;
	
			Int32	offset;
			outEnabled = EDT_CanPaste( *GetContext(), true ) == EDT_COP_OK
						&& (::GetScrap( NULL, 'TEXT', &offset ) > 0)
							|| (::GetScrap( NULL, 'EHTM', &offset ) > 0)
							|| (::GetScrap( NULL, 'PICT', &offset ) > 0);
			break;

		case cmd_SelectAll:
			outEnabled = IsDoneLoading();
			break;
		
		case cmd_BrowseDocument:
			if ( !IsDoneLoading() )
				return;
	
			if ( !XP_IsContextBusy( *GetContext() ) &&
					!Memory_MemoryIsLow())
				outEnabled = true;
			break;
		
		case cmd_Save:		// only enable if file is dirty or new
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = ( EDT_DirtyFlag( *GetContext() ) 
						|| EDT_IS_NEW_DOCUMENT( ((MWContext *)*GetContext()) ) );
			break;
		
		case CRecentEditMenuAttachment::cmd_ID_toSearchFor:
			outEnabled = true;
			break;
		
		case cmd_SaveAs:
		case cmd_InsertEdit_Target:
		case cmd_InsertEditLink:
		case cmd_InsertEditImage:
		case cmd_InsertEditLine:
		case cmd_Insert_Table:
		case cmd_Insert_Target:
		case cmd_Insert_Unknown_Tag:
		case cmd_Insert_Link:
		case cmd_Insert_Image:
		case cmd_Insert_Line:
		case cmd_Insert_Object:
		case cmd_FormatColorsAndImage:
			outEnabled = IsDoneLoading();
			break;
		
		
#if 0
		case cmd_Insert_NonbreakingSpace:
#endif
		case cmd_Insert_BreakBelowImage:			// is this always available?
		case cmd_Insert_LineBreak:
			outEnabled = IsDoneLoading() && 
						ED_ELEMENT_TEXT == EDT_GetCurrentElementType( *GetContext() );
		break;
				
// Edit Menu
		case cmd_Undo:
		case cmd_Redo:
			{
			if ( !IsDoneLoading() )
				return;
	
			Boolean isUndoEnabled;
			outEnabled = isUndoEnabled = EDT_GetUndoCommandID( *GetContext(), 0 ) != CEDITCOMMAND_ID_NULL;
			if ( !isUndoEnabled )
			{	// check if redo should be enabled
				outEnabled = EDT_GetRedoCommandID( *GetContext(), 0 ) != CEDITCOMMAND_ID_NULL;
				if ( outEnabled && inCommand != cmd_Redo )	// reset only if not already set
					SetMenuCommandAndString( inCommand, cmd_Redo, STRPOUND_EDITOR_MENUS, EDITOR_MENU_REDO, outName );
			}
			else if ( inCommand != cmd_Undo )
				SetMenuCommandAndString( inCommand, cmd_Undo, STRPOUND_EDITOR_MENUS, EDITOR_MENU_UNDO, outName );
			}
			break;
		
		case cmd_Format_Paragraph_Normal:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
				outMark = P_NSDT == EDT_GetParagraphFormatting( *GetContext() )
							? checkMark : 0;
			break;
		
		case cmd_Format_Paragraph_Head1:
		case cmd_Format_Paragraph_Head2:
		case cmd_Format_Paragraph_Head3:
		case cmd_Format_Paragraph_Head4:
		case cmd_Format_Paragraph_Head5:
		case cmd_Format_Paragraph_Head6:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
				outMark = EDT_GetParagraphFormatting( *GetContext() )
							== (inCommand - cmd_Format_Paragraph_Head1 + P_HEADER_1) ? checkMark : 0;
			break;

		case cmd_Format_Paragraph_Address:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
				outMark = P_ADDRESS == EDT_GetParagraphFormatting( *GetContext() )
							 ? checkMark : 0;
			break;
		
		case cmd_Format_Paragraph_Formatted:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
				outMark = P_PREFORMAT == EDT_GetParagraphFormatting( *GetContext() )
							? checkMark : 0;
			break;
		
		case cmd_Format_Paragraph_List_Item:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
				outMark = P_LIST_ITEM == EDT_GetParagraphFormatting( *GetContext() )
							? checkMark : 0;
			break;
		
		case cmd_Format_Paragraph_Desc_Title:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
				outMark = P_DESC_TITLE == EDT_GetParagraphFormatting( *GetContext() )
							? checkMark : 0;
			break;
		
		case cmd_Format_Paragraph_Desc_Text:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
				outMark = P_DESC_TEXT == EDT_GetParagraphFormatting( *GetContext() )
							? checkMark : 0;
			break;

		case cmd_Bold:	// cmd_Format_Character_Bold
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					if ( (better->mask & TF_BOLD) && (better->values & TF_BOLD) )
						outMark = checkMark;
					if ( !(better->mask & TF_BOLD) )
						outMark = dashMark;
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Italic:	// cmd_Format_Character_Italic
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					if ( (better->mask & TF_ITALIC) && (better->values & TF_ITALIC) )
						outMark = checkMark;
					if ( !(better->mask & TF_ITALIC) )
						outMark = dashMark;
					EDT_FreeCharacterData( better );
				}
			}
			break;

		case cmd_Underline:		// cmd_Format_Character_Underline
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					if ( (better->mask & TF_UNDERLINE) && (better->values & TF_UNDERLINE) )
						outMark = checkMark;
					if ( !(better->mask & TF_UNDERLINE) )
						outMark = dashMark;
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Format_Character_Nonbreaking:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					if ( (better->mask & TF_NOBREAK) && (better->values & TF_NOBREAK) )
						outMark = checkMark;
					if ( !(better->mask & TF_NOBREAK) )
						outMark = dashMark;
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Format_Character_Superscript:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					if ( (better->mask & TF_SUPER) && (better->values & TF_SUPER) )
						outMark = checkMark;
					if ( !(better->mask & TF_SUPER) )
						outMark = dashMark;
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Format_Character_Subscript:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					if ( (better->mask & TF_SUB) && (better->values & TF_SUB) )
						outMark = checkMark;
					if ( !(better->mask & TF_SUB) )
						outMark = dashMark;
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Format_Character_Strikeout:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					if ( (better->mask & TF_STRIKEOUT) && (better->values & TF_STRIKEOUT) )
						outMark = checkMark;
					if ( !(better->mask & TF_STRIKEOUT) )
						outMark = dashMark;
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Format_Character_Blink:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					if ( (better->mask & TF_BLINK) && (better->values & TF_BLINK) )
						outMark = checkMark;
					if ( !(better->mask & TF_BLINK) )
						outMark = dashMark;
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Format_Font_Size_N2:
		case cmd_Format_Font_Size_N1:
		case cmd_Format_Font_Size_0:
		case cmd_Format_Font_Size_P1:
		case cmd_Format_Font_Size_P2:
		case cmd_Format_Font_Size_P3:
		case cmd_Format_Font_Size_P4:
			if ( !IsDoneLoading() )
				return;
	
			outUsesMark = outEnabled = CanUseCharFormatting();
			if ( outUsesMark )
			{
				outMark = 0;
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					if ((better->mask & TF_FONT_SIZE) && better->iSize == (inCommand - cmd_Format_Font_Size_N2 + 1))
						outMark = checkMark;
					EDT_FreeCharacterData( better );
				}
			}
			break;

		case cmd_JustifyLeft:	// cmd_AlignParagraphLeft
		case cmd_JustifyCenter:	// cmd_AlignParagraphCenter
		case cmd_JustifyRight:	// cmd_AlignParagraphRight
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = CanUseCharFormatting();
			if ( outEnabled )
			{
				int alignItemNum = GetAlignmentMenuItemNum( *GetContext(),
													outEnabled );
				outUsesMark = true;
				if ( ((alignItemNum == 1) && (inCommand == cmd_JustifyLeft)) 
				|| ((alignItemNum == 2) && (inCommand == cmd_JustifyCenter))
				|| ((alignItemNum == 3) && (inCommand == cmd_JustifyRight)) )
					outMark = checkMark;
				else
					outMark = 0;
			}
			break;

		case cmd_Format_FontColor:
		case cmd_Format_Character_ClearAll:
		case cmd_Format_Paragraph_Indent:
		case cmd_Format_Paragraph_UnIndent:
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = CanUseCharFormatting();
			break;
		
		case cmd_Format_Text:	// Attributes
		{
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = true;
			if ( CanUseCharFormatting() )
			{
				if ( EDT_CanSetHREF( *GetContext() ) && EDT_GetHREF( *GetContext() ))
					index = EDITOR_MENU_LINK_ATTRIBUTES;
				else
				{
					index = EDITOR_MENU_CHARACTER_ATTRIBS;
					if ( ED_ELEMENT_SELECTION == EDT_GetCurrentElementType( *GetContext() ) ) 
					{
						// if no elements are text characteristics then--> outEnabled = false;
						outEnabled = EDT_CanSetCharacterAttribute( *GetContext() );
					}
				}
			}
			else
			{
				switch ( EDT_GetCurrentElementType( *GetContext() ) )
				{
					case ED_ELEMENT_IMAGE:	index = EDITOR_MENU_IMAGE_ATTRIBUTES;	break;
					case ED_ELEMENT_HRULE:	index = EDITOR_MENU_LINE_ATTRIBUTES;	break;
					case ED_ELEMENT_TARGET:	index = EDITOR_MENU_TARGET_ATTRIBUTES;	break;
					case ED_ELEMENT_UNKNOWN_TAG:	index = EDITOR_MENU_UNKNOWN_ATTRIBUTES;	break;
					default:
						index = EDITOR_MENU_CHARACTER_ATTRIBS;	
						outEnabled = false;
						break;
				}
			}		
			
			if ( index )
				::GetIndString( outName, STRPOUND_EDITOR_MENUS, index );
		}
			break;
		
		case msg_MakeNoList:
		case msg_MakeNumList:
		case msg_MakeUnumList:
			if ( !IsDoneLoading() )
				return;
	
			outEnabled = true;
			outUsesMark = true;
			outMark = 0;
			
			if ( P_LIST_ITEM == EDT_GetParagraphFormatting( *GetContext() ) )
			{
				EDT_ListData *list = EDT_GetListData( *GetContext() );
				if ( list )
				{
					if ( inCommand == msg_MakeNoList )
						;	// already 0
					else if ( inCommand == msg_MakeUnumList )
						outMark = ( list->iTagType == P_UNUM_LIST ) ? checkMark : 0;
					else if ( inCommand == msg_MakeNumList )
						outMark = ( list->iTagType == P_NUM_LIST ) ? checkMark : 0;

					EDT_FreeListData( list );
				}
			}
			else if (inCommand == msg_MakeNoList )
				outMark = checkMark;
			break;
		
		case cmd_FontSmaller:	// cmd_DecreaseFontSize
		case cmd_FontLarger:	// cmd_IncreaseFontSize
			outEnabled = IsDoneLoading();
			break;
		
		case cmd_DocumentInfo:
			outEnabled = IsDoneLoading() && !EDT_IS_NEW_DOCUMENT( ((MWContext *)*GetContext()) );
			break;
		
		default:
			if (inCommand >= COLOR_POPUP_MENU_BASE 
			&& inCommand <= COLOR_POPUP_MENU_BASE_LAST)
			{
				// if we haven't finished initializing yet nothing should be enabled
				
				outEnabled = IsDoneLoading();
				if ( !outEnabled )
					return;

				if ( mColorPopup && mColorPopup->IsEnabled() )
				{
					hasBroadcasting = mColorPopup->IsBroadcasting();
					if ( hasBroadcasting )
						mColorPopup->StopBroadcasting();
		
					if ( CanUseCharFormatting() )
					{
					 	better = EDT_GetCharacterData( *GetContext() );
					 	if ( better && better->pColor )
					 	{
							RGBColor	rgbColor;
							short		mItem;

							rgbColor.red = (better->pColor->red << 8);
							rgbColor.green = (better->pColor->green << 8);
							rgbColor.blue = (better->pColor->blue << 8);
							mItem = mColorPopup->GetMenuItemFromRGBColor( &rgbColor );
							
							// if it's a last resort (current item), set menu string
							if ( mItem == CColorPopup::CURRENT_COLOR_ITEM )
							{
								// set control to the color that just got picked
								Str255 colorstr;
								XP_SPRINTF( (char *)&colorstr[2], "%02X%02X%02X", better->pColor->red, better->pColor->green, better->pColor->blue);
								colorstr[1] = CColorPopup::CURRENT_COLOR_CHAR;	// put in leading character
								colorstr[0] = strlen( (char *)&colorstr[1] );
								mColorPopup->SetDescriptor( colorstr );
								::SetMenuItemText( mColorPopup->GetMenu()->GetMacMenuH(), 
													CColorPopup::CURRENT_COLOR_ITEM, 
													(unsigned char *)&colorstr );
							}
							mColorPopup->SetValue( mItem );
						}
						if ( better )
							EDT_FreeCharacterData( better );
					}
					
					// resume broadcasting
					if ( hasBroadcasting )
						mColorPopup->StartBroadcasting();
				}
			}
			else
				CHTMLView::FindCommandStatus( inCommand, outEnabled, outUsesMark, outMark, outName );
	}
	
	// force menu items to update (checkMarks and text (Undo/Redo) could change)
	{
	StUseCharFormattingCache stCharFormat( *this );
	LCommander::SetUpdateCommandStatus( true );
	}
}


Boolean CEditView::FindCommandStatusForContextMenu(
	CommandT inCommand,
	Boolean	&outEnabled,
	Boolean	&outUsesMark,
	Char16	&outMark,
	Str255	outName)
{
#pragma unused( outUsesMark, outMark )

	// We come here only if a CContextMenuAttachment is installed.
	// Return true if we want to bypass standard status processing.
	if ( !mCurrentClickRecord )
		return false;
	
	CHTMLClickRecord& cr = *mCurrentClickRecord; // for brevity.
	Boolean isAnchor = false;
	Boolean isImage = false;
	Boolean isTable = false;
	if ( cr.mElement )
	{
		isAnchor = cr.IsAnchor();
		isImage = !isAnchor && cr.mElement->type == LO_IMAGE
				&& NET_URL_Type(mContext->GetCurrentURL()) != FTP_TYPE_URL;
		isTable = cr.mElement->type == LO_TABLE;
	}
	switch ( inCommand )
	{
		case cmd_Format_Text:
		{
			short index = 0;
			outEnabled = true;

			if ( mCurrentClickRecord && mCurrentClickRecord->GetLayoutElement()
			&& mCurrentClickRecord->GetLayoutElement()->type == LO_TEXT )
			{
				if ( EDT_CanSetHREF( *GetContext() ) && EDT_GetHREF( *GetContext() ) )
					index = EDITOR_MENU_LINK_ATTRIBUTES;
				else
				{
					index = EDITOR_MENU_CHARACTER_ATTRIBS;
					if ( ED_ELEMENT_SELECTION == EDT_GetCurrentElementType( *GetContext() ) ) 
					{
						// if no elements are text characteristics then--> outEnabled = false;
						outEnabled = EDT_CanSetCharacterAttribute( *GetContext() );
					}
				}
			}
			else
			{
				if ( isAnchor )
					index = EDITOR_MENU_TARGET_ATTRIBUTES;
				else
				{
					switch ( cr.mElement->type )
					{
						case LO_IMAGE:	index = EDITOR_MENU_IMAGE_ATTRIBUTES;	break;
						case LO_HRULE:	index = EDITOR_MENU_LINE_ATTRIBUTES;	break;
						case LO_UNKNOWN: index = EDITOR_MENU_UNKNOWN_ATTRIBUTES;	break;
						default:
							index = EDITOR_MENU_CHARACTER_ATTRIBS;	
							outEnabled = false;
							break;
					}
				}
				
			}
			
			if ( index )
				::GetIndString( outName, STRPOUND_EDITOR_MENUS, index );
			return true;
			break;
		}
		
		case cmd_Format_Table:
		case cmd_Insert_Row:
		case cmd_Delete_Row:
		case cmd_Insert_Col:
		case cmd_Delete_Col:
		case cmd_Insert_Cell:
		case cmd_Delete_Cell:
		case cmd_Delete_Table:
			outEnabled = isTable;
			return true;
			break;
	
		case cmd_Insert_Image:
		case cmd_Insert_Link:
			outEnabled = ( mCurrentClickRecord && mCurrentClickRecord->GetLayoutElement()
						&& mCurrentClickRecord->GetLayoutElement()->type == LO_TEXT );
			return true;
			break;
		
		case cmd_Insert_Table:
			outEnabled = true;	// always enabled (?!?); even if image or hrule selected (!!!)
			return true;
			break;

		case cmd_NEW_WINDOW_WITH_FRAME:
			outEnabled = ((MWContext *)*GetContext())->is_grid_cell
							&& (GetContext()->GetCurrentHistoryEntry() != NULL);
			return true;
		
		case cmd_OPEN_LINK:
			outEnabled = isAnchor;
			return true;
		
		case cmd_AddToBookmarks:
			if ( isAnchor )
				outEnabled = true;
			return true; // for now, only have a "add bookmark for this link" command.

		case cmd_SAVE_LINK_AS:
			outEnabled = isAnchor;
			return true; // we don't know unless it's an anchor

		case cmd_COPY_LINK_LOC:
			outEnabled = isAnchor && mCurrentClickRecord->mClickURL.length() > 0;
			return true;

		case cmd_VIEW_IMAGE:
		case cmd_SAVE_IMAGE_AS:
		case cmd_COPY_IMAGE:
		case cmd_COPY_IMAGE_LOC:
		case cmd_LOAD_IMAGE:
			outEnabled = isImage;
			return true;
	}

	return false; // we don't know about it
} // CEditView::FindCommandStatusForContextMenu


void CEditView::TakeOffDuty()
{
	RemoveCaret();
	CHTMLView::TakeOffDuty();
}


void CEditView::PutOnDuty(LCommander *inNewTarget)
{
	mCaretActive = true;
	StartIdling();
	CHTMLView::PutOnDuty( inNewTarget );
}


Boolean CEditView::IsMouseInSelection( SPoint32 pt, CL_Layer *curLayer, Rect& selectRect )
{
#pragma unused(curLayer)

	Boolean returnValue = false;
	
	// zero out rectangle
	selectRect.top = selectRect.bottom = selectRect.left = selectRect.right = 0;
	
	int32 start_selection, end_selection;
	LO_Element *start_element = NULL, *end_element = NULL;
	CL_Layer *layer = NULL;
	
	LO_GetSelectionEndpoints( *GetContext(), &start_element, &end_element, 
								&start_selection, &end_selection, &layer );
	
	if ( start_element == NULL )
		return false;
	
	int32 caretX = 0, caretYLow = 0, caretYHigh = 0;
	returnValue = GetCaretPosition( *GetContext(), start_element, start_selection, 
								&caretX, &caretYLow, &caretYHigh );
	selectRect.left = caretX;
	selectRect.top = caretYLow;
	
	if ( returnValue )
		GetCaretPosition( *GetContext(), end_element, end_selection, 
								&caretX, &caretYLow, &caretYHigh );
	
	selectRect.right = caretX;
	selectRect.bottom = caretYHigh;
	
	// check if we are actually in the selection
	if ( returnValue )
		returnValue = ( pt.h >= selectRect.left && pt.h <= selectRect.right 
					&& pt.v >= selectRect.top && pt.v <= selectRect.bottom );
	
	return returnValue;
}


void CEditView::ClickSelf( const SMouseDownEvent &where )
{
	if ( !IsTarget() )
		SwitchTarget( this );
	
	FLUSH_JAPANESE_TEXT
	
	// the user may have clicked in a new location
	// erase caret in case it happens to be drawn in the window now
	EraseCaret();
	
	FocusDraw();
	
	SPoint32 firstP;
	LocalToImagePoint( where.whereLocal, firstP );

	if ( 2 == GetClickCount() )
	{
		EDT_DoubleClick( *GetContext(), firstP.h, firstP.v );
		
		ED_ElementType edtElemType = EDT_GetCurrentElementType( *GetContext() );
		
		if ( ED_ELEMENT_IMAGE == edtElemType )
			ObeyCommand( cmd_InsertEditImage, NULL);		
					
		else if ( ED_ELEMENT_HRULE == edtElemType )
			ObeyCommand( cmd_InsertEditLine, NULL);		
				
		else if ( ED_ELEMENT_TARGET == edtElemType )
			ObeyCommand( cmd_InsertEdit_Target, NULL);		
					
		else if ( ED_ELEMENT_UNKNOWN_TAG == edtElemType )
			CEditDialog::Start( EDITDLG_UNKNOWN_TAG, *GetContext() );
				
		else if ( ED_ELEMENT_TABLE == edtElemType || ED_ELEMENT_CELL == edtElemType 
				|| ED_ELEMENT_ROW == edtElemType || ED_ELEMENT_COL == edtElemType )
			ObeyCommand( cmd_Format_Table, NULL );
				
		else {
#ifdef LAYERS
			LO_Element* element = LO_XYToElement( *GetContext(), firstP.h, firstP.v,  0 );
#else
			LO_Element* element = LO_XYToElement( *GetContext(), firstP.h, firstP.v );
#endif
			
			Rect selectRect;
			SMouseDownEvent modifiedEvent( where );
			if ( IsMouseInSelection( firstP, NULL, selectRect ) )
			{
				// adjust/offset event point by same amount as image point
				modifiedEvent.whereLocal.h += ( selectRect.left + 1 - firstP.h );
//				modifiedEvent.whereLocal.v += ( selectRect.top + 1 - firstP.v );
				
				firstP.h = selectRect.left + 1;
				firstP.v = selectRect.top + 1;
			}
				
			CHTMLClickRecord cr1( modifiedEvent.whereLocal, firstP, mContext, element );
			cr1.Recalc();

			SInt16 mouseAction = cr1.WaitForMouseAction( where.whereLocal, where.macEvent.when, 2 * GetDblTime() );

			if ( mouseAction == cr1.eMouseDragging )
			{
				::SafeSetCursor( iBeamCursor );
				mDoContinueSelection = true;
				ClickTrackSelection( modifiedEvent, cr1 );
				mDoContinueSelection = false;
			}
		}
		
	}
	else
	{
		LO_Element*		element;
#ifdef LAYERS
		element = LO_XYToElement( *GetContext(), firstP.h, firstP.v, 0 );
#else
		element = LO_XYToElement( *GetContext(), firstP.h, firstP.v );
#endif
		CHTMLClickRecord cr2( where.whereLocal, firstP, mContext, element );
		cr2.Recalc();

		if ( element != NULL && element->type != LO_IMAGE && element->type != LO_HRULE )
			::SafeSetCursor( iBeamCursor );
		else
			::SetCursor( &UQDGlobals::GetQDGlobals()->arrow );

		Rect selectRect;
		Boolean isMouseInSelection = IsMouseInSelection( firstP, NULL, selectRect );
		
		// check if we should be drag/drop'ing or selecting
		// if the shift key is down, we're not drag/drop'ing
		if ( isMouseInSelection && !((where.macEvent.modifiers & shiftKey) != 0) )
		{
			Boolean canDragDrop = true /* && PrefSetToDrag&Drop */;
			if ( canDragDrop )
			{
				ClickTrackSelection( where, cr2 );
				// update status bar???
				return;
			}
		}
		
		mDoContinueSelection = !isMouseInSelection;
		ClickTrackSelection( where, cr2 );
		mDoContinueSelection = false;
	}
}


// if we are over an edge, track the edge movement
// otherwise, track the text
// clean this up with TrackSelection in mclick.cp
Boolean CEditView::ClickTrackSelection( const SMouseDownEvent& where, 
										CHTMLClickRecord& inClickRecord )
{
	long ticks = TickCount();
	Rect	frame, oldSizingRect;
	Boolean	didTrackSelection = false, didDisplayAttachment = false;
	Boolean isSizing = false;

	StopRepeating();
	
	// Text selection
#ifdef SOMEDAY_MAC_EDITOR_HANDLE_LAYERS
	XP_Rect theBoundingBox;
	theBoundingBox.left = theBoundingBox.top = theBoundingBox.right = theBoundingBox.bottom = 0;
	CL_GetLayerBboxAbsolute(inClickRecord.mLayer, &theBoundingBox);
#endif

	
	// Convert from image to layer coordinates
	SPoint32 theImagePointStart, theImagePointEnd, newP;
	LocalToImagePoint( where.whereLocal, theImagePointStart );
	newP = theImagePointStart;
#ifdef SOMEDAY_MAC_EDITOR_HANDLE_LAYERS
	theImagePointStart.h -= theBoundingBox.left;
	theImagePointStart.v -= theBoundingBox.top;
#endif

	theImagePointEnd.h = theImagePointEnd.v = -1;
	
// find out what we are doing before we start
// ARE WE DRAGGING???
	Rect selectRect;
	Boolean doDragSelection = IsMouseInSelection( theImagePointStart, NULL, selectRect ) 
								&& !((where.macEvent.modifiers & shiftKey) != 0);
	
// ARE WE IN TABLE (sizing or selecting)???
	Boolean bLock = (where.macEvent.modifiers & cmdKey) == cmdKey;
	
	LO_Element *pCurrentElement = NULL;
	ED_HitType iTableHit;
	Boolean hasSelectedTable = false;
	iTableHit = EDT_GetTableHitRegion( *GetContext(), newP.h, newP.v, &pCurrentElement, bLock );
	if ( pCurrentElement )	// we're in some part of a table
	{
		if ( iTableHit == ED_HIT_SIZE_TABLE_WIDTH || iTableHit == ED_HIT_SIZE_TABLE_HEIGHT
		|| iTableHit == ED_HIT_SIZE_COL
		|| iTableHit == ED_HIT_ADD_ROWS || iTableHit == ED_HIT_ADD_COLS )
		{
			isSizing = true;
		}
		else if ( iTableHit != ED_HIT_NONE )
		{
			hasSelectedTable =
				EDT_SelectTableElement( *GetContext(), newP.h, newP.v, 
									pCurrentElement, iTableHit, bLock,
									(where.macEvent.modifiers & shiftKey) == shiftKey );
			if ( hasSelectedTable )
			{
				mDoContinueSelection = false;
				UpdatePort();
//			break;	// this bails out of loop on single-click; can't bring up context menu
			}
		}
	}
	
	if ( ( pCurrentElement == NULL ) 						// we're not in a table or not in table hotspot
	|| ( pCurrentElement &&	iTableHit == ED_HIT_NONE ) )	// get element from click record
	{
// ARE WE IN A NON-TABLE SIZEABLE ELEMENT???
		pCurrentElement = inClickRecord.GetLayoutElement();
		isSizing = pCurrentElement && EDT_CanSizeObject( *GetContext(), pCurrentElement, newP.h, newP.v );

		if ( !doDragSelection )
		{
			// Setup
			if ( (where.macEvent.modifiers & shiftKey) != 0 && !inClickRecord.IsClickOnAnchor() )
				EDT_ExtendSelection( *GetContext(), theImagePointStart.h, theImagePointStart.v );	// this line modified!
			else if ( mDoContinueSelection )
				EDT_StartSelection( *GetContext(), theImagePointStart.h, theImagePointStart.v );	// this line modified!
		}
	}

	Point	qdWhere;
	::GetMouse( &qdWhere );
	LocalToPortPoint( qdWhere );		// convert qdWhere to port coordinates for cursor update
	AdjustCursorSelf( qdWhere, where.macEvent );
		
// INITIALIZATION FOR RESIZING INLINE
	XP_Rect	sizeRect;
	if ( isSizing )
	{
		if ( EDT_StartSizing( *GetContext(), pCurrentElement,
							newP.h, newP.v, bLock, &sizeRect ) )
		{
			UpdatePort();
			
			oldSizingRect.top = sizeRect.top;
			oldSizingRect.bottom = sizeRect.bottom;
			oldSizingRect.left = sizeRect.left;
			oldSizingRect.right = sizeRect.right;
			FocusDraw();
			DisplaySelectionFeedback( LO_ELE_SELECTED, oldSizingRect );
		}
	}

// BEGIN ACTUAL TRACKING OF MOVEMENT
	// track mouse till we are done
	do {
		FocusDraw();		// so that we get coordinates right
		::GetMouse( &qdWhere );
		
		if ( AutoScrollImage( qdWhere ) )		// auto-scrolling
		{	
			CalcLocalFrameRect( frame );
			if ( qdWhere.v < frame.top )
				qdWhere.v = frame.top;
			else if ( qdWhere.v > frame.bottom )
				qdWhere.v = frame.bottom;
		}
		
		LocalToImagePoint( qdWhere, newP );
		LocalToPortPoint( qdWhere );		// convert qdWhere to port coordinates for cursor update

#ifdef SOMEDAY_MAC_EDITOR_HANDLE_LAYERS
		// Convert from image to layer coordinates
		newP.h -= theBoundingBox.left;
		newP.v -= theBoundingBox.top;
#endif

		if ( ( newP.v != theImagePointEnd.v ) || ( newP.h != theImagePointEnd.h ) )
		{
			if ( doDragSelection )
			{
				// the following code can't be moved out of the StillDown() loop 
				// because we need to check if mouse moves so we can bring up
				// context menus at the right time
				
				// if the mouse is not in the selection, then we will be in "move" mode
				if ( !IsMouseInSelection( newP, NULL, frame ) )
				{
					// get data (store!) since selection will change as we move around!
					char *ppText = NULL;
					int32 pTextLen;
					EDT_ClipboardResult result;
					
					mDragData = NULL;
					mDragDataLength = 0;
					result = EDT_CopySelection( *GetContext(), &ppText, &pTextLen, 
												&mDragData, &mDragDataLength );
					XP_FREEIF( ppText );
					if ( result != EDT_COP_OK || mDragData == NULL )
						mDragDataLength = 0;
					
					// now start the dragging!
					::LocalToGlobal( &topLeft(selectRect) );
					::LocalToGlobal( &botRight(selectRect) );
					CComposerDragTask theDragTask( where.macEvent, selectRect, *this );
					
					OSErr theErr = ::SetDragSendProc(theDragTask.GetDragReference(), mSendDataUPP, (LDragAndDrop*)this);
					ThrowIfOSErr_(theErr);
					
					theDragTask.DoDrag();

					XP_FREEIF( mDragData );

					return true;
				}
			}
			else if ( isSizing )
			{
				if ( EDT_GetSizingRect( *GetContext(), newP.h, newP.v, bLock, &sizeRect ) )
				{
					// Remove last sizing feedback
					FocusDraw();
					DisplaySelectionFeedback( LO_ELE_SELECTED, oldSizingRect );
					
					// Save the new rect.
					oldSizingRect.top = sizeRect.top;
					oldSizingRect.bottom = sizeRect.bottom;
					oldSizingRect.left = sizeRect.left;
					oldSizingRect.right = sizeRect.right;
					
					// then draw new feedback
					DisplaySelectionFeedback(LO_ELE_SELECTED, oldSizingRect);
				}
			}
			else
			{
				didTrackSelection = true;
				if ( mDoContinueSelection )
					EDT_ExtendSelection( *GetContext(), newP.h, newP.v );	// this line is modified!
			}	

			// reset clock for context popup menu if mouse hasn't moved much
			if ( abs(newP.h - theImagePointEnd.h) >= eMouseHysteresis ||
				abs(newP.v - theImagePointEnd.v) >= eMouseHysteresis )
				ticks = TickCount();
		}
		
		// if mouse hasn't moved, check if it's time to bring up context menu
		if ( !isSizing 
			&& ( doDragSelection 
				|| ( newP.v == theImagePointEnd.v ) && ( newP.h == theImagePointEnd.h ) )
			&& TickCount() > ticks + 2 * GetDblTime() )
		{
			::SetCursor( &UQDGlobals::GetQDGlobals()->arrow );
			SInt16 mouseAction = inClickRecord.WaitForMouseAction( where, this, 2 * GetDblTime() );
			if ( mouseAction == inClickRecord.eHandledByAttachment )
			{
				didDisplayAttachment = true;
				break;
			}
			else
			{
				::GetMouse( &qdWhere );	// local coords; mouse may have moved in WaitForMouseAction?
				LocalToPortPoint( qdWhere );
				AdjustCursorSelf( qdWhere, where.macEvent );
			}
		}
		
		theImagePointEnd = newP;
		
		// • idling
		SystemTask();
		EventRecord dummy;
		::WaitNextEvent(0, &dummy, 5, NULL);
	} while ( ::StillDown() );
	
	if ( isSizing )
	{
		// remove last sizing feedback
		FocusDraw();
		DisplaySelectionFeedback( LO_ELE_SELECTED, oldSizingRect );

		if ( abs(theImagePointStart.h - theImagePointEnd.h) >= eMouseHysteresis
		|| abs(theImagePointStart.v - theImagePointEnd.v) >= eMouseHysteresis )
			EDT_EndSizing( *GetContext() );
		else
		{
			EDT_CancelSizing( *GetContext() );
			
			// move insertion caret
			EDT_StartSelection( *GetContext(), theImagePointEnd.h, theImagePointEnd.v );
			EDT_EndSelection( *GetContext(), theImagePointEnd.h, theImagePointEnd.v );
		}
	}
	
	if ( doDragSelection )
		return false;
	
	::GetMouse( &qdWhere );
	LocalToImagePoint( qdWhere, newP );
	
	if ( !mDoContinueSelection && !didDisplayAttachment && !isSizing && !hasSelectedTable )
	{
		// mouse was in a selection but no context menu displayed
		EDT_StartSelection( *GetContext(), theImagePointStart.h, theImagePointStart.v );
		EDT_ExtendSelection( *GetContext(), newP.h, newP.v );	// this line is modified!
		EDT_EndSelection( *GetContext(), newP.h, newP.v );
	}
	else if ( mDoContinueSelection )
		EDT_EndSelection( *GetContext(), newP.h, newP.v );				// this line modified!

	return didTrackSelection;
}

		
Boolean 
CEditView::ItemIsAcceptable( DragReference inDragRef, ItemReference inItemRef )
{
	HFSFlavor	fileData;
	Size		dataSize = sizeof(fileData);
	FlavorFlags	flags;
	
	if ( ::GetFlavorData( inDragRef, inItemRef, flavorTypeHFS, &fileData, &dataSize, 0 ) == noErr )
		return true;			// we'll take ANY file, at least as a link
	
	if (::GetFlavorFlags( inDragRef, inItemRef, emBookmarkDrag, &flags ) == noErr )
		return true;
	
	if ( ::GetFlavorFlags( inDragRef, inItemRef, 'TEXT', &flags ) == noErr )
		return true;

	if ( ::GetFlavorFlags( inDragRef, inItemRef, emComposerNativeDrag, &flags ) == noErr )
		return true; 
	/*
	flavorTypePromiseHFS
	emBookmarkFile
	emXPBookmarkInternal
	'TEXT'
	'ADDR'
	'ADRS'
	emBookmarkFileDrag
	emBookmarkDrag
	'PICT'
	MAIL_MESSAGE_FLAVOR
	TEXT_FLAVOR
	NEWS_ARTICLE_FLAVOR
	*/
	
	return false;
}

void CEditView::DoDragSendData(FlavorType inFlavor, ItemReference inItemRef, DragReference inDragRef)
{
	if ( inFlavor == emComposerNativeDrag )
	{
		OSErr theErr = ::SetDragItemFlavorData( inDragRef, inItemRef,
										 inFlavor, mDragData, mDragDataLength, 0 );
	}
	else
		CHTMLView::DoDragSendData( inFlavor, inItemRef, inDragRef );
}


void		
CEditView::InsideDropArea(DragReference	inDragRef)
{
	Point		mouseLoc;
	SPoint32	imagePt;
		
	FocusDraw();
	
	::GetDragMouse( inDragRef, &mouseLoc, NULL );
	::GlobalToLocal( &mouseLoc );
	
	Rect 	localFrame;
	Int32	scrollLeft = 0;
	Int32	scrollTop = 0;
	
	CalcLocalFrameRect( localFrame );
	if ( mouseLoc.h < (localFrame.left+10) )
		scrollLeft = -mScrollUnit.h;
	else if ( mouseLoc.h > (localFrame.right-10) )
		scrollLeft = mScrollUnit.h;
	
	if ( mouseLoc.v < (localFrame.top+10) )
		scrollTop = -mScrollUnit.v;
	else if ( mouseLoc.v > (localFrame.bottom-10) )
		scrollTop = mScrollUnit.v;

	if ( scrollLeft != 0 || scrollTop != 0 )
	{
		if ( ScrollPinnedImageBy( scrollLeft, scrollTop, true ) )
			Draw(NULL);
	}
		
	LocalToImagePoint( mouseLoc, imagePt );
	
	/* set the insert point here so that when the drag is over we can insert in the correct place. */
	EDT_PositionDropCaret( *GetContext(), imagePt.h, imagePt.v );
}


//
// ReceiveDragItem
//
// Called once for each item reference dropped on the composer window. After doing some view
// specific work, farm off the bulk of data extraction to our mixin class which will in turn
// call the HandleDropOf* classes below to do the work.
//
void 
CEditView::ReceiveDragItem ( DragReference inDragRef, DragAttributes inDragAttr, 
								ItemReference inItemRef, Rect& inItemBounds	)
{
	mIsHilited = false;
	UnhiliteDropArea( inDragRef );

	FocusDraw();
	
	Point mouseLoc;
	::GetDragMouse( inDragRef, &mouseLoc, NULL );
	::GlobalToLocal( &mouseLoc );
	LocalToImagePoint( mouseLoc, mDropLocationImageCoords );
	
	CComposerAwareURLDragMixin::ReceiveDragItem ( inDragRef, inDragAttr, inItemRef, inItemBounds,
													mDropLocationImageCoords );
													
} // ReceiveDragItem


//
// HandleDropOfComposerFlavor
//
// Put the data in the right place, given the current mouse location and if this is a copy 
// or a move. Will delete the current selection if it is a move.
//
void
CEditView :: HandleDropOfComposerFlavor ( const char* inData, bool inDoCopy, SPoint32 & inMouseLoc )
{
	EDT_BeginBatchChanges( *GetContext() );
	if ( inDoCopy || mDragData == NULL )
		EDT_PositionCaret( *GetContext(), inMouseLoc.h, inMouseLoc.v );
	else
		EDT_DeleteSelectionAndPositionCaret( *GetContext(), inMouseLoc.h, inMouseLoc.v );
	EDT_PasteHTML( *GetContext(), const_cast<char*>(inData) );
	EDT_EndBatchChanges( *GetContext() );

} // HandleDropOfComposerFlavor


//
// HandleDropOfPageProxy
//
// This one's real easy since all the data extraction code is already done for us.
//
void
CEditView :: HandleDropOfPageProxy ( const char* inURL, const char* inTitle )
{
	char* url = const_cast<char*>(inURL);
	char* title = const_cast<char*>(inTitle);
	
	if ( inURL ) {
		if ( inTitle )
			EDT_PasteHREF( *GetContext(), &url, &title, 1 );
		else
			EDT_PasteHREF( *GetContext(), &url, &url, 1 );
	}
	
} // HandleDropOfPageProxy


//
// HandleDropOfLocalFile
//
// Accepts gif/jpeg drops and puts them inline and inserts the url for other kinds
// of files. Clippings files are already handled by responding to the 'TEXT' flavor, so we 
// never get here (which is why that code was removed).
//
void
CEditView :: HandleDropOfLocalFile ( const char* inFileURL, const char* fileName,
											const HFSFlavor & inFileData )
{
	Boolean isImageDropped = false;
	char* URLStr = const_cast<char*>(inFileURL);
	char* titleStr = const_cast<char*>(fileName);

	switch ( inFileData.fileType )
	{
		case 'GIFf':
		case 'JPEG':
			isImageDropped = true;
			titleStr = NULL;
			break;
				
		default:
			if ( inFileURL )
			{
				char *link = URLStr;
				Bool bAutoAdjustLinks = CPrefs::GetBoolean( CPrefs::PublishMaintainLinks );
				if ( bAutoAdjustLinks )
				{
					char *abs = NULL;		// lets try making it relative
					if ( NET_MakeRelativeURL( LO_GetBaseURL( *GetContext() ), link, &abs )
						!= NET_URL_FAIL && abs )
					{
						link = abs;
						abs = NULL;
					}
					else if ( abs )
						XP_FREE(abs);
				}
				
				URLStr = link;
				titleStr = link;
			}
			break;
	}

	// if the user dragged an image, insert the image inline, otherwise paste in the URL (with 
	// title if one is present).
	if ( URLStr ) {
		if ( isImageDropped ) {
			EDT_ImageData* imageData = EDT_NewImageData(); 
			if ( imageData )
			{
				imageData->pSrc = XP_STRDUP(URLStr);
				EDT_InsertImage( *GetContext(), imageData, CPrefs::GetBoolean( CPrefs::PublishKeepImages ) );
				EDT_FreeImageData( imageData );
			}
		}
		else {
			if ( titleStr )
				EDT_PasteHREF( *GetContext(), &URLStr, &titleStr, 1 );
			else
				EDT_PasteHREF( *GetContext(), &URLStr, &URLStr, 1 );
		}
	}
		
} // HandleDropOfLocalFile


//
// HandleDropOfText
//
// Very simple, since all of the data extraction is done for us already
//
void
CEditView :: HandleDropOfText ( const char* inTextData )
{
	EDT_PositionCaret( *GetContext(), mDropLocationImageCoords.h, mDropLocationImageCoords.v );
	EDT_PasteText( *GetContext(), const_cast<char*>(inTextData) );

} // HandleDropOfText



void
CEditView :: HandleDropOfHTResource ( HT_Resource /*node*/ )
{
	DebugStr("\pNot yet implemented");

} // HandleDropOfHTResource


void CEditView::InsertDefaultLine()
{
	EDT_HorizRuleData *pData = EDT_NewHorizRuleData();	
	if ( pData )
	{
		EDT_InsertHorizRule( *GetContext(), pData );
		EDT_FreeHorizRuleData( pData );
	}
}


// should be XP code (make a list --> indent)
void CEditView::ToFromList(intn listType, ED_ListType elementType)
{
	Bool			alreadySet = false;
	EDT_ListData* list = NULL;
	
	EDT_BeginBatchChanges( *GetContext() );

	if (P_LIST_ITEM == EDT_GetParagraphFormatting( *GetContext() ) )
	{
		list = EDT_GetListData( *GetContext() );
		alreadySet = (list && ((list->iTagType == listType) || (listType == 0) ) );
	}
	else if ( 0 == listType )	// doesn't want to become a list and isn't a list (disable?)
		alreadySet = true;
	
	if ( alreadySet )
	{											// ListA -> Paragraph
		 EDT_Outdent( *GetContext() );
		 EDT_MorphContainer( *GetContext(), P_NSDT );
	}
	else
	{													// Paragraph -> ListA, ListB -> ListA
		if ( !list )
		{								// make it a list
			EDT_Indent( *GetContext() );
			EDT_MorphContainer( *GetContext(), P_LIST_ITEM );
			list = EDT_GetListData( *GetContext() );
			if ( list == NULL )
				return;	// assert?
		}
		
		list->iTagType = listType;
		list->eType = elementType;
		EDT_SetListData( *GetContext(), list );
	}
	
	if ( list )
		EDT_FreeListData( list );

	EDT_EndBatchChanges( *GetContext() );
}


void CEditView::DisplayLineFeed( int inLocation, LO_LinefeedStruct* lineFeed, Bool needBg )
{
	CHTMLView::DisplayLineFeed( inLocation, lineFeed, needBg );
	
	if ( ! mDisplayParagraphMarks )
		return;

	// The editor only draws linefeeds which are breaks.
	if ( ! lineFeed->break_type )
		return;
	
	// Also, don't draw mark after end-of-doc mark.
    if ( lineFeed->prev && lineFeed->prev->lo_any.edit_offset < 0)
    	return;

    // Allow selection feedback and display of end-of-paragraph marks in the editor.
    // We do nothing if this isn't an end-of-paragraph or hard break.
    // If we're displaying the marks, then we display them as little squares.
    // Otherwise we just do the selection feedback.
    // The selection feedback is a rectangle the height of the linefeed 1/2 of the rectangle's height.
    // located at the left edge of the linefeed's range.

    // We need to expand the linefeed's width here so that ResolveElement thinks it's at least as wide
    // as we're going to draw it.
    
    Bool bExpanded = false;
    
    // save these to restore later.
	int32 originalWidth = lineFeed->width;
	int32 originalHeight = lineFeed->height;
    
    const int32 kMinimumWidth = 0;						// Oh, I love constants in my code!!
    const int32 kMaxHeight = 30;
    const int32 kMinimumHeight = 0;

	int32 expandedWidth = originalWidth;
	int32 expandedHeight = originalHeight;

    if ( expandedWidth < kMinimumWidth )
    {
        expandedWidth = kMinimumWidth;
        bExpanded = true;
    }
    
    if ( expandedHeight < kMinimumHeight )
    {
        expandedHeight = kMinimumHeight;
        bExpanded = true;
    }
    
    lineFeed->width = expandedWidth;
    lineFeed->height = expandedHeight;
    
    if ( expandedHeight > kMaxHeight )
        expandedHeight = kMaxHeight;

    int32 desiredWidth = expandedHeight / 2 + 3;

    if ( expandedWidth > desiredWidth )
        expandedWidth = desiredWidth;

	Rect frame;

	if (!CalcElementPosition( (LO_Element*)lineFeed, frame ))
		return;		// can't see it.
	
   // Limit the size of the drawn paragraph marker.

	frame.right = frame.left + expandedWidth;
	frame.bottom = frame.top + expandedHeight;


	if ( frame.right - frame.left > 5 )
	{
		frame.left += 2 + 3;
		frame.right -= 2;
	}

	if ( frame.bottom - frame.top > 5 )
	{
		frame.top += 2;
		frame.bottom -= 2;
	}

	// draw line breaks at 1/3 height of paragraph marks
	if ( lineFeed->break_type == LO_LINEFEED_BREAK_HARD )
		frame.top += (frame.bottom - frame.top) * 2 / 3;

	RGBColor	tmp;
	tmp = UGraphics::MakeRGBColor( lineFeed->text_attr->fg.red, 
									lineFeed->text_attr->fg.green,
									lineFeed->text_attr->fg.blue );
	
	// If the linefeed is selected and the linefeed color is the same as selection color, invert it.
	// This doesn't do anything if there is no selection. So it is still possible for the linefeed
	// fg color to be the same as the linefeed bg color or the page background color.
	if ( lineFeed->ele_attrmask & LO_ELE_SELECTED )
	{
		RGBColor		color;
		LMGetHiliteRGB( &color );
	
		if ( UGraphics::EqualColor( tmp, color ) )
		{
			tmp.red = ~color.red;
			tmp.blue = ~color.blue;
			tmp.green = ~color.green;
		}
	}
	
	::RGBBackColor( &tmp );
	::EraseRect( &frame );
	
	// restore original size
	lineFeed->width = originalWidth;
	lineFeed->height = originalHeight;
}


void CEditView::DisplayTable( int inLocation, LO_TableStruct *inTableStruct )
{
	Boolean	hasZeroBorderWidth;
	int32 savedBorderStyle;
	LO_Color savedBorderColor;
	Rect theFrame;

	if ( !FocusDraw() )
		return;
	
	if ( !CalcElementPosition( (LO_Element*)inTableStruct, theFrame ) )
		return;
	
	int iSelectionBorderThickness;
	if ( !(inTableStruct->ele_attrmask & LO_ELE_SELECTED) )
		iSelectionBorderThickness = 0;
	else
	{
		// set the border thickness to be the minimum of all border widths
		iSelectionBorderThickness = inTableStruct->border_left_width;
		if ( inTableStruct->border_right_width < iSelectionBorderThickness )
			iSelectionBorderThickness = inTableStruct->border_right_width;
		if ( inTableStruct->border_top_width < iSelectionBorderThickness )
			iSelectionBorderThickness = inTableStruct->border_top_width;
		if ( inTableStruct->border_bottom_width < iSelectionBorderThickness )
			iSelectionBorderThickness = inTableStruct->border_bottom_width;
		
		// allow for a larger selection if the border is large
		if ( iSelectionBorderThickness > 2 * ED_SELECTION_BORDER )
			iSelectionBorderThickness = 2 * ED_SELECTION_BORDER;
		
		// else if the area is too small, use the spacing between cells
		else if ( iSelectionBorderThickness < ED_SELECTION_BORDER )
		{
			iSelectionBorderThickness += inTableStruct->inter_cell_space;
			
			// but don't use it all; stick to the minimal amount
			if ( iSelectionBorderThickness > ED_SELECTION_BORDER )
				iSelectionBorderThickness = ED_SELECTION_BORDER;
		}
	}

	hasZeroBorderWidth = (( inTableStruct->border_width == 0 ) &&
						 ( inTableStruct->border_top_width == 0 ) &&
						 ( inTableStruct->border_right_width == 0 ) &&
						 ( inTableStruct->border_bottom_width == 0 ) &&
						 ( inTableStruct->border_left_width == 0 ));
	if ( hasZeroBorderWidth )
	{
		if ( 0 == inTableStruct->inter_cell_space )
		{
			::InsetRect( &theFrame, -1, -1 );
			iSelectionBorderThickness = 1;
		}
	
		inTableStruct->border_width = 1;
		inTableStruct->border_top_width = 1;
		inTableStruct->border_right_width = 1;
		inTableStruct->border_bottom_width = 1;
		inTableStruct->border_left_width = 1;
		
		savedBorderStyle = inTableStruct->border_style;
		inTableStruct->border_style = BORDER_DOTTED;
		savedBorderColor = inTableStruct->border_color;
		inTableStruct->border_color.red += 0x80;
		inTableStruct->border_color.green += 0x80;
		inTableStruct->border_color.blue += 0x80;
	}

	CHTMLView::DisplayTable( inLocation, inTableStruct );
	
	// restore previous values; necessary?
	if ( hasZeroBorderWidth )
	{
		inTableStruct->border_width = 0;
		inTableStruct->border_top_width = 0;
		inTableStruct->border_right_width = 0;
		inTableStruct->border_bottom_width = 0;
		inTableStruct->border_left_width = 0;
		inTableStruct->border_style = savedBorderStyle;
		inTableStruct->border_color = savedBorderColor;
	}

	if ( (inTableStruct->ele_attrmask & LO_ELE_SELECTED)
	&& iSelectionBorderThickness < ED_SELECTION_BORDER )
	{
		if ( iSelectionBorderThickness )
			::InsetRect( &theFrame, iSelectionBorderThickness, iSelectionBorderThickness );

		DisplaySelectionFeedback( LO_ELE_SELECTED, theFrame );
	}
}


void CEditView::DisplayCell( int inLocation, LO_CellStruct *inCellStruct )
{
#pragma unused( inLocation )

	if ( !FocusDraw() )
		return;

	Rect theFrame;
	if ( CalcElementPosition( (LO_Element*)inCellStruct, theFrame ) )
	{
		int iBorder = 0;
		
		if ( (inCellStruct->ele_attrmask & LO_ELE_SELECTED)
		|| inCellStruct->ele_attrmask & LO_ELE_SELECTED_SPECIAL )
		{
			int32 iMaxWidth = 2 * ED_SELECTION_BORDER;
			
			iBorder = inCellStruct->border_width;

			if ( inCellStruct->inter_cell_space > 0 && iBorder < iMaxWidth )
			{
				int iExtraSpace;
				iExtraSpace = ::min( iMaxWidth - iBorder, inCellStruct->inter_cell_space / 2 );
				iBorder += iExtraSpace;
				InsetRect( &theFrame, -iExtraSpace, -iExtraSpace );
			}
		}
			
		if ( inCellStruct->border_width == 0 || LO_IsEmptyCell( inCellStruct ) )
		{
			StColorPenState state;
			PenPat( &qd.gray );
			PenMode( srcXor );
			
			if ( (inCellStruct->ele_attrmask & LO_ELE_SELECTED)
			|| (inCellStruct->ele_attrmask & LO_ELE_SELECTED_SPECIAL ) )
				::PenSize( ED_SELECTION_BORDER, ED_SELECTION_BORDER );
			
			FrameRect( &theFrame );
		}
		else if ( (inCellStruct->ele_attrmask & LO_ELE_SELECTED)
		|| inCellStruct->ele_attrmask & LO_ELE_SELECTED_SPECIAL )
		{
			if ( iBorder < ED_SELECTION_BORDER )
				InsetRect( &theFrame, 1, 1 );

			// don't have a color from table data so use highlight color from system
			RGBColor borderColor;
			::LMGetHiliteRGB( &borderColor );
			if ( inCellStruct->ele_attrmask & LO_ELE_SELECTED_SPECIAL )
				DisplayGrooveRidgeBorder( theFrame, borderColor, true, iBorder, iBorder, iBorder, iBorder );
			else if ( /* LO_ELE_SELECTED && */ iBorder < ED_SELECTION_BORDER )
				// draw selection inside cell border if not enough room
				DisplaySelectionFeedback( LO_ELE_SELECTED, theFrame );
			else	/* just LO_ELE_SELECTED */
				DisplaySolidBorder( theFrame, borderColor, iBorder, iBorder, iBorder, iBorder );
		}
		else	/* non-empty, non-zero border, non-selected, non-"special" --> Everything else */
		{
			// • this is really slow LAM
			for ( int i = 1; i <= inCellStruct->border_width; i++ )
			{
				UGraphics::FrameRectShaded( theFrame, true );
				::InsetRect( &theFrame, 1, 1 );
			}
		}
	}
}


void CEditView::UpdateEnableStates()
{
	CPaneEnabler::UpdatePanes();
}


void CEditView::DisplayFeedback( int inLocation, LO_Element *inElement )
{
#pragma unused(inLocation)

	if (!FocusDraw() || (inElement == NULL))
		return;

	// we only know how to deal with images and tables and cells right now
	if ( inElement->lo_any.type != LO_IMAGE )
		return;

	LO_ImageStruct *image = (LO_ImageStruct *)inElement;
	
	// if it's not selected we are done
	if (( image->ele_attrmask & LO_ELE_SELECTED ) == 0 )
		return;
	
	int selectionBorder = 3;
	int32 layerOriginX, layerOriginY;
	SPoint32 topLeftImage;
	Point topLeft;
	Rect borderRect;
	
	if ( mCurrentDrawable != NULL )
		mCurrentDrawable->GetLayerOrigin(&layerOriginX, &layerOriginY);
	else
	{
		layerOriginX = mLayerOrigin.h;
		layerOriginY = mLayerOrigin.v;
	}
	
	topLeftImage.h = image->x + image->x_offset + layerOriginX;
	topLeftImage.v = image->y + image->y_offset + layerOriginY;
	ImageToLocalPoint(topLeftImage, topLeft);

	borderRect.left = topLeft.h;
	borderRect.top = topLeft.v;	
	borderRect.right = borderRect.left + image->width 
						+ (2 * image->border_width);
	borderRect.bottom = borderRect.top + image->height 
						+ (2 * image->border_width);

	::PenSize( selectionBorder, selectionBorder );
	::PenMode( patXor );
	::PenPat( &qd.dkGray );
	
	::FrameRect(&borderRect);

	// reset pen size, mode, and pattern
	::PenNormal();
}


// this is not an FE_* call
void CEditView::DisplaySelectionFeedback( uint16 ele_attrmask, const Rect &inRect )
{
	Boolean isSelected = ele_attrmask & LO_ELE_SELECTED;
	if ( !isSelected )
		return;
	
	Rect portizedRect;
	SPoint32 updateUpperLeft, updateLowerRight;
	
	// Get the rect to update in image coordinates.
	updateUpperLeft.h = inRect.left;		// sorry to say, the horizontal will be ignored
	updateUpperLeft.v = inRect.top;			// later and set to the whole frame width
	updateLowerRight.h = inRect.right;
	updateLowerRight.v = inRect.bottom;
	
	// Convert from image to local coordinates...
	ImageToLocalPoint( updateUpperLeft, topLeft(portizedRect) );
	ImageToLocalPoint( updateLowerRight, botRight(portizedRect) );

	int32 rWidth = portizedRect.right - portizedRect.left;
	int32 rHeight = portizedRect.bottom - portizedRect.top;
	if ( rWidth <= (ED_SELECTION_BORDER*2) || rHeight <= (ED_SELECTION_BORDER*2) )
	{
		// Too narrow to draw frame. Draw solid.
		::InvertRect( &portizedRect );
	}
	else
	{
		::PenSize( ED_SELECTION_BORDER, ED_SELECTION_BORDER );
		::PenMode( srcXor );
		::FrameRect( &portizedRect );
		::PenNormal();
	}
}


/* This should invalidate the entire table or cell rect to cause redrawing of backgrounds
 *  and all table or cell contents. Use to unselect table/cell selection feedback
*/
void CEditView::InvalidateEntireTableOrCell( LO_Element* inElement )
{
	if ( !FocusDraw() )
		return;

	int32 iWidth, iHeight;
	if ( inElement->type == LO_TABLE )
	{
		iWidth = inElement->lo_table.width;
		iHeight = inElement->lo_table.height;
	}
	else if ( inElement->type == LO_CELL )
	{
		iWidth = inElement->lo_cell.width;
		iHeight = inElement->lo_cell.height;
	}
	else
		return; // Only Table and Cell types allowed

	Rect r;
	r.left = inElement->lo_any.x;
	r.top = inElement->lo_any.y;
	r.right = r.left + iWidth;
	r.bottom = r.top + iHeight;

	if ( inElement->type == LO_TABLE 
		&& inElement->lo_table.border_left_width == 0
		&& inElement->lo_table.border_right_width == 0
		&& inElement->lo_table.border_top_width == 0
		&& inElement->lo_table.border_bottom_width == 0 )
	{
		// We are displaying a "zero-width" table,
		//  increase rect by 1 pixel because that's
		//  where we drew the table's dotted-line border 
		::InsetRect( &r, -1, -1 );
	}
	
	// Include the inter-cell spacing area also used for highlighting a cell
	int32 iExtraSpace;
	if ( inElement->type == LO_CELL 
		&& inElement->lo_cell.border_width < eSelectionBorder
		&& 0 < (iExtraSpace = inElement->lo_cell.inter_cell_space / 2) )
	{
		::InsetRect( &r, -iExtraSpace, -iExtraSpace );
	}

	::InvalRect( &r );
}


void CEditView::DisplayAddRowOrColBorder( XP_Rect* inRect, XP_Bool inDoErase )
{
	if ( !FocusDraw() )
		return;

	if ( inDoErase )
	{
		SPoint32 updateUL, updateLR;
		Point	ul, lr;
		
		// Get the rect to update in image coordinates.
		updateUL.h = inRect->left;		// sorry to say, the horizontal will be ignored later and set to the whole frame width
		updateUL.v = inRect->top;
		updateLR.h = inRect->right;
		updateLR.v = inRect->bottom;
		
		// Convert from image to local to port coordinates... we don't have to worry about the conversion because we've already been clipped to the frame
		ImageToLocalPoint( updateUL, ul );
		ImageToLocalPoint( updateLR, lr );
		LocalToPortPoint( ul );
		LocalToPortPoint( lr );
		
		Rect r;
		r.top = ul.v;
		r.left = ul.h;
		r.bottom = lr.v;
		r.right = lr.h;

		// if there is no line thickness
		if ( inRect->left == inRect->right )
			r.right++;
		else // we're working on rows (horizontal line)
		{
			// increment bottom to ensure it's a valid rect
			XP_ASSERT( inRect->top == inRect->bottom );
			r.bottom++;
		}
		
		::InvalRect( &r );
		
		// force redraw in cases where dragging
		UpdatePort();
	}
	else
	{
		StColorPenState state;
		PenPat( &qd.gray );
		PenMode( srcXor );
		
		::MoveTo( inRect->left, inRect->top );
		::LineTo( inRect->right, inRect->bottom );
	}
}


void CEditView::DisplayHR( int inLocation, LO_HorizRuleStruct* hrule )
{
	if ( hrule->edit_offset != -1 || mDisplayParagraphMarks )
		CHTMLView::DisplayHR( inLocation, hrule );
}


void CEditView::HandleCut()
{
	char *ppText = NULL, *ppHtml = NULL;
	int32	pTextLen = 0, pHtmlLen = 0;
	
	if ( EDT_COP_OK != EDT_CutSelection( *GetContext(), &ppText, &pTextLen, &ppHtml, &pHtmlLen )  )
	{
		ErrorManager::PlainAlert( CUT_ACROSS_CELLS );
	}

	try {
		LClipboard::GetClipboard()->SetData( 'TEXT', ppText, pTextLen, true );
		LClipboard::GetClipboard()->SetData( 'EHTM', ppHtml, pHtmlLen, false );
	}
	catch(...)
	{}
	
	XP_FREEIF( ppText );
	XP_FREEIF( ppHtml );
}


void CEditView::HandleCopy()
{
	char *ppText = NULL, *ppHtml = NULL;
	int32 pTextLen = 0, pHtmlLen = 0;
	
	if ( EDT_COP_OK != EDT_CopySelection( *GetContext(), &ppText, &pTextLen, &ppHtml, &pHtmlLen ) )
	{
		ErrorManager::PlainAlert( CUT_ACROSS_CELLS );
	}
	
	try {
		LClipboard::GetClipboard()->SetData( 'TEXT', ppText, pTextLen, true );
		LClipboard::GetClipboard()->SetData( 'EHTM', ppHtml, pHtmlLen, false );
	}
	catch(...)
	{}
	
	XP_FREEIF( ppText );
	XP_FREEIF( ppHtml );
}


void CEditView::HandlePaste()
{
	int32 dataLength;
				
/* internal format */
	dataLength = LClipboard::GetClipboard()->GetData( 'EHTM', NULL );
	if ( dataLength > 0 )
	{
		Handle htmlHandle = ::NewHandle( dataLength + 1 );
		if ( htmlHandle )
		{
			LClipboard::GetClipboard()->GetData( 'EHTM', htmlHandle );
			::HLock( htmlHandle );
			(*htmlHandle)[ dataLength ] = '\0';
			int result = EDT_PasteHTML( *GetContext(), *htmlHandle );
			::DisposeHandle( htmlHandle );
			return;
		}
	}

/* text */
	dataLength = LClipboard::GetClipboard()->GetData( 'TEXT', NULL );
	if ( dataLength > 0 )
	{
		Handle textHandle = ::NewHandle( dataLength + 1 );
		if ( textHandle )
		{
			LClipboard::GetClipboard()->GetData( 'TEXT', textHandle );
			::HLock( textHandle );
			(*textHandle)[ dataLength ] = '\0';
			int result = EDT_PasteText( *GetContext(), *textHandle );
			::DisposeHandle( textHandle );
			return;
		}
	}

/* The rest is all PICT (ugh!) */
	dataLength = LClipboard::GetClipboard()->GetData( 'PICT', NULL );
	if ( dataLength <= 0 )
		return;
	
	// check for quicktime and jpeg compressor
	Int32	response;
	if ( ::Gestalt( gestaltQuickTime, &response ) != noErr
	|| ::Gestalt( gestaltCompressionMgr, &response ) != noErr )
		return;
	
	// get location where to save new jpeg file
    StandardFileReply sfReply;
    Str255 saveprompt;
    
	// Because resource changes are expensive (right now) for localization
	// this string was added to a "strange" place
	::GetIndString( sfReply.sfFile.name, STRPOUND_EDITOR_MENUS, 18 );
	::GetIndString( saveprompt, STRPOUND_EDITOR_MENUS, 19 );

	::UDesktop::Deactivate();
	::StandardPutFile( saveprompt[0] ? saveprompt : NULL, sfReply.sfFile.name, &sfReply );
	::UDesktop::Activate();

	if ( !sfReply.sfGood )
		return;
	
	Handle pictDataHandle = ::NewHandle( dataLength );
	if ( pictDataHandle == NULL )
		return;
	::HUnlock( pictDataHandle );
	
	LClipboard::GetClipboard()->GetData( 'PICT', pictDataHandle );
	
	OSErr err;
	if ( !sfReply.sfReplacing )
	{
		// create file
		err = FSpCreateCompat( &sfReply.sfFile, emSignature, 'JPEG', smSystemScript );
		if ( err )
		{
			::DisposeHandle( pictDataHandle );
			return;
		}
	}
	
	// open file
	short refNum;
	err = ::FSpOpenDFCompat( &sfReply.sfFile, fsRdWrPerm, &refNum );
	if ( err )
	{
		if ( !sfReply.sfReplacing )
			FSpDeleteCompat( &sfReply.sfFile );
		::DisposeHandle( pictDataHandle );
		return;
	}
	
	// write out header
	int zero = 0;
	long actualSizeWritten = sizeof( zero );
	for ( int count = 1; actualSizeWritten && (count <= (512 / actualSizeWritten)) && err == noErr; count++ )
		err = ::FSWrite( refNum, &actualSizeWritten, &zero );
	
	// write out data
	::HLock( pictDataHandle );
	actualSizeWritten = dataLength;
	err = FSWriteVerify( refNum, &actualSizeWritten, *pictDataHandle );
	DisposeHandle( pictDataHandle );
	if ( err )
	{
		::FSClose( refNum );
		FSpDeleteCompat( &sfReply.sfFile );
		return;
	}
	
	// overwrite file in place
	err = ::CompressPictureFile( refNum, refNum, codecNormalQuality, 'jpeg' );
	if ( err != noErr )
	{
		::FSClose( refNum );
		FSpDeleteCompat( &sfReply.sfFile );
		return;
	}
	
	// re-write file as jpeg (not PICT)
	long totalSize, bytesToWrite;
	char buffer[512];
	char jfifMarker1[3] = { 0xFF, 0xD8, 0xFF };
	char jfifMarker2[4] = { 'J', 'F', 'I', 'F' };
	
	::SetFPos( refNum, fsFromLEOF, 0 );
	::GetFPos( refNum, &totalSize );
	::SetFPos( refNum, fsFromStart, 512 );
	
	// skip over remainder of non-JFIF header
	// KNOWN BUG:  if JFIF header is split over two FSReads we'll miss it in the loop below
	Boolean headerFound = false;
	while ( err == noErr && !headerFound )
	{
		bytesToWrite = 512;
		err = ::FSRead( refNum, &bytesToWrite, buffer );
		if ( err == noErr )
		{
			int index = 0;
			for ( index = 0; index < bytesToWrite - sizeof( jfifMarker1 ); index++ )
			{
				if ( 0 == memcmp( &buffer[ index ], jfifMarker1, sizeof(jfifMarker1) ) )
				{
					if ( index + 4 + 2 > bytesToWrite // assume it's there without checking
					|| 0 == memcmp( &buffer[ index + 6 ], jfifMarker2, sizeof(jfifMarker2) ) )
					{
						// move fpos back to the start of jfifMarker
						err = ::SetFPos( refNum, fsFromMark, -( bytesToWrite - index) );
						headerFound = true;
						break;
					}
				}
			}
		}
	}
	
	if ( err )	// jfif header not found??
	{
		::FSClose( refNum );
		FSpDeleteCompat( &sfReply.sfFile );
		return;
	}
	
	long fileOffset;
	::GetFPos( refNum, &fileOffset );

	// write remainder (current fpos) as jfif to start of file
	Boolean doneReadingFile = false;
	actualSizeWritten = fileOffset;
	while ( actualSizeWritten < totalSize && err == noErr )
	{
		bytesToWrite = sizeof( buffer );
		err = ::FSRead( refNum, &bytesToWrite, buffer );
		
		// ignore eofErr if we have more bytesToWrite
		if ( err == eofErr && bytesToWrite )
			err = noErr;
		
		// move file pointer back to 512 bytes less than where we began read
		if ( err == noErr )
		{
			::SetFPos( refNum, fsFromStart, actualSizeWritten - fileOffset );
		
			err = ::FSWrite( refNum, &bytesToWrite, buffer );
			
			actualSizeWritten += bytesToWrite;
			::SetFPos( refNum, fsFromStart, actualSizeWritten );
		}
	}
	
	::SetEOF( refNum, actualSizeWritten - fileOffset );
	
	::FSClose( refNum );
	
	// create image link
	EDT_ImageData *image = EDT_NewImageData();
	if ( image == NULL )
		return;
	
	image->pSrc = CFileMgr::GetURLFromFileSpec( sfReply.sfFile );
	if ( image->pSrc )
		EDT_InsertImage( *GetContext(), image, true );
	EDT_FreeImageData( image );
}


void CEditView::AdjustCursorSelf( Point inPortPt, const EventRecord& inMacEvent )
{
	// if this page is tied-up, display a watch
	if (!IsDoneLoading() )
	{
		::SafeSetCursor( watchCursor );
		return;
	}

	//  bail if we did not move, saves time and flickering
	if ( ( inPortPt.h == mOldPoint.h ) && ( inPortPt.v == mOldPoint.v ) )
		return;

	// store for the next time we get called
	mOldPoint.h = inPortPt.h;
	mOldPoint.v = inPortPt.v;

	// find the element the cursor is above
	PortToLocalPoint( inPortPt );
	
	SPoint32 firstP;
	LocalToImagePoint( inPortPt, firstP );
	FocusDraw();								// Debugging only
		
#ifdef LAYERS
	LO_Element* element = LO_XYToElement( *GetContext(), firstP.h, firstP.v, 0 );
#else
	LO_Element* element = LO_XYToElement( *GetContext(), firstP.h, firstP.v );
#endif
	
	// The cursor is over an image or hrule
	if ( element && (element->type == LO_IMAGE || element->type == LO_HRULE) )
	{
		if ( mOldLastElementOver != -1 )
		{
			mOldLastElementOver = element->lo_any.ele_id;
			::SetCursor( &UQDGlobals::GetQDGlobals()->arrow );
	//		mContext->Progress( CStr255::sEmptyString );
			return;
		}
	}

	ED_HitType iTableHit;
	iTableHit = EDT_GetTableHitRegion( *GetContext(), firstP.h, firstP.v, &element, 
										(inMacEvent.modifiers & cmdKey) == cmdKey );
	if ( element )
	{
		int specialElementID = element->lo_any.ele_id + (iTableHit * 0x01000000);
		if ( mOldLastElementOver != specialElementID )
		{
			mOldLastElementOver = specialElementID;
		//	mContext->Progress( CStr255::sEmptyString );

			switch ( iTableHit )
			{
				case ED_HIT_SEL_ALL_CELLS:
				case ED_HIT_SEL_TABLE:			::SafeSetCursor( 12001 );	break;
				case ED_HIT_SEL_COL:			::SafeSetCursor( 12003 );	break;
				case ED_HIT_SEL_ROW:			::SafeSetCursor( 12002 );	break;
				case ED_HIT_SEL_CELL:			::SafeSetCursor( 12000 );	break;
				case ED_HIT_SIZE_TABLE_WIDTH:	::SafeSetCursor( 12005 );	break;
				case ED_HIT_SIZE_TABLE_HEIGHT:	::SafeSetCursor( 12004 );	break;
				case ED_HIT_SIZE_COL:			::SafeSetCursor( 130 );		break;
				case ED_HIT_ADD_ROWS:			::SafeSetCursor( 12006 );	break;
				case ED_HIT_ADD_COLS:			::SafeSetCursor( 12007 );	break;
				default:		::SafeSetCursor( iBeamCursor );
			}
		}
	}
	else
	{
		// The cursor is over blank space
		if ( mOldLastElementOver != -1 )
		{
			mOldLastElementOver = -1;
			::SetCursor( &UQDGlobals::GetQDGlobals()->arrow );
		//	::SafeSetCursor( iBeamCursor );		// not sure if I should do this if element is NULL
	//		mContext->Progress( CStr255::sEmptyString );
		}
		else
			::SafeSetCursor( iBeamCursor );
	}
	
}

void CEditView::ListenToMessage( MessageT inMessage, void* ioParam )
{
	switch ( inMessage )
	{
		case cmd_Bold:
		case cmd_Italic:
		case cmd_Underline:
		case cmd_FontSmaller:
		case cmd_FontLarger:
		case cmd_Print:
		case cmd_Format_FontColor:
		case cmd_InsertEditImage:
		case cmd_InsertEditLink:
		case cmd_Insert_Table:
		case cmd_InsertEdit_Target:
		case cmd_Insert_Target:
		case cmd_Insert_Line:
		case msg_MakeNoList:
		case msg_MakeNumList:
		case msg_MakeUnumList:
		case cmd_Format_Paragraph_Indent:
		case cmd_Format_Paragraph_UnIndent:
		case cmd_JustifyLeft:
		
		case cmd_Cut:
		case cmd_Copy:
		case cmd_Paste:
			ObeyCommand( inMessage, ioParam );
			break;

		case cmd_FormatViewerFont:
		case cmd_FormatFixedFont:
			break;
		
		case msg_NSCPEditorRepaginate:
			// turn off repagination to avoid infinite loop when setting prefs
			Boolean isRepaginating = GetRepaginate();
			if ( isRepaginating )
				SetRepaginate( false );

			NoteEditorRepagination();

			// turn off repagination to avoid infinite loop when setting prefs
			if ( isRepaginating )
				SetRepaginate( true );
			break;	

		default:
			if (inMessage >= COLOR_POPUP_MENU_BASE && inMessage <= COLOR_POPUP_MENU_BASE_LAST)
				ObeyCommand( inMessage, ioParam );
		break;

	}
}


Boolean CEditView::ObeyCommand( CommandT inCommand, void *ioParam )
{
	EDT_CharacterData* better;		// used by a bunch of cases.
	
	if (inCommand >= COLOR_POPUP_MENU_BASE && inCommand <= COLOR_POPUP_MENU_BASE_LAST)
	{
		FLUSH_JAPANESE_TEXT
		if ( mColorPopup && mColorPopup->IsEnabled() )
		{						
			if ( CanUseCharFormatting() )
			{
			 	better = EDT_GetCharacterData( *GetContext() );
			 	if ( better )
			 	{
					RGBColor outColor;
					LO_Color pColor = {0, 0, 0};
					short	mItem;
					
					if ( !EDT_GetFontColor( *GetContext(), &pColor ) )
					{	// still a perfectly respectable way to get the font color
						// get default color
						EDT_PageData *pagedata = EDT_GetPageData( *GetContext() );
						if ( pagedata )
						{
							pColor = *pagedata->pColorText;
							EDT_FreePageData( pagedata );
						}
					}
					
					mItem = inCommand - COLOR_POPUP_MENU_BASE + 1;
					if ( mColorPopup->GetMenuItemRGBColor( mItem, &outColor ) == true )
					{
						pColor.red = (outColor.red >> 8);
						pColor.green = (outColor.green >> 8);
						pColor.blue = (outColor.blue >> 8);
						
						better->mask = TF_FONT_COLOR;
						better->values = (mItem == CColorPopup::DEFAULT_COLOR_ITEM) ? 0 : TF_FONT_COLOR;
																// remove font color? or set font color?
						better->pColor = &pColor;
						EDT_BeginBatchChanges( *GetContext() );
						EDT_SetCharacterData( *GetContext(), better );
						EDT_SetFontColor( *GetContext(), &pColor );
						EDT_EndBatchChanges( *GetContext() );
						better->pColor = NULL;
					}
					
					EDT_FreeCharacterData( better );
				}
			}
		}
		
	}

	switch ( inCommand )
	{		
		case cmd_Publish:
			CEditDialog::Start( EDITDLG_PUBLISH, *GetContext(), 1 );
			break;
		
		case cmd_Refresh:
			EDT_RefreshLayout( *GetContext() );
			break;
		
#ifdef MOZ_MAIL_COMPOSE
		case cmd_MailDocument:
			FLUSH_JAPANESE_TEXT
			EDT_MailDocument( *GetContext() );
			break;
#endif
		
		case cmd_ViewSource:
			FLUSH_JAPANESE_TEXT
			EDT_DisplaySource( *GetContext() );
			break;

		case cmd_EditSource:
			FLUSH_JAPANESE_TEXT
			if (VerifySaveUpToDate())
			{
			    // Now that we have saved the document, lets get an FSSpec for it.
				History_entry *hist_ent = SHIST_GetCurrent( &(((MWContext *)*GetContext())->hist) );
				if ( hist_ent == NULL || hist_ent->address == NULL )
					break;
				
			    FSSpec	theDocument;
				if ( noErr != CFileMgr::FSSpecFromLocalUnixPath( 
										hist_ent->address + XP_STRLEN("file://"), &theDocument, false ) )
					break;
			    
			    // Get the FSSpec for the editor
			    FSSpec	theApplication;
			    XP_Bool hasEditor = false;
				PREF_GetBoolPref( "editor.use_html_editor", &hasEditor );
				if ( hasEditor )
			    	theApplication = CPrefs::GetFolderSpec(CPrefs::HTMLEditor);
			    
			    // Oops, the user has not picked an app in the preferences yet.
				if ( !hasEditor || (theApplication.vRefNum == -1 && theApplication.parID == -1) )
				{
					ErrorManager::PlainAlert( NO_SRC_EDITOR_PREF_SET );
					CPrefsDialog::EditPrefs( CPrefsDialog::eExpandEditor, 
													PrefPaneID::eEditor_Main );
					break;
				}
				else			
			    	StartDocInApp( theDocument, theApplication );
			}
			break;
		
		case cmd_Reload:
			FLUSH_JAPANESE_TEXT
			if ( !EDT_IS_NEW_DOCUMENT( ((MWContext *)*GetContext()) ) )
				if ( VerifySaveUpToDate() )
					DoReload();
			break;
		
		case cmd_Remove_Links:
			FLUSH_JAPANESE_TEXT
			EDT_SetHREF( *GetContext(), NULL );
			break;
		
#if 0
		case cmd_DisplayTables:
			EDT_SetDisplayTables( *GetContext(), 
								!EDT_GetDisplayTables( *GetContext() ) );
			break;
#endif
		
		case cmd_Format_Target:
			CEditDialog::Start( EDITDLG_TARGET, *GetContext(), 0, false );
			break;
		
		case cmd_Format_Unknown_Tag:
			CEditDialog::Start( EDITDLG_UNKNOWN_TAG, *GetContext() );
			break;
		
		case cmd_Format_DefaultFontColor:
			FLUSH_JAPANESE_TEXT
			EDT_SetFontColor( *GetContext(), NULL );
			break;
		
		case cmd_DisplayParagraphMarks:
			// update the whole view
			mDisplayParagraphMarks = !mDisplayParagraphMarks;
			if ( IsDoneLoading() )
				EDT_RefreshLayout( *GetContext() );
			break;

// File/Edit Toolbar
		case cmd_Save:
			FLUSH_JAPANESE_TEXT
			SaveDocument();
			break;
		
		case cmd_SaveAs:
			SaveDocumentAs();
			break;
		
		case cmd_BrowseDocument:
			FLUSH_JAPANESE_TEXT
			if ( VerifySaveUpToDate() )
			{
				History_entry* current = SHIST_GetCurrent( &(((MWContext *)*GetContext())->hist) );
				if ( !current )
					break;
				
				URL_Struct * nurl = NULL;
				if ( current && current->address )
					nurl = NET_CreateURLStruct( current->address, NET_DONT_RELOAD );

				if ( !nurl )
					break;

				try
				{
					CURLDispatcher::DispatchURL( nurl, NULL );
				}
				catch(...) { }
			}
			break;
		
		case cmd_Cut:
			FLUSH_JAPANESE_TEXT
			HandleCut();
			break;
		
		case cmd_Copy:
			FLUSH_JAPANESE_TEXT
			HandleCopy();
			break;
		
		case cmd_Paste:
			FLUSH_JAPANESE_TEXT
			HandlePaste();
			break;
		
		case cmd_Clear:
			FLUSH_JAPANESE_TEXT
			if ( EDT_CanCut( *GetContext(), true ) == EDT_COP_OK )
				EDT_DeletePreviousChar( *GetContext() );
			break;

		case cmd_SelectAll:
			FLUSH_JAPANESE_TEXT
			EDT_SelectAll( *GetContext() );
			break;

		case cmd_Print:
		case cmd_PrintOne:
			FLUSH_JAPANESE_TEXT
			DoPrintCommand( inCommand );
			break;
		
// Character Toolbar
		case cmd_FontSmaller:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
				EDT_SetFontSize( *GetContext(), 
									EDT_GetFontSize( *GetContext()) - 1 );
			break;
		
		case cmd_FontLarger:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
				EDT_SetFontSize( *GetContext(), 
									EDT_GetFontSize( *GetContext()) + 1 );
			break;
		
		case cmd_Bold:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
			{
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					better->mask = TF_BOLD;
					better->values ^= TF_BOLD;
					EDT_SetCharacterData( *GetContext(), better );
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Italic:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
			{
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					better->mask = TF_ITALIC;
					better->values ^= TF_ITALIC;
					EDT_SetCharacterData( *GetContext(), better );
					EDT_FreeCharacterData( better );
				}
			}
			break;

		case cmd_Underline:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
			{
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					better->mask = TF_UNDERLINE;
					better->values ^= TF_UNDERLINE;
					EDT_SetCharacterData( *GetContext(), better );
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Format_Character_Strikeout:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
			{
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					better->mask = TF_STRIKEOUT;
					better->values ^= TF_STRIKEOUT;
					EDT_SetCharacterData( *GetContext(), better );
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Format_FontColor:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
			{
			 	better = EDT_GetCharacterData( *GetContext() );
			 	if ( better )
			 	{
					Point where;
					where.h = where.v = 0;
					RGBColor inColor;
					RGBColor outColor;
					LO_Color pColor = {0, 0, 0};
					
					if ( !EDT_GetFontColor( *GetContext(), &pColor ) )
					{		// still a perfectly respectable way to get the font color....
							// get default color
						EDT_PageData *pagedata = EDT_GetPageData( *GetContext() );
						pColor = *pagedata->pColorText;
						EDT_FreePageData( pagedata );
					}
					
					inColor.red = (pColor.red << 8);
					inColor.green = (pColor.green << 8);
					inColor.blue = (pColor.blue << 8);
					
					if ( ::GetColor( where, (unsigned char *)*GetString(PICK_COLOR_RESID),
											&inColor, &outColor ) )
					{
						pColor.red = (outColor.red >> 8);
						pColor.green = (outColor.green >> 8);
						pColor.blue = (outColor.blue >> 8);
						
						better->mask = TF_FONT_COLOR;
						better->values = TF_FONT_COLOR;
						better->pColor = &pColor;
						EDT_BeginBatchChanges( *GetContext() );
						EDT_SetCharacterData( *GetContext(), better );
						EDT_SetFontColor( *GetContext(), &pColor );
						EDT_EndBatchChanges( *GetContext() );
						better->pColor = NULL;
						
						// set control to the color that just got picked
						Str255 colorstr;
						XP_SPRINTF( (char *)&colorstr[2], "%02X%02X%02X", pColor.red, pColor.green, pColor.blue);
						colorstr[1] = CColorPopup::CURRENT_COLOR_CHAR;	// put in leading character
						colorstr[0] = strlen( (char *)&colorstr[1] );
						mColorPopup->SetDescriptor( colorstr );
						::SetMenuItemText( mColorPopup->GetMenu()->GetMacMenuH(), 
											CColorPopup::CURRENT_COLOR_ITEM, 
											(unsigned char *)&colorstr );
					}
					EDT_FreeCharacterData( better );
				}
			}
			break;

		case cmd_Format_Character_ClearAll:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
				if ( !EDT_IsSelected( *GetContext() )
				|| !EDT_SelectionContainsLink( *GetContext() )		// Detect if any links are included and warn user
				|| HandleModalDialog( EDITDLG_CONFIRM_OBLITERATE_LINK, NULL,  NULL) == ok )
					EDT_FormatCharacter( *GetContext(), TF_NONE );
			break;
		
		case cmd_InsertEdit_Target:
			CEditDialog::Start( EDITDLG_TARGET, *GetContext(), 0, 
								EDT_GetCurrentElementType( *GetContext() ) != ED_ELEMENT_TARGET );
			break;
		
		case cmd_Format_Text:
			if ( CanUseCharFormatting() )
			{
				if ( EDT_CanSetHREF( *GetContext() ) && EDT_GetHREF( *GetContext() ) )
					CEditDialog::Start( EDITDLG_TEXT_AND_LINK, *GetContext(), 2 );
				else
					CEditDialog::Start( EDITDLG_TEXT_STYLE, *GetContext(), 1 );
			}
			else 
			{
				ED_ElementType edtElemType = EDT_GetCurrentElementType( *GetContext() );
				if ( ED_ELEMENT_IMAGE == edtElemType )
					CEditDialog::Start( EDITDLG_IMAGE, *GetContext(), 1 );
				else if ( ED_ELEMENT_HRULE == edtElemType )
					CEditDialog::Start( EDITDLG_LINE_FORMAT, *GetContext() );
				else if ( ED_ELEMENT_UNKNOWN_TAG == edtElemType )
					CEditDialog::Start( EDITDLG_UNKNOWN_TAG, *GetContext() );
				else if ( ED_ELEMENT_TARGET == edtElemType )
					CEditDialog::Start( EDITDLG_TARGET, *GetContext() ); 		
			}
			break;

// Paragraph Toolbar
		case msg_MakeNoList:
			ToFromList( 0, ED_LIST_TYPE_DEFAULT );
			break;
		
		case msg_MakeNumList:
			ToFromList( P_NUM_LIST, ED_LIST_TYPE_DIGIT );
			break;
		
		case msg_MakeUnumList:
			ToFromList( P_UNUM_LIST, ED_LIST_TYPE_DEFAULT );
			break;
		
		case cmd_CheckSpelling:
			FLUSH_JAPANESE_TEXT
			do_spellcheck( *GetContext(), this, NULL );
			break;
		
		case cmd_Format_Paragraph_Indent:
			EDT_Indent( *GetContext() );
			break;
		
		case cmd_Format_Paragraph_UnIndent:
			EDT_Outdent( *GetContext() );
			break;
		
		case cmd_JustifyLeft:
			if ( EDT_GetCurrentElementType( *GetContext() ) == ED_ELEMENT_HRULE )
			{
				EDT_HorizRuleData *fData = EDT_GetHorizRuleData( *GetContext() );
				if ( fData )
				{
					fData->align = ED_ALIGN_LEFT;
					EDT_SetHorizRuleData( *GetContext(), fData );
					EDT_FreeHorizRuleData( fData );
				}
			}
			else
				EDT_SetParagraphAlign( *GetContext(), ED_ALIGN_LEFT );
			break;

		case cmd_JustifyCenter:
			if ( EDT_GetCurrentElementType( *GetContext() ) == ED_ELEMENT_HRULE )
			{
				EDT_HorizRuleData *fData = EDT_GetHorizRuleData( *GetContext() );
				if ( fData )
				{
					fData->align = ED_ALIGN_DEFAULT;
					EDT_SetHorizRuleData( *GetContext(), fData );
					EDT_FreeHorizRuleData( fData );
				}

			}
			else
				EDT_SetParagraphAlign( *GetContext(), ED_ALIGN_CENTER );
			break;
		
		case cmd_JustifyRight:
			if ( EDT_GetCurrentElementType( *GetContext() ) == ED_ELEMENT_HRULE )
			{
				EDT_HorizRuleData *fData = EDT_GetHorizRuleData( *GetContext() );
				if ( fData )
				{
					fData->align = ED_ALIGN_RIGHT;
					EDT_SetHorizRuleData( *GetContext(), fData );
					EDT_FreeHorizRuleData( fData );
				}
			}
			else
				EDT_SetParagraphAlign( *GetContext(), ED_ALIGN_RIGHT );
			break;
		
// Edit Menu
		case cmd_Undo:
			FLUSH_JAPANESE_TEXT
			EDT_Undo( *GetContext() );
			SetMenuCommandAndString( inCommand, cmd_Redo, STRPOUND_EDITOR_MENUS, EDITOR_MENU_REDO, NULL );
			break;
		
		case cmd_Redo:
			FLUSH_JAPANESE_TEXT
			EDT_Redo( *GetContext() );
			SetMenuCommandAndString( inCommand, cmd_Undo, STRPOUND_EDITOR_MENUS, EDITOR_MENU_UNDO, NULL );
			break;
		
// Insert Menu
		case cmd_Insert_Target:
			CEditDialog::Start( EDITDLG_TARGET, *GetContext(), 0, true );
			break;

		case cmd_Insert_Unknown_Tag:
			CEditDialog::Start( EDITDLG_UNKNOWN_TAG, *GetContext(), 0, true );
			break;
		
		case cmd_Insert_Link:
			CEditDialog::Start( EDITDLG_TEXT_AND_LINK, *GetContext(), 2, true );
			break;

		case cmd_InsertEditLink:
			if ( EDT_CanSetHREF( *GetContext() ) )
			{
				if ( EDT_GetCurrentElementType( *GetContext() ) == ED_ELEMENT_IMAGE )
					CEditDialog::Start( EDITDLG_IMAGE, *GetContext(), 2 );
				else 
				{
					if ( EDT_IsSelected( *GetContext() ) )
						CEditDialog::Start( EDITDLG_TEXT_AND_LINK, *GetContext(), 2 );
					else
						CEditDialog::Start( EDITDLG_TEXT_AND_LINK, *GetContext(), 2, true );
				}
			} else
				CEditDialog::Start( EDITDLG_TEXT_AND_LINK, *GetContext(), 2, true );
			break;
		
		case cmd_Insert_Image:
			CEditDialog::Start( EDITDLG_IMAGE, *GetContext(), 1, true );
			break;
		
		case cmd_InsertEditImage:
			if ( EDT_GetCurrentElementType( *GetContext() ) == ED_ELEMENT_IMAGE )
				CEditDialog::Start( EDITDLG_IMAGE, *GetContext(), 1, false );
			else
				CEditDialog::Start( EDITDLG_IMAGE, *GetContext(), 1, true );
			break;
		
		case cmd_Insert_Line:
			FLUSH_JAPANESE_TEXT
			InsertDefaultLine();
			break;
		
		case cmd_InsertEditLine:
			FLUSH_JAPANESE_TEXT
			if ( EDT_GetCurrentElementType( *GetContext() ) == ED_ELEMENT_HRULE )
				CEditDialog::Start( EDITDLG_LINE_FORMAT, *GetContext() );
			else
				InsertDefaultLine();
			break;

		case cmd_Insert_LineBreak:
			FLUSH_JAPANESE_TEXT
			EDT_InsertBreak( *GetContext(), ED_BREAK_NORMAL );
			break;
		
		case cmd_Insert_BreakBelowImage:
			FLUSH_JAPANESE_TEXT
			EDT_InsertBreak( *GetContext(), ED_BREAK_RIGHT );
			break;
		
#if 0
		case cmd_Insert_NonbreakingSpace:
			FLUSH_JAPANESE_TEXT
			EDT_InsertNonbreakingSpace( *GetContext() );
			break;
#endif
		
// Format Menu
		case cmd_Format_Document:
			CEditDialog::Start( EDITDLG_DOC_INFO, *GetContext(), 2 );
			break;
		
		case cmd_Format_PageTitle:
			CEditDialog::Start( EDITDLG_DOC_INFO, *GetContext(), 1 );
			break;
		
// Format Character Menu		
		case cmd_Format_Character_Nonbreaking:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
			{
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					better->mask = TF_NOBREAK;
					better->values ^= TF_NOBREAK;
					EDT_SetCharacterData( *GetContext(), better );
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Format_Character_Superscript:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
			{
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					better->mask = TF_SUPER | TF_SUB;
					better->values ^= TF_SUPER;
					if (better->values & TF_SUB)
						better->values ^= TF_SUB;
					EDT_SetCharacterData( *GetContext(), better );
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Format_Character_Subscript:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
			{
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					better->mask = TF_SUB | TF_SUPER;
					better->values ^= TF_SUB;
					if ( better->values & TF_SUPER )
						better->values ^= TF_SUPER;
					EDT_SetCharacterData( *GetContext(), better );
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
		case cmd_Format_Character_Blink:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
			{
				better = EDT_GetCharacterData( *GetContext() );
				if ( better )
				{
					better->mask = TF_BLINK;
					better->values ^= TF_BLINK;
					EDT_SetCharacterData( *GetContext(), better );
					EDT_FreeCharacterData( better );
				}
			}
			break;
		
// Format Paragraph Menu		
		case cmd_Format_Paragraph_Normal:
			EDT_MorphContainer( *GetContext(), P_NSDT );
			break;
		
		case cmd_Format_Paragraph_Head1:
		case cmd_Format_Paragraph_Head2:
		case cmd_Format_Paragraph_Head3:
		case cmd_Format_Paragraph_Head4:
		case cmd_Format_Paragraph_Head5:
		case cmd_Format_Paragraph_Head6:
			EDT_MorphContainer( *GetContext(), inCommand - cmd_Format_Paragraph_Head1 + P_HEADER_1 );
			break;
		
		case cmd_Format_Paragraph_Address:
			EDT_MorphContainer( *GetContext(), P_ADDRESS );
			break;
		
		case cmd_Format_Paragraph_Formatted:
			EDT_MorphContainer( *GetContext(), P_PREFORMAT );
			break;
		
		case cmd_Format_Paragraph_List_Item:
			EDT_MorphContainer( *GetContext(), P_LIST_ITEM );
			break;
		
		case cmd_Format_Paragraph_Desc_Title:
			EDT_MorphContainer( *GetContext(), P_DESC_TITLE );
			break;
		
		case cmd_Format_Paragraph_Desc_Text:
			EDT_MorphContainer( *GetContext(), P_DESC_TEXT );
			break;
		
// Format Font Size Menu		
		case cmd_Format_Font_Size_N2:
		case cmd_Format_Font_Size_N1:
		case cmd_Format_Font_Size_0:
		case cmd_Format_Font_Size_P1:
		case cmd_Format_Font_Size_P2:
		case cmd_Format_Font_Size_P3:
		case cmd_Format_Font_Size_P4:
			FLUSH_JAPANESE_TEXT
			if ( CanUseCharFormatting() )
				EDT_SetFontSize( *GetContext(), inCommand - cmd_Format_Font_Size_N2 + 1 );
			break;

// Format Table Menu
		case cmd_Insert_Table:
			CEditDialog::Start( EDITDLG_TABLE, *GetContext(), 0, true );
			break;
		
		case cmd_Delete_Table:
			FLUSH_JAPANESE_TEXT
			EDT_DeleteTable( *GetContext() );
			break;
		
		case cmd_Select_Table:
			FLUSH_JAPANESE_TEXT
			EDT_SelectTable( *GetContext() );
			break;
		
		case cmd_Format_Table:
			Boolean acrossCells = ( EDT_COP_SELECTION_CROSSES_TABLE_DATA_CELL == 
									EDT_CanCut( *GetContext(), true ));

			if ( !acrossCells && EDT_IsInsertPointInTableCell( *GetContext() ))
				CEditDialog::Start( EDITDLG_TABLE_INFO, *GetContext(), 3 );
			
			else if ( EDT_IsInsertPointInTableRow( *GetContext() ))
				CEditDialog::Start( EDITDLG_TABLE_INFO, *GetContext(), 2 );
			
			else if ( EDT_IsInsertPointInTable( *GetContext() ))
				CEditDialog::Start( EDITDLG_TABLE_INFO, *GetContext(), 1 );
			break;
		
		case cmd_Insert_Row:
			FLUSH_JAPANESE_TEXT
			{
			EDT_TableRowData *pData = EDT_NewTableRowData();
			if ( pData )
			{
				pData->align = ED_ALIGN_DEFAULT;
				pData->valign = ED_ALIGN_DEFAULT;
				EDT_InsertTableRows( *GetContext(), pData, true, 1 );
				EDT_FreeTableRowData( pData );
			}
			}
			break;
		
		case cmd_Delete_Row:
			FLUSH_JAPANESE_TEXT
			EDT_DeleteTableRows( *GetContext(), 1 );
			break;
		
		case cmd_Format_Row:
			CEditDialog::Start( EDITDLG_TABLE_INFO, *GetContext(), 2 );
			break;
		
		case cmd_Insert_Col:
			FLUSH_JAPANESE_TEXT
			{
			EDT_TableCellData* pData = EDT_GetTableCellData( *GetContext() );
			if ( pData )
			{
				EDT_InsertTableColumns( *GetContext(), pData, true, 1 );
				EDT_FreeTableCellData( pData );
			}
			}
			break;
		
		case cmd_Delete_Col:
			FLUSH_JAPANESE_TEXT
			EDT_DeleteTableColumns( *GetContext(), 1 );
			break;
		
		case cmd_Insert_Cell:
			FLUSH_JAPANESE_TEXT
			{
			EDT_TableCellData* pData = EDT_GetTableCellData( *GetContext() );
			if ( pData )
			{
				EDT_InsertTableCells( *GetContext(), pData, true, 1 );
				EDT_FreeTableCellData( pData );
			}
			}
			break;

		case cmd_Delete_Cell:
			FLUSH_JAPANESE_TEXT
			EDT_DeleteTableCells( *GetContext(), 1 );
			break;
		
		case cmd_Format_Cell:
			CEditDialog::Start( EDITDLG_TABLE_INFO, *GetContext(), 3 );
			break;
					
#if 0
		case cmd_DisplayTableBoundaries:
			Boolean doDisplayTableBorders;
			// toggle the current state and reset editor view
			doDisplayTableBorders = !EDT_GetDisplayTables( *GetContext() );
			EDT_SetDisplayTables( *GetContext(), doDisplayTableBorders );
		break;
#endif

		case msg_TabSelect:
			FLUSH_JAPANESE_TEXT
			mCaretActive = true;
			return true;	// yes, we'll take a tab so we can be the active field!
			break;

		case cmd_FormatColorsAndImage:
			CEditDialog::Start( EDITDLG_BKGD_AND_COLORS, *GetContext() );
			break;
		
		case cmd_DocumentInfo:
			if ( IsDoneLoading() && !EDT_IS_NEW_DOCUMENT( ((MWContext *)*GetContext()) ) )
				return CHTMLView::ObeyCommand( inCommand, ioParam );
			break;

		default:
			return CHTMLView::ObeyCommand (inCommand, ioParam);
		}
	
	return true;
}


Boolean CEditView::SetDefaultCSID(Int16 prefCSID)
{
	int oldCSID = mContext->GetDefaultCSID();

	if ( prefCSID != oldCSID )
	{
		FLUSH_JAPANESE_TEXT

		mContext->SetDefaultCSID( prefCSID );
		mContext->SetWinCSID(INTL_DocToWinCharSetID( prefCSID ));

		if ( !EDT_SetEncoding( *GetContext(), prefCSID ))
		{
			// revert back!
			mContext->SetDefaultCSID( oldCSID );
			mContext->SetWinCSID( INTL_DocToWinCharSetID( oldCSID ) );
			return false;
		}
		
		EDT_RefreshLayout( *GetContext() );
	}
	return true;
}


void FE_DocumentChanged( MWContext * context, int32 iStartY, int32 iHeight )
{
	((CEditView *)ExtractHyperView(context))->DocumentChanged( iStartY, iHeight );
}


Boolean GetCaretPosition(MWContext *context, LO_Element * element, int32 caretPos, 
									int32* caretX, int32* caretYLow, int32* caretYHigh )
{
    if (!context || !element )
        return false;

    int32 xVal = element->lo_any.x + element->lo_any.x_offset;
    int32 yVal = element->lo_any.y + element->lo_any.y_offset;
    int32 yValHigh = yVal + element->lo_any.height;

    switch ( element->type )
    {
		case LO_TEXT:
		{
			LO_TextStruct *text_data = & element->lo_text;
			CCharSet 	charSet = ExtractHyperView(context)->GetCharSet();
			
			XP_FAIL_ASSERT( text_data->text_attr );
				
			GrafPtr theSavePort = NULL;	
			if ( ExtractHyperView(context)->GetCachedPort() != qd.thePort )
			{
				theSavePort = qd.thePort;
				::SetPort( ExtractHyperView(context)->GetCachedPort() );
			}

			HyperStyle style( context, &charSet, text_data->text_attr, true );
		
			style.Apply();
			
			// **** Don't know why this was here...
			//style.GetFontInfo();
			
			// • measure the text
			char *textPtr;
			PA_LOCK( textPtr, char*, text_data->text );

			xVal += style.TextWidth( textPtr, 0, caretPos ) - 1;

			PA_UNLOCK( text_data->text );
			
			if ( theSavePort != NULL )
				::SetPort( theSavePort );
		}
		break;

		case LO_IMAGE:
		{
			LO_ImageStruct *pLoImage = &element->lo_image;
			if ( caretPos == 0 )
				xVal -= 1;
			else
				xVal += pLoImage->width + 2 * pLoImage->border_width;
		}
		break;

		default:
		{
			LO_Any *any = &element->lo_any;
			if ( caretPos == 0 )
				xVal -= 1;
			else
				xVal += any->width;
		}
		break;
	}

	*caretX = xVal;
	*caretYLow = yVal;
	*caretYHigh = yValHigh;

   return true;
}


static void DisplayCaretAnyElement( MWContext *context, LO_Element *element, int caretPos )
{
	CEditView *myView = (CEditView *)ExtractHyperView(context);
	if ( (myView == NULL) || !(myView->FocusDraw()) )
		return;

    int32 xVal, yVal, yValHigh;

	Boolean gotPosition;
	gotPosition = GetCaretPosition( context, element, caretPos, &xVal, &yVal, &yValHigh );
	if ( gotPosition && context->is_editor )
		myView->PlaceCaret( xVal, yVal, yValHigh );	
}


void FE_DisplayGenericCaret( MWContext * context, LO_Any * pLoAny, ED_CaretObjectPosition caretPos )
{
	DisplayCaretAnyElement( context, (LO_Element *)pLoAny, caretPos );
}

void FE_DisplayTextCaret( MWContext * context, int /*loc*/, LO_TextStruct * text_data, int char_offset )
{
	DisplayCaretAnyElement( context, (LO_Element *)text_data, char_offset );
}

void FE_DisplayImageCaret(MWContext * context, LO_ImageStruct * pImageData, ED_CaretObjectPosition caretPos )
{
	DisplayCaretAnyElement( context, (LO_Element *)pImageData, caretPos );
}


Bool FE_GetCaretPosition(MWContext *context, LO_Position* where, int32* caretX, int32* caretYLow, int32* caretYHigh)
{
	return GetCaretPosition( context, where->element, where->position, caretX, caretYLow, caretYHigh );
}


void FE_DestroyCaret( MWContext *pContext )
{
	if ( pContext->is_editor )
		((CEditView *)ExtractHyperView(pContext))->RemoveCaret();	
}

void FE_GetDocAndWindowPosition( MWContext * context, int32 *pX, int32 *pY, int32 *pWidth, int32 *pHeight )
{
	SPoint32		frameLocation;
	SPoint32		imageLocation;
	SDimension16	frameSize;

	((CEditView *)ExtractHyperView(context))->GetDocAndWindowPosition( frameLocation, imageLocation, frameSize );
		
	*pX = frameLocation.h - imageLocation.h;
	*pY = frameLocation.v - imageLocation.v;
	*pWidth = frameSize.width;
	*pHeight = frameSize.height;
}
