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

#include "fmtxarea.h"
#include "odctrl.h"
#include "intl_csi.h"

//	This file is dedicated to form type select one elements
//		otherwise known as list boxes on windows and
//		their implementation as requried by the XP layout
//		library.

//	Construction simply clears all members.
CFormTextarea::CFormTextarea()
{
	//	No Widget yet.
	m_pWidget = NULL;

#ifdef XP_WIN16
	//	No segment yet.
	m_hSegment = NULL;
#endif
}

//	Destruction cleans out all members.
CFormTextarea::~CFormTextarea()
{
}

//	How to set the LO form element.
//	This may change during the lifetime of an instance, so use this
//		to update all referencing values.
void CFormTextarea::SetElement(LO_FormElementStruct *pFormElement)
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
void CFormTextarea::SetContext(CAbstractCX *pCX)
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
void CFormTextarea::DisplayFormElement(LTRB& Rect)
{
	//	Display only has meaning if our context is a device context.
	if(GetContext() && GetContext()->IsDCContext())	{
		//	Further, need to detect how we're going to be drawing ourselves.
		if(GetContext()->IsPureDCContext())	{
			//	Only works from a DC, needs a GDI drawing representation.
			CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
            LTRB r(Rect.left, Rect.top, Rect.right, Rect.bottom);

            //  Adjust for stuff done in FillSizeInfo
            r.Inflate(0, -1 * pDCCX->Pix2TwipsY(EDIT_SPACE) / 2);

            //  Do something simple.
            pDCCX->Display3DBox(r, pDCCX->ResolveLightLineColor(), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));

            //  Decide if we're doing scrollers.
            BOOL bHorizontal = FALSE;
            if(GetElementTextareaData() && GetElementTextareaData()->auto_wrap == TEXTAREA_WRAP_OFF)    {
                bHorizontal = TRUE;
            }

            //  Figure rects of scrollers.
            LTRB VScroller(r.right - pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth), r.top, r.right, r.bottom);
            if(bHorizontal) {
                VScroller.bottom -= pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight);
            }
            LTRB HScroller(r.left, r.bottom - pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight), r.right - pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth), r.bottom);

            //  Adjust the original rect inside of the scroll bars and border.
            r.left += pDCCX->Pix2TwipsY(2);
            r.top += pDCCX->Pix2TwipsY(2);
            r.right -= pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth);
            if(bHorizontal) {
                r.bottom -= pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight);
            }
            else    {
                r.bottom -= pDCCX->Pix2TwipsY(2);
            }

            //  Time for DC work.
            HDC pDC = pDCCX->GetContextDC();
            if(pDC) {
                //  Figure up the DrawText rectangle (so that we can get the proportionals right).
                int32 lDrawnWidth = 0;
                int32 lDrawnHeight = 0;
				CyaFont	*pMyFont = 0;
                if(GetElementTextareaData() && GetElementTextareaData()->current_text)    {
                    char *pText = (char *)GetElementTextareaData()->current_text;
                    int32 lLength = XP_STRLEN(pText);

					
					pDCCX->SelectNetscapeFont( pDC, GetTextAttr(), pMyFont );
					if (!pMyFont) {
						m_pWidget->ReleaseDC(CDC::FromHandle(pDC));
						DestroyWidget();
						pDCCX->ReleaseContextDC(pDC);
						return;
					}
                    r.left += pMyFont->GetMeanWidth() - pDCCX->Pix2TwipsY(2);

					//  Decide the output area for all text.
					RECT crClip;
					::SetRect(&crClip, CASTINT(r.left), CASTINT(r.top),
						CASTINT(r.right), CASTINT(r.bottom));
					RECT crSize = crClip;

					//  Figure up the draw text style we're asking for.
					UINT uStyle = DT_LEFT | DT_NOPREFIX;
					if(GetElementTextareaData()->auto_wrap != TEXTAREA_WRAP_OFF)    {
						//  Text should have already been wrapped, but do it anyhow.
						uStyle |= DT_WORDBREAK;
					}

					//  Figure out the size.
					//  This does not draw anything.
					int16 wincsid = 0 ;
					if(GetContext()->GetContext())
						wincsid = INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()->GetContext()));
				
					CIntlWin::DrawText(wincsid, pDC, pText, CASTINT(lLength), &crSize, uStyle | DT_CALCRECT);
					lDrawnWidth = crSize.right - crSize.left;
					lDrawnHeight = crSize.bottom - crSize.top;

					//  Set us to transparncy and do the deed.
					int iOldBk = ::SetBkMode(pDC, TRANSPARENT);
					CIntlWin::DrawText(wincsid, pDC, pText, CASTINT(lLength), &crClip, uStyle);
					::SetBkMode(pDC, iOldBk);
					pDCCX->ReleaseNetscapeFont( pDC, pMyFont );
					//  Restore the font.
                }            

                //  Draw the scrollers.
                //  Vertical is always present.
                //  Don't do if can't handle due to size limitations
                pDCCX->Display3DBox(VScroller, pDCCX->ResolveLightLineColor(), pDCCX->ResolveLightLineColor(), pDCCX->Pix2TwipsY(2));
                if(VScroller.Height() > pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight) * 2)  {
                    POINT aPoints[3];

                    //  Display top button.
                    LTRB TButton(VScroller.left, VScroller.top, VScroller.right, VScroller.top + pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight));
                    pDCCX->Display3DBox(TButton, RGB(0, 0, 0), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
                    TButton.Inflate(-1 * pDCCX->Twips2PixX(2), -1 * pDCCX->Pix2TwipsY(2));
                    VScroller.top += pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight);

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
                    LTRB BButton(VScroller.left, VScroller.bottom - pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight), VScroller.right, VScroller.bottom);
                    pDCCX->Display3DBox(BButton, RGB(0, 0, 0), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
                    BButton.Inflate(-1 * pDCCX->Twips2PixX(2), -1 * pDCCX->Pix2TwipsY(2));
                    VScroller.bottom -= pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight);

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
                    if(r.Height() < lDrawnHeight)  {
                        //  Size of thumbtrack is a percentage.
                        int32 lPercent = r.Height() * 100 / lDrawnHeight;
                        int32 lThumb = lPercent * VScroller.Height() / 100;
                        LTRB Thumb(VScroller.left, VScroller.top, VScroller.right, VScroller.top + lThumb);
                        pDCCX->Display3DBox(Thumb, RGB(0, 0, 0), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
                    }
                }

                if(bHorizontal) {
                    //  Don't do if can't handle due to size limitations
                    pDCCX->Display3DBox(HScroller, pDCCX->ResolveLightLineColor(), pDCCX->ResolveLightLineColor(), pDCCX->Pix2TwipsY(2));
                    if(HScroller.Width() > pDCCX->Pix2TwipsY(sysInfo.m_iScrollWidth) * 2)  {
                        POINT aPoints[3];

                        //  Display left button.
                        LTRB LButton(HScroller.left, HScroller.top, HScroller.left + pDCCX->Pix2TwipsY(sysInfo.m_iScrollWidth), HScroller.bottom);
                        pDCCX->Display3DBox(LButton, RGB(0, 0, 0), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
                        LButton.Inflate(-1 * pDCCX->Twips2PixX(2), -1 * pDCCX->Pix2TwipsY(2));
                        HScroller.left += pDCCX->Pix2TwipsY(sysInfo.m_iScrollWidth);

                        //  Arrow.
                        aPoints[0].x = CASTINT(LButton.left + 2 * LButton.Width() / 3);
                        aPoints[0].y = CASTINT(LButton.top + LButton.Height() / 4);
                        aPoints[1].x = CASTINT(LButton.left + LButton.Width() / 3);
                        aPoints[1].y = CASTINT(LButton.top + LButton.Height() / 2);
                        aPoints[2].x = CASTINT(LButton.left + 2 * LButton.Width() / 3);
                        aPoints[2].y = CASTINT(LButton.top + 3 * LButton.Height() / 4);
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

                        //  Display right button.
                        LTRB RButton(HScroller.right - pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth), HScroller.top, HScroller.right, HScroller.bottom);
                        pDCCX->Display3DBox(RButton, RGB(0, 0, 0), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
                        RButton.Inflate(-1 * pDCCX->Twips2PixX(2), -1 * pDCCX->Pix2TwipsY(2));
                        HScroller.right -= pDCCX->Pix2TwipsY(sysInfo.m_iScrollWidth);

                        //  Arrow.
                        aPoints[0].x = CASTINT(RButton.left + RButton.Width() / 3);
                        aPoints[0].y = CASTINT(RButton.top + RButton.Height() / 4);
                        aPoints[1].x = CASTINT(RButton.left + 2 * RButton.Width() / 3);
                        aPoints[1].y = CASTINT(RButton.top + RButton.Height() / 2);
                        aPoints[2].x = CASTINT(RButton.left + RButton.Width() / 3);
                        aPoints[2].y = CASTINT(RButton.top + 3 * RButton.Height() / 4);
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
                        if(r.Width() < lDrawnWidth)  {
                            //  Size of thumbtrack is a percentage.
                            int32 lPercent = r.Width() * 100 / lDrawnWidth;
                            int32 lThumb = lPercent * HScroller.Width() / 100;
                            LTRB Thumb(HScroller.left, HScroller.top, HScroller.left + lThumb, HScroller.bottom);
                            pDCCX->Display3DBox(Thumb, RGB(0, 0, 0), pDCCX->ResolveDarkLineColor(), pDCCX->Pix2TwipsY(2));
                        }
	               }
               }
                //  Release the DC.
                pDCCX->ReleaseContextDC(pDC);
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
void CFormTextarea::DestroyWidget()
{
	//	Get rid of the widget if around.
	if(m_pWidget)	{
		m_pWidget->DestroyWindow();
		delete m_pWidget;
		m_pWidget = NULL;
	}

#ifdef XP_WIN16
	//	Get rid of the extra segment if around.
	if(m_hSegment)	{
		GlobalFree(m_hSegment);
		m_hSegment = NULL;
	}
#endif
}

//	Create the widget (window) implementation of the form
//		but DO NOT DISPLAY.
void CFormTextarea::CreateWidget()
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
			m_hSegment = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, 4 * 1024);
			if(m_hSegment)	{
	            LPVOID lpPtr = GlobalLock(m_hSegment);
    	        LocalInit(HIWORD((LONG)lpPtr), 0, (WORD)(GlobalSize(m_hSegment) - 16));
        	    UnlockSegment(HIWORD((LONG)lpPtr));
            	m_pWidget->SetInstance((HINSTANCE)HIWORD((LONG)lpPtr)); 
			}
			else	{
				delete m_pWidget;
				m_pWidget = NULL;
				return;
			}
#endif

			//	The style of the widget to be created.
			DWORD dwStyle = WS_CHILD | WS_VSCROLL | WS_BORDER | ES_LEFT | WS_TABSTOP | ES_MULTILINE;
			if(GetElementTextareaData())	{
				//	If no word wrapping, add a horizontal scrollbar.
				if(GetElementTextareaData()->auto_wrap == TEXTAREA_WRAP_OFF)	{
					dwStyle |= WS_HSCROLL;
				}
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
#ifdef XP_WIN16
				GlobalFree(m_hSegment);
				m_hSegment = NULL;
#endif
				return;
			}

			CyaFont	*pMyFont;
			

			//	Measure some text.
			CDC *pDC = m_pWidget->GetDC();
			if(pDC)	{
				CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
				pDCCX->SelectNetscapeFont( pDC->GetSafeHdc(), GetTextAttr(), pMyFont );
				if (pMyFont) {
					SetWidgetFont(pDC->GetSafeHdc(), m_pWidget->m_hWnd);
					GetElement()->text_attr->FE_Data = pMyFont;
					//	Default length is 20
					//	Default lines is 1
					int32 lLength = 20;
					int32 lLines = 1;

					//	See if we can measure the default text, and/or
					//		set up the size and size limits.
					if(GetElementTextareaData())	{
						if(GetElementTextareaData()->cols > 0)	{
							//	Use provided size.
							lLength = GetElementTextareaData()->cols;
						}
						if(GetElementTextareaData()->rows > 0)	{
							//	Use provided size.
							lLines = GetElementTextareaData()->rows;
						}
					}

					//	Now figure up the width and height we would like.
	//				int32 lWidgetWidth = (lLength + 1) * tm.tmAveCharWidth + sysInfo.m_iScrollWidth;
	//				int32 lWidgetHeight = (lLines + 1) * tm.tmHeight;
					int32 lWidgetWidth = (lLength + 1) * pMyFont->GetMeanWidth() + sysInfo.m_iScrollWidth;
					int32 lWidgetHeight = (lLines + 1) * pMyFont->GetHeight();

					//	If no word wrapping, account a horizontal scrollbar.
					if(GetElementTextareaData()->auto_wrap == TEXTAREA_WRAP_OFF)	{
						lWidgetHeight += sysInfo.m_iScrollHeight;
					}

					//	Move the window.
					m_pWidget->MoveWindow(1, 1, CASTINT(lWidgetWidth), CASTINT(lWidgetHeight), FALSE);

					pDCCX->ReleaseNetscapeFont( pDC->GetSafeHdc(), pMyFont );
	                pDCCX->ReleaseContextDC(pDC->GetSafeHdc());
					m_pWidget->ReleaseDC(pDC);
				}
				else {
					m_pWidget->ReleaseDC(pDC);
					DestroyWidget();
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
void CFormTextarea::UseCurrentData()
{
	//	Detect context type and do the right thing.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Need a widget.
			if(m_pWidget)	{
				//	Determine the current text to set.
				char *pCurrent = "";
				if(GetElementTextareaData() && GetElementTextareaData()->current_text)	{
					pCurrent = (char *)GetElementTextareaData()->current_text;
				}
				// We have to SetContext to the widget before we SetWindowText
				// Otherwise, the widget don't know what csid the text is.
				m_pWidget->SetContext(GetContext()->GetContext(), GetElement());
				m_pWidget->SetWindowText(pCurrent);
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Printing/metafile
            //  Whatever is current is current.
		}
	}
}

//	Copy the default data out of the layout struct into the form
//		widget, or mark that you should represent using the default data.
void CFormTextarea::UseDefaultData()
{
	//	Detect context type and do the right thing.
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			//	Need a widget.
			if(m_pWidget)	{
				//	Determine the default text to set.
				char *pDefault = "";
				if(GetElementTextareaData() && GetElementTextareaData()->default_text)	{
					pDefault = (char *)GetElementTextareaData()->default_text;
				}
				// We have to SetContext to the widget before we SetWindowText
				// Otherwise, the widget don't know what csid the text is.
				m_pWidget->SetContext(GetContext()->GetContext(), GetElement());
				m_pWidget->SetWindowText(pDefault);
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Printing/metafile
            if(GetElementTextareaData())    {
                //  Clear the current text if present.
			    if(GetElementTextareaData()->current_text)	{
				    XP_FREE(GetElementTextareaData()->current_text);
				    GetElementTextareaData()->current_text = NULL;
			    }

                //  Copy over the default_text.
                if(GetElementTextareaData()->default_text)  {
                    int32 lSize = XP_STRLEN((char *)GetElementTextareaData()->default_text) + 1;
                    GetElementTextareaData()->current_text = (XP_Block)XP_ALLOC(lSize);
                    if(GetElementTextareaData()->current_text)  {
                        memcpy(GetElementTextareaData()->current_text,
                            GetElementTextareaData()->default_text,
                            CASTSIZE_T(lSize));
                    }
                }
            }
		}
	}
}

//	Fill in the layout size information in the layout struct regarding
//		the dimensions of the widget.
void CFormTextarea::FillSizeInfo()
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
					CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
					pDCCX->SelectNetscapeFont( pDC->GetSafeHdc(), GetTextAttr(), pMyFont );
					if (pMyFont) {
						SetWidgetFont(pDC->GetSafeHdc(), m_pWidget->m_hWnd);
						GetElement()->baseline -= (pMyFont->GetHeight() / 2 + pMyFont->GetDescent()) / 2;
						pDCCX->ReleaseNetscapeFont( pDC->GetSafeHdc(), pMyFont );
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
            int32 lWidth = 0;
            int32 lHeight = 0;
            int32 lBaseline = 0;

            CDCCX *pDCCX = VOID2CX(GetContext(), CDCCX);
            HDC pDC = pDCCX->GetAttribDC();
            if(pDC) {
                //  Have the DC, attempt to select the widget font.
					CyaFont	*pMyFont;
					
					pDCCX->SelectNetscapeFont( pDC, GetTextAttr(), pMyFont );
					if (pMyFont) {
                        int32 lColumns = 20;
                        int32 lRows = 1;

                        //  See if we can measure the default text and/or set up
                        //      size limits.
                        if(GetElementTextareaData())    {
                            if(GetElementTextareaData()->cols > 0)  {
                                lColumns = GetElementTextareaData()->cols;
                            }
                            if(GetElementTextareaData()->rows > 0)  {
                                lRows = GetElementTextareaData()->rows;
                            }
                        }

                        //  Figure up width and height we would like.
//                            lWidth = (lColumns + 1) * tm.tmAveCharWidth + pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth);
//                            lHeight = (lRows + 1) * tm.tmHeight;
                        lWidth = (lColumns + 1) * pMyFont->GetMeanWidth() + pDCCX->Pix2TwipsX(sysInfo.m_iScrollWidth);
                        lHeight = (lRows + 1) * pMyFont->GetHeight();

                        //  If no word wrapping, add a horizontal scrollbar.
                        if(GetElementTextareaData() && GetElementTextareaData()->auto_wrap == TEXTAREA_WRAP_OFF) {
                            lHeight += pDCCX->Pix2TwipsY(sysInfo.m_iScrollHeight);
                        }

                        //  Adjust a little bit more, will need to know how to decode this
                        //      in display.
                        lHeight += pDCCX->Pix2TwipsY(EDIT_SPACE);

                        //  Figure the baseline.
//                            lBaseline = lHeight - ((tm.tmHeight / 2 + tm.tmDescent) / 2);
                        lBaseline = lHeight - ((pMyFont->GetHeight() / 2 + pMyFont->GetDescent()) / 2);
//                        }

					pDCCX->ReleaseNetscapeFont( pDC, pMyFont );
				}
				else {
					m_pWidget->ReleaseDC(CDC::FromHandle(pDC));
					DestroyWidget();
				}

                pDCCX->ReleaseContextDC(pDC);
            }

            //  Tell layout what we know.
			GetElement()->width = lWidth;
			GetElement()->height = lHeight;
			GetElement()->baseline = lBaseline;
		}
	}
}

//	Copy the current data out of the form element back into the
//		layout struct.
void CFormTextarea::UpdateCurrentData()
{
	if(GetContext())	{
		if(GetContext()->IsWindowContext())	{
			if(m_pWidget && ::IsWindow(m_pWidget->GetSafeHwnd()) && GetElementTextareaData())	{
				//	Free off any current text.
				if(GetElementTextareaData()->current_text)	{
					XP_FREE(GetElementTextareaData()->current_text);
					GetElementTextareaData()->current_text = NULL;
				}

				//	If we have hard wrapping, set the flag in the edit field now
				//		bofore we get the length.
				if(GetElementTextareaData()->auto_wrap == TEXTAREA_WRAP_HARD)	{
					m_pWidget->FmtLines(TRUE);
				}

				//	See if we've got anything.
				int32 lLength = m_pWidget->GetWindowTextLength();
				if(lLength > 0)	{
					GetElementTextareaData()->current_text = (XP_Block)XP_ALLOC(lLength + 1);
					if(GetElementTextareaData()->current_text)	{
						m_pWidget->GetWindowText((char *)GetElementTextareaData()->current_text, CASTINT(lLength + 1));

						// strip out the \r\r\n if we are a hard-break text area
						if(GetElementTextareaData()->auto_wrap == TEXTAREA_WRAP_HARD)	{
							char *pSrc;
							char *pDest;

							// start both off at the beginning
							pSrc  = (char *)GetElementTextareaData()->current_text;
							pDest = (char *)GetElementTextareaData()->current_text;

							while(pSrc && *pSrc)	{
								if(pSrc[0] == '\r' && pSrc[1] == '\r' && pSrc[2] == '\n')	{
									// convert the soft line break into a hard line break
									pSrc += 3;
									*pDest++ = '\r';
									*pDest++ = '\n';
								}
								else	{
									// just copy this one over
									*pDest++ = *pSrc++;
								}
							}

							// NULL termination is your friend
							*pDest = '\0';
						}
					}
				}
			}
		}
		else if(GetContext()->IsPureDCContext())	{
			//	Printing/metafile
            //  Whatever is current is current.
		}
	}
}

HWND CFormTextarea::GetRaw()
{
    return(m_pWidget ? m_pWidget->m_hWnd : NULL);
}

