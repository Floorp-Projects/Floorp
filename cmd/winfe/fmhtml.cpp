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

#include "fmhtml.h"
#include "odctrl.h"
#include "intl_csi.h"
#include "netsdoc.h" 
#include "edview.h"  
#include "edt.h"     

#ifdef MOZ_ENDER_MIME
#include "mhtmlstm.h"
#endif

extern char * EDT_NEW_DOC_URL; 

//	This file is dedicated to form type select one elements
//		otherwise known as list boxes on windows and
//		their implementation as requried by the XP layout
//		library.

//	Construction simply clears all members.
CFormHtmlarea::CFormHtmlarea()
{
    //	No Widget yet.
    m_pWidget = NULL;
}

//	Destruction cleans out all members.
CFormHtmlarea::~CFormHtmlarea()
{
}

//	How to set the LO form element.
//	This may change during the lifetime of an instance, so use this
//		to update all referencing values.
void CFormHtmlarea::SetElement(LO_FormElementStruct *pFormElement)
{
    //	Call the base.2
    CFormElement::SetElement(pFormElement);
    if (m_pWidget && m_pWidget->GetEditView())
        m_pWidget->GetEditView()->SetElement(pFormElement);
    else
        return;
    MWContext *t_context;
    CNetscapeEditView *t_view = m_pWidget->GetEditView();
    if (!t_view)
        return;
    t_context = t_view->GetContext()->GetContext();
    if (t_context)
        EDT_SetEmbeddedEditorData(t_context, pFormElement);
    else
        XP_ASSERT(FALSE);
}

//	Set the owning context.1
//	Use this to determine what context we live in and how we should
//		represent ourself (DC or window).
void CFormHtmlarea::SetContext(CAbstractCX *pCX)
{
    //	Call the base.
    CFormElement::SetContext(pCX);

}

//	Display the form element given the particular context we are in.
//	Possibly only use a DC for representation, or have the
//		window move.
void CFormHtmlarea::DisplayFormElement(LTRB& Rect)
{
    //	Display only has meaning if our context is a device context.
    if(GetContext() && GetContext()->IsDCContext())	{
        //	Further, need to detect how we're going to be drawing ourselves.
        RECT rect;
        rect.left=Rect.left;
        rect.right=Rect.right;
        rect.top=Rect.top;
        rect.bottom=Rect.bottom;
        MoveWindow(m_pWidget->m_hWnd, Rect.left, Rect.top + EDIT_SPACE / 2);
        m_pWidget->ShowWindow(SW_SHOW);
        m_pWidget->UpdateWindow();
        if (m_pWidget->GetEditView())
        {
            m_pWidget->GetEditView()->ShowWindow(SW_SHOW);
            m_pWidget->GetEditView()->UpdateWindow();
        }
        CWnd::FromHandle(VOID2CX(GetContext(), CPaneCX)->GetPane())->InvalidateRect(&rect);
    }
    //	Call the base.
    CFormElement::DisplayFormElement(Rect);
}


//	Destroy the widget (window) implemenation of the form.
void CFormHtmlarea::DestroyWidget()
{
    //	Get rid of the widget if around.
    if (m_pWidget)
    {
        m_pWidget->DestroyWindow();
        m_pWidget=NULL;
    }
}

//	Create the widget (window) implementation of the form
//		but DO NOT DISPLAY.3
void CFormHtmlarea::CreateWidget()
{
    if(GetContext() && GetElement())	
        if(VOID2CX(GetContext(), CPaneCX)->GetPane())	
        {
            m_pWidget = new CEnderView(GetContext());
            if (!m_pWidget->Create(CWnd::FromHandle(VOID2CX(GetContext(), CPaneCX)->GetPane()),GetElementHtmlareaData(),GetTextAttr() ))
                DestroyWidget();
            MWContext *t_context;
            t_context = m_pWidget->GetEditView()->GetContext()->GetContext();
            if (t_context)
            {
                lo_FormElementTextareaData *t_data = (lo_FormElementTextareaData *)GetElementHtmlareaData();
                EDT_SetEmbeddedEditorData(t_context, t_data);
            }
            else
                XP_ASSERT(FALSE);
        }
}

//	Copy the current data out of the layout struct into the form 
//		widget, or mark that you should represent using the current data.
//use the mime bits if you dont have current_text!
void CFormHtmlarea::UseCurrentData()
{
    //	Detect context type and do the right thing.
    if(GetContext())	{
        //	Need a widget.
        if(m_pWidget && m_pWidget->GetEditView())	{
            //	Determine the default text to set.
            char *pCurrent = "";
            lo_FormElementTextareaData *t_data = (lo_FormElementTextareaData *)GetElementHtmlareaData();
            if(t_data && t_data->current_text)
                pCurrent = (char *)t_data->current_text;
            if (*pCurrent)
#ifdef MOZ_ENDER_MIME
                EDT_SetDefaultHTML( m_pWidget->GetEditView()->GetContext()->GetContext(), pCurrent );
#else
                EDT_SetDefaultHTML( m_pWidget->GetEditView()->GetContext()->GetContext(), pCurrent );
#endif /*MOZ_ENDER_MIME*/
        }
    }
}

//	Copy the default data out of the layout struct into the form 4
//		widget, or mark that you should represent using the default data.
void CFormHtmlarea::UseDefaultData()
{
    //	Detect context type and do the right thing.
    if(GetContext())	
    {
        //	Need a widget.
        if(m_pWidget)	{
            //	Determine the default text to set.
            char *pDefault = "";
            lo_FormElementTextareaData *t_data = (lo_FormElementTextareaData *)GetElementHtmlareaData();
            if(t_data && t_data->default_text)	{
                pDefault = (char *)t_data->default_text;
            }
            if (pDefault)
#ifdef MOZ_ENDER_MIME
                EDT_SetDefaultHTML( m_pWidget->GetEditView()->GetContext()->GetContext(), pDefault );
#else
                EDT_SetDefaultHTML( m_pWidget->GetEditView()->GetContext()->GetContext(), pDefault );
#endif /*MOZ_ENDER_MIME*/
        }
    }
}

//	Fill in the layout size information in the layout struct regarding
//		the dimensions of the widget.6
void CFormHtmlarea::FillSizeInfo()
{
    //	Detect context type and do the right thing.
    if(GetContext() && GetElement())	
    {
        //	Need a widget.
        if(m_pWidget)	
        {
            //	Determine our window position.
            CRect crRect;
            m_pWidget->GetWindowRect(crRect);

            //	Munge the coordinate a little for layout.
            //	We'll have to know how to decode this information
            //		in the display routine (by half).
            GetElement()->width = crRect.Width();
            GetElement()->height = crRect.Height() + EDIT_SPACE;
            GetElement()->border_vert_space = EDIT_SPACE/2;
        }
        else	
        {
            //	No widget, tell layout nothing.
            GetElement()->width = 0;
            GetElement()->height = 0;
            GetElement()->baseline = 0;
        }
    }
}

//	Copy the current data out of the form element back into the
//		layout struct.
void CFormHtmlarea::UpdateCurrentData(BOOL bSubmit)
{
    if(GetContext())	
    {
        lo_FormElementTextareaData *t_data = (lo_FormElementTextareaData *)GetElementHtmlareaData();
        if(m_pWidget && ::IsWindow(m_pWidget->GetSafeHwnd()) && t_data)	
        {

            if(t_data->current_text)	{
                XP_FREE(t_data->current_text);
                t_data->current_text = NULL;
            }
#ifdef MOZ_ENDER_MIME
            if (bSubmit)
            {
                
                MWContext *t_context;
                t_context = m_pWidget->GetEditView()->GetContext()->GetContext();

                XP_FREEIF(GetElementHtmlareaData()->mime_bits);/*this also nulls it*/

                /*first save data the way it WAS!*/
                char *t_oldbuffer = NULL;
                EDT_SaveToBuffer(t_context,(char **)&t_oldbuffer);

                char *pRootPartName = NULL; 
                lo_FormElementHtmlareaData *t_htmldata = (lo_FormElementHtmlareaData *)t_data;
                //fs will be deleted in the UrlExitRoutine called from the ::Complete function.
                MSG_MimeRelatedStreamSaver *fs = new MSG_MimeRelatedStreamSaver(NULL, t_context, NULL, 
                    FALSE, MSG_DeliverNow, 
                    NULL, 0,
                    NULL,
                    NULL,
                    &pRootPartName,(char **)&t_htmldata->mime_bits);
                EDT_SaveFileTo(t_context, 
                    ED_FINISHED_MAIL_SEND, 
                    pRootPartName,fs, TRUE, TRUE);
                // Spin here until temp file saving is finished
                while(  t_context->edit_saving_url ) 
                {
                    FEU_StayingAlive();
                }
                if (t_oldbuffer)
                {
                    EDT_SetDefaultHTML( t_context, t_oldbuffer );
                    XP_FREE(t_oldbuffer);
                }
            }
            else
#endif //MOZ_ENDER_MIME
            {
                MWContext *t_context = m_pWidget->GetEditView()->GetContext()->GetContext();
                EDT_SaveToBuffer(t_context,(char **)&t_data->current_text);
            }
        }
    }

    // Note: EDT_SaveFileTo will delete fs, even if it returns an error.  So
    // it is incorrect to delete it here.  Also, we ignore the result, because
    // it calls FE_Alert itself.
}

HWND CFormHtmlarea::GetRaw()
{
    return(m_pWidget ? m_pWidget->m_hWnd : NULL);
}

