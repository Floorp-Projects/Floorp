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
// button.cpp : implementation file
//
#include "stdafx.h"
#include "fmabstra.h"

#include "button.h"

#include "libevent.h"
#include <windowsx.h>

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#ifndef _AFXDLL
#define new DEBUG_NEW  // MSVC Debugging new...goes to regular new in release mode
#endif

#ifndef _WIN32
#define GET_WM_COMMAND_CMD(wp, lp)	((UINT)HIWORD(lp))
#endif

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CNetscapeButton dialog

IMPLEMENT_DYNAMIC(CNetscapeButton, CButton)                                     

CNetscapeButton::CNetscapeButton(MWContext * context, LO_FormElementStruct * form, CWnd* pParent)
{
    m_Context = context;
    m_Form = form;
    m_bDepressed = FALSE;
    m_pwndEdit = NULL;
    m_pPaneCX = PANECX(m_Context);
    m_callBase = FALSE;
}

BEGIN_MESSAGE_MAP(CNetscapeButton, CButton)
    ON_WM_CHAR()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDBLCLK()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNetscapeButton message handlers
/////////////////////////////////////////////////////////////////////////////

//
// Called when the 'submit' event has been processed
//
static void
button_submit_closure(MWContext * pContext, LO_Element * pEle, int32 event,
                     void * pObj, ETEventStatus status)
{
    if(status != EVENT_OK || !pEle || !pContext)
        return;
    FE_SubmitInputElement(pContext, pEle);
}

//
// Called when the 'click' event has been processed
//
static void
button_click_closure(MWContext * pContext, LO_Element * pEle, int32 event,
                     void * pObj, ETEventStatus status)
{

    // make sure every thing is OK
    if(status == EVENT_PANIC || !pEle || !pContext)
        return;

    CNetscapeButton      * pButton = (CNetscapeButton *) pObj;
    LO_FormElementStruct * pForm   = (LO_FormElementStruct *) pEle;

	lo_FormElementMinimalData * min_data;
	lo_FormElementToggleData  * tog_data;
	min_data = (lo_FormElementMinimalData *) pForm->element_data;

    if(!min_data)
		return;

    if(status == EVENT_OK) {
        switch(min_data->type) {
            case FORM_TYPE_SUBMIT:
                // send the event to libmocha --- do any further processing
                //   in our closure routine
		JSEvent *event;
		event = XP_NEW_ZAP(JSEvent);
		event->type = EVENT_SUBMIT;
		event->layer_id = pForm->layer_id;
                
		ET_SendEvent(pContext, pEle, event, 
                             button_submit_closure, pObj);
                break;

            case FORM_TYPE_RESET:
				// libmocha has said it was OK to allow the reset to happen
				//   tell layout to go forward with the operation
                // the world may not be safe after this so return
		        LO_ResetForm(pContext, pForm);
                return;

            default:
				break;
    	}
    }
    else    {
        //  Undo state operations.
        switch(min_data->type) {
            case FORM_TYPE_RADIO:
                if(pButton->m_pLastSelectedRadio)  {
                    tog_data = (lo_FormElementToggleData *)
                        pButton->m_pLastSelectedRadio->element_data;
                    if(!tog_data->toggled)
                        LO_FormRadioSet(ABSTRACTCX(pContext)->GetDocumentContext(), 
                                    pButton->m_pLastSelectedRadio);

                    CFormElement *pRadioButton = CFormElement::GetFormElement(ABSTRACTCX(pContext), 
                                                   pButton->m_pLastSelectedRadio);
                    if(pRadioButton)    {
                        pRadioButton->SetFormElementToggle(tog_data->toggled);
                    }
                }
                break;
            case FORM_TYPE_CHECKBOX: 
                tog_data = (lo_FormElementToggleData *) pForm->element_data;
                if(tog_data->toggled)
                    tog_data->toggled = FALSE;
                else 
                    tog_data->toggled = TRUE;

                if((pButton->GetCheck() ? TRUE : FALSE) != tog_data->toggled)
                    pButton->SetCheck(tog_data->toggled);

                break;
            default:
                break;
        }
    }

}


//  NFlags as per OnLButtonUp
//
//  
//  for FORM_TYPE_CHECKBOX, there are 3 places in this file call Click().
//  1. OnChar           for space-key
//  2. OnButtonEvent,   initiated by java script?
//  3. OnChildNotify    maybe not necesary to handle this msg.
// 
//  We taggled state for 1 and 2.We do NOT taggle for 3. It always comes 
//  with 1 or 2. It is not clear whether item 3 is redundent, so it is 
//  not removed.
//
//  Click() function is still called for all 3 cases. 
//  
//  Messages interfare with debug, Trace statements are left for debuging.
//
//  When change this code, please make sure radio-button and check-box 
//  work for both Tab-Space keys and mouse click(with out debug).
//  
//  Arthur 3/12/97, fix for #49682
// 
void CNetscapeButton::Click(UINT nFlags, BOOL bNotify, BOOL &bReturnImmediately)  
{
	if(!m_Form || !m_Context)
		return;

	lo_FormElementMinimalData * min_data;
	lo_FormElementToggleData  * tog_data;
	min_data = (lo_FormElementMinimalData *) m_Form->element_data;
	char * pFileName;
	if(!min_data)
		return;

	m_pLastSelectedRadio = NULL;
	switch(min_data->type) {

	case FORM_TYPE_RADIO:
		tog_data = (lo_FormElementToggleData *) m_Form->element_data;
		if(!tog_data->toggled) {
#ifdef MOZ_NGLAYOUT
      XP_ASSERT(0);
#else
			m_pLastSelectedRadio = LO_FormRadioSet(ABSTRACTCX(m_Context)->GetDocumentContext(), m_Form);  // Turn off everyone else
#endif
		}
        if(GetCheck() != tog_data->toggled)   {
            SetCheck(tog_data->toggled);
#ifdef DEBUG_aliu
			TRACE0("Set\n");
#endif  // DEBUG_aliu
        }
		break;

	case FORM_TYPE_CHECKBOX: 
        //  Make sure we don't already reflect that state, and don't do this
        //      if via OnChar, as MFC/Windows will do it for us.
        if(bNotify == FALSE)    {
			tog_data = (lo_FormElementToggleData *) m_Form->element_data;
			if(tog_data->toggled) {
#ifdef DEBUG_aliu
				TRACE0("Off ");
#endif  // DEBUG_aliu
				tog_data->toggled = FALSE;
			} else {
				tog_data->toggled = TRUE;
#ifdef DEBUG_aliu
				TRACE0("On ");
#endif  // DEBUG_aliu
			}

             if((GetCheck() ? TRUE : FALSE) != tog_data->toggled)  {
#ifdef DEBUG_aliu
				TRACE0("Set\n");
#endif  // DEBUG_aliu
                SetCheck(tog_data->toggled);
             }
        }
		break;

	case FORM_TYPE_BUTTON:
		break;

    case FORM_TYPE_RESET: 
    case FORM_TYPE_SUBMIT:
        // don't do anything yet --- send the OnClick to libmocha first
	// Sigh don't make double submits
	if (bNotify == TRUE) return;
        break; 

    case FORM_TYPE_FILE:
        pFileName = wfe_GetExistingFileName(m_hWnd, 
					    szLoadString(IDS_FILE_UPLOAD), 
					    HTM, 
					    TRUE);

        /* update both the widget display and the lo-datastructure */
	if(pFileName && m_pwndEdit) {
            m_pwndEdit->SetWindowText(pFileName);

	    if (m_Form->element_data->ele_text.current_text)
		XP_FREE(m_Form->element_data->ele_text.current_text);

	    m_Form->element_data->ele_text.current_text = (PA_Block) pFileName;
	}

        break;

    default:
        break;

    }

    // send the event to libmocha --- do any further processing
    //   in our closure routine
    JSEvent *event;
    event = XP_NEW_ZAP(JSEvent);
    event->type = EVENT_CLICK;
    event->layer_id = m_Form->layer_id;
    event->which = 1;
    event->modifiers = (nFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
		    | (nFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
		    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 

    ET_SendEvent(m_Context, (LO_Element *)m_Form, event, 
                 button_click_closure, this);

}

void CNetscapeButton::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	// CButton::OnChar( nChar,  nRepCnt, nFlags);
    if(( nChar == VK_SPACE  || nChar == VK_RETURN) && !(nFlags>>14 & 1))  {
        //  buttons need a push.
        BOOL bReturnImmediately = FALSE;
#ifdef DEBUG_aliu
		TRACE0("onCharClick ");
#endif  // DEBUG_aliu
        Click(0, FALSE, bReturnImmediately);   //#49682 TRUE
        if(bReturnImmediately)  {
            return;
        }
    }
}

void CNetscapeButton::OnSetFocus(CWnd * pWnd)
{
    CButton::OnSetFocus(pWnd);		//must have this for fixing #51086, ref. #49682 
    if(m_Context && m_Form) {
        // set tab focus to this element
        CWinCX *pWinCX = WINCX(m_Context);
//	if ((LO_Element *)m_Form == pWinCX->getLastTabFocusElement())
//	    return;

        pWinCX->setFormElementTabFocus( (LO_Element *)m_Form );

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


void CNetscapeButton::OnKillFocus(CWnd * pWnd)
{
    CButton::OnKillFocus(pWnd);
    if(m_Context && m_Form) {
        // send the event to libmocha --- do any further processing
        //   in our closure routine
	JSEvent *event;
	event = XP_NEW_ZAP(JSEvent);
	event->type = EVENT_BLUR;
	event->layer_id = m_Form->layer_id;
        
	ET_SendEvent(m_Context, (LO_Element *)m_Form, event, NULL, this);
    }
}

void CNetscapeButton::OnLButtonDblClk(UINT uFlags, CPoint cpPoint)	{

    if (m_callBase) {
	CButton::OnLButtonDblClk(uFlags, cpPoint);
	return;
    }

    //	Don't continue if this context is destroyed.
    if(!m_Context || ABSTRACTCX(m_Context)->IsDestroyed())	{
	    return;
    }

    MapWindowPoints(GetParent(), &cpPoint, 1); 	

    //	Convert the point to something we understand.
    XY Point;
    m_pPaneCX->ResolvePoint(Point, cpPoint);

	if (m_Context->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;
	    
	    event.type = CL_EVENT_MOUSE_BUTTON_MULTI_CLICK;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.x = Point.x;
	    event.y = Point.y;
	    event.which = 1;
	    event.data = 2;
	    event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			    | (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	
	    CL_DispatchEvent(m_Context->compositor,
			     &event);
	}
	else 
	    CButton::OnLButtonDblClk(uFlags, cpPoint);

    return;
}

void CNetscapeButton::OnLButtonDown(UINT uFlags, CPoint cpPoint)	{

    if (m_callBase) {
        CButton::OnLButtonDown(uFlags, cpPoint);
	return;
    }

    //	Don't continue if this context is destroyed.
    if(!m_Context)	{
	    return;
    }

    //	Start capturing all mouse events.
    if(m_hWnd)	{
    ::SetCapture(m_hWnd);
    }

    MapWindowPoints(GetParent(), &cpPoint, 1); 	

    XY Point;
    m_pPaneCX->ResolvePoint(Point, cpPoint);

	if (m_Context->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;

	    event.type = CL_EVENT_MOUSE_BUTTON_DOWN;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.x = Point.x;
	    event.y = Point.y;
	    event.which = 1;
	    event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			    | (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	
	    CL_DispatchEvent(m_Context->compositor,
			     &event);
	}
	else 

	    CButton::OnLButtonDown(uFlags, cpPoint);

    return;

}
void CNetscapeButton::OnLButtonUp(UINT uFlags, CPoint cpPoint)	{

    if (m_callBase) {
	CButton::OnLButtonUp(uFlags, cpPoint);
        return;
    }

    //	Don't continue if this context is destroyed.
    if(!m_Context)	{
	    return;
    }

    //	Start capturing all mouse events.
    if(m_hWnd)	{
	    ::ReleaseCapture();
    }

    // translate mouse position to screen
    CPoint cpScreenPoint(cpPoint);
    ClientToScreen(&cpScreenPoint);

    //  Don't send the event on to JS if we're out of the button area.
    if (::WindowFromPoint(cpScreenPoint) != m_hWnd) {
	CButton::OnLButtonUp(uFlags, cpPoint);
	m_bDepressed = FALSE;
	return;
    }
    
    MapWindowPoints(GetParent(), &cpPoint, 1); 	

    XY Point;
    m_pPaneCX->ResolvePoint(Point, cpPoint);

	if (m_Context->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;

	    event.type = CL_EVENT_MOUSE_BUTTON_UP;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.x = Point.x;
	    event.y = Point.y;
	    event.which = 1;
	    event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			    | (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	
	    CL_DispatchEvent(m_Context->compositor,
			     &event);
	}
	else 
	    CButton::OnLButtonUp(uFlags, cpPoint);

    return;

}

void CNetscapeButton::OnRButtonDblClk(UINT uFlags, CPoint cpPoint)	{

    if (m_callBase) {
        CButton::OnRButtonDblClk(uFlags, cpPoint);
	return;
    }

    //	Don't continue if this context is destroyed.
    if(!m_Context)	{
	    return;
    }

    MapWindowPoints(GetParent(), &cpPoint, 1); 	

    XY Point;
    m_pPaneCX->ResolvePoint(Point, cpPoint);

	if (m_Context->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;
	    
	    event.type = CL_EVENT_MOUSE_BUTTON_MULTI_CLICK;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.x = Point.x;
	    event.y = Point.y;
	    event.which = 3;
	    event.data = 2;
	    event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			    | (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	
	    CL_DispatchEvent(m_Context->compositor,
			     &event);
	}
	else 
	    CButton::OnRButtonDblClk(uFlags, cpPoint);

    return;
}

void CNetscapeButton::OnRButtonDown(UINT uFlags, CPoint cpPoint)	{

    if (m_callBase) {
	CButton::OnRButtonDown(uFlags, cpPoint);
	return;
    }

    //	Don't continue if this context is destroyed.
    if(!m_Context)	{
	    return;
    }

    //	Start capturing all mouse events.
    if(m_hWnd)	{
    ::SetCapture(m_hWnd);
    }

    MapWindowPoints(GetParent(), &cpPoint, 1); 	

    XY Point;
    m_pPaneCX->ResolvePoint(Point, cpPoint);

	if (m_Context->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;

	    event.type = CL_EVENT_MOUSE_BUTTON_DOWN;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.x = Point.x;
	    event.y = Point.y;
	    event.which = 3;
	    event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			    | (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	
	    CL_DispatchEvent(m_Context->compositor,
			     &event);
	}
	else
	    CButton::OnRButtonDown(uFlags, cpPoint);

    return;
}
void CNetscapeButton::OnRButtonUp(UINT uFlags, CPoint cpPoint)	{

    if (m_callBase) {
	CButton::OnRButtonUp(uFlags, cpPoint);
	return;
    }

    //	Don't continue if this context is destroyed.
    if(!m_Context)	{
	    return;
    }

    //	Start capturing all mouse events.
    if(m_hWnd)	{
	    ::ReleaseCapture();
    }

    MapWindowPoints(GetParent(), &cpPoint, 1); 	

    XY Point;
    m_pPaneCX->ResolvePoint(Point, cpPoint);

	if (m_Context->compositor) {
	    CL_Event event;
	    fe_EventStruct fe_event;
	    
	    fe_event.uFlags = uFlags;
	    fe_event.x = cpPoint.x;
	    fe_event.y = cpPoint.y;
	    
	    event.type = CL_EVENT_MOUSE_BUTTON_UP;
	    event.fe_event = (void *)&fe_event;
	    event.fe_event_size = sizeof(fe_EventStruct);
	    event.x = Point.x;
	    event.y = Point.y;
	    event.which = 3;
	    event.modifiers = (uFlags & MK_SHIFT ? EVENT_SHIFT_MASK : 0) 
			    | (uFlags & MK_CONTROL ? EVENT_CONTROL_MASK : 0) 
			    | (GetKeyState(VK_MENU) < 0 ? EVENT_ALT_MASK : 0); 
	
	    CL_DispatchEvent(m_Context->compositor,
			     &event);
	}
	else 
	    CButton::OnRButtonUp(uFlags, cpPoint);

    return;
}

void
CNetscapeButton::OnButtonEvent(CL_EventType type, UINT nFlags, CPoint point) {
    BOOL bReturnImmediately;
    m_callBase = TRUE;

    switch (type) {
    case CL_EVENT_MOUSE_BUTTON_DOWN:
	SendMessage(WM_LBUTTONDOWN, nFlags, MAKELONG(point.x, point.y));
	// Hack since Windows stubbornly refuses to accept current state.
	SetState(FALSE);
	SetState(TRUE);
	m_bDepressed = TRUE;
	break;
    case CL_EVENT_MOUSE_BUTTON_UP:
	// Only send click event if button is already down.
        if (m_bDepressed) {
    	    SendMessage(WM_LBUTTONUP, nFlags, MAKELONG(point.x, point.y));
#ifdef DEBUG_aliu
	    TRACE0("ButtonEventClick ");
#endif  // DEBUG_aliu
	    Click(nFlags, FALSE, bReturnImmediately);
	    m_bDepressed = FALSE;
	}
	break;
    case CL_EVENT_MOUSE_BUTTON_MULTI_CLICK:
	SendMessage(WM_LBUTTONDBLCLK, nFlags, MAKELONG(point.x, point.y));
	break;
    }
    m_callBase = FALSE;
}



