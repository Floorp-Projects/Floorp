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

// medit.cpp : implementation file
//

#include "stdafx.h"

#include "medit.h"
#include "fmabstra.h"
#include "libevent.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#define EDIT_ID 25000

BEGIN_MESSAGE_MAP(CNetscapeEdit, CGenericEdit)
	//{{AFX_MSG_MAP(CNetscapeEdit)
	ON_WM_CHAR()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_WM_GETDLGCODE()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

IMPLEMENT_DYNAMIC(CNetscapeEdit, CGenericEdit)


#ifndef _AFXDLL
#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode
#endif

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
CNetscapeEdit::CNetscapeEdit()
{
    m_Context = NULL;
    m_Form = NULL;
    m_Submit = FALSE;
    m_callBase = FALSE;
#ifdef DEBUG
    m_bOnCreateCalled = FALSE;
#endif
}

int CNetscapeEdit::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
#ifdef DEBUG
    m_bOnCreateCalled = TRUE;
#endif

	if (CGenericEdit::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	return 0;
}

//
// Make sure we have a valid context
// We might nto be in a form so don't barf if we don't have a form
//
BOOL CNetscapeEdit::SetContext(MWContext * context, LO_FormElementStruct * form)
{

	if(!context)
		return(FALSE);

	m_Context = context;
	m_Form    = form;

	return(TRUE);

}

LO_FormElementStruct *CNetscapeEdit::GetFormElement()
{
    return(m_Form);
}


UINT CNetscapeEdit::OnGetDlgCode()
{
    return CEdit::OnGetDlgCode() | DLGC_WANTARROWS | DLGC_WANTALLKEYS;
}

#ifdef XP_WIN16
//
// The creator of this form element should have created a segment for
//   us to live in before we got here.  Tell Windows to use that 
//   segment rather than the application's so that we don't run out of
//   DGROUP space
//
BOOL CNetscapeEdit::PreCreateWindow( CREATESTRUCT& cs )
{
    ASSERT(m_hInstance);

    if(!m_hInstance)
        return(TRUE);

    cs.hInstance = m_hInstance;
    return(TRUE);
}
#endif  

//
// Called when the 'submit' event has been processed
//
static void
edit_submit_closure(MWContext * pContext, LO_Element * pEle, int32 event,
                     void * pObj, ETEventStatus status)
{
    if(status != EVENT_OK || !pEle || !pContext)
        return;
    FE_SubmitInputElement(pContext, pEle);
}

//
// If the user has hit return load the url
// Otherwise pass the key on to the base library
//
void CNetscapeEdit::OnChar(UINT nChar, UINT nCnt, UINT nFlags)
{
    BOOL bJsWantsEvent = m_Form->event_handler_present ||
	LM_EventCaptureCheck(m_Context, EVENT_KEYDOWN | EVENT_KEYUP | EVENT_KEYPRESS);

    // Check if we generated this event.  If so, the base.  If not, call JS
    if (m_callBase || !bJsWantsEvent) {

	if(nChar == '\r') {	   // the user hit return

#ifdef DEBUG
        if(!m_bOnCreateCalled) {
            TRACE("Bug 88020 still present\n");
#if defined (DEBUG_blythe) || defined(DEBUG_ftang) || defined(DEBUG_bstell) || defined(DEBUG_nhotta) || defined(DEBUG_jliu)
            ASSERT(0);
#endif
        }
#endif

	    if(m_Submit) { // if we are the only element, return == submit

		// if the text is different throw out the old text and add the
		//   new text into the layout structure
		UpdateAndCheckForChange();

		// we are about to submit so tell libmocha about it first
		//   do the submit in our closure if libmocha says its OK
		JSEvent *event;
		event = XP_NEW_ZAP(JSEvent);
		event->type = EVENT_SUBMIT;
		event->layer_id = m_Form->layer_id;

		ET_SendEvent(m_Context, (LO_Element *)m_Form, event, 
			     edit_submit_closure, this);

		return;
	    } 
	    else {

		// we aren't auto-submitting, but the user still hit return
		//   they prolly mean that we should check to see if we should
		//   send a change event
		if(!(GetStyle() & ES_MULTILINE))
		    UpdateAndCheckForChange();

	    }

        } // wasn't the return key
	
//#ifdef  NO_TAB_NAVIGATION 
/*
	// let the CGenericView to handle the Tab, to step through Form elements
	// and links, even links inside a form.
	if(nChar == VK_TAB) {
        // if the high order bit is set (i.e. its negative) the shift 
        //   key is being held down
        BOOL bShift = GetKeyState(VK_SHIFT) < 0 ? TRUE : FALSE;

        //  Determine our form element.
        if(m_Context)   {
            CFormElement *pMe = CFormElement::GetFormElement(ABSTRACTCX(m_Context), m_Form);
            if(pMe) {
                //  We're handling tabbing between form elements.
                pMe->Tab(bShift);
                return;
            }
        }
	}                                              
	CEdit::OnChar(nChar, nCnt, nFlags);
*/
//#endif	/* NO_TAB_NAVIGATION */

	CEdit::OnChar(nChar, nCnt, nFlags);
	//If m_callBase is set, this event is a callback from a JS event and
	//we have to update the value of the edit field to reflect new values.
	if (bJsWantsEvent) {
	    UpdateEditField();
	}
	return;
    }

    // Give the event to JS.  They'll call the base when they get back.
    //
    // Grab key events for this layer's parent.
    CL_GrabKeyEvents(m_Context->compositor, CL_GetLayerParent(m_Form->layer));

    /* 
     * If there's a compositor and someone has keyboard focus.
     * Note that if noone has event focus, we don't really need
     * to dispatch the event.
     */
    if (m_Context->compositor && 
        !CL_IsKeyEventGrabber(m_Context->compositor, NULL)) {
        CL_Event event;
        fe_EventStruct fe_event;

	fe_event.fe_modifiers = nCnt;	    
	fe_event.nChar = nChar;	    
	fe_event.uFlags = nFlags;
	fe_event.x = 1;

        event.type = CL_EVENT_KEY_DOWN;
        event.which = nChar;
	event.fe_event = (void *)&fe_event;
	event.fe_event_size = sizeof(fe_EventStruct);
        event.modifiers = (GetKeyState(VK_SHIFT) < 0 ? EVENT_SHIFT_MASK : 0) 
			| (GetKeyState(VK_CONTROL) < 0 ? EVENT_CONTROL_MASK : 0) 
			| (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
        event.x = CL_GetLayerXOrigin(m_Form->layer);
        event.y = CL_GetLayerYOrigin(m_Form->layer);
	event.data = nFlags>>14 & 1;//Bit represeting key repetition

        CL_DispatchEvent(m_Context->compositor, &event);
    }

}

//
// We just got focus --- maybe libmocha cares
//
void CNetscapeEdit::OnSetFocus(CWnd * pWnd)
{
    CEdit::OnSetFocus(pWnd);
    if(m_Context && m_Form) {

        // set tab_focus to this element
        CWinCX *pWinCX = WINCX(m_Context);

//	if ((LO_Element *)m_Form == pWinCX->getLastTabFocusElement())
//	    return;

        pWinCX->setFormElementTabFocus( (LO_Element *)m_Form );
	CL_GrabKeyEvents(m_Context->compositor, CL_GetLayerParent(m_Form->layer));

        // send the event to libmocha --- do any further processing
        //   in our closure routine
	JSEvent *event;
	event = XP_NEW_ZAP(JSEvent);
	event->type = EVENT_FOCUS;
	event->layer_id = m_Form->layer_id;

        ET_SendEvent(m_Context, (LO_Element *)m_Form, event, 
                     NULL, this);
    }
}


lo_FormElementTextData *CNetscapeEdit::GetTextData()	{
	lo_FormElementTextData *pRetval = NULL;

	if(m_Form && m_Form->element_data)	{
		switch(m_Form->element_data->type)	{
			case FORM_TYPE_READONLY:
				break;
			default:
				pRetval = &(m_Form->element_data->ele_text);
				break;
		}
	}

	return(pRetval);
}


//
// We just lost focus --- maybe libmocha cares
//
// If the current value is different from the value that we have previously
//   stored call SendOnChange() for this element before calling BlurElement();
//
void CNetscapeEdit::OnKillFocus(CWnd * pWnd)
{
	// Call base class and be done with the message processing first
    CEdit::OnKillFocus(pWnd);
	
    // if the text is different throw out the old text and add the
    //   new text into the layout structure
    UpdateAndCheckForChange();

    // Send the blur message.
    JSEvent *event;
    event = XP_NEW_ZAP(JSEvent);
    event->type = EVENT_BLUR;
    event->layer_id = m_Form->layer_id;

    ET_SendEvent(m_Context, (LO_Element *) m_Form, event, 
                 NULL, this);

}

void CNetscapeEdit::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if(m_Form->event_handler_present ||
	    LM_EventCaptureCheck(m_Context, EVENT_KEYDOWN | EVENT_KEYUP | EVENT_KEYPRESS)) {
	UpdateEditField();
    }
    
    CEdit::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CNetscapeEdit::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    BOOL bJsWantsEvent = m_Form->event_handler_present ||
	LM_EventCaptureCheck(m_Context, EVENT_KEYDOWN | EVENT_KEYUP | EVENT_KEYPRESS);

    // Check if we generated this event.  If so, the base.  If not, call JS
    if (m_callBase || !bJsWantsEvent) {
        CEdit::OnKeyUp(nChar, nRepCnt, nFlags);
	return;
    }

    //	Don't continue if this context is destroyed.
    if(!m_Context || ABSTRACTCX(m_Context)->IsDestroyed())	{
	    return;
    }
    
    // Grab key events for this layer's parent.
    CL_GrabKeyEvents(m_Context->compositor, CL_GetLayerParent(m_Form->layer));

    /* 
     * If there's a compositor and someone has keyboard focus.
     * Note that if noone has event focus, we don't really need
     * to dispatch the event.
     */
    if (m_Context->compositor && 
        !CL_IsKeyEventGrabber(m_Context->compositor, NULL)) {
        CL_Event event;
        fe_EventStruct fe_event;
	BYTE kbstate[256];
	WORD asciiChar = 0;
	 
	GetKeyboardState(kbstate);
#ifdef WIN32	
	ToAscii(nChar, nFlags & 0xff, kbstate, &asciiChar, 0);
#else
	ToAscii(nChar, nFlags & 0xff, kbstate, (DWORD*)&asciiChar, 0);
#endif

	fe_event.fe_modifiers = nRepCnt;	    
	fe_event.nChar = nChar;	    
	fe_event.uFlags = nFlags;
	fe_event.x = 1;
        
        event.type = CL_EVENT_KEY_UP;
	event.fe_event = (void *)&fe_event;
	event.fe_event_size = sizeof(fe_EventStruct);
        event.which = asciiChar;
        event.modifiers = (GetKeyState(VK_SHIFT) < 0 ? EVENT_SHIFT_MASK : 0) 
			| (GetKeyState(VK_CONTROL) < 0 ? EVENT_CONTROL_MASK : 0) 
			| (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
        event.x = CL_GetLayerXOrigin(m_Form->layer);
        event.y = CL_GetLayerYOrigin(m_Form->layer);
        
        CL_DispatchEvent(m_Context->compositor, &event);
    }
}

// This function returns true if it has to update the value of the field 
// and false if it does not.
XP_Bool CNetscapeEdit::UpdateEditField()
{
    // find out what the current text is
    CString csText;
    GetWindowText(csText);

    // if the text is different throw out the old text and add the
    //   new text into the layout structure
    lo_FormElementTextData * textData = GetTextData();

    if(!textData)
	return FALSE;
    
    //
    // This if() statement has two parts.  If there used to not be
    //   any text and there is some now we will send a change message
    //   Or, if there already was text but it was different we also
    //   want to send a change event
    if((!textData->current_text && !csText.IsEmpty()) ||
       (textData->current_text && csText != (char *) textData->current_text))
    {
	    LO_UpdateTextData(textData, (const char *)csText);
	    return TRUE;
    }

    return FALSE;
}

//
// See if our value has changed since the last time we checked.
//   If it has, send a change event to Mocha
//
void CNetscapeEdit::UpdateAndCheckForChange()
{

    if(UpdateEditField())
    {
	JSEvent *event;
	event = XP_NEW_ZAP(JSEvent);
	event->type = EVENT_CHANGE;
	event->layer_id = m_Form->layer_id;

	ET_SendEvent(m_Context, (LO_Element *) m_Form, event, 
		     NULL, this);
    }
}

void CNetscapeEdit::OnEditKeyEvent(CL_EventType type, UINT nChar, UINT nRepCnt, UINT nFlags) {
    m_callBase = TRUE;
    if (type == CL_EVENT_KEY_DOWN)
	SendMessage(WM_CHAR, nChar, MAKELONG(nRepCnt, nFlags));
    else if (type == CL_EVENT_KEY_UP)
	SendMessage(WM_KEYUP, nChar, MAKELONG(nRepCnt, nFlags));
    m_callBase = FALSE;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CMochaListBox, CListBox)
	//{{AFX_MSG_MAP(CMochaListBox)
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_CHAR()
#if defined(MSVC4)
	ON_CONTROL_REFLECT(LBN_SELCHANGE, OnSelChange)
#endif	// MSVC4
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CMochaListBox::SetContext(MWContext * context, LO_FormElementStruct * form)
{
	m_Context = context;
	m_Form    = form;
}

void CMochaListBox::OnSetFocus(CWnd * pWnd)
{
    CListBox::OnSetFocus(pWnd); 

    // set tab_focus to this element
    CWinCX *pWinCX = WINCX(m_Context);
//    if ((LO_Element *)m_Form == pWinCX->getLastTabFocusElement())
//	return;
    pWinCX->setFormElementTabFocus( (LO_Element *)m_Form );
    CL_GrabKeyEvents(m_Context->compositor, CL_GetLayerParent(m_Form->layer));

    JSEvent *event;
    event = XP_NEW_ZAP(JSEvent);
    event->type = EVENT_FOCUS;
    event->layer_id = m_Form->layer_id;

    ET_SendEvent(m_Context, (LO_Element *) m_Form, event, 
                 NULL, this);
}

//
// If the current value is different from the value that we have previously
//   stored call SendOnChange() for this element before calling BlurElement();
//
void CMochaListBox::CheckForChange()
{

    // get the base form element
    lo_FormElementSelectData     * pSelectData     = NULL;
    pSelectData = (lo_FormElementSelectData *) m_Form->element_data;
    if(!pSelectData)
        return;

    // get the option state
    lo_FormElementOptionData     * pOptionData     = NULL;
    pOptionData = (lo_FormElementOptionData *) pSelectData->options;
    if(!pOptionData)
        return;

    BOOL bChanged = FALSE;
    for(int i = 0; i < pSelectData->option_cnt; i++) {

        int iIsSelected = GetSel(i);
        
        // see if we have found a selection which is not the current
        //   selection
        if(pOptionData[i].selected && (iIsSelected < 1)) {
            pOptionData[i].selected = FALSE;
            bChanged = TRUE;
        }

        // see if item #i has gone from being unselected to being selected
        if(!pOptionData[i].selected && (iIsSelected > 0)) {
            pOptionData[i].selected = TRUE;
            bChanged = TRUE;
        }

    }

    // only send change message if things have changed
    if(bChanged) {
	JSEvent *event;
	event = XP_NEW_ZAP(JSEvent);
	event->type = EVENT_CHANGE;
    event->layer_id = m_Form->layer_id;

        ET_SendEvent(m_Context, (LO_Element *) m_Form, event,
                 NULL, this);
    }

}

//
// We just lost focus --- maybe libmocha cares
//
void CMochaListBox::OnKillFocus(CWnd * pWnd)
{
    // let MFC do its thing
    CListBox::OnKillFocus(pWnd);
	
    // see if we have changed and fire a change event if necessary
    CheckForChange();

    // we have lost focus so whether the value changed or not we 
    //   should tell the mocha library
    JSEvent *event;
    event = XP_NEW_ZAP(JSEvent);
    event->type = EVENT_BLUR;
    event->layer_id = m_Form->layer_id;

    ET_SendEvent(m_Context, (LO_Element *) m_Form, event, NULL, this);
}

#if defined(MSVC4)
void CMochaListBox::OnSelChange() 
{
	// The OnKillFocus() handler takes care of leaving a control, but Mocha
	// also wants to know about a selection change in a combobox or listbox.
    CheckForChange();
}
#else
// In Win16 and MFC prior to 4.0, only the parent of a control could get
// notification messages.
BOOL CMochaListBox::OnChildNotify(UINT nMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	if (nMsg == WM_COMMAND) {
		if (GET_WM_COMMAND_CMD(wParam, lParam) == LBN_SELCHANGE) {
			CheckForChange();
			return TRUE;
		}
	}

	return CListBox::OnChildNotify(nMsg, wParam, lParam, pResult);
}
#endif


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CMochaComboBox, CComboBox)
	//{{AFX_MSG_MAP(CMochaComboBox)
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_CHAR()
#if defined(MSVC4)
	ON_CONTROL_REFLECT(CBN_SELCHANGE, OnSelChange)
#endif	// MSVC4
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CMochaComboBox::SetContext(MWContext * context, LO_FormElementStruct * form)
{
	m_Context = context;
	m_Form    = form;
}

void CMochaComboBox::OnSetFocus(CWnd * pWnd)
{
    CComboBox::OnSetFocus(pWnd); 

    // set tab_focus to this element
    CWinCX *pWinCX = WINCX(m_Context);
//    if ((LO_Element *)m_Form == pWinCX->getLastTabFocusElement())
//	return;
    pWinCX->setFormElementTabFocus( (LO_Element *)m_Form );
    CL_GrabKeyEvents(m_Context->compositor, CL_GetLayerParent(m_Form->layer));

    JSEvent *event;
    event = XP_NEW_ZAP(JSEvent);
    event->type = EVENT_FOCUS;
    event->layer_id = m_Form->layer_id;

    ET_SendEvent(m_Context, (LO_Element *) m_Form, event, NULL, this);
}

//
// If the current value is different from the value that we have previously
//   stored call SendOnChange() for this element before calling BlurElement();
//
void CMochaComboBox::CheckForChange()
{

    // get the base form element
    lo_FormElementSelectData     * pSelectData     = NULL;
    pSelectData = (lo_FormElementSelectData *) m_Form->element_data;
    if(!pSelectData)
        return;

    // get the option state
    lo_FormElementOptionData     * pOptionData     = NULL;
    pOptionData = (lo_FormElementOptionData *) pSelectData->options;
    if(!pOptionData) 
        return;

    // index of single selection
    int iCurSelection = GetCurSel();  
    if(iCurSelection == LB_ERR) 
        return;

    BOOL bChanged = FALSE;
    for(int i = 0; i < pSelectData->option_cnt; i++) {
        
        // see if we have found a selection which is not the current
        //   selection
        if(pOptionData[i].selected && i != iCurSelection) {
            pOptionData[i].selected = FALSE;
            bChanged = TRUE;
        }

        // see if item #i has gone from being unselected to being selected
        if(!pOptionData[i].selected && i == iCurSelection) {
            pOptionData[i].selected = TRUE;
            bChanged = TRUE;
        }

    }

    // only send change message if things have changed
    if(bChanged) {
	JSEvent *event;
	event = XP_NEW_ZAP(JSEvent);
	event->type = EVENT_CHANGE;
    event->layer_id = m_Form->layer_id;

        ET_SendEvent(m_Context, (LO_Element *) m_Form, event, 
                     NULL, this);
    }

}

//
// We just lost focus --- maybe libmocha cares
//
void CMochaComboBox::OnKillFocus(CWnd * pWnd)
{
    CComboBox::OnKillFocus(pWnd);
	
    CheckForChange();

    // we have lost focus so whether the value changed or not we should tell the
    //   mocha library
    JSEvent *event;
    event = XP_NEW_ZAP(JSEvent);
    event->type = EVENT_BLUR;
    event->layer_id = m_Form->layer_id;

    ET_SendEvent(m_Context, (LO_Element *) m_Form, event, NULL, this);
}

#if defined(MSVC4)
void CMochaComboBox::OnSelChange() 
{
	// The OnKillFocus() handler takes care of leaving a control, but Mocha
	// also wants to know about a selection change in a combobox or listbox.
    CheckForChange();
}
#else
// In Win16 and MFC prior to 4.0, only the parent of a control could get
// notification messages.
BOOL CMochaComboBox::OnChildNotify(UINT nMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	if (nMsg == WM_COMMAND) {
		if (GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE) {
			CheckForChange();
			return TRUE;
		}
	}

	return CComboBox::OnChildNotify(nMsg, wParam, lParam, pResult);
}
#endif	// MSVC4


BOOL FirstCharMatch( char * searchFor, lo_FormElementSelectData  *pSelectData, int oldSel, int *newSel  )
{
	char	*pCurrent = NULL;
	int		ii;
	
	if( searchFor == NULL ) 
		return( FALSE);


    // get the option state
    lo_FormElementOptionData     * pOptionData     = NULL;
     pOptionData = (lo_FormElementOptionData *) pSelectData->options;
    if(!pOptionData) 
        return(FALSE);

    // index of single selection
    BOOL bChanged = FALSE;
	ii = oldSel + 1;		// start with current selection.
	if( ii == pSelectData->option_cnt )
		ii = 0;				// fold back to the begining of the list.
    while( ! bChanged && ii != oldSel ) {
        
		pCurrent = (char *)pOptionData[ii].text_value;
		if( pCurrent && (toupper(*pCurrent) == toupper(* searchFor)) ) {   // todo multi byte
			bChanged = TRUE;
			*newSel = ii;
		    break;				// ii is the index to select.
        }

		ii++;
		if( ii == pSelectData->option_cnt )
			ii = 0;				// fold back to the begining of the list.
    }

	return( bChanged );
}

void CMochaComboBox::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	CComboBox::OnChar(nChar, nRepCnt, nFlags);  // call base class 

	// refer to void CFormSelectOne::CreateWidget() in fmdelone.cpp
	// combo boxes are created with CBS_OWNERDRAWFIXED, for i18n support.
	// First-char-searching in list doesn't work any more.
	// Need to do it here.
	
	int		newSel;
	char  searchFor[2];
	searchFor[0] = nChar;
	searchFor[1] = '\0';

    // get the base form element
    lo_FormElementSelectData     * pSelectData     = NULL;
    pSelectData = (lo_FormElementSelectData *) m_Form->element_data;
    if(!pSelectData)
        return;

    int iCurSelection = GetCurSel();  
    if(iCurSelection == LB_ERR) 
        return;

	BOOL changed = FirstCharMatch( searchFor, pSelectData, iCurSelection, &newSel );
	if( changed )
		CComboBox::SetCurSel( newSel );

}

void CMochaListBox::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	
	CListBox::OnChar(nChar, nRepCnt, nFlags);
	int		newSel;
	char  searchFor[2];
	searchFor[0] = nChar;
	searchFor[1] = '\0';

    // get the base form element
    lo_FormElementSelectData     * pSelectData     = NULL;
    pSelectData = (lo_FormElementSelectData *) m_Form->element_data;
    if(!pSelectData)
        return;

    int iCurSelection = GetCurSel();  
    if(iCurSelection == LB_ERR) 
        return;

	BOOL changed = FirstCharMatch( searchFor, pSelectData, iCurSelection, &newSel );
	if( changed ) {
		CListBox::SetSel( -1, FALSE );	// clear all selections.
		CListBox::SetSel( newSel );
	}

}
