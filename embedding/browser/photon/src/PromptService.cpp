/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* PromptService is intended to override the default Mozilla PromptService,
   giving nsIPrompt implementations of our own design, rather than using
   Mozilla's. Do this by building this into a component and registering the
   factory with the same CID/ContractID as Mozilla's (see MfcEmbed.cpp).
*/

#include "PromptService.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsIDOMWindow.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIFactory.h"
#include "nsIPromptService.h"
#include "nsIServiceManager.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWindowWatcher.h"
#include "PtMozilla.h"


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
	CWebBrowserContainer *GetWebBrowser(nsIDOMWindow *aWindow);
};

//*****************************************************************************

NS_IMPL_ISUPPORTS1(CPromptService, nsIPromptService)

CPromptService::CPromptService() :
	mWWatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1")) {
	NS_INIT_REFCNT();
	}

CPromptService::~CPromptService() {
	}

CWebBrowserContainer *CPromptService::GetWebBrowser(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsIWebBrowserChrome> chrome;
  CWebBrowserContainer *val = 0;

	nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
	if (!wwatch) return nsnull;

  if( wwatch ) {
    nsCOMPtr<nsIDOMWindow> fosterParent;
    if (!aWindow) { // it will be a dependent window. try to find a foster parent.
      wwatch->GetActiveWindow(getter_AddRefs(fosterParent));
      aWindow = fosterParent;
    }
    nsresult rv = wwatch->GetChromeForWindow(aWindow, getter_AddRefs(chrome));
  }

  if (chrome) {
    nsCOMPtr<nsIEmbeddingSiteWindow> site(do_QueryInterface(chrome));
    if (site) {
      site->GetSiteWindow(reinterpret_cast<void **>(&val));
    }
  }

  return val;
}


NS_IMETHODIMP CPromptService::Alert(nsIDOMWindow *parent, const PRUnichar *dialogTitle,
                                    const PRUnichar *text)
{
	nsString 			mTitle(dialogTitle);
	nsString 			mText(text);
	CWebBrowserContainer *w = GetWebBrowser( parent );
	w->InvokeDialogCallback(Pt_MOZ_DIALOG_ALERT, mTitle.ToNewCString(), mText.ToNewCString(), nsnull, nsnull);

	return NS_OK;
	}

NS_IMETHODIMP CPromptService::AlertCheck(nsIDOMWindow *parent,
                                         const PRUnichar *dialogTitle,
                                         const PRUnichar *text,
                                         const PRUnichar *checkboxMsg,
                                         PRBool *checkValue)
{
	nsString 	mTitle(dialogTitle);
	nsString 	mText(text);
	nsString 	mMsg(checkboxMsg);
	int 		ret;
	CWebBrowserContainer *w = GetWebBrowser( parent );

	w->InvokeDialogCallback(Pt_MOZ_DIALOG_ALERT, mTitle.ToNewCString(), mText.ToNewCString(), \
			mMsg.ToNewCString(), &ret);
	*checkValue = ret;

	return NS_OK;
	}


NS_IMETHODIMP CPromptService::Confirm(nsIDOMWindow *parent,
                                      const PRUnichar *dialogTitle,
                                      const PRUnichar *text,
                                      PRBool *_retval)
{
	nsString 			mTitle(dialogTitle);
	nsString 			mText(text);
	CWebBrowserContainer *w = GetWebBrowser( parent );

	if (w->InvokeDialogCallback(Pt_MOZ_DIALOG_CONFIRM, mTitle.ToNewCString(), mText.ToNewCString(), nsnull, nsnull) == Pt_CONTINUE)
		*_retval = PR_TRUE;
	else
		*_retval = PR_FALSE;

	return NS_OK;
}

NS_IMETHODIMP CPromptService::ConfirmCheck(nsIDOMWindow *parent,
                                           const PRUnichar *dialogTitle,
                                           const PRUnichar *text,
                                           const PRUnichar *checkboxMsg,
                                           PRBool *checkValue,
                                           PRBool *_retval)
{
  // TODO implement proper confirmation checkbox dialog
  return Confirm(parent, dialogTitle, text, _retval);
}

NS_IMETHODIMP CPromptService::Prompt(nsIDOMWindow *parent,
                                     const PRUnichar *dialogTitle,
                                     const PRUnichar *text,
                                     PRUnichar **value,
                                     const PRUnichar *checkboxMsg,
                                     PRBool *checkValue,
                                     PRBool *_retval)
{
	nsString 	mTitle(dialogTitle);
	nsString 	mText(text);
	nsString 	mMsg(checkboxMsg);
	int 		ret;
	CWebBrowserContainer *w = GetWebBrowser( parent );


	if (w->InvokeDialogCallback(Pt_MOZ_DIALOG_CONFIRM, mTitle.ToNewCString(), mText.ToNewCString(), \
			mMsg.ToNewCString(), &ret) == Pt_CONTINUE)
		*_retval = PR_TRUE;
	else
		*_retval = PR_FALSE;
	*checkValue = ret;

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
	CWebBrowserContainer *w = GetWebBrowser( parent );
	PtMozillaWidget_t 			*moz = (PtMozillaWidget_t *) w->m_pOwner;
	PtCallbackList_t 			*cb;
	PtCallbackInfo_t 			cbinfo;
	PtMozillaAuthenticateCb_t   auth;
	nsString 					mTitle(dialogTitle);
	nsString 					mRealm(checkboxMsg);

/* ATENTIE */ printf( "CWebBrowserContainer::PromptUsernameAndPassword\n" );

	if (!moz->auth_cb)
	    return NS_OK;

	cb = moz->auth_cb;
	memset(&cbinfo, 0, sizeof(cbinfo));
	cbinfo.reason = Pt_CB_MOZ_AUTHENTICATE;
	cbinfo.cbdata = &auth;

	memset(&auth, 0, sizeof(PtMozillaAuthenticateCb_t));
	auth.title = mTitle.ToNewCString();
	auth.realm = mRealm.ToNewCString();

  if (PtInvokeCallbackList(cb, (PtWidget_t *)moz, &cbinfo) == Pt_CONTINUE)
    {
		nsCString	mUser(auth.user);
		nsCString	mPass(auth.pass);
		*username = mUser.ToNewUnicode();
		*password = mPass.ToNewUnicode();
    	*_retval = PR_TRUE;
    }
    else
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
/* ATENTIE */ printf( "CWebBrowserContainer::PromptPassword\n" );
	return NS_ERROR_NOT_IMPLEMENTED;
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

void InitPromptService( ) {
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
