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
#include "nsIPSMComponent.h"
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
#include "nsIPSMSocketInfo.h"

#include "nsIURI.h"

#include "prmem.h"

#include "nsINetSupportDialogService.h"
#include "nsIPrompt.h"
#include "nsICommonDialogs.h"
#include "nsIPref.h"

static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kCommonDialogsCID,         NS_CommonDialog_CID );
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define ENTER_SITE_PREF      "security.warn_entering_secure"
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"
#define INSECURE_SUBMIT_PREF "security.warn_submit_insecure"

#define STRING_BUNDLE_URL "chrome://navigator/locale/security.properties"


 
NS_IMETHODIMP                                                            
nsSecureBrowserUIImpl::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) 
{                                                                               
    nsresult rv;                                                                
                                                                                
    nsSecureBrowserUIImpl * inst;                                                      
                                                                                
    if (NULL == aResult) {                                                      
        rv = NS_ERROR_NULL_POINTER;                                             
        return rv;                                                              
    }                                                                           
    *aResult = NULL;                                                            
    if (NULL != aOuter) {                                                       
        rv = NS_ERROR_NO_AGGREGATION;                                           
        return rv;                                                              
    }                                                                           
                                                                                
    NS_NEWXPCOM(inst, nsSecureBrowserUIImpl);                                          
    if (NULL == inst) {                                                         
        rv = NS_ERROR_OUT_OF_MEMORY;                                            
        return rv;                                                              
    }                                                                           
    NS_ADDREF(inst);                                                            
    rv = inst->QueryInterface(aIID, aResult);                                   
    NS_RELEASE(inst);                                                           
                                                                                
    return rv;                                                                  
}                                                                               

nsSecureBrowserUIImpl::nsSecureBrowserUIImpl()
{
	NS_INIT_REFCNT();

	mIsSecureDocument = mMixContentAlertShown = mIsDocumentBroken = PR_FALSE;
    mLastPSMStatus    = nsnull;
    mHost             = nsnull;
    mSecurityButton   = nsnull;
}

nsSecureBrowserUIImpl::~nsSecureBrowserUIImpl()
{
    PR_FREEIF(mLastPSMStatus);
    PR_FREEIF(mHost);
}

NS_IMPL_ISUPPORTS2(nsSecureBrowserUIImpl, nsIDocumentLoaderObserver, nsSecureBrowserUI); 


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
			mSecurityButton = button;
            mWindow         = window;

			docShell->GetDocLoaderObserver(getter_AddRefs(mOldWebShellObserver));
	        docShell->SetDocLoaderObserver(this);
        
            nsresult rv = nsServiceManager::GetService( kPrefCID, 
                                                        NS_GET_IID(nsIPref), 
                                                        getter_AddRefs(mPref));  


            if (NS_FAILED(rv)) return rv;

            NS_WITH_SERVICE(nsIStringBundleService, service, kCStringBundleServiceCID, &rv);
            if (NS_FAILED(rv)) return rv;
            
            nsILocale* locale = nsnull;
            rv = service->CreateBundle(STRING_BUNDLE_URL, locale, getter_AddRefs(mStringBundle));
            if (NS_FAILED(rv)) return rv;
        }
	}
	return NS_OK;
}


NS_IMETHODIMP
nsSecureBrowserUIImpl::DisplayPageInfoUI()
{
    nsresult res;
    NS_WITH_SERVICE(nsIPSMComponent, psm, PSM_COMPONENT_PROGID, &res);
	if (NS_FAILED(res)) 
		return res;

    return psm->DisplaySecurityAdvisor(mLastPSMStatus, mHost);
}



NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStartDocumentLoad(nsIDocumentLoader* aLoader, 
                                       nsIURI* aURL, 
                                       const char* aCommand)
{
	nsresult res;
    
    PR_FREEIF(mLastPSMStatus); mLastPSMStatus = nsnull;
    PR_FREEIF(mHost); mHost = nsnull;
    
    aURL->GetHost(&mHost);

    mIsSecureDocument = mMixContentAlertShown = mIsDocumentBroken = PR_FALSE;

    if (!aURL || !aLoader || !mSecurityButton || !mPref)
		return NS_ERROR_NULL_POINTER;	

    // lets call the old webshell observer.
	if (mOldWebShellObserver)
    {
	    res = mOldWebShellObserver->OnStartDocumentLoad(aLoader, aURL, aCommand);
        if (NS_FAILED(res)) return res;
    }

	//  Check to see if the URL that the current page was 
    //  loaded by https://.  
    PRBool isOldSchemeSecure;
	nsCOMPtr<nsIURI> uri;
	res = GetURIFromDocumentLoader(aLoader, getter_AddRefs(uri));
    
    if (NS_FAILED(res)) 
		return res;
	
    //  passing false means we only care about https
    res = IsSecureUrl(PR_FALSE, uri, &isOldSchemeSecure);

	if (NS_FAILED(res)) 
		return res;
    
    //    check to see if the new url to load is a 
    //    secure url.

    PRBool isNewSchemeSecure; 
	res	= IsSecureUrl(PR_FALSE, aURL, &isNewSchemeSecure);
	if (NS_FAILED(res)) 
		return res;
    
    PRBool boolpref;
    // Check to see if we are going from a secure page to and insecure page
	if ( !isNewSchemeSecure && isOldSchemeSecure)
	{
		mSecurityButton->RemoveAttribute( "level" );
	
        if ((mPref->GetBoolPref(LEAVE_SITE_PREF, &boolpref) != 0))
			boolpref = PR_TRUE;
		
		if (boolpref) 
		{
			NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &res);
			if (NS_FAILED(res)) 
				return res;
            
            nsAutoString windowTitle, message, dontShowAgain;
            
            GetBundleString("Title", windowTitle);
            GetBundleString("LeaveSiteMessage", message);
            GetBundleString("DontShowAgain", dontShowAgain);

            PRBool outCheckValue = PR_TRUE;
			dialog->AlertCheck(mWindow, 
                               windowTitle.GetUnicode(), 
                               message.GetUnicode(), 
                               dontShowAgain.GetUnicode(), 
                               &outCheckValue);
            
            if (!outCheckValue)
            {
                mPref->SetBoolPref(LEAVE_SITE_PREF, PR_FALSE);
		        NS_WITH_SERVICE(nsIPSMComponent, psm, PSM_COMPONENT_PROGID, &res);
            	if (NS_FAILED(res)) 
		            return res;
                psm->PassPrefs();
            }
        }
	}
	// check to see if we are going from an insecure page to a secure one.
	else if (isNewSchemeSecure && !isOldSchemeSecure)
	{
    
		if ((mPref->GetBoolPref(ENTER_SITE_PREF, &boolpref) != 0))
			boolpref = PR_TRUE;
		
		if (boolpref) 
		{
            NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &res);
			if (NS_FAILED(res)) 
				return res;
            
            nsAutoString windowTitle, message, dontShowAgain;
            
            GetBundleString("Title", windowTitle);
            GetBundleString("EnterSiteMessage", message);
            GetBundleString("DontShowAgain", dontShowAgain);

            PRBool outCheckValue = PR_TRUE;
			dialog->AlertCheck(mWindow, 
                               windowTitle.GetUnicode(), 
                               message.GetUnicode(), 
                               dontShowAgain.GetUnicode(), 
                               &outCheckValue);
            
            if (!outCheckValue)
            {
                mPref->SetBoolPref(ENTER_SITE_PREF, PR_FALSE);
                NS_WITH_SERVICE(nsIPSMComponent, psm, PSM_COMPONENT_PROGID, &res);
            	if (NS_FAILED(res)) 
		            return res;
                psm->PassPrefs();
            }
		}
	}

	mIsSecureDocument = isNewSchemeSecure;

    return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnEndDocumentLoad(nsIDocumentLoader* aLoader, 
                                     nsIChannel* channel, 
                                     nsresult aStatus)
{
	nsresult rv;
	
	if (mOldWebShellObserver)
	{
		rv = mOldWebShellObserver->OnEndDocumentLoad(aLoader, channel, aStatus);
	}
  
    if (! mIsSecureDocument)
        return rv;
    
    if (!mSecurityButton || !channel || !mIsSecureDocument)
        return NS_ERROR_NULL_POINTER;
    
    // check for an error from the above OnEndDocumentLoad().
    if (NS_FAILED(rv)) 
    {
         mIsDocumentBroken = PR_TRUE;
         mSecurityButton->SetAttribute( "level", nsString("broken") );
         return rv;
    }

    if (NS_SUCCEEDED(aStatus) && !mIsDocumentBroken)
    {
        // qi for the psm information about this channel load.
        nsCOMPtr<nsISupports> info;
	    channel->GetSecurityInfo(getter_AddRefs(info));
	    nsCOMPtr<nsIPSMSocketInfo> psmInfo = do_QueryInterface(info, &rv);
        if ( psmInfo )
        {
            // Everything looks okay.  Lets stash the picked status.
            PR_FREEIF(mLastPSMStatus);
            rv = psmInfo->GetPickledStatus(&mLastPSMStatus);
    
            if (NS_SUCCEEDED(rv))
            {
                return mSecurityButton->SetAttribute( "level", nsString("high") );
            }
	    }
    }
    return mSecurityButton->SetAttribute( "level", nsString("broken") );
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStartURLLoad(nsIDocumentLoader* loader, 
                                  nsIChannel* channel)
{
    nsresult rv; 

	if (mOldWebShellObserver)
	{
		rv = mOldWebShellObserver->OnStartURLLoad(loader, channel);
	}
	
    if (! mIsSecureDocument)
        return rv;

    if (!channel || !loader || !mSecurityButton)
	    return NS_ERROR_NULL_POINTER;

    // check for an error from the above OnStartURLLoad().
    if (NS_FAILED(rv)) 
    {
         mIsDocumentBroken = PR_TRUE;
         mSecurityButton->SetAttribute( "level", nsString("broken") );
         return rv;
    }

    // check to see if the URL that we are about to load 
    // is a secure.  We do this by checking the scheme
	PRBool secure;
	nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
	if (NS_FAILED(rv))
	    return rv;

    rv = IsSecureUrl(PR_TRUE, uri, &secure);
    if (NS_FAILED(rv))
	    return rv;

    if (!secure)
	{
		mIsDocumentBroken = PR_TRUE;
        mSecurityButton->SetAttribute( "level", nsString("broken") );

        // if we were going to block unsecure links, this is where
        // we would try to do it:
        //		nsCOMPtr<nsIURI> uri;
        //		channel->GetURI(getter_AddRefs(uri));
        //		uri->SetSpec("chrome://navigator/skin/insecureLink.gif");  //fix

		nsresult res;  

		if (!mPref) return NS_ERROR_NULL_POINTER;
	
        PRBool boolpref;
		if ((mPref->GetBoolPref(MIXEDCONTENT_PREF, &boolpref) != 0))
			boolpref = PR_TRUE;

		if (boolpref && !mMixContentAlertShown) 
		{
			NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &res);
			if (NS_FAILED(res)) 
				return res;
            
            nsAutoString windowTitle, message, dontShowAgain;

            GetBundleString("Title", windowTitle);
            GetBundleString("MixedContentMessage", message);
            GetBundleString("DontShowAgain", dontShowAgain);

            PRBool outCheckValue = PR_TRUE;
			
            dialog->AlertCheck(mWindow, 
                               windowTitle.GetUnicode(), 
                               message.GetUnicode(), 
                               dontShowAgain.GetUnicode(), 
                               &outCheckValue);
            
            if (!outCheckValue)
            {
                mPref->SetBoolPref(MIXEDCONTENT_PREF, PR_FALSE);
                NS_WITH_SERVICE(nsIPSMComponent, psm, PSM_COMPONENT_PROGID, &res);
            	if (NS_FAILED(res)) 
		            return res;
                psm->PassPrefs();
            }
            
            
            mMixContentAlertShown = PR_TRUE;
		}
	}
    return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnProgressURLLoad(nsIDocumentLoader* loader, 
                                     nsIChannel* channel, 
                                     PRUint32 aProgress, 
                                     PRUint32 aProgressMax)
{
	if (mOldWebShellObserver)
	{
		return mOldWebShellObserver->OnProgressURLLoad(loader, channel, aProgress, aProgressMax);
	}

	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStatusURLLoad(nsIDocumentLoader* loader, 
                                   nsIChannel* channel, 
                                   nsString& aMsg)
{
	if (mOldWebShellObserver)
	{
		return mOldWebShellObserver->OnStatusURLLoad(loader, channel, aMsg);
	}
	return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsSecureBrowserUIImpl::OnEndURLLoad(nsIDocumentLoader* loader, 
                                nsIChannel* channel, 
                                nsresult aStatus)
{
    nsresult rv;
    if (mOldWebShellObserver)
	{
		rv = mOldWebShellObserver->OnEndURLLoad(loader, channel, aStatus);
	}

    if (!mIsSecureDocument)
        return rv;
   
    if (!channel || !loader || !mSecurityButton)
		return NS_ERROR_NULL_POINTER;
    
    // check for an error from the above OnStartURLLoad().
    if (NS_FAILED(rv)) 
    {
        mIsDocumentBroken = PR_TRUE;
        mSecurityButton->SetAttribute( "level", nsString("broken") );
        return rv;
    }

    if (NS_SUCCEEDED(aStatus) && !mIsDocumentBroken)
    {
        nsCOMPtr<nsISupports> info;
	    channel->GetSecurityInfo(getter_AddRefs(info));
	    nsCOMPtr<nsIPSMSocketInfo> psmInfo = do_QueryInterface(info, &rv);
        
        // qi for the psm information about this channel load.
        if ( psmInfo )
        {
            return NS_OK;    
	    }
    }

    mSecurityButton->SetAttribute( "level", nsString("broken") );
	mIsDocumentBroken = PR_TRUE;
	
	return NS_OK;
}

// fileSecure flag determines if we should include file: and other local protocols.
nsresult
nsSecureBrowserUIImpl::IsSecureUrl(PRBool fileSecure, nsIURI* aURL, PRBool* value)
{
	*value = PR_FALSE;

	if (!aURL)
		return NS_ERROR_NULL_POINTER;

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


nsresult 
nsSecureBrowserUIImpl::GetURIFromDocumentLoader(nsIDocumentLoader* aLoader, nsIURI** uri)
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


void nsSecureBrowserUIImpl::GetBundleString(const nsString& name, nsString &outString)
{
  if (mStringBundle && name.Length() > 0)
  {
    PRUnichar *ptrv = nsnull;
    if (NS_SUCCEEDED(mStringBundle->GetStringFromName(name.GetUnicode(), &ptrv)))
      outString = ptrv;
    else
      outString = "";
    
    nsAllocator::Free(ptrv);
  }
  else
  {
    outString = "";
  }
}
