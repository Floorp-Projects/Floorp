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
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

#include "nspr.h"

#include "nsP3PDefines.h"
#include "nsP3PUI.h"
#include "nsP3PLogging.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#include "nsIObserverService.h"
#include "nsIDocumentLoader.h"
#include "nsCURILoader.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIDocumentViewer.h"
#include "nsCURILoader.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIContent.h"
#include "nsIWebProgress.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIPrompt.h"

#include "prmem.h"


NS_IMPL_ISUPPORTS4( nsP3PUI, nsIP3PUI,
                             nsIP3PCUI,
                             nsIWebProgressListener,
                             nsISupportsWeakReference);

nsP3PUI::nsP3PUI()
  : mPrivacyStatus( P3P_STATUS_NO_P3P ),
    mFirstInitialLoad( PR_TRUE ) {
  NS_INIT_ISUPPORTS( );
}

nsP3PUI::~nsP3PUI()
{
}

NS_IMETHODIMP
nsP3PUI::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;

  nsP3PUI * inst;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUI::Create()\n");
#endif
  if (nsnull != aResult) {
    *aResult = NULL;
    if (nsnull == aOuter) {
      NS_NEWXPCOM(inst, nsP3PUI);
      if (nsnull != inst) {
        NS_ADDREF(inst);
        rv = inst->QueryInterface(aIID, aResult);
        NS_RELEASE(inst);
      } else {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    } else {
      rv = NS_ERROR_NO_AGGREGATION;
    }
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }

  return rv;
}

//
// nsIP3PUI methods
//

NS_IMETHODIMP
nsP3PUI::Init(nsIDOMWindow *window, nsIDOMElement *button)
{
  nsresult rv;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUI::Init(%x, %x)\n", window, button);
#endif

  mPrivacyButton = button;
  mWindow        = window;

  mP3PService = do_GetService( NS_P3PSERVICE_CONTRACTID, &rv );
  if (NS_SUCCEEDED(rv)) {

    // nsP3PService created gP3PLogModule, so we know it is valid.

    PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUI:  Initializing instance %x\n", this) );

    PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUI:  Find the DocShellTreeItem for the DOMWindow.\n") );
    rv = DOMWindowToDocShellTreeItem( );
    if (NS_SUCCEEDED(rv)) {

      PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUI:  Get the P3PUIService.\n") );
      mP3PUIService = do_GetService( NS_P3PUISERVICE_CONTRACTID, &rv );
      if (NS_SUCCEEDED(rv)) {
        PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUI:  Register this P3PUI instance with the P3PUIService.\n") );
        rv = mP3PUIService->RegisterUIInstance(mDocShellTreeItem, NS_STATIC_CAST(nsIP3PCUI *, this));

        if (NS_SUCCEEDED(rv)) {
          PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUI:  Get the Preference service.\n") );
          mPrefServ = do_GetService( NS_PREF_CONTRACTID, &rv );
          if (NS_SUCCEEDED(rv)) {

            // Hook up to the webprogress notifications.
            PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUI:  Add this P3PUI as a WebProgress listener.\n") );

            nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(window, &rv);
            if (NS_SUCCEEDED(rv)) {
              nsCOMPtr<nsIDocShell> docShell;
              rv = sgo->GetDocShell(getter_AddRefs(docShell));
              if (NS_SUCCEEDED(rv)) {
                if (docShell)  {
                  nsCOMPtr<nsIWebProgress> wp = do_GetInterface(docShell, &rv);
                  if (NS_SUCCEEDED(rv)) {
                    rv = wp->AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener*, this));

                    if (NS_SUCCEEDED(rv)) {
                      // Let the P3P Service know a page is starting to load.
                      nsCOMPtr<nsIWebNavigation> webNavigation = do_QueryInterface(docShell, &rv);
                      if (NS_SUCCEEDED( rv )) {
                        PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUI:  Tell the P3PService that a page is loading.\n") );
                        nsCOMPtr<nsIURI> pURI;
                        webNavigation->GetCurrentURI(getter_AddRefs(pURI));

                        mCurrentURI = pURI;

                        mP3PService->LoadingObject(pURI, mDocShellTreeItem, mDocShellTreeItem, PR_TRUE);
                      }
                    }
                  }
                } else {
                  rv = NS_ERROR_NULL_POINTER;
                }
              }
            }
          }
        }
      }
    }

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule, PR_LOG_ERROR, ("P3PUI:  Initialization failed:  %X.\n", rv) );
    } else {
      PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUI:  Initialization successful.\n" ) );
    }
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUI::Close()
{
  nsresult rv;

  PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUI:  Closing instance %X\n", this) );

  // Deregister this Browser UI instance from the UI Service.
  if (mP3PUIService) {
    mP3PUIService->DeregisterUIInstance(mDocShellTreeItem, NS_STATIC_CAST(nsIP3PCUI *, this));
  }

  // Remove this Browser UI instance from being a WebProgress listener.
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(mWindow, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDocShell> docShell;
    rv = sgo->GetDocShell(getter_AddRefs(docShell));
    if (NS_SUCCEEDED(rv)) {
      if (docShell)  {
        nsCOMPtr<nsIWebProgress> wp = do_GetInterface(docShell, &rv);
        if (NS_SUCCEEDED(rv)) {
          rv = wp->RemoveProgressListener(NS_STATIC_CAST(nsIWebProgressListener*, this));
        }
      } else {
        rv = NS_ERROR_NULL_POINTER;
      }
    }
  }

  // Tell the P3P service the window is closing
  mP3PService->ClosingBrowserWindow( mDocShellTreeItem );

  return rv;
}

NS_IMETHODIMP
nsP3PUI::GetPrivacyStatus( PRInt32 * aStatus )
{
#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUI::GetPrivacyStatus()  mPrivacyStatus is %d.\n", mPrivacyStatus);
#endif

  *aStatus = mPrivacyStatus;

  return NS_OK;
}

NS_IMETHODIMP
nsP3PUI::GetCurrentURI( char ** aResult )
{
  nsresult rv = NS_OK;

  if (mCurrentURI) {
    mCurrentURI->GetSpec(aResult);
  } else {
    *aResult = nsnull;
    rv = NS_ERROR_NULL_POINTER;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUI::GetPrivacyInfo( nsIRDFDataSource **aDataSource )
{
  nsresult rv = NS_OK;

  rv = mP3PService->GetPrivacyInfo( mDocShellTreeItem,
                                    aDataSource );

  return rv;
}

//
// nsIP3PCUI methods
//

NS_IMETHODIMP
nsP3PUI::MarkNoP3P()
{
  nsresult rv = NS_OK;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUI::MarkNoP3P()\n");
#endif

  if ( mPrivacyButton ) {
    if (mPrivacyStatus != P3P_STATUS_NO_P3P) {
      mPrivacyStatus = P3P_STATUS_NO_P3P;
      rv = mPrivacyButton->RemoveAttribute( NS_LITERAL_STRING("level") );

      if (NS_SUCCEEDED(rv)) {
        rv = mPrivacyButton->RemoveAttribute( NS_LITERAL_STRING("tooltiptext") );
      }
    }
  } else {
    rv = NS_ERROR_NOT_INITIALIZED;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUI::MarkNoPrivacy()
{
  nsresult rv = NS_OK;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUI::MarkNoPrivacy()\n");
#endif

  if ( mPrivacyButton ) {
    if (mPrivacyStatus != P3P_STATUS_NO_POLICY) {
      mPrivacyStatus = P3P_STATUS_NO_POLICY;
      rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("level"), NS_LITERAL_STRING("none") );

      if (NS_SUCCEEDED(rv)) {
        nsAutoString  sToolTip;
        mP3PService->GetLocaleString( "StatusNone", sToolTip );
        rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("tooltiptext"), sToolTip );
      }
    }
  } else {
    rv = NS_ERROR_NOT_INITIALIZED;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUI::MarkPrivate()
{
  nsresult rv = NS_OK;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUI::MarkPrivate()\n");
#endif

  if ( mPrivacyButton ) {
    if (mPrivacyStatus != P3P_STATUS_POLICY_MET) {
      mPrivacyStatus = P3P_STATUS_POLICY_MET;
      rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("level"), NS_LITERAL_STRING("yes") );

      if (NS_SUCCEEDED(rv)) {
        nsAutoString  sToolTip;
        mP3PService->GetLocaleString( "StatusMet", sToolTip );
        rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("tooltiptext"), sToolTip );
      }
    }
  } else {
    rv = NS_ERROR_NOT_INITIALIZED;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUI::MarkNotPrivate()
{
  nsresult rv = NS_OK;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUI::MarkNotPrivate()\n");
#endif

  if ( mPrivacyButton ) {
    if (mPrivacyStatus != P3P_STATUS_POLICY_NOT_MET) {
      mPrivacyStatus = P3P_STATUS_POLICY_NOT_MET;
      rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("level"), NS_LITERAL_STRING("no") );

      if (NS_SUCCEEDED(rv)) {
        nsAutoString  sToolTip;
        mP3PService->GetLocaleString( "StatusNotMet", sToolTip );
        rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("tooltiptext"), sToolTip );
      }

      mP3PUIService->WarningNotPrivate( mDocShellTreeItem );
    }
  } else {
    rv = NS_ERROR_NOT_INITIALIZED;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUI::MarkPartialPrivacy()
{
  nsresult rv = NS_OK;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUI::MarkPartialPrivacy()\n");
#endif

  if ( mPrivacyButton ) {
    if (mPrivacyStatus != P3P_STATUS_PARTIAL_POLICY) {
      mPrivacyStatus = P3P_STATUS_PARTIAL_POLICY;
      rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("level"), NS_LITERAL_STRING("partial") );

      if (NS_SUCCEEDED(rv)) {
        nsAutoString  sToolTip;
        mP3PService->GetLocaleString( "StatusPartial", sToolTip );
        rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("tooltiptext"), sToolTip );
      }

      mP3PUIService->WarningPartialPrivacy( mDocShellTreeItem );
    }
  } else {
    rv = NS_ERROR_NOT_INITIALIZED;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUI::MarkPrivacyBroken()
{
  nsresult rv = NS_OK;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUI::MarkPrivacyBroken()\n");
#endif

  if ( mPrivacyButton ) {
    if (mPrivacyStatus != P3P_STATUS_BAD_POLICY) {
      mPrivacyStatus = P3P_STATUS_BAD_POLICY;
      rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("level"), NS_LITERAL_STRING("broken") );

      if (NS_SUCCEEDED(rv)) {
        nsAutoString  sToolTip;
        mP3PService->GetLocaleString( "StatusBroken", sToolTip );
        rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("tooltiptext"), sToolTip );
      }
    }
  } else {
    rv = NS_ERROR_NOT_INITIALIZED;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUI::MarkInProgress()
{
  nsresult rv = NS_OK;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUI::MarkInProgress()\n");
#endif

  if ( mPrivacyButton ) {
    if (mPrivacyStatus != P3P_STATUS_IN_PROGRESS) {
      mPrivacyStatus = P3P_STATUS_IN_PROGRESS;
      rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("level"), NS_LITERAL_STRING("inprogress") );

      if (NS_SUCCEEDED(rv)) {
        nsAutoString  sToolTip;
        mP3PService->GetLocaleString( "StatusInProg", sToolTip );
        rv = mPrivacyButton->SetAttribute( NS_LITERAL_STRING("tooltiptext"), sToolTip );
      }
    }
  } else {
    rv = NS_ERROR_NOT_INITIALIZED;
  }

  return rv;
}

//
// nsIWebProgressListener methods
//

NS_IMETHODIMP
nsP3PUI::OnStateChange(nsIWebProgress *aWebProgress,
                              nsIRequest *aRequest,
                              PRInt32 aStateFlags,
                              PRUint32 aStatus)
{
  nsresult  rv;


  nsCOMPtr<nsIChannel> pChannel = do_QueryInterface(aRequest);
  if (pChannel) {

    nsCOMPtr<nsIURI> pURI;
    pChannel->GetURI(getter_AddRefs(pURI));
    if (pURI) {

      if (mPrivacyButton &&
          (aStateFlags & nsIWebProgressListener::STATE_START) &&
          (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK)) {

        // Starting to load a webpage.  Save the URI and reset the privacy icon.
        mFirstInitialLoad = PR_FALSE;

        mCurrentURI = pURI;

        MarkNoP3P();
      }

      if (aStateFlags & nsIWebProgressListener::STATE_START) {

        nsCOMPtr<nsIDocumentLoader> pDocLoader = do_QueryInterface(aWebProgress);
        if (pDocLoader) {

          nsCOMPtr<nsISupports> pContainer;
          pDocLoader->GetContainer(getter_AddRefs(pContainer));
          if (pContainer) {

            nsCOMPtr<nsIDocShellTreeItem> pDocShellTreeItem = do_QueryInterface(pContainer);
            if (pDocShellTreeItem) {

              // Check if this is a redirection
              nsLoadFlags loadFlags = 0;

              rv = aRequest->GetLoadFlags(&loadFlags);
              if (NS_SUCCEEDED( rv )) {
              
                if (loadFlags & nsIChannel::LOAD_REPLACE) {

                  // Redirection, get the original URI and modify the PrivacyResult object we created
                  nsCOMPtr<nsIURI>  pOriginalURI;
                  rv = pChannel->GetOriginalURI( getter_AddRefs( pOriginalURI ) );
                  if (NS_SUCCEEDED( rv )) {

                    mP3PService->ModifyPrivacyResult( pOriginalURI,
                                                      pURI,
                                                      pDocShellTreeItem );
                  }
                }
                else {
                  // Let the P3P Service know an object is starting to load
                  if (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK) {

                    // Let the P3P Service know this is the initial load
                    mP3PService->LoadingObject(pURI, mDocShellTreeItem, pDocShellTreeItem, PR_TRUE);
                  }
                  else {

                    // Let the P3P Service know this is not the initial load
                    mP3PService->LoadingObject(pURI, mDocShellTreeItem, pDocShellTreeItem, PR_FALSE);
                  }
                }
              }
            }
          }
        }
      }

      else if (aStateFlags & nsIWebProgressListener::STATE_STOP) {

        if (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK) {

          mP3PService->DocumentComplete( mDocShellTreeItem );
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsP3PUI::OnProgressChange(nsIWebProgress *aWebProgress,
                                 nsIRequest *aRequest,
                                 PRInt32 aCurSelfProgress,
                                 PRInt32 aMaxSelfProgress,
                                 PRInt32 aCurTotalProgress,
                                 PRInt32 aMaxTotalProgress)
{
    return NS_OK;
}

NS_IMETHODIMP
nsP3PUI::OnLocationChange(nsIWebProgress *aWebProgress,
                                 nsIRequest *aRequest,
                                 nsIURI *aLocation)
{
#ifdef DEBUG_P3P
  char *pURI = nsnull;
  aLocation->GetSpec(&pURI);
  printf("P3P:  nsP3PUI::OnLocationChange(%s)\n", pURI);
  nsMemory::Free(pURI);
#endif

  if (mFirstInitialLoad) {
    // Initial state change hasn't been called (window just created) so
    // save the URI and modify the URI associated with the PrivacyResult
    // object
    mFirstInitialLoad = PR_FALSE;

    mP3PService->ModifyPrivacyResult( mCurrentURI,
                                      aLocation,
                                      mDocShellTreeItem );

    mCurrentURI = aLocation;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsP3PUI::OnStatusChange(nsIWebProgress *aWebProgress,
                               nsIRequest *aRequest,
                               nsresult aStatus,
                               const PRUnichar *aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsP3PUI::OnSecurityChange(nsIWebProgress *aWebProgress,
                               nsIRequest *aRequest,
                               PRInt32 aState)
{
  return NS_OK;
}

//
// nsP3PUI methods
//

NS_METHOD
nsP3PUI::DOMWindowToDocShellTreeItem( )
{
  nsresult  rv;

  // Find the nsIDocShellTreeItem for the nsIDOMWindow.
  nsCOMPtr<nsIScriptGlobalObject> pScriptGlobalObject = do_QueryInterface(mWindow, &rv);
  if (NS_SUCCEEDED(rv)) {

    nsCOMPtr<nsIScriptGlobalObjectOwner> pScriptGlobalObjectOwner;
    rv = pScriptGlobalObject->GetGlobalObjectOwner(getter_AddRefs(pScriptGlobalObjectOwner));
    if (NS_SUCCEEDED(rv)) {
      if (pScriptGlobalObjectOwner != nsnull) {

        mDocShellTreeItem = do_QueryInterface(pScriptGlobalObjectOwner, &rv);

      } else {
        rv = NS_ERROR_NULL_POINTER;
      }
    }
  }

  return rv;
}

