/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "stdafx.h"
#include "HelperAppService.h"
#include "HelperAppDlg.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsILocalFile.h"
#include "nsIURI.h"
#include "nsIMIMEInfo.h"
#include "nsIDOMWindow.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIServiceManager.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

// This file contains code which overrides the default
// Gecko implementation for helper app dialog handling
// 
// General Overview:
//
//    We register the factory object for overriding helper app dialogs
//    [See OverrideComponents() in mfcembed.cpp]
//
//
//    When Gecko encounters content it cannot handle it will:
//      1. Call CHelperAppLauncherDialog::Show() method
//
//      2. If the user chooses SaveToDisk in the dialog presented by
//         step 1, then CHelperAppLauncherDialog::PromptForSaveToFile()
//         will be called to get the filename to save the content to
//         
//         If the user chooses OpenUsingAnExternalApp option in the
//         dialog box(and subsequnetly specifies the app to use by 
//         clicking on the "Choose..." button), the specified app
//         is run after the content is downloaded
//
//      3. In either case, an instance of nsIProgressDialog will be created
//         which is used to display a download progress
//         dialog box. This dialog box registers itself as a
//         WebProgressListener so that it can receive the progress change
//         messages, using which it can update the progress bar
//
//      4. The downloads can be cancelled by clicking on the "Cancel" button
//         at anytime during the download.

static HINSTANCE gInstance;

//*****************************************************************************
// ResourceState
//***************************************************************************** 

class ResourceState {
public:
    ResourceState() {
        mPreviousInstance = ::AfxGetResourceHandle();
        ::AfxSetResourceHandle(gInstance);
    }
    ~ResourceState() {
        ::AfxSetResourceHandle(mPreviousInstance);
    }
private:
    HINSTANCE mPreviousInstance;
};

//*****************************************************************************
// CHelperAppLauncherDialogFactory Creation and Init Functions
//*****************************************************************************   

void InitHelperAppDlg(HINSTANCE instance) 
{
  gInstance = instance;
}

nsresult NS_NewHelperAppDlgFactory(nsIFactory** aFactory)
{
  NS_ENSURE_ARG_POINTER(aFactory);
  *aFactory = nsnull;
  
  CHelperAppLauncherDialogFactory *result = new CHelperAppLauncherDialogFactory;
  if (!result)
    return NS_ERROR_OUT_OF_MEMORY;
    
  NS_ADDREF(result);
  *aFactory = result;
  
  return NS_OK;
}

//*****************************************************************************
// CHelperAppLauncherDialogFactory
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(CHelperAppLauncherDialogFactory, nsIFactory)

CHelperAppLauncherDialogFactory::CHelperAppLauncherDialogFactory() 
{
}

CHelperAppLauncherDialogFactory::~CHelperAppLauncherDialogFactory() {
}

NS_IMETHODIMP CHelperAppLauncherDialogFactory::CreateInstance(nsISupports *aOuter, const nsIID & aIID, void **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  
  *aResult = NULL;  
  CHelperAppLauncherDialog *inst = new CHelperAppLauncherDialog;
  if (!inst)
    return NS_ERROR_OUT_OF_MEMORY;
    
  nsresult rv = inst->QueryInterface(aIID, aResult);
  if (rv != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  
    
  return rv;
}

NS_IMETHODIMP CHelperAppLauncherDialogFactory::LockFactory(PRBool lock)
{
  return NS_OK;
}

//*******************************************************************************
// CHelperAppLauncherDialog - Implements the nsIHelperAppLauncherDialog interface
//*******************************************************************************

NS_IMPL_ISUPPORTS1(CHelperAppLauncherDialog, nsIHelperAppLauncherDialog)

CHelperAppLauncherDialog::CHelperAppLauncherDialog() :
      mWWatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID))
{
}

CHelperAppLauncherDialog::~CHelperAppLauncherDialog() 
{
}

// Returns a CWnd given aWindowContext - the returned CWnd is mainly used as 
// a parent window for the dialog boxes we display
//
CWnd* CHelperAppLauncherDialog::GetParentFromContext(nsISupports *aWindowContext)
{
    nsCOMPtr<nsIDOMWindow> domWnd(do_GetInterface(aWindowContext));
    if(!domWnd) 
        return NULL;

    CWnd *retWnd = NULL;

    nsCOMPtr<nsIWebBrowserChrome> chrome;
    if(mWWatch)
    {
        nsCOMPtr<nsIDOMWindow> fosterParent;
        if (!domWnd) 
        { // it will be a dependent window. try to find a foster parent.
            mWWatch->GetActiveWindow(getter_AddRefs(fosterParent));
            domWnd = fosterParent;
        }
        mWWatch->GetChromeForWindow(domWnd, getter_AddRefs(chrome));
    }

    if (chrome) 
    {
        nsCOMPtr<nsIEmbeddingSiteWindow> site(do_QueryInterface(chrome));
        if (site)
        {
            HWND w;
            site->GetSiteWindow(reinterpret_cast<void **>(&w));
            retWnd = CWnd::FromHandle(w);
        }
    }

    return retWnd;
}

// Displays the "Save To Disk" or "Open Using..." dialog box
// when Gecko encounters a mime type it cannot handle
//
NS_IMETHODIMP CHelperAppLauncherDialog::Show(nsIHelperAppLauncher *aLauncher, 
                                             nsISupports *aContext,
                                             PRBool aForced)
{
    ResourceState setState;

    NS_ENSURE_ARG_POINTER(aLauncher);

    CChooseActionDlg dlg(aLauncher, GetParentFromContext(aContext));
    if(dlg.DoModal() == IDCANCEL)
    {
        // User chose Cancel - just cancel the download

        aLauncher->Cancel();

        return NS_OK;
    }

    // User did not cancel out of the dialog box i.e. OK was chosen

    if(dlg.m_ActionChosen == CONTENT_SAVE_TO_DISK)
    {
        m_HandleContentOp = CONTENT_SAVE_TO_DISK;

        return aLauncher->SaveToDisk(nsnull, PR_FALSE);
    }
    else
    {
        m_HandleContentOp = CONTENT_LAUNCH_WITH_APP;

        m_FileName = dlg.m_OpenWithAppName;

        USES_CONVERSION;
        nsCAutoString fileName(T2CA(m_FileName));

        nsCOMPtr<nsILocalFile> openWith;
        nsresult rv = NS_NewNativeLocalFile(fileName, PR_FALSE, getter_AddRefs(openWith));
        if (NS_FAILED(rv))
            return aLauncher->LaunchWithApplication(nsnull, PR_FALSE);
        else
            return aLauncher->LaunchWithApplication(openWith, PR_FALSE);
    }
}

// User chose the "Save To Disk" option in the dialog before
// We prompt the user for the filename to which the content
// will be saved into
//
NS_IMETHODIMP CHelperAppLauncherDialog::PromptForSaveToFile(nsIHelperAppLauncher* aLauncher,
                                                             nsISupports *aWindowContext, 
                                                             const PRUnichar *aDefaultFile, 
                                                             const PRUnichar *aSuggestedFileExtension, 
                                                             nsILocalFile **_retval)
{
    USES_CONVERSION;

    NS_ENSURE_ARG_POINTER(_retval);

    TCHAR *lpszFilter = _T("All Files (*.*)|*.*||");
    CFileDialog cf(FALSE, W2CT(aSuggestedFileExtension), W2CT(aDefaultFile),
                    OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                    lpszFilter, GetParentFromContext(aWindowContext));
    if(cf.DoModal() == IDOK)
    {
        m_FileName = cf.GetPathName(); // Will be like: c:\tmp\junk.exe
        USES_CONVERSION;
        nsCAutoString fileName(T2CA(m_FileName));
        return NS_NewNativeLocalFile(fileName, PR_FALSE, _retval);
    }
    else
        return NS_ERROR_FAILURE;
}

//*****************************************************************************
// CChooseActionDlg
//***************************************************************************** 

CChooseActionDlg::CChooseActionDlg(nsIHelperAppLauncher *aLauncher, CWnd* pParent /*=NULL*/)
    : CDialog(CChooseActionDlg::IDD, pParent)
{
    m_HelperAppLauncher = aLauncher;
}

void CChooseActionDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_CONTENT_TYPE, m_ContentType);
}

BOOL CChooseActionDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

    // Get the mimeInfo from nsIHelperAppLauncher
    //
    nsCOMPtr<nsIMIMEInfo> mimeInfo;
    if(m_HelperAppLauncher)
        m_HelperAppLauncher->GetMIMEInfo(getter_AddRefs(mimeInfo));

    if(mimeInfo) 
    {
        // Retrieve and set the Mime type of the content we're downloading
        // in the content type field of the dialog box
        //
        nsCAutoString mimeType;
        nsresult rv = mimeInfo->GetMIMEType(mimeType);
        if(NS_SUCCEEDED(rv)) 
        {
            CStatic *pMimeType = (CStatic *)GetDlgItem(IDC_CONTENT_TYPE);
            if(pMimeType)
            {
                USES_CONVERSION;
                pMimeType->SetWindowText(A2CT(mimeType.get()));
            }
        }

        // See if we can get the preferred action from the mime info
        // and init the controls accordingly
        //
        InitWithPreferredAction(mimeInfo);
    }
    else
        SelectSaveToDiskOption();
    
    return FALSE;  // return TRUE unless you set the focus to a control
}

// See if we can determine the default handling action, if any,
// from aMimeInfo
// If not, simply select SaveToDisk as the default
//
void CChooseActionDlg::InitWithPreferredAction(nsIMIMEInfo* aMimeInfo)
{
    // Retrieve and set the specified default action
    //
    nsMIMEInfoHandleAction prefAction = nsIMIMEInfo::saveToDisk;
    aMimeInfo->GetPreferredAction(&prefAction);
    if(prefAction == nsIMIMEInfo::saveToDisk)
    {
        SelectSaveToDiskOption();
        return;
    }

    // OpenWithApp is the preferred action - select it
    //
    SelectOpenUsingAppOption();

    // See if we can get the appname
    //
    nsAutoString appDesc;
    nsresult rv = aMimeInfo->GetApplicationDescription(appDesc);
    if(NS_SUCCEEDED(rv)) 
    {
        USES_CONVERSION;
        m_OpenWithAppName = W2CT(appDesc.get());

        // Update with the app name
        //
        UpdateAppNameField(m_OpenWithAppName);
    }
}

void CChooseActionDlg::UpdateAppNameField(CString& appName)
{
    CStatic *pAppName = (CStatic *)GetDlgItem(IDC_APP_NAME);
    if(pAppName)
        pAppName->SetWindowText(appName);    
}

void CChooseActionDlg::SelectSaveToDiskOption()
{
    CButton *pRadioSaveToDisk = (CButton *)GetDlgItem(IDC_SAVE_TO_DISK);
    if(pRadioSaveToDisk)
    {
        pRadioSaveToDisk->SetCheck(1);

        pRadioSaveToDisk->SetFocus();

        OnSaveToDiskRadioBtnClicked();
    }
}

void CChooseActionDlg::SelectOpenUsingAppOption()
{
    CButton *pRadioOpenUsing = (CButton *)GetDlgItem(IDC_OPEN_USING);
    if(pRadioOpenUsing)
    {
        pRadioOpenUsing->SetCheck(1);

        pRadioOpenUsing->SetFocus();

        OnOpenUsingRadioBtnClicked();
    }
}

void CChooseActionDlg::EnableChooseBtn(BOOL bEnable)
{
    CButton *pChooseBtn = (CButton *)GetDlgItem(IDC_CHOOSE_APP);
    if(pChooseBtn)
    {
        pChooseBtn->EnableWindow(bEnable);
    }
}

void CChooseActionDlg::EnableAppName(BOOL bEnable)
{
    CStatic *pAppName = (CStatic *)GetDlgItem(IDC_APP_NAME);
    if(pAppName)
    {
        pAppName->EnableWindow(bEnable);
    }
}

void CChooseActionDlg::OnOpenUsingRadioBtnClicked()
{
    EnableChooseBtn(TRUE);
    EnableAppName(TRUE);
}

void CChooseActionDlg::OnSaveToDiskRadioBtnClicked()
{
    EnableChooseBtn(FALSE);
    EnableAppName(FALSE);
}

void CChooseActionDlg::OnChooseAppClicked() 
{	
    TCHAR *lpszFilter =
        _T("EXE Files Only (*.exe)|*.exe|")
        _T("All Files (*.*)|*.*||");

    CFileDialog cf(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                    lpszFilter, this);
    if(cf.DoModal() == IDOK)
    {
        m_OpenWithAppName = cf.GetPathName(); // Will be like: c:\tmp\junk.exe

        UpdateAppNameField(m_OpenWithAppName);
    }
}

void CChooseActionDlg::OnOK() 
{	
    CButton *pRadioOpenWithApp = (CButton *)GetDlgItem(IDC_OPEN_USING);

    if(pRadioOpenWithApp->GetCheck())
    {
        // Do not allow to leave the dialog if the OpenWithAnApp option 
        // is selected and the the name of the app is unknown/empty
        //
        if(m_OpenWithAppName.IsEmpty())
        {
            ::MessageBox(this->m_hWnd,
                _T("You have chosen to open the content with an external application, but,\nno application has been specified.\n\nPlease click the Choose... button to select an application"),
                _T("MfcEmbed"), MB_OK);
            return;
        }
        else
            m_ActionChosen = CONTENT_LAUNCH_WITH_APP;
    }
    else
        m_ActionChosen = CONTENT_SAVE_TO_DISK;

    CDialog::OnOK();
}

void CChooseActionDlg::OnCancel() 
{	
    CDialog::OnCancel();
}

BEGIN_MESSAGE_MAP(CChooseActionDlg, CDialog)
    ON_BN_CLICKED(IDC_CHOOSE_APP, OnChooseAppClicked)
    ON_BN_CLICKED(IDC_SAVE_TO_DISK, OnSaveToDiskRadioBtnClicked)
    ON_BN_CLICKED(IDC_OPEN_USING, OnOpenUsingRadioBtnClicked)
END_MESSAGE_MAP()

//*****************************************************************************
// CProgressDlg
//***************************************************************************** 

NS_IMPL_ISUPPORTS2(CProgressDlg, nsIWebProgressListener, nsISupportsWeakReference)

CProgressDlg::CProgressDlg(nsIHelperAppLauncher *aLauncher, int aHandleContentOp,
                           CString& aFileName, CWnd* pParent /*=NULL*/)
	: CDialog(CProgressDlg::IDD, pParent)
{
    m_HelperAppLauncher = aLauncher;
    m_HandleContentOp = aHandleContentOp;
    m_FileName = aFileName;
}

CProgressDlg::~CProgressDlg() 
{
}

NS_IMETHODIMP CProgressDlg::OnStateChange(nsIWebProgress *aWebProgress, 
                                          nsIRequest *aRequest, PRUint32 aStateFlags, 
                                          nsresult aStatus)
{
    if((aStateFlags & STATE_STOP) && (aStateFlags & STATE_IS_DOCUMENT))
    {
        // We've completed the download - close the progress window
        
        if(m_HelperAppLauncher)
            m_HelperAppLauncher->CloseProgressWindow();

        DestroyWindow();
    }

    return NS_OK;
}

NS_IMETHODIMP CProgressDlg::OnProgressChange(nsIWebProgress *aWebProgress, 
                                             nsIRequest *aRequest, 
                                             PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, 
                                             PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    // Update the progress control

    if(::IsWindow(m_ProgressCtrl.m_hWnd))
    {
        m_ProgressCtrl.SetRange32(0, aMaxTotalProgress);
        m_ProgressCtrl.SetPos(aCurTotalProgress);
    }

    return NS_OK;
}

NS_IMETHODIMP CProgressDlg::OnLocationChange(nsIWebProgress *aWebProgress, 
                                             nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

NS_IMETHODIMP CProgressDlg::OnStatusChange(nsIWebProgress *aWebProgress, 
                                           nsIRequest *aRequest, nsresult aStatus, 
                                           const PRUnichar *aMessage)
{
    return NS_OK;
}

NS_IMETHODIMP CProgressDlg::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                             nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}

BOOL CProgressDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

    // Set the "SavingFrom" field
    if(m_HelperAppLauncher)
    {
        nsCOMPtr<nsIURI> srcUri;
        nsresult rv = m_HelperAppLauncher->GetSource(getter_AddRefs(srcUri));
        if(NS_SUCCEEDED(rv))
        {
            nsCAutoString uriString;
            srcUri->GetSpec(uriString);
            USES_CONVERSION;
            m_SavingFrom.SetWindowText(A2CT(uriString.get()));
        }
    }

    // Set the "Action" field
    if(m_HandleContentOp == CONTENT_SAVE_TO_DISK)
        m_Action.SetWindowText(_T("[Saving file to:] ") + m_FileName);
    else if(m_HandleContentOp == CONTENT_LAUNCH_WITH_APP)
        m_Action.SetWindowText(_T("[Opening file with:] ") + m_FileName);

    return TRUE;
}

void CProgressDlg::OnCancel() 
{
    if(m_HelperAppLauncher)
        m_HelperAppLauncher->Cancel();

	DestroyWindow();
}

void CProgressDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_PROGRESS, m_ProgressCtrl);
    DDX_Control(pDX, IDC_SAVING_FROM, m_SavingFrom);
    DDX_Control(pDX, IDC_ACTION, m_Action);
}

BEGIN_MESSAGE_MAP(CProgressDlg, CDialog)
END_MESSAGE_MAP()
