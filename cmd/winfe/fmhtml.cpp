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

	//	Let the widget know the element.
/*
	if(m_pWidget)
		m_pWidget->SetContext(GetContext()); // ? GetContext()->GetContext() : NULL); //, GetElement());
		*/
}

//	Set the owning context.1
//	Use this to determine what context we live in and how we should
//		represent ourself (DC or window).
void CFormHtmlarea::SetContext(CAbstractCX *pCX)
{
	//	Call the base.
	CFormElement::SetContext(pCX);

	//	Let the widget know the context.
/*
	if(m_pWidget)
		m_pWidget->SetContext(GetContext());// ? GetContext()->GetContext() : NULL); //, GetElement());
		*/
}

//	Display the form element given the particular context we are in.
//	Possibly only use a DC for representation, or have the
//		window move.
void CFormHtmlarea::DisplayFormElement(LTRB& Rect)
{
	//	Display only has meaning if our context is a device context.
	if(GetContext() && GetContext()->IsDCContext())	{
		//	Further, need to detect how we're going to be drawing ourselves.
		if(GetContext()->IsPureDCContext())	
		{
			//	Only works from a DC, needs a GDI drawing representation.
		}
		else if(GetContext()->IsWindowContext())	
		{
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
		else	
		{
			//	Is undefined....
			ASSERT(0);
		}
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
		if(GetContext()->IsWindowContext() && VOID2CX(GetContext(), CPaneCX)->GetPane())	
        {
			m_pWidget = new CEnderView(GetContext());
            if (!m_pWidget->Create(CWnd::FromHandle(VOID2CX(GetContext(), CPaneCX)->GetPane()),GetElementHtmlareaData(),GetTextAttr() ))
                DestroyWidget();
        }
}

//	Copy the current data out of the layout struct into the form 5
//		widget, or mark that you should represent using the current data.
void CFormHtmlarea::UseCurrentData()
{
	//	Detect context type and do the right thing.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Need a widget.
			//	Need a widget.
			if(m_pWidget && m_pWidget->GetEditView())	{
				//	Determine the default text to set.
				char *pCurrent = "";
				if(GetElementHtmlareaData() && GetElementHtmlareaData()->current_text)	{
					pCurrent = (char *)GetElementHtmlareaData()->current_text;
				}
				if (pCurrent)
					EDT_SetDefaultHTML( m_pWidget->GetEditView()->GetContext()->GetContext(), pCurrent );
				// We have to SetContext to the widget before we SetWindowText
				// Otherwise, the widget don't know what csid the text is.
				//m_pWidget->SetContext(GetContext());//, GetElement());
				//m_pWidget->SetWindowText(pDefault);
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Printing/metafile
            //  Whatever is current is current.
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
		if(GetContext()->IsWindowContext())	
		{
			//	Need a widget.
			if(m_pWidget)	{
				//	Determine the default text to set.
				char *pDefault = "";
				if(GetElementHtmlareaData() && GetElementHtmlareaData()->default_text)	{
					pDefault = (char *)GetElementHtmlareaData()->default_text;
				}
				if (pDefault)
					EDT_SetDefaultHTML( m_pWidget->GetEditView()->GetContext()->GetContext(), pDefault );
				// We have to SetContext to the widget before we SetWindowText
				// Otherwise, the widget don't know what csid the text is.
				//m_pWidget->SetContext(GetContext());//, GetElement());
				//m_pWidget->SetWindowText(pDefault);
			}
		}
		else if(GetContext()->IsPureDCContext())	
		{
			//	Printing/metafile
            if(GetElementHtmlareaData())    
			{
                //  Clear the current text if present.
			    if(GetElementHtmlareaData()->current_text)	
				{
				    XP_FREE(GetElementHtmlareaData()->current_text);
				    GetElementHtmlareaData()->current_text = NULL;
			    }

                //  Copy over the default_text.
                if(GetElementHtmlareaData()->default_text)  
				{
                    int32 lSize = XP_STRLEN((char *)GetElementHtmlareaData()->default_text) + 1;
                    GetElementHtmlareaData()->current_text = (XP_Block)XP_ALLOC(lSize);
                    if(GetElementHtmlareaData()->current_text)  
					{
                        memcpy(GetElementHtmlareaData()->current_text,
                            GetElementHtmlareaData()->default_text,
                            CASTSIZE_T(lSize));
                    }
                }
			}
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
		if(GetContext()->IsWindowContext())	
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
		else if(GetContext()->IsPureDCContext())	
		{
			//  Tell layout what we know.
			GetElement()->width = 0;
			GetElement()->height = 0;
			GetElement()->baseline = 0;
        }
	}
}

//	Copy the current data out of the form element back into the
//		layout struct.
void CFormHtmlarea::UpdateCurrentData()
{
	if(GetContext())	
	{
		if(GetContext()->IsWindowContext())	
		{
			if(m_pWidget && ::IsWindow(m_pWidget->GetSafeHwnd()) && GetElementHtmlareaData())	
			{
				if(GetElementHtmlareaData()->current_text)	{
					XP_FREE(GetElementHtmlareaData()->current_text);
					GetElementHtmlareaData()->current_text = NULL;
				}
//                if (EDT_DirtyFlag(m_pWidget->GetContext()->GetContext()))
//                {
				    EDT_SaveToBuffer(m_pWidget->GetEditView()->GetContext()->GetContext(),(char **)&GetElementHtmlareaData()->current_text);
//                }
			}
		}
		else if(GetContext()->IsPureDCContext())	
		{
			//	Printing/metafile
            //  Whatever is current is current.
		}
	}
}

HWND CFormHtmlarea::GetRaw()
{
    return(m_pWidget ? m_pWidget->m_hWnd : NULL);
}

