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

#include "nspr.h"
#include "prlog.h"

#include "nsISecureBrowserUI.h"
#include "nsSecureBrowserUIImpl.h"
#include "nsIPSMComponent.h"
#include "nsPSMComponent.h"
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
#include "nsIWebProgress.h"
#include "nsIChannel.h"
#include "nsIPSMSocketInfo.h"

#include "nsIURI.h"

#include "prmem.h"

#include "nsINetSupportDialogService.h"
#include "nsIPrompt.h"
#include "nsICommonDialogs.h"
#include "nsIPref.h"

#include "nsIFormSubmitObserver.h"

static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kCommonDialogsCID,         NS_CommonDialog_CID );
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#define ENTER_SITE_PREF      "security.warn_entering_secure"
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"
#define INSECURE_SUBMIT_PREF "security.warn_submit_insecure"

#if defined(PR_LOGGING)
//
// Log module for nsSecureBroswerUI logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsSecureBroswerUI:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gSecureDocLog = nsnull;
#endif /* PR_LOGGING */

 
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

#if defined(PR_LOGGING)
  if (nsnull == gSecureDocLog) {
      gSecureDocLog = PR_NewLogModule("nsSecureBroswerUI");
  }
#endif /* PR_LOGGING */


	mIsSecureDocument = mMixContentAlertShown = mIsDocumentBroken = PR_FALSE;
    mLastPSMStatus    = nsnull;
    mCurrentURI       = nsnull;
    mSecurityButton   = nsnull;
}

nsSecureBrowserUIImpl::~nsSecureBrowserUIImpl()
{
    PR_FREEIF(mLastPSMStatus);
}

NS_IMPL_ISUPPORTS4(nsSecureBrowserUIImpl, 
                   nsSecureBrowserUI, 
                   nsIWebProgressListener, 
                   nsIFormSubmitObserver,
                   nsIObserver); 


NS_IMETHODIMP
nsSecureBrowserUIImpl::Init(nsIDOMWindow *window, nsIDOMElement *button)
{
    mSecurityButton = button;
    mWindow         = window;
    
    nsresult rv = nsServiceManager::GetService( kPrefCID, 
                                                NS_GET_IID(nsIPref), 
                                                getter_AddRefs(mPref));  
    if (NS_FAILED(rv)) return rv;
    
    NS_WITH_SERVICE(nsIStringBundleService, service, kCStringBundleServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsILocale* locale = nsnull;
    rv = service->CreateBundle(SECURITY_STRING_BUNDLE_URL, locale, getter_AddRefs(mStringBundle));
    if (NS_FAILED(rv)) return rv;

    // hook up to the form post notifications:
    nsIObserverService *svc = 0;
    rv = nsServiceManager::GetService(NS_OBSERVERSERVICE_PROGID, 
                                      NS_GET_IID(nsIObserverService), 
                                      (nsISupports**)&svc );
    if ( NS_SUCCEEDED( rv ) && svc )
    {
        nsString  topic; topic.AssignWithConversion(NS_FORMSUBMIT_SUBJECT);
        rv = svc->AddObserver( this, topic.GetUnicode());
        nsServiceManager::ReleaseService( NS_OBSERVERSERVICE_PROGID, svc );
    }

    // hook up to the webprogress notifications.
    nsCOMPtr<nsIDocShell> docShell;
	
    nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(window);
	if (!sgo) return NS_ERROR_NULL_POINTER;
    
    sgo->GetDocShell(getter_AddRefs(docShell));
    if (!docShell) return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIWebProgress> wp = do_QueryInterface(docShell);
    if (!wp) return NS_ERROR_NULL_POINTER;

    wp->AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener*,this));

    // Set up stuff the first time the window loads:
    docShell->GetCurrentURI(getter_AddRefs(mCurrentURI));

    return IsURLHTTPS(mCurrentURI, &mIsSecureDocument);
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::DisplayPageInfoUI()
{
    nsresult res;
    NS_WITH_SERVICE(nsIPSMComponent, psm, PSM_COMPONENT_PROGID, &res);
	if (NS_FAILED(res)) 
		return res;
    
    nsXPIDLCString temp;
    mCurrentURI->GetHost(getter_Copies(temp));
    return psm->DisplaySecurityAdvisor(mLastPSMStatus, temp);
}




NS_IMETHODIMP 
nsSecureBrowserUIImpl::Observe(nsISupports*, const PRUnichar*, const PRUnichar*) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsSecureBrowserUIImpl::Notify(nsIContent* formNode, nsIDOMWindow* window, nsIURI* actionURL)
{
    // Return NS_OK unless we want to prevent this form from submitting.
    if (!window || (mWindow.get() != window) || !actionURL) {
      return NS_OK;
    }
    PRBool okayToPost;
    nsresult res = CheckPost(actionURL, &okayToPost);

    // Return NS_OK unless we want to prevent this form from submitting.
    if (NS_SUCCEEDED(res) && okayToPost)
        return NS_OK;
    
    return NS_ERROR_FAILURE;
}

//  nsIWebProgressListener
NS_IMETHODIMP 
nsSecureBrowserUIImpl::OnProgressChange(nsIChannel* aChannel, 
                                        PRInt32 aCurSelfProgress, 
                                        PRInt32 aMaxSelfProgress, 
                                        PRInt32 aCurTotalProgress, 
                                        PRInt32 aMaxTotalProgress)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsSecureBrowserUIImpl::OnChildProgressChange(nsIChannel* aChannel, 
                                             PRInt32 aCurSelfProgress, 
                                             PRInt32 aMaxSelfProgress)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsSecureBrowserUIImpl::OnStatusChange(nsIChannel* aChannel, 
                                      PRInt32 aProgressStatusFlags)
{
    nsresult res;

    if (aChannel == nsnull || !mSecurityButton || !mPref)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIURI> loadingURI;
    aChannel->GetURI(getter_AddRefs(loadingURI));
    
#if defined(DEBUG)
        nsXPIDLCString temp;
        loadingURI->GetSpec(getter_Copies(temp));
        PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI:%p: OnStatusChange: %x :%s\n", this, aProgressStatusFlags,(const char*)temp));
#endif

    if (aProgressStatusFlags & nsIWebProgress::flag_net_start)
    {
        // starting to load a webpage
        PR_FREEIF(mLastPSMStatus); mLastPSMStatus = nsnull;

        mIsSecureDocument = mMixContentAlertShown = mIsDocumentBroken = PR_FALSE;
        
        res = CheckProtocolContextSwitch( loadingURI, mCurrentURI);    

    } 
    else if ((aProgressStatusFlags & nsIWebProgress::flag_net_stop)  && mIsSecureDocument)
    {
        if (!mIsDocumentBroken) // and status is okay  FIX
        {
            // qi for the psm information about this channel load.
            nsCOMPtr<nsISupports> info;
	        aChannel->GetSecurityInfo(getter_AddRefs(info));
	        nsCOMPtr<nsIPSMSocketInfo> psmInfo = do_QueryInterface(info);
            if ( psmInfo )
            {
                // Everything looks okay.  Lets stash the picked status.
                PR_FREEIF(mLastPSMStatus);
                res = psmInfo->GetPickledStatus(&mLastPSMStatus);
    
                if (NS_SUCCEEDED(res))
                {
                    PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI:%p: Icon set to lock\n", this));
                    res = mSecurityButton->SetAttribute( "level", nsString("high") );
                }
	        }
        }
        else
        {

            PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI:%p: Icon set to broken\n", this));
            mIsDocumentBroken = PR_TRUE;
            res = mSecurityButton->SetAttribute( "level", nsString("broken") );
        }
    }
    else // if (aProgressStatusFlags == nsIWebProgress::flag_net_redirecting)
    {
        res = NS_ERROR_NOT_IMPLEMENTED;
        // xxx need to fix.
    }

    return res;
}

NS_IMETHODIMP 
nsSecureBrowserUIImpl::OnChildStatusChange(nsIChannel* aChannel, PRInt32 aProgressStatusFlags)
{
    nsresult rv;
    if (aChannel == nsnull || !mSecurityButton || !mPref)
        return NS_ERROR_NULL_POINTER;
    
    nsCOMPtr<nsIURI> uri;
    rv = aChannel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;
    
#if defined(DEBUG)
        nsXPIDLCString temp;
        uri->GetSpec(getter_Copies(temp));
        PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI:%p: OnChildStatusChange: %x :%s\n", this, aProgressStatusFlags,(const char*)temp));
#endif
    
    // don't need to do anything more if the page is broken or not secure...

    if (!mIsSecureDocument || mIsDocumentBroken)
        return NS_OK;

    if (aProgressStatusFlags & nsIWebProgress::flag_net_start)
    {   // check to see if we are going to mix content.
        return CheckMixedContext(uri);
    }
    
    if (aProgressStatusFlags & nsIWebProgress::flag_net_stop)
    {
        if (1)  // FIX status from the flag...
        {
            nsCOMPtr<nsISupports> info;
	        aChannel->GetSecurityInfo(getter_AddRefs(info));
	        nsCOMPtr<nsIPSMSocketInfo> psmInfo = do_QueryInterface(info, &rv);
        
            // qi for the psm information about this channel load.
            if ( psmInfo )
            {
                return NS_OK;    
	        }
        }

        PR_LOG(gSecureDocLog, PR_LOG_DEBUG, ("SecureUI:%p: OnChildStatusChange - Icon set to broken\n", this));
        mSecurityButton->SetAttribute( "level", nsString("broken") );
	    mIsDocumentBroken = PR_TRUE;
    }    
    
    
    return NS_OK;
}

NS_IMETHODIMP 
nsSecureBrowserUIImpl::OnLocationChange(nsIURI* aLocation)
{
    mCurrentURI = aLocation;
    return NS_OK;
}


nsresult
nsSecureBrowserUIImpl::IsURLHTTPS(nsIURI* aURL, PRBool* value)
{
	*value = PR_FALSE;

	if (!aURL)
		return NS_OK;

    char* scheme;
	aURL->GetScheme(&scheme);

	if (scheme == nsnull)
		return NS_ERROR_NULL_POINTER;

    if ( PL_strncasecmp(scheme, "https",  5) == 0 )
		*value = PR_TRUE;
	
	nsAllocator::Free(scheme);
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

nsresult 
nsSecureBrowserUIImpl::CheckProtocolContextSwitch( nsIURI* newURI, nsIURI* oldURI)
{
    nsresult res;
    PRBool isNewSchemeSecure, isOldSchemeSecure, boolpref;

    res = IsURLHTTPS(oldURI, &isOldSchemeSecure);
	if (NS_FAILED(res)) 
		return res;
    res = IsURLHTTPS(newURI, &isNewSchemeSecure);
	if (NS_FAILED(res)) 
		return res;
    
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


nsresult
nsSecureBrowserUIImpl::CheckMixedContext(nsIURI* nextURI)
{
    PRBool secure;

    nsresult rv = IsURLHTTPS(nextURI, &secure);
    if (NS_FAILED(rv))
	    return rv;
    
    if (!secure  && mIsSecureDocument)
    {
        mIsDocumentBroken = PR_TRUE;
        mSecurityButton->SetAttribute( "level", nsString("broken") );

        if (!mPref) return NS_ERROR_NULL_POINTER;
	    
        PRBool boolpref;
	    if ((mPref->GetBoolPref(MIXEDCONTENT_PREF, &boolpref) != 0))
		    boolpref = PR_TRUE;

	    if (boolpref && !mMixContentAlertShown) 
	    {
		    NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
		    if (NS_FAILED(rv)) 
			    return rv;
        
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
                NS_WITH_SERVICE(nsIPSMComponent, psm, PSM_COMPONENT_PROGID, &rv);
                if (NS_FAILED(rv)) 
		            return rv;
                psm->PassPrefs();
            }
        
        
            mMixContentAlertShown = PR_TRUE;
        }
    }
    
    return NS_OK;
}

nsresult
nsSecureBrowserUIImpl::CheckPost(nsIURI *actionURL, PRBool *okayToPost)
{
    PRBool secure;

    nsresult rv = IsURLHTTPS(actionURL, &secure);
    if (NS_FAILED(rv))
	    return rv;
    
    if (!secure  && mIsSecureDocument)
    {
        PRBool boolpref;    

        // posting to a non https URL.
        if ((mPref->GetBoolPref(INSECURE_SUBMIT_PREF, &boolpref) != 0))
			boolpref = PR_TRUE;
		
		if (boolpref) 
		{
            NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
			if (NS_FAILED(rv)) 
				return rv;

            nsAutoString windowTitle, message, dontShowAgain;
            
            GetBundleString("Title", windowTitle);
            GetBundleString("PostToInsecure", message);
            GetBundleString("DontShowAgain", dontShowAgain);

            PRBool outCheckValue = PR_TRUE;
			dialog->ConfirmCheck(mWindow, 
                                 windowTitle.GetUnicode(), 
                                 message.GetUnicode(), 
                                 dontShowAgain.GetUnicode(), 
                                 &outCheckValue, 
                                 okayToPost);
            
            if (!outCheckValue)
            {
                mPref->SetBoolPref(INSECURE_SUBMIT_PREF, PR_FALSE);
                NS_WITH_SERVICE(nsIPSMComponent, psm, PSM_COMPONENT_PROGID, &rv);
            	if (NS_FAILED(rv)) 
		            return rv;
                psm->PassPrefs();
            }
        }
    }
    return NS_OK;
}



