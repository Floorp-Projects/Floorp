/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
*/

#include "nsISecureBrowserUI.h"
#include "nsSecureBrowserUIImpl.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

#include "nsIScriptGlobalObject.h"
#include "nsIObserverService.h"
#include "nsIDocumentLoader.h"
#include "nsCURILoader.h"
#include "nsIDocShell.h"
#include "nsIDocumentViewer.h"
#include "nsCURILoader.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIChannel.h"

#include "nsIURI.h"

#include "prmem.h"

#include "nsINetSupportDialogService.h"
#include "nsIPrompt.h"
#include "nsIPref.h"

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);


#define ENTER_SITE_PREF      "security.warn_entering_secure"
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"
#define INSECURE_SUBMIT_PREF "security.warn_submit_insecure"


nsSecureBrowserUIImpl* nsSecureBrowserUIImpl::mInstance = nsnull;

nsSecureBrowserUIImpl::nsSecureBrowserUIImpl()
{
    NS_INIT_REFCNT();
}

nsSecureBrowserUIImpl::~nsSecureBrowserUIImpl()
{
}

NS_IMPL_ISUPPORTS1(nsSecureBrowserUIImpl, nsSecureBrowserUI); 

NS_IMETHODIMP      
nsSecureBrowserUIImpl::CreateSecureBrowserUI(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{                                                                  
    if (!aResult) {                                                
        return NS_ERROR_INVALID_POINTER;                           
    }                                                              
    if (aOuter) {                                                  
        *aResult = nsnull;                                         
        return NS_ERROR_NO_AGGREGATION;                              
    }                                                                
    
    if (mInstance == nsnull) 
    {
        mInstance = new nsSecureBrowserUIImpl();
    }

    if (mInstance == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = mInstance->QueryInterface(aIID, aResult);                        
    if (NS_FAILED(rv)) 
    {                                             
        *aResult = nsnull;                                           
    }                                                                
    return rv;                                                       
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::Init(nsIDOMWindow *window, nsIDOMElement *button)
{
	
	nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(window);
	if (sgo)
	{
      nsCOMPtr<nsIDocShell> docShell;

		sgo->GetDocShell(getter_AddRefs(docShell));
		if (docShell)
		{
			nsCOMPtr<nsSecureBrowserObserver> sbo = new nsSecureBrowserObserver();
			if (sbo)
			{
				// sbo->Init has the docShell addref sbo
				return sbo->Init(button, docShell); // does the window delete us when it close?
			}
		}
	}
	return NS_OK;
}


nsSecureBrowserObserver::nsSecureBrowserObserver()
{
	NS_INIT_REFCNT();
	mIsSecureDocument = mMixContentAlertShown = mIsDocumentBroken = PR_FALSE;

}

nsSecureBrowserObserver::~nsSecureBrowserObserver()
{
}

NS_IMPL_ISUPPORTS1(nsSecureBrowserObserver, nsIDocumentLoaderObserver); 


nsresult
nsSecureBrowserObserver::Init(nsIDOMElement *button, nsIDocShell* content)
{
	if (!button || !content)
		return NS_ERROR_NULL_POINTER;

	mSecurityButton = button;
	content->GetDocLoaderObserver(getter_AddRefs(mOldWebShellObserver));
	content->SetDocLoaderObserver(this);
	return NS_OK;
}


NS_IMETHODIMP
nsSecureBrowserObserver::OnStartDocumentLoad(nsIDocumentLoader* aLoader, 
                                       nsIURI* aURL, 
                                       const char* aCommand)
{
	nsresult res;
	
	if (mOldWebShellObserver)
	{
		mOldWebShellObserver->OnStartDocumentLoad(aLoader, aURL, aCommand);
	}
	
	
	if (!mSecurityButton)
		return NS_OK;
    
	if (!aURL || !aLoader)
		return NS_ERROR_NULL_POINTER;

	
	mIsSecureDocument = mMixContentAlertShown = mIsDocumentBroken = PR_FALSE;
	  
    // check to see that we are going to load the same 
	// kind of URL (scheme) as we just loaded.
	
	
	PRBool isOldSchemeSecure;
	res	= IsSecureDocumentLoad(aLoader, &isOldSchemeSecure);
	if (NS_FAILED(res)) 
		return NS_OK;
	
	PRBool isNewSchemeSecure; 
	res	= IsSecureUrl(PR_FALSE, aURL, &isNewSchemeSecure);
	if (NS_FAILED(res)) 
		return NS_OK;

#if DEBUG_dougt
	printf("[StartPageLoad]  isOldSchemeSecure = %d isNewSchemeSecure = %d\n", isOldSchemeSecure, isNewSchemeSecure);
#endif
	// if we are going from a secure page to and insecure page
	if ( !isNewSchemeSecure && isOldSchemeSecure)
	{
#if DEBUG_dougt
		printf("change lock icon to unlock - new document\n");
#endif
		mSecurityButton->RemoveAttribute( "level" );
		
		
		PRBool boolpref;
		NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res);
		if (NS_FAILED(res)) 
			return res;
	
		if ((prefs->GetBoolPref(LEAVE_SITE_PREF, &boolpref) != 0))
			boolpref = PR_TRUE;
		
		if (boolpref) 
		{
			NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
			if (NS_FAILED(res)) 
				return res;

			dialog->Alert(nsString("You are leaving a secure document").GetUnicode());  // fix localize!
		}
	}
	// if we are going from an insecure page to a secure one.
	else if (isNewSchemeSecure && !isOldSchemeSecure)
	{
		PRBool boolpref;
		NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res);
		if (NS_FAILED(res)) 
			return res;

		if ((prefs->GetBoolPref(ENTER_SITE_PREF, &boolpref) != 0))
			boolpref = PR_TRUE;
		
		if (boolpref) 
		{
			NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
			if (NS_FAILED(res)) 
				return res;

			dialog->Alert(nsString("You are entering a secure document").GetUnicode());  // fix localize!
		}
	}

	mIsSecureDocument = isNewSchemeSecure;

    return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserObserver::OnEndDocumentLoad(nsIDocumentLoader* aLoader, 
                                     nsIChannel* channel, 
                                     nsresult aStatus)
{
	nsresult rv;
	
	if (mOldWebShellObserver)
	{
		mOldWebShellObserver->OnEndDocumentLoad(aLoader, channel, aStatus);
	}

	if (!mIsSecureDocument)
		return NS_OK;
	
	if (!mSecurityButton)
        return NS_ERROR_NULL_POINTER;

#if DEBUG_dougt
	printf("[EndPageLoad]  mIsSecureDocument = %d aStatus = %d mIsDocumentBroken = %d\n", mIsSecureDocument, aStatus, mIsDocumentBroken);
#endif
	
	if ( NS_SUCCEEDED(aStatus) && !mIsDocumentBroken )
    {
#if DEBUG_dougt
        printf("change lock icon to secure \n");
#endif
        rv = mSecurityButton->SetAttribute( "level", nsString("high") );
		mIsSecureDocument = PR_TRUE;
	}
    else
    {
#if DEBUG_dougt
        printf("change lock icon to broken\n");
#endif
        rv = mSecurityButton->SetAttribute( "level", nsString("broken") );
		mIsSecureDocument = PR_FALSE;            
    }

    return rv;
}

NS_IMETHODIMP
nsSecureBrowserObserver::OnStartURLLoad(nsIDocumentLoader* loader, 
                                  nsIChannel* channel)
{
	if (mOldWebShellObserver)
	{
		mOldWebShellObserver->OnStartURLLoad(loader, channel);
	}
	
#if DEBUG_dougt
	printf("[StartURLLoad]  mIsSecureDocument = %d\n", mIsSecureDocument);
#endif

	PRBool secure;
	nsresult rv = IsSecureChannelLoad(channel, &secure);
	if (NS_FAILED(rv)) 
		return rv;

	if (mIsSecureDocument && !secure)
	{
		mIsDocumentBroken = PR_TRUE;

//		nsCOMPtr<nsIURI> uri;
//		channel->GetURI(getter_AddRefs(uri));

//		uri->SetSpec("chrome://navigator/skin/insecureLink.gif");  //fix

		nsresult res;  

		PRBool boolpref;
		NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res);
		if (NS_FAILED(res)) 
			return res;

		if ((prefs->GetBoolPref(MIXEDCONTENT_PREF, &boolpref) != 0))
			boolpref = PR_TRUE;

		if (boolpref && !mMixContentAlertShown) 
		{
			NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
			if (NS_FAILED(res)) 
				return res;

			dialog->Alert(nsString("There is mixed content on this page").GetUnicode());  // fix localize!
			mMixContentAlertShown = PR_TRUE;
		}
	}
    return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserObserver::OnProgressURLLoad(nsIDocumentLoader* loader, 
                                     nsIChannel* channel, 
                                     PRUint32 aProgress, 
                                     PRUint32 aProgressMax)
{
	if (mOldWebShellObserver)
	{
		mOldWebShellObserver->OnProgressURLLoad(loader, channel, aProgress, aProgressMax);
	}
	return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserObserver::OnStatusURLLoad(nsIDocumentLoader* loader, 
                                   nsIChannel* channel, 
                                   nsString& aMsg)
{
	if (mOldWebShellObserver)
	{
		mOldWebShellObserver->OnStatusURLLoad(loader, channel, aMsg);
	}
	return NS_OK;
}


NS_IMETHODIMP
nsSecureBrowserObserver::OnEndURLLoad(nsIDocumentLoader* loader, 
                                nsIChannel* channel, 
                                nsresult aStatus)
{
    if (mOldWebShellObserver)
	{
		mOldWebShellObserver->OnEndURLLoad(loader, channel, aStatus);
	}

#if DEBUG_dougt
	printf("[OnEndURLLoad]  mIsSecureDocument = %d aStatus = %d\n", mIsSecureDocument, aStatus);
#endif

	if ( mIsSecureDocument && NS_FAILED(aStatus))
	{
#if DEBUG_dougt
		printf("change lock icon to broken\n");
#endif
		mSecurityButton->SetAttribute( "level", nsString("broken") );
		mIsDocumentBroken = PR_TRUE;
	}
	return NS_OK;
}

// fileSecure flag determines if we should include file: and other local protocols.
nsresult
nsSecureBrowserObserver::IsSecureUrl(PRBool fileSecure, nsIURI* aURL, PRBool* value)
{
	*value = PR_FALSE;

	if (!aURL)
		return NS_ERROR_NULL_POINTER;
#if DEBUG_dougt
	char* string;
	aURL->GetSpec(&string);
	printf("[ensuring channel]: %s\n", string);
	nsAllocator::Free(string);
#endif

	char* scheme;
	aURL->GetScheme(&scheme);

	if (scheme == nsnull)
		return NS_ERROR_NULL_POINTER;

    if ( (strncmp(scheme, "https",  5) == 0) ||
		 (fileSecure &&
		 (strncmp(scheme, "file",   4) == 0) ))
		*value = PR_TRUE;
	
	nsAllocator::Free(scheme);
	return NS_OK;
    
}


nsresult nsSecureBrowserObserver::IsSecureDocumentLoad(nsIDocumentLoader* loader, PRBool *value)
{
	if (!loader)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIURI> uri;
	nsresult rv = GetURIFromDocumentLoader(loader, getter_AddRefs(uri));
	
	if (NS_FAILED(rv))
		return rv;

	return IsSecureUrl(PR_FALSE, uri, value);
}

nsresult nsSecureBrowserObserver::IsSecureChannelLoad(nsIChannel* channel, PRBool *value)
{
	if (!channel)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIURI> uri;
	nsresult rv = channel->GetURI(getter_AddRefs(uri));
	if (NS_FAILED(rv))
		return rv;

	return IsSecureUrl(PR_TRUE, uri, value);
}

nsresult 
nsSecureBrowserObserver::GetURIFromDocumentLoader(nsIDocumentLoader* aLoader, nsIURI** uri)
{	
	nsresult rv;

    if (aLoader == nsnull)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsISupports> cont;
    rv = aLoader->GetContainer(getter_AddRefs(cont));
    if (NS_FAILED(rv) || (cont == nsnull)) 
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(cont));
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

    nsCOMPtr<nsIContentViewer> cv;
    rv = docShell->GetContentViewer(getter_AddRefs(cv));
    if (NS_FAILED(rv) || (cv == nsnull)) 
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIDocumentViewer> docViewer(do_QueryInterface(cv));
    NS_ENSURE_TRUE(docViewer, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocument> doc;
    rv = docViewer->GetDocument(*getter_AddRefs(doc));
    if (NS_FAILED(rv) || (doc == nsnull)) 
        return NS_ERROR_NULL_POINTER;

    *uri = doc->GetDocumentURL();
    if (!*uri) 
        return NS_ERROR_NULL_POINTER;
    
	return NS_OK;
}
