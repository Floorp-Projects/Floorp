/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIDOMMsgAppCore.h"
#include "nsMsgAppCore.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMBaseAppCore.h"
#include "nsJSMsgAppCore.h"

/* rhp - for access to webshell */
#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"

#include "nsNNTPProtocol.h" // mscott - hopefully this dependency should only be temporary...

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

NS_BEGIN_EXTERN_C

nsresult NS_MailNewsLoadUrl(const nsString& urlString, nsISupports * aConsumer);

NS_END_EXTERN_C

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

class nsMsgAppCore : public nsIDOMMsgAppCore,
                     public nsIScriptObjectOwner
{
  
public:
  nsMsgAppCore();
  virtual ~nsMsgAppCore();

  NS_DECL_ISUPPORTS
  NS_DECL_IDOMBASEAPPCORE

  // nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  // nsIMsgAppCore
  NS_IMETHOD Open3PaneWindow();
  NS_IMETHOD GetNewMail();
  NS_IMETHOD SetWindow(nsIDOMWindow* aWin);
  NS_IMETHOD OpenURL(char * url);

private:
  
  nsString mId;
  void *mScriptObject;

  /* rhp - need this to drive message display */
  nsIDOMWindow       *mWindow;
  nsIWebShell        *mWebShell;
};

nsresult nsMsgAppCore::SetDocumentCharset(class nsString const & aCharset) 
{
	nsresult res = NS_OK;
	if (nsnull != mWindow) 
	{
		nsIDOMDocument* domDoc;
		res = mWindow->GetDocument(&domDoc);
		if (NS_SUCCEEDED(res) && nsnull != domDoc) 
		{
			nsIDocument * doc;
			res = domDoc->QueryInterface(nsIDocument::GetIID(), (void**)&doc);
			if (NS_SUCCEEDED(res) && nsnull != doc) 
			{
				nsString *aNewCharset = new nsString(aCharset);
				if (nsnull != aNewCharset) 
				{
					doc->SetDocumentCharacterSet(aNewCharset);
				}
				
				NS_RELEASE(doc);
			}
			
			NS_RELEASE(domDoc);
		}
	}
	return res;
}

//
// nsMsgAppCore
//
nsMsgAppCore::nsMsgAppCore()
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  mWebShell = nsnull; 
  mWindow = nsnull;
}

nsMsgAppCore::~nsMsgAppCore()
{

}

//
// nsISupports
//
NS_IMPL_ADDREF(nsMsgAppCore);
NS_IMPL_RELEASE(nsMsgAppCore);

NS_IMETHODIMP
nsMsgAppCore::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
      return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIScriptObjectOwnerIID)) {
      *aInstancePtr = (void*) ((nsIScriptObjectOwner*)this);
      AddRef();
      return NS_OK;
  }
  if ( aIID.Equals(nsIDOMBaseAppCore::GetIID())) {
      *aInstancePtr = (void*) ((nsIDOMBaseAppCore*)this);
      AddRef();
      return NS_OK;
  }
  else if ( aIID.Equals(nsIDOMMsgAppCore::GetIID()) ) {
      *aInstancePtr = (void*) (nsIDOMMsgAppCore*)this;
      AddRef();
      return NS_OK;
  }
  else if ( aIID.Equals(kISupportsIID) ) {
      *aInstancePtr = (void*)(nsISupports*) (nsIScriptObjectOwner *) this;
      AddRef();
      return NS_OK;
  }

  return NS_NOINTERFACE;
}

//
// nsIScriptObjectOwner
//
nsresult
nsMsgAppCore::GetScriptObject(nsIScriptContext *aContext, void **aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) 
  {
      res = NS_NewScriptMsgAppCore(aContext, 
                                   (nsISupports *)(nsIDOMMsgAppCore*)this, 
                                   nsnull, 
                                   &mScriptObject);
  }

  *aScriptObject = mScriptObject;
  return res;
}

nsresult
nsMsgAppCore::SetScriptObject(void* aScriptObject)
{
  
  return NS_OK;
} 

//
// nsIDOMBaseAppCore
//
nsresult
nsMsgAppCore::Init(const nsString& aId)
{
#ifdef NS_DEBUG
  printf("Init\n");
#endif
  mId = aId;
  return NS_OK;
}


nsresult
nsMsgAppCore::GetId(nsString& aId)
{
#ifdef NS_DEBUG
  printf("GetID\n");
#endif
  aId = mId;
  return NS_OK;
}

//
// nsIMsgAppCore
//
NS_IMETHODIMP    
nsMsgAppCore::Open3PaneWindow()
{
	static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

    char *  urlstr=nsnull;
	nsresult rv;
	nsString controllerCID;

	urlstr = "resource:/res/samples/messenger.html";
    nsService<nsIAppShellService> appShell(kAppShellServiceCID, &rv);
    if (NS_FAILED(rv)) goto done;
	nsIURL* url;
	nsIWidget* newWindow;
  
	rv = NS_NewURL(&url, urlstr);
	if (NS_FAILED(rv)) goto done;

	controllerCID = "6B75BB61-BD41-11d2-9D31-00805F8ADDDE";
	appShell->CreateTopLevelWindow(nsnull,      // parent
                                   url,
                                   controllerCID,
                                   newWindow,   // result widget
                                   nsnull,      // observer
                                   nsnull,      // callbacks
                                   200,         // width
                                   200);        // height
	done:
	NS_RELEASE(url);
	return NS_OK;
}

nsresult
nsMsgAppCore::GetNewMail()
{
#ifdef NS_DEBUG
  printf("nsMsgAppCore::GetNewMail()\n");
#endif
  return NS_OK;
}
                              
extern "C"
nsresult
NS_NewMsgAppCore(nsIDOMMsgAppCore **aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;

  nsMsgAppCore *appcore = new nsMsgAppCore();
  if (appcore) {
    return appcore->QueryInterface(nsIDOMMsgAppCore::GetIID(),
                                   (void **)aResult);

  }
  return NS_ERROR_NOT_INITIALIZED;
}


NS_IMETHODIMP    
nsMsgAppCore::SetWindow(nsIDOMWindow* aWin)
{
  nsAutoString  webShellName("browser.webwindow");
  mWindow = aWin;
  NS_ADDREF(aWin);

  /* rhp - Needed to access the webshell to drive message display */
#ifdef NS_DEBUG
  printf("nsMsgAppCore::SetWindow(): Getting the webShell of interest...\n");
#endif

  nsresult rv;
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin, &rv) );
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsIWebShell *webShell = nsnull;
  nsIWebShell *rootWebShell = nsnull;

  globalObj->GetWebShell(&webShell);
  if (nsnull == webShell) 
  {
    return NS_ERROR_FAILURE;
  }

  webShell->GetRootWebShell(rootWebShell);
  if (nsnull != rootWebShell) 
  {
    rootWebShell->FindChildWithName(webShellName, mWebShell);
#ifdef NS_DEBUG
    if (nsnull != mWebShell)
        printf("nsMsgAppCore::SetWindow(): Got the webShell %s.\n", webShellName.ToNewCString());
    else
        printf("nsMsgAppCore::SetWindow(): Failed to find webshell %s.\n", webShellName.ToNewCString());
#endif
    NS_RELEASE(rootWebShell);
  }

  NS_RELEASE(webShell);
	return NS_OK;
}

NS_IMETHODIMP
nsMsgAppCore::OpenURL(char * url)
{
    // here's where we call mscott's LoadURL...
	// mscott: right now, this only works for news urls!!!!
	NS_MailNewsLoadUrl(url, mWebShell);
	return NS_OK;
}

//  to load the webshell!
//  mWebShell->LoadURL(nsAutoString("http://www.netscape.com"), 
//                      nsnull, PR_TRUE, nsURLReload, 0);
