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

#include "fmfile.h"
#include "odctrl.h"
#include "intl_csi.h"

//	This file is dedicated to form type select one elements
//		otherwise known as list boxes on windows and
//		their implementation as requried by the XP layout
//		library.

//	Construction simply clears all members.
CFormFile::CFormFile()
{
	//	No widgets yet.
	m_pWidget = NULL;
	m_pBrowse = NULL;
	m_pWidgetOverride = NULL;
}

//	Destruction cleans out all members.
CFormFile::~CFormFile()
{
}

//	How to set the LO form element.
//	This may change during the lifetime of an instance, so use this
//		to update all referencing values.
void CFormFile::SetElement(LO_FormElementStruct *pFormElement)
{
	//	Call the base.
	CFormElement::SetElement(pFormElement);

	//	Let the widget know the element.
	if(m_pWidget)	{
		m_pWidget->SetContext(GetContext() ? GetContext()->GetContext() : NULL, GetElement());
	}

	//	Update the widget if present for correct callbacks.
	if(m_pBrowse)	{
		m_pBrowse->RegisterForm(GetElement());
	}
}

//	Set the owning context.
//	Use this to determine what context we live in and how we should
//		represent ourself (DC or window).
void CFormFile::SetContext(CAbstractCX *pCX)
{
	//	Call the base.
	CFormElement::SetContext(pCX);

	//	Let the widget know the context.
	if(m_pWidget)	{
		m_pWidget->SetContext(GetContext() ? GetContext()->GetContext() : NULL, GetElement());
	}

	//	Update the widget if present for correct callbacks.
	if(m_pBrowse)	{
		m_pBrowse->RegisterContext(GetContext() ? GetContext()->GetContext() : NULL);
	}
}

//	Display the form element given the particular context we are in.
//	Possibly only use a DC for representation, or have the
//		window move.
void CFormFile::DisplayFormElement(LTRB& Rect)
{
	//	Display only has meaning if our context is a device context.
	if(GetContext() && GetContext()->IsDCContext())	{
		//	Further, need to detect how we're going to be drawing ourselves.
		if(GetContext()->IsPureDCContext())	{
			//	Only works from a DC, needs a GDI drawing representation.
			CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
		}
		else if(GetContext()->IsWindowContext())	{
			//	Is a window, needs a widget/window representation.
			MoveWindow(m_pWidget->m_hWnd, Rect.left, Rect.top + EDIT_SPACE / 2);
			CRect crWidget;
			m_pWidget->GetWindowRect(crWidget);
			MoveWindow(m_pBrowse->m_hWnd, Rect.left + 2 + (crWidget.right - crWidget.left), Rect.top + 1);
		}
		else	{
			//	Is undefined....
			ASSERT(0);
		}
	}

	//	CALL THE BASE, TWICE!
	//	Fake out the base class by dynamically switching
	//		what is reported with GetRaw().
	CFormElement::DisplayFormElement(Rect);
	if(m_pBrowse)	{
		m_pWidgetOverride = m_pBrowse;
		CFormElement::DisplayFormElement(Rect);
		m_pWidgetOverride = NULL;
	}
}

//	Destroy the widget (window) implemenation of the form.
void CFormFile::DestroyWidget()
{
	//	Destroy any widgets around.
	if(m_pWidget)	{
		m_pWidget->DestroyWindow();
		delete m_pWidget;
		m_pWidget = NULL;
	}

	if(m_pBrowse)	{
		m_pBrowse->DestroyWindow();
		delete m_pBrowse;
		m_pBrowse = NULL;
	}
}

//	Create the widget (window) implementation of the form
//		but DO NOT DISPLAY.
void CFormFile::CreateWidget()
{
	//	Detect context type.
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext() && VOID2CX(GetContext(), CPaneCX)->GetPane())	{
			//	Widget representation.
			//	Text entry field first.
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

			//	Decide the style of the widget.
			DWORD dwStyle = WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL;

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

			//	Next, size the widget.
			CDC *pDC = m_pWidget->GetDC();
			if(pDC)	{
				CyaFont	*pMyFont;
				LO_TextAttr *text_attr = GetElement()->text_attr;
				VOID2CX(GetContext(), CWinCX)->SelectNetscapeFont( pDC->GetSafeHdc(), GetTextAttr(), pMyFont );
				//	Text metrics won't work unless we select the font.
				if(pMyFont == NULL)	{
					m_pWidget->ReleaseDC(pDC);
					DestroyWidget();
					return;
				}
				else {
					//	Default length is 20
					int32 lLength = 20;
					SetWidgetFont(pDC->GetSafeHdc(), m_pWidget->m_hWnd);
					text_attr->FE_Data = pMyFont;

					//	Figure up width and height we would like.
//					int32 lWidgetWidth = tm.tmMaxCharWidth;
//					int32 lWidgetHeight = tm.tmHeight + tm.tmHeight / 2;
					int32 lWidgetWidth = pMyFont->GetMaxWidth();
					int32 lWidgetHeight = pMyFont->GetHeight() + pMyFont->GetHeight() / 2;

					//	See if we can measure the default text, and/or
					//		set up the size and size limits.
					if(GetElementTextData())	{
						if(GetElementTextData()->size > 0)	{
							//	Use provided size.
							lLength = GetElementTextData()->size;

							//	Use average width.
							lWidgetWidth += lLength * pMyFont->GetMeanWidth();
						}
						else if(GetElementTextData()->default_text)	{
							//	Use length of text, exactly.
							char *pDefault = (char *)GetElementTextData()->default_text;

							lLength = XP_STRLEN(pDefault);
	//						CSize csz = CIntlWin::GetTextExtent(0, pDC->GetSafeHdc(), pDefault, CASTINT(lLength));

							SIZE csz;
							CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
								INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
								 pDC->GetSafeHdc(), pDefault, CASTINT(lLength), &csz);

							//	Use this width.
							lWidgetWidth += csz.cx;
						}

						//	Hold to the maximum size limit, and set it.
						if(GetElementTextData()->max_size >= 0)	{
							m_pWidget->LimitText(CASTINT(GetElementTextData()->max_size));
						}
					}

					//	Restore old font selection.
					VOID2CX(GetContext(), CWinCX)->ReleaseNetscapeFont( pDC->GetSafeHdc(), pMyFont );
					m_pWidget->MoveWindow(1, 1, CASTINT(lWidgetWidth), CASTINT(lWidgetHeight), FALSE);
				}
				m_pWidget->ReleaseDC(pDC);
			}
			else	{
				DestroyWidget();
				return;
			}

			//	2nd widget coming up, the browse button.
			m_pBrowse = new CODNetscapeButton(GetContext()->GetContext(), GetElement());
			if(m_pBrowse == NULL)	{
				DestroyWidget();
				return;
			}

			//	Create with a bad size and hidden.
			dwStyle = WS_CHILD | BS_PUSHBUTTON;
			bCreate = m_pBrowse->Create("",
				dwStyle,
				CRect(1, 1, 0, 0),
				CWnd::FromHandle(VOID2CX(GetContext(), CPaneCX)->GetPane()),
				GetDynamicControlID());
			if(!bCreate)	{
				delete m_pBrowse;
				m_pBrowse = NULL;
				DestroyWidget();
				return;
			}

			//	Font in the button.
			//	Measure what the size of the button should be.
			pDC = m_pBrowse->GetDC();
			if(pDC)	{
				CyaFont	*pMyFont;
				VOID2CX(GetContext(), CWinCX)->SelectNetscapeFont( pDC->GetSafeHdc(), GetTextAttr(), pMyFont );

				if (pMyFont) {
					int32 lWidgetWidth = 0;
					int32 lWidgetHeight = 0;


					SetWidgetFont(pDC->GetSafeHdc(), m_pWidget->m_hWnd);
					//	Determine caption text.
    				char *pCaption = szLoadString(IDS_BROWSE_BUTTON);
					if(GetElementMinimalData() && GetElementMinimalData()->value)	{
						pCaption = (char *)GetElementMinimalData()->value;
					}

					SIZE csz;
					CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
						INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
						 pDC->GetSafeHdc(), pCaption, XP_STRLEN(pCaption), &csz);
	//				csz = CIntlWin::GetTextExtent(0, pDC->GetSafeHdc(), pCaption, XP_STRLEN(pCaption));

					lWidgetWidth = 3 * csz.cx / 2;
					lWidgetHeight = 3 * csz.cy / 2;

					//	Done measureing.
					VOID2CX(GetContext(), CWinCX)->ReleaseNetscapeFont( pDC->GetSafeHdc(), pMyFont );
					m_pBrowse->MoveWindow(1, 1, CASTINT(lWidgetWidth), CASTINT(lWidgetHeight), FALSE);
					m_pBrowse->ReleaseDC(pDC);
				}
				else {
					m_pBrowse->ReleaseDC(pDC);
					DestroyWidget();
					return;
				}

				//	Move it.

				//	Tell it what edit control is associated.
				m_pBrowse->RegisterEdit(m_pWidget);
			}
			else	{
				DestroyWidget();
				return;
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	DC representation.
		}
	}
}

//	Copy the current data out of the layout struct into the form
//		widget, or mark that you should represent using the current data.
void CFormFile::UseCurrentData()
{
	//	Detect context type and do the right thing.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			if(m_pWidget)	{
				//	Determine the current text to set.
				char *pCurrent = "";
				if(GetElementTextData() && GetElementTextData()->current_text)	{
					pCurrent = (char *)GetElementTextData()->current_text;
				}
				// We have to SetContext to the widget before we SetWindowText
				// Otherwise, the widget don't know what csid the text is.
				m_pWidget->SetContext(GetContext()->GetContext(), GetElement());

				m_pWidget->SetWindowText(pCurrent);
			}

			if(m_pBrowse)	{
				//	Set the button caption.
				char *pCaption = szLoadString(IDS_BROWSE_BUTTON);
				if(GetElementMinimalData() && GetElementMinimalData()->value)	{
					pCaption = (char *)GetElementMinimalData()->value;
				}
				m_pBrowse->SetWindowText(pCaption);
			}
		}
		else if(GetContext()->IsPureDCContext())	{
		}
	}
}

//	Copy the default data out of the layout struct into the form
//		widget, or mark that you should represent using the default data.
void CFormFile::UseDefaultData()
{
	//	Detect context type and do the right thing.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			if(m_pWidget)	{
				//	Determine the default text to set.
				char *pDefault = "";
				if(GetElementTextData() && GetElementTextData()->default_text)	{
					pDefault = (char *)GetElementTextData()->default_text;
				}
				// We have to SetContext to the widget before we SetWindowText
				// Otherwise, the widget don't know what csid the text is.
				m_pWidget->SetContext(GetContext()->GetContext(), GetElement());

				m_pWidget->SetWindowText(pDefault);
			}

			if(m_pBrowse)	{
				//	Set the button caption.
				char *pCaption = szLoadString(IDS_BROWSE_BUTTON);
				if(GetElementMinimalData() && GetElementMinimalData()->value)	{
					pCaption = (char *)GetElementMinimalData()->value;
				}
				m_pBrowse->SetWindowText(pCaption);
			}
		}
		else if(GetContext()->IsPureDCContext())	{
		}
	}
}

//	Fill in the layout size information in the layout struct regarding
//		the dimensions of the widget.
void CFormFile::FillSizeInfo()
{
	//	Detect context type and do the right thing.
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext())	{
			if(m_pWidget)	{
				//	Calculate the size we need.
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

				//	Now for the second part.
				//	Add the width of browse button and take the max height.
				//	Whichever height we take, controls the baseline.
				CRect crBrowse;
				m_pBrowse->GetWindowRect(crBrowse);

				//	This is a recreation of csz.cy in CreateWidget.
				int32 csz_cy = 2 * crBrowse.Height() / 3;

				//	Munge return coordinates a little for
				//		layout.  We have to know how to reverse
				//		these in Display.
				GetElement()->width += crBrowse.Width() + 2;
				GetElement()->border_horiz_space = 1;
				if(crBrowse.Height() >= GetElement()->height)	{
					GetElement()->height = crBrowse.Height();
					GetElement()->baseline = crBrowse.Height() - csz_cy / 4 - 1;
				}
			}
			else	{
				//	Nope.
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
void CFormFile::UpdateCurrentData()
{
	//	Detect context type and do the right thing.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			if(m_pWidget && ::IsWindow(m_pWidget->GetSafeHwnd()) && GetElementTextData())	{
				//	Free off any current text.
				if(GetElementTextData()->current_text)	{
					XP_FREE(GetElementTextData()->current_text);
					GetElementTextData()->current_text = NULL;
				}

				//	See if we've got anything.
				int32 lLength = m_pWidget->GetWindowTextLength();
				if(lLength > 0)	{
					GetElementTextData()->current_text = (XP_Block)XP_ALLOC(lLength + 1);
					if(GetElementTextData()->current_text)	{
						m_pWidget->GetWindowText((char *)GetElementTextData()->current_text, CASTINT(lLength + 1));
					}
				}
			}
		}
		else if(GetContext()->IsPureDCContext())	{
		}
	}
}

HWND CFormFile::GetRaw()
{
	//	We may be returning an overriding value instead of our main
	//		widget.
	if(m_pWidgetOverride)	{
		return(m_pWidgetOverride->m_hWnd);
	}

	//	Return only our main widget here (the interesting one).
	//	The edit field, not the button.
	return(m_pWidget ? m_pWidget->m_hWnd : NULL);
}

CWnd *CFormFile::GetSecondaryWidget()
{
	//	Return the button widget so that click events can be 
	//	processed correctly.
	return(m_pBrowse);
}

