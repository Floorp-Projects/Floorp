/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *   Adam Lock <adamlock@eircom.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef PAGESETUPDLG_H
#define PAGESETUPDLG_H

#include "resource.h"

#include "nsIPrintSettings.h"

/* VC98 doesn't define _tstof() */
#if defined(_MSC_VER) && _MSC_VER < 1300
#ifndef UNICODE
#define _tstof atof
#else
inline double _wtof(const wchar_t *string)
{
   USES_CONVERSION;
   return atof(W2CA(string));
}
#define _tstof _wtof
#endif
#endif

class CCustomFieldDlg : public CDialogImpl<CCustomFieldDlg>
{
public:
    enum { IDD = IDD_CUSTOM_FIELD };

    BEGIN_MSG_MAP(CCustomFieldDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDOK, OnOK)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    END_MSG_MAP()

    nsString m_field;

    CCustomFieldDlg(const nsAString &str) :
        m_field(str)
    {
    }

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam,  LPARAM lParam, BOOL& bHandled)
    {
        USES_CONVERSION;
        SetDlgItemText(IDC_VALUE, W2CT(m_field.get()));
        return 0;
    }

    LRESULT OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
    {
        TCHAR szBuf[128];
        GetDlgItemText(IDC_VALUE, szBuf, sizeof(szBuf) / sizeof(szBuf[0]));
        USES_CONVERSION;
        m_field = T2CW(szBuf);
        EndDialog(IDOK);
        return 0;
    }

    LRESULT OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
    {
        EndDialog(IDCANCEL);
        return 0;
    }
};

class CPageSetupFormatDlg : public CDialogImpl<CPageSetupFormatDlg>
{
public:
    enum { IDD = IDD_PPAGE_FORMAT };

    BEGIN_MSG_MAP(CPageSetupFormatDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_ID_HANDLER(IDC_SHRINKTOFIT, OnClickShrinkToFit)
    END_MSG_MAP()

    CPageSetupFormatDlg()
    {
    }

    void Init(nsIPrintSettings *aPrintSettings)
    {
        // Set landscape / portrait mode
        PRInt32 orientation = nsIPrintSettings::kPortraitOrientation;
        aPrintSettings->GetOrientation(&orientation);
        SendDlgItemMessage(
            (orientation == nsIPrintSettings::kPortraitOrientation)
                ? IDC_PORTRAIT : IDC_LANDSCAPE,
            BM_SETCHECK, 
            BST_CHECKED, 0);

        // Set scaling
        TCHAR szBuf[10];
        double scaling = 1.0;
        aPrintSettings->GetScaling(&scaling);
        _stprintf(szBuf, _T("%.1f"), scaling * 100.0);
        SetDlgItemText(IDC_SCALE, szBuf);

        // Set shrink to fit (& disable scale field)
        PRBool shrinkToFit = PR_FALSE;
        aPrintSettings->GetShrinkToFit(&shrinkToFit);
        CheckDlgButton(IDC_SHRINKTOFIT, shrinkToFit ? BST_CHECKED : BST_UNCHECKED);
        ::EnableWindow(GetDlgItem(IDC_SCALE), shrinkToFit ? FALSE : TRUE);

        // Print background - we use PrintBGColors to control both images & colours
        PRBool printBGColors = PR_TRUE;
        aPrintSettings->GetPrintBGColors(&printBGColors);
        CheckDlgButton(IDC_PRINTBACKGROUND, printBGColors ? BST_CHECKED : BST_UNCHECKED);
    }

    void Apply(nsIPrintSettings *aPrintSettings)
    {

        // Background options are tied to a single checkbox
        PRBool boolVal = 
            (SendDlgItemMessage(IDC_PRINTBACKGROUND, BM_GETCHECK) == BST_CHECKED) ?
                PR_TRUE : PR_FALSE;
        aPrintSettings->SetPrintBGColors(boolVal);
        aPrintSettings->SetPrintBGImages(boolVal);

        // Print scale
        TCHAR szBuf[128];
        GetDlgItemText(IDC_SCALE, szBuf, sizeof(szBuf) / sizeof(szBuf[0]));
        double scale = _tstof(szBuf) / 100.0;
        aPrintSettings->SetScaling(scale);

        // Shrink to fit
        PRBool shrinkToFit =
            (IsDlgButtonChecked(IDC_SHRINKTOFIT) == BST_CHECKED) ? PR_TRUE : PR_FALSE;
        aPrintSettings->SetShrinkToFit(shrinkToFit);

        // Landscape or portrait
        PRInt32 orientation = nsIPrintSettings::kLandscapeOrientation;
        if (SendDlgItemMessage(IDC_PORTRAIT, BM_GETCHECK) == BST_CHECKED)
        {
            orientation = nsIPrintSettings::kPortraitOrientation;
        }
        aPrintSettings->SetOrientation(orientation);
    }

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam,  LPARAM lParam, BOOL& bHandled)
    {
        return 0;
    }

    LRESULT OnClickShrinkToFit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
    {
        BOOL bEnableScale = TRUE;
        if (IsDlgButtonChecked(IDC_SHRINKTOFIT) == BST_CHECKED)
            bEnableScale = FALSE;
        ::EnableWindow(GetDlgItem(IDC_SCALE), bEnableScale);
        return 0;
    }
};

class CPageSetupMarginsDlg : public CDialogImpl<CPageSetupMarginsDlg>
{
public:
    nsString mHdrLeft;
    nsString mHdrCenter;
    nsString mHdrRight;
    nsString mFtrLeft;
    nsString mFtrCenter;
    nsString mFtrRight;

    enum { IDD = IDD_PPAGE_MARGINS };

    BEGIN_MSG_MAP(CPageSetupMarginsDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        COMMAND_RANGE_HANDLER(IDC_HDR_LEFT, IDC_FTR_RIGHT, OnHeaderFooterChange)
    END_MSG_MAP()

    CPageSetupMarginsDlg()
    {
    }

    const wchar_t * const * GetHeaderFooterValues(size_t &aNumValues) const
    {
        static const wchar_t *szValues[] = {
            L"",
            L"&T",
            L"&U",
            L"&D",
            L"&P",
            L"&PT"
        };
        aNumValues = sizeof(szValues) / sizeof(szValues[0]);
        return szValues;
    }

    const TCHAR * const * GetHeaderFooterOptions(size_t &aNumOptions) const
    {
        static const TCHAR *szOptions[] =
        {
            _T("-- Blank --"),
            _T("Title"),
            _T("URL"),
            _T("Date/Time"),
            _T("Page #"),
            _T("Page # of #"),
            _T("Custom...")
        };
        aNumOptions = sizeof(szOptions) / sizeof(szOptions[0]);
        return szOptions;
    }

    void Init(nsIPrintSettings *aPrintSettings)
    {
        double top = 0.0;
        double left = 0.0;
        double right = 0.0;
        double bottom = 0.0;
        aPrintSettings->GetMarginTop(&top);
        aPrintSettings->GetMarginLeft(&left);
        aPrintSettings->GetMarginRight(&right);
        aPrintSettings->GetMarginBottom(&bottom);

        // Get the margins
        TCHAR szBuf[16];
        _stprintf(szBuf, _T("%5.2f"), top);
        SetDlgItemText(IDC_MARGIN_TOP, szBuf);
        _stprintf(szBuf, _T("%5.2f"), left);
        SetDlgItemText(IDC_MARGIN_LEFT, szBuf);
        _stprintf(szBuf, _T("%5.2f"), right);
        SetDlgItemText(IDC_MARGIN_RIGHT, szBuf);
        _stprintf(szBuf, _T("%5.2f"), bottom);
        SetDlgItemText(IDC_MARGIN_BOTTOM, szBuf);

        // Get the header & footer settings
        PRUnichar* uStr = nsnull;
        aPrintSettings->GetHeaderStrLeft(&uStr);
        mHdrLeft = uStr;
        SetComboIndex(IDC_HDR_LEFT, uStr);
        if (uStr != nsnull) nsMemory::Free(uStr);

        aPrintSettings->GetHeaderStrCenter(&uStr);
        mHdrCenter = uStr;
        SetComboIndex(IDC_HDR_MIDDLE, uStr);
        if (uStr != nsnull) nsMemory::Free(uStr);

        aPrintSettings->GetHeaderStrRight(&uStr);
        mHdrRight = uStr;
        SetComboIndex(IDC_HDR_RIGHT, uStr);
        if (uStr != nsnull) nsMemory::Free(uStr);

        aPrintSettings->GetFooterStrLeft(&uStr);
        mFtrLeft = uStr;
        SetComboIndex(IDC_FTR_LEFT, uStr);
        if (uStr != nsnull) nsMemory::Free(uStr);

        aPrintSettings->GetFooterStrCenter(&uStr);
        mFtrCenter = uStr;
        SetComboIndex(IDC_FTR_MIDDLE, uStr);
        if (uStr != nsnull) nsMemory::Free(uStr);

        aPrintSettings->GetFooterStrRight(&uStr);
        mFtrRight = uStr;
        SetComboIndex(IDC_FTR_RIGHT, uStr);
        if (uStr != nsnull) nsMemory::Free(uStr);
    }

    void SetComboIndex(int aID, const wchar_t *szValue)
    {
        size_t nValues;
        const wchar_t * const * szValues = GetHeaderFooterValues(nValues);

        int nCurSel = 0;
        if (szValue[0] != L'\0')
        {
            while (nCurSel < nValues)
            {
                if (wcscmp(szValue, szValues[nCurSel]) == 0)
                {
                    break;
                }
                ++nCurSel;
            }
            // nCurSel might contain nValues but that just means the
            // Custom... field gets selected.
        }
        SendDlgItemMessage(aID, CB_SETCURSEL, nCurSel);
    }

    void Apply(nsIPrintSettings *aPrintSettings)
    {
        TCHAR szBuf[128];
        const size_t kBufSize = sizeof(szBuf) / sizeof(szBuf[0]);

        GetDlgItemText(IDC_MARGIN_TOP, szBuf, kBufSize);
        aPrintSettings->SetMarginTop(_tstof(szBuf));
        GetDlgItemText(IDC_MARGIN_LEFT, szBuf, kBufSize);
        aPrintSettings->SetMarginLeft(_tstof(szBuf));
        GetDlgItemText(IDC_MARGIN_BOTTOM, szBuf, kBufSize);
        aPrintSettings->SetMarginBottom(_tstof(szBuf));
        GetDlgItemText(IDC_MARGIN_RIGHT, szBuf, kBufSize);
        aPrintSettings->SetMarginRight(_tstof(szBuf));

        aPrintSettings->SetHeaderStrLeft(mHdrLeft.get());
        aPrintSettings->SetHeaderStrCenter(mHdrCenter.get());
        aPrintSettings->SetHeaderStrRight(mHdrRight.get());
        aPrintSettings->SetFooterStrLeft(mFtrLeft.get());
        aPrintSettings->SetFooterStrCenter(mFtrCenter.get());
        aPrintSettings->SetFooterStrRight(mFtrRight.get());
    }

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam,  LPARAM lParam, BOOL& bHandled)
    {
        size_t nOptions;
        const TCHAR * const *szOptions = GetHeaderFooterOptions(nOptions);

        // Fill out the drop down choices
        for (size_t i = 0; i < nOptions; ++i)
        {
            const TCHAR *szOption = szOptions[i];
            SendDlgItemMessage(IDC_HDR_LEFT, CB_ADDSTRING, 0, LPARAM(szOption));
            SendDlgItemMessage(IDC_HDR_MIDDLE, CB_ADDSTRING, 0, LPARAM(szOption));
            SendDlgItemMessage(IDC_HDR_RIGHT, CB_ADDSTRING, 0, LPARAM(szOption));
            SendDlgItemMessage(IDC_FTR_LEFT, CB_ADDSTRING, 0, LPARAM(szOption));
            SendDlgItemMessage(IDC_FTR_MIDDLE, CB_ADDSTRING, 0, LPARAM(szOption));
            SendDlgItemMessage(IDC_FTR_RIGHT, CB_ADDSTRING, 0, LPARAM(szOption));
        }
        return 0;
    }

    LRESULT OnHeaderFooterChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
    {
        if (wNotifyCode != CBN_SELCHANGE)
        {
            bHandled = FALSE;
            return 0;
        }

        // One of the header / footer combos has changed, so set the string
        // to the updated value.

        nsString *pStr = nsnull;
        switch (wID)
        {
        case IDC_HDR_LEFT:   pStr = &mHdrLeft;   break;
        case IDC_HDR_MIDDLE: pStr = &mHdrCenter; break;
        case IDC_HDR_RIGHT:  pStr = &mHdrRight;  break;
        case IDC_FTR_LEFT:   pStr = &mFtrLeft;   break;
        case IDC_FTR_MIDDLE: pStr = &mFtrCenter; break;
        case IDC_FTR_RIGHT:  pStr = &mFtrRight;  break;
        }
        if (!pStr)
        {
            return 0;
        }

        size_t nValues;
        const wchar_t * const * szValues = GetHeaderFooterValues(nValues);

        int nCurSel = SendDlgItemMessage(wID, CB_GETCURSEL);
        if (nCurSel == nValues) // Custom...
        {
            CCustomFieldDlg dlg(*pStr);
            if (dlg.DoModal() == IDOK)
            {
                *pStr = dlg.m_field;
            }
            // Update combo in case their custom value is not custom at all
            // For example, if someone opens the custom dlg and types "&P"
            // (i.e. "Page #"), we should select that since it is already a
            // choice in the combo.
            SetComboIndex(wID, pStr->get());
        }
        else
        {
            *pStr = szValues[nCurSel];
        }

        return 0;
    }

};
class CPageSetupDlg : public CDialogImpl<CPageSetupDlg>
{
public:
    enum { IDD = IDD_PAGESETUP };

    CPPageDlg *mPPage;

    BEGIN_MSG_MAP(CPageSetupDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        NOTIFY_HANDLER(IDC_TAB, TCN_SELCHANGE, OnTabSelChange)
        COMMAND_ID_HANDLER(IDOK, OnOK)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    END_MSG_MAP()

    nsCOMPtr<nsIPrintSettings> mPrintSettings;
    CPageSetupFormatDlg mFormatDlg;
    CPageSetupMarginsDlg mMarginsDlg;

    CPageSetupDlg(nsIPrintSettings *aPrintSettings) :
        mPrintSettings(aPrintSettings)
    {
    }

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam,  LPARAM lParam, BOOL& bHandled)
    {
        // The marker tells us where to stick the pages
        RECT rcMarker;
        ::GetWindowRect(GetDlgItem(IDC_PAGE_MARKER), &rcMarker);
        ScreenToClient(&rcMarker);

         // Create the two pages, the first is shown, the second is not
        mFormatDlg.Create(m_hWnd);
        mFormatDlg.Init(mPrintSettings);
        mFormatDlg.SetWindowPos(HWND_TOP, &rcMarker, SWP_SHOWWINDOW);
        
        mMarginsDlg.Create(m_hWnd);
        mMarginsDlg.Init(mPrintSettings);
        mMarginsDlg.SetWindowPos(HWND_TOP, &rcMarker, SWP_HIDEWINDOW);

        // Get the tab control
        HWND hwndTab = GetDlgItem(IDC_TAB);
        
        TCITEM tcItem;
        
        memset(&tcItem, 0, sizeof(tcItem));
        tcItem.mask = TCIF_TEXT;

        tcItem.pszText = _T("Format && Options");
        TabCtrl_InsertItem(hwndTab, 0, &tcItem);

        tcItem.pszText = _T("Margins && Header / Footer");
        TabCtrl_InsertItem(hwndTab, 1, &tcItem);

        TabCtrl_SetCurSel(hwndTab, 0);

        return 0;
    }

    LRESULT OnTabSelChange(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
    {
        HWND hwndTab = GetDlgItem(IDC_TAB);
        if (TabCtrl_GetCurSel(hwndTab) == 0)
        {
            mFormatDlg.SetWindowPos(HWND_TOP, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            mMarginsDlg.ShowWindow(SW_HIDE); 
        }
        else
        {
            mMarginsDlg.SetWindowPos(HWND_TOP, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            mFormatDlg.ShowWindow(SW_HIDE); 
        }
        return 0;
    }

    LRESULT OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
    {
        mFormatDlg.Apply(mPrintSettings);
        mMarginsDlg.Apply(mPrintSettings);
        EndDialog(IDOK);
        return 0;
    }

    LRESULT OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
    {
        EndDialog(IDCANCEL);
        return 0;
    }
};

#endif
