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

#include "nsP3PDefines.h"

#include "nsP3PObserverLayout.h"
#include "nsP3PLogging.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsObserverService.h"
#include "nsIPresShell.h"

#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"

#include "nsCURILoader.h"
#include "nsIURILoader.h"
#include "nsILoadGroup.h"
#include "nsIHttpChannel.h"
#include "nsISimpleEnumerator.h"

#include "nsString.h"
#include "nsXPIDLString.h"


// ****************************************************************************
// nsP3PObserverLayout Implementation routines
// ****************************************************************************

static NS_DEFINE_CID( kP3PServiceCID, NS_P3PSERVICE_CID );

// P3P Observer Layout nsISupports
NS_IMPL_ISUPPORTS2( nsP3PObserverLayout, nsIObserver,
                                         nsISupportsWeakReference );

// P3P Observer Layout creation routine
NS_METHOD
NS_NewP3PObserverLayout( nsIObserver **aObserverLayout ) {

  nsresult             rv;

  nsP3PObserverLayout *pNewObserverLayout = nsnull;


#ifdef DEBUG_P3P
  printf("P3P:  ObserverLayout initializing.\n");
#endif

  NS_ENSURE_ARG_POINTER( aObserverLayout );

  *aObserverLayout = nsnull;

  pNewObserverLayout = new nsP3PObserverLayout( );

  if (pNewObserverLayout) {
    NS_ADDREF( pNewObserverLayout );

    rv = pNewObserverLayout->Init( );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewObserverLayout->QueryInterface( NS_GET_IID( nsIObserver ),
                                      (void **)aObserverLayout );
    }

    NS_RELEASE( pNewObserverLayout );
  }
  else {
    NS_ASSERTION( 0, "P3P:  ObserverLayout unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Observer Layout Constructor
nsP3PObserverLayout::nsP3PObserverLayout( )
  : mP3PIsEnabled( PR_FALSE ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Observer Layout Destructor
nsP3PObserverLayout::~nsP3PObserverLayout( ) {

  if (mObserverService) {
    mObserverService->RemoveObserver( this, NS_PRESSHELL_REFLOW_TOPIC);
  }
}

// P3P Observer Layout: Init
//
// Function:  Initialization routine for the Layout observer.
//
// Parms:     None
//
NS_METHOD
nsP3PObserverLayout::Init( ) {

  nsresult  rv;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PObserverLayout:  Init, initializing.\n") );

  // Get the Observer service
  mObserverService = do_GetService( NS_OBSERVERSERVICE_CONTRACTID,
                                   &rv );

  if (NS_SUCCEEDED( rv )) {
    // Register to observe Reflows
    rv = mObserverService->AddObserver( this, NS_PRESSHELL_REFLOW_TOPIC, PR_FALSE)

    if (NS_FAILED( rv )) {
#ifdef DEBUG_P3P
      printf( "P3P:  Unable to register with Observer Service for Layout notification topic: %X\n", rv );
#endif

      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PObserverLayout:  Init, mObserverService->AddObserver failed - %X.\n", rv) );
    }
  }
  else {
#ifdef DEBUG_P3P
    printf( "P3P:  Unable to obtain Observer Service: %X\n", rv );
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PObserverLayout:  Init, do_GetService for Observer service failed - %X.\n", rv) );
  }

  if (NS_SUCCEEDED( rv )) {
    mURILoader = do_GetService( NS_URI_LOADER_CONTRACTID,
                               &rv );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PObserverLayout:  Init, do_GetService for URI Loader service failed - %X.\n", rv) );
    }
  }

  return rv;
}


// ****************************************************************************
// nsIObserver routines
// ****************************************************************************

// P3P Observer Layout: Observe
//
// Function:  Allows for processing before the specified type of layout processing begins.
//
// Parms:     1. In     The DocShell object associated with the layout
//            2. In     The observer topic
//            3. In     The type of layout being done
//
NS_IMETHODIMP
nsP3PObserverLayout::Observe( nsISupports      *aSubject,
                              const PRUnichar  *aTopic,
                              const PRUnichar  *aSomeData ) {

  nsresult                       rv = NS_OK;

  nsCOMPtr<nsIDocShell>          pDocShell;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItem;

  nsCOMPtr<nsIWebNavigation>     pWebNavigation;

  nsCOMPtr<nsIURI>               pURI;

  nsAutoString                   sRequestMethod;


  // Get the P3P service and check if it is enabled
  rv = GetP3PService( );

  if (NS_SUCCEEDED( rv ) && mP3PIsEnabled) {
#ifdef DEBUG_P3P
    { nsCString  csTopic, csData;

      csTopic.AssignWithConversion( aTopic ),
      csData.AssignWithConversion( aSomeData );
      printf( "P3P:  Observer invoked: Topic is %s, Data is %s\n", (const char *)csTopic, (const char *)csData );
    }
#endif

    if (nsCRT::strcmp( aSomeData, NS_PRESSHELL_INITIAL_REFLOW ) == 0) {
      // Initial reflow - start of layout for a page
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PObserverLayout:  Observe, notified of initial reflow.\n") );

      // Get the DocShell
      pDocShell = do_QueryInterface( aSubject,
                                    &rv );

      if (NS_SUCCEEDED( rv )) {
        // Get the DocShellTreeItem
        pDocShellTreeItem = do_QueryInterface( pDocShell,
                                              &rv );

        if (NS_SUCCEEDED( rv )) {
          pWebNavigation = do_QueryInterface( pDocShell,
                                             &rv );

          if (NS_SUCCEEDED( rv )) {
            // Get the URI
            rv = pWebNavigation->GetCurrentURI( getter_AddRefs( pURI ) );

            if (NS_SUCCEEDED( rv )) {
              // Get the request method used
              rv = GetRequestMethod( pDocShell,
                                     pURI,
                                     sRequestMethod );

              if (NS_SUCCEEDED( rv )) {
                // Check the privacy
                mP3PService->CheckPrivacy( sRequestMethod,
                                           pURI,
                                           pDocShellTreeItem,
                                           nsnull );
              }
            }
            else {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PObserverLayout:  Observe, pDocShell->GetCurrentURI failed - %X.\n", rv) );
            }
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PObserverLayout:  Observe, do_QueryInterface for WebNavigation failed - %X.\n", rv) );
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PObserverLayout:  Observe, do_QueryInterface for DocShellTreeItem failed - %X.\n", rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PObserverLayout:  Observe, do_QueryInterface for DocShell failed - %X.\n", rv) );
      }
    }
  }

  return rv;
}

// ****************************************************************************
// nsP3PObserverLayout routines
// ****************************************************************************

// P3P Observer Layout: GetRequestMethod
//
// Function:  Determines the request <METHOD> associated with the DocShell object.
//
// Parms:     1. In     The DocShell object
//            2. In     The URI object
//            3. Out    The request <METHOD>
//
NS_METHOD
nsP3PObserverLayout::GetRequestMethod( nsIDocShell  *aDocShell,
                                       nsIURI       *aURI,
                                       nsString&     aRequestMethod ) {

  nsresult                       rv;

  nsCOMPtr<nsILoadGroup>         pLoadGroup;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  PRBool                         bMoreRequests,
                                 bURIMatch;

  nsCOMPtr<nsIRequest>           pRequest;

  nsCOMPtr<nsIHttpChannel>       pHTTPChannel;

  nsCOMPtr<nsIURI>               pURI;


  // Get the load group associated with this DocShell
  rv = mURILoader->GetLoadGroupForContext( aDocShell,
                                           getter_AddRefs( pLoadGroup ) );

  if (NS_SUCCEEDED( rv ) && pLoadGroup) {
    // Get the channels in the load group
    rv = pLoadGroup->GetRequests( getter_AddRefs( pEnumerator ) );

    if (NS_SUCCEEDED( rv )) {
      // Loop through the channels looking for the channel that matches this DocShell
      bURIMatch = PR_FALSE;
      rv = pEnumerator->HasMoreElements(&bMoreRequests );

      while (NS_SUCCEEDED( rv ) && !bURIMatch && bMoreRequests) {
        rv = pEnumerator->GetNext( getter_AddRefs( pRequest ) );

        if (NS_SUCCEEDED( rv )) {
          pHTTPChannel = do_QueryInterface( pRequest );

          if (pHTTPChannel) {
            rv = pHTTPChannel->GetURI( getter_AddRefs( pURI ) );

            if (NS_SUCCEEDED( rv )) {
              // Compare the URIs
              rv = pURI->Equals( aURI,
                                &bURIMatch );

              if (bURIMatch) {
                // They match, so get the request method
                nsXPIDLCString requestMethod;
                rv = pHTTPChannel->GetRequestMethod( getter_Copies( requestMethod ) );

                if (NS_SUCCEEDED( rv )) {
                  aRequestMethod.AssignWithConversion(requestMethod);
                }
                else {
                  PR_LOG( gP3PLogModule,
                          PR_LOG_ERROR,
                          ("P3PObserverLayout:  GetRequestMethod, pHTTPChannel->GetRequestMethod failed - %X.\n", rv) );
                }
              }
            }
            else {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PObserverLayout:  GetRequestMethod, pHTTPChannel->GetURI failed - %X.\n", rv) );
            }
          }
        }

        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreRequests );
        }
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PObserverLayout:  GetRequestMethod, pLoadGroup->GetRequests failed - %X.\n", rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PObserverLayout:  GetRequestMethod, mURILoader->GetLoadGroupForContext failed - %X.\n", rv) );
  }

  return rv;
}

// P3P Observer Layout: GetP3PService
//
// Function:  Obtain the P3P service and determine if it is enabled.
//
// Parms:     None
//
NS_METHOD
nsP3PObserverLayout::GetP3PService( ) {

  nsresult  rv = NS_OK;


  if (!mP3PService) {
    mP3PService = do_GetService( NS_P3PSERVICE_CONTRACTID,
                                &rv );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PObserverLayout:  GetP3PService, do_GetService for P3P service failed - %X.\n", rv) );
    }
  }

  if (mP3PService) {
    rv = mP3PService->P3PIsEnabled(&mP3PIsEnabled );

    if (NS_SUCCEEDED( rv ) && !mP3PIsEnabled) {
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PObserverLayout:  GetP3PService, P3P Service is not enabled.\n") );
    }
    else if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PObserverLayout:  GetP3PService, mP3PService->P3PIsEnabled failed - %X.\n", rv) );
    }
  }

  return rv;
}
