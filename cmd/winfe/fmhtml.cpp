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
		CNetscapeDoc* pDoc = (CNetscapeDoc *)m_pWidget->GetDocument();
		//warning! do not allow the CWinCX to RE-FREE its frame. 
		//we are borrowning the frame from the "browser window or layer"
		//call ClearFrame to "Clear the frame"
		CWinCX *pCX = m_pWidget->GetContext();
		if (pCX)
		{
			pCX->ClearFrame();//NOT WORKING, MUST FIX
			EDT_DestroyEditBuffer(pCX->GetContext());
		}
		m_pWidget->DestroyWindow();
		if (pDoc)
			delete pDoc;
		m_pWidget=NULL;
	}
}

//	Create the widget (window) implementation of the form
//		but DO NOT DISPLAY.3
void CFormHtmlarea::CreateWidget()
{
	if(GetContext() && GetElement())	
	{
		if(GetContext()->IsWindowContext() && VOID2CX(GetContext(), CPaneCX)->GetPane())	
		{
			//	Need a widget representation.
			//create a new CPaneCX
			CNetscapeDoc* pDoc = new CNetscapeDoc();
			m_pWidget = new CNetscapeEditView();
			m_pWidget->SetEmbedded(TRUE);
			RECT rect;
			rect.left=0;
			rect.top=0;
			rect.right=1;
			rect.bottom=1;
			if (!m_pWidget->Create(NULL, NULL, 
				WS_CHILD | WS_VSCROLL | WS_BORDER | ES_LEFT | WS_TABSTOP | ES_MULTILINE //AFX_WS_DEFAULT_VIEW
				, rect, 
				CWnd::FromHandle(VOID2CX(GetContext(), CPaneCX)->GetPane()), ID_ENDER, NULL))
			{
				TRACE("Warning: could not create view for frame.\n");
				m_pWidget=NULL;
				return;
			}
			CPaneCX* cx= VOID2CX(GetContext(), CPaneCX);
			HWND hwnd= cx->GetPane();
			CWnd *pwnd= CWnd::FromHandle(hwnd);
			CGenericView *genview=NULL;
			if (pwnd->IsKindOf(RUNTIME_CLASS(CGenericView)))
				genview=(CGenericView *)pwnd;
			if (!genview)
				return;
			CWinCX* pDontCare = new CWinCX((CGenericDoc *)pDoc,
									 genview->GetFrame(), (CGenericView *)m_pWidget);
			pDontCare->GetContext()->is_editor = TRUE;
			m_pWidget->SetContext(pDontCare);
			pDontCare->Initialize(pDontCare->CDCCX::IsOwnDC(), &rect);
			pDontCare->NormalGetUrl(EDT_NEW_DOC_URL);
//ADJUST THE SIZE OF THE WINDOW ACCORDING TO ROWS AND COLS EVEN THOUGH THAT IS NOT ACCURATE
			//	Measure some text.
			CDC *pDC = m_pWidget->GetDC();
            CyaFont *pMyFont;
			if(pDC)	
			{
                CDC t_dc;
                t_dc.CreateCompatibleDC( pDC );
				CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
				pDCCX->SelectNetscapeFont( t_dc.GetSafeHdc(), GetTextAttr(), pMyFont );
				if (pMyFont) 
				{
					//SetWidgetFont(pDC->GetSafeHdc(), m_pWidget->m_hWnd);
					//GetElement()->text_attr->FE_Data = pMyFont;
					//	Default length is 20
					//	Default lines is 1
					int32 lLength = 20;
					int32 lLines = 1;

					//	See if we can measure the default text, and/or
					//		set up the size and size limits.
					if(GetElementHtmlareaData())	
					{
						if(GetElementHtmlareaData()->cols > 0)	{
							//	Use provided size.
							lLength = GetElementHtmlareaData()->cols;
						}
						if(GetElementHtmlareaData()->rows > 0)	{
							//	Use provided size.
							lLines = GetElementHtmlareaData()->rows;
						}
					}

					//	Now figure up the width and height we would like.
	//				int32 lWidgetWidth = (lLength + 1) * tm.tmAveCharWidth + sysInfo.m_iScrollWidth;
	//				int32 lWidgetHeight = (lLines + 1) * tm.tmHeight;
					int32 lWidgetWidth = (lLength + 1) * pMyFont->GetMeanWidth() + sysInfo.m_iScrollWidth;
					int32 lWidgetHeight = (lLines + 1) * pMyFont->GetHeight();

					//	If no word wrapping, account a horizontal scrollbar.
					if(GetElementHtmlareaData()->auto_wrap == TEXTAREA_WRAP_OFF)	{
						lWidgetHeight += sysInfo.m_iScrollHeight;
					}

					//	Move the window.
					m_pWidget->MoveWindow(1, 1, CASTINT(lWidgetWidth), CASTINT(lWidgetHeight), FALSE);

					pDCCX->ReleaseNetscapeFont( t_dc.GetSafeHdc(), pMyFont );
	                pDCCX->ReleaseContextDC(t_dc.GetSafeHdc());
					m_pWidget->ReleaseDC(pDC);
				}
				else
				{
					m_pWidget->ReleaseDC(pDC);
					DestroyWidget();
				}
			}
			else
			{
				DestroyWidget();
			}
		}
		else if(GetContext()->IsPureDCContext())	
		{
			//	Need a drawn representation.
		}
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
			if(m_pWidget)	{
				//	Determine the default text to set.
				char *pCurrent = "";
				if(GetElementHtmlareaData() && GetElementHtmlareaData()->current_text)	{
					pCurrent = (char *)GetElementHtmlareaData()->current_text;
				}
				if (pCurrent)
					EDT_SetDefaultHTML( m_pWidget->GetContext()->GetContext(), pCurrent );
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
					EDT_SetDefaultText( m_pWidget->GetContext()->GetContext(), pDefault );
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
				    EDT_SaveToBuffer(m_pWidget->GetContext()->GetContext(),(char **)&GetElementHtmlareaData()->current_text);
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

