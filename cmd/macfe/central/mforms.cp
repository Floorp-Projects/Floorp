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

// mforms.cp - contains all the form widgets.

// macfe
#include "Netscape_Constants.h"
#include "mforms.h"
#include "uprefd.h"
#include "macgui.h"
#include "macutil.h"
#include "resgui.h"
#include "ufilemgr.h"
#include "uerrmgr.h"
#include "xpassert.h"
#include "UFormElementFactory.h"
#include "CBrowserContext.h"
#include "CBrowserWindow.h"
#include "CHTMLView.h"
#include "CWindowMediator.h"
#include "uintl.h"

// xp
#include "proto.h"
#include "lo_ele.h"
#include "xlate.h"
#include "shist.h"
#include "libevent.h"
#include "layers.h" // mjc
#include "prefapi.h"
#include "intl_csi.h"

// PP
//#include <LTextSelection.h>
//#include <LTextModel.h>
//#include <LEventHandler.h>
#include <PP_KeyCodes.h>
#include <UTextTraits.h>
#include <LArray.h>
#include <LCommander.h>

#include "CSimpleTextView.h"

#include "UUnicodeTextHandler.h"
#include "UUTF8TextHandler.h"
#include "UFixedFontSwitcher.h"
#include "UPropFontSwitcher.h"
#include "UFontSwitcher.h"
#include "CMochaHacks.h"
#include <UGAColorRamp.h>
#include <UGraphicsUtilities.h>

// macros
#define FEDATAPANE	((FormFEData *)formElem->element_data->ele_minimal.FE_Data)->fPane	
#define FEDATAHOST 	((FormFEData *)formElem->element_data->ele_minimal.FE_Data)->fHost
#define FEDATACOMMANDER ((FormFEData *)formElem->element_data->ele_minimal.FE_Data)->fCommander

// debugging
//#define DEBUG_FORMS
#ifdef DEBUG_FORMS
void TraceForm(char * what, LO_FormElementStruct *formElem);
void TraceForm(char * what, LO_FormElementStruct *formElem)
{
	XP_TRACE(("Routine: %s, formEle %d, type  %d, FE_Data %d \n",
		 what, (Int32)formElem, (Int32)formElem->type,
		 (Int32)formElem->element_data->ele_minimal.FE_Data));
}
#define XP_TRACEFORM(ROUTINE, FORM) TraceForm(ROUTINE, FORM)
#else
#define XP_TRACEFORM(ROUTINE, FORM)
#endif

inline  FormFEData *GetFEData(LO_FormElementStruct *formElem) { return ((FormFEData *)formElem->element_data->ele_minimal.FE_Data); }
inline	LPane *GetFEDataPane( LO_FormElementStruct *formElem) { return FEDATAPANE; } 
inline  void  SetFEDataPane(LO_FormElementStruct *formElem, LPane *p) { FEDATAPANE = p; }

static void	FocusFormElementTimer(	LO_FormElementStruct * formElement );
static void	BlurFormElementTimer(	LO_FormElementStruct * formElement );

static void MochaFocusCallback(MWContext * pContext, LO_Element * lo_element, int32 lType, void * whatever, ETEventStatus status);
static void MochaChangedCallback(MWContext * pContext, LO_Element * lo_element, int32 lType, void * whatever, ETEventStatus status);

static void MochaClick( MWContext* context, LO_Element* ele, CMochaEventCallback * cb );
static void MochaMouseUp( MWContext* context, LO_Element* ele, CMochaEventCallback * cb );
//static void MochaKeyPress( MWContext* context, LO_Element* ele, CMochaEventCallback * cb, EventRecord inKeyEvent );

void	FE_RaiseWindow(MWContext* inContext);
void	FE_LowerWindow( MWContext* inContext);

// Drawing setup
// Sets up the colors for drawing forms
StSetupFormDrawing::StSetupFormDrawing( LPane* formElement )
{
	fForm = formElement;

	UGraphics::SetWindowColor( fForm->GetMacPort(), wTextColor, CPrefs::GetColor( CPrefs::Black ) );
	UGraphics::SetWindowColor( fForm->GetMacPort(), wContentColor, CPrefs::GetColor( CPrefs::White ) );
}

StSetupFormDrawing::~StSetupFormDrawing()
{
// FIX ME! Why do we do this?
	CHTMLView*		view = (CHTMLView*)fForm->GetSuperView();
	
	UGraphics::SetWindowColor( fForm->GetMacPort(), wTextColor, CPrefs::GetColor( CPrefs::Black ) );
	if (view)
		UGraphics::SetWindowColor( fForm->GetMacPort(), wContentColor, view->GetBackgroundColor() );
}

/*-----------------------------------------------------------------------------
	FormFEData
  -----------------------------------------------------------------------------*/
FormFEData::FormFEData()
{
	fPane = NULL;
	fHost = NULL;
}

FormFEData::~FormFEData()
{
	// Schedule the form element to be 
	// destroyed at idle time
	if (fHost != NULL)
	{
		fHost->MarkForDeath();
		fHost->SetFEData(NULL);
		
	}		

	fPane		= NULL;
	fHost 		= NULL;
	fCommander 	= NULL;			
}




#pragma mark == FormsPopup ==

/*-----------------------------------------------------------------------------
	Forms Popup
	Displays a FORM_TYPE_SELECT_ONE structure
-----------------------------------------------------------------------------*/

FormsPopup::FormsPopup (CGAPopupMenu * target, lo_FormElementSelectData_struct * selections)
:	StdPopup (target), LFormElement()
{
	fSelections = selections;
	fFocusState = FocusState_None;
	fSavedFocus = nil;
	fOldValue = -1;
	target->SetMaxValue(selections->option_cnt);
	SetReflectOnChange(true);
}

short
FormsPopup::GetCount ()
{
	return fSelections->option_cnt;
}

CStr255
FormsPopup::GetText (short item)
{
	lo_FormElementOptionData *options = nil;
	PA_LOCK (options, lo_FormElementOptionData*, fSelections->options);
	CStr255 text;
	PA_LOCK (text, char*, options[item-1].text_value);
	PA_UNLOCK (options[item-1].text_value);
	PA_UNLOCK (fSelections->options);
	
	if (text == "")
		text = " ";
	return text;
}


/*
	FormsPopup::ExecuteSelf
	
	2 important things here when we're handling a click...
	
	1) if the value of the popup changes, we call MochaChanged
	2) popups have onFocus and onBlur handlers in Javascript, so
	   we fake giving the popup focus, even though they really
	   don't get focus on Macs.  So we give it focus real quick,
	   then take it away after the click is handled.  Yippee.

*/
void	
FormsPopup::ExecuteSelf(MessageT 	inMessage, 
						void*		ioParam		)
{
	fTarget->SetNeedCustomDrawFlag(NeedCustomPopup());
	// we're only interested in clicks
	if (inMessage == msg_Click)
	{
		fOldValue = fTarget->GetValue();
		mExecuteHost = true;
		// Temporarily give focus to the popup in Javascript land
		if (this != sTargetedFormElement)
		{
			// If we aren't targeted form element, set sTargetedFormElement
			// to popup menu and blur currently targeted form element
			if (sTargetedFormElement != NULL) {
				fSavedFocus = sTargetedFormElement;
				sTargetedFormElement->MochaFocus(false);
			}
			// 97-06-14 pkc -- set fFocusState to execute correct code on callback
			fFocusState = FocusState_FirstCall;
			// focus popup menu
			MochaFocus(true);
		}
		else {
			// 97-06-14 pkc -- set fFocusState to execute correct code on callback
			fFocusState = FocusState_FirstCall;
			// focus popup menu
			MochaFocus(true);
		}			
	}
	else {
		StdPopup::ExecuteSelf(inMessage, ioParam);		
	}
}
//
// do ScriptCode Menu Trick on IM:MacTbxEss Page3-46
//
//	Good for most encoding except
//		Greek, Turkish, and UTF8
//
void	
FormsPopup::SetMenuItemText(MenuHandle aquiredMenu, int item, CStr255& itemText)
{
	StdPopup::SetMenuItemText( aquiredMenu, item, itemText);
	if (! NeedDrawTextInOurOwn())
	{		
		// do ScriptCode Menu Trick on IM:MacTbxEss Page3-46
		CCharSet charset;
		if(CPrefs::GetFont(GetWinCSID(), &charset))
		{
			::SetItemCmd (aquiredMenu, item, 0x1c );
			::SetItemIcon (aquiredMenu, item, charset.fFallbackFontScriptID );
		}
	}
}
//
//	Decide wheather we use override SetMenuItemText or PopUpMenuSelect
//
Boolean	FormsPopup::NeedDrawTextInOurOwn() const
{
	return (fContext &&
		(GetWinCSID() == CS_UTF8));
}
void 	FormsPopup::DrawTruncTextBox (CStr255 text, const Rect& box)
{
	if(GetWinCSID() == CS_UTF8)
	{
		/*
        Truncates the text before drawing.
        Does not word wrap.
		*/
        FontInfo fontInfo;
        UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
        UFontSwitcher *fs = UPropFontSwitcher::Instance();
        
        th->GetFontInfo (fs, &fontInfo);
        MoveTo (box.left, box.bottom - fontInfo.descent -1);
        // ¥¥¥ÊFix me 
        //	UTF8 vresion does not truncate for now.
        // TruncString (box.right - box.left, text, truncEnd);       
        
        th->DrawString (fs,  text);
	}
	else
	{
		 StdPopup::DrawTruncTextBox(text, box);
	}
}

Boolean	FormsPopup::NeedCustomPopup() const
{
	XP_Bool	useGrayscaleFormControls = false;
	int		prefResult = PREF_GetBoolPref("browser.mac.use_grayscale_form_controls", &useGrayscaleFormControls);

	return (prefResult != PREF_NOERROR || !useGrayscaleFormControls || NeedDrawTextInOurOwn());
}

// 97-06-14 pkc -- state machine callback hack to handle onFocus correctly
void FormsPopup::PostMochaFocusCallback()
{
	if (fFocusState == FocusState_FirstCall) {
		//
		// Call through to handle the click, and let 
		// Javascript know if the value changed
		//
		Assert_(fTarget != NULL);
		
		if (fOldValue != fTarget->GetValue()) {
			fFocusState = FocusState_SecondCall;
			MochaChanged();
		}
		else {
			// Restore original focus state
			// First, blur popup menu
			// 97-06-14 pkc -- set fFocusState to execute correct code on callback
			fFocusState = FocusState_ThirdCall;
			MochaFocus(false, false);
			// 97-06-20 pkc -- set sTargetedFormElement to NULL becuase popup menus
			// aren't really targeted
			sTargetedFormElement = NULL;
		}
	}
	else if (fFocusState == FocusState_SecondCall) {
		// Restore original focus state
		// First, blur popup menu
		// 97-06-14 pkc -- set fFocusState to execute correct code on callback
		fFocusState = FocusState_ThirdCall;
		MochaFocus(false, false);
		// 97-06-20 pkc -- set sTargetedFormElement to NULL becuase popup menus
		// aren't really targeted
		sTargetedFormElement = NULL;
	}
	else if (fFocusState == FocusState_ThirdCall) {
		// Restore original focus state
		// Now focus previously focused element if there was one
		if (fSavedFocus != NULL) {
			fSavedFocus->MochaFocus(true);
			fSavedFocus = NULL;
		}
	}
}

#pragma mark == LFormElement ==

int16 LFormElement::GetWinCSID() const
{
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(fContext);

	return INTL_GetCSIWinCSID(csi);
}

#pragma mark == CWhiteEditField ==

CWhiteEditField::CWhiteEditField(LStream *inStream)
	: 	CTSMEditField(inStream), LFormElement()
{
}

// Draws background to white
void
CWhiteEditField::DrawSelf()
{
	Rect	frame;
	CalcLocalFrameRect(frame);

	::InsetRect(&frame, 1,1);
	::EraseRect(&frame);

	CTSMEditField::DrawSelf();	

	::InsetRect(&frame, -1,-1);
	UGraphics::FrameRectSubtle(frame, true);	
}


void
CWhiteEditField::BeTarget()
{
	CTSMEditField::BeTarget();
	MochaFocus(true);
}

void
CWhiteEditField::DontBeTarget()
{	
	
	MochaFocus(false);  // this must go first because
						// it might indirectly set us to be the target again,
						// which would be wrong.
						
	CTSMEditField::DontBeTarget();
}

void
CWhiteEditField::UserChangedText()
{
	CTSMEditField::UserChangedText();
	MochaChanged();
}

void	
CWhiteEditField::ClickSelf(	const SMouseDownEvent& event)
{
	SInt16 oldSelStart, oldSelEnd;
	
	oldSelStart = (**mTextEditH).selStart;
	oldSelEnd	= (**mTextEditH).selEnd;
	
	CTSMEditField::ClickSelf(event);
	
	if (oldSelStart != (**mTextEditH).selStart	||
		oldSelEnd != (**mTextEditH).selEnd)
	{	
		MochaSelect();
	}	
}

Boolean 
CWhiteEditField::HandleKeyPress(const EventRecord& inKeyEvent)
{
	if (keyUp == inKeyEvent.what) return true; // ignore key ups for now
	else return CTSMEditField::HandleKeyPress(inKeyEvent);
}


#pragma mark == CFormLittleText ==

//---------------------------------------------------------------------------
// class CFormLittleText
//---------------------------------------------------------------------------

CFormLittleText::CFormLittleText(LStream *inStream) : CWhiteEditField(inStream)
{
	fDoBroadcast = FALSE;
	fLOForm = NULL;
#ifdef LAYERS
	// add an attachment which will intercept key events and dispatch them to layers
	CLayerKeyPressDispatchAttachment* led = new CLayerKeyPressDispatchAttachment;
	AddAttachment(led);
#endif
}

void CFormLittleText::BroadcastValueMessage()
{
	BroadcastMessage(msg_SubmitText, (void*) this);
}

void CFormLittleText::SetVisibleChars(Int16 visChars)
{
	FocusDraw();
	UTextTraits::SetPortTextTraits(mTextTraitsID);
	FontInfo fInfo;
	::GetFontInfo(&fInfo);
	
	ResizeFrameTo(::CharWidth('a') * visChars + 6, fInfo.ascent + fInfo.descent+ fInfo.leading + 4, FALSE);	
}

// Key events are intercepted by an attachment, routed through layers and return
// to the front end (through FE_HandleLayerEvent) if javascript lets us have the event. 
// CHTMLView::HandleKeyPressLayer calls this.
Boolean CFormLittleText::HandleKeyPress(const EventRecord& inKeyEvent)
{
	Boolean handled = false;
	if (keyUp == inKeyEvent.what) return true; // ignore key ups for now
	
	char c = inKeyEvent.message & charCodeMask;

	if (fDoBroadcast && ((c == char_Enter) || (c == char_Return)))
	{
		BroadcastValueMessage();
		handled = true;
	}
	else
	{
		handled = CWhiteEditField::HandleKeyPress(inKeyEvent);
	}
	return handled;
}

void CFormLittleText::FindCommandStatus(
								CommandT			inCommand,
								Boolean				&outEnabled,
								Boolean				&outUsesMark,
								Char16				&outMark,
								Str255				outName)
{
	if ( inCommand == cmd_Cut || inCommand == cmd_Copy )
	{
		lo_FormElementTextData* textData;
		if ( fLOForm )
		{
			textData = (lo_FormElementTextData*)fLOForm->element_data;
			if ( textData && textData->type == FORM_TYPE_PASSWORD )
			{
				outEnabled = FALSE;
				return;
			}
		}
	}
	CWhiteEditField::FindCommandStatus( inCommand, outEnabled, outUsesMark, outMark, outName );
}

void CFormLittleText::BeTarget()
{
// FIX ME!
//	((CHyperView*)mSuperView)->ShowView(this);
	CHTMLView* theHTMLView = dynamic_cast<CHTMLView*>(GetSuperView());
	if (theHTMLView)
		theHTMLView->ShowView(*this);

	CWhiteEditField::BeTarget();
}
StringPtr CFormLittleText::GetDescriptor(
								Str255				outDescriptor) const
{
	if(fContext && (CS_UTF8 == GetWinCSID()))
	{
		Str255 nonUTF8Str;
		StringPtr nonUTF8text;
		nonUTF8text = CWhiteEditField::GetDescriptor(nonUTF8Str);
		int16 font_csid = ScriptToEncoding(FontToScript((**UTextTraits::LoadTextTraits(mTextTraitsID)).fontNumber));
		char* utf8text = (char*) INTL_ConvertLineWithoutAutoDetect(font_csid, CS_UTF8, nonUTF8text+ 1, nonUTF8text[0] );
		if(utf8text)
		{
			int len = XP_STRLEN(utf8text);
			if(len > 254)
				len = 254;
			outDescriptor[0] = len;
			XP_MEMCPY((outDescriptor + 1), utf8text, len );
			XP_FREE(utf8text);			
			return outDescriptor;
		}		
		else
		{
			outDescriptor[0] = 0;	// somehow we cannot get Descriptor
			return outDescriptor;
		}
	} else {
		return CWhiteEditField::GetDescriptor(outDescriptor);
	}
}
void CFormLittleText::SetDescriptor(
								ConstStr255Param	inDescriptor)
{
	if(fContext && (CS_UTF8 == GetWinCSID()))
	{
		// Find out the font csid
		int16 font_csid = ScriptToEncoding(FontToScript((**UTextTraits::LoadTextTraits(mTextTraitsID)).fontNumber));
		// Convert the UTF8 text into non-UTF8 text
		char* nonUTF8text = (char*) INTL_ConvertLineWithoutAutoDetect(CS_UTF8, font_csid, (unsigned char*)inDescriptor+ 1, inDescriptor[0] );
		if(nonUTF8text)
		{
			int len = XP_STRLEN(nonUTF8text);
			if(len > 254) len = 254;
			Str255 nonUTF8Str;
			nonUTF8Str[0] = len;
			XP_MEMCPY((nonUTF8Str + 1), nonUTF8text, len );
			XP_FREE(nonUTF8text);
			CWhiteEditField::SetDescriptor(nonUTF8Str);
		}
		else
		{
			CWhiteEditField::SetDescriptor("\p");	// Somehow we failed. set it to null string			
		}
	} else {
		CWhiteEditField::SetDescriptor(inDescriptor);
	}
}

#pragma mark == CFormBigText ==

//---------------------------------------------------------------------------
// class CFormBigText
//---------------------------------------------------------------------------

CFormBigText::CFormBigText(LStream *inStream)
	:	CSimpleTextView(inStream),
		LFormElement()
{

#ifdef LAYERS
	// add an attachment which will intercept key events and dispatch them to layers
	CLayerKeyPressDispatchAttachment* led = new CLayerKeyPressDispatchAttachment;
	AddAttachment(led);
#endif
}

CFormBigText::~CFormBigText()
{
	PostAction(NULL);
}

void CFormBigText::DrawSelf()
{
	Rect	frame;
	CalcLocalFrameRect(frame);
	
	::InsetRect(&frame, 1,1);
	::EraseRect(&frame);

	CSimpleTextView::DrawSelf();
}

Boolean CFormBigText::ObeyCommand(CommandT inCommand, void* ioParam)
{
	Boolean cmdHandled = true;
	
	switch (inCommand)
	{
	case msg_TabSelect:
		if (!IsEnabled())
		{
			cmdHandled = false;
			break;
		}
		inCommand = cmd_SelectAll;
		//	fall through to default ...
		
	default:
		cmdHandled = CSimpleTextView::ObeyCommand(inCommand, ioParam);
		break;
	}
	return cmdHandled;
}

Boolean CFormBigText::FocusDraw(LPane* /*inSubPane*/)
{
//	CSimpleTextView::SetMultiScript(fContext && (GetWinCSID() == CS_UTF8));
	if (CSimpleTextView::FocusDraw())
	{
		UGraphics::SetBack(CPrefs::White);
		return TRUE;
	}
	else
		return FALSE;
}

void CFormBigText::BeTarget()
{
// Show scrollbar inside the hyperview
// FIX ME!
//	((CHyperView*)(mSuperView->GetSuperView()))->ShowView(mSuperView);
#if 0 // Disable auto scrolling. This auto scrolling breaks click selection since the mouse location isn't updated
	  //  
	if (GetSuperView())
	{
		CHTMLView* theHTMLView = dynamic_cast<CHTMLView*>(GetSuperView()->GetSuperView());
		if (theHTMLView)
			theHTMLView->ShowView(*GetSuperView());
	}
#endif //  0
	CSimpleTextView::BeTarget();
	MochaFocus(true);
//	CSimpleTextView::SetMultiScript(fContext && (GetWinCSID() == CS_UTF8));
}




void 
CFormBigText::DontBeTarget()
{
	MochaFocus(false);
	CSimpleTextView::DontBeTarget();
}


void CFormBigText::ClickSelf(const	SMouseDownEvent& event)	// to call MochaSelect
{

	SInt32	selStart 	= 0;
	SInt32	selEnd		= 0;
	
//	GetSelection( &selStart, &selEnd );
//	SInt32 theOldRange = selEnd - selStart;

	CSimpleTextView::ClickSelf(event);

//	GetSelection( &selStart, &selEnd );
//	SInt32 theNewRange = selEnd - selStart;
	
//	if (theOldRange != theNewRange)
//		MochaSelect();
		
	}

// The textengine sends us a message when the text changes
/*
void CFormBigText::ListenToMessage(MessageT inMessage, void *ioParam)	// to call MochaChanged
{
//	if (inMessage == msg_CumulativeRangeChanged)
//		MochaChanged();
		
//	CSimpleTextView::ListenToMessage(inMessage, ioParam);
}	
*/


//  VText's text engine used to broadcast stuff if something changed. WASTE
//  does not do this, however there is a UserChangedText method in the CWASTEEdit
//  class that is called when the text is changed. I hope this will suffice...


void
CFormBigText::UserChangedText()
	{
	
		MochaChanged();
	
	}
	

// Key events are intercepted by an attachment, routed through layers and return
// to the front end (through FE_HandleLayerEvent) if javascript lets us have the event. 
// CHTMLView::HandleKeyPressLayer calls this.
Boolean CFormBigText::HandleKeyPress(const EventRecord& inKeyEvent)
{	
	Boolean handled = false;
	char c = inKeyEvent.message & charCodeMask;
	
	if (keyUp == inKeyEvent.what) handled = true; // ignore key ups for now
	else handled = CSimpleTextView::HandleKeyPress(inKeyEvent);
	return handled;
}

	
#pragma mark == CFormList ==

//---------------------------------------------------------------------------
// class CFormList
//---------------------------------------------------------------------------

// We use the CallerLDEF approach from the March 1995 Develop article "An Object-Oriented
// Approach to Hierarchical Lists." The author of the article is Jan Bruyndonckx.

CFormList::CFormList(LStream * inStream) : LListBox(inStream), LFormElement()
{
	fLongestItem = 36;
	::LAddColumn(1, 0, mMacListH);
	SetReflectOnChange(true);
	
	(*mMacListH)->refCon = (long) callerLDEFUPP ;	// put address of callback in refCon
 	(*mMacListH)->userHandle = (Handle) this ;	// keep a pointer to ourself

}

CFormList::~CFormList()
{
	Rect theNullRect = { 0, 0, 0, 0 };

	ControlHandle& theScrollBar = (*mMacListH)->vScroll;
	if (theScrollBar != NULL)
		(*theScrollBar)->contrlRect = theNullRect;

	theScrollBar = (*mMacListH)->hScroll;
	if (theScrollBar != NULL)
		(*theScrollBar)->contrlRect = theNullRect;

	// we're not necessarily destroyed because our
	// superview is, so we need to explicitly kill
	// the focus box.
	if (mFocusBox != NULL)
		delete mFocusBox;
}

ListDefUPP CFormList::callerLDEFUPP = NewListDefProc (LDefProc) ;	// create UPP for LDEF callback

// ¥¥ Misc

void CFormList::SetSelectMultiple(Boolean multipleSel)
{
	if (multipleSel)
		(*mMacListH)->selFlags = lUseSense;
	else
		(*mMacListH)->selFlags = lOnlyOne;
}

// Add an item to the list. Figure out its length so we can shrinkToFit
void CFormList::AddItem( const cstring& itemName, SInt16 rowNum )
{
	Point aCell;
	
	aCell.h = 0;
	aCell.v = rowNum;
	::LSetCell( itemName.data(), itemName.length(), aCell, mMacListH );
	long newLength;
	if(GetWinCSID() == CS_UTF8)
	{
		UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
	  	UFontSwitcher *fs = UPropFontSwitcher::Instance();
		newLength = th->TextWidth(fs,	(char*)itemName.data(), (int)itemName.length());
	} else {
		newLength = ::TextWidth( itemName.data(), 0, itemName.length());
	}
	newLength += 26;
	if ( newLength > fLongestItem )
		fLongestItem = newLength;
}


void CFormList::SetTextTraits()
{
	UTextTraits::SetPortTextTraits(mTextTraitsID);	
}

void CFormList::SyncRows( UInt16 nRows )
{
	if ( (**mMacListH).dataBounds.bottom > nRows )
		::LDelRow( (**mMacListH).dataBounds.bottom - nRows, nRows, mMacListH );
	else
		::LAddRow( nRows - (**mMacListH).dataBounds.bottom, (**mMacListH).dataBounds.bottom, mMacListH );
}


void
CFormList::BeTarget()
{
	LListBox::BeTarget();
	MochaFocus(true);
}
	
	
void
CFormList::DontBeTarget()
{
	LListBox::DontBeTarget();
	MochaFocus(false);	
}


// Selecting items
void CFormList::SetSelect(int item, Boolean selected)
{
	FocusDraw();
	Point aCell;
	SetPt(&aCell, 0, item);
	::LSetSelect(selected,aCell,mMacListH);
}

void CFormList::SelectNone()
{
	Cell c;
	SInt16	total = (*mMacListH)->dataBounds.bottom;

	c.h = 0;
	
	FocusDraw();
		
	::LSetDrawingMode(false, mMacListH);
	// Just a friendly reminder, List Mangler cell
	// coords are zero based.
	for (c.v = 0; c.v < total; c.v++)
		::LSetSelect(false, c, mMacListH);
		
	::LSetDrawingMode(true, mMacListH);
	
	Refresh();
}


/*
void
CFormList::Reset(	lo_FormElementSelectData	*	selectData
					Boolean							resetSelection,
					Boolean							resetOptions	)
{					
	lo_FormElementOptionData * mother;
	
	PA_LOCK(mother, lo_FormElementOptionData *, selectData->options);		
	for (int i=0; i<selectData->option_cnt; i++)
	{
		char * itemName = NULL;
		Boolean select;
		
		PA_LOCK(itemName, char*, mother[i].text_value);
		myList->AddItem (itemName, 0);
		PA_UNLOCK(mother[i].text_value);
		
		if (fromDefaults)
			select = mother[i].def_selected;
		else
			select = mother[i].selected;
			
		if (select)
			SetSelect(i, select);
	}
}
*/

// Are they selected
Boolean CFormList::IsSelected(int item)
{
	Point theCell;
	SetPt(&theCell, 0, item);
	return (::LGetSelect(FALSE,&theCell,mMacListH));
}


// Resizes the list to the width of the longest item, and visRows
void CFormList::ShrinkToFit( Int32 visRows )
{
	// We need to adjust the height for CS_UTF8
	if(GetWinCSID() == CS_UTF8)
	{
		FontInfo		info;
		UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
	  	UFontSwitcher *fs = UPropFontSwitcher::Instance();
		th->GetFontInfo(fs, &info );		
		 (*mMacListH)->cellSize.v = info.ascent + info.descent + info.leading;
	}	
	ResizeFrameTo(fLongestItem, visRows * (*mMacListH)->cellSize.v+2, FALSE); // Must get a scroller in there
	// We need to resize our cell to fill the whole list
	Rect displayRect;
	CalcLocalFrameRect(displayRect);
	::InsetRect(&displayRect, 1, 1);
	displayRect.right -= 15;	// To account for the scrollbar
	(**mMacListH).cellSize.h = displayRect.right - displayRect.left;

}

// Sets background to white
Boolean CFormList::FocusDraw(LPane* /*inSubPane*/)
{
	if (LListBox::FocusDraw())
	{
		UGraphics::SetBack(CPrefs::White);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

extern Boolean gIsPrinting;


// white background
void CFormList::DrawSelf()
{
	Rect	frame;
	
	CalcLocalFrameRect(frame);
	::EraseRect(&frame);
	LListBox::DrawSelf();	
}

void CFormList::MakeSelection( LArray& list )
{
	Cell	look = { 0, 0 };

	while ( ::LGetSelect( TRUE, &look, mMacListH ) )
	{
		list.InsertItemsAt( 1, 1, &look );
		::LNextCell( TRUE, TRUE, &look, mMacListH );
	}
}

Boolean CFormList::CheckSelection( LArray& list )
{
	Cell	look = { 0, 0 };
	Cell	tmp;

	while ( ::LGetSelect( TRUE, &look, mMacListH ) )
	{
		if ( !list.FetchItemAt( 1, &tmp ) )
			return TRUE;
		if ( tmp.v != look.v || tmp.h != tmp.h )
			return TRUE;
		list.RemoveItemsAt( 1, 1 );
		::LNextCell( TRUE, TRUE, &look, mMacListH );
	}
	if ( list.GetCount() > 0 )
		return TRUE;
	return FALSE;
}	

	
Boolean	
CFormList::HandleKeyPress(const EventRecord	&inKeyEvent)
{
	char c = inKeyEvent.message & charCodeMask;
	
	if (keyUp == inKeyEvent.what) return true; // ignore key ups for now
	
	Boolean result = LListBox::HandleKeyPress(inKeyEvent);
	
	// there's no easy way to know if the selection changed
	// because multiple items can be selected.
	//
	// We could basically copy LListBox into here and
	// recongnize the selection to be changing.
	//
	// For now, however, we'll slime it and assume that
	// the selection changes.  
	
	MochaChanged();
	
	return result;
}

void CFormList::ClickSelf( const SMouseDownEvent& event )
{
	StTempFormBlur	formBlur;
	
	LArray		list( sizeof( Cell ) );
	Boolean		single;
	Cell		selCell;
	Boolean		changed;
	
	single = ( (**mMacListH).selFlags & lOnlyOne ) != 0;

	if ( !single )
		this->MakeSelection( list );
	else
		this->GetLastSelectedCell( selCell );
	
	LListBox::ClickSelf( event );
	
	if ( !single )
		changed = this->CheckSelection( list );
	else
	{
		Cell	tmp;
		this->GetLastSelectedCell( tmp );
		if ( tmp.h != selCell.h || tmp.v != selCell.v )
			changed = TRUE;
		else
			changed = FALSE;
	}
	
	if ( changed )
	{
		// Same deal as HandleKeyPress...
		MochaChanged();
	}
}



//----------------------------------------------------------------------------

void CFormList::DrawElement (const short lMessage, 
								  const Boolean lSelect,
								  const Rect *lRect,
							      const void *lElement, 
							      const short lDataLen)

// Member function for responding to LDEF calls.
// Calls DrawElementSelf to draw a list element.

{
  switch (lMessage) {

   case lDrawMsg :
		    ::EraseRect (lRect) ;
		    if (lDataLen == 0)
				break ;

			DrawElementSelf (lRect, lElement, lDataLen) ;
			
   			if (!lSelect)
   				break ;

   case lHiliteMsg :
   			LMSetHiliteMode( 0 );
   			::InvertRect (lRect) ;
   			break ;
  }
}

//----------------------------------------------------------------------------

void CFormList::DrawElementSelf (const Rect *lRect, 
									  const void *lElement, 
									  const short lDataLen)

// Draw contents of a single list element on the screen.
// Default version just draws text; override for other types of data.

{
 if(fContext && (GetWinCSID() == CS_UTF8))
  {
  	 UFontSwitcher *fs = UPropFontSwitcher::Instance();
  	 UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
     FontInfo fi;
	 th->GetFontInfo(fs, &fi);	
	 ::MoveTo (lRect->left+2, lRect->bottom - fi.descent) ;	
  	 th->DrawText(fs, (char*) lElement, (int)lDataLen);
  }
  else
  {
     FontInfo fi;
     GetFontInfo(&fi);	
     ::MoveTo (lRect->left+2, lRect->bottom - fi.descent) ;	
 	 ::DrawText (lElement, 0, lDataLen) ;
  }
}

// ---------------------------------------------------------------------------
//		¥ ActivateSelf
// ---------------------------------------------------------------------------

void CFormList::ActivateSelf()
{
	// 97-06-07 pkc -- Only call ActivateSelf when we're visible because layout/layers
	// puts us in a strange spot originally. When the form element is shown, then it's
	// coordinates reflect its true position.
	if (IsVisible())
		LListBox::ActivateSelf();
}


// ---------------------------------------------------------------------------
//		¥ DeactivateSelf
// ---------------------------------------------------------------------------

void CFormList::DeactivateSelf()
{
	// 97-06-07 pkc -- Only call DectivateSelf when we're visible because layout/layers
	// puts us in a strange spot originally. When the form element is shown, then it's
	// coordinates reflect its true position.
	if (IsVisible())
		LListBox::DeactivateSelf();
}

//----------------------------------------------------------------------------
// 'self' matches the previously saved 'this'.

pascal void LDefProc    (short 			lMessage,
								Boolean 		lSelect, 
								Rect 			*lRect,
								Cell 			/*lCell*/,
								unsigned short 	lDataOffset,
								unsigned short 	lDataLen,
								ListHandle		lHandle)
								
// Custom list definition function procedure for CCustomListBox.
// Called by the LDEF stub; returns control back to class method
// DrawElement to do the actual drawing.

{
  // ignore init and dispose messages
  if ((lMessage == lInitMsg) 	||
  	  (lMessage == lCloseMsg))
   	return ;

  // restore the application's A5, so that we can access global
  // variables.
  long savedA5 = ::SetCurrentA5() ;
  
  // get the pointer back to 'this'.  As the function is no method,
  // and 'this' is a keyword, use 'self' instead.
  CFormList *self = (CFormList*) (*lHandle)->userHandle ;

  Handle h = (*self->mMacListH)->cells ;
  char saveState = ::HGetState (h) ;
  ::HLock (h) ;

  void *lElement = (void*) (*h + lDataOffset) ;
  self->DrawElement (lMessage, lSelect, lRect, lElement, lDataLen) ;

  ::HSetState (h, saveState) ;
  ::SetA5 (savedA5) ;
}

//----------------------------------------------------------------------------



#pragma mark == CFormButton ==

//---------------------------------------------------------------------------
// class CFormButton
//---------------------------------------------------------------------------

CFormButton::CFormButton(LStream * inStream) : LStdButton(inStream)
{
	SetReflectOnChange(true);
#ifdef LAYERS
	// add an attachment which will intercept clicks and dispatch them to layers
	CLayerClickDispatchAttachment* led = new CLayerClickDispatchAttachment;
	AddAttachment(led);
#endif
}

// ¥¥ Misc

void CFormButton::SetDescriptor(ConstStr255Param	inDescriptor)
{
	LStdButton::SetDescriptor(inDescriptor);
	if (FocusDraw()) {
		UTextTraits::SetPortTextTraits(mTextTraitsID);	// Set the right fontage
		// Resize to text width	
		Int32 width;
		if(fContext && (GetWinCSID() == CS_UTF8))		
		{	
			UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
			UFontSwitcher *fs = UPropFontSwitcher::Instance();
			width = th->StringWidth(fs , (unsigned char*)inDescriptor);
		}
		else
		{	
			width = StringWidth(inDescriptor);
		}				
		ResizeFrameTo(width + 16, mFrameSize.height, FALSE);
	}
}

void CFormButton::BroadcastValueMessage()
{
	if (mValueMessage != cmd_Nothing) {
		Int32	value = mValue;
		BroadcastMessage(mValueMessage, (void*) this);
	}
}

// This gets the mouse up and sends it to javascript in the event that
// a mouse down is cancelled; hot spot tracking normally eats the mouse up
// so this won't be called otherwise. We won't get the mouse up if it occurred
// outside the pane.
void CFormButton::EventMouseUp(const EventRecord& /*inEvent*/)
{
	CMochaHacks::SendEvent(fContext, EVENT_MOUSEUP, fLayoutElement);
}

void CFormButton::ClickSelfLayer(const SMouseDownEvent	&inMouseDown)
{
	LStdButton::ClickSelf(inMouseDown);
}

/*
 * CFormButtonHotSpotResultMochaCallback
 * callback for HotSpotResult
 * crappy inline implementation, because the whole callback thing is crappy
 */
class CFormButtonHotSpotResultMochaCallback : public CMochaEventCallback
{
public:
	CFormButtonHotSpotResultMochaCallback( CFormButton * button, Int16 inHotSpot, int32 inEventType)
		: fButton(button), fHotSpot(inHotSpot), fEventType(inEventType) {}

	virtual void Complete(MWContext * context, 
				LO_Element * element, int32 /*type*/, ETEventStatus status)
	{
		if (status == EVENT_OK)
		{
			if (fEventType == EVENT_MOUSEUP)
				MochaClick(context, element, new CFormButtonHotSpotResultMochaCallback(fButton, fHotSpot, EVENT_CLICK));
		}
		if (fEventType == EVENT_CLICK)
		{
			// if event wasn't ok, set message to one that won't be sent, so button
			// will unhighlight but perform no action.
			MessageT saveMsg = fButton->GetValueMessage();
			if (status != EVENT_OK) fButton->SetValueMessage(msg_AnyMessage);
			try
			{
				fButton->HotSpotResultCallback( fHotSpot );
			}
			catch (...) {}
			if (status != EVENT_OK) fButton->SetValueMessage(saveMsg);
		}
	}

private:
	int32 fEventType; // the javascript event type, defined in libevent.h
	Int16 fHotSpot;
	CFormButton * fButton;
};

void
CFormButton::HotSpotResultCallback(Int16 inHotSpot)
{
	if (!IsMarkedForDeath())
	{
		LStdButton::HotSpotResult( inHotSpot );
	}
}

// This will actually get processed through the callback (see above method )
// Sends a mouse up to javascript, and if javascript lets us process the event, the callback sends
// a click to javascript.
void
CFormButton::HotSpotResult(Int16 inHotSpot)
{
	MWContext*		myContext = fContext;
	LO_Element*		myEle = fLayoutElement;
	
	MochaMouseUp(fContext, fLayoutElement, new CFormButtonHotSpotResultMochaCallback(this, inHotSpot, EVENT_MOUSEUP));
}

//
//	Addition function for UTF8
//	
void
CFormButton::HotSpotAction(
	Int16		inHotSpot,
	Boolean		inCurrInside,
	Boolean		inPrevInside)
{
	if(fContext && (GetWinCSID() == CS_UTF8))	
	{
		if (inCurrInside != inPrevInside) {		
			FocusDraw();
			Rect	frame;
			CalcLocalFrameRect(frame);
			short   weight = (frame.bottom-frame.top) / 2;
			StColorPenState::Normalize();
			::InvertRoundRect(&frame,weight,weight);
		}
	}
	else
	{
		LStdButton::HotSpotAction(inHotSpot, inCurrInside, inPrevInside);
	}
}

Boolean
CFormButton::TrackHotSpot(
	Int16	inHotSpot,
	Point 	inPoint,
	Int16 inModifiers)
{
	if(fContext && (GetWinCSID() == CS_UTF8))	
	{
										// For the initial mouse down, the
										// mouse is currently inside the HotSpot
										// when it was previously outside
		Boolean		currInside = true;
		Boolean		prevInside = false;
		HotSpotAction(inHotSpot, currInside, prevInside);
		
										// Track the mouse while it is down
		Point	currPt = inPoint;
		while (StillDown()) {
			GetMouse(&currPt);			// Must keep track if mouse moves from
			prevInside = currInside;	// In-to-Out or Out-To-In
			currInside = PointInHotSpot(currPt, inHotSpot);
			HotSpotAction(inHotSpot, currInside, prevInside);
		}
		
		EventRecord	macEvent;			// Get location from MouseUp event
		if (GetOSEvent(mUpMask, &macEvent)) {
			currPt = macEvent.where;
			GlobalToLocal(&currPt);
		}

		this->DrawSelf();
										// Check if MouseUp occurred in HotSpot
		return PointInHotSpot(currPt, inHotSpot);
	}
	else
	{
		return LStdButton::TrackHotSpot(inHotSpot, inPoint, inModifiers);
	}
}

void
CFormButton::DrawTitle()
{
	Rect frame;
	CalcLocalFrameRect(frame);	
	Str255	outText;
	this->GetDescriptor(outText);
	if(!IsEnabled())
	{
		RGBColor color = { 0, 0, 0 };
		::RGBForeColor(&color);
		::TextMode(grayishTextOr);
	}
	
	//	¥¥¥ To Be Test	
	UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
	UFontSwitcher *fs = UPropFontSwitcher::Instance();
	
	FontInfo info;	
	th->GetFontInfo(fs, &info);
	::MoveTo((frame.right + frame.left - 
			th->StringWidth(fs ,outText) ) / 2,
			 (frame.top + frame.bottom + info.ascent ) / 2);
	th->DrawString(fs, outText);	
}
void
CFormButton::DrawSelf()
{
	if(fContext && (GetWinCSID() == CS_UTF8))
	{
		// Do whatever a control shoul do.
		Rect	frame;
		CalcLocalFrameRect(frame);
		short   weight = (frame.bottom-frame.top) / 2;
		StColorPenState::Normalize();
		::FillRoundRect(&frame,weight,weight, &UQDGlobals::GetQDGlobals()->white);
		::FrameRoundRect(&frame,weight,weight);
		DrawTitle();
	}
	else
	{
		LStdButton::DrawSelf();		
	}
}

#pragma mark == CGAFormPushButton ==

//---------------------------------------------------------------------------
// class CGAFormPushButton
//---------------------------------------------------------------------------

CGAFormPushButton::CGAFormPushButton(LStream * inStream) : LGAPushButton(inStream)
{
	SetReflectOnChange(true);
#ifdef LAYERS
	// add an attachment which will intercept clicks and dispatch them to layers
	AddAttachment(new CLayerClickDispatchAttachment);
#endif
}

// ¥¥ Misc

void CGAFormPushButton::SetDescriptor(ConstStr255Param	inDescriptor)
{
	super::SetDescriptor(inDescriptor);
	if (FocusDraw())
	{
		UTextTraits::SetPortTextTraits(mTextTraitsID);	// Set the right fontage
		// Resize to text width	
		Int32 width;

		if(fContext && (GetWinCSID() == CS_UTF8))		
		{	
			UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
			UFontSwitcher *fs = UPropFontSwitcher::Instance();
			width = th->StringWidth(fs , (unsigned char*)inDescriptor);
		}
		else
		{	
			width = StringWidth(inDescriptor);
		}				
		ResizeFrameTo(width + 16, mFrameSize.height, FALSE);
	}
}

void CGAFormPushButton::BroadcastValueMessage()
{
	if (mValueMessage != cmd_Nothing) {
		Int32	value = mValue;
		BroadcastMessage(mValueMessage, (void*) this);
	}
}

// This gets the mouse up and sends it to javascript in the event that
// a mouse down is cancelled; hot spot tracking normally eats the mouse up
// so this won't be called otherwise. We won't get the mouse up if it occurred
// outside the pane.
void CGAFormPushButton::EventMouseUp(const EventRecord& /*inEvent*/)
{
	CMochaHacks::SendEvent(fContext, EVENT_MOUSEUP, fLayoutElement);
}

void CGAFormPushButton::ClickSelfLayer(const SMouseDownEvent &inMouseDown)
{
	super::ClickSelf(inMouseDown);
}

/*
 * CGAFormPushButtonHotSpotResultMochaCallback
 * callback for HotSpotResult
 * crappy inline implementation, because the whole callback thing is crappy
 */
class CGAFormPushButtonHotSpotResultMochaCallback : public CMochaEventCallback
{
public:
	CGAFormPushButtonHotSpotResultMochaCallback( CGAFormPushButton * button, Int16 inHotSpot, int32 inEventType)
		: fButton(button), fHotSpot(inHotSpot), fEventType(inEventType) {}

	virtual void Complete(MWContext * context, 
				LO_Element * element, int32 /*type*/, ETEventStatus status)
	{
		if (status == EVENT_OK)
		{
			if (fEventType == EVENT_MOUSEUP)
				MochaClick(context, element, new CGAFormPushButtonHotSpotResultMochaCallback(fButton, fHotSpot, EVENT_CLICK));
		}
		if (fEventType == EVENT_CLICK)
		{
			// if event wasn't ok, set message to one that won't be sent, so button
			// will unhighlight but perform no action.
			MessageT saveMsg = fButton->GetValueMessage();
			if (status != EVENT_OK) fButton->SetValueMessage(msg_AnyMessage);
			try
			{
				fButton->HotSpotResultCallback( fHotSpot );
			}
			catch (...) {}
			if (status != EVENT_OK) fButton->SetValueMessage(saveMsg);
		}
	}

private:
	int32 fEventType; // the javascript event type, defined in libevent.h
	Int16 fHotSpot;
	CGAFormPushButton * fButton;
};

void
CGAFormPushButton::HotSpotResultCallback(Int16 inHotSpot)
{
	if (!IsMarkedForDeath())
	{
		super::HotSpotResult( inHotSpot );
	}
}

// This will actually get processed through the callback (see above method )
// Sends a mouse up to javascript, and if javascript lets us process the event, the callback sends
// a click to javascript.
void
CGAFormPushButton::HotSpotResult(Int16 inHotSpot)
{
	MWContext*		myContext = fContext;
	LO_Element*		myEle = fLayoutElement;
	
	MochaMouseUp(fContext, fLayoutElement, new CGAFormPushButtonHotSpotResultMochaCallback(this, inHotSpot, EVENT_MOUSEUP));
}

void
CGAFormPushButton::DrawButtonTitle	( Int16	inDepth )
{
	if(! (fContext && (GetWinCSID() == CS_UTF8)))
	{
		super::DrawButtonTitle(inDepth);
		return;
	}

	StColorPenState	theColorPenState;
	StColorPenState::Normalize ();
	StTextState			theTextState;
	RGBColor				textColor;
	
	// ¥ Get some loal variables setup including the rect for the title
	ResIDT	textTID = GetTextTraitsID ();
	Rect	titleRect;
	
	// ¥ Get the port setup with the text traits
	UTextTraits::SetPortTextTraits ( textTID );
	
	// ¥ Save off a reference to the fore color used to draw the title
	::GetForeColor ( &textColor );
	
	// ¥ We always want to have the title be centered
	Int16	titleJust = teCenter;
	
	// ¥ Calculate the title rect
	CalcTitleRect ( titleRect );
	
	// ¥ Get the title
	Str255 controlTitle;
	GetDescriptor ( controlTitle );

	// ¥ Handle drawing based on the current depth of the device
	if ( inDepth < 4 )
	{
		// ¥ If the control is dimmed then we use the grayishTextOr 
		// transfer mode to draw the text
		if ( !IsEnabled ())
		{
			::RGBForeColor ( &UGAColorRamp::GetBlackColor () );
			::TextMode ( grayishTextOr );
		}
		else if ( IsEnabled () && IsHilited () )
		{
			// ¥ When we are hilited we simply draw the title in white
			::RGBForeColor ( &UGAColorRamp::GetWhiteColor () );
		}
		
		// ¥ Now get the actual title drawn with all the appropriate settings
		UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
		UFontSwitcher *fs = UPropFontSwitcher::Instance();
		
		FontInfo info;	
		th->GetFontInfo(fs, &info);
		::MoveTo((titleRect.right + titleRect.left - 
				th->StringWidth(fs ,controlTitle) ) / 2,
				 (titleRect.top + titleRect.bottom + info.ascent  - info.descent) / 2);
		th->DrawString(fs, controlTitle);	
	
	}
	else if ( inDepth >= 4 ) 
	{
		// ¥ If control is selected we always draw the text in the title
		// hilite color, if requested
		if ( IsHilited ())
			::RGBForeColor ( &UGAColorRamp::GetWhiteColor() );
		
		// ¥ If the control is dimmed then we have to do our own version of the
		// grayishTextOr as it does not appear to work correctly across
		// multiple devices
		if ( !IsEnabled () || !IsActive ())
		{
			textColor = UGraphicsUtilities::Lighten ( &textColor );
			::TextMode ( srcOr );
			::RGBForeColor ( &textColor );
		}

		// ¥ Now get the actual title drawn with all the appropriate settings
															
		UMultiFontTextHandler *th = UUTF8TextHandler::Instance();
		UFontSwitcher *fs = UPropFontSwitcher::Instance();
		
		FontInfo info;	
		th->GetFontInfo(fs, &info);
		::MoveTo((titleRect.right + titleRect.left - 
				th->StringWidth(fs ,controlTitle) ) / 2,
				 (titleRect.top + titleRect.bottom + info.ascent  - info.descent) / 2);
		th->DrawString(fs, controlTitle);	
	}
	
}	//	CGAFormPushButton::DrawButtonTitle

#pragma mark == CFormRadio ==

//---------------------------------------------------------------------------
// class CFormRadio
//---------------------------------------------------------------------------
RgnHandle CFormRadio::sRadioRgn = NULL;
CFormRadio::CFormRadio(LStream * inStream) : LStdRadioButton(inStream), LFormElement(), CLayerClickCallbackMixin()
{
	SetReflectOnChange(true);
#ifdef LAYERS
	// add an attachment which will intercept clicks and dispatch them to layers
	CLayerClickDispatchAttachment* led = new CLayerClickDispatchAttachment;
	AddAttachment(led);
#endif
}

void CFormRadio::SetupClip()
{
	if (sRadioRgn == NULL)	// Allocate the clip region
	{
		sRadioRgn = ::NewRgn();
		Rect r;
		r.top = r.left = 0;
		r.bottom = r.right = 12;
		OpenRgn();
		::FrameOval(&r);
		::CloseRgn(sRadioRgn);
	}
	::OffsetRgn(sRadioRgn, 	// Position the clip region. Offset was calculated by trial and error
				(*mMacControlH)->contrlRect.left -(*sRadioRgn)->rgnBBox.left + 2, 
				(*mMacControlH)->contrlRect.top -(*sRadioRgn)->rgnBBox.top);
}

void CFormRadio::DrawSelf()
{
	SetupClip();
	StSetupFormDrawing colorEnv(this);

	StClipRgnState	theClip;
	theClip.ClipToIntersectionRgn(sRadioRgn);

	LStdRadioButton::DrawSelf();
}


void CFormRadio::ClickSelfLayer(const SMouseDownEvent	&inMouseDown)
{
	LStdRadioButton::ClickSelf(inMouseDown);
}

Boolean CFormRadio::TrackHotSpot(Int16	inHotSpot,Point inPoint, Int16 inModifiers)
{
	SetupClip();
	StSetupFormDrawing colorEnv(this);

	StClipRgnState	theClip;
	theClip.ClipToIntersectionRgn(sRadioRgn);

	return LStdRadioButton::TrackHotSpot(inHotSpot, inPoint, inModifiers);
}

void CFormRadio::SetValue(Int32	inValue)
{
	SetupClip();
	StSetupFormDrawing colorEnv(this);

	StClipRgnState	theClip;
	theClip.ClipToIntersectionRgn(sRadioRgn);

	LStdRadioButton::SetValue(inValue);
}

/*
 * CFormRadioHotSpotResultMochaCallback
 * callback for HotSpotResult
 * crappy inline implementation, because the whole callback thing is crappy
 */
class CFormRadioHotSpotResultMochaCallback : public CMochaEventCallback
{
public:
	CFormRadioHotSpotResultMochaCallback( CFormRadio * radio, 
									Int16 inHotSpot,
									LO_FormElementStruct* oldRadio,
									Int16 		savedValue,
									int32 inEventType)
		: fRadio(radio), 
		fHotSpot(inHotSpot),
		fOldRadio(oldRadio),
		fSavedValue(savedValue),
		fEventType(inEventType)
	{}

	void	Complete(MWContext * context, 
				LO_Element * element, int32 /*type*/, ETEventStatus status)
	{
		if (status == EVENT_CANCEL)
		{
			if (fEventType == EVENT_CLICK)
				fRadio->HotSpotResultCallback( fHotSpot , fOldRadio, fSavedValue, context);
		} else if (status == EVENT_OK)
		{
			if (fEventType == EVENT_MOUSEUP)
				MochaClick(context, 
							element, 
							new CFormRadioHotSpotResultMochaCallback(fRadio, fHotSpot, fOldRadio, fSavedValue, EVENT_CLICK));
		}
	}

private:
	int32					fEventType;
	Int16					fHotSpot;	// No clue what these arguments mean, I am just saving
	CFormRadio *			fRadio;		// them for the calllback
	LO_FormElementStruct*	fOldRadio;
	Int16					fSavedValue;
};


void
CFormRadio::HotSpotResultCallback(Int16 /*inHotSpot*/, 
								LO_FormElementStruct* oldRadio,
								Int16 savedValue,
								MWContext * context)
{
	if (!IsMarkedForDeath())
	{
		SetValue(savedValue);
		ReflectData();
		
		if (oldRadio != NULL)
		{
			LO_FormRadioSet(context, oldRadio);
				
			LControl* control = dynamic_cast<LControl*>(GetFEDataPane(oldRadio));
			ThrowIfNil_(control);
			
			control->SetValue(true);
			control->Refresh();
		}
	}
}

void
CFormRadio::HotSpotResult(Int16 inHotSpot)
{
	MWContext*	myContext 	= fContext;
	LO_Element*	myEle 		= fLayoutElement;
	Int16 		savedValue	= GetValue();

	LO_FormElementStruct*	oldRadio = NULL;
	
	// Change the value, call JavaScript, if it cancels 
	// the click, then change the value back.
	LStdRadioButton::HotSpotResult( inHotSpot );
	ReflectData();
	oldRadio = LO_FormRadioSet(myContext, (LO_FormElementStruct*) myEle);

	// Adding another callback loop to do a mouse up for xp consistency.
	// Cancelling the mouse up will have no effect on the state
	// of the radio element after the mouse down, but will prevent a click from being sent.
	MochaMouseUp( myContext, myEle, new CFormRadioHotSpotResultMochaCallback( this, inHotSpot, oldRadio, savedValue, EVENT_MOUSEUP));
}

// This gets the mouse up and sends it to javascript in the event that
// a mouse down is cancelled; hot spot tracking normally eats the mouse up
// so this won't be called otherwise. We won't get the mouse up if it occurred
// outside the pane.
void CFormRadio::EventMouseUp(const EventRecord& /*inEvent*/)
{
	CMochaHacks::SendEvent(fContext, EVENT_MOUSEUP, fLayoutElement);
}

#pragma mark == CGAFormRadio ==

// ---------------------------------------------------------------------------
//		¥ CGAFormRadio
// ---------------------------------------------------------------------------

CGAFormRadio::CGAFormRadio(
	LStream* inStream)
	:	super(inStream)
{
	SetReflectOnChange(true);
#ifdef LAYERS
	// add an attachment which will intercept clicks and dispatch them to layers
	AddAttachment(new CLayerClickDispatchAttachment);
#endif
}

// ---------------------------------------------------------------------------
//		¥ DrawSelf
// ---------------------------------------------------------------------------

void
CGAFormRadio::DrawSelf()
{
	StSetupFormDrawing colorEnv(this);

	super::DrawSelf();
}

// ---------------------------------------------------------------------------
//		¥ ClickSelfLayer
// ---------------------------------------------------------------------------

void
CGAFormRadio::ClickSelfLayer(
	const SMouseDownEvent& inMouseDown)
{
	super::ClickSelf(inMouseDown);
}

// ---------------------------------------------------------------------------
//		¥ TrackHotSpot
// ---------------------------------------------------------------------------

Boolean
CGAFormRadio::TrackHotSpot(
	Int16	inHotSpot,
	Point	inPoint,
	Int16 inModifiers)
{
	StSetupFormDrawing colorEnv(this);

	return super::TrackHotSpot(inHotSpot, inPoint, inModifiers);
}

// ---------------------------------------------------------------------------
//		¥ SetValue
// ---------------------------------------------------------------------------

void
CGAFormRadio::SetValue(
	Int32	inValue)
{
	StSetupFormDrawing colorEnv(this);

	super::SetValue(inValue);
}

/*
 * CGAFormRadioHotSpotResultMochaCallback
 * callback for HotSpotResult
 * crappy inline implementation, because the whole callback thing is crappy
 */
class CGAFormRadioHotSpotResultMochaCallback : public CMochaEventCallback
{
public:
	CGAFormRadioHotSpotResultMochaCallback( CGAFormRadio * radio, 
									Int16 inHotSpot,
									LO_FormElementStruct* oldRadio,
									Int16 		savedValue,
									int32 inEventType)
		: fRadio(radio), 
		fHotSpot(inHotSpot),
		fOldRadio(oldRadio),
		fSavedValue(savedValue),
		fEventType(inEventType)
	{}

	void	Complete(MWContext * context, 
				LO_Element * element, int32 /*type*/, ETEventStatus status)
	{
		if (status == EVENT_CANCEL)
		{
			if (fEventType == EVENT_CLICK)
				fRadio->HotSpotResultCallback( fHotSpot , fOldRadio, fSavedValue, context);
		} else if (status == EVENT_OK)
		{
			if (fEventType == EVENT_MOUSEUP)
				MochaClick(context, 
							element, 
							new CGAFormRadioHotSpotResultMochaCallback(fRadio, fHotSpot, fOldRadio, fSavedValue, EVENT_CLICK));
		}
	}

private:
	int32					fEventType;
	Int16					fHotSpot;	// No clue what these arguments mean, I am just saving
	CGAFormRadio*			fRadio;		// them for the calllback
	LO_FormElementStruct*	fOldRadio;
	Int16					fSavedValue;
};

// ---------------------------------------------------------------------------
//		¥ HotSpotResultCallback
// ---------------------------------------------------------------------------

void
CGAFormRadio::HotSpotResultCallback(
	Int16					/*inHotSpot*/, 
	LO_FormElementStruct*	oldRadio,
	Int16					savedValue,
	MWContext*				context)
{
	if (!IsMarkedForDeath())
	{
		SetValue(savedValue);
		ReflectData();
		
		if (oldRadio != NULL)
		{
			LO_FormRadioSet(context, oldRadio);
				
			LControl* control = dynamic_cast<LControl*>(GetFEDataPane(oldRadio));
			ThrowIfNil_(control);

			control->SetValue(true);
			control->Refresh();
		}
	}
}

// ---------------------------------------------------------------------------
//		¥ HotSpotResult
// ---------------------------------------------------------------------------

void
CGAFormRadio::HotSpotResult(
	Int16 inHotSpot)
{
	MWContext*	myContext 	= fContext;
	LO_Element*	myEle 		= fLayoutElement;
	Int16 		savedValue	= GetValue();

	LO_FormElementStruct*	oldRadio = NULL;
	
	// Change the value, call JavaScript, if it cancels 
	// the click, then change the value back.
	super::HotSpotResult( inHotSpot );
	ReflectData();
	oldRadio = LO_FormRadioSet(myContext, (LO_FormElementStruct*) myEle);

	// Adding another callback loop to do a mouse up for xp consistency.
	// Cancelling the mouse up will have no effect on the state
	// of the radio element after the mouse down, but will prevent a click from being sent.
	MochaMouseUp( myContext, myEle, new CGAFormRadioHotSpotResultMochaCallback(this, inHotSpot, oldRadio, savedValue, EVENT_MOUSEUP));
}

// ---------------------------------------------------------------------------
//		¥ EventMouseUp
// ---------------------------------------------------------------------------

// This gets the mouse up and sends it to javascript in the event that
// a mouse down is cancelled; hot spot tracking normally eats the mouse up
// so this won't be called otherwise. We won't get the mouse up if it occurred
// outside the pane.
void
CGAFormRadio::EventMouseUp(const EventRecord& /*inEvent*/)
{
	CMochaHacks::SendEvent(fContext, EVENT_MOUSEUP, fLayoutElement);
}

#pragma mark == CFormCheckbox ==

//---------------------------------------------------------------------------
// class CFormCheckbox
//---------------------------------------------------------------------------
RgnHandle CFormCheckbox::sCheckboxRgn = NULL;
CFormCheckbox::CFormCheckbox(LStream * inStream) : LStdCheckBox(inStream), CLayerClickCallbackMixin() {
	SetReflectOnChange(true);
#ifdef LAYERS
	// add an attachment which will intercept clicks and dispatch them to layers
	CLayerClickDispatchAttachment* led = new CLayerClickDispatchAttachment;
	AddAttachment(led);
#endif
}

void CFormCheckbox::SetupClip()
{
	if (sCheckboxRgn == NULL)	// Allocate the clip region
	{
		sCheckboxRgn = ::NewRgn();
		Rect r;
		r.top = r.left = 0;
		r.bottom = r.right = 12;
		OpenRgn();
		::FrameRect(&r);
		::CloseRgn(sCheckboxRgn);
	}
	::OffsetRgn(sCheckboxRgn, 	// Position the clip region
				(*mMacControlH)->contrlRect.left -(*sCheckboxRgn)->rgnBBox.left+2, 
				(*mMacControlH)->contrlRect.top -(*sCheckboxRgn)->rgnBBox.top);	
}


void CFormCheckbox::DrawSelf()
{
	SetupClip();	
	StSetupFormDrawing colorEnv(this);

	StClipRgnState	theClip;
	theClip.ClipToIntersectionRgn(sCheckboxRgn);

	LStdCheckBox::DrawSelf();
}


void CFormCheckbox::ClickSelfLayer(const SMouseDownEvent	&inMouseDown)
{
	LStdCheckBox::ClickSelf(inMouseDown);
}


Boolean CFormCheckbox::TrackHotSpot(Int16	inHotSpot,Point inPoint, Int16 inModifiers)
{
	SetupClip();	
	StSetupFormDrawing colorEnv(this);

	StClipRgnState	theClip;
	theClip.ClipToIntersectionRgn(sCheckboxRgn);
	
	return LStdCheckBox::TrackHotSpot(inHotSpot, inPoint, inModifiers);
}

void CFormCheckbox::SetValue(Int32	inValue)
{
	SetupClip();
	StSetupFormDrawing colorEnv(this);

	StClipRgnState	theClip;
	theClip.ClipToIntersectionRgn(sCheckboxRgn);

	LStdCheckBox::SetValue(inValue);
}

/*
 * CFormCheckboxHotSpotResultMochaCallback
 * callback for HotSpotResult
 * crappy inline implementation, because the whole callback thing is crappy
 */
class CFormCheckboxHotSpotResultMochaCallback : public CMochaEventCallback
{
public:
	CFormCheckboxHotSpotResultMochaCallback( CFormCheckbox* checkBox, 
									Int16 inHotSpot,
									Int16 inSavedValue,
									int32 inEventType )
		: fCheckbox(checkBox), fHotSpot(inHotSpot), fSavedValue(inSavedValue), fEventType(inEventType)
	{}

	virtual void Complete(MWContext * context, 
				LO_Element * element, int32 /*type*/, ETEventStatus status)
	{
		// call HotSpotResultCallback if mocha wants us to cancel action
		if (status == EVENT_CANCEL)
		{
			if (fEventType == EVENT_CLICK)
				fCheckbox->HotSpotResultCallback( fHotSpot, fSavedValue );
		} else if (status == EVENT_OK)
		{
			if (fEventType == EVENT_MOUSEUP)
				MochaClick(context, element, new CFormCheckboxHotSpotResultMochaCallback(fCheckbox, fHotSpot, fSavedValue, EVENT_CLICK));
		}
	}

private:
	int32					fEventType;
	Int16					fHotSpot;	// No clue what these arguments mean, I am just saving
	CFormCheckbox *			fCheckbox;		// them for the calllback
	Int16					fSavedValue;
};


void
CFormCheckbox::HotSpotResultCallback( Int16 /*inHotSpot*/, Int16 savedValue )
{
	// Reset to the old value
	if (!IsMarkedForDeath())
	{
		SetValue(savedValue);
		ReflectData();
	}
}

void CFormCheckbox::HotSpotResult(Int16 inHotSpot)
{
	MWContext*	myContext = fContext;
	LO_Element*	myEle = fLayoutElement;

	Int16 savedValue = GetValue();
	
	// Change the value, call JavaScript, if it cancels 
	// the click, then change the value back.

	LStdCheckBox::HotSpotResult(inHotSpot);
	ReflectData();
	
	//
	// Call the JavaScript onClick handler,
	// which may cancel the click
	//
	
	// Adding another callback loop to do a mouse up for xp consistency.
	// Cancelling the mouse up will have no effect on the state
	// of the checkbox after the mouse down, but will prevent a click from being sent.
	MochaMouseUp( myContext, myEle, new CFormCheckboxHotSpotResultMochaCallback( this, inHotSpot, savedValue, EVENT_MOUSEUP));
}

// This gets the mouse up and sends it to javascript in the event that
// a mouse down is cancelled; hot spot tracking normally eats the mouse up
// so this won't be called otherwise. We won't get the mouse up if it occurred
// outside the pane.
void CFormCheckbox::EventMouseUp(const EventRecord& /*inEvent*/)
{
	CMochaHacks::SendEvent(fContext, EVENT_MOUSEUP, fLayoutElement);
}

#pragma mark == CGAFormCheckbox ==

// ---------------------------------------------------------------------------
//		¥ CGAFormCheckbox
// ---------------------------------------------------------------------------

CGAFormCheckbox::CGAFormCheckbox(
	LStream* inStream)
	:	super(inStream)
{
	SetReflectOnChange(true);
#ifdef LAYERS
	// add an attachment which will intercept clicks and dispatch them to layers
	AddAttachment(new CLayerClickDispatchAttachment);
#endif
}

// ---------------------------------------------------------------------------
//		¥ DrawSelf
// ---------------------------------------------------------------------------

void
CGAFormCheckbox::DrawSelf()
{
	StSetupFormDrawing colorEnv(this);

	super::DrawSelf();
}

// ---------------------------------------------------------------------------
//		¥ ClickSelfLayer
// ---------------------------------------------------------------------------

void
CGAFormCheckbox::ClickSelfLayer(
	const SMouseDownEvent& inMouseDown)
{
	super::ClickSelf(inMouseDown);
}

// ---------------------------------------------------------------------------
//		¥ TrackHotSpot
// ---------------------------------------------------------------------------

Boolean
CGAFormCheckbox::TrackHotSpot(
	Int16	inHotSpot,
	Point	inPoint,
	Int16 inModifiers)
{
	StSetupFormDrawing colorEnv(this);

	return super::TrackHotSpot(inHotSpot, inPoint, inModifiers);
}

// ---------------------------------------------------------------------------
//		¥ SetValue
// ---------------------------------------------------------------------------

void
CGAFormCheckbox::SetValue(
	Int32	inValue)
{
	StSetupFormDrawing colorEnv(this);

	super::SetValue(inValue);
}

/*
 * CGAFormCheckboxHotSpotResultMochaCallback
 * callback for HotSpotResult
 * crappy inline implementation, because the whole callback thing is crappy
 */
class CGAFormCheckboxHotSpotResultMochaCallback : public CMochaEventCallback
{
public:
	CGAFormCheckboxHotSpotResultMochaCallback( CGAFormCheckbox* checkBox, 
									Int16 inHotSpot,
									Int16 inSavedValue,
									int32 inEventType )
		: fCheckbox(checkBox), fHotSpot(inHotSpot), fSavedValue(inSavedValue), fEventType(inEventType)
	{}

	virtual void Complete(MWContext * context, 
				LO_Element * element, int32 /*type*/, ETEventStatus status)
	{
		// call HotSpotResultCallback if mocha wants us to cancel action
		if (status == EVENT_CANCEL)
		{
			if (fEventType == EVENT_CLICK)
				fCheckbox->HotSpotResultCallback( fHotSpot, fSavedValue );
		} else if (status == EVENT_OK)
		{
			if (fEventType == EVENT_MOUSEUP)
				MochaClick(context, element, new CGAFormCheckboxHotSpotResultMochaCallback(fCheckbox, fHotSpot, fSavedValue, EVENT_CLICK));
		}
	}

private:
	int32					fEventType;
	Int16					fHotSpot;	// No clue what these arguments mean, I am just saving
	CGAFormCheckbox*		fCheckbox;		// them for the calllback
	Int16					fSavedValue;
};

// ---------------------------------------------------------------------------
//		¥ HotSpotResultCallback
// ---------------------------------------------------------------------------

void
CGAFormCheckbox::HotSpotResultCallback(
	Int16					inHotSpot, 
	Int16					savedValue)
{
#pragma unused(inHotSpot)

	// Reset to the old value
	if (!IsMarkedForDeath())
	{
		SetValue(savedValue);
		ReflectData();
	}
}

// ---------------------------------------------------------------------------
//		¥ HotSpotResult
// ---------------------------------------------------------------------------

void
CGAFormCheckbox::HotSpotResult(
	Int16 inHotSpot)
{
	MWContext*	myContext = fContext;
	LO_Element*	myEle = fLayoutElement;

	Int16 savedValue = GetValue();
	
	// Change the value, call JavaScript, if it cancels 
	// the click, then change the value back.

	super::HotSpotResult(inHotSpot);
	ReflectData();
	
	//
	// Call the JavaScript onClick handler,
	// which may cancel the click
	//
	
	// Adding another callback loop to do a mouse up for xp consistency.
	// Cancelling the mouse up will have no effect on the state
	// of the checkbox after the mouse down, but will prevent a click from being sent.
	MochaMouseUp( myContext, myEle, new CGAFormCheckboxHotSpotResultMochaCallback( this, inHotSpot, savedValue, EVENT_MOUSEUP));
}

// ---------------------------------------------------------------------------
//		¥ EventMouseUp
// ---------------------------------------------------------------------------

// This gets the mouse up and sends it to javascript in the event that
// a mouse down is cancelled; hot spot tracking normally eats the mouse up
// so this won't be called otherwise. We won't get the mouse up if it occurred
// outside the pane.
void
CGAFormCheckbox::EventMouseUp(const EventRecord& /*inEvent*/)
{
	CMochaHacks::SendEvent(fContext, EVENT_MOUSEUP, fLayoutElement);
}

#pragma mark == CFormFile ==

//
// class CFormFile
// 

CFormFile::CFormFile(LStream * inStream) : LView(inStream), LFormElement()
{
	fHasFile = FALSE;

	SetReflectOnChange(true);	
}

void CFormFile::FinishCreateSelf()
{
	fEditField = (CFormFileEditField *)FindPaneByID(CFormFileEditField::class_ID);
	fButton = (LStdButton*)FindPaneByID('ffib');
	ThrowIfNil_(fButton);
	ThrowIfNil_(fEditField);
	fButton->AddListener(this);
}

void CFormFile::SetVisibleChars(Int16 numOfChars)
{
// Resize the edit field to the proper number of characters.
	FocusDraw();	// Algorithm copied from CFormLittleText::SetVisibleChars
	UTextTraits::SetPortTextTraits(fEditField->mTextTraitsID);
	FontInfo fInfo;
	::GetFontInfo(&fInfo);
	fEditField->ResizeFrameTo(::CharWidth('a') * numOfChars + 6, fInfo.ascent + fInfo.descent+ fInfo.leading + 4, FALSE);	
// Move the button so that it does not overlap	
	SPoint32 location;
	SDimension16 size;
	fEditField->GetFrameLocation(location);
	fEditField->GetFrameSize(size);
	fButton->PlaceInSuperFrameAt(location.h+size.width + 2 - mFrameLocation.h, 0, FALSE);
// Resize ourselves
	fButton->GetFrameLocation(location);
	fButton->GetFrameSize(size);
	ResizeFrameTo(location.h+size.width + 2, size.height, FALSE);
// Reset ourselves
	fHasFile = FALSE;
	fEditField->SetDescriptor(CStr255::sEmptyString);
}

void CFormFile::GetFontInfo(Int32& width, Int32& height, Int32& baseline)
{
	FocusDraw();	// Algorithm copied from CFormLittleText::SetVisibleChars
	UTextTraits::SetPortTextTraits(fEditField->mTextTraitsID);
	FontInfo fInfo;
	::GetFontInfo(&fInfo);
	SPoint32 location;
	fEditField->GetFrameLocation(location);
	width = mFrameSize.width;
	height = mFrameSize.height;
	baseline = fInfo.ascent+3 + location.v - mFrameLocation.v;	// Looking for the baseline of the text field
}

Boolean CFormFile::GetFileSpec(FSSpec & spec)
{
	if (!fHasFile)
		return FALSE;
	else
	{
		spec = fSpec;
		return TRUE;
	}
}

void CFormFile::SetFileSpec(FSSpec & spec)
{
	fSpec = spec;
	fHasFile = TRUE;
	fEditField->FocusDraw();
	fEditField->SetDescriptor(fSpec.name);	// Make sure that all text is selected
	::TESetSelect(0, 32767, fEditField->mTextEditH);
	

	MochaChanged();
}

void CFormFile::ListenToMessage(MessageT inMessage, void */*ioParam*/)
{
	switch (inMessage)	{
		case 2001:
		{
			StandardFileReply myReply;
			SFTypeList myTypes;
			UDesktop::Deactivate();	// Always bracket this
			StandardGetFile(NULL, -1, myTypes, &myReply);
			UDesktop::Activate();
			if (myReply.sfGood)
			{
// Fortezza and MacBinary form submission uploads need to be able to select a file
// that just has a resource fork so we can't reject these files any more
//				if (!CFileMgr::FileHasDataFork(myReply.sfFile))
//					ErrorManager::PlainAlert(SUBMIT_FILE_WITH_DATA_FK);
//				else
					SetFileSpec(myReply.sfFile);	
			}
		}
		break;
		case 2002:	// User has deleted the text
			fHasFile = FALSE;
			MochaChanged();
		default:
			break;
	}
}

#pragma mark == CFormFileEditField ==

//
// CFormFileEditField
//

CFormFileEditField::CFormFileEditField(LStream * inStream) : LEditField (inStream)
{
	mKeyFilter = UKeyFilters::AlphaNumericField;
}

void CFormFileEditField::ClickSelf(const SMouseDownEvent	&/*inMouseDown*/)
{
	SwitchTarget(this);
	::TESetSelect(0, 32767, mTextEditH);
}


void
CFormFileEditField::DrawSelf()
{
	Rect	frame;
	CalcLocalFrameRect(frame);

	::InsetRect(&frame, 1,1);
	::EraseRect(&frame);

	LEditField::DrawSelf();	

	::InsetRect(&frame, -1,-1);
	UGraphics::FrameRectSubtle(frame, true);	
}


void CFormFileEditField::FindCommandStatus(CommandT	inCommand,
							Boolean		&outEnabled,
							Boolean		&outUsesMark,
							Char16		&outMark,
							Str255		outName)
{
	switch (inCommand) {
	
		case cmd_Cut:				// Cut, Copy, and Clear enabled
		case cmd_Copy:				//   if something is selected
		case cmd_Clear:
			outEnabled = ((**mTextEditH).selStart != (**mTextEditH).selEnd);
			break;
					
		case cmd_Paste: {
			outEnabled = FALSE;
			break;
		}
		
		case cmd_SelectAll:			// Check if any characters are present
			outEnabled = (**mTextEditH).teLength > 0;
			break;
			
		default:
			LCommander::FindCommandStatus(inCommand, outEnabled,
									outUsesMark, outMark, outName);
			break;
	}
}

// Copied from LEditField::HandleKeyPress.
// Filter everything byt delete
Boolean	CFormFileEditField::HandleKeyPress(const EventRecord&	inKeyEvent)
{
	Boolean		keyHandled = true;
	EKeyStatus	theKeyStatus = keyStatus_Input;
	Int16		theKey = inKeyEvent.message & charCodeMask;
	
	if (inKeyEvent.modifiers & cmdKey) {	// Always pass up when the command
		theKeyStatus = keyStatus_PassUp;	//   key is down
	
	} else if (mKeyFilter != nil) {
		Char16		charCode = inKeyEvent.message & charCodeMask;
		theKeyStatus = (*mKeyFilter)(mTextEditH, inKeyEvent.message, charCode, inKeyEvent.modifiers);
	}
	switch(theKeyStatus)
	{
		case keyStatus_TEDelete:
			return LEditField::HandleKeyPress(inKeyEvent);
		default:
			return LCommander::HandleKeyPress(inKeyEvent);	// Bypass the edit field
	}
	return FALSE;
}

void CFormFileEditField::UserChangedText()
{
	((CFormFile *)mSuperView)->ListenToMessage(2002, NULL);
}



static LArray*	gDeadFormElementsList 	= NULL;
static void*	gKillFormElementsTimer 	= 0;	// timer ID

static void
KillFormElementsTimer(	void */*ignore*/ )
{
	Assert_(gDeadFormElementsList != NULL);
	
	LArrayIterator 	iterator(*gDeadFormElementsList);
	LPane*			pane;
	
	while (iterator.Next(&pane)) {
		delete pane;
	}

	gDeadFormElementsList->RemoveItemsAt(
								gDeadFormElementsList->GetCount(), 
								LArray::index_First);
								
	gKillFormElementsTimer = 0;								
}


// Switch focus to an input element

void
FE_FocusInputElement(	MWContext	*	context,
						LO_Element	*	element)
{
	
	Assert_(context != NULL);
	
	// If element is NULL, we are to give focus to
	// the window itself, bringing it to front if
	// necessary...
	//
	if (element == NULL)
	{
		FE_RaiseWindow(context);
	}
	else
	{
		LO_FormElementStruct * formElem;
		LFormElement		 * toFocus;
		FormFEData			 * feData;

		Assert_(element->type == LO_FORM_ELE);
		
			
		formElem 	= (LO_FormElementStruct *) element;
		feData		= GetFEData(formElem);
		if ( feData == NULL) {
			return;
		}

	// Can't call SwitchTarget, because the Mocha script calling
	// this callback may have been invoked by a target switch
	// that is in progress, but not complete.
	//
	// So we set a timer to do it for us...

			
		toFocus = feData->fHost;

		if (toFocus != NULL)
		{
			// limitation of 1 timer per form element
			if (toFocus->fTimer != 0)
				FE_ClearTimeout(toFocus->fTimer);
			
			toFocus->fTimer = FE_SetTimeout((TimeoutCallbackFunction)FocusFormElementTimer, (void*)formElem, 0);
		}
		
	//	toFocus 	= (LCommander *) GetFEDataPane(formElement);
	//	Assert_(toFocus != NULL);
	//	LCommander::SwitchTarget(toFocus);
	
	}
}

// Take focus away from a input element and give it
// to its commander
void
FE_BlurInputElement(	MWContext	*	context,
						LO_Element	*	element)
{
	// If element is NULL, we blur focus from the window,
	// but only if the window contains the current target,
	// as denoted by the result of IsOnDuty()
	//
	// When we blur focus, we send focus to nothing...
	//
	if (element == NULL)
	{
		FE_LowerWindow(context);
	}
	else
	{
		LO_FormElementStruct 	* formElem;
		LFormElement			* unFocus;
		FormFEData				* feData;
			
		Assert_(context != NULL && element != NULL);
		Assert_(element->type == LO_FORM_ELE);
		
		formElem	= (LO_FormElementStruct *) element;
		feData 		= GetFEData(formElem);
		if (feData == NULL) { 
			return;
		}
		
		unFocus = feData->fHost;
		
	// Can't call SwitchTarget, because the Mocha script calling
	// this callback may have been invoked by a target switch
	// that is in progress, but not complete.
	//
	// So we set a timer to do it for us...

		if (unFocus != NULL)
		{
			// limitation of 1 timer per form element
			if (unFocus->fTimer != 0)
				FE_ClearTimeout(unFocus->fTimer);
				
			unFocus->fTimer = FE_SetTimeout((TimeoutCallbackFunction)BlurFormElementTimer, (void*)formElem, 0);	
		}
		
	/*	if (LCommander::GetTarget() == unFocus)
			LCommander::SwitchTarget(unFocus->GetSuperCommander());
		else
			Assert_(false);
	*/		
	}
}


//
// Timer callback to switch the target
// to a given form element
//
static void
FocusFormElementTimer(	LO_FormElementStruct * formElem )
{
	Assert_(formElem != NULL);
	Assert_(GetFEData(formElem) != NULL);
	
	if ( !GetFEData( formElem ) )
		return;
	
	// Having no element_data would be a BAD THING 
	if (!formElem->element_data)
		return;
	
	LCommander		*	commander = FEDATACOMMANDER;
	LFormElement	*	element = FEDATAHOST;
		
	element->fTimer = 0;
	
	if (commander != NULL)
		LCommander::SwitchTarget(commander);
}

//
// Timer callback to take focus from
// a form element.
//
static void
BlurFormElementTimer(	LO_FormElementStruct * formElem )
{
	Assert_(formElem != NULL);
	Assert_(GetFEData(formElem) != NULL);
	
	if ( !GetFEData( formElem ) )
		return;
	
	// Having no element_data would be a BAD THING
	if (!formElem->element_data)
		return;
	
	LCommander		*	commander = FEDATACOMMANDER;
	LFormElement	*	element = FEDATAHOST;
			
	element->fTimer = 0;
	
	if (commander != NULL)
	{
	
		if (LCommander::GetTarget() == commander)
			LCommander::SwitchTarget(commander->GetSuperCommander());
		else
			Assert_(false);
	}
}


//
// "Select" the data in an input element.
//	For now this only applies to text fields
// 
void
FE_SelectInputElement(	MWContext	*	window,
						LO_Element	*	element)
{
	LO_FormElementStruct * formElement;	
	
	Assert_(window != NULL && element != NULL);
	Assert_(element->type == LO_FORM_ELE);
		
	formElement = (LO_FormElementStruct *) element;

	switch (formElement->element_data->type) 
	{
		// LEditFields - just call SelectAll
		case FORM_TYPE_TEXT:
		case FORM_TYPE_PASSWORD:
		case FORM_TYPE_ISINDEX:
		case FORM_TYPE_READONLY:
		{
			if ( !GetFEData( formElement ) )
				return;
			if ( !GetFEDataPane( formElement ) )
				return;
			LEditField	*editField = (LEditField *) GetFEDataPane(formElement);
			editField->SelectAll();
			break;
		}
		
		// Text engine - select all
		case FORM_TYPE_TEXTAREA:
			if ( !GetFEData( formElement ) )
				return;
			if ( !GetFEDataPane( formElement ) )
				return;

			LPane 		*scroller = GetFEDataPane(formElement);			
			CSimpleTextView	*textEdit = (CSimpleTextView*) scroller->FindPaneByID(formBigTextID);	Assert_(textEdit != NULL);
			textEdit->SelectAll();
			
			break;
					
/*	These are not selectable

		case FORM_TYPE_FILE:
		case FORM_TYPE_SELECT_ONE:
		case FORM_TYPE_SELECT_MULT:
		case FORM_TYPE_RADIO:
		case FORM_TYPE_CHECKBOX:	
		case FORM_TYPE_HIDDEN:
		case FORM_TYPE_KEYGEN:	
		case FORM_TYPE_SUBMIT:
		case FORM_TYPE_RESET:
		case FORM_TYPE_BUTTON:
		case FORM_TYPE_IMAGE:
		case FORM_TYPE_JOT:
		case FORM_TYPE_OBJECT:
*/
		default: break;
	}		
}	


//
// Notification that a input element's data has changed.
// Grab the data from XP using ResetFormElement and
// then redraw...
//
void
FE_ChangeInputElement(	MWContext	*	window,
						LO_Element	*	element)
{
	LO_FormElementStruct	*	formElement;
	
	Assert_(window != NULL && element != NULL);
	Assert_(element->type == LO_FORM_ELE);
	
	Try_
	{
		LO_LockLayout(); 	// prevent race condition with mocha thread	
		// reflect back from Layout
		formElement = (LO_FormElementStruct *) element;
		UFormElementFactory::ResetFormElementData(formElement, true, false, false);
		// pkc -- If we're setting a radio button, call LO_FormRadioSet
		// to turn off other radio buttons.
		// WHY AREN'T WE USING LRadioGroup?!?!?!?!?!?!?!?!?!?!?!?!?!?!?
		if ( formElement->element_data->type == FORM_TYPE_RADIO &&
			 ((lo_FormElementToggleData *)(formElement->element_data))->toggled )
			LO_FormRadioSet(window, formElement);
	}
	Catch_(inErr)
	{
		LO_UnlockLayout();
		Throw_(inErr);
	}
	EndCatch_
		LO_UnlockLayout();
}										


// 
// Initiate a form submission through a given input element
//
void
FE_SubmitInputElement(	MWContext	*	window,
						LO_Element	*	element)
{
	LO_FormElementStruct	*	formElement;
	URL_Struct 				* 	url;
	LO_FormSubmitData		*	submitData;
	History_entry			* 	currentHistoryPosition;
	
	Assert_(window != NULL && element != NULL);	
	
	// I get an element->type of LO_IMAGE when going to VeriSign
	Assert_(element->type == LO_FORM_ELE || element->type == LO_IMAGE);
	
	formElement = (LO_FormElementStruct *) element;
	
	//
	// Call the LO module to figure out the form's context
	//
	submitData = LO_SubmitForm(window, formElement);
	if (submitData == NULL)	
		return;

#ifdef SingleSignon
	// Check for a password submission and remember the data if so
	SI_RememberSignonData(window, submitData);
#endif
	
	url =  NET_CreateURLStruct((char *)submitData->action, NET_DONT_RELOAD);
	currentHistoryPosition = SHIST_GetCurrent( &window->hist );

	if (currentHistoryPosition && currentHistoryPosition->address)
		url->referer = XP_STRDUP(currentHistoryPosition->address);
	
	NET_AddLOSubmitDataToURLStruct(submitData, url);

	CBrowserContext* theNSContext = ExtractBrowserContext(window);
	Assert_(theNSContext != NULL);
	theNSContext->SwitchLoadURL(url, FO_CACHE_AND_PRESENT);
	
	LO_FreeSubmitData(submitData);
}	


//
// Simulate a "hot-spot" click on an input element.
// Applies only to buttons.
//
void
FE_ClickInputElement(	MWContext	*	window,
						LO_Element	*	element)
{

	LO_FormElementStruct	*	formElement;
	
	Assert_(window != NULL && element != NULL);
	Assert_(element->type == LO_FORM_ELE);

	formElement = (LO_FormElementStruct *) element;
	switch (formElement->element_data->type) 
	{
		case FORM_TYPE_SUBMIT:
		case FORM_TYPE_RESET:
		case FORM_TYPE_BUTTON:
			if ( !GetFEData( formElement ) )
				return;
			if ( !GetFEDataPane( formElement ) )
				return;

			LControl	*control = (LControl *) GetFEDataPane(formElement);	
			control->SimulateHotSpotClick(0);
			break;
					
/*	These are not programmatically "clickable"

		case FORM_TYPE_TEXT:
		case FORM_TYPE_PASSWORD:
		case FORM_TYPE_ISINDEX:
		case FORM_TYPE_TEXTAREA:
		case FORM_TYPE_FILE:
		case FORM_TYPE_SELECT_ONE:
		case FORM_TYPE_SELECT_MULT:
		case FORM_TYPE_RADIO:
		case FORM_TYPE_CHECKBOX:	
		case FORM_TYPE_HIDDEN:
		case FORM_TYPE_KEYGEN:	
		case FORM_TYPE_IMAGE:
		case FORM_TYPE_JOT:
		case FORM_TYPE_OBJECT:
*/
		default:	break;
	}
}

void MochaFocusCallback(	MWContext * /* pContext */,
							LO_Element * /* lo_element */,
							int32 /* lType */,
							void * whatever,
							ETEventStatus status	)
{
	if (status == EVENT_OK && whatever)
	{
		LFormElement* element = reinterpret_cast<LFormElement*>(whatever);
		element->ClearInFocusCallAlready();
		// 97-06-14 pkc -- call virtual callback method
		element->PostMochaFocusCallback();
	}
}


void
LFormElement::MochaFocus( 	Boolean doFocus,
							Boolean switchTarget)
{
	// Don't do anything if we're really dead
	if (IsMarkedForDeath()) 
		return;
		
	// ick ick ick ick ick
	//
	// Here's the deal...
	// PowerPlant doesn't change sTarget before
	// calling it's DontBeTarget method, so if that
	// method does something to change the target,
	// you recurse.
	//		
	if (fInFocusCallAlready && doFocus == fPreviousFocus)
	{
		fPreviousFocus = doFocus;
		return;
	}
	else
	{
		fPreviousFocus = doFocus;
	}
		
	if (!XP_IsContextInList(fContext))
		return;
		
	fInFocusCallAlready = true;
	
	if (doFocus) 
	{
		if (switchTarget)
			sTargetedFormElement = this;
		
		// ET_SendEvent now takes a JSEvent struct instead of an int type
		JSEvent* event = XP_NEW_ZAP(JSEvent);
		if (event)
		{
			event->type = EVENT_FOCUS;
			event->layer_id = fLayoutElement->lo_form.layer_id;
			ET_SendEvent( fContext, fLayoutElement, event, MochaFocusCallback, this );
		}
	}
	else
	{
		if (switchTarget)
			sTargetedFormElement = NULL;
		
		if (fMochaChanged)
		{
			// fReflectOnChange denotes when LM_SendOnChange should be called.
			// If it is true, the call is made immediately when the data changes.
			// If false, the call is made here, when focus is lost.
			// 
			// The only reason for this flag is that some form elements are not
			// focusable and therefore must immediately reflect state changes.
			
			if (!fReflectOnChange)
				ReflectData();
				
			fMochaChanged = false;	// important to do this first to avoid recursion

			// ET_SendEvent now takes a JSEvent struct instead of an int type
			JSEvent* event = XP_NEW_ZAP(JSEvent);
			if (event)
			{
				event->type = EVENT_CHANGE;
				event->layer_id = fLayoutElement->lo_form.layer_id;
				ET_SendEvent( fContext, fLayoutElement, event, MochaFocusCallback, this );
			}
		}
		
		// Don't do anything if the onChange handler killed this form element
		if (!IsMarkedForDeath())
		{
			// ET_SendEvent now takes a JSEvent struct instead of an int type
			JSEvent* event = XP_NEW_ZAP(JSEvent);
			if (event)
			{
				event->type = EVENT_BLUR;
				event->layer_id = fLayoutElement->lo_form.layer_id;
				ET_SendEvent( fContext, fLayoutElement, event, MochaFocusCallback, this );
			}
		}
	}

//	97-06-03 pkc -- this is now set top false in MochaFocusCallback
//	fInFocusCallAlready = false;
 }


#pragma global_optimizer off

StTempFormBlur::StTempFormBlur()
{
	fSavedFocus = LFormElement::GetTargetedElement();
	
	if ( fSavedFocus )
	{
		// 97.08.18 pchen -- fix bug #79140
		// If fSavedFocus is an LCommander, call SwitchTarget on it's
		// supercommander to "unfocus" form element.
		LCommander* commander = dynamic_cast<LCommander*>(fSavedFocus);
		if (commander)
		{
			LCommander* super = commander->GetSuperCommander();
			if (super)
			{
				LCommander::SwitchTarget(super);
				fSavedFocus = NULL;
			}
		}
		else
			fSavedFocus->MochaFocus(false, false);
	}
}


StTempFormBlur::~StTempFormBlur()
{
	if ( fSavedFocus )
		fSavedFocus->MochaFocus(true, false);
		
	fSavedFocus = NULL;	
}


LFormElement* LFormElement::sTargetedFormElement = NULL;


void
LFormElement:: MochaSelect()
{	
	// don't do anything if we're dead
	if (IsMarkedForDeath()) return;
	
	// ET_SendEvent now takes a JSEvent struct instead of an int type
	JSEvent* event = XP_NEW_ZAP(JSEvent);
	if (event)
	{
		event->type = EVENT_SELECT;
		event->layer_id = fLayoutElement->lo_form.layer_id;
		ET_SendEvent( fContext, fLayoutElement, event, NULL, NULL );
	}
}

void MochaChangedCallback(	MWContext * /* pContext */,
							LO_Element * /* lo_element */,
							int32 /* lType */,
							void * whatever,
							ETEventStatus status	)
{
	if (status == EVENT_OK && whatever)
	{
		LFormElement* element = reinterpret_cast<LFormElement*>(whatever);
		// 97-06-14 pkc -- call PostMochaFocusCallback to handle FormsPopup special case
		element->PostMochaFocusCallback();
	}
}

Boolean HasKeyEventCapture(MWContext* context);

// Returns whether the context or any of its parents is capturing key events.
Boolean HasKeyEventCapture(MWContext* context)
{
	MWContext *mwcx;
    uint32 mask = EVENT_KEYDOWN | EVENT_KEYUP | EVENT_KEYPRESS;
	
    if (context->event_bit & mask) return true;

    if (context->is_grid_cell) 
    {
        mwcx = context;
		while (mwcx->grid_parent != NULL) 
		{
		    mwcx = mwcx->grid_parent;
		    if (mwcx->event_bit & mask) return true;
		}
    }
	return false;
}

void
LFormElement::MochaChanged()
{
	// don't do anything if we're dead OR we don't have a fLayoutElement which
	// can be the case when we are creating a text area and the text engine sends
	// a message that the newly created area has changed before we get a chance to
	// assign it a fLayoutElement
	if (IsMarkedForDeath() || fLayoutElement == NULL)
		return;
	
	//
	// If fReflectOnChange is false,
	// we will reflect the data when the
	// element loses focus.
	//
	// If true, we must reflect it now
	//	
	if (fReflectOnChange) 
	{
		fMochaChanged = false;
		ReflectData(); 

		// ET_SendEvent now takes a JSEvent struct instead of an int type
		JSEvent* event = XP_NEW_ZAP(JSEvent);
		if (event)
		{
			event->type = EVENT_CHANGE;
			event->layer_id = fLayoutElement->lo_form.layer_id;
//			ET_SendEvent( fContext, fLayoutElement, event, NULL, NULL );
			// 97-06-14 pkc -- Use MochaChangedCallback to handle FormsPopup special case
			ET_SendEvent( fContext, fLayoutElement, event, MochaChangedCallback, this );
		}
	}
	else
	{
		// if the form element is a text entry form and there is a key event handler for
		// any containing context or for the form, reflect the data.
		LO_FormElementData* data = fLayoutElement->lo_form.element_data;
		if (data != NULL &&
			(data->type == FORM_TYPE_TEXTAREA ||
			 data->type == FORM_TYPE_TEXT ||
			 data->type == FORM_TYPE_PASSWORD) &&
			(fLayoutElement->lo_form.event_handler_present || HasKeyEventCapture(fContext)))
		{
			ReflectData();
		}
		fMochaChanged = true; 	
	}
}


void
LFormElement::MochaSubmit()	
{	
	// don't do anything if we're dead
	if (IsMarkedForDeath()) return;
	
	StTempFormBlur	tempBlur;
	// ET_SendEvent now takes a JSEvent struct instead of an int type
	JSEvent* event = XP_NEW_ZAP(JSEvent);
	if (event)
	{
		event->type = EVENT_SUBMIT;
		event->layer_id = fLayoutElement->lo_form.layer_id;
		ET_SendEvent( fContext, fLayoutElement, event, NULL, NULL );
	}
}
	
void
MochaClick( MWContext* context, LO_Element* ele, CMochaEventCallback * cb )	
{	
	StTempFormBlur tempBlur;
	JSEvent* event = XP_NEW_ZAP(JSEvent);
	if (event)
	{
		LO_FormElementStruct* form = (LO_FormElementStruct *)ele;
		CHTMLView* view = dynamic_cast<CHTMLView*>(GetFEDataPane(form)->GetSuperView());
		ThrowIfNil_(view);
		SPoint32 formPos = {CL_GetLayerXOrigin(form->layer), CL_GetLayerYOrigin(form->layer)};
		Point screenPt;
		view->ImageToAvailScreenPoint(formPos, screenPt);
		event->type = EVENT_CLICK;
		event->which = 1;
		event->x = CL_GetLayerXOffset(form->layer);
		event->y = CL_GetLayerYOffset(form->layer);
		event->docx = CL_GetLayerXOrigin(form->layer);
		event->docy = CL_GetLayerYOrigin(form->layer);
		event->screenx = screenPt.h;
		event->screeny = screenPt.v;
		event->layer_id = form->layer_id;
		event->modifiers = CMochaHacks::MochaModifiersFromKeyboard();
		ET_SendEvent( context, ele, event, CMochaEventCallback::MochaCallback, cb);
	}
	//cb->SendEvent(context, ele, EVENT_CLICK);
}

// EventMouseUp
void
MochaMouseUp( MWContext* context, LO_Element* ele, CMochaEventCallback * cb )	
{	
	JSEvent* event = XP_NEW_ZAP(JSEvent);
	if (event)
	{
		LO_FormElementStruct* form = (LO_FormElementStruct *)ele;
		CHTMLView* view = dynamic_cast<CHTMLView*>(GetFEDataPane(form)->GetSuperView());
		ThrowIfNil_(view);
		SPoint32 formPos = {CL_GetLayerXOrigin(form->layer), CL_GetLayerYOrigin(form->layer)};
		Point screenPt;
		view->ImageToAvailScreenPoint(formPos, screenPt);
		event->type = EVENT_MOUSEUP;
		event->which = 1;
		event->x = CL_GetLayerXOffset(form->layer);
		event->y = CL_GetLayerYOffset(form->layer);
		event->docx = CL_GetLayerXOrigin(form->layer);
		event->docy = CL_GetLayerYOrigin(form->layer);
		event->screenx = screenPt.h;
		event->screeny = screenPt.v;
		event->layer_id = form->layer_id;
		event->modifiers = CMochaHacks::MochaModifiersFromKeyboard();
		ET_SendEvent( context, ele, event, CMochaEventCallback::MochaCallback, cb);
	}
	//cb->SendEvent(context, ele, EVENT_MOUSEUP);
}

#pragma global_optimizer on


void
LFormElement::ReflectData()	// reflect to XP data structures
{
	if (!XP_IsContextInList(fContext))
		return;
	
	UFormElementFactory::GetFormElementValue(&(fLayoutElement->lo_form), false);
}


LFormElement::LFormElement()
{
	fTimer						= 0; 
	fLayoutElement 				= NULL; 
	fContext 					= NULL; 
	fPane						= NULL;
	fReflectOnChange 			= false; 
	fInFocusCallAlready 		= false; 
	fMarkedForDeath 			= false;
	fPreviousFocus = false;
}

void
LFormElement::InitFormElement(MWContext * context, LO_FormElementStruct *lo_element)  
{
 	fContext = context;
 	fLayoutElement = (LO_Element *) lo_element;
}

/*
	LFormElement::MarkForDeath()

	Adds the form element's pane to the list of form elements to be destroyed
	at idle time.  This is necessary because JavaScript invoked from a widget
	class may clear the document, killing the widget in the process, but the
	widget may do more processing after the call to JavaScript returns, so
	we can't realyl kill it until sometime later, like idle time.
*/
void
LFormElement::MarkForDeath()
{
	fMarkedForDeath = true;
	
	// Hide the element's pane, and add it to death row
	if (fPane != NULL)
	{	
		fPane->Hide();
		
		if (gDeadFormElementsList == NULL)
		{
			gDeadFormElementsList = new LArray;
			if (gDeadFormElementsList == NULL) return;	// leak me
		}

		gDeadFormElementsList->InsertItemsAt(1, LArray::index_First, &fPane);
			
		// set a timer to kill the elements in the list	
		if (gKillFormElementsTimer == 0) {
			gKillFormElementsTimer = FE_SetTimeout(KillFormElementsTimer, 0, 0);
		}
	}
}


/*
	LFormElement::~LFormElement()

	If this form element is the targeted form element, clear that field.
	
	If this form element has been marked for death earlier, it removes
	itself (actually its pane) from the gDeadFormElementsList.
	
	The key thing to remember with the mark for death is that it is
	actually the element's pane that gets kiled.  If the LPane and LFormElement
	are different objects, the pane is responsible for killing the form element,
	which then takes the pane out of the dead list (it's all very sick).
*/
LFormElement::~LFormElement()
{
	if (this == sTargetedFormElement)
		sTargetedFormElement = NULL;
		
	if (fTimer != 0)
	{
		FE_ClearTimeout(fTimer);
		fTimer = 0;
	}
	
	if (fMarkedForDeath)
		gDeadFormElementsList->Remove(&fPane);
	
	// Since all of our form element pane classes mixin this class,
	// we should set fPane and fHost in fFEData to NULL
	if (fFEData)
	{
		if (fFEData->fPane == fPane)
			fFEData->fPane = NULL;
	
		if (fFEData->fHost == this)
			fFEData->fHost = NULL;
	}
}




/*
	FE_RaiseWindow
	
	Moves window belonging to inContext to the front
	of its layer within the window list.
	
	pkc (2/13/97) -- if there is no window associated with
	context, then we need to highlight frame.
*/
void FE_RaiseWindow(MWContext* inContext)
{
	CBrowserContext* theNSContext = ExtractBrowserContext(inContext);
	Assert_(theNSContext != NULL);

	CBrowserWindow* theTargetWindow = NULL;
	CMediatedWindow* theIterWindow = NULL;
	CWindowIterator theWindowIterator(WindowType_Browser);
	while (theWindowIterator.Next(theIterWindow))
		{
		CBrowserWindow* theBrowserWindow = dynamic_cast<CBrowserWindow*>(theIterWindow);
		if (theBrowserWindow->GetWindowContext() == theNSContext)
			{
			theTargetWindow = theBrowserWindow;
			break;
			}
		}
		
	if (theTargetWindow != NULL)
		theTargetWindow->Select();
	else if (theNSContext->IsGridChild())
	{
		// context associated with a frame, call SwitchTarget on
		// CHTMLView in context to focus frame
		CHTMLView* theView = ExtractHyperView(inContext);
		if (theView)
			LCommander::SwitchTarget(theView);
	}
}


/*
	FE_LowerWindow
	
	Moves window belonging to inContext to the back
	of its layer within the window list.
*/
void
FE_LowerWindow( MWContext*	inContext)
{
	CBrowserContext* theNSContext = ExtractBrowserContext(inContext);
	Assert_(theNSContext != NULL);

	// window.blur() in a frame context should blur the window.
	CBrowserWindow* theTargetWindow = CBrowserWindow::WindowForContext(theNSContext);
		
	if (theTargetWindow != NULL)
	{
		if (theTargetWindow->IsZLocked()) return;
		// context is associated with a window
		LWindow* theBehind = NULL;
		if (theTargetWindow->HasAttribute(windAttr_Floating))
					theBehind = UDesktop::FetchBottomFloater();
		else if (theTargetWindow->HasAttribute(windAttr_Modal))
					theBehind = UDesktop::FetchBottomModal();
		else theBehind = CWindowMediator::GetWindowMediator()->FetchBottomWindow(false);
		
		if (theBehind != theTargetWindow)
		{
			if (UDesktop::FetchTopRegular() == theTargetWindow)
				theTargetWindow->Deactivate();
			::SendBehind(theTargetWindow->GetMacPort(), (theBehind != NULL) ? theBehind->GetMacPort() : NULL);
			// The following hack is based on UDesktop::Resume() and fixes a case where the
			// window just sent to the back would be activated, when there is a floating
			// window present.
			LWindow* theTopWindow = UDesktop::FetchTopRegular();
			if (theTopWindow != NULL)
			{
				theTopWindow->Activate();
				LMSetCurActivate(nil); 	// Suppress any Activate event
			}
		}
	}
}

// A word about events...

// We send a mouse down event to javascript via layers before any click processing 
// can occur. If layers/javascript lets us have the event, it arrives back at the
// front end and we resume click processing where it left off. The usual powerplant
// tracking mechanisms go into action, and when the mouse button is released,
// we send a mouse up event to javascript which takes a similar path. Only when
// the mouse up event comes back to us do we fire a click event.

// The reason why we use this model is to have some degree of platform consistency
// for event handling in a script:
// A script should be able to cancel mouse down and not get a click (but still
// get a mouse up. Alternatively, a script should be able to cancel mouse up and
// not get a click. The mac differs slightly from windows in that if you cancel a
// mouse down in the button, and the mouse up occurs outside the button, you won't
// get a mouse up.

// This can be summarized in the following diagram, where x means an event that
// was sent but cancelled, s means an event that was sent, and - means an event
// that was never sent.

// Mac				event order ->
//			down        up        click
//			x			s*			-         * if inside element
//
//			s			x			-
//
// Win
//
//			x			s			-
//
//			s			x			-

// Events may be synthesized in the backend; synthesizing a mouse down event
// on the mac will immediately trigger a mouse up event if the mouse is already
// up. It's not easy to rearchitect the front end so that we can leave a widget
// in a clicked state and explicitly release it later, because much of the
// powerplant control code for tracking the mouse is done modally.
									
// In an ideal xp world we would have total control over event sequencing, and
// be able to synthesize any events that a user could generate. The front ends
// haven't been architected this way so we have to compromise.

// NOTE: this behavior has been implemented for buttons, radio buttons, and
// checkboxes. Radio buttons and checkboxes send mouse up events which can be
// cancelled, but cancelling them just leaves the form element in the same state
// and prevents a click from being sent. The reason why we need to send a mouse
// up event for these toggle items is for xp consistency.

// 1997-03-06 mjc

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CLayerClickDispatchAttachment
//  Dispatches mouse down events to layers.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CLayerClickDispatchAttachment::CLayerClickDispatchAttachment()
	:	LAttachment(msg_Click, false)
{
}

void CLayerClickDispatchAttachment::ExecuteSelf(
	MessageT			inMessage,
	void*				ioParam)
{
	Assert_(inMessage == msg_Click);
	mOwningControl = dynamic_cast<LControl*>(GetOwnerHost());
	Assert_(mOwningControl != NULL);
	
	MouseDown(*(SMouseDownEvent*)ioParam);
}

// Dispatches a mouse down to layers. The event comes back to the front end through
// FE_HandleLayerEvent if javascript lets us have it.
void 
CLayerClickDispatchAttachment::MouseDown(const SMouseDownEvent& inMouseDown)
{
	// get the compositor from the form element which is in the usercon of the control

	LO_FormElementStruct* formElem = (LO_FormElementStruct*) mOwningControl->GetUserCon();
	
	CL_Compositor* compositor = CL_GetLayerCompositor(formElem->layer);

	// ALERT: ignore mouse down if part of a multi-click. We don't need to handle multi
	// clicks on a form element.
	if (mOwningControl->GetClickCount() > 1) return;
	
	if (compositor != NULL) 
	{
		CL_Event		event;
		fe_EventStruct	fe_event;
		
		//SPoint32		firstP;
		//Point			where = inMouseDown.wherePort;
		//mOwningControl->GetSuperView()->PortToLocalPoint(where);
		//mOwningControl->GetSuperView()->LocalToImagePoint(where, firstP);
		
		fe_event.event.mouseDownEvent = inMouseDown;
		event.type = CL_EVENT_MOUSE_BUTTON_DOWN;
		event.fe_event = (void *)&fe_event;
		event.fe_event_size = sizeof(fe_EventStruct);
		//event.x = firstP.h;
		//event.y = firstP.v;
		// Set the event coordinates to the button layer origin even though we have the
		// actual click coordinates, to be consistent with coordinates sent for mouse up
		// and click events on form buttons, when we don't have the click coordinates.
		event.x = CL_GetLayerXOrigin(formElem->layer);
		event.y = CL_GetLayerYOrigin(formElem->layer);
		event.which = 1; // which button is down (0 = none, 1 onwards starts from the left)
		event.modifiers = CMochaHacks::MochaModifiers(inMouseDown.macEvent.modifiers);
		event.data = 1; // number of clicks

		CL_DispatchEvent(compositor, &event);
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CLayerKeyPressDispatchAttachment
//  Dispatches key events to layers.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CLayerKeyPressDispatchAttachment::CLayerKeyPressDispatchAttachment()
	:	LAttachment(msg_KeyPress, false)
{
}

void CLayerKeyPressDispatchAttachment::ExecuteSelf(
	MessageT			inMessage,
	void*				ioParam)
{
	Assert_(inMessage == msg_KeyPress);
	mOwner = dynamic_cast<LFormElement*>(GetOwnerHost());
	Assert_(mOwner != NULL);
	
	KeyPress(*(EventRecord*)ioParam);
}

// Dispatches a key event over a form. The event comes back to the front end through
// FE_HandleLayerEvent if javascript lets us have it.
void 
CLayerKeyPressDispatchAttachment::KeyPress(const EventRecord& inKeyEvent)
{
	Boolean executeHost = true;
	LO_FormElementStruct* formElem = (LO_FormElementStruct*)mOwner->GetLayoutElement();
	Assert_(formElem != NULL);
	
	// To determine if we need to dispatch the key event to layers or not, check
	// the context and its parents to see if they have handlers for any key events,
	// and check if the form element has a key handler.
	
	if (formElem->event_handler_present ||
		HasKeyEventCapture(mOwner->GetContext()))
	{
		// In order to determine which element gets the key event when the event comes back to
		// the front end, the form's parent layer gets key focus, and we pass in the layer
		// coordinates (i.e. image coordinates). Layers hands to the FE the parent layer and 
		// an event whose x,y are now the coordinates of the element relative to that layer.
		//
		// The layers are organized in a tree:
		//
		//			   |---- _BACKGROUND
		//			   |	
		//	_DOCUMENT--|---- _BODY ------- _CONTENT
		//			   |	
		//			   |---- LAYER1 ----|----_BACKGROUND
		//                              |
		//                              |----_CONTENT
		//								|
		//								|----FORM ELEMENT #1 LAYER (cutout)
		
		CL_Compositor* compositor = CL_GetLayerCompositor(formElem->layer);
		CL_GrabKeyEvents(compositor, CL_GetLayerParent(formElem->layer));
		
		if (compositor != NULL &&
			!CL_IsKeyEventGrabber(compositor, NULL))
		{
			CL_Event		event;
			fe_EventStruct	fe_event;
			
			Char16		theChar = inKeyEvent.message & charCodeMask;
			
			event.data = 0;
			switch (inKeyEvent.what)
			{
				case keyDown:
					event.type = CL_EVENT_KEY_DOWN;
					break;
				case autoKey:
					event.type = CL_EVENT_KEY_DOWN;
					event.data = 1; // repeating
					break;
				case keyUp:
					event.type = CL_EVENT_KEY_UP;
					break;
			}
			fe_event.event.macEvent = inKeyEvent;
			event.fe_event = (void *)&fe_event;
			event.fe_event_size = sizeof(fe_EventStruct);
			// event x, y must be form location in document coordinates.
			event.x = CL_GetLayerXOrigin(formElem->layer);
			event.y = CL_GetLayerYOrigin(formElem->layer);
			event.which = theChar;
			event.modifiers = CMochaHacks::MochaModifiers(inKeyEvent.modifiers);

			CL_DispatchEvent(compositor, &event);
			executeHost = false;
		}
	}
	else
	{
		if (inKeyEvent.what != keyUp)
		{
			CHTMLView::SetLastFormKeyPressDispatchTime(inKeyEvent.when);
			executeHost = true;
		}
	}
	mExecuteHost = executeHost;
}
