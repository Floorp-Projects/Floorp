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

#include "fmtext.h"
#include "odctrl.h"
#include "intl_csi.h"
//	This file is dedicated to form type select one elements
//		otherwise known as list boxes on windows and
//		their implementation as requried by the XP layout
//		library.

//	Construction simply clears all members.
CFormText::CFormText()
{
	//	No widget yet.
	m_pWidget = NULL;
}

//	Destruction cleans out all members.
CFormText::~CFormText()
{
}

//	How to set the LO form element.
//	This may change during the lifetime of an instance, so use this
//		to update all referencing values.
void CFormText::SetElement(LO_FormElementStruct *pFormElement)
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
void CFormText::SetContext(CAbstractCX *pCX)
{
	//	Call the base.
	CFormElement::SetContext(pCX);

	//	Let the widget know the context.
	if(m_pWidget)	{
		m_pWidget->SetContext(GetContext() ? GetContext()->GetContext() : NULL, GetElement());
	}
}

//	Display the form element given the particular context we are in.
//	Possibly only use a DC for representation, or have the
//		window move.
void CFormText::DisplayFormElement(LTRB& Rect)
{
	//	Display only has meaning if our context is a device context.
	if(GetContext() && GetContext()->IsDCContext())	{
		//	Further, need to detect how we're going to be drawing ourselves.
		if(GetContext()->IsPureDCContext())	{
			//	Only works from a DC, needs a GDI drawing representation.
			CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
            LTRB r(Rect.left, Rect.top, Rect.right, Rect.bottom);

            //  Adjust for what we bastardized earlier in fill size info.
            r.Inflate(0, -1 * pDCCX->Pix2TwipsY(EDIT_SPACE) / 2);

            //  Do something simple.
            pDCCX->Display3DBox(r, pDCCX->ResolveLightLineColor(), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));

            //  Final adjustment for below...
            r.Inflate(-1 * pDCCX->Twips2PixX(2), -1 * pDCCX->Twips2PixY(2));

            //  Is there text to output.
            if(GetElementTextData() && GetElementTextData()->current_text)  {
                char *pText = (char *)GetElementTextData()->current_text;

                //  Obtain the font for the text to be drawn.
                    //  Get the DC.
                    HDC pDC = pDCCX->GetContextDC();
                    if(pDC) {
						CyaFont	*pMyFont;
						
						pDCCX->SelectNetscapeFont( pDC, GetTextAttr(), pMyFont );
						if (pMyFont) {
							SetWidgetFont(pDC, m_pWidget->m_hWnd);
                            int32 lTextLength = XP_STRLEN(pText);
                            char *pTextBuf = new char[lTextLength + 1];
                            if(pTextBuf)    {
                                //  Clear it.
                                memset(pTextBuf, 0, CASTSIZE_T(lTextLength + 1));

                                //  Decide if a password.
                                if(GetElementMinimalData()->type == FORM_TYPE_PASSWORD)  {
                                    memset(pTextBuf, '*', CASTSIZE_T(lTextLength));
                                }
                                else    {
                                    strcpy(pTextBuf, pText);
                                }

                                //  Text mode must go transparent (some printers block out drawing around text).
                                int iOldBk = ::SetBkMode(pDC, TRANSPARENT);

                                //  Decide the destination for the text rect.
                                //  We need to move the text to the right and left some, and on the Y.
                                RECT crDest;
								::SetRect(&crDest, CASTINT(r.left), CASTINT(r.top), CASTINT(r.right), CASTINT(r.bottom));
								crDest.left  += pMyFont->GetMeanWidth();
                                crDest.top += ((crDest.bottom - crDest.top) - (pMyFont->GetHeight())) / 2;
                                //  Finally draw the text.
                                //  The clipping rect is not the destination rect.
								CIntlWin::ExtTextOut(
									(GetContext()->GetContext() ?
									 	INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())) :0),
									pDC, crDest.left,
                                    crDest.top, ETO_CLIPPED, CRect(CASTINT(r.left),
                                    CASTINT(r.top), CASTINT(r.right),
                                    CASTINT(r.bottom)), pTextBuf, CASTINT(lTextLength), NULL);

                                //  Restore Bk mode.
                                ::SetBkMode(pDC, iOldBk);

                                //  Get rid of the buffer.
                                delete[] pTextBuf;
//                            }

							pDCCX->ReleaseNetscapeFont( pDC, pMyFont );
						}
						else {
							m_pWidget->ReleaseDC(CDC::FromHandle(pDC));
							DestroyWidget();
						}
                        pDCCX->ReleaseContextDC(pDC);
                    }
                }
            }
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
void CFormText::DestroyWidget()
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
void CFormText::CreateWidget()
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
			DWORD dwStyle = WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | WS_TABSTOP;
			if(GetElementMinimalData())	{
				switch(GetElementMinimalData()->type)	{
					case FORM_TYPE_PASSWORD:
						dwStyle |= ES_PASSWORD;
						break;
					default:
						break;
				}
			}
			else	{
				//	Must set styles.
				delete m_pWidget;
				m_pWidget = NULL;
				return;
			}

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
			CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
			if(pDC)	{
				//	Default length is 20
				int32 lLength = 20;
				CyaFont	*pMyFont;
					
				pDCCX->SelectNetscapeFont( pDC->GetSafeHdc(), GetTextAttr(), pMyFont );
				if (pMyFont) {
					SetWidgetFont(pDC->GetSafeHdc(), m_pWidget->m_hWnd);
					GetElement()->text_attr->FE_Data = pMyFont;
					//	Now figure up the width and height we would like.
					int32 lWidgetWidth = pMyFont->GetMaxWidth();
					int32 lWidgetHeight = pMyFont->GetHeight() + (pMyFont->GetHeight() / 2);

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
					pDCCX->ReleaseNetscapeFont( pDC->GetSafeHdc(), pMyFont );
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
void CFormText::UseCurrentData()
{
	//	Detect context type and do the right thing.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Need a widget.
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
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Printing/metafile
            //  Whatever is in current data is current....
		}
	}
}

//	Copy the default data out of the layout struct into the form
//		widget, or mark that you should represent using the default data.
void CFormText::UseDefaultData()
{
	//	Detect context type and do the right thing.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Need a widget.
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
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Printing/metafile
            //  Delete any current data, attempt to use the default data if present.
            if(GetElementTextData())    {
                if(GetElementTextData()->current_text)  {
                    XP_FREE(GetElementTextData()->current_text);
                    GetElementTextData()->current_text = NULL;
                }

                if(GetElementTextData()->default_text) {
                    char *pText = (char *)GetElementTextData()->default_text;
                    int32 lSize = XP_STRLEN(pText) + 1;
                    GetElementTextData()->current_text = (XP_Block)XP_ALLOC(lSize);
                    memcpy(GetElementTextData()->current_text, pText, CASTSIZE_T(lSize));
                }
            }
		}
	}
}

//	Fill in the layout size information in the layout struct regarding
//		the dimensions of the widget.
void CFormText::FillSizeInfo()
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
                GetElement()->baseline = GetElement()->height - GetElement()->height / 4;
			}
			else	{
				//	No widget, tell layout nothing.
				GetElement()->width = 0;
				GetElement()->height = 0;
				GetElement()->baseline = 0;
			}
		}
		else if(GetContext()->IsPureDCContext())	{
            int32 lWidth = 0;
            int32 lHeight = 0;
            int32 lBaseline = 0;

            //  Need to figure the size of the widget.
            CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
                HDC pDC = pDCCX->GetAttribDC();
                if(pDC) {
					CyaFont	*pMyFont;
                    int32 lLength = 20;
					
					pDCCX->SelectNetscapeFont( pDC, GetTextAttr(), pMyFont );
					if (pMyFont) {
						SetWidgetFont(pDC, m_pWidget->m_hWnd);
						lWidth = pMyFont->GetMaxWidth();
						lHeight = pMyFont->GetHeight() + pMyFont->GetHeight() / 2 + pDCCX->Twips2PixY(EDIT_SPACE);

						//  Attempt to measure text.
						if(GetElementTextData())    {
							if(GetElementTextData()->size > 0)  {
								//  Use provided size.
								lLength = GetElementTextData()->size;

								//  Figure width using average char width.
								lWidth += lLength * pMyFont->GetMeanWidth();
							}
							else if(GetElementTextData()->default_text) {
								//  Use length of text instead.
								char *pText = (char *)GetElementTextData()->default_text;
								lLength = XP_STRLEN(pText);

								//  Use exact size information.
								SIZE csz;
								CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, 
									INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext())),
									pDC, pText, CASTINT(lLength), &csz);
								lWidth += csz.cx;
							}
						}

						//  Figure baseline.
						lBaseline = lHeight - (pMyFont->GetHeight() / 2 + pMyFont->GetDescent()) / 2;
							pDCCX->ReleaseNetscapeFont( pDC, pMyFont );
					}
					else {
						m_pWidget->ReleaseDC(CDC::FromHandle(pDC));
						DestroyWidget();
					}
                    pDCCX->ReleaseContextDC(pDC);
            }

            //  Inform layout what we now know.
            GetElement()->width = lWidth;
            GetElement()->height = lHeight;
            GetElement()->baseline = lBaseline;
		}
	}
}

//	Copy the current data out of the form element back into the
//		layout struct.
void CFormText::UpdateCurrentData()
{
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
			//	Printing/metafile
            //  If there is no current data, attempt to use the default data if present.
            if(GetElementTextData())    {
                if(GetElementTextData()->default_text) {
                    char *pText = (char *)GetElementTextData()->default_text;
                    if(GetElementTextData()->current_text == NULL)  {
                        int32 lSize = XP_STRLEN(pText) + 1;
                        GetElementTextData()->current_text = (XP_Block)XP_ALLOC(lSize);
                        memcpy(GetElementTextData()->current_text, pText, CASTSIZE_T(lSize));
                    }
                }
            }
		}
	}
}

HWND CFormText::GetRaw()
{
    return(m_pWidget ? m_pWidget->m_hWnd : NULL);
}
