/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "stdafx.h"

#include "BrowserToolTip.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBrowserToolTip

CBrowserToolTip::CBrowserToolTip()
{
}

CBrowserToolTip::~CBrowserToolTip()
{
}

BEGIN_MESSAGE_MAP(CBrowserToolTip, CWnd)
	//{{AFX_MSG_MAP(CBrowserToolTip)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CBrowserToolTip::Create(CWnd *pParentWnd)
{
    return CWnd::CreateEx(WS_EX_TOOLWINDOW,
        AfxRegisterWndClass(CS_SAVEBITS, NULL, GetSysColorBrush(COLOR_INFOBK), NULL),
        _T("ToolTip"), WS_POPUP | WS_BORDER, 0, 0, 1, 1, pParentWnd->GetSafeHwnd(), NULL);
}

/////////////////////////////////////////////////////////////////////////////
// CBrowserToolTip message handlers

void CBrowserToolTip::OnPaint() 
{
	CPaintDC dc(this); // device context for painting

    CRect rcClient;
    GetClientRect(&rcClient);

    // Draw tip text
    int oldBkMode = dc.SetBkMode(TRANSPARENT);
    COLORREF oldTextColor = dc.SetTextColor(GetSysColor(COLOR_INFOTEXT));
    HGDIOBJ oldFont = dc.SelectObject(GetStockObject(DEFAULT_GUI_FONT));

    dc.DrawText(m_szTipText, -1, rcClient, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

    dc.SetBkMode(oldBkMode);
    dc.SetTextColor(oldTextColor);
    dc.SelectObject(oldFont);
}

BOOL CBrowserToolTip::PreCreateWindow(CREATESTRUCT& cs) 
{
	return CWnd::PreCreateWindow(cs);
}

void CBrowserToolTip::SetTipText(const CString &szTipText)
{
    m_szTipText = szTipText;
}

void CBrowserToolTip::Show(CWnd *pOverWnd, long left, long top)
{
    // Calculate the client window size
    CRect rcNewClient(0,0,0,0);
    CDC *pdc = GetDC();
    HGDIOBJ oldFont = pdc->SelectObject(GetStockObject(DEFAULT_GUI_FONT));
    rcNewClient.bottom = pdc->DrawText(m_szTipText, -1, rcNewClient,
        DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_CALCRECT);
    pdc->SelectObject(oldFont);
    ReleaseDC(pdc);
    rcNewClient.right += 8;
    rcNewClient.bottom += 8;
    
    // Adjust the tooltip to new size
    AdjustWindowRectEx(rcNewClient, GetWindowLong(m_hWnd, GWL_STYLE), FALSE, GetWindowLong(m_hWnd, GWL_EXSTYLE));

    // Adjust the left, top position of the tooltip
    CPoint ptTip(left, top);
    pOverWnd->ClientToScreen(&ptTip);

    // Make sure tip is below cursor
    POINT ptCursor;
    GetCursorPos(&ptCursor);
    long cyCursor = GetSystemMetrics(SM_CYCURSOR);
    if (ptTip.y < ptCursor.y + cyCursor)
        ptTip.y = ptCursor.y + cyCursor;

    // Make sure tip is fully visible
    RECT rcScreen;
    GetDesktopWindow()->GetClientRect(&rcScreen);
    if (ptTip.x < 0)
        ptTip.x = 0;
    else if (ptTip.x + rcNewClient.Width() > rcScreen.right)
        ptTip.x = rcScreen.right - rcNewClient.Width();
    if (ptTip.y < 0)
        ptTip.y = 0;
    else if (ptTip.y + rcNewClient.Height() > rcScreen.bottom)
        ptTip.y = rcScreen.bottom - rcNewClient.Height();

    // Position and show the tip
    SetWindowPos(&CWnd::wndTop, ptTip.x, ptTip.y, rcNewClient.Width(), rcNewClient.Height(),
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void CBrowserToolTip::Hide()
{
    ShowWindow(SW_HIDE);
}
