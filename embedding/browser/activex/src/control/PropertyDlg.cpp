/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
