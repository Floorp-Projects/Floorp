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
#include "fmselone.h"
#include "intl_csi.h"

//	This file is dedicated to form type select one elements
//		otherwise known as combo boxes on windows and
//		their implementation as requried by the XP layout
//		library.

//	Construction simply clears all members.
CFormSelectOne::CFormSelectOne()
{
	//	No widget representation yet.
	m_pWidget = NULL;
}

//	Destruction cleans out all members.
CFormSelectOne::~CFormSelectOne()
{
}

//	How to set the LO form element.
//	This may change during the lifetime of an instance, so use this
//		to update all referencing values.
void CFormSelectOne::SetElement(LO_FormElementStruct *pFormElement)
{
	//	Call the base.
	CFormElement::SetElement(pFormElement);

	//	Have our widget update if present.
	if(m_pWidget != NULL)	{
		m_pWidget->SetContext(GetContext() ? GetContext()->GetContext() : NULL, GetElement());
	}
}

//	Set the owning context.
//	Use this to determine what context we live in and how we should
//		represent ourself (DC or window).
void CFormSelectOne::SetContext(CAbstractCX *pCX)
{
	//	Call the base.
	CFormElement::SetContext(pCX);

	//	Have our widget update if present.
	if(m_pWidget != NULL)	{
		m_pWidget->SetContext(GetContext() ? GetContext()->GetContext() : NULL, GetElement());
	}
}

//	Display the form element given the particular context we are in.
//	Possibly only use a DC for representation, or have the
//		window move.
void CFormSelectOne::DisplayFormElement(LTRB& Rect)
{
	//	Display only has meaning if our context is a device context.
	if(GetContext() && GetContext()->IsDCContext())	{
		//	Further, need to detect how we're going to be drawing ourselves.
		if(GetContext()->IsPureDCContext())	{
			//	Only works from a DC, needs a GDI drawing representation.
			CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
            LTRB r(Rect.left, Rect.top, Rect.right, Rect.bottom);

            //  Compensate for munging in FillSizeInfo.
            r.Inflate(0, -1 * pDCCX->Pix2TwipsY(EDIT_SPACE));

            //  Do something simple.
            pDCCX->Display3DBox(r, pDCCX->ResolveLightLineColor(), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
            r.Inflate(0, -1 * pDCCX->Pix2TwipsY(2));

            HDC pDC = pDCCX->GetContextDC();
            if(pDC) {
                //  Draw the down arrow on the right.
                r.right -= pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth);
                LTRB BButton(r.right, r.top - pDCCX->Pix2TwipsY(2), r.right + pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth), r.bottom + pDCCX->Pix2TwipsY(2));
                pDCCX->Display3DBox(BButton, RGB(0, 0, 0), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
                BButton.Inflate(-1 * pDCCX->Pix2TwipsX(2), -1 * pDCCX->Pix2TwipsY(2));

                //  Arrow.
                POINT aPoints[3];
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

                //  Output visible text.
                if(GetElementSelectData())  {
					CyaFont	*pMyFont;
					VOID2CX(GetContext(), CDCCX)->SelectNetscapeFont( pDC, GetTextAttr(), pMyFont );
					if (pMyFont) {
                        lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)GetElementSelectData()->options;
                        if(pOptionData) {
                            char *pText = NULL;
                            int32 lCounter = 0;

                            //  As a default for the text, use the first entry.
                            //  Sometimes nothing is selected, and then we don't output anything....
                            //  This will get retset by the loop below if something actually
                            //      does exist.
                            if(GetElementSelectData()->option_cnt > 0)  {
                                pText = (char *)pOptionData[0].text_value;
                            }

                            while(lCounter < GetElementSelectData()->option_cnt)    {
                                if(pOptionData[lCounter].selected)  {
                                    //  Show this one.
                                    pText = (char *)pOptionData[lCounter].text_value;
                                    break;
                                }
                                lCounter++;
                            }

                            //  Do we have anything to show?
                            if(pText)   {
                                //  Need to modify the coordinates a little bit.
								r.left += pMyFont->GetMeanWidth();
                                int32 lCenterY = (r.Height() - pMyFont->GetHeight()) / 2;

                                //  Transparent please.
                                int iOldBk = ::SetBkMode(pDC, TRANSPARENT);

								CIntlWin::ExtTextOut(
                                    ((GetContext()->GetContext()) ?   
										INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext()))
									 : 0) ,
                                    pDC,
									CASTINT(r.left),
                                    CASTINT(r.top + lCenterY),
                                    ETO_CLIPPED,
                                    CRect(CASTINT(r.left), CASTINT(r.top), CASTINT(r.right), CASTINT(r.bottom)),
                                    pText,
                                    XP_STRLEN(pText),
                                    NULL);

                                //  Restore mode.
                                ::SetBkMode(pDC, iOldBk);
                            }
						}
						VOID2CX(GetContext(), CDCCX)->ReleaseNetscapeFont( pDC, pMyFont );
					}
					else {
						m_pWidget->ReleaseDC(CDC::FromHandle(pDC));
						DestroyWidget();
					}
				}
                //  Done with DC.
				pDCCX->ReleaseContextDC(pDC);
            }
		}
		else if(GetContext()->IsWindowContext())	{
			MoveWindow(m_pWidget->m_hWnd, Rect.left, Rect.top + EDIT_SPACE);
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
void CFormSelectOne::DestroyWidget()
{
	//	Get rid of the widget.
	if(m_pWidget != NULL)	{
    	m_pWidget->DestroyWindow();
		delete m_pWidget;
		m_pWidget = NULL;
	}
}

//	Create the widget (window) implementation of the form
//		but DO NOT DISPLAY.
void CFormSelectOne::CreateWidget()
{
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext() && VOID2CX(GetContext(), CPaneCX)->GetPane())	{
			//	For a window context, we use a widget representation.
			//	Allocate.
			ASSERT(m_pWidget == NULL);
			m_pWidget = new CODMochaComboBox();
			if(m_pWidget == NULL)	{
				return;
			}

			//	Inform the widget of the context and form element (for mocha).
			m_pWidget->SetContext(GetContext()->GetContext(), GetElement());

			//	Create the widget, initially hidden with a bad size
			if(!m_pWidget->Create(WS_CHILD | WS_BORDER | WS_VSCROLL | CBS_AUTOHSCROLL | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED,
					CRect(0, 0, 1, 1),
					CWnd::FromHandle(VOID2CX(GetContext(), CPaneCX)->GetPane()),
					GetDynamicControlID()))	{
				delete m_pWidget;
				m_pWidget = NULL;
				return;
			}

			//	Prepare to measure text.
			CDC *pDC = m_pWidget->GetDC();
			HDC hdc = pDC->GetSafeHdc();
			if(pDC)	{
				CyaFont	*pMyFont;
				
				VOID2CX(GetContext(), CDCCX)->SelectNetscapeFont( hdc, GetTextAttr(), pMyFont );
				int32 lWidgetWidth;
				int32 lWidgetHeight = 0;
				if (pMyFont) {
					SetWidgetFont(pDC->GetSafeHdc(), m_pWidget->m_hWnd);
					GetElement()->text_attr->FE_Data = pMyFont;
					SIZE csz;
					CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
						INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
						hdc, "hi", 2, &csz);
					lWidgetWidth = csz.cx;
					lWidgetHeight = 0;

					//	Count the maximum width by looping through all entries.
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
										hdc, pCurrent, XP_STRLEN(pCurrent), &csz);
									if(csz.cx > lWidgetWidth)	{
										lWidgetWidth = csz.cx;
									}
								}

								//	Servicing next in line now.
								lCounter++;
							}
						}
						lWidgetHeight = GetElementSelectData()->option_cnt;
					}
					lWidgetWidth += 2 * sysInfo.m_iScrollWidth;

					//	Figure the maximum height we'll allow.
					//	NOTE:	This algorithm could use some work (legacy code).
					if(lWidgetHeight > 20)	{
						lWidgetHeight = 20;
					}
					lWidgetHeight += 2;
					lWidgetHeight *= csz.cy;

					//	Done measuring text width.
					VOID2CX(GetContext(), CDCCX)->ReleaseNetscapeFont( hdc, pMyFont );
					m_pWidget->ReleaseDC(pDC);
				}
				else {
					m_pWidget->ReleaseDC(pDC);
					DestroyWidget();
					return;
				}

				if (GetElement()->width > lWidgetWidth) {
					lWidgetWidth = GetElement()->width;
				}
				if (GetElement()->height > lWidgetHeight) {
					lWidgetHeight = GetElement()->height;
				}

				//	Finally, size the widget to the width and height
				//		we have figured here.
				m_pWidget->MoveWindow(1, 1, CASTINT(lWidgetWidth), CASTINT(lWidgetHeight), FALSE);
			}
			else	{
				//	No DC, no widget.
				DestroyWidget();
				return;
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	For a print/metafile context we use a drawn representation.
			//	Figure out what size it will be and save the data.			
		}
	}
}

//	Copy the current data out of the layout struct into the form
//		widget, or mark that you should represent using the current data.
void CFormSelectOne::UseCurrentData()
{
	//	Detect context type and copy appropriate data
	//		according to representation.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Only continue if we've got a widget to fill at all.
			if(m_pWidget)	{
				//	Remove all entries.
				m_pWidget->ResetContent();

				//	Loop through and copy the data into the widget.
				LO_LockLayout();
				lo_FormElementSelectData * selectData = GetElementSelectData();
				if(selectData)	{
					lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)selectData->options;
					if(pOptionData)	{
						char *pCurrent = NULL;
						int32 lCounter = 0;
						while(lCounter < selectData->option_cnt)	{
							pCurrent = (char *)pOptionData[lCounter].text_value;

							//	Add it to the list.
				m_pWidget->AddString(pCurrent ? pCurrent : "");

							//	Is it selected?
							if(pOptionData[lCounter].selected)	{
								m_pWidget->SetCurSel(CASTINT(lCounter));
							}

							//	Servicing next in line now.
							lCounter++;
						}
					}
				}
				LO_UnlockLayout();
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	For a print/metafile context we use a drawn representation.
			//	So obviously we need to do something other than fill in a widget's data.
		}
	}
}

//	Copy the default data out of the layout struct into the form
//		widget, or mark that you should represent using the default data.
void CFormSelectOne::UseDefaultData()
{
	//	Detect context type and copy appropriate data
	//		according to representation.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Only continue if we've got a widget to fill at all.
			if(m_pWidget)	{
				//	Remove all entries.
				m_pWidget->ResetContent();

				//	Loop through and copy the data into the widget.
				LO_LockLayout();
				lo_FormElementSelectData * selectData = GetElementSelectData();
				if(selectData)	{
					lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)selectData->options;
					if(pOptionData)	{
						char *pCurrent = NULL;
						int32 lCounter = 0;
						while(lCounter < selectData->option_cnt)	{
							pCurrent = (char *)pOptionData[lCounter].text_value;

							//	Add it to the list.
                            m_pWidget->AddString(pCurrent ? pCurrent : "");

							//	Restore default selection.
							pOptionData[lCounter].selected = pOptionData[lCounter].def_selected;

							//	Is it selected?
							if(pOptionData[lCounter].selected)	{
								m_pWidget->SetCurSel(CASTINT(lCounter));
							}

							//	Servicing next in line now.
							lCounter++;
						}
					}
				}
				LO_UnlockLayout();
			}
		}
		else if(GetContext()->IsPureDCContext())	{
            //  Loop through and set the current data to reflect the default data.
			if(GetElementSelectData())	{
				lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)GetElementSelectData()->options;
				if(pOptionData)	{
					int32 lCounter = 0;
					while(lCounter < GetElementSelectData()->option_cnt)	{
						//	Restore default selection.
						pOptionData[lCounter].selected = pOptionData[lCounter].def_selected;

						//	Servicing next in line now.
						lCounter++;
					}
				}
			}
		}
	}
}

//	Fill in the layout size information in the layout struct regarding
//		the dimensions of the widget.
void CFormSelectOne::FillSizeInfo()
{
	//	Detect context type and copy appropriate data
	//		according to representation.
	if(GetContext() && GetElement())	{
		if(GetContext()->IsWindowContext())	{
			//	Only continue if we've got a widget to fill at all.
			if(m_pWidget)	{
				//	Determine our window position.
				CRect crRect;
				m_pWidget->GetWindowRect(crRect);

				//	Munge it a little for layout.
				//	We'll have to know how to decode
				//		this information in the display
				//		routine.
				GetElement()->width = crRect.Width();
				GetElement()->height = crRect.Height() + 2 * EDIT_SPACE;
				GetElement()->border_vert_space = EDIT_SPACE;
				GetElement()->baseline = 3 * GetElement()->height / 4;
			}
			else	{
				//	Widget not present, give LAYOUT nothing.
				GetElement()->width = 0;
				GetElement()->height = 0;
				GetElement()->baseline = 0;
			}
		}
		else if(GetContext()->IsPureDCContext())	{
            //  Need to figure size of combo box.
            int32 lWidth = 0;
            int32 lHeight = 0;
            int32 lBaseline = 0;

            CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
                HDC pDC = pDCCX->GetAttribDC();
                if(pDC) {
					CyaFont	*pMyFont;
					
					pDCCX->SelectNetscapeFont( pDC, GetTextAttr(), pMyFont );
					if (pMyFont) {
                        SIZE csz;
						csz.cx = csz.cy = 0;

                        //  Default if no other text.
						CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
							INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
							pDC, "hi", 2, &csz);
                        lWidth = csz.cx;

                        //  Get max width by looping through and measuring all entries.
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

                        //  Add some size information.
                        lWidth += 2 * pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth);
 						lWidth += pMyFont->GetMeanWidth();

                        lHeight = csz.cy;
                        lHeight += csz.cy / 2;
                        lHeight += 2 * pDCCX->Pix2TwipsY(EDIT_SPACE);

						// Check and see if we should use preset sizes
						if (GetElement()->width > lWidth) {
							lWidth = GetElement()->width;
						}
						if (GetElement()->height > lHeight) {
							lHeight = GetElement()->height;
						}

                        lBaseline = 3 * lHeight / 4;

                        //  Restore the font.
						pDCCX->ReleaseNetscapeFont( pDC, pMyFont );
                    }
					else {
						m_pWidget->ReleaseDC(CDC::FromHandle(pDC));
						DestroyWidget();
					}
                    //  Done with DC.
                    pDCCX->ReleaseContextDC(pDC);
				}

            //  Let layout know what we know.
	        GetElement()->width = lWidth;
	        GetElement()->height = lHeight;
            GetElement()->baseline = lBaseline;
		}
	}
}

//	Copy the current data out of the form element back into the
//		layout struct.
void CFormSelectOne::UpdateCurrentData()
{
	//	Detect context type and copy appropriate data
	//		according to representation.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Only continue if we've got a widget and data space to fill at all.
			if(m_pWidget && ::IsWindow(m_pWidget->GetSafeHwnd()) && GetElementSelectData())	{
				int32 lSelected = m_pWidget->GetCurSel();
				lo_FormElementOptionData *pOptionData = (lo_FormElementOptionData *)GetElementSelectData()->options;

				if(pOptionData)	{
					//	Clear out all old selections.
					int32 lCounter = 0;
					while(lCounter < GetElementSelectData()->option_cnt)	{
						pOptionData[lCounter].selected = FALSE;
						lCounter++;
					}

					//	Save this into the layout struct.
					if(lSelected != CB_ERR)	{
						pOptionData[lSelected].selected = TRUE;
					}
				}
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	For a print/metafile context we use a drawn representation.
			//	So obviously we need to do something other than fill in a widget's data.
		}
	}
}

HWND CFormSelectOne::GetRaw()
{
    return(m_pWidget ? m_pWidget->m_hWnd : NULL);
}
