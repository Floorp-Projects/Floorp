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

#ifndef __HelperAppDlg_h
#define __HelperAppDlg_h

#include "nsError.h"
#include "resource.h"
#include "nsIFactory.h"
#include "nsIExternalHelperAppService.h"
#include "nsIHelperAppLauncherDialog.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "nsIWindowWatcher.h"

// this component is for an MFC app; it's Windows. make sure this is defined.
#ifndef XP_WIN
#define XP_WIN
#endif

class CHelperAppLauncherDialogFactory : public nsIFactory {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIFACTORY

    CHelperAppLauncherDialogFactory();
    virtual ~CHelperAppLauncherDialogFactory();
};

enum {
    CONTENT_SAVE_TO_DISK,
    CONTENT_LAUNCH_WITH_APP
};

// This class implements the nsIHelperAppLauncherDialog interface
//
class CHelperAppLauncherDialog : public nsIHelperAppLauncherDialog {
public:
    CHelperAppLauncherDialog();
    virtual ~CHelperAppLauncherDialog();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIHELPERAPPLAUNCHERDIALOG

    int m_HandleContentOp;
    CString m_FileName;

private:
  nsCOMPtr<nsIWindowWatcher> mWWatch;
  CWnd* GetParentFromContext(nsISupports *aWindowContext);
};

// This class implements a download progress dialog
// which displays the progress of a file download
//
class CProgressDlg : public CDialog, 
                     public nsIWebProgressListener,
                     public nsSupportsWeakReference
{
public:
    enum { IDD = IDD_PROGRESS_DIALOG };

    CProgressDlg(nsIHelperAppLauncher *aLauncher, int aHandleContentOp,
                CString& aFileName, CWnd* pParent = NULL);
    virtual ~CProgressDlg();

    int m_HandleContentOp;
    CString m_FileName;
    CStatic	m_SavingFrom;
    CStatic	m_Action;
    CProgressCtrl m_ProgressCtrl;

    nsCOMPtr<nsIHelperAppLauncher> m_HelperAppLauncher;

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnCancel();

	DECLARE_MESSAGE_MAP()
};

// This class implements a dialog box which gives the user
// a choice of saving the content being downloaded to disk
// or to open with an external application
//
class CChooseActionDlg : public CDialog
{
public:
    CChooseActionDlg(nsIHelperAppLauncher* aLauncher, CWnd* pParent = NULL);

    enum { IDD = IDD_CHOOSE_ACTION_DIALOG };
    CStatic	m_ContentType;

    int m_ActionChosen;
    CString m_OpenWithAppName;
    nsCOMPtr<nsIHelperAppLauncher> m_HelperAppLauncher;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

protected:
    virtual BOOL OnInitDialog();
    afx_msg void OnChooseAppClicked();
    virtual void OnOK();
    virtual void OnCancel();
    afx_msg void OnOpenUsingRadioBtnClicked();
    afx_msg void OnSaveToDiskRadioBtnClicked();

    void EnableAppName(BOOL bEnable);
    void EnableChooseBtn(BOOL bEnable);
    void InitWithPreferredAction(nsIMIMEInfo* aMimeInfo);
    void SelectSaveToDiskOption();
    void SelectOpenUsingAppOption();
    void UpdateAppNameField(CString& appName);

    DECLARE_MESSAGE_MAP()
};

#endif
