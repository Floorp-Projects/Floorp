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

#ifndef PROPERTYDLG_H
#define PROPERTYDLG_H

class CPPageDlg : public CDialogImpl<CPPageDlg>
{
public:
    enum { IDD = IDD_PPAGE_LINK };

    nsCString mProtocol;
    nsCString mType;
    nsCString mURL;

    BEGIN_MSG_MAP(CPPageLinkDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam,  LPARAM lParam, BOOL& bHandled);
};


class CPropertyDlg : public CDialogImpl<CPropertyDlg>
{
public:
    enum { IDD = IDD_PROPERTIES };

    CPPageDlg *mPPage;

    BEGIN_MSG_MAP(CPropertyDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDOK, OnOK)
        COMMAND_ID_HANDLER(IDCLOSE, OnClose)
    END_MSG_MAP()

    CPropertyDlg();

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam,  LPARAM lParam, BOOL& bHandled);
    LRESULT OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnClose(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

    HRESULT AddPage(CPPageDlg *pPage);
};


#endif