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
#include "fmbutton.h"
#include "intl_csi.h"
#include "odctrl.h"

//	This file is dedicated to form type button elements
//		otherwise known as a button on windows and
//		their implementation as requried by the XP layout
//		library.

//	Construction simply clears all members.
CFormButton::CFormButton()
{
	//	No widget yet.
	m_pWidget = NULL;
}

//	Destruction cleans out all members.
CFormButton::~CFormButton()
{
}

//	How to set the LO form element.
//	This may change during the lifetime of an instance, so use this
//		to update all referencing values.
void CFormButton::SetElement(LO_FormElementStruct *pFormElement)
{
	//	Call the base.
	CFormElement::SetElement(pFormElement);

	//	Update the widget if present for correct callbacks.
	if(m_pWidget)	{
		m_pWidget->RegisterForm(GetElement());
	}
}

//	Set the owning context.
//	Use this to determine what context we live in and how we should
//		represent ourself (DC or window).
void CFormButton::SetContext(CAbstractCX *pCX)
{
	//	Call the base.
	CFormElement::SetContext(pCX);

	//	Update the widget if present for correct callbacks.
	if(m_pWidget)	{
		m_pWidget->RegisterContext(pCX->GetContext());
	}
}

//	Display the form element given the particular context we are in.
//	Possibly only use a DC for representation, or have the
//		window move.
void CFormButton::DisplayFormElement(LTRB& Rect)
{
	//	Display only has meaning if our context is a device context.
	if(GetContext() && GetContext()->IsDCContext())	{
		//	Further, need to detect how we're going to be drawing ourselves.
		if(GetContext()->IsPureDCContext())	{
			//	Only works from a DC, needs a GDI drawing representation.
			CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
            HDC pDC = pDCCX->GetContextDC();
            if(pDC && Rect.Width() && Rect.Height()) {
                pDCCX->Display3DBox(Rect, pDCCX->ResolveDarkLineColor(), pDCCX->ResolveLightLineColor(), pDCCX->Pix2TwipsY(2));

                //  Draw the text inside the button.
                //  This is refiguring the width and height of the text.
			    char *pData = NULL;
			    if(GetElementMinimalData())	{
				    pData = (char *)GetElementMinimalData()->value;
			    }
			    if(pData)	{
                    //  Decide text color.
                    COLORREF rgbUseMe;
					CyaFont	*pMyFont;
                    rgbUseMe = pDCCX->ResolveTextColor(NULL);

                    //  Output text.
                    int iBkMode = ::SetBkMode(pDC, TRANSPARENT);
                    COLORREF rgbTxtClr = ::SetTextColor(pDC, rgbUseMe);
					pDCCX->SelectNetscapeFont( pDC,  GetTextAttr(), pMyFont );
					if(pMyFont != NULL)	{
						SIZE csz;
						CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
							INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
							pDC, pData, XP_STRLEN(pData), &csz);
						// Center text.
						int32 lWidth = Rect.Width() - csz.cx;
						int32 lHeight = Rect.Height() -csz.cy;
						int32 lX = Rect.left + lWidth / 2;
						int32 lY = Rect.top + (lHeight / 2);
						CIntlWin::TextOutWithCyaFont(
							pMyFont,
							INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
							pDC, 
							CASTINT(lX),
							CASTINT(lY),
							pData,
							XP_STRLEN(pData));

							::SetTextColor(pDC, rgbTxtClr);
							::SetBkMode(pDC, iBkMode);
						pDCCX->ReleaseNetscapeFont( pDC, pMyFont );
					}
				}
                pDCCX->ReleaseContextDC(pDC);
            }
        }
		else if(GetContext()->IsWindowContext() && GetRaw())	{
			MoveWindow(GetRaw(), Rect.left, Rect.top);
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
void CFormButton::DestroyWidget()
{
	//	Free off the widget if present.
	if(m_pWidget)	{
        m_pWidget->DestroyWindow();
        delete m_pWidget;
		m_pWidget = NULL;
	}
}

//	Create the widget (window) implementation of the form
//		but DO NOT DISPLAY.
void CFormButton::CreateWidget()
{
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext() && VOID2CX(GetContext(), CPaneCX)->GetPane())	{
			//	Widget representation, create one.
			ASSERT(m_pWidget == NULL);
            m_pWidget = new CODNetscapeButton(GetContext()->GetContext(), GetElement());
			if(m_pWidget == NULL)	{
				return;
			}
            
			//	Create the widget, initially with a bad size and hidden.
			BOOL bCreate = m_pWidget->Create("",
				WS_CHILD | BS_OWNERDRAW,
				CRect(1, 1, 0, 0),
                CWnd::FromHandle(VOID2CX(GetContext(), CPaneCX)->GetPane()),
				GetDynamicControlID());
            if(!bCreate)	{
                DestroyWidget();
                return;
            }

			//	Set the font of the widget.
			CyaFont	*pMyFont;
			LO_TextAttr *text_attr = GetElement()->text_attr;
			CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);

			//	Measure what the size of the button should be.
			HDC hDC = NULL;
			if(GetRaw()) {
			    hDC = ::GetDC(GetRaw());
			}
			if(hDC)	{
				SIZE csz;
				int32 lWidgetWidth = 0;
				int32 lWidgetHeight = 0;
				pDCCX->SelectNetscapeFont( hDC, GetTextAttr(), pMyFont );
				if(pMyFont == NULL)	{
                    ::ReleaseDC(GetRaw(), hDC);
                    DestroyWidget();
                    return;
				}
				else {

					//	Must select the font while measuring.
					SetWidgetFont(hDC, GetRaw());
					text_attr->FE_Data = pMyFont;

					CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
						INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
						hDC, "hi", 2, &csz);
					//	Determine what data will be shown.
					char *pCaption = NULL;
					if(GetElementMinimalData())	{
						pCaption = (char *)GetElementMinimalData()->value;
					}

					//	Measure it if present.
					if(pCaption && *pCaption)	{
						CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
							INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
							hDC, pCaption, XP_STRLEN(pCaption), &csz);
					}

					lWidgetWidth = 3 * csz.cx / 2;
					lWidgetHeight = 3 * csz.cy / 2;

					if (GetElement()->width > lWidgetWidth) {
						lWidgetWidth = GetElement()->width;
					}
					if (GetElement()->height > lWidgetHeight) {
						lWidgetHeight = GetElement()->height;
					}
					//	Done measuring text.
					pDCCX->ReleaseNetscapeFont( hDC, pMyFont );

					//	Finally, size the widget to the width and height
					//		we have figured here.
					::MoveWindow(GetRaw(), 1, 1, CASTINT(lWidgetWidth), CASTINT(lWidgetHeight), FALSE);
				}
				::ReleaseDC(GetRaw(), hDC);
				hDC = NULL;
			}
			else	{
				//	No DC, no widget.
				DestroyWidget();
				return;
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Printing representation.
		}
	}
}

//	Copy the current data out of the layout struct into the form
//		widget, or mark that you should represent using the current data.
void CFormButton::UseCurrentData()
{
	//	Detect the context type for further action.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			if(m_pWidget)	{
				//	Just set it to the value if present.
				//	No real concept of current or default data in a button yet.
				char *pData = NULL;
				if(GetElementMinimalData())	{
					pData = (char *)GetElementMinimalData()->value;
				}
				if(pData && GetRaw())	{
					::SetWindowText(GetRaw(), pData);
				}
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Do some printing specific stuff here...
		}
	}
}

//	Copy the default data out of the layout struct into the form
//		widget, or mark that you should represent using the default data.
void CFormButton::UseDefaultData()
{
	//	Detect the context type for further action.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			if(m_pWidget)	{
				//	Just set it to the value if present.
				//	No real concept of current or default data in a button yet.
				char *pData = NULL;
				if(GetElementMinimalData())	{
					pData = (char *)GetElementMinimalData()->value;
				}
				if(pData && GetRaw())	{
					::SetWindowText(GetRaw(), pData);
				}
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Do some printing specific stuff here...
		}
	}
}

//	Fill in the layout size information in the layout struct regarding
//		the dimensions of the widget.
void CFormButton::FillSizeInfo()
{
	//	Detect the context type for further action.
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext())	{
			if(GetRaw())	{
				//	Determine our window position.
				CRect crWidget;
				::GetWindowRect(GetRaw(), crWidget);

				//	This is a recreation of csz.cy in CreateWidget.
				int32 csz_cy = 2 * crWidget.Height() / 3;

				//	Munge return coordinates a little for
				//		layout.
				GetElement()->width = crWidget.Width();
				GetElement()->height = crWidget.Height();
				GetElement()->baseline = crWidget.Height() - csz_cy / 4 - 1;
			}
			else	{
				//	Widget not present, give layout nothing.
				GetElement()->width = 0;
				GetElement()->height = 0;
				GetElement()->baseline = 0;
			}
		}
		else if(GetContext()->IsPureDCContext())	{
            CDCCX *pCX = VOID2CX(GetContext(), CDCCX);
            HDC pDC = pCX->GetAttribDC();

            //  Size the Button text.
            int32 lWidth = 0;
            int32 lHeight = 0;
            int32 lBaseline = 0;
            if(pDC) {
			    char *pData = NULL;
			    if(GetElementMinimalData())	{
				    pData = (char *)GetElementMinimalData()->value;
			    }
			    if(pData && *pData)	{
                    //  Size the text.
					CyaFont	*pMyFont;
					pCX->SelectNetscapeFont( pDC, GetTextAttr(), pMyFont );
					if (pMyFont) {
						SIZE csz;
						CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
							INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
							pDC, pData, XP_STRLEN(pData), &csz);

						//  Give some extra room.
						//  We'll need to know how to undo this in the display routine.
						lWidth = 3 * csz.cx / 2;
						lHeight = 3 * csz.cy / 2;

						// Check and see if we should use preset sizes
						if (GetElement()->width > lWidth) {
							lWidth = GetElement()->width;
						}
						if (GetElement()->height > lHeight) {
							lHeight = GetElement()->height;
						}

						lBaseline = lHeight - csz.cy / 4 - pCX->Pix2TwipsY(1);
						pCX->ReleaseNetscapeFont( pDC, pMyFont );
					}
                }

                //  Done with the DC.
                pCX->ReleaseContextDC(pDC);
            }

            //  Fill in the layout information.
	        GetElement()->width = lWidth;
	        GetElement()->height = lHeight;
            GetElement()->baseline = lBaseline;
		}
	}
}

//	Copy the current data out of the form element back into the
//		layout struct.
void CFormButton::UpdateCurrentData()
{
	//	There is literally no data to update for this element type right now.
	//	Perhaps one day....
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			if(m_pWidget)	{
			}
			else	{
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Do some printing specific stuff here...
		}
	}
}

HWND CFormButton::GetRaw()
{
    return(m_pWidget ? m_pWidget->m_hWnd : NULL);
}
