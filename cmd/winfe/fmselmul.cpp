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
#include "odctrl.h"

#include "fmselmul.h"
#include "intl_csi.h"

//	This file is dedicated to form type select mult elements
//		otherwise known as list boxes on windows and
//		their implementation as requried by the XP layout
//		library.

//	Construction simply clears all members.
CFormSelectMult::CFormSelectMult()
{
	//	No widget yet.
	m_pWidget = NULL;
}

//	Destruction cleans out all members.
CFormSelectMult::~CFormSelectMult()
{
}

//	How to set the LO form element.
//	This may change during the lifetime of an instance, so use this
//		to update all referencing values.
void CFormSelectMult::SetElement(LO_FormElementStruct *pFormElement)
{
	//	Call the base.
	CFormElement::SetElement(pFormElement);

	//	Have our widget update itself.
	if(m_pWidget)	{
		m_pWidget->SetContext(GetContext() ? GetContext()->GetContext() : NULL, GetElement());
	}
}

//	Set the owning context.
//	Use this to determine what context we live in and how we should
//		represent ourself (DC or window).
void CFormSelectMult::SetContext(CAbstractCX *pCX)
{
	//	Call the base.
	CFormElement::SetContext(pCX);

	//	Have the widget update if present.
	if(m_pWidget)	{
		m_pWidget->SetContext(GetContext() ? GetContext()->GetContext() : NULL, GetElement());
	}
}

//	Display the form element given the particular context we are in.
//	Possibly only use a DC for representation, or have the
//		window move.
void CFormSelectMult::DisplayFormElement(LTRB& Rect)
{
	//	Display only has meaning if our context is a device context.
	if(GetContext() && GetContext()->IsDCContext())	{
		//	Further, need to detect how we're going to be drawing ourselves.
		if(GetContext()->IsPureDCContext())	{
			//	Only works from a DC, needs a GDI drawing representation.
			CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
            LTRB r(Rect.left, Rect.top, Rect.right, Rect.bottom);

            //  Do something simple.
            pDCCX->Display3DBox(r, pDCCX->ResolveLightLineColor(), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
            r.Inflate(-1 * pDCCX->Twips2PixX(2), -1 * pDCCX->Pix2TwipsY(2));

            HDC pDC = pDCCX->GetContextDC();
            if(pDC) {
                if(GetElementSelectData())  {
                    int32 lTotal = GetElementSelectData()->option_cnt;
                    int32 lSize = GetElementSelectData()->size;
                    if(lSize < 1)   {
                        lSize = 1;
                    }

                    //  Draw the scroll bar on the right.
                    //  Adjust for size of scroller.
                    r.right -= pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth);
                    LTRB Scroller(r.right, r.top - pDCCX->Pix2TwipsY(2), r.right + pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth), r.bottom + pDCCX->Pix2TwipsY(2));
                    //  Display generic outline.
                    pDCCX->Display3DBox(Scroller, pDCCX->ResolveLightLineColor(), pDCCX->ResolveLightLineColor(), pDCCX->Pix2TwipsY(2));
                    //  Don't do if can't handle due to size limitations
                    if(Scroller.Height() > pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight) * 2)  {
                        POINT aPoints[3];

                        //  Display top button.
                        LTRB TButton(Scroller.left, Scroller.top, Scroller.right, Scroller.top + pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight));
                        pDCCX->Display3DBox(TButton, RGB(0, 0, 0), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
                        TButton.Inflate(-1 * pDCCX->Twips2PixX(2), -1 * pDCCX->Pix2TwipsY(2));
                        Scroller.top += pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight);

                        //  Arrow.
                        aPoints[0].x = CASTINT(TButton.left + TButton.Width() / 4);
                        aPoints[0].y = CASTINT(TButton.top + 2 * TButton.Height() / 3);
                        aPoints[1].x = CASTINT(TButton.left + TButton.Width() / 2);
                        aPoints[1].y = CASTINT(TButton.top + TButton.Height() / 3);
                        aPoints[2].x = CASTINT(TButton.left + 3 * TButton.Width() / 4);
                        aPoints[2].y = CASTINT(TButton.top + 2 * TButton.Height() / 3);
                        TRY {
                            HPEN cpBlack = ::CreatePen(PS_SOLID, 0, RGB(0, 0, 0));
                            HBRUSH cbBlack;
                            HBRUSH pOldBrush = NULL;
                            if(cbBlack = CreateSolidBrush(RGB(0, 0, 0)))  {
                                pOldBrush = (HBRUSH)::SelectObject(pDC, cbBlack);
                            }
                            HPEN pOldPen = (HPEN)::SelectObject(pDC, cpBlack);

                            int iOldMode = ::SetPolyFillMode(pDC, ALTERNATE);
                            ::Polygon(pDC, aPoints, 3);
                            ::SetPolyFillMode(pDC, iOldMode);

                            if(pOldPen) {
                                ::SelectObject(pDC, pOldPen);
                            }
                            if(pOldBrush)   {
                                ::SelectObject(pDC, pOldBrush);
                            }
							VERIFY(::DeleteObject(cpBlack));
							VERIFY(::DeleteObject(cbBlack));
                        }
                        CATCH(CException, e)    {
                            //  Can't create pen.
                        }
                        END_CATCH

                        //  Display bottom button.
                        LTRB BButton(Scroller.left, Scroller.bottom - pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight), Scroller.right, Scroller.bottom);
                        pDCCX->Display3DBox(BButton, RGB(0, 0, 0), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
                        BButton.Inflate(-1 * pDCCX->Twips2PixX(2), -1 * pDCCX->Pix2TwipsY(2));
                        Scroller.bottom -= pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight);

                        //  Arrow.
                        aPoints[0].x = CASTINT(BButton.left + BButton.Width() / 4);
                        aPoints[0].y = CASTINT(BButton.top + BButton.Height() / 3);
                        aPoints[1].x = CASTINT(BButton.left + BButton.Width() / 2);
                        aPoints[1].y = CASTINT(BButton.top + 2 * BButton.Height() / 3);
                        aPoints[2].x = CASTINT(BButton.left + 3 * BButton.Width() / 4);
                        aPoints[2].y = CASTINT(BButton.top + BButton.Height() / 3);
                        TRY {
                            HPEN cpBlack = ::CreatePen(PS_SOLID, 0, RGB(0, 0, 0));
                            HBRUSH cbBlack;
                            HBRUSH pOldBrush = NULL;
                            if(cbBlack = ::CreateSolidBrush(RGB(0, 0, 0)))  {
                                pOldBrush = (HBRUSH)::SelectObject(pDC, cbBlack);
                            }
                            HPEN pOldPen = (HPEN)::SelectObject(pDC, cpBlack);

                            int iOldMode = ::SetPolyFillMode(pDC, ALTERNATE);
                            ::Polygon(pDC, aPoints, 3);
                            ::SetPolyFillMode(pDC, iOldMode);

                            if(pOldPen) {
                                ::SelectObject(pDC, pOldPen);
                            }
                            if(pOldBrush)   {
                                ::SelectObject(pDC, pOldBrush);
                            }
							VERIFY(::DeleteObject(cpBlack));
							VERIFY(::DeleteObject(cbBlack));
                        }
                        CATCH(CException, e)    {
                            //  Can't create pen.
                        }
                        END_CATCH

                        //  Decide if drawing inner tumbtrack.
                        if(lSize < lTotal)  {
                            //  Size of thumbtrack is a percentage.
                            int32 lPercent = lSize * 100 / lTotal;
                            int32 lThumb = lPercent * Scroller.Height() / 100;
                            LTRB Thumb(Scroller.left, Scroller.top, Scroller.right, Scroller.top + lThumb);
                            pDCCX->Display3DBox(Thumb, RGB(0, 0, 0), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
                        }
                    }

                    //  Output the visible text.
					CyaFont	*pMyFont;
					VOID2CX(GetContext(), CWinCX)->SelectNetscapeFont( pDC, GetTextAttr(), pMyFont );
					if (pMyFont) {
						SetWidgetFont(pDC, m_pWidget->m_hWnd);
                        lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)GetElementSelectData()->options;
                        if(pOptionData) {
                            char *pCurrent = NULL;
                            int32 lCounter = 0;

                            //  Need to modify output coords a bit.
                            //  We'll move a bit to the right, and we'll clip a bit off the right.
                            //  Also, we'll figure up the height of each entry (or the space we'll have to draw each one),
                            //      and it's offset (center Y in each entry space).
                            RECT crDest;
							::SetRect(&crDest, CASTINT(r.left), CASTINT(r.top), CASTINT(r.right), CASTINT(r.bottom));
                            int32 lQuantumHeight = (crDest.bottom - crDest.top) / lSize;
                            int32 lHeightOffset = 0;
                            TEXTMETRIC tm;

                            crDest.left += pMyFont->GetMeanWidth();

                            //  Decide if we offset the text into the quantum height
                            //      for each entry.
                            lHeightOffset = (lQuantumHeight - pMyFont->GetHeight()) / 2;

                            //  We'll want the text to be transparent.
                            int iOldBk = ::SetBkMode(pDC, TRANSPARENT);

                            while(lCounter < GetElementSelectData()->option_cnt && lCounter < lSize)    {
                                //  Draw any selections that will be visible.
                                COLORREF rgbOldColor;
                                if(pOptionData[lCounter].selected)  {
                                    RECT crFill;
									::SetRect(&crFill, CASTINT(r.left), CASTINT(r.top + lQuantumHeight * lCounter), CASTINT(r.right), CASTINT(r.top + lQuantumHeight * (lCounter + 1)));
                                    HBRUSH cbFill;
									HBRUSH hOldBrush;
                                    if(cbFill = CreateSolidBrush(pDCCX->ResolveDarkLineColor()))  {
										hOldBrush = (HBRUSH)::SelectObject(pDC, cbFill);
                                        ::FillRect(pDC, &crFill, cbFill);
										::SelectObject(pDC, hOldBrush);
										VERIFY(::DeleteObject(hOldBrush));
                                    }

                                    //  Need to change text color to white.
                                    rgbOldColor = ::SetTextColor(pDC, RGB(255, 255, 255));
                                }

                                pCurrent = (char *)pOptionData[lCounter].text_value;
                                if(pCurrent)    {

									CIntlWin::ExtTextOut(
										((GetContext()->GetContext()) ?
											INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext()))
										 : 0) ,

                                        pDC, 
										crDest.left,
                                        CASTINT(crDest.top + lQuantumHeight * lCounter + lHeightOffset),
                                        ETO_CLIPPED,
                                        CRect(CASTINT(r.left), CASTINT(r.top), CASTINT(r.right), CASTINT(r.bottom)),
                                        pCurrent,
                                        XP_STRLEN(pCurrent),
                                        NULL);

                                }

                                //  Turn text color back if we were selected.
                                if(pOptionData[lCounter].selected)  {
                                    ::SetTextColor(pDC, rgbOldColor);
                                }

                                lCounter++;
                            }

                            //  Restore BK mode.
                            ::SetBkMode(pDC, iOldBk);
                        }

					pDCCX->ReleaseNetscapeFont( pDC, pMyFont );
					}
					else {
						m_pWidget->ReleaseDC(CDC::FromHandle(pDC));
						DestroyWidget();
					}
                }
                //  Release DC.
                pDCCX->ReleaseContextDC(pDC);
            }
		}
		else if(GetContext()->IsWindowContext())	{
			MoveWindow(m_pWidget->m_hWnd, Rect.left, Rect.top);
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
void CFormSelectMult::DestroyWidget()
{
	//	Get rid of the widget if present.
	if(m_pWidget)	{

		m_pWidget->DestroyWindow();
		delete m_pWidget;
		m_pWidget = NULL;
	}
}

//	Create the widget (window) implementation of the form
//		but DO NOT DISPLAY.
void CFormSelectMult::CreateWidget()
{
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext() && VOID2CX(GetContext(), CPaneCX)->GetPane())	{
			//	Allocate the widget.
			ASSERT(m_pWidget == NULL);
			m_pWidget = new CODMochaListBox();
			if(m_pWidget == NULL)	{
				return;
			}

			//	Inform the widget of the element and the context for callbacks.
			m_pWidget->SetContext(GetContext()->GetContext(), GetElement());

			//	Determine the style for the list box.
			DWORD dwStyle = WS_CHILD | WS_VSCROLL | WS_BORDER |
				LBS_DISABLENOSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED;
			if(GetElementSelectData()->multiple)	{
				dwStyle |= LBS_EXTENDEDSEL | LBS_MULTIPLESEL;
			}

			//	Create the widget hidden and with a bad size (will size it later).
			BOOL bCreate =
#ifdef XP_WIN16
				m_pWidget->Create(dwStyle, CRect(1, 1, 0, 0), 
					CWnd::FromHandle(VOID2CX(GetContext(), CPaneCX)->GetPane()), 
					GetDynamicControlID());
#else
				m_pWidget->CreateEx(WS_EX_CLIENTEDGE,
					_T("LISTBOX"),
					NULL,
					dwStyle,
					1, 1, 0, 0,
					VOID2CX(GetContext(), CPaneCX)->GetPane(),
					(HMENU)GetDynamicControlID(),
					NULL);
#endif
			if(!bCreate)	{
				delete m_pWidget;
				m_pWidget = NULL;
				return;
			}

			//	Next we need to correctly size the widget.
			//	First step in doing that is selecting the correct font.

			//	Prepare to measure text.
			CDC *pDC = m_pWidget->GetDC();
			CyaFont	*pMyFont;  // the font use for the widget.
			if(pDC)	{

				VOID2CX(GetContext(), CPaneCX)->SelectNetscapeFont( pDC->GetSafeHdc(), GetTextAttr(), pMyFont );
				int32 lWidgetWidth;
				int32 lWidgetHeight = 0;
				SIZE csz;
				if (pMyFont) {
					GetElement()->text_attr->FE_Data = pMyFont;
					SetWidgetFont(pDC->GetSafeHdc(), m_pWidget->m_hWnd);
					//	Select the font while measuring text, or will be off.
					//	Default if no other text.
					CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
						INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
						pDC->GetSafeHdc(), "hi", 2, &csz);
					lWidgetWidth = csz.cx;
					lWidgetHeight = 0;

					//	Count the max width by looping through all the entries.
					if(GetElementSelectData())	{
						lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)GetElementSelectData()->options;
						if(pOptionData)	{
							char *pCurrent = NULL;
							int32 lCounter = 0;
							while(lCounter < GetElementSelectData()->option_cnt)	{
								pCurrent = (char *)pOptionData[lCounter].text_value;
								if(pCurrent && *pCurrent)	{
									CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
											INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
											pDC->GetSafeHdc(), pCurrent, XP_STRLEN(pCurrent), &csz);

									if(csz.cx > lWidgetWidth)	{
										lWidgetWidth = csz.cx;
									}
								}

								//	Next, please.
								lCounter++;
							}
						}
					}
					lWidgetWidth += 2 * sysInfo.m_iScrollWidth;

					VOID2CX(GetContext(), CPaneCX)->ReleaseNetscapeFont( pDC->GetSafeHdc(), pMyFont );
					m_pWidget->ReleaseDC(pDC);
				}
				else {
					m_pWidget->ReleaseDC(pDC);
					DestroyWidget();
					return;
				}

				//	Figure the height we desire.
				//	NOTE:	This algorithm could use some work....
				lWidgetHeight = GetElementSelectData()->size;
				//	Catch minimal case.
				if(lWidgetHeight < 1)	{
					lWidgetHeight = 1;
				}
				lWidgetHeight *= csz.cy;
				lWidgetHeight += csz.cy / 2;
				if (GetElement()->width > lWidgetWidth) {
					lWidgetWidth = GetElement()->width;
				}
				if (GetElement()->height > lWidgetHeight) {
					lWidgetHeight = GetElement()->height;
				}
				//	Finally, size the widget to the width and height.
				//	Don't draw or display it.
				m_pWidget->MoveWindow(1, 1, CASTINT(lWidgetWidth), CASTINT(lWidgetHeight), FALSE);
			}
			else	{
				//	No DC, no widget.
				DestroyWidget();
				return;
			}
		}
		else if(GetContext()->IsPureDCContext())	{
		}
	}
}

//	Copy the current data out of the layout struct into the form
//		widget, or mark that you should represent using the current data.
void CFormSelectMult::UseCurrentData()
{
	//	Switch on context type for proper functionality.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Only continue if we've a widget.
			if(m_pWidget)	{
				//	Remove all previous entries.
				m_pWidget->ResetContent();

				//	Loop through and add all data to the element.
				LO_LockLayout();
				lo_FormElementSelectData * selectData = GetElementSelectData();
				if(selectData)	{
					lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)selectData->options;
					if(pOptionData)	{
						char *pCurrent = NULL;
						int32 lCurrent = 0;
						while(lCurrent < selectData->option_cnt)	{
							pCurrent = (char *)pOptionData[lCurrent].text_value;

							//	add it.
                            m_pWidget->AddString(pCurrent ? pCurrent : "");

							//	Should it be selected?
							if(pOptionData[lCurrent].selected)	{
								if(selectData->multiple)	{
									//	Multiple selections allowed.
									m_pWidget->SetSel(CASTINT(lCurrent));
								}
								else	{
									//	Single selections only.
									m_pWidget->SetCurSel(CASTINT(lCurrent));
								}
							}

							//	Next.
							lCurrent++;
						}

						//	Go to top of list box.
						if(lCurrent)	{
							m_pWidget->SetTopIndex(0);
						}
					}
				}
				LO_UnlockLayout();
			}
		}
		else if(GetContext()->IsPureDCContext())	{
            //  Whatever is current is current.
		}
	}
}

//	Copy the default data out of the layout struct into the form
//		widget, or mark that you should represent using the default data.
void CFormSelectMult::UseDefaultData()
{
	//	Switch on context type for proper functionality.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Only continue if we've a widget.
			if(m_pWidget)	{
				//	Remove all previous entries.
				m_pWidget->ResetContent();

				//	Loop through and add all data to the element.
				LO_LockLayout();
				lo_FormElementSelectData * selectData = GetElementSelectData();
				if(selectData)	{
					lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)selectData->options;
					if(pOptionData)	{
						char *pCurrent = NULL;
						int32 lCurrent = 0;
						while(lCurrent < selectData->option_cnt)	{
							pCurrent = (char *)pOptionData[lCurrent].text_value;

							//	add it.
                            m_pWidget->AddString(pCurrent ? pCurrent : "");

							//	Restore default selection.
							pOptionData[lCurrent].selected = pOptionData[lCurrent].def_selected;

							//	Should it be selected?
							if(pOptionData[lCurrent].selected)	{
								if(selectData->multiple)	{
									//	Multiple selections allowed.
									m_pWidget->SetSel(CASTINT(lCurrent));
								}
								else	{
									//	Single selections only.
									m_pWidget->SetCurSel(CASTINT(lCurrent));
								}
							}

							//	Next.
							lCurrent++;
						}

						//	Go to top of list box.
						if(lCurrent)	{
							m_pWidget->SetTopIndex(0);
						}
					}
				}
				LO_UnlockLayout();
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Loop through and set the current data to reflect the default data.
			if(GetElementSelectData())	{
				lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)GetElementSelectData()->options;
				if(pOptionData)	{
					int32 lCurrent = 0;
					while(lCurrent < GetElementSelectData()->option_cnt)	{
						//	Restore default selection.
						pOptionData[lCurrent].selected = pOptionData[lCurrent].def_selected;

                        //	Next.
						lCurrent++;
					}
                }
            }        
        }
	}
}

//	Fill in the layout size information in the layout struct regarding
//		the dimensions of the widget.
void CFormSelectMult::FillSizeInfo()
{
	//	Switch on context type for proper functionality.
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext())	{
			//	Only do this if we've got a widget at all.
			if(m_pWidget)	{
				//	Determine our current window position.
				CRect crRect;
				m_pWidget->GetWindowRect(crRect);

				//	Munge it a little for layout.
				//	We'll have to know how to reverse this
				//		effect in the display routine.
				GetElement()->width = crRect.Width();
				GetElement()->height = crRect.Height();

				int32 lLines = 1;
				if(GetElementSelectData() && GetElementSelectData()->size > 1)	{
					lLines = GetElementSelectData()->size;
				}
				int32 csz_cy = (2 * crRect.Height() / (2 * lLines + 1));	//	This is the recalculation of csz.cy in CreateWidget
				GetElement()->baseline = crRect.Height() - (csz_cy / 4);
			}
			else	{
				//	No widget, zero size us with Layout.
				GetElement()->width = 0;
				GetElement()->height = 0;
				GetElement()->baseline = 0;
			}
		}
		else if(GetContext()->IsPureDCContext())	{
            //  Need to figure the size of the select box.
            int32 lWidth = 0;
            int32 lHeight = 0;
            int32 lBaseline = 0;

            CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
			CyaFont	*pMyFont;
            HDC pDC = pDCCX->GetAttribDC();
            if(pDC) {
				pDCCX->SelectNetscapeFont( pDC, GetTextAttr(), pMyFont );
				if (pMyFont) {
                //  Select the font.
					SetWidgetFont(pDC, m_pWidget->m_hWnd);
					SIZE csz;
					CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
						INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
						pDC, "hi", 2, &csz);
                    lWidth = csz.cx;

                    //  Count the max width by looping through all entries.
                    if(GetElementSelectData())  {
                        lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)GetElementSelectData()->options;
                        if(pOptionData) {
                            char *pCurrent = NULL;
                            int32 lCounter = 0;
                            while(lCounter < GetElementSelectData()->option_cnt)    {
                                pCurrent = (char *)pOptionData[lCounter].text_value;
                                if(pCurrent && *pCurrent)    {
									CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
										INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
										pDC, pCurrent, XP_STRLEN(pCurrent), &csz);

                                    if(csz.cx > lWidth) {
                                        lWidth = csz.cx;
                                    }
                                }

                                lCounter++;
                            }
                        }
                    }

                    //  Add some size information (basically backwards derived from the
                    //      display code for best look).
                    lWidth += 2 * pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth);
					lWidth += pMyFont->GetMeanWidth();

                    lHeight = GetElementSelectData()->size;
                    if(lHeight < 1) {
                        lHeight = 1;
                    }
                    lHeight *= csz.cy;
                    lHeight += csz.cy / 2;

 					// Check and see if we should use preset sizes
					if (GetElement()->width > lWidth) {
						lWidth = GetElement()->width;
					}
                    lBaseline = lHeight - csz.cy / 4;

					pDCCX->ReleaseNetscapeFont( pDC, pMyFont );
				}
				else {
					m_pWidget->ReleaseDC(CDC::FromHandle(pDC));
					DestroyWidget();
				}
                pDCCX->ReleaseContextDC(pDC);
	        }

            //	Tell Layout what we've figured out.
	        GetElement()->width = lWidth;
	        GetElement()->height = lHeight;
			GetElement()->baseline = lBaseline;
		}
	}
}

//	Copy the current data out of the form element back into the
//		layout struct.
void CFormSelectMult::UpdateCurrentData()
{
	//	Switch on context type for proper functionality.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Only continue if we've got a widget and data space to fill at all.
			if(m_pWidget && ::IsWindow(m_pWidget->GetSafeHwnd()) && GetElementSelectData())	{
				lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)GetElementSelectData()->options;
				if(pOptionData)	{
					//	Turn everything off in the layout structure.
					int32 lCounter = 0;
					while(lCounter < GetElementSelectData()->option_cnt)	{
						//	Turn your head and cough.
						pOptionData[lCounter].selected = FALSE;
						//	Next.
						lCounter++;
					}

					//	Determine wether multiple selections allowed.
					if(GetElementSelectData()->multiple)	{
						//	Get number selected.
						int32 lNumber = m_pWidget->GetSelCount();
						if(lNumber > 0)	{
							//	Need an array to store the indexes of selection.
							LPINT pIxs = new int[lNumber];
							if(pIxs)	{
								//	Gather the info.
								m_pWidget->GetSelItems(CASTINT(lNumber), pIxs);

								//	Loop through each one.
								lCounter = 0;
								while(lCounter < lNumber)	{
									//	Stick your tongue out and say "aaaaaa....."
									pOptionData[pIxs[lCounter]].selected = TRUE;
									//	Next.
									lCounter++;
								}

								delete [] pIxs;
							}
						}
					}
					else	{
						//	Only one selection.
						int32 lSelected = m_pWidget->GetCurSel();
						if(lSelected != LB_ERR)	{
							pOptionData[lSelected].selected = TRUE;
						}
					}					
				}
			}
		}
		else if(GetContext()->IsPureDCContext())	{
            //  Whatever is current, will be represented.
		}
	}
}

HWND CFormSelectMult::GetRaw()
{
    return(m_pWidget ? m_pWidget->m_hWnd : NULL);
}

