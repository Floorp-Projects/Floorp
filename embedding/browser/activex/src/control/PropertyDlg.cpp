/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s): 
 */

#include "stdafx.h"

#include "PropertyDlg.h"
#include "resource.h"

CPropertyDlg::CPropertyDlg() :
    mPPage(NULL)
{
}

HRESULT CPropertyDlg::AddPage(CPPageDlg *pPage)
{
    mPPage = pPage;
    return S_OK;
}


LRESULT CPropertyDlg::OnInitDialog(UINT uMsg, WPARAM wParam,  LPARAM lParam, BOOL& bHandled)
{
    if (mPPage)
    {
        // Create property page over the marker
        RECT rc;
        ::GetWindowRect(GetDlgItem(IDC_PPAGE_MARKER), &rc);
        ScreenToClient(&rc);
        mPPage->Create(m_hWnd, rc);
        mPPage->SetWindowPos(HWND_TOP, &rc, SWP_SHOWWINDOW);
    }
    return 1;
}


LRESULT CPropertyDlg::OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (mPPage)
    {
        mPPage->DestroyWindow();
    }
    EndDialog(IDOK);
    return 1;
}


LRESULT CPropertyDlg::OnClose(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
    if (mPPage)
    {
        mPPage->DestroyWindow();
    }
    EndDialog(IDCLOSE);
    return 1;
}


///////////////////////////////////////////////////////////////////////////////


LRESULT CPPageDlg::OnInitDialog(UINT uMsg, WPARAM wParam,  LPARAM lParam, BOOL& bHandled)
{
    SetDlgItemText(IDC_PROTOCOL, mProtocol);
    SetDlgItemText(IDC_TYPE, mType);
    SetDlgItemText(IDC_ADDRESS, mURL);
    return 1;
}
