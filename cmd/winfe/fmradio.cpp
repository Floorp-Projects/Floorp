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

#include "fmradio.h"

//	This file is dedicated to form type radio elements
//		otherwise known as list boxes on windows and
//		their implementation as requried by the XP layout
//		library.

//	Construction simply clears all members.
CFormRadio::CFormRadio()
{
	//	No widget.
	m_pWidget = NULL;
}

//	Destruction cleans out all members.
CFormRadio::~CFormRadio()
{
}

//	How to set the LO form element.
//	This may change during the lifetime of an instance, so use this
//		to update all referencing values.
void CFormRadio::SetElement(LO_FormElementStruct *pFormElement)
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
void CFormRadio::SetContext(CAbstractCX *pCX)
{
	//	Call the base.
	CFormElement::SetContext(pCX);

	//	Update the widget if present for correct callbacks.
	if(m_pWidget)	{
		m_pWidget->RegisterContext(GetContext() ? GetContext()->GetContext() : NULL);
	}
}

//	Display the form element given the particular context we are in.
//	Possibly only use a DC for representation, or have the
//		window move.
void CFormRadio::DisplayFormElement(LTRB& Rect)
{
	//	Display only has meaning if our context is a device context.
	if(GetContext() && GetContext()->IsDCContext())	{
		//	Further, need to detect how we're going to be drawing ourselves.
		if(GetContext()->IsPureDCContext())	{
			//	Only works from a DC, needs a GDI drawing representation.
			CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
            HDC pDC = pDCCX->GetContextDC();
            if(pDC && Rect.Width() && Rect.Height()) {
                //  Adjust the rect some (reverse what we did earlier for layout).
                RECT crRect;
				::SetRect(&crRect, CASTINT(Rect.left), CASTINT(Rect.top),
                    CASTINT(Rect.right), CASTINT(Rect.bottom));
                ::InflateRect(&crRect, CASTINT(-1 * pDCCX->Pix2TwipsX(BUTTON_FUDGE) / 2),
                    CASTINT(-1 * pDCCX->Pix2TwipsY(BUTTON_FUDGE) / 2));

                //  Figure out what type we're about to draw, and its current state.
                BOOL bRadio = FALSE;    //  Radio or checkbox.
                BOOL bChecked = FALSE;  //  State, checked or not.

			    if(GetElementMinimalData())	{
				    switch(GetElementMinimalData()->type)	{
				    case FORM_TYPE_RADIO:
                        bRadio = TRUE;
					    break;
				    case FORM_TYPE_CHECKBOX:
                        bRadio = FALSE;
					    break;
				    default:	//	Unknown!
					    ASSERT(0);
					    return;
				    }
			    }

                if(GetElementToggleData())  {
                    bChecked = GetElementToggleData()->toggled ? TRUE : FALSE;
                }

                //  Are we drawing a circle or a square?
                if(bRadio)  {
                    TRY {
                        //  Circle.
                        //  Select the outer pen.
                        HPEN hpOuter = ::CreatePen(PS_SOLID, CASTINT(pDCCX->Pix2TwipsY(1)), pDCCX->ResolveLightLineColor());
                        HPEN hOldPen = (HPEN)::SelectObject(pDC, hpOuter);
                        if(hOldPen) {
                            //  Draw an ellipse.
                            ::Ellipse(pDC, crRect.left, crRect.top, crRect.right, crRect.bottom);

                            //  Select the inner pen.
                            HPEN hpInner = ::CreatePen(PS_SOLID, CASTINT(pDCCX->Pix2TwipsY(1)), pDCCX->ResolveDarkLineColor());
                            ::SelectObject(pDC, hpInner);
                            ::InflateRect(&crRect, CASTINT(-1 * pDCCX->Pix2TwipsX(1)), CASTINT(-1 * pDCCX->Pix2TwipsY(1)));
                            ::Ellipse(pDC, crRect.left, crRect.top, crRect.right, crRect.bottom);

                            //  Are we drawing a circle in the radio button (checked?)
                            if(bChecked)    {
                                HPEN hpBlack = CreatePen(PS_SOLID, CASTINT(pDCCX->Pix2TwipsY(1)), RGB(0, 0, 0));
                                ::SelectObject(pDC, hpBlack);

                                //  Get the brush with which to fill the ellipse this time around.
                                HBRUSH hbBlack = ::CreateSolidBrush(RGB(0, 0, 0));
                                HBRUSH hOldBrush = (HBRUSH)::SelectObject(pDC, hbBlack);
                                ::InflateRect(&crRect, CASTINT(-1 * pDCCX->Pix2TwipsX(3)), CASTINT(-1 * pDCCX->Pix2TwipsY(3)));
	                            ::Ellipse(pDC, crRect.left, crRect.top, crRect.right, crRect.bottom);

                                //  Restore the brush.
                                if(hOldBrush)   {
                                    ::SelectObject(pDC, hOldBrush);
                                }
								VERIFY(::DeleteObject( hbBlack ));
								VERIFY(::DeleteObject( hpBlack ));
                            }

                            //  Restore original pen.
                            ::SelectObject(pDC, hOldPen);
							VERIFY(::DeleteObject( hpOuter ));
							VERIFY(::DeleteObject( hpInner ));

                        }
                    }
                    CATCH(CException, e)    {
                        //  Caught some pen/brush exception, do nothing special.
                    }
                    END_CATCH
                }
                else    {
                    //  Square.
                    pDCCX->Display3DBox(LTRB(crRect.left, crRect.top, crRect.right, crRect.bottom),
                        pDCCX->ResolveLightLineColor(), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));

                    //  Are we checking it?
                    if(bChecked)    {
                        ::InflateRect(&crRect, CASTINT(-1 * pDCCX->Pix2TwipsX(2)), CASTINT(-1 * pDCCX->Pix2TwipsY(2)));

                        TRY {
                            HPEN hpBlack = ::CreatePen(PS_SOLID, CASTINT(pDCCX->Pix2TwipsY(2)), RGB(0, 0, 0));
                            HPEN hOldPen = (HPEN)::SelectObject(pDC, hpBlack);
                            if(hOldPen) {
                                //  Draw a check.
								int width = crRect.right - crRect.left;
								int height = crRect.bottom - crRect.top;

                                ::MoveToEx(pDC, crRect.left + width / 4, crRect.top + height / 2, NULL);
                                ::LineTo(pDC, crRect.left + width / 3, crRect.top + 3 * height / 4);
                                ::LineTo(pDC, crRect.left + 3 * width / 4, crRect.top + height / 4);

                                //  Restore old pen.
                                ::SelectObject(pDC, hOldPen);
								VERIFY(::DeleteObject(hpBlack));
                            }
                        }
                        CATCH(CException, e)    {
                            //  Caught some pen exeption, do nothing special here.
                        }
                        END_CATCH
                    }
                }

                pDCCX->ReleaseContextDC(pDC);
            }
		}
		else if(GetContext()->IsWindowContext())	{
			//	Is a window, needs a widget/window representation.
			MoveWindow(m_pWidget->m_hWnd, Rect.left + BUTTON_FUDGE / 2, 
								Rect.top + BUTTON_FUDGE / 2);
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
void CFormRadio::DestroyWidget()
{
	if(m_pWidget)	{
		m_pWidget->DestroyWindow();
		delete m_pWidget;
		m_pWidget = NULL;
	}
}

//	Create the widget (window) implementation of the form
//		but DO NOT DISPLAY.
void CFormRadio::CreateWidget()
{
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext() && VOID2CX(GetContext(), CPaneCX)->GetPane())	{
			//	For a window context, allocate a widget.
			ASSERT(m_pWidget == NULL);
			m_pWidget = new CNetscapeButton(GetContext()->GetContext(), GetElement());
			if(m_pWidget == NULL)	{
				return;
			}

			//	Figure the window style.
			DWORD dwStyle = WS_CHILD;
			if(GetElementMinimalData())	{
				switch(GetElementMinimalData()->type)	{
				case FORM_TYPE_RADIO:
					dwStyle |= BS_RADIOBUTTON;  
					break;
				case FORM_TYPE_CHECKBOX:
					dwStyle |= BS_CHECKBOX;		// bug 49682 removed AUTO, see button.cpp click()
					break;
				default:	//	Unknown!
					ASSERT(0);
					delete m_pWidget;
					m_pWidget = NULL;
					return;
				}
			}
			else	{
				//	Must know the type.
				delete m_pWidget;
				m_pWidget = NULL;
				return;
			}

			BOOL bCreate = m_pWidget->Create(NULL,
				dwStyle,
				CRect(0, 0, 1, 1),
				CWnd::FromHandle(VOID2CX(GetContext(), CPaneCX)->GetPane()),
				GetDynamicControlID());
			if(!bCreate)	{
				delete m_pWidget;
				m_pWidget = NULL;
				return;
			}

			//	Size the thing.
			//	Buttons are 7 x 7 in dialog units.
			//	Perform a conversion to pixels.
			LONG lUnits = GetDialogBaseUnits();
			int32 lWidth = 7 * LOWORD(lUnits) / 4;
			int32 lHeight = 7 * HIWORD(lUnits) / 8;

			//	Move it.
			m_pWidget->MoveWindow(1, 1, CASTINT(lWidth), CASTINT(lHeight), FALSE);
		}
		else if(GetContext()->IsPureDCContext())	{
		}
	}
}

//	Copy the current data out of the layout struct into the form
//		widget, or mark that you should represent using the current data.
void CFormRadio::UseCurrentData()
{
	//	Detect context type.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Only continue if there's a widget at all.
			if(m_pWidget)	{
				int iChecked = 0;	//	Sad default.
				if(GetElementToggleData())	{
					iChecked = GetElementToggleData()->toggled ? 1 : 0;
				}

				//	Update the widget.
                if(m_pWidget->GetCheck() != iChecked)   {
				    m_pWidget->SetCheck(iChecked);
                }
			}
		}
		else if(GetContext()->IsPureDCContext())	{
            //  Current data already reflected in toggle data.
		}
	}
}

//	Copy the default data out of the layout struct into the form
//		widget, or mark that you should represent using the default data.
void CFormRadio::UseDefaultData()
{
	//	Detect context type.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Only continue if there's a widget at all.
			if(m_pWidget)	{
				int iChecked = 0;	//	Sad default.
				if(GetElementToggleData())	{
					iChecked = GetElementToggleData()->default_toggle ? 1 : 0;
				}

				//	Update the widget.
				m_pWidget->SetCheck(iChecked);
			}
		}
		else if(GetContext()->IsPureDCContext())	{
            //  Set current to default.
            if(GetElementToggleData())  {
                GetElementToggleData()->toggled = GetElementToggleData()->default_toggle;
            }
		}
	}
}

//	Fill in the layout size information in the layout struct regarding
//		the dimensions of the widget.
void CFormRadio::FillSizeInfo()
{
	//	Detect context type.
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext())	{
			//	Only continue if there's a widget at all.
			if(m_pWidget)	{
				//	Determine out window position.
				CRect crWidget;
				m_pWidget->GetWindowRect(crWidget);

				//	Munge it for layout.
				//	We'll need to know how to undo this
				//		in the display routine.
				GetElement()->width = crWidget.Width() + BUTTON_FUDGE;
				GetElement()->height = crWidget.Height() + BUTTON_FUDGE;
				GetElement()->baseline = GetElement()->height - (BUTTON_FUDGE - 1);
				GetElement()->border_vert_space = BUTTON_FUDGE/2;
				GetElement()->border_horiz_space = BUTTON_FUDGE/2;
			}
			else	{
				GetElement()->width = 0;
				GetElement()->height = 0;
				GetElement()->baseline = 0;
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Do some printing specific stuff here...
            CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);

			//	Size the thing.
			//	Buttons are 7 x 7 in dialog units.
			//	Perform a conversion to twips/whatever.
			LONG lUnits = GetDialogBaseUnits();
			int32 lWidth = 7 * LOWORD(lUnits) / 4;
			int32 lHeight = 7 * HIWORD(lUnits) / 8;
            int32 lButtonFudge = pDCCX->Pix2TwipsY(BUTTON_FUDGE);

            lWidth = pDCCX->Pix2TwipsX(lWidth);
            lHeight = pDCCX->Pix2TwipsX(lHeight);

            GetElement()->width = lWidth + lButtonFudge;
            GetElement()->height = lHeight + lButtonFudge;
            GetElement()->baseline = GetElement()->height - (lButtonFudge - pDCCX->Pix2TwipsY(1));
        }
	}
}

//	Copy the current data out of the form element back into the
//		layout struct.
void CFormRadio::UpdateCurrentData()
{
	//	Detect context type.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Only continue if there's a widget at all.
			if(m_pWidget && ::IsWindow(m_pWidget->GetSafeHwnd()))	{
				if(GetElementToggleData())	{
					//	Update layout data as BOOL.
					GetElementToggleData()->toggled = m_pWidget->GetCheck() ? TRUE : FALSE;
				}
			}
		}
		else if(GetContext()->IsPureDCContext())	{
            //  Nothing to update.
		}
	}
}

HWND CFormRadio::GetRaw()
{
    return(m_pWidget ? m_pWidget->m_hWnd : NULL);
}

