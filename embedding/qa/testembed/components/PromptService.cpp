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

/* PromptService is intended to override the default Mozilla PromptService,
   giving nsIPrompt implementations of our own design, rather than using
   Mozilla's. Do this by building this into a component and registering the
   factory with the same CID/ContractID as Mozilla's (see MfcEmbed.cpp).
*/

#include "stdafx.h"
#include "Dialogs.h"
#include "PromptService.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIDOMWindow.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIFactory.h"
#include "nsIPromptService.h"
#include "nsIServiceManager.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWindowWatcher.h"

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
// CPromptService
//*****************************************************************************

class CPromptService: public nsIPromptService {
public:
                 CPromptService();
  virtual       ~CPromptService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROMPTSERVICE

private:
  nsCOMPtr<nsIWindowWatcher> mWWatch;
  CWnd *CWndForDOMWindow(nsIDOMWindow *aWindow);
};

//*****************************************************************************

NS_IMPL_ISUPPORTS1(CPromptService, nsIPromptService)

CPromptService::CPromptService() :
  mWWatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"))
{
  NS_INIT_REFCNT();
}

CPromptService::~CPromptService() {
}

CWnd *
CPromptService::CWndForDOMWindow(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsIWebBrowserChrome> chrome;
  CWnd *val = 0;

  if (mWWatch) {
    nsCOMPtr<nsIDOMWindow> fosterParent;
    if (!aWindow) { // it will be a dependent window. try to find a foster parent.
      mWWatch->GetActiveWindow(getter_AddRefs(fosterParent));
      aWindow = fosterParent;
    }
    mWWatch->GetChromeForWindow(aWindow, getter_AddRefs(chrome));
  }

  if (chrome) {
    nsCOMPtr<nsIEmbeddingSiteWindow> site(do_QueryInterface(chrome));
    if (site) {
      HWND w;
      site->GetSiteWindow(reinterpret_cast<void **>(&w));
      val = CWnd::FromHandle(w);
    }
  }

  return val;
}

NS_IMETHODIMP CPromptService::Alert(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                    const PRUnichar *text)
{
  USES_CONVERSION;
  CWnd *wnd = CWndForDOMWindow(parent);
  if (wnd)
    wnd->MessageBox(W2T(text), W2T(dialogTitle), MB_OK | MB_ICONEXCLAMATION);
  else
    ::MessageBox(0, W2T(text), W2T(dialogTitle), MB_OK | MB_ICONEXCLAMATION);

  return NS_OK;
}

NS_IMETHODIMP CPromptService::AlertCheck(nsIDOMWindow *parent,
                                         const PRUnichar *dialogTitle,
                                         const PRUnichar *text,
                                         const PRUnichar *checkboxMsg,
                                         PRBool *checkValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CPromptService::Confirm(nsIDOMWindow *parent,
                                      const PRUnichar *dialogTitle,
                                      const PRUnichar *text,
                                      PRBool *_retval)
{
  USES_CONVERSION;
  CWnd *wnd = CWndForDOMWindow(parent);
  int choice;

  if (wnd)
    choice = wnd->MessageBox(W2T(text), W2T(dialogTitle),
                      MB_YESNO | MB_ICONEXCLAMATION);
  else
    choice = ::MessageBox(0, W2T(text), W2T(dialogTitle),
                      MB_YESNO | MB_ICONEXCLAMATION);

  *_retval = choice == IDYES ? PR_TRUE : PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP CPromptService::ConfirmCheck(nsIDOMWindow *parent,
                                           const PRUnichar *dialogTitle,
                                           const PRUnichar *text,
                                           const PRUnichar *checkboxMsg,
                                           PRBool *checkValue,
                                           PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CPromptService::Prompt(nsIDOMWindow *parent,
                                     const PRUnichar *dialogTitle,
                                     const PRUnichar *text,
                                     PRUnichar **value,
                                     const PRUnichar *checkboxMsg,
                                     PRBool *checkValue,
                                     PRBool *_retval)
{
  ResourceState setState;
  USES_CONVERSION;

  CWnd *wnd = CWndForDOMWindow(parent);
  CPromptDialog dlg(wnd, W2T(dialogTitle), W2T(text),
                    text && *text ? W2T(text) : 0,
                    checkValue != nsnull, W2T(checkboxMsg),
                    checkValue ? *checkValue : 0);
  if(dlg.DoModal() == IDOK) {
    // Get the value entered in the editbox of the PromptDlg
    if (value && *value) {
      nsMemory::Free(*value);
      *value = nsnull;
    }
    nsString csPromptEditValue;
    csPromptEditValue.AssignWithConversion(dlg.m_csPromptAnswer.GetBuffer(0));

    *value = ToNewUnicode(csPromptEditValue);

    *_retval = PR_TRUE;
  } else
    *_retval = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP CPromptService::PromptUsernameAndPassword(nsIDOMWindow *parent,
                                                        const PRUnichar *dialogTitle,
                                                        const PRUnichar *text,
                                                        PRUnichar **username,
                                                        PRUnichar **password,
                                                        const PRUnichar *checkboxMsg,
                                                        PRBool *checkValue,
                                                        PRBool *_retval)
{
  ResourceState setState;
  USES_CONVERSION;

  CWnd *wnd = CWndForDOMWindow(parent);
  CPromptUsernamePasswordDialog dlg(wnd, W2T(dialogTitle), W2T(text),
                    username && *username ? W2T(*username) : 0,
                    password && *password ? W2T(*password) : 0, 
                    checkValue != nsnull, W2T(checkboxMsg),
                    checkValue ? *checkValue : 0);

  if (dlg.DoModal() == IDOK) {
    // Get the username entered
    if (username && *username) {
        nsMemory::Free(*username);
        *username = nsnull;
    }
    nsString csUserName;
    csUserName.AssignWithConversion(dlg.m_csUserName.GetBuffer(0));
    *username = ToNewUnicode(csUserName);

    // Get the password entered
    if (password && *password) {
      nsMemory::Free(*password);
      *password = nsnull;
    }
    nsString csPassword;
    csPassword.AssignWithConversion(dlg.m_csPassword.GetBuffer(0));
    *password = ToNewUnicode(csPassword);

    if(checkValue)		
      *checkValue = dlg.m_bCheckBoxValue;

    *_retval = PR_TRUE;
  } else
    *_retval = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP CPromptService::PromptPassword(nsIDOMWindow *parent,
                                             const PRUnichar *dialogTitle,
                                             const PRUnichar *text,
                                             PRUnichar **password,
                                             const PRUnichar *checkboxMsg,
                                             PRBool *checkValue,
                                             PRBool *_retval)
{
  ResourceState setState;
  USES_CONVERSION;

  CWnd *wnd = CWndForDOMWindow(parent);
  CPromptPasswordDialog dlg(wnd, W2T(dialogTitle), W2T(text),
                            password && *password ? W2T(*password) : 0,
                            checkValue != nsnull, W2T(checkboxMsg),
                            checkValue ? *checkValue : 0);
  if(dlg.DoModal() == IDOK) {
    // Get the password entered
    if (password && *password) {
        nsMemory::Free(*password);
        *password = nsnull;
    }
    nsString csPassword;
    csPassword.AssignWithConversion(dlg.m_csPassword.GetBuffer(0));
    *password = ToNewUnicode(csPassword);

    if(checkValue)
      *checkValue = dlg.m_bCheckBoxValue;

    *_retval = PR_TRUE;
  } else
    *_retval = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP CPromptService::Select(nsIDOMWindow *parent,
                                     const PRUnichar *dialogTitle,
                                     const PRUnichar *text, PRUint32 count,
                                     const PRUnichar **selectList,
                                     PRInt32 *outSelection,
                                     PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP CPromptService::ConfirmEx(nsIDOMWindow *parent,
                                        const PRUnichar *dialogTitle,
                                        const PRUnichar *text,
                                        PRUint32 buttonFlags,
                                        const PRUnichar *button0Title,
                                        const PRUnichar *button1Title,
                                        const PRUnichar *button2Title,
                                        const PRUnichar *checkMsg,
                                        PRBool *checkValue,
                                        PRInt32 *buttonPressed)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
//*****************************************************************************
// CPromptServiceFactory
//*****************************************************************************   

class CPromptServiceFactory : public nsIFactory {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY

  CPromptServiceFactory();
  virtual ~CPromptServiceFactory();
};

//*****************************************************************************

NS_IMPL_ISUPPORTS1(CPromptServiceFactory, nsIFactory)

CPromptServiceFactory::CPromptServiceFactory() {

  NS_INIT_ISUPPORTS();
}

CPromptServiceFactory::~CPromptServiceFactory() {
}

NS_IMETHODIMP CPromptServiceFactory::CreateInstance(nsISupports *aOuter, const nsIID & aIID, void **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  
  *aResult = NULL;  
  CPromptService *inst = new CPromptService;    
  if (!inst)
    return NS_ERROR_OUT_OF_MEMORY;
    
  nsresult rv = inst->QueryInterface(aIID, aResult);
  if (rv != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  
    
  return rv;
}

NS_IMETHODIMP CPromptServiceFactory::LockFactory(PRBool lock)
{
  return NS_OK;
}

//*****************************************************************************

void InitPromptService(HINSTANCE instance) {

  gInstance = instance;
}

nsresult NS_NewPromptServiceFactory(nsIFactory** aFactory)
{
  NS_ENSURE_ARG_POINTER(aFactory);
  *aFactory = nsnull;
  
  CPromptServiceFactory *result = new CPromptServiceFactory;
  if (!result)
    return NS_ERROR_OUT_OF_MEMORY;
    
  NS_ADDREF(result);
  *aFactory = result;
  
  return NS_OK;
}
