/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CWASTEEdit.h"
#include "CSimpleTextView.h"

//#include <LTextModel.h>
//#include <LTextSelection.h>
//#include <LTextEditHandler.h>
//#include <LTextElemAEOM.h>
//#include <VStyleSet.h>
//#include <VOTextModel.h>

#include <UCursor.h>

#include "CSpellChecker.h"
#include "resgui.h" // for emSignature

//#include "Textension.h"

//#pragma global_optimizer off			//why???


Boolean 							CSimpleTextView::sInitialized 	= false;
Boolean 							CSimpleTextView::sHasTSM 				= false;
WETSMPreUpdateUPP		 	CSimpleTextView::sPreUpdateUPP 	= NewWETSMPreUpdateProc( CSimpleTextView::TSMPreUpdate );
CSimpleTextView 			*CSimpleTextView::sTargetView 	= NULL;


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥¥¥ 	Construction/Destruction
#pragma mark --- Construction/Destruction ---


CSimpleTextView::CSimpleTextView()
	{

		if ( !sInitialized )
			Initialize();

		Assert_( false );	//	Must use parameters

		mTabIntoFieldSelectsAll = true;		//default behaviour
	}


CSimpleTextView::CSimpleTextView( LStream *inStream )

	:	CWASTEEdit( inStream )

	{
		
		if ( !sInitialized )
			Initialize();

		
		mTabIntoFieldSelectsAll = true;		//default behaviour

/*			
		mTextParms = NULL;
		
		try
			{
			
				// This is the first run at a new text scheme.
				// The streaming parameters will probably change
				mTextParms = new SSimpleTextParms;
				inStream->ReadData(&mTextParms->attributes, sizeof(Uint16));
				inStream->ReadData(&mTextParms->wrappingMode, sizeof(Uint16));
				inStream->ReadData(&mTextParms->traitsID, sizeof(ResIDT));
				inStream->ReadData(&mTextParms->textID, sizeof(ResIDT));
				inStream->ReadData(&mTextParms->pageWidth, sizeof(Int32));
				inStream->ReadData(&mTextParms->pageHeight, sizeof(Int32));
				inStream->ReadData(&mTextParms->margins, sizeof(Rect));
			
			}
		
		catch ( ... )
			{
			
				// this is either a streaming error or we couldn't get the memory for
				// a text parm structure, in which case you're totally snookered.
				delete mTextParms;
				throw;
			
			}
*/			
	}


CSimpleTextView::~CSimpleTextView()
	{
	
	}


// Wrap fix page size.
//  turn word wrap off
//  set image size

// Wrap to frame

void 
CSimpleTextView::FinishCreateSelf( void )
	{

//	Assert_( mTextParms != NULL );

//  Install a pre-update UPP so we can focus the view 
//  correctly before the TSM does it's magic.

		if ( sHasTSM )
			WESetInfo( weTSMPreUpdate, &sPreUpdateUPP, GetWEHandle() );

//  Turn this feature off. In CWASTEEdit, unfortunately, it is always
//  turned on.

		WEFeatureFlag( weFOutlineHilite, weBitClear, GetWEHandle() );
		
/*	
		switch (mTextParms->wrappingMode)
			{
				
				case wrapMode_WrapToFrame:

					mTextParms->attributes |= textAttr_WordWrap;
					
				break;
				
				case wrapMode_FixedPage:
					
					Assert_(false);	// not yet implemented
					
				break;
				
				case wrapMode_FixedPageWrap:
					
					mTextParms->attributes &= ~textAttr_WordWrap;
					ResizeImageTo(mTextParms->pageWidth, mTextParms->pageHeight, false);
					
				break;
				
			}
*/
			
		Inherited::FinishCreateSelf();

	}


/*
void CSimpleTextView::BuildTextObjects(LModelObject *inSuperModel)
{
	Assert_(mTextParms != NULL);
	VTextView::BuildTextObjects(inSuperModel);

	StRecalculator	change(mTextEngine);

	mTextEngine->SetAttributes(mTextParms->attributes);

	SetInitialTraits(mTextParms->traitsID);
	if (mTextParms->textID != 0)
		SetInitialText(mTextParms->textID);
		
	mTextEngine->SetAttributes(mTextParms->attributes);	//	restore editable/selectable bits

	if (!(mTextParms->attributes & textAttr_Selectable))
		SetSelection(NULL);
		
	mTextEngine->SetTextMargins(mTextParms->margins);
	
	VOTextModel* theModel = dynamic_cast<VOTextModel*>(GetTextModel());
	theModel->GetStyleManager().SetBehavior(setBehavior_Ignore);
	
	delete mTextParms;
	mTextParms = NULL;
}
*/


/*
void CSimpleTextView::NoteOverNewThing(LManipulator *inThing)
{
	if (inThing)
		{
		switch(inThing->ItemType())
			{
			case kManipulator:
				UCursor::Reset();	//	?
				break;

			case kSelection:
				if (mEventHandler && ((LDataDragEventHandler *)mEventHandler)->GetStartsDataDrags())
					{
					CursHandle theCursH = ::GetCursor(131);
					if (theCursH != nil)
						::SetCursor(*theCursH);
					break;
					}
				else
					{
					//	fall through
					}

			case kSelectableItem:
				UCursor::Tick(cu_IBeam);	//	?
				break;

			}
		}
	else
		{
		UCursor::Reset();
		}
}
*/


Boolean 
CSimpleTextView::ObeyCommand( CommandT inCommand, void *ioParam )
	{
		Boolean		cmdHandled = true;
	
		switch (inCommand)
			{
		
		#ifdef MOZ_SPELLCHK
				case cmd_CheckSpelling:
					do_spellcheck(NULL, NULL, this);
					return true;
					break;
		#endif // MOZ_SPELLCHK
				
				// handle msg_TabSelect ourselves, because we don't want the CWASTEEdit
				// select all behaviour
				case msg_TabSelect:
					if ( ! mTabIntoFieldSelectsAll )
					{
						// do nothing
						break;
					}
					// else
					// fallthrough for default behaviour
					
				default:
					cmdHandled = Inherited::ObeyCommand(inCommand, ioParam);
			}
	
		return cmdHandled;
	}


void
CSimpleTextView::ClickSelf( const SMouseDownEvent &inMouseDown )
	{
	
//  Make sure the target is set or WASTE will do weird things
//  when you drag from it if it's not the target. (especially
//  if you are typing in a non-Roman script.

		if ( !IsTarget() )
			SwitchTarget( this );
	
		Inherited::ClickSelf( inMouseDown );
		
	}


void
CSimpleTextView::BeTarget()
	{
	
		OSErr	result = noErr;
		
		sTargetView = this;
		
		if ( sHasTSM )
			{
			
				result = WEInstallTSMHandlers();
				Assert_( result == noErr );
				
			}
			
		//Inherited::BeTarget();
		// ugly copy and paste because I don't want to change CWASTEEdit
		// but want to override its scroller activation behaviour

		if (FocusDraw()) {				// Show active selection
			WEActivate( GetWEHandle() );
		}
		
		StartIdling();

		ProtectSelection();
			
	}
	
	
void
CSimpleTextView::DontBeTarget()
	{
	
		OSErr	result = noErr;
		
		sTargetView = NULL;
		
		if ( sHasTSM )
			{
			
				result = WERemoveTSMHandlers();
				Assert_( result == noErr );
				
			}
			
		// Inherited::DontBeTarget();
		// ugly copy and paste because I don't want to change CWASTEEdit
		// but want to override its scroller activation behaviour
		
			if (FocusDraw()) {				// Show inactive selection
				WEDeactivate( GetWEHandle() );
			}
			StopIdling();					// Stop flashing the cursor
	
	}
	
	
void CSimpleTextView::FindCommandStatus( 	CommandT 	inCommand, 
																					Boolean 	&outEnabled, 
																					Boolean		&outUsesMark, 
																					Char16		&outMark, 
																					Str255 		outName)
	{
	
		switch (inCommand)
			{
		#ifdef MOZ_SPELLCHK
				case cmd_CheckSpelling:
					outEnabled = true;
					break;
		#endif // MOZ_SPELLCHK
				
				default:
					Inherited::FindCommandStatus(inCommand, outEnabled, 
											outUsesMark, outMark, outName);
			}
	
	}

Boolean CSimpleTextView::HandleKeyPress( const EventRecord &	inKeyEvent)
	{
		Boolean			commandKeyDown = (inKeyEvent.modifiers & cmdKey) != 0;
		Int16				theKey = inKeyEvent.message & charCodeMask;
		
		if (commandKeyDown && ( theKey == char_Tab) )
		{
			// strip out the command key modifier so the tab group handles it
			const_cast<EventRecord *>(& inKeyEvent)->modifiers &= ~cmdKey;
			
			// send it up to the supercommander (the window) to handle
			return LCommander::HandleKeyPress( inKeyEvent );
			
		}
		else
		{
			return Inherited::HandleKeyPress( inKeyEvent );
		}
	}

void	CSimpleTextView::Save( const FSSpec	&inFileSpec )
	{
	
		LFileStream	*file = new LFileStream( inFileSpec );
	
		if ( file != NULL ) 
			{
			
				file->CreateNewFile(emSignature, 'TEXT');
				file->OpenDataFork(fsRdWrPerm);
			
//  write the text data
			
				Handle msgBody = GetTextHandle();
				UInt32 textLength = GetTextLength();
			
				StHandleLocker lock( msgBody );
				file->WriteData( *msgBody, textLength );

//  Close File

				file->CloseDataFork();	
	
			}
			
	}


pascal void CSimpleTextView::TSMPreUpdate( WEReference )
	{
	
//  AHHHH!!! Why didn't they allow us to pass in a context
//  pointer? Now I have to maintain this stupid sTargetView item.

		Assert_( sTargetView != NULL );
		
		if ( sTargetView != NULL )
			sTargetView->FocusDraw();	
	
	}
	
	
#pragma mark --- Initialization ---


void CSimpleTextView::SetInitialTraits(ResIDT inTextTraitsID)
{
	TextTraitsRecord	traits;
	TextTraitsH				traitsH = UTextTraits::LoadTextTraits(inTextTraitsID);
	TextStyle					theTextStyle;
	
	if (traitsH)
		traits = **traitsH;
	else
		UTextTraits::LoadSystemTraits(traits);

//	LStyleSet* defaultStyle = mTextEngine->GetDefaultStyleSet();
//	defaultStyle->SetTextTraits(traits);
		
	theTextStyle.tsFont = traits.fontNumber;
	theTextStyle.tsSize = traits.size;
	theTextStyle.tsFace = traits.style;
	theTextStyle.tsColor = traits.color;
			
	SetStyle( weDoAll, &theTextStyle );

//	SPoint32 theScrollUnit;
//	theScrollUnit.v = defaultStyle->GetHeight();
//	theScrollUnit.h = theScrollUnit.v;
//	SetScrollUnit(theScrollUnit);

}


void CSimpleTextView::SetInitialText(ResIDT inTextID)
{

	StResource	textRsrc('TEXT', inTextID, false);

	if (textRsrc.mResourceH != nil)
		{

//	mTextEngine->TextReplaceByHandle(LTextEngine::sTextAll, textRsrc.mResourceH);
//	((LTextSelection *)mSelection)->SetSelectionRange(LTextEngine::sTextStart);

			UseText( textRsrc.mResourceH );
			SetSelection( 0, 0 );

		}
}


Boolean
CSimpleTextView::IsReadOnly()
	{
	
		Boolean		bResult			= false;
		SInt16		flagStatus 	= 0;
		
		flagStatus = FeatureFlag( weFReadOnly, weBitTest );
		
		if ( flagStatus > 0 )
			bResult = true;
			
		return bResult;
	
	}


void
CSimpleTextView::SetReadOnly( const Boolean inFlag )
	{
	
		if ( inFlag )
		
			FeatureFlag( weFReadOnly, weBitSet );
		
		else FeatureFlag( weFReadOnly, weBitClear );
	
	}
	
	
void
CSimpleTextView::Initialize()
	{

//  Here we check for TSM presence.
	
		OSErr		result						= noErr;
		SInt32	gestaltResponse		= 0;
		
		Assert_( sInitialized == false );
		
		if ( sInitialized == false )
			{
			
				sInitialized = true;
				
		 		result = ::Gestalt( gestaltTSMgrVersion, &gestaltResponse );
		 	 	
				if ( (result == noErr) && (gestaltResponse >= 1) ) 
					{
						
						result = ::Gestalt( gestaltTSMTEAttr, &gestaltResponse );
				
						if ( (result == noErr) && ((gestaltResponse >> gestaltTSMTEPresent) & 1) )	
							{
							
								sHasTSM = true;
								
							}
							
					}
					
			}
			
	}
	
	
