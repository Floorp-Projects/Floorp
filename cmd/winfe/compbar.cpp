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

/* COMPBAR.CPP Contains all the code for the dialog bar portion of the compose
 * window.  This currently comprises the addressing block and attachment 
 * blocks. 
 *
 */
 
#include "stdafx.h"

#include "rosetta.h"
#include "resource.h"
#include "compstd.h"
#include "compbar.h"
#include "compfrm.h"
#include "compfile.h"
#include "apiaddr.h"
#include "addrfrm.h"  //For MOZ_NEWADDR

#include "msg_srch.h"

#define TOTAL_TABS              3
#define TAB_SPACING_X           24
#define TAB_SPACING_Y           20
#define TAB_HORIZONTAL_OFFSET   2
#define TAB_ICON_SIZE           16
#define PIXEL_OFFSET_Y          4
#define PIX_X_FACTOR            20
#define PIX_Y_FACTOR            25
#define COMP_VTAB_WIDTH         9
#define COMP_LEFT_MARGIN        4
#define COMP_HTAB_HEIGHT        10
#define COMP_VTAB_TOP_HEIGHT        7
#define COMP_VTAB_BOTTOM_HEIGHT     3
#define COMP_VTAB_SECTION_HEIGHT    6

/////////////////////////////////////////////////////////////////////////////
// CComposeBar

BEGIN_MESSAGE_MAP(CComposeBar, CDialogBar)
    ON_COMMAND(IDC_BUTTON_ATTACH,OnButtonAttach)
    ON_WM_CREATE()
    ON_WM_TIMER()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_ERASEBKGND()
    ON_WM_DROPFILES()
    HG38729
 	ON_MESSAGE(WM_LEAVINGLASTFIELD,OnLeavingLastField)

END_MESSAGE_MAP()

extern "C" char * wfe_ExpandForNameCompletion(char * pString);
extern "C" char * wfe_ExpandName(char * pString);

CComposeBar::CComposeBar (MWContext *pContext) :
    CDialogBar ()
{
	m_iBoxHeight = 0;
    m_pAttachmentList = NULL;       // widget for attachment list
    m_bSizing =         FALSE;      // for resizing dialog bar (needs work)
    // start off with the address block displayed
    m_iSelectedTab =    IDX_COMPOSEADDRESS;
    m_pSubjectEdit =    NULL;       // text for subject field
    m_pPriority    =    NULL;       // message priority
    m_bClosed =         FALSE;      // dialog bar is collapsed or expanded
    m_bHidden =         FALSE;      // dialog bar is "hidden" (collapsed with no widget) or showing full size
    m_pComposeEdit =    NULL;       // pointer to the composition editor
    m_pWidget =         NULL;
    m_iPriorityIdx =    2;			// 0 based index; 0-lowest 1-low 2-normal 3-high 4-highest
    m_bCanSize =        FALSE;
    m_iTotalAttachments = 0;
	m_iHeight			= 0;
    m_iPrevHeight       = 0;
    m_iMinSize          = 0;
    m_iMaxSize          = 0;
    m_pReturnReceipt =  NULL;
    m_pEncrypted =      NULL;
    m_pSigned =         NULL;
    m_bReceipt =        FALSE;
    m_bSigned =         FALSE;
    m_bEncrypted =      FALSE;
    m_pDropTarget =     NULL;
    m_pMessageFormat =  NULL;
    m_pMessageFormatText = NULL;
    m_pszMessageFormat = NULL;
//    m_bUse8Bit =        FALSE;
    m_bUseUUENCODE =    FALSE;
//    m_pUse8Bit =        NULL;
    m_pUseUUENCODE =    NULL;
	m_pUnkImage = NULL;
    m_pUnkAddressControl = NULL;
	ApiApiPtr(api);
	m_pUnkImage = api->CreateClassInstance(
		APICLASS_IMAGEMAP,NULL,(APISIGNATURE)IDB_COMPOSETABS);
	m_pUnkImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIImage);
	ASSERT(m_pIImage);
	if (!m_pIImage->GetResourceID())
		m_pIImage->Initialize(IDB_COMPOSETABS,16,16);
	m_pContext = pContext;
}

CComposeBar::~CComposeBar ( )
{
    // free up any allocated resources and release any used apis
   if (m_pszMessageFormat)
      free(m_pszMessageFormat);
   if (m_pUnkImage) {
      if (m_pIImage)
         m_pUnkImage->Release();
   }
   if (m_pUnkAddressControl)
      m_pUnkAddressControl->Release();
   delete m_pToolTip;
   if (m_pDropTarget)
   {
      m_pDropTarget->Revoke();
      delete m_pDropTarget;
   }
}

/////////////////////////////////////////////////////////////////////////////
// GetAddressWidget() returns a pointer to the NSAddressList object.

LPADDRESSCONTROL CComposeBar::GetAddressWidgetInterface()
{
    ASSERT(m_pIAddressList);
    return(m_pIAddressList);
}

/////////////////////////////////////////////////////////////////////////////
// TabControl() controls the tabbing order between the controls in
// the composition window.  The controls are parented at different
// levels which make focus issues a little confusing. This function
// returns TRUE if it knew where to put focus, if it couldn't figure 
// out where it should go, FALSE is returned.

BOOL CComposeBar::TabControl(BOOL bShift, BOOL bControl, CWnd * pWnd)
{
    // get a pointer to the parent frame (FIXME JRE).  The child
    // really shouldn't need to know anything about the parent.
	CComposeFrame *pCompose = (CComposeFrame *)GetParent();
    CListBox * pListBox = NULL;

    switch (m_iSelectedTab)
    {
        case IDX_COMPOSEADDRESS:
            pListBox = m_pIAddressList->GetListBox();
            break;
        case IDX_COMPOSEATTACH:
            pListBox = (CListBox*)m_pAttachmentList;
            break;
    }
	if (pWnd == (CWnd*)TABCTRL_HOME)    // this value position to address list
    {
    	pListBox->SetFocus();
        pCompose->SetFocusField(pListBox);
    	return TRUE;
    }
    else if (bControl)
    {
      TabChanging(m_iSelectedTab);
      switch (m_iSelectedTab)
      {
         case IDX_COMPOSEADDRESS:
            TabChanged(bShift ? IDX_COMPOSEOPTIONS : IDX_COMPOSEATTACH);
            break;
         case IDX_COMPOSEATTACH:
            TabChanged(bShift ? IDX_COMPOSEADDRESS: IDX_COMPOSEOPTIONS);
            break;
         case IDX_COMPOSEOPTIONS:
            TabChanged(bShift ? IDX_COMPOSEATTACH : IDX_COMPOSEADDRESS);
            break;
      }
      
      return TRUE;
    }
	else if (pWnd == pCompose->GetEditorWnd()) 
    {
    	// if the editor has focus and the shift-tab was hit, position to the
    	// subject field
    	if (bShift)
    	{
    	    m_pSubjectEdit->SetFocus();
            pCompose->SetFocusField(m_pSubjectEdit);
    	    return TRUE;
    	}
    }
    else if (pWnd == m_pSubjectEdit)
    {
        // if the subject field has focus, give focus to the address list
        // if shift-tab was pressed.
        if (bShift)
        {
            switch (m_iSelectedTab)
            {
                case IDX_COMPOSEATTACH:
                case IDX_COMPOSEADDRESS:
                    pListBox->SetFocus();
                    pCompose->SetFocusField(pListBox);
                    break;
                case IDX_COMPOSEOPTIONS:
                    if (pCompose->UseHtml())
                    {
                        m_pMessageFormat->SetFocus();
                        pCompose->SetFocusField(m_pMessageFormat);
                    }
                    else
                    {
                        m_pPriority->SetFocus();
                        pCompose->SetFocusField(m_pPriority);
                    }
                    break;
            }
            return TRUE;
        }
        else if (pCompose && pCompose->GetEditorWnd())
        {
            pCompose->GetEditorWnd()->SetFocus();
            pCompose->SetFocusField(pCompose->GetEditorWnd());
        }
        return TRUE;
    } 

	return FALSE;   // couldn't figure out who to give focus to
}

BOOL CComposeBar::OnEraseBkgnd(CDC* pDC)
{
    CRect WinRect, rect;
    GetClientRect(WinRect);
    GetWidgetRect(WinRect,rect);
    rect.InflateRect(-2,-2);
    CBrush brush(GetSysColor(COLOR_BTNFACE)), *pOldBrush;
    CPen pen(PS_SOLID,1,GetSysColor(COLOR_BTNFACE)), *pOldPen;
    pOldBrush = pDC->SelectObject(&brush);
    pOldPen = pDC->SelectObject(&pen);

    if ((m_iSelectedTab != IDX_COMPOSEOPTIONS) && !m_bClosed)
    {
        CRect fillrect;
        fillrect = WinRect;
        fillrect.right = rect.left + 2;
        pDC->Rectangle(fillrect);
        fillrect = WinRect;
        fillrect.left = rect.right;
        pDC->Rectangle(fillrect);
        fillrect = WinRect;
        fillrect.bottom = rect.top;
        pDC->Rectangle(fillrect);
        fillrect = WinRect;
        fillrect.top = rect.bottom;
        pDC->Rectangle(fillrect);
    }
    else
        pDC->Rectangle(WinRect);


    pDC->SelectObject(pOldBrush);
    pDC->SelectObject(pOldPen);
    return TRUE;
}
/////////////////////////////////////////////////////////////////////////////
// OnLeavingLastField() is a responded to the WM_LEAVINGLASTFIELD message
// which is generated by the address list widget.  When the focus is on
// the last valid entry in the address list and tab is pressed, this
// message is sent.

LONG CComposeBar::OnLeavingLastField(UINT, LONG)
{
    // when leaving the last field of the address widget, give focus to
    // the subject field
	if (m_pSubjectEdit)
    {
    	CComposeFrame *pCompose = (CComposeFrame *)GetParent();
		m_pSubjectEdit->SetFocus();
        pCompose->SetFocusField(m_pSubjectEdit);
    }
	return 0;
}

void CComposeBar::OnLButtonUp( UINT nFlags, CPoint point )
{
    ReleaseCapture();
    m_bSizing = FALSE;
    CDialogBar::OnLButtonUp(nFlags,point);
}

void CComposeBar::OnLButtonDown( UINT nFlags, CPoint point )
{
    if (m_bCanSize && !m_bClosed)
    {
        SetCapture();
        m_iY = point.y;
        m_bSizing = TRUE;
    }
    else if (!m_bClosed && !collapser.ButtonPress(point))
    {
       for (int i = 0; i < MAX_TIPS; i++)
       {
           if (m_ToolTipInfo[i].m_rect.PtInRect(point))
           {
               if (i != m_iSelectedTab)
               {
                   SendMessage(WM_COMMAND,(WPARAM)m_ToolTipInfo[i].m_idCommand);
                   Invalidate();
                   break;
               }
           }
       }
    }
    CDialogBar::OnLButtonDown(nFlags,point);
}

BOOL CComposeBar::OnSetCursor( CWnd* pWnd, UINT nHitTest, UINT message )
{
    if (!m_bClosed)
    {
    	CRect WinRect;
        POINT point;
        GetCursorPos(&point);
        ScreenToClient(&point);
        GetClientRect(WinRect);
        CRect rect;
        if (m_pWidget)
        {
            m_pWidget->GetWindowRect(rect);
            ScreenToClient(rect);
            WinRect.top = rect.bottom;
            WinRect.bottom = WinRect.top + GetSystemMetrics(SM_CYFRAME)*2;
            if ( nHitTest == HTCLIENT && PtInRect( &WinRect, point ))
            {
                SetCursor( theApp.LoadCursor ( AFX_IDC_VSPLITBAR ) );
                m_bCanSize = TRUE;
                return TRUE;
            }
        }
        m_bCanSize = FALSE;
    }
	return CDialogBar::OnSetCursor( pWnd, nHitTest, message );
}

void CComposeBar::OnMouseMove( UINT nFlags, CPoint point )
{
    if (m_bSizing) 
    {
        if (m_iY != point.y)
        {
    	    m_iHeight += (point.y - m_iY);
    	    m_iHeight = max(m_iHeight,m_iMinSize);
    	    m_iHeight = min(m_iHeight,m_iMaxSize);
	        m_iY = point.y;
	        GetParentFrame()->RecalcLayout();
	        CalcFieldLayout();
	        Invalidate();
	        UpdateWindow();
        }
    }
    CDialogBar::OnMouseMove(nFlags,point);
    if (!m_bClosed)
        collapser.MouseAround(point);
}

static SIZE sizeSelTab = { 2, 2 };

BOOL CComposeBar::IsAttachmentsMailOnly(void)
{
	CComposeFrame *pCompose = (CComposeFrame *)GetParent();
   if (pCompose->GetMsgPane())
   {
      const MSG_AttachmentData * pDataList = MSG_GetAttachmentList(pCompose->GetMsgPane());
      if(pDataList) 
      {
	      for (int i = 0; pDataList[i].url!=NULL; i++) 
	         if (strncmp(pDataList[i].url,"mailbox:",8))
    		      return FALSE;
      }
      return TRUE;
   }
   return FALSE;
}


void CComposeBar::DrawVerticalTab(CDC & dc, int index, CRect &rect)
{
    int x = m_cxChar + COMP_VTAB_WIDTH + COMP_LEFT_MARGIN;
    int y = PIXEL_OFFSET_Y + 5;
    BOOL bSelected = m_iSelectedTab == index;

    rect.SetRect(
    	x,
    	y+(index*TAB_SPACING_Y),
    	0,0);
    rect.right = rect.left + TAB_SPACING_X - 1;
    rect.bottom = rect.top + TAB_SPACING_Y;

    HPEN hPenHilite, hPenShadow, hPenDark;
    hPenHilite = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_BTNHIGHLIGHT ) );
    hPenShadow = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_BTNSHADOW ) );
#ifdef XP_WIN32
    hPenDark = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_3DDKSHADOW ) );    
    HPEN hPenLite = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_3DLIGHT ) );
#else
    hPenDark = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_BTNTEXT ) );       
    HPEN hPenLite = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_BTNHIGHLIGHT ) );
#endif

    if (bSelected)
	{
	    InflateRect(&rect, sizeSelTab.cx, sizeSelTab.cy);
		rect.right++;
    	rect.bottom -= 2;
	}

    HBRUSH hBrush = (HBRUSH)(COLOR_BTNFACE + 1);
    ::FillRect(dc.GetSafeHdc(), &rect, hBrush);

    rect.right -= 2;
    rect.bottom += 2;

    HPEN hOldPen = (HPEN)dc.SelectObject(hPenDark);
    dc.MoveTo(rect.right - (bSelected ? 2 : 0),rect.bottom-1);
    dc.LineTo(rect.left+2,rect.bottom-1);
    dc.LineTo(rect.left,rect.bottom-3);
    dc.SelectObject(hPenHilite);
    dc.LineTo(rect.left,rect.top+2);
    dc.LineTo(rect.left+2,rect.top);
    dc.LineTo(rect.right + ((bSelected&&!index) ? 2 : 0 ),rect.top);

    dc.SelectObject(hPenShadow);
    dc.MoveTo(rect.right - (bSelected ? 1 : 0),rect.bottom-2);
    dc.LineTo(rect.left+2,rect.bottom-2);
    dc.LineTo(rect.left+1,rect.bottom-3);
    dc.SelectObject(hPenLite);
    dc.LineTo(rect.left+1,rect.top+2);
    dc.LineTo(rect.left+2,rect.top+1);
    if (!bSelected && index)
    	dc.LineTo(rect.right + ((bSelected&&!index) ? 2 : 0),rect.top+1);

    //update the attachment count here since the attachments may have come
    //from the command line and need a way of notifying the control.
    if (GetTotalAttachments() && !m_iTotalAttachments)
        m_iTotalAttachments = GetTotalAttachments();

    int idxImage = index;
    if (index == IDX_COMPOSEATTACH)
    {
    	if (m_iTotalAttachments > 0)
    	{
    	    if (IsAttachmentsMailOnly())
        		idxImage = IDX_COMPOSEATTACHMAIL;
    	    else
	        	idxImage = IDX_COMPOSEATTACHFILE;
    	}
    }
    m_pIImage->DrawImage(
    	idxImage,
    	rect.left + (TAB_SPACING_X-TAB_ICON_SIZE)/2,
    	rect.top + (TAB_SPACING_Y-TAB_ICON_SIZE)/2,
    	dc.GetSafeHdc(), TRUE);

	if (hOldPen != NULL)
		dc.SelectObject(hOldPen);

    ::DeleteObject((HGDIOBJ) hPenHilite);
    ::DeleteObject((HGDIOBJ) hPenShadow);
    ::DeleteObject((HGDIOBJ) hPenDark);
    ::DeleteObject((HGDIOBJ) hPenLite);
}

void CComposeBar::GetWidgetRect(CRect &WinRect, CRect &rect)
{
	 int y = PIXEL_OFFSET_Y;
    int x = TAB_SPACING_X + 6 + COMP_VTAB_WIDTH + COMP_LEFT_MARGIN;
    int iHeight = (WinRect.Height() - (m_iBoxHeight * 3)/2) - 5;
    rect.SetRect(
		x, y, 
		WinRect.Width()-4, 
		iHeight);
}

void CComposeBar::Draw3DStaticEdgeSimulation(CDC & dc, CRect &rect, BOOL bReverse)
{
    CPen ShadowPen (PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
    CPen HilitePen (PS_SOLID, 1, GetSysColor(COLOR_BTNHIGHLIGHT));
    CPen * pOldPen = dc.SelectObject(bReverse ? &HilitePen : &ShadowPen);

    dc.MoveTo(rect.left,rect.bottom-1);
    dc.LineTo(rect.left,rect.top);
    dc.LineTo(rect.right-1,rect.top);
    dc.SelectObject(bReverse ? &ShadowPen : &HilitePen);
    dc.LineTo(rect.right-1,rect.bottom-1);
    dc.LineTo(rect.left-1,rect.bottom-1);

    dc.SelectObject(pOldPen);
}

void CComposeBar::OnPaint()
{
   CPaintDC dc(this);

   // draw shadow around address widget and line at top
   // of compose bar
   CRect WinRect, rect;
   GetClientRect(&WinRect);

   GetWidgetRect(WinRect,rect);
   if (!m_bClosed)
   {
      rect.left = COMP_VTAB_WIDTH + 2;
      rect.InflateRect(2,2);
      Draw3DStaticEdgeSimulation(dc, rect, TRUE);
   }

   if (m_pSubjectEdit && m_pSubjectEdit->IsWindowVisible())
   {
      m_pSubjectEdit->GetWindowRect(rect);
      ScreenToClient(rect);
      rect.InflateRect(1,1);
      Draw3DStaticEdgeSimulation(dc, rect);
   }

   if (!m_bClosed)
   {
	   GetWidgetRect(WinRect,rect);
	   rect.InflateRect(-1,-1);
	   rect.left = COMP_VTAB_WIDTH + 3;
	   Draw3DStaticEdgeSimulation(dc, rect);
   }

   if (!m_bClosed)
   {
      HPEN hPenHilite = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_BTNHIGHLIGHT ) );
      HPEN hPenShadow = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_BTNSHADOW ) );
      HPEN hOldPen = (HPEN)dc.SelectObject(hPenShadow);
	   GetWidgetRect(WinRect,rect);
      rect.InflateRect(2,0);
      rect.top++ ;
      rect.bottom -= 2;
      dc.MoveTo(rect.left,rect.top);
      dc.LineTo(rect.left,rect.bottom);
      dc.SelectObject(hPenHilite);
      dc.MoveTo(rect.left + 2,rect.top);
      dc.LineTo(rect.left + 2,rect.bottom);
      CPoint point1(rect.left+1,rect.top);
      CPoint point2(rect.left+1,rect.bottom);
      dc.SetPixel(point1, GetSysColor(COLOR_BTNFACE));
      dc.SetPixel(point2, GetSysColor(COLOR_BTNFACE));
      if (hOldPen != NULL)
		   dc.SelectObject(hOldPen);
      ::DeleteObject((HGDIOBJ) hPenHilite);
      ::DeleteObject((HGDIOBJ) hPenShadow);

      // paint the tabs here
      for (int i =0; i< TOTAL_TABS; i++)
      {
         if (i != m_iSelectedTab)
         {
            DrawVerticalTab(dc,i, rect);
            rect.InflateRect(0,-3);
            m_ToolTipInfo[i].m_rect = rect;

         }
      }
      DrawVerticalTab(dc,m_iSelectedTab,rect);
      rect.InflateRect(0,-3);
      m_ToolTipInfo[m_iSelectedTab].m_rect = rect;
   }
   else
   {
      for (int i =0; i< TOTAL_TABS; i++)
      {
         m_ToolTipInfo[i].m_rect.SetRect(0,0,0,0);
      }
   }

   int iHeight = (WinRect.Height() - (m_iBoxHeight * 3)/2) - 3;

   if (!m_bClosed){
      collapser.DrawCollapseWidget(dc, collapse_open, FALSE, WinRect.Height() - iHeight);
      collapser.GetRect(m_ToolTipInfo[IDX_TOOL_COLLAPSE].m_rect);
   }
}

void CComposeBar::ShowTab(int idx)
{
}

int CComposeBar::GetTab()
{
    return m_iSelectedTab;
}

void CComposeBar::Cleanup(void)
{
    TabChanging(m_iSelectedTab);
    if (m_pAttachmentList && ::IsWindow(m_pAttachmentList->m_hWnd))
    {
        m_pAttachmentList->DestroyWindow();
        delete m_pAttachmentList;
    }
    DestroyStandardFields();
}

void CComposeBar::TabChanging(int tab)
{
    switch (tab)
    {
        case IDX_COMPOSEADDRESS:
            DestroyAddressPage();
            break;
        case IDX_COMPOSEATTACH:
            DestroyAttachmentsPage();
            break;
        case IDX_COMPOSEOPTIONS:
            DestroyOptionsPage();
            break;
        default:
            ASSERT(0);
   }

}

void CComposeBar::TabChanged(int tab)
{
    m_pIAddressList->EnableParsing(tab == IDX_COMPOSEADDRESS);
   m_iSelectedTab = tab;
   switch (m_iSelectedTab) 
   {
       case IDX_COMPOSEADDRESS:
    	   CreateAddressPage();
           m_pWidget->SetFocus();
    	   break;
       case IDX_COMPOSEATTACH:
    	   CreateAttachmentsPage();
    	   m_pWidget->SetFocus();
    	   break;
       case IDX_COMPOSEOPTIONS:
    	   CreateOptionsPage();
           m_pReturnReceipt->SetFocus();
    	   m_pWidget = NULL;
    	   break;
       default:
	       ASSERT(0);
   }

   GetParentFrame()->RecalcLayout();
   
   if (!m_pSubjectEdit)
      CreateStandardFields();
      
   Invalidate();
}

void CComposeBar::OnAddressTab(void)
{
    if (m_iSelectedTab != IDX_COMPOSEADDRESS)
    {
    	TabChanging(m_iSelectedTab);
    	TabChanged(IDX_COMPOSEADDRESS);
    }
}

void CComposeBar::OnAttachTab(void)
{
    if (m_iSelectedTab != IDX_COMPOSEATTACH)
    {
    	TabChanging(m_iSelectedTab);
    	TabChanged(IDX_COMPOSEATTACH);
    }
}

void CComposeBar::OnOptionsTab(void)
{
    if (m_iSelectedTab != IDX_COMPOSEOPTIONS)
    {
    	TabChanging(m_iSelectedTab);
    	TabChanged(IDX_COMPOSEOPTIONS);
    }
}

void CComposeBar::OnToggleShow(void)
{
    if (m_bHidden)
    {
        m_bHidden = FALSE;
        // Always uncollapse when showing
        if (m_bClosed)
        {
            OnCollapse();
        } 
        else if (m_pWidget && ::IsWindow(m_pWidget->m_hWnd))
        {
        	m_pWidget->ShowWindow(SW_NORMAL);
        }
    } else {
        m_bHidden = TRUE;

        // Always collapse when hidding
        if (!m_bClosed) {
            OnCollapse();
        } 
        else if (m_pWidget && ::IsWindow(m_pWidget->m_hWnd))
        {
        	m_pWidget->ShowWindow(SW_HIDE);
        	Invalidate();
        }
        // The only difference between Hiding and collapsing is the 
        //  removal of the collapsed tab
        CGenericFrame * pFrame = (CGenericFrame*)GetParent();
        CCustToolbar * pToolbar = pFrame->GetChrome()->GetCustomizableToolbar();
        if( pToolbar ){
            pToolbar->RemoveExternalTab(1);
        }
       	GetParentFrame()->RecalcLayout();
    }
}

void CComposeBar::OnCollapse(void)
{
    if (!m_bClosed)
    {
    	m_iPrevHeight = m_iHeight;
    	m_bClosed = TRUE;
    	if (m_pWidget && ::IsWindow(m_pWidget->m_hWnd))
    	    m_pWidget->ShowWindow(SW_HIDE);
    	m_iHeight = m_iBoxHeight + /*COMP_HTAB_HEIGHT +*/ 13;
    	if (m_iSelectedTab == IDX_COMPOSEOPTIONS)
    	    DestroyOptionsPage();

        CGenericFrame * pFrame = (CGenericFrame*)GetParent();
        CCustToolbar * pToolbar = pFrame->GetChrome()->GetCustomizableToolbar();
        if( pToolbar ){
            pToolbar->AddExternalTab(pFrame, eLARGE_HTAB, IDS_COMPOSECLOSE,  1);
        }
    	GetParentFrame()->RecalcLayout();
    	Invalidate();
    }
    else 
    {
    	m_bClosed = FALSE;
    	m_iHeight = m_iPrevHeight;
    	GetParentFrame()->RecalcLayout();
    	if (m_iSelectedTab == IDX_COMPOSEADDRESS || m_iSelectedTab == IDX_COMPOSEATTACH)
    	    m_pWidget->ShowWindow(SW_NORMAL);
    	if (m_iSelectedTab == IDX_COMPOSEOPTIONS)
    	    CreateOptionsPage();
    	Invalidate();
    }
}

#define BORDER_WIDTH 5

int CComposeBar::GetTotalAttachments(void)
{
	CComposeFrame *pCompose = (CComposeFrame *)GetParent();
	if (pCompose->GetMsgPane())
	{
		const MSG_AttachmentData * pDataList = MSG_GetAttachmentList(pCompose->GetMsgPane());
		if(pDataList) 
		{
		for (int i = 0; pDataList[i].url!=NULL; i++) 
			;
		return i;
		}
	}
    return 0;
}

void CComposeBar::OnSize( UINT nType, int cx, int cy )
{
	CDialogBar::OnSize( nType, cx, cy );
    if (m_pSubjectEdit)
	CalcFieldLayout();
}

void CComposeBar::OnButtonAttach(void)
{
    CWnd * pWnd = GetFocus();
    GetParent()->SendMessage(WM_COMMAND,ID_FILE_ATTACH);
    if (pWnd)
       pWnd->SetFocus();
}

void CComposeBar::OnTimer( UINT  nIDEvent )
{
   if( !m_bClosed )
       collapser.TimerEvent(nIDEvent);
   else 
      KillTimer(nIDEvent);

   CDialogBar::OnTimer(nIDEvent);
}

int CComposeBar::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
    int retval = CDialogBar::OnCreate ( lpCreateStruct );

    CComposeFrame *pCompose = (CComposeFrame *)GetParent();
    collapser.Initialize(this,IDC_COLLAPSE);

    m_pszMessageFormat = strdup(szLoadString(pCompose->UseHtml() ? IDS_FORMAT_ASKME : IDS_FORMAT_PLAIN));

	ApiApiPtr(api);
    m_pUnkAddressControl = api->CreateClassInstance(
	    APICLASS_ADDRESSCONTROL, NULL, (APISIGNATURE)pCompose);
    m_pUnkAddressControl->QueryInterface(IID_IAddressControl,(LPVOID*)&m_pIAddressList);

    m_pIAddressList->SetControlParent(this);
	m_pIAddressList->SetContext(m_pContext);
    // Static text is translated to many languages
    // That's why we choose font name based on resource definition.
	CClientDC dc(this);

	LOGFONT lf;
    memset(&lf,0,sizeof(LOGFONT));
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
    strcpy(lf.lfFaceName, szLoadString(IDS_FONT_PROPNAME));
    lf.lfHeight = -MulDiv(8, dc.GetDeviceCaps(LOGPIXELSY), 72);
    lf.lfQuality = PROOF_QUALITY;    
	m_cfStaticFont = theApp.CreateAppFont( lf );

	SetCSID(INTL_DefaultWinCharSetID(0)); 

    m_ToolTipInfo[IDX_TOOL_ADDRESS].Initialize(IDS_ADDRESSMESSAGE,CRect(0,0,0,0),IDC_ADDRESSTAB);
    m_ToolTipInfo[IDX_TOOL_ATTACH].Initialize(IDS_ATTACHFILE,CRect(0,0,0,0),IDC_ATTACHTAB);
    m_ToolTipInfo[IDX_TOOL_OPTIONS].Initialize(IDS_COMPOSEOPTIONS,CRect(0,0,0,0),IDC_OPTIONSTAB);
    m_ToolTipInfo[IDX_TOOL_COLLAPSE].Initialize(IDS_COMPOSEOPEN,CRect(0,0,0,0),IDC_COLLAPSE);
    
    m_pToolTip = new CNSToolTip2;
    m_pToolTip->Create(this, TTS_ALWAYSTIP);
    m_pToolTip->SetDelayTime(250);
    m_pToolTip->Activate(TRUE);

    EnableToolTips(TRUE);

   UpdateFixedSize();
   if(!m_pDropTarget) {
       m_pDropTarget = new CNSAttachDropTarget;
       m_pDropTarget->Register(this);
   }
   DragAcceptFiles();
   return retval;
}
    
void CComposeBar::SetCSID(int iCSID)
{
   CClientDC pdc ( this );
   LOGFONT lf;

   if (m_cfTextFont) {
      theApp.ReleaseAppFont(m_cfTextFont);
   }
   if (m_cfSubjectFont) {
      theApp.ReleaseAppFont(m_cfSubjectFont);
   }
   memset(&lf,0,sizeof(LOGFONT));
   if (iCSID == CS_LATIN1)
      strcpy(lf.lfFaceName, "MS Sans Serif");
   else
      strcpy(lf.lfFaceName, IntlGetUIPropFaceName(iCSID));
   lf.lfCharSet = IntlGetLfCharset(iCSID);
   lf.lfHeight = -MulDiv(8,pdc.GetDeviceCaps(LOGPIXELSY), 72);
   lf.lfQuality = PROOF_QUALITY;    

   lf.lfPitchAndFamily = FF_SWISS;
	m_cfTextFont = theApp.CreateAppFont( lf );

   XP_MEMSET(&lf,0,sizeof(LOGFONT));
   lf.lfPitchAndFamily = FF_MODERN | FIXED_PITCH;
   strcpy(lf.lfFaceName, IntlGetUIFixFaceName(iCSID));
   lf.lfCharSet = IntlGetLfCharset(iCSID); 
   lf.lfHeight = -MulDiv(9,pdc.GetDeviceCaps(LOGPIXELSY), 72);
   lf.lfQuality = PROOF_QUALITY;    
	m_cfSubjectFont = theApp.CreateAppFont( lf );

	CClientDC dc(this);
	CFont * pOldFont = dc.SelectObject(CFont::FromHandle(m_cfSubjectFont));
   TEXTMETRIC tm;
   dc.GetTextMetrics(&tm);
   m_iBoxHeight = tm.tmHeight + tm.tmExternalLeading;
   m_cxChar = tm.tmAveCharWidth;
   CString cs;
   cs.LoadString(IDS_COMPOSE_SUBJECT);
   m_iFirstX = dc.GetTextExtent(cs,cs.GetLength()).cx + (m_cxChar * 2);
   dc.SelectObject(pOldFont);
   
	if (m_pSubjectEdit) {
		::SendMessage(m_pSubjectEdit->GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfSubjectFont, FALSE);
		CalcFieldLayout();
	}
	if (m_pIAddressList) {
		m_pIAddressList->SetCSID(iCSID);
		CListBox * pListBox = m_pIAddressList->GetListBox();
		if (pListBox && pListBox->m_hWnd)
		{
			m_iMinSize = pListBox->GetItemHeight(0)*4;
			m_iMinSize += (14 + PIXEL_OFFSET_Y + m_iBoxHeight);
			if (m_iHeight < m_iMinSize) {
				m_iHeight = m_iMinSize;
			}
		}
	}
	UpdateFixedSize();
}

void CComposeBar::UpdateAttachmentInfo(int total)
{
    if (total != m_iTotalAttachments)
    {
        m_iTotalAttachments = total;
        Invalidate();
    }
}



void CComposeBar::UpdateFixedSize ( void )    
{

}

CSize CComposeBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)   
{
	CSize size;
	size.cx = (bStretch && bHorz ? 32767 : 8 );
	size.cy = GetHeightNeeded ( );
	return size;
}

void CComposeBar::UpdateHeaderInfo ( void )
{
	CComposeFrame *pCompose = (CComposeFrame *)GetParent();
    MSG_Pane *pComposePane = pCompose->GetMsgPane();
    ASSERT(pComposePane);
    UpdateOptionsInfo();
    if (m_pIAddressList->IsCreated())
    {
        MSG_HeaderEntry * entry = NULL;
        CListBox * pTypeList = m_pIAddressList->GetAddressTypeComboBox();

        // read all the entries (minus hidden fields) from the list
        CListBox * pListBox = m_pIAddressList->GetListBox();
        int count = pListBox->GetCount();
        int iTotalFields = count;
        int iTypeCount = pTypeList->GetCount();
        int i;

        for (i = 0; i < iTypeCount; i++)
        {
            int32 bHidden;
            char * pszValue;
            m_pIAddressList->GetTypeInfo(i,ADDRESS_TYPE_FLAG_HIDDEN, (void**)&bHidden);
            m_pIAddressList->GetTypeInfo(i,ADDRESS_TYPE_FLAG_VALUE,  (void**)&pszValue);
   	        if (bHidden && pszValue)
               iTotalFields++;
        }
        if (count)
	        entry = (MSG_HeaderEntry*)XP_ALLOC(sizeof(MSG_HeaderEntry)*iTotalFields);
        MSG_ClearComposeHeaders(pComposePane);
        if (entry == NULL)
	        return;
        int iRealCount = 0;
        for (i=0; i<count; i++)
        {
            char * szName, * szType;
	        if(m_pIAddressList->GetEntry(i,&szType, &szName))
	        {
#ifdef MOZ_NEWADDR
				AB_NameCompletionCookie *pCookie = NULL;
				int nNumResults;
				m_pIAddressList->GetNameCompletionCookieInfo(&pCookie, &nNumResults, i);

				if(pCookie && nNumResults == 1)
				{
					entry[iRealCount].header_value = 
						AB_GetExpandedHeaderString(pCookie);
			
				}
				else

#endif
		        entry[iRealCount].header_value = XP_STRDUP(szName);
		        if (!strcmp(szType,szLoadString(IDS_ADDRESSTO)))
			        entry[iRealCount].header_type = MSG_TO_HEADER_MASK;
		        else if(!strcmp(szType,szLoadString(IDS_ADDRESSCC)))
			        entry[iRealCount].header_type = MSG_CC_HEADER_MASK;
		        else if(!strcmp(szType,szLoadString(IDS_ADDRESSBCC)))
			        entry[iRealCount].header_type = MSG_BCC_HEADER_MASK;
		        else if(!strcmp(szType,szLoadString(IDS_ADDRESSNEWSGROUP)))
			        entry[iRealCount].header_type = MSG_NEWSGROUPS_HEADER_MASK;
		        else if(!strcmp(szType,szLoadString(IDS_ADDRESSFOLLOWUPTO)))
			        entry[iRealCount].header_type = MSG_FOLLOWUP_TO_HEADER_MASK;
		        else if(!strcmp(szType,szLoadString(IDS_ADDRESSREPLYTO)))
			        entry[iRealCount].header_type = MSG_REPLY_TO_HEADER_MASK;
		        iRealCount++;
	        }
        }

        // read all hidden fields from the list
        for (int j = 0; j < iTypeCount; j++)
        {
            int32 bHidden;
            char * pszValue;
            MSG_HEADER_SET header_set;
            m_pIAddressList->GetTypeInfo(j,ADDRESS_TYPE_FLAG_HIDDEN,(void**)&bHidden);
            m_pIAddressList->GetTypeInfo(j,ADDRESS_TYPE_FLAG_VALUE, (void**)&pszValue);
            m_pIAddressList->GetTypeInfo(j,ADDRESS_TYPE_FLAG_USER,  (void**)&header_set);
   	        if (bHidden && pszValue)
	        {
		        entry[iRealCount].header_type = header_set;
		        entry[iRealCount].header_value = XP_STRDUP(pszValue);
		        iRealCount++;
	        }
        }

        MSG_HeaderEntry * list;     
        count = MSG_CompressHeaderEntries(entry,iRealCount,&list);
        MSG_SetHeaderEntries(pComposePane,list,count);

        if (m_pSubjectEdit)
        {
	        CString cs;
	        m_pSubjectEdit->GetWindowText(cs);
	        MSG_SetCompHeader(pComposePane,MSG_SUBJECT_HEADER_MASK,cs);
        }

		char untranslatedPriority[32];
		MSG_GetUntranslatedPriorityName((MSG_PRIORITY) (m_iPriorityIdx+2),
										  untranslatedPriority, 32);
        MSG_SetCompHeader(pComposePane, MSG_PRIORITY_HEADER_MASK, untranslatedPriority);
        MSG_SetCompBoolHeader(pComposePane, MSG_RETURN_RECEIPT_BOOL_HEADER_MASK, m_bReceipt);
        HG87211
      	MSG_SetCompBoolHeader(pComposePane, MSG_UUENCODE_BINARY_BOOL_HEADER_MASK, m_bUseUUENCODE);
        MSG_SetCompBoolHeader(pComposePane, MSG_ATTACH_VCARD_BOOL_HEADER_MASK, m_bAttachVCard);

	    if (!strcmp(m_pszMessageFormat,szLoadString(IDS_FORMAT_ASKME)))
            MSG_SetHTMLAction(pComposePane,MSG_HTMLAskUser);
        else if (!strcmp(m_pszMessageFormat,szLoadString(IDS_FORMAT_PLAIN)))
            MSG_SetHTMLAction(pComposePane,MSG_HTMLConvertToPlaintext);
        else if (!strcmp(m_pszMessageFormat,szLoadString(IDS_FORMAT_HTML)))
            MSG_SetHTMLAction(pComposePane,MSG_HTMLSendAsHTML);
        else if (!strcmp(m_pszMessageFormat,szLoadString(IDS_FORMAT_BOTH)))
            MSG_SetHTMLAction(pComposePane,MSG_HTMLUseMultipartAlternative);
        else
            ASSERT(0);
    }
}

void CComposeBar::DisplayHeaders ( MSG_HEADER_SET headers )
{
    DestroyAttachmentsPage();
    DestroyAddressPage();
	::SendMessage(GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfTextFont, FALSE);
    CreateStandardFields();
    CreateAddressPage();
    GetParentFrame()->PostMessage(WM_IDLEUPDATECMDUI);
}

int CComposeBar::GetHeightNeeded ( void )
{
    return m_iHeight;
}                 

void CComposeBar::DestroyAttachmentsPage()
{
    if (m_pAttachmentList && ::IsWindow(m_pAttachmentList->m_hWnd))
        m_pAttachmentList->ShowWindow(SW_HIDE);
    m_pWidget = NULL;
}

void CComposeBar::DestroyAddressPage()
{
    CListBox * pListBox = m_pIAddressList->GetListBox();
    if (pListBox && ::IsWindow(pListBox->m_hWnd))
    	pListBox->ShowWindow(SW_HIDE);
    m_pWidget = NULL;
}

void CComposeBar::DestroyStandardFields()
{
    if (m_pSubjectEdit && ::IsWindow(m_pSubjectEdit->m_hWnd))
    {
        m_pSubjectEdit->DestroyWindow();
        delete m_pSubjectEdit;
        m_pSubjectEdit = NULL;
    }
    if (m_pSubjectEditText && ::IsWindow(m_pSubjectEditText->m_hWnd))
    {
        m_pSubjectEditText->DestroyWindow();
        delete m_pSubjectEditText;
        m_pSubjectEditText = NULL;
    }
}

void CComposeBar::CreateOptionsPage()
{
    CString cs;

    // create the options on the left
    HG81325

    m_pUseUUENCODE = new CButton;
    m_pUseUUENCODE->Create(
        szLoadString(IDS_USEUUENCODE),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,
        CRect(0,0,0,0),this,1013);
	::SendMessage(m_pUseUUENCODE->GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfStaticFont, FALSE);
    m_pUseUUENCODE->SetCheck(m_bUseUUENCODE);

    m_pReturnReceipt = new CButton;
    m_pReturnReceipt->Create(
        szLoadString(IDS_RETURNRECEIPT),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,
        CRect(0,0,0,0),this,1010);
	::SendMessage(m_pReturnReceipt->GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfStaticFont, FALSE);
    m_pReturnReceipt->SetCheck(m_bReceipt);

    // create priority combo box
    m_pPriorityText = new CStatic;
    cs.LoadString(IDS_COMPOSE_PRIORITY);
    m_pPriorityText->Create( cs, WS_CHILD | WS_VISIBLE, CRect(0,0,0,0),this,1500);
	::SendMessage(m_pPriorityText->GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfStaticFont, FALSE);
    m_pPriority = new CComboBox;
    m_pPriority->Create(
        CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
        CRect(0,0,0,0), this, 1501);
	::SendMessage(m_pPriority->GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfStaticFont, FALSE);

	char priStr[32];
	MSG_GetPriorityName(MSG_LowestPriority, priStr, 32);
    m_pPriority->AddString (priStr);
	MSG_GetPriorityName(MSG_LowPriority, priStr, 32);
    m_pPriority->AddString (priStr);
	MSG_GetPriorityName(MSG_NormalPriority, priStr, 32);
    m_pPriority->AddString (priStr);
	MSG_GetPriorityName(MSG_HighPriority, priStr, 32);
    m_pPriority->AddString (priStr);
	MSG_GetPriorityName(MSG_HighestPriority, priStr, 32);
    m_pPriority->AddString (priStr);
    m_pPriority->SetCurSel(m_iPriorityIdx);

	CComposeFrame *pCompose = (CComposeFrame *)GetParent();
    m_pMessageFormatText = new CStatic;
    cs.LoadString(IDS_MESSAGEFORMAT);
    m_pMessageFormatText->Create( cs, WS_CHILD | WS_VISIBLE, CRect(0,0,0,0),this,1504);
	::SendMessage(m_pMessageFormatText->GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfStaticFont, FALSE);
    m_pMessageFormat = new CComboBox;
    m_pMessageFormat->Create(
        CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
        CRect(0,0,0,0), this, 1505);
	::SendMessage(m_pMessageFormat->GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfStaticFont, FALSE);
    m_pMessageFormat->AddString(szLoadString(IDS_FORMAT_ASKME));
    m_pMessageFormat->AddString(szLoadString(IDS_FORMAT_PLAIN));
    m_pMessageFormat->AddString(szLoadString(IDS_FORMAT_HTML));
    m_pMessageFormat->AddString(szLoadString(IDS_FORMAT_BOTH ));
    int idx = m_pMessageFormat->FindString(0,m_pszMessageFormat);
    if (idx == LB_ERR)
        m_pMessageFormat->SetCurSel(pCompose->UseHtml() ? 0 : 1);     // default to askme
    else
        m_pMessageFormat->SetCurSel(idx);

    if (!pCompose->UseHtml())
        m_pMessageFormat->EnableWindow(FALSE);
    CalcFieldLayout();
}

void CComposeBar::UpdateOptionsInfo()
{
    if (m_pReturnReceipt && ::IsWindow(m_pReturnReceipt->m_hWnd))
        m_bReceipt = m_pReturnReceipt->GetCheck();
    if (m_pPriority && ::IsWindow(m_pPriority->m_hWnd))
    {
        m_iPriorityIdx = m_pPriority->GetCurSel();
		if (m_iPriorityIdx < 0)
		  m_iPriorityIdx = 2;
    }
    if (m_pMessageFormat && ::IsWindow(m_pMessageFormat->m_hWnd))
    {
        CString cs;
        int idx = m_pMessageFormat->GetCurSel();
        m_pMessageFormat->GetLBText(idx, cs);
        if (m_pszMessageFormat)
            free(m_pszMessageFormat);
        m_pszMessageFormat = strdup(cs);
    }
	HG87322
    if (m_pUseUUENCODE && ::IsWindow(m_pUseUUENCODE->m_hWnd))
        m_bUseUUENCODE = m_pUseUUENCODE->GetCheck();
}

void CComposeBar::DestroyOptionsPage()
{
    UpdateOptionsInfo();
    if (m_pPriorityText && ::IsWindow(m_pPriorityText->m_hWnd))
    {
        m_pPriorityText->DestroyWindow();
        delete m_pPriorityText;
        m_pPriorityText = NULL;
    }
    if (m_pPriority && ::IsWindow(m_pPriority->m_hWnd))
    {
        m_pPriority->DestroyWindow();
        delete m_pPriority;
        m_pPriority = NULL;
    }
    if (m_pReturnReceipt && ::IsWindow(m_pReturnReceipt->m_hWnd))
    {
        m_pReturnReceipt->DestroyWindow();
        delete m_pReturnReceipt;
        m_pReturnReceipt = NULL;
    }

    if (m_pUseUUENCODE && ::IsWindow(m_pUseUUENCODE->m_hWnd))
    {
        m_pUseUUENCODE->DestroyWindow();
        delete m_pUseUUENCODE;
        m_pUseUUENCODE = NULL;
    }
    if (m_pMessageFormat && ::IsWindow(m_pMessageFormat->m_hWnd))
    {
        m_pMessageFormat->DestroyWindow();
        delete m_pMessageFormat;
        m_pMessageFormat = NULL;
    }
    if (m_pMessageFormatText && ::IsWindow(m_pMessageFormatText->m_hWnd))
    {
        m_pMessageFormatText->DestroyWindow();
        delete m_pMessageFormatText;
        m_pMessageFormatText = NULL;
    }
}

void CComposeBar::CreateAttachmentsPage()
{
    CString cs;                                                                                                                                                 
    // the attachment control
    MSG_Pane *pComposePane = ((CComposeFrame *)GetParent())->GetMsgPane();
    if (!m_pAttachmentList)
    {
        m_pAttachmentList = new CNSAttachmentList(pComposePane);
        m_pAttachmentList->Create(this,1550);
    }
    else if (!m_pAttachmentList->IsWindowVisible())
        m_pAttachmentList->ShowWindow(SW_SHOW);

    m_pWidget = (CWnd *)m_pAttachmentList;
    CalcFieldLayout();
}

void CComposeBar::CreateAddressPage()
{
    CreateAddressingBlock();
    CalcFieldLayout();
}


void CComposeBar::Enable3d(BOOL bEnable)
{

}

void CComposeBar::CreateAddressingBlock(void)
{
    Enable3d(FALSE);
    CListBox * pListBox = m_pIAddressList->GetListBox();
	if (!m_pIAddressList->IsCreated()) 
	{
    	m_pIAddressList->Create(this,IDC_ADDRESSLIST);
    	m_pIAddressList->AddAddressType(szLoadString(IDS_ADDRESSTO),IDB_PERSON);
    	m_pIAddressList->AddAddressType(szLoadString(IDS_ADDRESSCC),IDB_PERSON);
    	m_pIAddressList->AddAddressType(szLoadString(IDS_ADDRESSBCC),IDB_PERSON);
    	m_pIAddressList->AddAddressType(szLoadString(IDS_ADDRESSNEWSGROUP),IDB_NEWSART, FALSE);
    	m_pIAddressList->AddAddressType(szLoadString(IDS_ADDRESSREPLYTO),IDB_PERSON, FALSE, TRUE, TRUE,(DWORD)MSG_REPLY_TO_HEADER_MASK);
    	m_pIAddressList->AddAddressType(szLoadString(IDS_ADDRESSFOLLOWUPTO), IDB_PERSON, FALSE, TRUE, TRUE, (DWORD)MSG_FOLLOWUP_TO_HEADER_MASK);
		CComposeFrame *pCompose = (CComposeFrame *)GetParentFrame();
		m_pIAddressList->SetCSID(pCompose->m_iCSID);
	}
    else if (!pListBox->IsWindowVisible())
    	pListBox->ShowWindow(SW_SHOW);
    pListBox->Invalidate();
    pListBox->UpdateWindow();
	m_iMinSize = pListBox->GetItemHeight(0)*4;
	m_iMinSize += (14 + PIXEL_OFFSET_Y + m_iBoxHeight);
	if (m_iHeight < m_iMinSize)
	{
		m_iHeight = m_iMinSize;
	}
    m_pWidget = (CWnd *)pListBox;
    Enable3d(TRUE);
}



void CComposeBar::AddedItem (HWND hwnd, LONG id,int index)
{
#ifndef MOZ_NEWADDR
    char * szName = NULL;
    BOOL bRetVal = m_pIAddressList->GetEntry(index, NULL, &szName);
	if (bRetVal)
	{
		char * pszFullName = NULL;
    	ChangedItem(szName, index, hwnd, &pszFullName);
		if (pszFullName != NULL)
		{
			m_pIAddressList->SetItemName(index,pszFullName);
			free(pszFullName);
		}
	}
#endif
}

int CComposeBar::ChangedItem(char * pString, int, HWND, char** ppszFullName, unsigned long* entryID, UINT* bitmapID)
{
#ifndef MOZ_NEWADDR
	(*ppszFullName) = wfe_ExpandName(pString);
#endif
	return TRUE;
}

void CComposeBar::DeletedItem (HWND hwnd, LONG id,int index)
{
}

char * CComposeBar::NameCompletion(char * pString)
{
	return wfe_ExpandForNameCompletion(pString);
}

void CComposeBar::StartNameCompletionSearch()
{
	if(IsWindow(m_hWnd))
	{
		CComposeFrame *pComposeFrame = (CComposeFrame*)GetParent();
		if(pComposeFrame)
			pComposeFrame->GetChrome()->StartAnimation();
	}

}

void CComposeBar::StopNameCompletionSearch()
{
	if(IsWindow(m_hWnd))
	{
		CComposeFrame *pComposeFrame = (CComposeFrame*)GetParent();

		if(pComposeFrame)
			pComposeFrame->GetChrome()->StopAnimation();
	}

}

void CComposeBar::SetProgressBarPercent(int32 lPercent)
{

}

void CComposeBar::SetStatusText(const char* pMessage)
{

}

CWnd *CComposeBar::GetOwnerWindow()
{

	return GetParent();
}

void CComposeBar::CreateStandardFields(void)
{
    // create subject edit field
    m_pSubjectEditText = new CStatic;
    CString cs;
    cs.LoadString(IDS_COMPOSE_SUBJECT);
    m_pSubjectEditText->Create( cs, WS_CHILD | WS_VISIBLE, CRect(0,0,0,0), this, 1500 );
	::SendMessage(m_pSubjectEditText->GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfStaticFont, FALSE);

    m_pSubjectEdit = new CComposeSubjectEdit;
    m_pSubjectEdit->Create(
    	WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 
    	CRect(0,0,0,0), (CWnd *) this, IDC_SUBJECT_EDIT );
	::SendMessage(m_pSubjectEdit->GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfSubjectFont, FALSE);
#ifdef XP_WIN32
	if (sysInfo.m_bWin4)
	{
		::SendMessage(m_pSubjectEdit->m_hWnd, EM_SETMARGINS, EC_LEFTMARGIN, 0L);
		::SendMessage(m_pSubjectEdit->m_hWnd, EM_SETMARGINS, EC_RIGHTMARGIN, 0L);
	}
#endif
    m_pSubjectEdit->SetWindowText( "" );
}

// This function recalculates the field layout based on the window width
// for the subject, priority, address widget, and buttons...
// This function assumes that the fields are created already.

#define FIELD_PIXEL_OFFSET  3
#define TEXT_PIXEL_OFFSET   4

void CComposeBar::CalcFieldLayout(void)
{
    CRect WinRect, rect;
    GetClientRect(WinRect);
    int x = WinRect.Width();
    int controlWidth;
    
    CDC * pDC = m_pSubjectEditText->GetDC();
    CString cs;
    m_pSubjectEditText->GetWindowText(cs);
#ifdef _WIN32
    controlWidth = pDC->GetTextExtent(cs).cx - 16;
#else
    controlWidth = pDC->GetTextExtent(LPCTSTR(cs), cs.GetLength()).cx - 16;
#endif
    ReleaseDC(pDC);

    pDC = GetDC();
    int pix_per_inch_x = pDC->GetDeviceCaps(LOGPIXELSX);
    int pix_per_inch_y = pDC->GetDeviceCaps(LOGPIXELSY);
    ReleaseDC(pDC);

    GetWidgetRect(WinRect,rect);
    rect.bottom = WinRect.Height() - 2;
    CRect rect2 = rect;
    rect2.bottom -= 2;
    rect2.left = COMP_VTAB_WIDTH + COMP_LEFT_MARGIN;
    rect2.right = rect2.left + controlWidth;
    rect2.top = rect2.bottom - m_iBoxHeight;

    m_pSubjectEditText->MoveWindow(rect2);

    rect2.left = rect2.right + 4;
    rect2.right = rect.right;
    rect2.top--;
    m_pSubjectEdit->MoveWindow(rect2);
    
    // resize and position the buttons.
    BOOL bHasButtons = FALSE;

    x = m_cxChar + TAB_SPACING_X + 4;
    int y = PIXEL_OFFSET_Y + 2;
    int iButtonWidth = 0;

    CRect parentRect;
    GetParentFrame()->GetWindowRect(parentRect);
    m_iMaxSize = parentRect.Height()-(m_iBoxHeight*3);
    if (m_iMaxSize <= 0)
       m_iMaxSize = m_iBoxHeight*3;

    if (m_pWidget)
    {
    	ASSERT(::IsWindow(m_pWidget->m_hWnd));
		GetWidgetRect(WinRect,rect);
		int pix_x = 2; //pix_per_inch_x/PIX_X_FACTOR;
		int pix_y = 2; //pix_per_inch_y/PIX_Y_FACTOR;
		rect.InflateRect(-pix_x,-pix_y);
		rect.left += pix_x;
		m_pWidget->MoveWindow(rect);
    }

    // options page
    if (m_iSelectedTab == IDX_COMPOSEOPTIONS && 
       m_pPriority && ::IsWindow(m_pPriority->m_hWnd))
    {
	    // allign the text
	    rect.top = (PIXEL_OFFSET_Y+TEXT_PIXEL_OFFSET);
	    rect.bottom = rect.top + m_iBoxHeight;
	    rect.left = x + m_cxChar * 3;
	    rect.right = (WinRect.Width()/2) + 24;
	    HG76528

	    rect.top = rect.bottom + 1;
	    rect.bottom = rect.top + m_iBoxHeight;
	    m_pUseUUENCODE->MoveWindow(rect);

	    ASSERT(::IsWindow(m_pPriorityText->m_hWnd));
	    pDC = m_pPriorityText->GetDC();
	    m_pPriorityText->GetWindowText(cs);
#ifdef _WIN32
	    controlWidth = pDC->GetTextExtent(cs).cx - 16;
#else
	    controlWidth = pDC->GetTextExtent(LPCTSTR(cs), cs.GetLength()).cx - 16;
#endif
        m_pMessageFormatText->GetWindowText(cs);
#ifdef _WIN32
	    controlWidth = max(controlWidth,pDC->GetTextExtent(cs).cx - 16);
#else
	    controlWidth = max(controlWidth,pDC->GetTextExtent(LPCTSTR(cs), cs.GetLength()).cx - 16);
#endif
	    ReleaseDC(pDC);

       CRect rect3;
 		 GetWidgetRect(WinRect,rect3);
	    rect.top = (PIXEL_OFFSET_Y+TEXT_PIXEL_OFFSET);
	    rect.bottom = rect.top + m_iBoxHeight;
	    rect.left = rect.right + 2;
       rect.right = rect3.right - 2;
       m_pReturnReceipt->MoveWindow(rect);

	    rect.right = rect.left + controlWidth;
	    rect.top = rect.bottom + 7;
	    rect.bottom = rect.top + m_iBoxHeight;
       rect2 = rect;

	    m_pPriorityText->MoveWindow(rect);

	    CDC * pDC = m_pPriority->GetDC();
	    int i, maxwidth = 0;
	    CString cs;
	    // find the longest string in the priority box
	    for (i=0; i<m_pPriority->GetCount(); i++)
	    {
	        m_pPriority->GetLBText(i,cs);
#ifdef _WIN32
            CSize size = pDC->GetTextExtent(cs);
#else
            CSize size = pDC->GetTextExtent(LPCTSTR(cs), cs.GetLength());
#endif
	        maxwidth = max(size.cx,maxwidth);
	    }

	    for (i=0; i<m_pMessageFormat->GetCount(); i++)
	    {
            m_pMessageFormat->GetLBText(i,cs);
#ifdef _WIN32
            CSize size = pDC->GetTextExtent(cs);
#else
            CSize size = pDC->GetTextExtent(LPCTSTR(cs), cs.GetLength());
#endif
	        maxwidth = max(size.cx,maxwidth);
	    }

        ReleaseDC(pDC);

        // right allign it    
        rect2.top -= TEXT_PIXEL_OFFSET;
        rect2.bottom = rect2.top + (m_iBoxHeight+2)*8;
        rect2.left = rect2.right;
        rect2.right = rect2.left + maxwidth + GetSystemMetrics(SM_CXHSCROLL) + 2;
        int iOldHeight = rect2.Height();
        m_pPriority->MoveWindow(rect2);

        rect.top = rect.bottom + TEXT_PIXEL_OFFSET + 4;
        rect.bottom = rect.top + m_iBoxHeight;
        m_pMessageFormatText->MoveWindow(rect);

        rect2.top = rect.top - TEXT_PIXEL_OFFSET;
        rect2.bottom = rect2.top + (m_iBoxHeight+2)*8;
        m_pMessageFormat->MoveWindow(rect2);

    }
}

int CComposeBar::OnToolHitTest( CPoint point, TOOLINFO* pTI ) const
{
    if (pTI != NULL)
    {
	    for (int i = 0; i < MAX_TIPS; i++)
	    {
	        if (m_ToolTipInfo[i].m_rect.PtInRect(point))
	        {

		    pTI->hwnd = m_hWnd;
		    pTI->rect = m_ToolTipInfo[i].m_rect;
		    if (i == IDX_TOOL_COLLAPSE && m_bClosed == TRUE)
		    {
		        CString cs;
		        cs.LoadString(IDS_COMPOSECLOSE);
		        pTI->lpszText = strdup(cs);
		    }
		    else
		        pTI->lpszText = strdup(m_ToolTipInfo[i].m_csToolTip);
		    pTI->uId = i;
		    return 1;
	        }
	    }
    }
#ifdef XP_WIN32
    return CDialogBar::OnToolHitTest(point,pTI);
#else
    return FALSE;
#endif
}

void CComposeBar::UpdateRecipientInfo ( char *pTo, char *pCc, char *pBcc )
{
}

void CComposeBar::AttachFile(void)
{
    ASSERT(m_pAttachmentList);
    m_pAttachmentList->AttachFile();
}

void CComposeBar::AttachUrl(void)
{
   CComposeFrame *pCompose = (CComposeFrame *)GetParent();
   MSG_Pane *pComposePane = pCompose->GetMsgPane();
   ASSERT(m_pAttachmentList);
   char * pUrl =  (char*)MSG_GetAssociatedURL(pComposePane);
   m_pAttachmentList->AttachUrl(pUrl);
}
    
extern "C" char * wfe_ExpandForNameCompletion(char * pString)
{
    ABID entryID = -1;
    ABID field = -1;
	DIR_Server* pab  = NULL;

	DIR_GetComposeNameCompletionAddressBook (theApp.m_directories, &pab);
    if (pab != NULL && theApp.m_pABook)
    {
	AB_GetIDForNameCompletion(
	    theApp.m_pABook,
	    pab, 
	    &entryID,&field,(LPCTSTR)pString);
	if (entryID != -1)
	{
		    if (field == ABNickname) {
			    char szNickname[kMaxNameLength];
			    AB_GetNickname(
				    pab, 
				    theApp.m_pABook, entryID, szNickname);
			    if (strlen(szNickname))
				    return strdup(szNickname);
		    }
	    else {
			    char szFullname[kMaxFullNameLength];
			    AB_GetFullName(pab, theApp.m_pABook, 
				    entryID, szFullname);
			    if (strlen(szFullname))
				    return strdup(szFullname);
		    }
	    }
    }

	return NULL;
}

extern "C" char * wfe_ExpandName(char * pString)
{
	ABID entryID = -1;
	ABID field = -1;
	char * fullname = NULL;

	DIR_Server* pab  = NULL;

	DIR_GetComposeNameCompletionAddressBook (theApp.m_directories, &pab);
    if (pab != NULL && theApp.m_pABook)
    {
	    AB_GetIDForNameCompletion(
		    theApp.m_pABook,
		    pab,
		    &entryID, &field,pString);
	    if (entryID != -1)
	    {
		    AB_GetExpandedName(
			    pab, 
			    theApp.m_pABook, entryID, &fullname);
		    if (fullname)
			    return fullname;
    	}
    }
	return NULL;
}


void CComposeBar::OnDropFiles( HDROP hDropInfo )
{
   UINT wNumFilesDropped = ::DragQueryFile(hDropInfo,(UINT)-1,NULL,0);
   if (wNumFilesDropped > 0)
   {
      OnAttachTab();
      UpdateWindow();
      if (m_pAttachmentList && ::IsWindow(m_pAttachmentList->m_hWnd))
         m_pAttachmentList->OnDropFiles(hDropInfo);
   }
}

BOOL CComposeBar::ProcessAddressBookIndexFormat(COleDataObject *pDataObject,
												DROPEFFECT effect,CPoint &point)
{
	HGLOBAL hContent;
	BOOL retVal = FALSE;
	if(!pDataObject)
		return FALSE;

	if (pDataObject->IsDataAvailable( ::RegisterClipboardFormat(ADDRESSBOOK_INDEX_FORMAT) ))
	{
		hContent = pDataObject->GetGlobalData (::RegisterClipboardFormat(ADDRESSBOOK_INDEX_FORMAT));
		AddressBookDragData *pDragData = (AddressBookDragData *) GlobalLock(hContent);

		BOOL retVal;
		CWnd * pWnd = GetFocus();
   
		XP_List * pEntries;
		int32 iEntries;
		CComposeFrame *pCompose = (CComposeFrame *)GetParentFrame();
   		ApiApiPtr(api);
		LPUNKNOWN pUnk = api->CreateClassInstance(
 			APICLASS_ADDRESSCONTROL, NULL, (APISIGNATURE)pCompose);
 		LPADDRESSCONTROL pIAddressControl = NULL;
   		if (pUnk)
   		{
   			HRESULT hRes = pUnk->QueryInterface(IID_IAddressControl,(LPVOID*)&pIAddressControl);
   			ASSERT(hRes==NOERROR);
   		}
		if (pIAddressControl)
		{
			int itemNum = pIAddressControl->GetItemFromPoint(&point);
			char * szType = NULL;
			char * szName = NULL;
			CListBox * pListBox = pIAddressControl->GetListBox();
			if (itemNum <= pListBox->GetCount())
				pIAddressControl->GetEntry (itemNum, &szType, &szName); 
			char * pszType = strdup(szType);
			if (pListBox->GetCount() == 1 && (!szName || !strlen(szName)))
			   pListBox->ResetContent();
			else
			   if (!szName || !strlen(szName))
				  pIAddressControl->DeleteEntry(itemNum);
			// this is where we get all of the info
			for (int32 i = 0; i < pDragData->m_count; i++)
			{
				AB_NameCompletionCookie *pCookie = AB_GetNameCompletionCookieForIndex( 
											pDragData->m_pane, pDragData->m_indices[i]);
				char * pString = AB_GetHeaderString(pCookie);
				AB_EntryType type = AB_GetEntryTypeForNCCookie(pCookie);

				if (pString != NULL)
					pIAddressControl->AppendEntry(FALSE, pszType,pString,
												  (type == AB_Person) ?
												    IDB_PERSON :IDB_MAILINGLIST);
				int count = pListBox->GetCount();
				pIAddressControl->SetNameCompletionCookieInfo(pCookie, 1, 
										 NC_NameComplete, count - 1);



			}
			if (pUnk)
				pUnk->Release();
			if (pszType)
				free(pszType);
			retVal = TRUE;
		}
		GlobalUnlock(hContent);
		if (pWnd && ::IsWindow(pWnd->m_hWnd))
			pWnd->SetFocus();
	}
	return retVal;

}


BOOL CComposeBar::ProcessVCardData(COleDataObject * pDataObject, CPoint &point)
{
   UINT clipFormat;
   BOOL retVal;
   CWnd * pWnd = GetFocus();
   if(pDataObject->IsDataAvailable(
      clipFormat = ::RegisterClipboardFormat(vCardClipboardFormat))) 
   {
      HGLOBAL hAddresses = pDataObject->GetGlobalData(clipFormat);
      LPSTR pAddresses = (LPSTR)GlobalLock(hAddresses);
      ASSERT(pAddresses);
      XP_List * pEntries;
      int32 iEntries;
		CComposeFrame *pCompose = (CComposeFrame *)GetParentFrame();
   	ApiApiPtr(api);
	   LPUNKNOWN pUnk = api->CreateClassInstance(
 		   APICLASS_ADDRESSCONTROL, NULL, (APISIGNATURE)pCompose);
 		LPADDRESSCONTROL pIAddressControl = NULL;
   	if (pUnk)
   	{
   		HRESULT hRes = pUnk->QueryInterface(IID_IAddressControl,(LPVOID*)&pIAddressControl);
   		ASSERT(hRes==NOERROR);
   	}
      if (pIAddressControl)
      {
         int itemNum = pIAddressControl->GetItemFromPoint(&point);
         char * szType = NULL;
         char * szName = NULL;
         CListBox * pListBox = pIAddressControl->GetListBox();
         if (itemNum <= pListBox->GetCount())
            pIAddressControl->GetEntry (itemNum, &szType, &szName); 
         char * pszType = strdup(szType);
         if (!AB_ConvertVCardsToExpandedName(theApp.m_pABook,pAddresses,&pEntries,&iEntries))
         {
            XP_List * node = pEntries;
            if (pListBox->GetCount() == 1 && (!szName || !strlen(szName)))
               pListBox->ResetContent();
            else
               if (!szName || !strlen(szName))
                  pIAddressControl->DeleteEntry(itemNum);
            for (int32 i = 0; i < iEntries+1; i++)
            {
               char * pString = (char *)node->object;
               if (pString != NULL)
                  pIAddressControl->AppendEntry(FALSE, pszType,pString,IDB_PERSON);
               node = node->next;
               if (!node)
                  break;
            }
            XP_ListDestroy(pEntries);
         }
         if (pUnk)
            pUnk->Release();
         if (pszType)
            free(pszType);
         GlobalUnlock(hAddresses);
         retVal = TRUE;
      }
   }
   if (pWnd && ::IsWindow(pWnd->m_hWnd))
      pWnd->SetFocus();
   return retVal;
}

BOOL CComposeBar::AddURLToAddressPane(COleDataObject * pDataObject, CPoint &point, LPSTR szURL)
{
  if(!pDataObject->IsDataAvailable(RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT)))
    return FALSE;

  CWnd * pWnd = GetFocus();
  CComposeFrame *pCompose = (CComposeFrame *)GetParentFrame();
  ApiApiPtr(api);
  LPUNKNOWN pUnk = api->CreateClassInstance(
  APICLASS_ADDRESSCONTROL, NULL, (APISIGNATURE)pCompose);
  LPADDRESSCONTROL pIAddressControl = NULL;

  if(pUnk == NULL)
    return FALSE;

  HRESULT hRes = pUnk->QueryInterface(IID_IAddressControl,(LPVOID*)&pIAddressControl);
  ASSERT(hRes==NOERROR);

  if(pIAddressControl == NULL)
    return FALSE;

  CListBox * pListBox = pIAddressControl->GetListBox();
  char * szName = NULL;
  char * szType = NULL;
  BOOL bOutside = FALSE;
  int iPlace = pListBox->ItemFromPoint(point, bOutside);

  pIAddressControl->GetEntry(iPlace, &szType, &szName); 
  
  if(iPlace == pListBox->GetCount() - 1)
  {
    if(!szName || !strlen(szName))
      if(pListBox->GetCount() == 1)
        pListBox->ResetContent();
      else
        pIAddressControl->DeleteEntry(iPlace);
  }

  CString type;
  type.LoadString(IDS_ADDRESSNEWSGROUP);

  pIAddressControl->AppendEntry(FALSE, type, szURL, IDB_NEWSART);

  pUnk->Release();

  if(pWnd && ::IsWindow(pWnd->m_hWnd))
    pWnd->SetFocus();

  return TRUE;
}
//////////////////////////////////////////////////////////////////////////////
// CNSCollapser

#define IDT_CNSCOLLAPSER 16425

CNSCollapser::CNSCollapser()
{
   m_hVertSectionBitmap =  NULL;
   m_hHTabBitmap =         NULL;
   m_bHilite =             FALSE;
}

CNSCollapser::~CNSCollapser()
{
   if (m_hVertSectionBitmap)
      VERIFY(::DeleteObject( (HGDIOBJ) m_hVertSectionBitmap ));
   if (m_hHTabBitmap)
      VERIFY(::DeleteObject( (HGDIOBJ) m_hHTabBitmap ));
}

void CNSCollapser::Initialize(CWnd * pWnd, UINT nCmdId)
{
   m_pWnd = pWnd;
   m_cmdId = nCmdId;
}

void CNSCollapser::DrawCollapseWidget(CDC &dc, COLLAPSE_STATE state, BOOL bHilite, int iSubtract)
{
   // this is the flippy thing
   if ( !m_hVertSectionBitmap ) 
   {
   	m_hVertSectionBitmap = WFE_LoadSysColorBitmap( 
   	    AfxGetResourceHandle(), 
          MAKEINTRESOURCE(IDB_VERTICALTAB));
   }
   if ( !m_hHTabBitmap ) 
   {
   	m_hHTabBitmap = WFE_LoadSysColorBitmap( 
   	    AfxGetResourceHandle(), 
          MAKEINTRESOURCE(IDB_LARGE_HFTAB));
   }

   m_cState = state;
   m_iSubtract = iSubtract;
   CDC dcVSection;
   dcVSection.CreateCompatibleDC(&dc);
   HBITMAP hOldBitmap = (HBITMAP)dcVSection.SelectObject(
      state == collapse_closed ? m_hHTabBitmap : m_hVertSectionBitmap);

   CRect rect, WinRect;
   m_pWnd->GetClientRect(&WinRect);

   rect.left   = 0;
   rect.top    = 2;
   rect.right  = COMP_VTAB_WIDTH;
   rect.bottom = WinRect.Height() - iSubtract;

   if (state == collapse_closed)
   {
      BITMAP bm;
      ::GetObject(m_hHTabBitmap,sizeof(BITMAP),(LPVOID)&bm);
      rect.right = bm.bmWidth;
      rect.bottom = 2 + COMP_HTAB_HEIGHT;
   }
   
   m_rect = rect;

   if (state == collapse_closed)
   {
      int ySrc = bHilite ? COMP_HTAB_HEIGHT : 0;
      dc.BitBlt( 
      	rect.left, rect.top, rect.Width(), COMP_HTAB_HEIGHT,
      	&dcVSection, 0, ySrc, SRCCOPY );
   }
   else 
   {
      int xSrc = bHilite ? COMP_VTAB_WIDTH : 0;
      dc.BitBlt( 
   	   rect.left, rect.top, COMP_VTAB_WIDTH, COMP_VTAB_TOP_HEIGHT,
   	   &dcVSection, xSrc, 0, SRCCOPY );
      int iMidSectionHeight = rect.Height()-(COMP_VTAB_TOP_HEIGHT+COMP_VTAB_BOTTOM_HEIGHT);
      int iRemainingHeight = iMidSectionHeight%COMP_VTAB_SECTION_HEIGHT;
      rect.top += COMP_VTAB_TOP_HEIGHT;
      if (iMidSectionHeight > COMP_VTAB_SECTION_HEIGHT)
      {
         int iSections = iMidSectionHeight/COMP_VTAB_SECTION_HEIGHT;
         for (int i = 0; i < iSections; i++)
         {
            dc.BitBlt( 
               rect.left, rect.top, COMP_VTAB_WIDTH, COMP_VTAB_SECTION_HEIGHT,
    	         &dcVSection, xSrc, COMP_VTAB_TOP_HEIGHT+1, SRCCOPY );
            rect.top += COMP_VTAB_SECTION_HEIGHT;            
         }
      }
      if (iRemainingHeight)
      {
         dc.BitBlt( 
            rect.left, rect.top, COMP_VTAB_WIDTH, iRemainingHeight,
    	      &dcVSection, xSrc, COMP_VTAB_TOP_HEIGHT + 1, SRCCOPY );
         rect.top += iRemainingHeight;
      }

      dc.BitBlt( 
         rect.left, rect.top, COMP_VTAB_WIDTH, COMP_VTAB_BOTTOM_HEIGHT,
    	   &dcVSection, xSrc, COMP_VTAB_TOP_HEIGHT + COMP_VTAB_SECTION_HEIGHT + 2, SRCCOPY );
   }

   dcVSection.SelectObject(hOldBitmap);
   VERIFY(dcVSection.DeleteDC());
}

void CNSCollapser::TimerEvent(UINT nIDEvent)
{
	CRect rect;
	m_pWnd->GetWindowRect(&rect);
	POINT pt;
	GetCursorPos(&pt);

	if(nIDEvent == IDT_CNSCOLLAPSER)
	{
		if(!rect.PtInRect(pt))
		{
         m_bHilite = FALSE;
			m_pWnd->KillTimer(m_nTimer);
         CDC * pDC = m_pWnd->GetDC();
         DrawCollapseWidget(*pDC, m_cState, FALSE, m_iSubtract);
         m_pWnd->ReleaseDC(pDC);
		}
	}
}

BOOL CNSCollapser::ButtonPress(POINT & point)
{
   if (m_rect.PtInRect(point))
   {
      m_pWnd->SendMessage(WM_COMMAND, m_cmdId);
      return TRUE;
   }
   return FALSE;
}

void CNSCollapser::MouseAround(POINT & point)
{
   if (m_rect.PtInRect(point))
   {
      if (!m_bHilite)
      {
         m_bHilite = TRUE;
			m_nTimer = m_pWnd->SetTimer(IDT_CNSCOLLAPSER, 100, NULL);
         CDC * pDC = m_pWnd->GetDC();
         DrawCollapseWidget(*pDC, m_cState, m_bHilite, m_iSubtract);
         m_pWnd->ReleaseDC(pDC);
      }
   }
   else
   {
      if (m_bHilite)
      {
         m_bHilite = FALSE;
         CDC * pDC = m_pWnd->GetDC();
         DrawCollapseWidget(*pDC, m_cState, m_bHilite, m_iSubtract);
         m_pWnd->ReleaseDC(pDC);
      }
   }
}



void CComposeBar::OnUpdateOptions(void)
{
   UpdateOptionsInfo();
}

void CComposeBar::UpdateSecurityOptions(void)
{
    HG26723
}
