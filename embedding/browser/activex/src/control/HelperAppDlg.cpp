/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@eircom.net>
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

#include <commctrl.h>

// commdlg.h is needed to build with WIN32_LEAN_AND_MEAN
#include <commdlg.h>

#include "HelperAppDlg.h"

#include "nsCOMPtr.h"
#include "nsIExternalHelperAppService.h"
#include "nsILocalFile.h"
#include "nsIMIMEInfo.h"

#include "nsIHelperAppLauncherDialog.h"


///////////////////////////////////////////////////////////////////////////////

class AppLauncherDlg
{
public:
    AppLauncherDlg();
    virtual ~AppLauncherDlg();

    int Show(nsIHelperAppLauncher* aLauncher, HWND hwndParent);

    PRPackedBool mSaveToDisk;
    nsCAutoString mOpenWith;

private:
    static BOOL CALLBACK LaunchProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnInitDialog();
    void OnOK();
    void OnCancel();
    void OnChooseApp();

    HWND mHwndDlg;
    nsCOMPtr<nsIHelperAppLauncher> mHelperAppLauncher;
};

AppLauncherDlg::AppLauncherDlg() : mHwndDlg(NULL), mSaveToDisk(TRUE)
{
}

AppLauncherDlg::~AppLauncherDlg()
{
}

int
AppLauncherDlg::Show(nsIHelperAppLauncher* aLauncher, HWND hwndParent)
{
    HINSTANCE hInstResource = _Module.m_hInstResource;

    mHelperAppLauncher = aLauncher;

    INT result = DialogBoxParam(hInstResource,
        MAKEINTRESOURCE(IDD_HELPERAPP), hwndParent, LaunchProc, (LPARAM) this);
    
    return result;
}

void AppLauncherDlg::OnInitDialog()
{
    USES_CONVERSION;
    nsCOMPtr<nsIMIMEInfo> mimeInfo;
    nsCAutoString url;
    if (mHelperAppLauncher)
    {
        mHelperAppLauncher->GetMIMEInfo(getter_AddRefs(mimeInfo));
        nsCOMPtr<nsIURI> uri;
        mHelperAppLauncher->GetSource(getter_AddRefs(uri));
        uri->GetSpec(url);
    }
    nsMIMEInfoHandleAction prefAction = nsIMIMEInfo::saveToDisk;
    nsAutoString appName;
    nsCAutoString contentType;
    if (mimeInfo)
    {
        mimeInfo->GetPreferredAction(&prefAction);
        mimeInfo->GetApplicationDescription(appName);
        mimeInfo->GetMIMEType(contentType);
    }
    if (prefAction == nsIMIMEInfo::saveToDisk)
    {
        CheckRadioButton(mHwndDlg, IDC_OPENWITHAPP, IDC_SAVETOFILE, IDC_SAVETOFILE);
    }
    else
    {
        CheckRadioButton(mHwndDlg, IDC_OPENWITHAPP, IDC_SAVETOFILE, IDC_OPENWITHAPP);
    }
    SetDlgItemText(mHwndDlg, IDC_URL,
        url.IsEmpty() ? _T("") : A2CT(url.get()));
    SetDlgItemText(mHwndDlg, IDC_APPLICATION,
        appName.IsEmpty() ? _T("<No Application>") : W2CT(appName.get()));
    SetDlgItemText(mHwndDlg, IDC_CONTENTTYPE,
        contentType.IsEmpty() ? _T("") : A2CT(contentType.get()));
}

void AppLauncherDlg::OnOK()
{
    mSaveToDisk = IsDlgButtonChecked(mHwndDlg, IDC_SAVETOFILE) ? TRUE : FALSE;
    EndDialog(mHwndDlg, IDOK);
}

void AppLauncherDlg::OnCancel()
{
    EndDialog(mHwndDlg, IDCANCEL);
}

void AppLauncherDlg::OnChooseApp()
{
    USES_CONVERSION;

    TCHAR szPath[MAX_PATH + 1];
    memset(szPath, 0, sizeof(szPath));

    TCHAR *lpszFilter =
        _T("EXE Files Only (*.exe)\0*.exe\0"
           "All Files (*.*)\0*.*\0\0");

    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(ofn));
#if _WIN32_WINNT >= 0x0500
    ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
    ofn.lStructSize = sizeof(ofn);
#endif
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrFilter = lpszFilter;
    ofn.lpstrDefExt = _T("exe");
    ofn.lpstrFile = szPath;
    ofn.nMaxFile = MAX_PATH;

    if (GetOpenFileName(&ofn))
    {
        USES_CONVERSION;
        mOpenWith = T2A(szPath);
        SetDlgItemText(mHwndDlg, IDC_APPLICATION, szPath);
        CheckRadioButton(mHwndDlg, IDC_OPENWITHAPP, IDC_SAVETOFILE, IDC_OPENWITHAPP);
    }
}

BOOL CALLBACK
AppLauncherDlg::LaunchProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    AppLauncherDlg *pThis = NULL;
    if (uMsg == WM_INITDIALOG)
    {
        pThis = (AppLauncherDlg *) lParam;
        NS_WARN_IF_FALSE(pThis, "need a pointer to this!");
        pThis->mHwndDlg = hwndDlg;
        SetWindowLong(hwndDlg, DWL_USER, lParam);
    }
    else
    {
        pThis = (AppLauncherDlg *) GetWindowLong(hwndDlg, DWL_USER);
    }
    switch (uMsg)
    {
    case WM_INITDIALOG:
        pThis->OnInitDialog();
        return TRUE;
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            NS_WARN_IF_FALSE(pThis, "Should be non-null!");
            switch (LOWORD(wParam))
            {
            case IDC_CHOOSE:
                pThis->OnChooseApp();
                break;
            case IDOK:
                pThis->OnOK();
                break;
            case IDCANCEL:
                pThis->OnCancel();
                break;
            }
        }
        return TRUE;
    }
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////

//*****************************************************************************
// ProgressDlg
//***************************************************************************** 


class ProgressDlg :
     public nsIWebProgressListener,
     public nsSupportsWeakReference
{
public:
    ProgressDlg();

    void Show(nsIHelperAppLauncher *aHelperAppLauncher, HWND hwndParent);

protected:
    virtual ~ProgressDlg();

    void OnInitDialog();
    void OnCancel();

    static BOOL CALLBACK ProgressProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    HWND mHwndDlg;
    nsCOMPtr<nsIHelperAppLauncher> mHelperAppLauncher;

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
};


ProgressDlg::ProgressDlg()
{
}

ProgressDlg::~ProgressDlg() 
{
}

void
ProgressDlg::Show(nsIHelperAppLauncher *aHelperAppLauncher, HWND hwndParent)
{
    HINSTANCE hInstResource = _Module.m_hInstResource;

    mHelperAppLauncher = aHelperAppLauncher;
    mHwndDlg = CreateDialogParam(hInstResource, MAKEINTRESOURCE(IDD_PROGRESS),
        hwndParent, ProgressProc, (LPARAM) this);
}


NS_IMPL_ISUPPORTS2(ProgressDlg, nsIWebProgressListener, nsISupportsWeakReference)

NS_IMETHODIMP
ProgressDlg::OnStateChange(nsIWebProgress *aWebProgress, 
                           nsIRequest *aRequest, PRUint32 aStateFlags, 
                           nsresult aStatus)
{
    if ((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_DOCUMENT))
    {
        // We've completed the download - close the progress window
        if(mHelperAppLauncher)
            mHelperAppLauncher->CloseProgressWindow();

        DestroyWindow(mHwndDlg);
    }

    return NS_OK;
}

NS_IMETHODIMP ProgressDlg::OnProgressChange(nsIWebProgress *aWebProgress, 
                                             nsIRequest *aRequest, 
                                             PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, 
                                             PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    // Update the progress control
    if (IsWindow(mHwndDlg))
    {
        HWND hwndProgress = GetDlgItem(mHwndDlg, IDC_PROGRESS);
        SendMessage(hwndProgress, PBM_SETRANGE32, 0, aMaxTotalProgress);
        SendMessage(hwndProgress, PBM_SETPOS, aCurTotalProgress, 0);
    }

    return NS_OK;
}

NS_IMETHODIMP ProgressDlg::OnLocationChange(nsIWebProgress *aWebProgress, 
                                             nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

NS_IMETHODIMP ProgressDlg::OnStatusChange(nsIWebProgress *aWebProgress, 
                                           nsIRequest *aRequest, nsresult aStatus, 
                                           const PRUnichar *aMessage)
{
    return NS_OK;
}

NS_IMETHODIMP ProgressDlg::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                             nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK
ProgressDlg::ProgressProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ProgressDlg *pThis = NULL;
    if (uMsg == WM_INITDIALOG)
    {
        pThis = (ProgressDlg *) lParam;
        SetWindowLong(hwndDlg, DWL_USER, lParam);
    }
    else
    {
        pThis = (ProgressDlg *) GetWindowLong(hwndDlg, DWL_USER);
    }
    switch (uMsg)
    {
    case WM_INITDIALOG:
        NS_WARN_IF_FALSE(pThis, "Should be non-null!");
        pThis->OnInitDialog();
        return TRUE;
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            NS_WARN_IF_FALSE(pThis, "Should be non-null!");
            switch (LOWORD(wParam))
            {
            case IDCANCEL:
                pThis->OnCancel();
                break;
            }
        }
        return TRUE;
    }

    return FALSE;
}

void ProgressDlg::OnInitDialog() 
{
    // Set the "SavingFrom" field
    if (mHelperAppLauncher)
    {
        nsCOMPtr<nsIURI> srcUri;
        nsresult rv = mHelperAppLauncher->GetSource(getter_AddRefs(srcUri));
        if(NS_SUCCEEDED(rv))
        {
            USES_CONVERSION;
            nsCAutoString uriString;
            srcUri->GetSpec(uriString);
            SetDlgItemText(mHwndDlg, IDC_SOURCE, A2CT(uriString.get()));
        }
    }

    // Set the "Action" field
//    if(m_HandleContentOp == CONTENT_SAVE_TO_DISK)
//        m_Action.SetWindowText("[Saving file to:] " + m_FileName);
//    else if(m_HandleContentOp == CONTENT_LAUNCH_WITH_APP)
//        m_Action.SetWindowText("[Opening file with:] " + m_FileName);
}

void ProgressDlg::OnCancel() 
{
    if (mHelperAppLauncher)
        mHelperAppLauncher->Cancel();
	DestroyWindow(mHwndDlg);
}



///////////////////////////////////////////////////////////////////////////////

class CHelperAppLauncherDlgFactory : public nsIFactory
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFACTORY

    CHelperAppLauncherDlgFactory();
    virtual ~CHelperAppLauncherDlgFactory();
};

class CHelperAppLauncherDlg :
    public nsIHelperAppLauncherDialog
{
public:
    CHelperAppLauncherDlg();

protected:
    virtual ~CHelperAppLauncherDlg();

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHELPERAPPLAUNCHERDIALOG

    friend class CHelperAppLauncherDlgFactory;
};

NS_IMPL_ISUPPORTS1(CHelperAppLauncherDlg, nsIHelperAppLauncherDialog)

CHelperAppLauncherDlg::CHelperAppLauncherDlg()
{
}

CHelperAppLauncherDlg::~CHelperAppLauncherDlg() 
{
}

/* void show (in nsIHelperAppLauncher aLauncher, in nsISupports aContext, in boolean aForced); */
NS_IMETHODIMP
CHelperAppLauncherDlg::Show(nsIHelperAppLauncher *aLauncher, nsISupports *aContext, PRBool aForced)
{
    NS_ENSURE_ARG_POINTER(aLauncher);
    
    AppLauncherDlg dlg;
    if (dlg.Show(aLauncher, NULL) == IDCANCEL)
    {
        aLauncher->Cancel();
        return NS_OK;
    }

    if (dlg.mSaveToDisk)
    {
        return aLauncher->SaveToDisk(nsnull, PR_FALSE);
    }
    else
    {
        nsCOMPtr<nsILocalFile> openWith;
        nsresult rv = NS_NewNativeLocalFile(dlg.mOpenWith, PR_FALSE, getter_AddRefs(openWith));
        return aLauncher->LaunchWithApplication(openWith, PR_FALSE);
    }
}

/* nsILocalFile promptForSaveToFile (in nsIHelperAppLauncher aLauncher, in nsISupports aWindowContext, in wstring aDefaultFile, in wstring aSuggestedFileExtension); */
NS_IMETHODIMP
CHelperAppLauncherDlg::PromptForSaveToFile(nsIHelperAppLauncher *aLauncher, 
                                           nsISupports *aWindowContext, 
                                           const PRUnichar *aDefaultFile, 
                                           const PRUnichar *aSuggestedFileExtension, 
                                           nsILocalFile **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    USES_CONVERSION;

    TCHAR szPath[MAX_PATH + 1];
    memset(szPath, 0, sizeof(szPath));
    _tcsncpy(szPath, W2T(aDefaultFile), MAX_PATH);

    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(ofn));
#if _WIN32_WINNT >= 0x0500
    ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
    ofn.lStructSize = sizeof(ofn);
#endif
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrFilter = _T("All Files (*.*)\0*.*\0\0");
    ofn.lpstrDefExt = W2T(aSuggestedFileExtension);
    ofn.lpstrFile = szPath;
    ofn.nMaxFile = MAX_PATH;

    if (GetSaveFileName(&ofn))
    {
        return NS_NewNativeLocalFile(nsDependentCString(szPath), PR_FALSE, _retval);
    }
    
    return NS_ERROR_FAILURE;
}

//*****************************************************************************
// CHelperAppLauncherDlgFactory
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(CHelperAppLauncherDlgFactory, nsIFactory)

CHelperAppLauncherDlgFactory::CHelperAppLauncherDlgFactory()
{
}

CHelperAppLauncherDlgFactory::~CHelperAppLauncherDlgFactory()
{
}

NS_IMETHODIMP CHelperAppLauncherDlgFactory::CreateInstance(nsISupports *aOuter, const nsIID & aIID, void **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = NULL;  
    CHelperAppLauncherDlg *inst = new CHelperAppLauncherDlg;    
    if (!inst)
    return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = inst->QueryInterface(aIID, aResult);
    if (rv != NS_OK) {  
        // We didn't get the right interface, so clean up  
        delete inst;  
    }  
    return rv;
}

NS_IMETHODIMP CHelperAppLauncherDlgFactory::LockFactory(PRBool lock)
{
    return NS_OK;
}

nsresult NS_NewHelperAppLauncherDlgFactory(nsIFactory** aFactory)
{
    NS_ENSURE_ARG_POINTER(aFactory);
    *aFactory = nsnull;

    CHelperAppLauncherDlgFactory *result = new CHelperAppLauncherDlgFactory;
    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aFactory = result;

    return NS_OK;
}
