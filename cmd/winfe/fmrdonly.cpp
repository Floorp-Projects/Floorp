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

#include "fmrdonly.h"
#include "odctrl.h"
#include "intl_csi.h"
//	This file is dedicated to form type read only elements
//		otherwise known as edit fields on windows and
//		their implementation as requried by the XP layout
//		library.

//	Construction simply clears all members.
CFormReadOnly::CFormReadOnly()
{
	//	No widget yet.
	m_pWidget = NULL;
}

//	Destruction cleans out all members.
CFormReadOnly::~CFormReadOnly()
{
}

//	How to set the LO form element.
//	This may change during the lifetime of an instance, so use this
//		to update all referencing values.
void CFormReadOnly::SetElement(LO_FormElementStruct *pFormElement)
{
	//	Call the base.
	CFormElement::SetElement(pFormElement);

	//	Let the widget know the element.
	if(m_pWidget)	{
		m_pWidget->SetContext(GetContext() ? GetContext()->GetContext() : NULL, GetElement());
	}
}

//	Set the owning context.
//	Use this to determine what context we live in and how we should
//		represent ourself (DC or window).
void CFormReadOnly::SetContext(CAbstractCX *pCX)
{
	//	Call the base.
	CFormElement::SetContext(pCX);

	//	Have our widget update if present.
	if(m_pWidget)	{
		m_pWidget->SetContext(GetContext() ? GetContext()->GetContext() : NULL, GetElement());
	}
}

//	Display the form element given the particular context we are in.
//	Possibly only use a DC for representation, or have the
//		window move.
void CFormReadOnly::DisplayFormElement(LTRB& Rect)
{
	//	Display only has meaning if our context is a device context.
	if(GetContext() && GetContext()->IsDCContext())	{
		//	Further, need to detect how we're going to be drawing ourselves.
		if(GetContext()->IsPureDCContext())	{
			//	Only works from a DC, needs a GDI drawing representation.
			CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
		}
		else if(GetContext()->IsWindowContext())	{
			MoveWindow(m_pWidget->m_hWnd, Rect.left, Rect.top + EDIT_SPACE / 2);
		}
		else	{
			//	Is undefined....
			ASSERT(0);
		}
	}

	//	Call the base.
	CFormElement::DisplayFormElement(Rect);
}

//	Destroy the widget (window) implemenation of the form.
void CFormReadOnly::DestroyWidget()
{
	//	Get rid of the widget if around.
	if(m_pWidget)	{
		m_pWidget->DestroyWindow();
		delete m_pWidget;
		m_pWidget = NULL;
	}
}

//	Create the widget (window) implementation of the form
//		but DO NOT DISPLAY.
void CFormReadOnly::CreateWidget()
{
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext() && VOID2CX(GetContext(), CPaneCX)->GetPane())	{
			//	Need a widget representation.
			ASSERT(m_pWidget == NULL);
			m_pWidget = new CODNetscapeEdit;
			if(m_pWidget == NULL)	{
				return;
			}

			//	Inform widget of context and element.
			m_pWidget->SetContext(GetContext()->GetContext(), GetElement());

#ifdef XP_WIN16
			//	On 16 bits, we need to set the segment of the widget before creation.
			HINSTANCE hSegment = GetSegment();
			if(hSegment)	{
				m_pWidget->SetInstance(hSegment);
			}
			else	{
				delete m_pWidget;
				m_pWidget = NULL;
				return;
			}
#endif

			//	The style of the widget to be created.
			DWORD dwStyle = WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | ES_READONLY;

			//	Create the widget, hidden, and with a bad size.
			BOOL bCreate =
#ifdef XP_WIN32
				m_pWidget->CreateEx(WS_EX_CLIENTEDGE, 
					 _T("EDIT"),
					 NULL,
					 dwStyle, 
					 1, 1, 0, 0,
					 VOID2CX(GetContext(), CPaneCX)->GetPane(), 
					 (HMENU)GetDynamicControlID(),
					 NULL);			
#else
				m_pWidget->Create(dwStyle, CRect(1, 1, 0, 0), 
					CWnd::FromHandle(VOID2CX(GetContext(), CPaneCX)->GetPane()), 
					GetDynamicControlID());
#endif

			if(!bCreate)	{
				delete m_pWidget;
				m_pWidget = NULL;
				return;
			}

			//	Next, need to size the widget.
			//	Obtain a font.
			//	Measure some text.
			CDC *pDC = m_pWidget->GetDC();
			if(pDC)	{
				//	Textmetrics won't work unless we specifically select the font.
				CyaFont	*pMyFont;
				LO_TextAttr *text_attr = GetElement()->text_attr;
				VOID2CX(GetContext(), CWinCX)->SelectNetscapeFont( pDC->GetSafeHdc(), GetTextAttr(), pMyFont );
				//	Text metrics won't work unless we select the font.

				if (pMyFont) {
					SetWidgetFont(pDC->GetSafeHdc(), m_pWidget->m_hWnd);
					text_attr->FE_Data = pMyFont;

					//	Default length will be for "hi".
					int32 lLength = 2;

					//	Now figure up the width and height we would like.
					int32 lWidgetWidth = pMyFont->GetMaxWidth();
					int32 lWidgetHeight = pMyFont->GetHeight() + pMyFont->GetHeight() / 2;

					//	See if we can get the minimal data.
					if(GetElementMinimalData() && GetElementMinimalData()->value)	{
							//	Use length of text, exactly.
							char *pDefault = (char *)GetElementMinimalData()->value;

							lLength = XP_STRLEN(pDefault);
							SIZE csz;
							CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
								INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
								 pDC->GetSafeHdc(), pDefault, CASTINT(lLength), &csz);

							//	Use this width.
							lWidgetWidth += csz.cx;
					}
					else	{
						//	Use an average width.
						lWidgetWidth += lLength * pMyFont->GetMeanWidth();
					}

					VOID2CX(GetContext(), CWinCX)->ReleaseNetscapeFont( pDC->GetSafeHdc(), pMyFont );
					m_pWidget->MoveWindow(1, 1, CASTINT(lWidgetWidth), CASTINT(lWidgetHeight), FALSE);
					m_pWidget->ReleaseDC(pDC);
				}
				else {
					m_pWidget->ReleaseDC(pDC);
					DestroyWidget();
					return;
				}
			}
			else	{
				//	No DC, no widget.
				DestroyWidget();
				return;
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Need a drawn representation.
		}
	}
}

//	Copy the current data out of the layout struct into the form
//		widget, or mark that you should represent using the current data.
void CFormReadOnly::UseCurrentData()
{
	//	Detect context type and do the right thing.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Need a widget.
			if(m_pWidget)	{
				//	Just set it to the value if present.
				//	No real concept of current or default data in a read only yet.
				char *pData = NULL;
				if(GetElementMinimalData())	{
					pData = (char *)GetElementMinimalData()->value;
				}
				if(pData)	{
					// We have to SetContext to the widget before we SetWindowText
					// Otherwise, the widget don't know what csid the text is.
					m_pWidget->SetContext(GetContext()->GetContext(), GetElement());

					m_pWidget->SetWindowText(pData);
				}
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Printing/metafile
		}
	}
}

//	Copy the default data out of the layout struct into the form
//		widget, or mark that you should represent using the default data.
void CFormReadOnly::UseDefaultData()
{
	//	Detect context type and do the right thing.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Need a widget.
			if(m_pWidget)	{
				//	Just set it to the value if present.
				//	No real concept of current or default data in a read only yet.
				char *pData = NULL;
				if(GetElementMinimalData())	{
					pData = (char *)GetElementMinimalData()->value;
				}
				if(pData)	{
					// We have to SetContext to the widget before we SetWindowText
					// Otherwise, the widget don't know what csid the text is.
					m_pWidget->SetContext(GetContext()->GetContext(), GetElement());

					m_pWidget->SetWindowText(pData);
				}
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Printing/metafile
		}
	}
}

//	Fill in the layout size information in the layout struct regarding
//		the dimensions of the widget.
void CFormReadOnly::FillSizeInfo()
{
	//	Detect context type and do the right thing.
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext())	{
			//	Need a widget.
			if(m_pWidget)	{
				//	Determine our window position.
				CRect crRect;
				m_pWidget->GetWindowRect(crRect);

				//	Munge the coordinate a little for layout.
				//	We'll have to know how to decode this information
				//		in the display routine (by half).
				GetElement()->width = crRect.Width();
				GetElement()->height = crRect.Height() + EDIT_SPACE;
				GetElement()->border_vert_space = EDIT_SPACE/2;

				//	Baseline needs text metric information.
				GetElement()->baseline = GetElement()->height;	//	ugly default in case of failure below.
				CDC *pDC = m_pWidget->GetDC();
				if(pDC)	{
					//	Need to specifically select the font before calling GetTextMetrics.
					CyaFont	*pMyFont;
					VOID2CX(GetContext(), CPaneCX)->SelectNetscapeFont( pDC->GetSafeHdc(), GetTextAttr(), pMyFont );
					if (pMyFont) {
						SetWidgetFont(pDC->GetSafeHdc(), m_pWidget->m_hWnd);
						GetElement()->baseline -= (pMyFont->GetHeight() / 2 + pMyFont->GetDescent()) / 2;
						VOID2CX(GetContext(), CPaneCX)->ReleaseNetscapeFont( pDC->GetSafeHdc(), pMyFont );
						m_pWidget->ReleaseDC(pDC);
					}
					else {
						m_pWidget->ReleaseDC(pDC);
						DestroyWidget();
						return;
					}
				}
			}
			else	{
				//	No widget, tell layout nothing.
				GetElement()->width = 0;
				GetElement()->height = 0;
				GetElement()->baseline = 0;
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Do some printing specific stuff here...
			GetElement()->width = 0;
			GetElement()->height = 0;
			GetElement()->baseline = 0;
		}
	}
}

//	Copy the current data out of the form element back into the
//		layout struct.
void CFormReadOnly::UpdateCurrentData()
{
	//	There is literally no data to update for this element type right now.
	//	Perhaps one day....
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Need a widget.
			if(m_pWidget)	{
			}
			else	{
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Printing/metafile
		}
	}
}

HWND CFormReadOnly::GetRaw()
{
    return(m_pWidget ? m_pWidget->m_hWnd : NULL);
}
