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

#include "stdafx.h"

#include "fmabstra.h"
#include "fmbutton.h"
#include "fmfile.h"
#include "fmradio.h"
#include "fmselmul.h"
#include "fmselone.h"
#include "fmtext.h"
#include "fmtxarea.h"
#include "fmrdonly.h"

#include "windowsx.h"

//	Class to define common base functionality of all form elements
//		in the front end in an object oriented manner.

//	Static control ID unique for each instance of this class.
UINT CFormElement::m_uNextControlID = 0;

//	Function to create and/or retrieve a CFormElement from
//		a layout form struct.
CFormElement *CFormElement::GetFormElement(CAbstractCX *pCX, LO_FormElementStruct *pFormElement)
{
	CFormElement *pRetval = NULL;

	//	Ensure we have enough information with which to continue.
	if(pCX == NULL)	{
		return(pRetval);
	}
	else if(pFormElement == NULL)	{
		return(pRetval);
	}

	pRetval = GetFormElement(pCX, pFormElement->element_data);

	//	Look at the form element structure to see if one already exists.
	if(pRetval == NULL && pFormElement->element_data != NULL)	{
		//	We need to create a new form class.
		TRY	{
			switch(pFormElement->element_data->type)	{
				case FORM_TYPE_TEXT:
				case FORM_TYPE_PASSWORD:
					pRetval = (CFormElement *)new CFormText();
					break;
				case FORM_TYPE_CHECKBOX:
				case FORM_TYPE_RADIO:
					pRetval = (CFormElement *)new CFormRadio();
					break;
				case FORM_TYPE_BUTTON:
				case FORM_TYPE_RESET:
				case FORM_TYPE_SUBMIT:
					pRetval = (CFormElement *)new CFormButton();
					break;
				case FORM_TYPE_SELECT_ONE:
					pRetval = (CFormElement *)new CFormSelectOne();
					break;
				case FORM_TYPE_SELECT_MULT:
					pRetval = (CFormElement *)new CFormSelectMult();
					break;
				case FORM_TYPE_TEXTAREA:
					pRetval = (CFormElement *)new CFormTextarea();
					break;
				case FORM_TYPE_FILE:
					pRetval = (CFormElement *)new CFormFile();
					break;
				case FORM_TYPE_READONLY:
					pRetval = (CFormElement *)new CFormReadOnly();
					break;
				default:
					//	What exactly are we doing here?
					TRACE("Unimplemented form type (%d) requested.\n", (int)pFormElement->element_data->type);
					pRetval = NULL;
					break;
			}
		}
		CATCH(CException, e)	{
			pRetval = NULL;
		}
		END_CATCH
	}

	//	Set up and/or update the relevant information in the class.
	if(pRetval != NULL)	{
		pRetval->SetContext(pCX);
		pRetval->SetElement(pFormElement);
	}

	return(pRetval);
}

//	Only retrieve the form element class, do not create.
CFormElement *CFormElement::GetFormElement(CAbstractCX *pCX, LO_FormElementData *pFormData)
{
    CFormElement *pRetval = NULL;

    //	Ensure we have enough information with which to continue.
    if(pFormData == NULL) {
	    return(pRetval);
    }
    else if(pFormData->type == FORM_TYPE_KEYGEN)    {
        //  Doesn't have minimal data.
        return(pRetval);
    }

    //	Look at the form element structure to see if one already exists.
    if(pFormData->ele_minimal.FE_Data != NULL)	{
	    //	Already there, return it.
	    pRetval = (CFormElement *)pFormData->ele_minimal.FE_Data;
    }

    //	Set up and/or update the relevant information in the class.
    if(pRetval != NULL)	{
	    pRetval->SetContext(pCX);
    }

    return(pRetval);
}


//	Construction simply clears all members.
CFormElement::CFormElement()
{
	//	No LO form element.
	SetElement(NULL);

	//	No context.
	SetContext(NULL);

	//	By default, we have no widget allocated, and we are using default data.
	m_bUseCurrentData = FALSE;
	m_bWidgetPresent = FALSE;
}

//	Destruction cleans out all members.
CFormElement::~CFormElement()
{
    //	No LO form anymore.
    //	FreeFormElement must be used to delete the class.
    //	This assert knows that FreeFormElement calls SetElement(NULL).
    ASSERT(GetElement() == NULL);

	//	No context anymore.
	SetContext(NULL);
}

//	How to set the LO form element.
//	This may change during the lifetime of an instance, so use this to update all referencing values.
void CFormElement::SetElement(LO_FormElementStruct *pFormElement)
{
	//	Simply copy.
	m_pFormElement = pFormElement;
	
	//	Ensure its FE_Data points to us.
	if(GetElementMinimalData())	{
		GetElementMinimalData()->FE_Data = (void *)this;
	}
}

//	Return the owning LO element.
LO_FormElementStruct *CFormElement::GetElement() const
{
	return(m_pFormElement);
}

LO_FormElementData *CFormElement::GetElementData() const
{
	LO_FormElementData *pRetval = NULL;
	if(GetElement())	{
		pRetval = GetElement()->element_data;
	}

	return(pRetval);
}

//	Careful when using, may return incorrect structure if not correct
//		form type.
lo_FormElementTextData *CFormElement::GetElementTextData() const
{
	lo_FormElementTextData *pRetval = NULL;
	if(GetElementData())	{
		ASSERT(
			GetElementData()->type == FORM_TYPE_TEXT ||
			GetElementData()->type == FORM_TYPE_PASSWORD ||
			GetElementData()->type == FORM_TYPE_FILE
			);
		pRetval = &(GetElementData()->ele_text);
	}

	return(pRetval);
}

//	Careful when using, may return incorrect structure if not correct
//		form type.
lo_FormElementTextareaData *CFormElement::GetElementTextareaData() const
{
	lo_FormElementTextareaData *pRetval = NULL;
	if(GetElementData())	{
		ASSERT(
			GetElementData()->type == FORM_TYPE_TEXTAREA
			);
		pRetval = &(GetElementData()->ele_textarea);
	}

	return(pRetval);
}


//	Always safe to use.
lo_FormElementMinimalData *CFormElement::GetElementMinimalData() const
{
	lo_FormElementMinimalData *pRetval = NULL;
	if(GetElementData())	{
		pRetval = &(GetElementData()->ele_minimal);
	}

	return(pRetval);
}

//	Careful when using, may return incorrect structure if not correct
//		form type.
lo_FormElementToggleData *CFormElement::GetElementToggleData() const
{
	lo_FormElementToggleData *pRetval = NULL;
	if(GetElementData())	{
		ASSERT(
			GetElementData()->type == FORM_TYPE_RADIO ||
			GetElementData()->type == FORM_TYPE_CHECKBOX
			);
		pRetval = &(GetElementData()->ele_toggle);
	}

	return(pRetval);
}

//	Careful when using, may return incorrect structure if not correct
//		form type.
lo_FormElementSelectData *CFormElement::GetElementSelectData() const
{
	lo_FormElementSelectData *pRetval = NULL;
	if(GetElementData())	{
		ASSERT(
			GetElementData()->type == FORM_TYPE_SELECT_ONE ||
			GetElementData()->type == FORM_TYPE_SELECT_MULT
			);
		pRetval = &(GetElementData()->ele_select);
	}

	return(pRetval);
}

void *CFormElement::GetElementFEData() const
{
	void *pRetval = NULL;
	if(GetElementMinimalData())	{
		pRetval = GetElementMinimalData()->FE_Data;
	}

	return(pRetval);
}

//	Set the owning context.
void CFormElement::SetContext(CAbstractCX *pCX)
{
	//	Simply copy.
	m_pCX = pCX;

    //  If we're printing, we know (or assume), that
    //      any form data has been set to the last context's current
    //      state, so we don't want to just throw it all away by
    //      calling UseDefaultData in GetFormElementInfo.
    //  This does not apply to metafiles until they serialize the form data also.
    if(GetContext() && GetContext()->IsPrintContext())  {
        m_bUseCurrentData = TRUE;
    }
}

//	Return the owning context.
CAbstractCX *CFormElement::GetContext() const
{
	return(m_pCX);
}

//	Update the coordinates of display from the layout
//		element and display if need be.
void CFormElement::DisplayFormElement(LTRB& Rect)
{
    //	Class which need to handle this should know how in the derivation.
    //	Call the base class after your implementation.
    
    //	In any event, make sure that the widget is visible.
    //	Again, call the base class after your implementation.
    HWND hRaw = GetRaw();
    if(hRaw)	{
        BOOL bVisible = ::IsWindowVisible(hRaw);
        
        /* If window is visible, but should be invisible, then make it disappear */
        if (m_pFormElement->ele_attrmask & LO_ELE_INVISIBLE) {
            if (bVisible) {
                ::ShowWindow(hRaw, SW_HIDE);
            }
        }
        /* If window is invisible, but should be visible, then draw it */
        else {
            if (!bVisible) {
                ::ShowWindow(hRaw, SW_SHOWNA);
            }
        }
    }
}

//	This is called by LAYOUT when a form element should interpret a newline as
//		hitting the submit button in a form.  This is done mainly on single line
//		edit fields.
void CFormElement::FormTextIsSubmit()
{
    HWND hRaw = GetRaw();
    if(hRaw) {
        CNetscapeEdit *pEdit = (CNetscapeEdit *)CEdit::FromHandlePermanent(hRaw);
        pEdit->SetSubmit(TRUE);
    }
}

//	Free the form element.
//	The form data is passed in, because layout has already
//		disassociated it with the form struct.
//	No need to override really....
void CFormElement::FreeFormElement(LO_FormElementData *pFormData)
{
	//	Cause all widgets to be deallocated if not already.
	DestroyWidget();

	//	NOTE:	m_pFormElement at this point can not be guaranteed to be valid.
	//			Do not use it or members which access it.
	SetElement(NULL);

	//	We're done.
    //  Set the FE data to be null, since we're deleting it.
	if(pFormData)	{
		pFormData->ele_minimal.FE_Data = NULL;
	}
	delete this;
}

//	Layout calls this to determine the size of a form element.
void CFormElement::GetFormElementInfo()
{
	//	Avoid creating lots of widgets, see if we already have one.
	if(IsWidgetPresent() == FALSE)	{
		//	Not present, we have to create it on our own.
		//	Be sure to check ShouldUseCurrentData to see if you should use the
		//		current or default data from the Layout struct when
		//		filling in the information (however, the actual size
		//		calculation of the widget should be done with default
		//		data always).
		CreateWidget();

		if(ShouldUseCurrentData())	{
			UseCurrentData();
		}
		else	{
			UseDefaultData();

			//	First time creations should by default fill in the
			//		current data with the default data, so that
			//		MOCHA has some data to play with.
			UpdateCurrentData();
		}
	}

	//	A widget should be present, so that we can check cached
	//		size values in the future.
	m_bWidgetPresent = TRUE;

	//	Get widget information into the layout structure.
	FillSizeInfo();
}

//	Retrieve the data from the form element, and possibly
//		delete the form element widget.
void CFormElement::GetFormElementValue(BOOL bTurnOff)
{
	//	Collect data from the widgets.
	UpdateCurrentData();

	if(bTurnOff)	{
		//	If we have to turn off, then mark that we should use current data in the future
		//		when we have to recreate ourselves, and mark that our widget is gone.
		m_bUseCurrentData = TRUE;
		m_bWidgetPresent = FALSE;

		//	Get rid of the widgets.
		DestroyWidget();
	}
}

//	Reset the form element to its default value.
void CFormElement::ResetFormElement()
{
	//	Just use the default data in the LAYOUT struct.
	UseDefaultData();

	//	Update (write over) any modified current (now old) data.
	UpdateCurrentData();
}

//	Toggle the form element's state.
void CFormElement::SetFormElementToggle(BOOL bState)
{
    HWND hRaw = GetRaw();
    if(hRaw) {
        //  Set state if not already reflected.
        if(Button_GetCheck(hRaw) != bState) {
            Button_SetCheck(hRaw, bState);
        }
        UpdateCurrentData();
    }
}

//	Give focus to the form element.
void CFormElement::FocusInputElement()
{
    HWND hRaw = GetRaw();
    if(hRaw && GetContext() && GetElement()) {
        FEU_MakeElementVisible(GetContext()->GetContext(), (LO_Any *)GetElement());
        ::SetFocus(hRaw);
    }
}

//	Do not give focus to the form element.
void CFormElement::BlurInputElement()
{
    HWND hRaw = GetRaw();
    if(hRaw) {
        // Only blur focus if we already have it and we can give it to our parent.
        if(hRaw == ::GetFocus() && ::GetParent(hRaw)) {
            ::SetFocus(::GetParent(hRaw));
        }
    }
}

//	Select the form element's data.
void CFormElement::SelectInputElement()
{
    HWND hRaw = GetRaw();
    if(hRaw) {
        Edit_SetSel(hRaw, 0, -1);
    }
}

//	Change the value of the form element.
void CFormElement::ChangeInputElement()
{
    //	All we really need to do is update ourselves so that we reflect the
	//		current data.
	UseCurrentData();

	if(GetElementMinimalData())	{
		if(GetElementMinimalData()->type == FORM_TYPE_RADIO)	{
			if(GetContext() && GetContext()->GetContext())	{
                lo_FormElementToggleData * tog_data;
                tog_data = (lo_FormElementToggleData *) GetElement()->element_data;
		        if(tog_data->toggled) {
                    // If we are supposed to be on, turn off everyone else
#ifdef MOZ_NGLAYOUT
            XP_ASSERT(0);
#else
    				LO_FormRadioSet(GetContext()->GetDocumentContext(), GetElement());  
#endif
		        }
			}
		}
	}
}

//  Enable the form element (Editable to the user).
void CFormElement::EnableInputElement()
{
    HWND hRaw = GetRaw();
    if(hRaw) {
        ::EnableWindow(hRaw, TRUE);
    }
}

//  Disable the form element (Read only to the user).
void CFormElement::DisableInputElement()
{
    HWND hRaw = GetRaw();
    if(hRaw) {
        ::EnableWindow(hRaw, FALSE);
    }
}

//	Return a unique control ID each time called.
UINT CFormElement::GetDynamicControlID()
{
	//	Handle wrap.
	if(m_uNextControlID == 0)	{
		m_uNextControlID = DYNAMIC_RESOURCE;
	}

	return(m_uNextControlID++);
}

#ifdef XP_WIN16
//	Call this to get the segment for edit fields under 16 bit windows.
//	This is so they are taken out of the precious DGROUP.
HINSTANCE CFormElement::GetSegment()
{
	HINSTANCE hRetval = NULL;

	//	Only if we have a context, and it has widgets.
	if(GetContext() && GetContext()->IsWindowContext())	{
		CPaneCX *pPaneCX = VOID2CX(GetContext(), CPaneCX);
		hRetval = pPaneCX->GetSegment();
	}

	return(hRetval);
}
#endif

extern "C" void FE_RaiseWindow(MWContext *pContext)    {
    //  Make sure this is possible.
    if(pContext && ABSTRACTCX(pContext) && !ABSTRACTCX(pContext)->IsDestroyed()
        && ABSTRACTCX(pContext)->IsFrameContext() && PANECX(pContext)->GetPane()
        && WINCX(pContext)->GetFrame() && WINCX(pContext)->GetFrame()->GetFrameWnd()) {
        CWinCX *pWinCX = WINCX(pContext);
        CFrameWnd *pFrameWnd = pWinCX->GetFrame()->GetFrameWnd();

        //  Bring the frame to the top first.
		if(pFrameWnd->IsIconic())	{
			pFrameWnd->ShowWindow(SW_RESTORE);
		}
        ::SetWindowPos(pFrameWnd->GetSafeHwnd(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#ifdef XP_WIN32
        pFrameWnd->SetForegroundWindow();
#endif

        //  Now, set focus to the view being raised,
        //      possibly a frame cell.
        pWinCX->
            GetFrame()->
            GetFrameWnd()->
            SetActiveView(pWinCX->GetView(), TRUE);
    }
}

extern "C" void FE_LowerWindow(MWContext *pContext)    {
    //  Make sure this is possible.
    if(pContext && ABSTRACTCX(pContext) && !ABSTRACTCX(pContext)->IsDestroyed()
        && ABSTRACTCX(pContext)->IsFrameContext() && PANECX(pContext)->GetPane()
        && WINCX(pContext)->GetFrame() && WINCX(pContext)->GetFrame()->GetFrameWnd()) {
        CWinCX *pWinCX = WINCX(pContext);
        CFrameWnd *pFrameWnd = pWinCX->GetFrame()->GetFrameWnd();

	MWContext *pTravContext = NULL;
	CWinCX *pTravWinCX = NULL;
	CFrameWnd *pTravFrameWnd = NULL;
	HWND pTopBottommostHwnd = NULL;
	HWND pTravHwnd;
	XP_Bool LowerWindow;

	// We don't have a good way to do this.  Look through the global
	// context list for always lowered windows.
	XP_List *pTraverse = XP_GetGlobalContextList();
	while (pTravContext = (MWContext *)XP_ListNextObject(pTraverse)) {
	    if(pTravContext && ABSTRACTCX(pTravContext) && 
		    !ABSTRACTCX(pTravContext)->IsDestroyed() &&
		    ABSTRACTCX(pTravContext)->IsFrameContext() &&
		    WINCX(pTravContext)->GetFrame() && 
		    WINCX(pTravContext)->GetFrame()->GetFrameWnd())	{

		// Find each always lowered window.
		pTravWinCX = WINCX(pTravContext);
		pTravFrameWnd = pTravWinCX->GetFrame()->GetFrameWnd();
		if (((CGenericFrame*)pTravFrameWnd)->IsBottommost()) {
		    if (!pTopBottommostHwnd)
			pTopBottommostHwnd = pTravFrameWnd->GetSafeHwnd();
		    else {
			//Ugly!  See if any of the other bottommost windows are above
			//this one.  If so, use them.
			LowerWindow = FALSE;
			pTravHwnd = pTopBottommostHwnd;
			while (pTravHwnd = GetNextWindow(pTravHwnd, GW_HWNDPREV)) {
			    if (pTravHwnd == pTravFrameWnd->GetSafeHwnd())
				LowerWindow = TRUE;
			}
			if (LowerWindow)
			    pTopBottommostHwnd = pTravFrameWnd->GetSafeHwnd();
		    }
		}
	    }
	}
	
	if (pTopBottommostHwnd)
	    //  Place the frame above the top bottommost window..
	    ::SetWindowPos(pFrameWnd->GetSafeHwnd(), GetNextWindow(pTopBottommostHwnd, GW_HWNDPREV), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	else
	    //  Push the frame to the bottom.
	    ::SetWindowPos(pFrameWnd->GetSafeHwnd(), HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        //  Remove focus.
        ::SetFocus(NULL);
    }
}

//
// Mocha-betties demand attention --- give them the focus
//
extern "C" void FE_FocusInputElement(MWContext * pContext, LO_Element * pElement)
{
    //  They may intend the actual context.
    if(pElement == NULL)    {
        TRACE("Focusing context\n");
        FE_RaiseWindow(pContext);
        return;
    }
	else if(pElement->type == LO_FORM_ELE)	{
		CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(pContext), &(pElement->lo_form));
		if(pFormClass != NULL)	{
			pFormClass->FocusInputElement();
		}
	}
}

//
// Mocha-betties are sulking and don't want anyone looking at them --- just give
//    focus to our parent (if we have the focus to begin with)
//
extern "C" void FE_BlurInputElement(MWContext * pContext, LO_Element * pElement)
{
    //  They may intend the actual context.
    if(pElement == NULL)    {
        TRACE("Blurring context\n");
        FE_LowerWindow(pContext);
        return;
    }
	else if(pElement->type == LO_FORM_ELE)	{
		CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(pContext), &(pElement->lo_form));
		if(pFormClass != NULL)	{
			pFormClass->BlurInputElement();
		}
	}
}

//
// Mocha-betties want to be the ones that are selected
//
extern "C" void FE_SelectInputElement(MWContext * pContext, LO_Element * pElement)
{
	//	Get our front end form element, and have it do its thang.
	ASSERT(pElement && pElement->type == LO_FORM_ELE);
	if(pElement && pElement->type == LO_FORM_ELE)	{
		CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(pContext), &(pElement->lo_form));
		if(pFormClass != NULL)	{
			pFormClass->SelectInputElement();
		}
	}
}

// Mocha-betties have decided to change the value of a form element
//   deal with it
extern "C" void FE_ChangeInputElement(MWContext * pContext, LO_Element * pElement)
{
	//	Get our front end form element, and have it do its thang.
	ASSERT(pElement && pElement->type == LO_FORM_ELE);
	if(pElement && pElement->type == LO_FORM_ELE)	{
		CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(pContext), &(pElement->lo_form));
		if(pFormClass != NULL)	{
			pFormClass->ChangeInputElement();
		}
	}
}

// FE_ClickAnyElement() is based on FE_ClickInputElement(), and should replace the latter.
// If haveXY != 0, the xx, yy will be used for click. otherwise click the center of element.
//
// xx, yy are element coordinates, This function take care of :
// lo_any.x_offset, pWinCX->GetOriginX(), ClientToScreen(), and  ScreenToClient()
extern "C" int FE_ClickAnyElement(MWContext * pContext,  LO_Element * pElement, int haveXY, int32 xx, int32 yy )
{
    if(pElement && ABSTRACTCX(pContext) && ABSTRACTCX(pContext)->IsWindowContext())    {
        HWND hRaw = NULL;
        CPaneCX *pPaneCX = PANECX(pContext);
    	//	Get our front end form element, and have it do its thang.
        //  This differs only in that they are child windows and therefore, the destination window
        //      for the events differ.
	    if(pElement->type == LO_FORM_ELE)	{
		    CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(pContext), &(pElement->lo_form));
		    if(pFormClass != NULL)	{
                hRaw = pFormClass->GetRaw();
		    }
	    }
        else    {
            //  The destination for the click is the view.
            hRaw = pPaneCX->GetPane();
        }

        if(hRaw) {

			if( ! haveXY ) {
				//  Emulate a mouse click in the center of the element.
				xx  = pElement->lo_any.x + pElement->lo_any.width / 2;
				yy  = pElement->lo_any.y + pElement->lo_any.height / 2;
			}
			
			//  Mouse coordinates are relative to the widget, not absolute to the screen.
            //  We may have some problems if the destination is not viewable.
            
            //  Figure the coordinates of the element according to context (view).
            xx += pElement->lo_any.x_offset - pPaneCX->GetOriginX();
	        yy += pElement->lo_any.y_offset - pPaneCX->GetOriginY();
 
            //  Adjust for coords to the widget.
            CPoint pCoords(CASTINT(xx), CASTINT(yy));
            ::ClientToScreen(pPaneCX->GetPane(), &pCoords);
            ::ScreenToClient(hRaw, &pCoords);
            //  Fire.
            ::SendMessage(hRaw, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pCoords.x, pCoords.y));
            ::SendMessage(hRaw, WM_LBUTTONUP, 0, MAKELPARAM(pCoords.x, pCoords.y));
			return( 1 );
        }
    }	// if(pElement && ABSTRACTCX(pCon
	return( 0 );
}	// FE_ClickAnyElement()

extern "C" void FE_ClickInputElement(MWContext * pContext, LO_Element * pElement)
{
    if(pElement && ABSTRACTCX(pContext) && ABSTRACTCX(pContext)->IsWindowContext())    {
        HWND hRaw = NULL;
        CPaneCX *pPaneCX = PANECX(pContext);
    	//	Get our front end form element, and have it do its thang.
        //  This differs only in that they are child windows and therefore, the destination window
        //      for the events differ.
	    if(pElement->type == LO_FORM_ELE)	{
		    CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(pContext), &(pElement->lo_form));
		    if(pFormClass != NULL)	{
                hRaw = pFormClass->GetRaw();
		    }
	    }
        else    {
            //  The destination for the click is the view.
            hRaw = pPaneCX->GetPane();
        }

        if(hRaw) {
            //  Emulate a mouse click in the center of the element.
            //  Mouse coordinates are relative to the widget, not absolute to the screen.
            //  We may have some problems if the destination is not viewable.
            
            //  Figure the coordinates of the element according to context (view).
            LTRB Rect;
	        Rect.left = pElement->lo_any.x + pElement->lo_any.x_offset - pPaneCX->GetOriginX();
	        Rect.top = pElement->lo_any.y + pElement->lo_any.y_offset - pPaneCX->GetOriginY();
	        Rect.right = Rect.left + pElement->lo_any.width;
	        Rect.bottom = Rect.top + pElement->lo_any.height;
            //  Adjust for coords to the widget.
            CPoint pCoords(CASTINT(Rect.left), CASTINT(Rect.top));
            ::ClientToScreen(pPaneCX->GetPane(), &pCoords);
            ::ScreenToClient(hRaw, &pCoords);
            //  Center target.
            pCoords.x += CASTINT(Rect.Width() / 2);
            pCoords.y += CASTINT(Rect.Height() / 2);
            //  Fire.
            ::SendMessage(hRaw, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pCoords.x, pCoords.y));
            ::SendMessage(hRaw, WM_LBUTTONUP, 0, MAKELPARAM(pCoords.x, pCoords.y));
        }
    }
}

extern "C" void FE_EnableInputElement(MWContext *pContext, LO_Element *pElement)
{
	//	Get our front end form element, and have it do its thang.
	ASSERT(pElement && pElement->type == LO_FORM_ELE);
	if(pElement && pElement->type == LO_FORM_ELE)	{
		CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(pContext), &(pElement->lo_form));
		if(pFormClass != NULL)	{
			pFormClass->EnableInputElement();
		}
	}
}

extern "C" void FE_DisableInputElement(MWContext *pContext, LO_Element *pElement)
{
	//	Get our front end form element, and have it do its thang.
	ASSERT(pElement && pElement->type == LO_FORM_ELE);
	if(pElement && pElement->type == LO_FORM_ELE)	{
		CFormElement *pFormClass = CFormElement::GetFormElement(ABSTRACTCX(pContext), &(pElement->lo_form));
		if(pFormClass != NULL)	{
			pFormClass->DisableInputElement();
		}
	}
}

void CFormElement::MoveWindow(HWND hWnd, int32 lX, int32 lY)
{
    if(hWnd && ::GetParent(hWnd))	{
        //	Is a window, needs a widget/window representation.
        //	All we need to do is to make sure that the widget is
        //		at the correct location as requested by layout.
        //	Figure the XY layout wants.

        //	Determine widget offset into the client window.
        RECT crWidget;
        ::GetWindowRect(hWnd, &crWidget);
        
        POINT ptLT;
        ptLT.x = crWidget.left;
        ptLT.y = crWidget.top;
        ::ScreenToClient(::GetParent(hWnd), &ptLT);

        //  Only move the widget if not already there.
        //  No need to cause the widget to redraw.
        if(ptLT.x != lX || ptLT.y != lY) {
            ::MoveWindow(hWnd, CASTINT(lX), CASTINT(lY),
                crWidget.right - crWidget.left,
                crWidget.bottom - crWidget.top,
                TRUE);
        }
    }
}


void CFormElement::SetWidgetFont(HDC hdc, HWND hWidget)
{
	// this is just a trick to get the HFONT from the dc.
	if (hWidget) {
		HFONT hFont = (HFONT)::SelectObject(hdc, 
						(HFONT)	GetStockObject(SYSTEM_FONT));
		// now select the correct font into the DC.		
		::SelectObject(hdc, hFont);
		
#ifdef XP_WIN32
		//  Win95 systems need to keep their previous margins, or things get changed (margins)
		//      upon switching fonts.
		//  MS Knowledge Base article ID: Q138419
		DWORD dwMargins = 0;
		if(sysInfo.IsWin4_32()) {
			dwMargins = ::SendMessage(hWidget, EM_GETMARGINS, 0L, 0L);
		}
#endif
		::SendMessage(hWidget, WM_SETFONT, (WPARAM)hFont, TRUE);
#ifdef XP_WIN32
		if(sysInfo.IsWin4_32()) {
			::SendMessage(
			    hWidget,
			    EM_SETMARGINS,
			    EC_LEFTMARGIN | EC_RIGHTMARGIN,
			    MAKELPARAM(LOWORD(dwMargins), HIWORD(dwMargins)));
		}
#endif
	}
}
