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

#include "nsP3PObserverXML.h"
#include "nsP3PObserverUtils.h"
#include "nsP3PLogging.h"

#include "nsIServiceManager.h"
#include "nsIParser.h"

#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"
#include "nsIDocument.h"

#include "nsString.h"
#include "nsXPIDLString.h"


// ****************************************************************************
// nsP3PObserverXML Implementation routines
// ****************************************************************************

static NS_DEFINE_CID( kP3PServiceCID, NS_P3PSERVICE_CID );

// P3P Observer XML nsISupports
NS_IMPL_ISUPPORTS3( nsP3PObserverXML, nsIObserver,
                                      nsIElementObserver,
                                      nsISupportsWeakReference );

// P3P Observer XML creation routine
NS_METHOD
NS_NewP3PObserverXML( nsIObserver **aObserverXML ) {

  nsresult          rv;

  nsP3PObserverXML *pNewObserverXML = nsnull;


#ifdef DEBUG_P3P
  printf("P3P:  ObserverXML initializing.\n");
#endif

  NS_ENSURE_ARG_POINTER( aObserverXML );

  *aObserverXML = nsnull;

  pNewObserverXML = new nsP3PObserverXML( );

  if (pNewObserverXML) {
    NS_ADDREF( pNewObserverXML );

    rv = pNewObserverXML->Init( );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewObserverXML->QueryInterface( NS_GET_IID( nsIObserver ),
                                   (void **)aObserverXML );
    }

    NS_RELEASE( pNewObserverXML );
  }
  else {
    NS_ASSERTION( 0, "P3P:  ObserverXML unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Observer XML: Constructor
nsP3PObserverXML::nsP3PObserverXML( )
  : mP3PIsEnabled( PR_FALSE ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Observer XML: Destructor
nsP3PObserverXML::~nsP3PObserverXML( ) {

  if (mObserverService) {
    mObserverService->RemoveObserver( this,
                                      NS_LITERAL_STRING("text/xml").get() );
    mObserverService->RemoveObserver( this,
                                      NS_LITERAL_STRING("application/xml").get() );
    mObserverService->RemoveObserver( this,
                                      NS_LITERAL_STRING("application/xhtml+xml").get() );
  }
}

// P3P Observer XML: Init
//
// Function:  Initialization routine for the XML Tag observer.
//
// Parms:     None
//
NS_METHOD
nsP3PObserverXML::Init( ) {

  nsresult  rv;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PObserverXML:  Init, initializing.\n") );

  // Get the observer service
  mObserverService = do_GetService( "@mozilla.org/observer-service;1",
                                   &rv );

  if (NS_SUCCEEDED( rv )) {
    // Register to observer XML tags
    rv = mObserverService->AddObserver( this, "text/xml", PR_FALSE );
    if (NS_SUCCEEDED(rv)) {
      rv = mObserverService->AddObserver( this, "application/xml",PR_FALSE );
      if (NS_SUCCEEDED(rv)) {
        rv = mObserverService->AddObserver( this, "application/xhtml" PR_FALSE );
      }
    }

    if (NS_FAILED( rv )) {
#ifdef DEBUG_P3P
      printf( "P3P:  Unable to register with Observer Service for XML tag notification topic: %X\n", rv );
#endif

      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PObserverXML:  Init, mObserverService->AddObserver failed - %X.\n", rv) );
    }
  }
  else {
#ifdef DEBUG_P3P
    printf( "P3P:  Unable to obtain Observer Service: %X\n", rv );
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PObserverXML:  Init, do_GetService for Observer service failed - %X.\n", rv) );
  }

  return rv;
}


// ****************************************************************************
// nsIObserver routines
// ****************************************************************************

// P3P Observer XML: Observe
//
// Function:  Not used, only here to satisfy the interface.
//
NS_IMETHODIMP
nsP3PObserverXML::Observe( nsISupports      *aSubject,
                           const PRUnichar  *aTopic,
                           const PRUnichar  *aSomeData ) {

  return NS_OK;
}

// ****************************************************************************
// nsIElementObserver routines
// ****************************************************************************

// P3P Observer XML: GetTagNameAt
//
// Function:  Returns the XML tags that this observer wants to be notified about.
//
// Parms:     1. In     The index or count of this call
//
NS_IMETHODIMP_(const char *)
nsP3PObserverXML::GetTagNameAt( PRUint32   aTagIndex ) {

  // Return the tag(s) to be observed
  return gObserveElements[aTagIndex];
}

// P3P Observer XML: Notify
//
// Function:  Allows for processing based upon the presence of an XML tag.
//
// Parms:     1. In     The Document object
//            2. In     The XML tag found
//            3. In     The number of attributes associated with the XML tag
//            4. In     The attribute names
//            5. In     The attribute values
//
NS_IMETHODIMP
nsP3PObserverXML::Notify( PRUint32          aDocumentID,
                          eHTMLTags         aTag,
                          PRUint32          aNumOfAttributes,
                          const PRUnichar  *aNameArray[],
                          const PRUnichar  *aValueArray[] ) {

  nsresult                       rv = NS_OK;

  nsCOMPtr<nsIDocument>          pDocument;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItem;

  nsIURI                        *pURI              = nsnull;


  rv = GetP3PService( );

  if (NS_SUCCEEDED( rv ) && mP3PIsEnabled) {
#ifdef DEBUG_P3P
    { if (aTag == eHTMLTag_link)
        printf( "P3P:  ObserverXML #1 has been notified of LINK tag.\n" );
      else
        printf( "P3P:  ObserverXML #1 has been notified of UNKNOWN tag.\n" );
    }
#endif

    if (aTag == eHTMLTag_link) {
      pDocument = do_QueryInterface((nsISupports *)aDocumentID,
                                    &rv );

      if (NS_SUCCEEDED( rv )) {
        pDocShellTreeItem = do_QueryInterface( pDocument,
                                              &rv );

        if (NS_SUCCEEDED( rv )) {
          pDocument->GetDocumentURL( &pURI );

          if (pURI) {
            rv = nsP3PObserverUtils::ExamineLINKTag( pURI,
                                                     aNumOfAttributes,
                                                     aNameArray,
                                                     aValueArray,
                                                     pDocShellTreeItem,
                                                     mP3PService );
            NS_RELEASE( pURI );
          }
        }
      }
    }
  }

  return rv;
}

// P3P Observer XML: Notify
//
// Function:  Allows for processing based upon the presence of an XML tag.
//
// Parms:     1. In     The Document object
//            2. In     The XML tag found
//            3. In     The number of attributes associated with the XML tag
//            4. In     The attribute names
//            5. In     The attribute values
//
NS_IMETHODIMP
nsP3PObserverXML::Notify( PRUint32          aDocumentID,
                          const PRUnichar  *aTag,
                          PRUint32          aNumOfAttributes,
                          const PRUnichar  *aNameArray[],
                          const PRUnichar  *aValueArray[] ) {

  nsresult                       rv = NS_OK;

  nsCOMPtr<nsIDocument>          pDocument;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItem;

  nsIURI                        *pURI              = nsnull;


  rv = GetP3PService( );

  if (NS_SUCCEEDED( rv ) && mP3PIsEnabled) {
#ifdef DEBUG_P3P
    { nsCAutoString  csTag;

      csTag.AssignWithConversion( aTag );
      printf( "P3P:  ObserverXML #2 has been notified of %s tag.\n", (const char *)csTag );
    }
#endif

    if (nsCRT::strcasecmp(aTag, "LINK") == 0) {
      pDocument = do_QueryInterface((nsISupports *)aDocumentID,
                                    &rv );

      if (NS_SUCCEEDED( rv )) {
        pDocShellTreeItem = do_QueryInterface( pDocument,
                                              &rv );

        if (NS_SUCCEEDED( rv )) {
          pDocument->GetDocumentURL( &pURI );

          if (pURI) {
            rv = nsP3PObserverUtils::ExamineLINKTag( pURI,
                                                     aNumOfAttributes,
                                                     aNameArray,
                                                     aValueArray,
                                                     pDocShellTreeItem,
                                                     mP3PService );

            NS_RELEASE( pURI );
          }
        }
      }
    }
  }

  return rv;
}

// P3P Observer XML: Notify
//
// Function:  Allows for processing based upon the presence of an XML tag.
//
// Parms:     1. In     The ParserBundle object
//            2. In     The XML tag found
//            3. In     The attribute names
//            4. In     The attribute values
//
NS_IMETHODIMP
nsP3PObserverXML::Notify( nsISupports          *aDocumentID,
                          const PRUnichar      *aTag,
                          const nsStringArray  *aKeys,
                          const nsStringArray  *aValues ) {

  nsresult                           rv = NS_OK;

  nsCOMPtr<nsISupportsParserBundle>  pParserBundle;

  nsAutoString                       sDocumentKey;

  nsCOMPtr<nsIDocShell>              pDocShell;

  nsCOMPtr<nsIDocShellTreeItem>      pDocShellTreeItem;

  nsCOMPtr<nsIWebNavigation>         pWebNavigation;

  nsCOMPtr<nsIURI>                   pURI;


  // Get the P3P service and check if it is enabled
  rv = GetP3PService( );

  if (NS_SUCCEEDED( rv ) && mP3PIsEnabled) {
#ifdef DEBUG_P3P
    { nsCAutoString  csTag;

      csTag.AssignWithConversion( aTag );
      printf( "P3P:  ObserverXML #3 has been notified of %s tag.\n", (const char *)csTag );
    }
#endif

    if (nsCRT::strcasecmp( aTag, "LINK" ) == 0) {
      // Notified of a "LINK" tag
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PObserverXML:  Notify, notified of \"LINK\" tag.\n") );

      // Get the ParserBundle
      pParserBundle = do_QueryInterface( aDocumentID,
                                        &rv );

      if (NS_SUCCEEDED( rv )) {
        // Extract the DocShell from the ParserBundle
        sDocumentKey.AssignWithConversion( "docshell" );
        rv = pParserBundle->GetDataFromBundle( sDocumentKey,
                                               getter_AddRefs( pDocShell ) );

        if (NS_SUCCEEDED( rv ) && pDocShell) {
          pDocShellTreeItem = do_QueryInterface( pDocShell,
                                                &rv );

          if (NS_SUCCEEDED( rv )) {
            pWebNavigation = do_QueryInterface( pDocShell,
                                               &rv );

            if (NS_SUCCEEDED( rv )) {
              rv = pWebNavigation->GetCurrentURI( getter_AddRefs( pURI ) );

              if (NS_SUCCEEDED( rv ) && pURI) {
                // Process the LINK tag
                rv = nsP3PObserverUtils::ExamineLINKTag( pURI,
                                                         aKeys,
                                                         aValues,
                                                         pDocShellTreeItem,
                                                         mP3PService );
              }
              else if (NS_FAILED( rv )) {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PObserverXML:  Notify, pWebNavigation->GetCurrentURI failed - %X.\n", rv) );
              }
            }
            else {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PObserverXML:  Notify, do_QueryInterface for WebNavigation failed - %X.\n", rv) );
            }
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PObserverXML:  Notify, do_QueryInterface for DocShellTreeItem failed - %X.\n", rv) );
          }
        }
        else if (NS_FAILED( rv )) {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PObserverXML:  Notify, pParserBundle->GetDataFromBundle failed - %X.\n", rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PObserverXML:  Notify, do_QueryInterface for ParserBundle failed - %X.\n", rv) );
      }
    }
  }

  return rv;
}

// ****************************************************************************
// nsP3PObserverXML routines
// ****************************************************************************

// P3P Observer XML: GetP3PService
//
// Function:  Obtain the P3P service and determine if it is enabled.
//
// Parms:     None
//
NS_METHOD
nsP3PObserverXML::GetP3PService( ) {

  nsresult  rv = NS_OK;


  if (!mP3PService) {
    mP3PService = do_GetService( NS_P3PSERVICE_CONTRACTID,
                                &rv );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PObserverXML:  GetP3PService, do_GetService for P3P service failed - %X.\n", rv) );
    }
  }

  if (mP3PService) {
    rv = mP3PService->P3PIsEnabled(&mP3PIsEnabled );

    if (NS_SUCCEEDED( rv ) && !mP3PIsEnabled) {
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PObserverXML:  GetP3PService, P3P Service is not enabled.\n") );
    }
    else if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PObserverXML:  GetP3PService, mP3PService->P3PIsEnabled failed - %X.\n", rv) );
    }
  }

  return rv;
}
