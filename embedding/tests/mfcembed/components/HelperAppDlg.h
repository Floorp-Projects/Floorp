/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chak Nanga <chak@netscape.com> 
 *
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
