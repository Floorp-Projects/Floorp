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

#include "nsP3PHTTPNotify.h"
#include "nsP3PLogging.h"

#include <nsIServiceManager.h>

#include <nsIHTTPHeader.h>
#include <nsIURI.h>
#include <nsILoadGroup.h>

#include <nsIInterfaceRequestor.h>
#include <nsIDocumentLoader.h>

#include <nsString.h>
#include <nsXPIDLString.h>


// ****************************************************************************
// nsP3PHTTPNotify Implementation routines
// ****************************************************************************

static NS_DEFINE_CID( kNetModuleMgrCID, NS_NETMODULEMGR_CID );

// P3P HTTP Notify: nsISupports
NS_IMPL_ISUPPORTS2( nsP3PHTTPNotify, nsIHTTPNotify,
                                     nsINetNotify );

// P3P HTTP Notify: Creation routine
NS_METHOD
NS_NewP3PHTTPNotify( nsIHTTPNotify **aHTTPNotify ) {

  nsresult         rv;

  nsP3PHTTPNotify *pNewHTTPNotify = nsnull;


#ifdef DEBUG_P3P
  printf("P3P:  HTTPNotify initializing.\n");
#endif

  NS_ENSURE_ARG_POINTER( aHTTPNotify );

  *aHTTPNotify = nsnull;

  pNewHTTPNotify = new nsP3PHTTPNotify( );

  if (pNewHTTPNotify) {
    NS_ADDREF( pNewHTTPNotify );

    rv = pNewHTTPNotify->Init( );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewHTTPNotify->QueryInterface( NS_GET_IID( nsIHTTPNotify ),
                                  (void **)aHTTPNotify );
    }

    NS_RELEASE( pNewHTTPNotify );
  }
  else {
    NS_ASSERTION( 0, "P3P:  HTTPNotify unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P HTTP Notify constructor
nsP3PHTTPNotify::nsP3PHTTPNotify( )
  : mP3PIsEnabled( PR_FALSE ) {

  NS_INIT_ISUPPORTS( );
}

// P3P HTTP Notify destructor
nsP3PHTTPNotify::~nsP3PHTTPNotify( ) {

  if (mNetModuleMgr) {
    mNetModuleMgr->UnregisterModule( NS_NETWORK_MODULE_MANAGER_HTTP_RESPONSE_CONTRACTID,
                                     this );

#ifdef DEBUG_P3P
    mNetModuleMgr->UnregisterModule( NS_NETWORK_MODULE_MANAGER_HTTP_REQUEST_CONTRACTID,
                                     this );
#endif
  }
}

// P3P HTTP Notify: Init
//
// Function:  Initialization routine for the HTTP Notify listener.
//
// Parms:     None
//
NS_METHOD
nsP3PHTTPNotify::Init( ) {

  nsresult  rv;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PHTTPNotify:  Init, initializing.\n") );

  // Create the "P3P" atom
  mP3PHeader = NS_NewAtom( P3P_HTTPEXT_P3P_KEY );
  if (!mP3PHeader) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PHTTPNotify:  Init, NS_NewAtom for \"P3P\" failed.\n") );
    NS_ASSERTION( 0, "P3P:  P3PHTTPNotify unable to create \"P3P\" atom.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_SUCCEEDED( rv )) {
    // Create the "Set-Cookie" atom
    mSetCookieHeader = NS_NewAtom( P3P_HTTP_SETCOOKIE_KEY );
    if (!mSetCookieHeader) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PHTTPNotify:  Init, NS_NewAtom for \"Set-Cookie\" failed.\n") );
      NS_ASSERTION( 0, "P3P:  P3PHTTPNotify unable to create \"Set-Cookie\" atom.\n" );
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Create the "Content-Type" atom
    mContentTypeHeader = NS_NewAtom( P3P_HTTP_CONTENTTYPE_KEY );
    if (!mContentTypeHeader) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PHTTPNotify:  Init, NS_NewAtom for \"Content-Type\" failed.\n") );
      NS_ASSERTION( 0, "P3P:  P3PHTTPNotify unable to create \"Content-Type\" atom.\n" );
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Create the "Referer" atom
    mRefererHeader = NS_NewAtom( P3P_HTTP_REFERER_KEY );
    if (!mRefererHeader) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PHTTPNotify:  Init, NS_NewAtom for \"Referer\" failed.\n") );
      NS_ASSERTION( 0, "P3P:  P3PHTTPNotify unable to create \"Referer\" atom.\n" );
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the Network Module Manager service to register the listeners
    mNetModuleMgr = do_GetService( kNetModuleMgrCID,
                                  &rv );

    if (NS_SUCCEEDED( rv )) {
      // Register this object to listen for response headers
      rv = mNetModuleMgr->RegisterModule( NS_NETWORK_MODULE_MANAGER_HTTP_RESPONSE_CONTRACTID,
                                          this );

      if (NS_SUCCEEDED( rv )) {
#ifdef DEBUG_P3P
        // Only register for request headers for debug build
        rv = mNetModuleMgr->RegisterModule( NS_NETWORK_MODULE_MANAGER_HTTP_REQUEST_CONTRACTID,
                                            this );

        if (NS_FAILED( rv )) {
          printf( "P3P:  HTTPNotify Request registration failed: %X\n", rv );

          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PHTTPNotify:  Init, mNetModuleMgr->RegisterModule for HTTP responses failed - %X.\n", rv) );
        }
#endif
      }
      else {
#ifdef DEBUG_P3P
        printf( "P3P:  HTTPNotify Response registration failed: %X\n", rv );
#endif

        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PHTTPNotify:  Init, mNetModuleMgr->RegisterModule for HTTP responses failed - %X.\n", rv) );
      }
    }
    else {
#ifdef DEBUG_P3P
      printf( "P3P:  Unable to obtain Network Module Manager Service: %X\n", rv );
#endif

      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PHTTPNotify:  Init, do_GetService for Network Module Manager service failed - %X.\n", rv) );
    }

  }

  return rv;
}


// ****************************************************************************
// nsIHTTPNotify routines
// ****************************************************************************

// P3P HTTP Notify: ModifyRequest
//
// Function:  Allows modification to the request headers before they are sent to the server.
//
// Parms:     1. In     The HTTPChannel object of the request
//
NS_IMETHODIMP
nsP3PHTTPNotify::ModifyRequest( nsISupports  *aContext ) {

  // Not used in retail build, just satisfying the interface
#ifdef DEBUG_P3P
  nsresult                       rv;

  nsCOMPtr<nsIHTTPChannel>       pHTTPChannel;

  nsCOMPtr<nsIURI>               pURI;


  // Get the P3P service and check if it is enabled
  rv = GetP3PService( );

  if (NS_SUCCEEDED( rv ) && mP3PIsEnabled) {
    // Make sure we have an HTTP channel
    pHTTPChannel = do_QueryInterface( aContext,
                                     &rv );

    if (NS_SUCCEEDED( rv )) {

      rv = pHTTPChannel->GetURI( getter_AddRefs( pURI ) );

      if (NS_SUCCEEDED( rv )) {

        if (!mP3PService->IsP3PRelatedURI( pURI )) {
          P3P_PRINT_REQUEST_HEADERS( pHTTPChannel );
        }
      }
    }
  }
#endif

  return NS_OK;
}

// P3P HTTP Notify: AsyncExamineResponse
//
// Function:  Allows examination of the response headers received from a server.
//
// Parms:     1. In     The HTTPChannel object of the response
//
NS_IMETHODIMP
nsP3PHTTPNotify::AsyncExamineResponse( nsISupports  *aContext ) {

  nsresult                       rv;

  nsCOMPtr<nsIHTTPChannel>       pHTTPChannel;

  nsXPIDLCString                 xcsURISpec,
                                 xcsStatus;

  PRUint32                       uiStatus;

  nsXPIDLCString                 xcsHeaderKey,
                                 xcsHeaderValue;

  nsCOMPtr<nsIURI>               pURI;

  nsCOMPtr<nsIAtom>              pRequestMethod;

  nsAutoString                   sRequestMethod;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItem;


  // Get the P3P service and check if it is enabled
  rv = GetP3PService( );

  if (NS_SUCCEEDED( rv ) && mP3PIsEnabled) {
    // Make sure we have an HTTP channel
    pHTTPChannel = do_QueryInterface( aContext,
                                     &rv );

    if (NS_SUCCEEDED( rv )) {

      rv = pHTTPChannel->GetURI( getter_AddRefs( pURI ) );

      if (NS_SUCCEEDED( rv )) {

        if (!mP3PService->IsP3PRelatedURI( pURI )) {
#ifdef DEBUG_P3P
          { P3P_PRINT_RESPONSE_HEADERS( pHTTPChannel );
          }
#endif

          pURI->GetSpec( getter_Copies( xcsURISpec ) );
          pHTTPChannel->GetResponseStatus(&uiStatus );
          pHTTPChannel->GetResponseString( getter_Copies( xcsStatus ) );
          PR_LOG( gP3PLogModule,
                  PR_LOG_NOTICE,
                  ("P3PHTTPNotify:  AsyncExamineRequest, Receiving document %s - %i %s.\n", (const char *)xcsURISpec, uiStatus, (const char *)xcsStatus) );

          // Process the request headers related to and important to P3P
          ProcessRequestHeaders( pHTTPChannel );

          // Process the response headers related to and important to P3P
          ProcessResponseHeaders( pHTTPChannel );

          if (((uiStatus / 100) != 1) && ((uiStatus / 100) != 3)) {
            // Not an informational or redirection status

            // Look for the "content-type" response header
            rv = pHTTPChannel->GetResponseHeader( mContentTypeHeader,
                                                  getter_Copies( xcsHeaderValue ) );

            if (NS_SUCCEEDED( rv )) {
              PRBool  bCheckPrivacy = PR_FALSE;

              if ((uiStatus / 100) == 2) {
                // OK response, so "content-type" should be valid
                if (xcsHeaderValue) {
                  nsCAutoString  csHeaderValue((const char *)xcsHeaderValue );

                  if (!IsHTMLorXML( csHeaderValue )) {
                    // Not HTML or XML, so we don't have to wait for <LINK> tag detection
                    bCheckPrivacy = PR_TRUE;
                  }
                }
                else {
                  // No "content-type" header, check privacy
                  bCheckPrivacy = PR_TRUE;
                }
              }
              else {
                // Not an OK response, so perform the check
                bCheckPrivacy = PR_TRUE;
              }

              if (bCheckPrivacy) {
                // Privacy Check to be performed now
#ifdef DEBUG_P3P
                { printf( "P3P:  Non HTML/XML or unknown content encountered, complete processing\n" );
                }
#endif

                PR_LOG( gP3PLogModule,
                        PR_LOG_NOTICE,
                        ("P3PHTTPNotify:  AsyncExamineRequest, Non HTML/XML or unknown content encountered.\n") );

                // Obtain the request method used
                rv = pHTTPChannel->GetRequestMethod( getter_AddRefs( pRequestMethod ) );

                if (NS_SUCCEEDED( rv )) {
                  rv = pRequestMethod->ToString( sRequestMethod );

                  if (NS_SUCCEEDED( rv )) {
                    // Get the DocShellTreeItem associated with the request
                    rv = GetDocShellTreeItem( pHTTPChannel,
                                              getter_AddRefs( pDocShellTreeItem ) );

                    if (NS_SUCCEEDED( rv )) {
                      // Check the privacy
                      rv = mP3PService->CheckPrivacy( sRequestMethod,
                                                      pURI,
                                                      pDocShellTreeItem,
                                                      nsnull );

                      if (rv == P3P_PRIVACY_NOT_MET) {
                        // Delete set-cookie header and the cookie
#ifdef DEBUG_P3P
                        { printf( "P3P:    Privacy not met, removing \"Set-Cookie\" header and cookie\n" );
                        }
#endif

                        rv = DeleteCookies( pHTTPChannel );
                      }
                    }
                  }
                  else {
                    PR_LOG( gP3PLogModule,
                            PR_LOG_ERROR,
                            ("P3PHTTPNotify:  AsyncExamineResponse, pRequestMethod->ToString failed - %X.\n", rv) );
                  }
                }
                else {
                  PR_LOG( gP3PLogModule,
                          PR_LOG_ERROR,
                          ("P3PHTTPNotify:  AsyncExamineResponse, pHTTPChannel->GetRequestMethod failed - %X.\n", rv) );
                }
              }
            }
            else {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PHTTPNotify:  AsyncExamineResponse, pHTTPChannel->GetResponseHeader failed - %X.\n", rv) );
            }
          }
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PHTTPNotify:  AsyncExamineResponse, pHTTPChannel->GetURI failed - %X.\n", rv) );
      }
    }
  }

  return NS_OK;
}


// ****************************************************************************
// nsP3PHTTPNotify routines
// ****************************************************************************

// P3P HTTP Notify: ProcessRequestHeaders
//
// Function:  Examines the request headers looking for the "Referer" request header.
//
//              The "Referer" request header value is saved for later processing.
//
// Parms:     1. In     The HTTPChannel object of the response
//
NS_METHOD_( void )
nsP3PHTTPNotify::ProcessRequestHeaders( nsIHTTPChannel  *aHTTPChannel ) {

  nsresult                       rv;

  nsCOMPtr<nsIURI>               pURI;

  nsXPIDLCString                 xcsRefererValue;

  nsAutoString                   sRefererValue;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItem;


  rv = aHTTPChannel->GetURI( getter_AddRefs( pURI ) );

  if (NS_SUCCEEDED( rv )) {
    // Look for the "referer" request header
    rv = aHTTPChannel->GetRequestHeader( mRefererHeader,
                                         getter_Copies( xcsRefererValue ) );

    if (NS_SUCCEEDED( rv )) {
      // Process the "referer" request header value
      sRefererValue.AssignWithConversion((const char *)xcsRefererValue );

      // Get the DocShellTreeItem associated with the request
      rv = GetDocShellTreeItem( aHTTPChannel,
                                getter_AddRefs( pDocShellTreeItem ) );

      if (NS_SUCCEEDED( rv )) {
        mP3PService->SetRefererHeader( pDocShellTreeItem,
                                       pURI,
                                       sRefererValue );
      }
    }
    else if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PHTTPNotify:  ProcessRequestHeaders, pHTTPChannel->GetRequestHeader \"Referer\" failed - %X.\n", rv) );
    }
  }

  return;
}

// P3P HTTP Notify: ProcessResponseHeaders
//
// Function:  Examines the response headers looking for the "P3P" response header
//            and the "Set-Cookie" response header.
//
//              If an "P3P" response header is present further processing is
//              performed using other routines.
//
//              The "Set-Cookie" response header value is saved for later processing.
//
// Parms:     1. In     The HTTPChannel object of the response
//
NS_METHOD_( void )
nsP3PHTTPNotify::ProcessResponseHeaders( nsIHTTPChannel  *aHTTPChannel ) {

  nsresult                       rv;

  nsCOMPtr<nsIURI>               pURI;

  nsXPIDLCString                 xcsP3PKey,
                                 xcsP3PValue;

  nsXPIDLCString                 xcsSetCookieKey,
                                 xcsSetCookieValues;

  nsCAutoString                  csP3PValue;

  nsAutoString                   sSetCookieValues;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItem;


  rv = aHTTPChannel->GetURI( getter_AddRefs( pURI ) );

  if (NS_SUCCEEDED( rv )) {
    // Look for the "P3P" response header
    rv = aHTTPChannel->GetResponseHeader( mP3PHeader,
                                          getter_Copies( xcsP3PValue ) );

    if (NS_SUCCEEDED( rv ) && xcsP3PValue) {
      // Process the "P3P" response header value(s)
      csP3PValue = xcsP3PValue;
      ProcessP3PHeaders( csP3PValue,
                         aHTTPChannel );
    }
    else if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PHTTPNotify:  ProcessResponseHeaders, aHTTPChannel->GetResponseHeader \"P3P\" failed - %X.\n", rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PHTTPNotify:  ProcessResponseHeaders, aHTTPChannel->GetURI failed - %X.\n", rv) );
  }

  if (NS_SUCCEEDED( rv )) {
    // Look for the "set-cookie" response header
    rv = aHTTPChannel->GetResponseHeader( mSetCookieHeader,
                                          getter_Copies( xcsSetCookieValues ) );

    if (NS_SUCCEEDED( rv ) && xcsSetCookieValues) {
      // Process the "set-cookie" response header value(s)
      sSetCookieValues.AssignWithConversion((const char *)xcsSetCookieValues );

      // Get the DocShellTreeItem associated with the request
      rv = GetDocShellTreeItem( aHTTPChannel,
                                getter_AddRefs( pDocShellTreeItem ) );

      if (NS_SUCCEEDED( rv )) {
        mP3PService->SetCookieHeader( pDocShellTreeItem,
                                      pURI,
                                      sSetCookieValues );
      }
    }
    else if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PHTTPNotify:  ProcessResponseHeaders, pHTTPChannel->GetResponseHeader \"Set-Cookie\" failed - %X.\n", rv) );
    }
  }

  return;
}

// P3P HTTP Notify: ProcessP3PHeaders
//
// Function:  Process all the HTTP "P3P" response headers values
//
// Parms:     1. In     The P3P response header value
//            2. In     The HTTPChannel object of the response
//
NS_METHOD
nsP3PHTTPNotify::ProcessP3PHeaders( nsCString&       aP3PValues,
                                    nsIHTTPChannel  *aHTTPChannel ) {

  nsresult       rv;

  PRInt32        iCharOffset;

  nsCAutoString  csValues,
                 csWork;


#ifdef DEBUG_P3P
  printf( "P3P:  P3P: %s\n", (const char *)aP3PValues );
#endif

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PHTTPNotify:  ProcessP3PHeaders, processing P3P header(s): %s.\n", (const char *)aP3PValues) );

  // Loop through all "P3P" response header values
  csValues = aP3PValues;
  do {
    iCharOffset = csValues.FindChar( ',' );
    csValues.Left( csWork,
                   iCharOffset );
    csValues.Cut( 0,
                  iCharOffset + 1 );

    rv = ProcessIndividualP3PHeader( csWork,
                                     aHTTPChannel );
  } while (iCharOffset != kNotFound);

  return rv;
}

// P3P HTTP Notify: ProcessIndividualP3PHeader
//
// Function:  Process each HTTP "P3P" response headers value
//
// Parms:     1. In     The P3P response header value
//            2. In     The HTTPChannel object of the response
//
NS_METHOD
nsP3PHTTPNotify::ProcessIndividualP3PHeader( nsCString&      aP3PValue,
                                             nsIHTTPChannel *aHTTPChannel ) {

  nsresult                       rv;

  nsCOMPtr<nsIURI>               pURI;

  PRInt32                        iLocation;

  nsAutoString                   sPolicyRefFileURISpec;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItem;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PHTTPNotify:  ProcessIndividualP3PHeader, processing P3P header: %s.\n", (const char *)aP3PValue) );

  rv = aHTTPChannel->GetURI( getter_AddRefs( pURI ) );

  if (NS_SUCCEEDED( rv )) {
#ifdef DEBUG_P3P
    { nsXPIDLCString  xcsURISpec;

      rv = pURI->GetSpec( getter_Copies( xcsURISpec ) );
      printf( "      URI: %s\n", (const char *)xcsURISpec );
    }
#endif

    iLocation = aP3PValue.Find( P3P_HTTPEXT_POLICYREF_KEY,
                                PR_TRUE );

    if (iLocation >= 0) {
      nsCAutoString  csPolicyRefURISpec;

      iLocation = aP3PValue.Find( "=" );
      aP3PValue.Right( csPolicyRefURISpec,
                       aP3PValue.Length( ) - (iLocation + 1) );

      csPolicyRefURISpec.Trim( "\"" );

      if (csPolicyRefURISpec.Length( ) > 0) {
#ifdef DEBUG_P3P
        { printf( "      Policy Ref File:  %s\n", (const char *)csPolicyRefURISpec );
        }
#endif

        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PHTTPNotify:  ProcessIndividualP3PHeader, P3P PolicyRef header found - %s.\n", (const char *)csPolicyRefURISpec) );

        sPolicyRefFileURISpec.AssignWithConversion( csPolicyRefURISpec );

        // Get the DocShellTreeItem associated with the request
        rv = GetDocShellTreeItem( aHTTPChannel,
                                  getter_AddRefs( pDocShellTreeItem ) );

        if (NS_SUCCEEDED( rv )) {
          // Save the PolicyRefFile URI spec
          mP3PService->SetPolicyRefFileURISpec( pDocShellTreeItem,
                                                pURI,
                                                P3P_REFERENCE_HTTP_HEADER,
                                                sPolicyRefFileURISpec );
        }
      }
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PHTTPNotify:  ProcessIndividualP3PHeader, aHTTPChannel->GetURI failed - %X.\n", rv) );
  }

  return rv;
}

// P3P HTTP Notify: IsHTMLorXML
//
// Function:  Determines if the content type of the URI object is HTML or XML.
//
// Parms:     1. In     The Content-Type response header value
//
NS_METHOD_( PRBool )
nsP3PHTTPNotify::IsHTMLorXML( nsCString&  aContentType ) {

  nsCAutoString  csContentType( aContentType );


  if (csContentType.Find( ";" ) >= 0) {
    nsCAutoString  csWork;

    csContentType.Left( csWork,
                        csContentType.Find( ";" ) );
    csContentType = csWork;
  }

  return (csContentType.EqualsIgnoreCase( "text/html" ) ||
          csContentType.EqualsIgnoreCase( "text/xml" )  ||
          csContentType.EqualsIgnoreCase( "application/xml" )  ||
          csContentType.EqualsIgnoreCase( "application/xhtml+xml" ));
}

// P3P HTTP Notify: GetDocShellTreeItem
//
// Function:  Finds the DocShellTreeItem object associated with this request.
//
// Parms:     1. In     The HTTPChannel object of the request
//            2. Out    The DocShellTreeItem object
//
NS_METHOD
nsP3PHTTPNotify::GetDocShellTreeItem( nsIHTTPChannel       *aHTTPChannel,
                                      nsIDocShellTreeItem **aDocShellTreeItem ) {

  nsresult                         rv;

  nsCOMPtr<nsIInterfaceRequestor>  pNotificationCallbacks;

  nsCOMPtr<nsILoadGroup>           pLoadGroup;

  nsCOMPtr<nsIRequest>             pRequest;

  nsCOMPtr<nsIChannel>             pChannel;

  nsCOMPtr<nsIDocumentLoader>      pDocumentLoader;

  nsCOMPtr<nsIDocShellTreeItem>    pDocShellTreeItem;


  NS_ENSURE_ARG_POINTER( aDocShellTreeItem );

  *aDocShellTreeItem = nsnull;

  // Get the notification callbacks for the HTTP channel
  rv = aHTTPChannel->GetNotificationCallbacks( getter_AddRefs( pNotificationCallbacks ) );

  if (NS_SUCCEEDED( rv )) {

    if (!pNotificationCallbacks) {
      // No notification callbacks for the HTTP channel, check the default channel
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PHTTPNotify:  GetDocShellTreeItem, No notification callbacks on current channel, getting default channel.\n") );

      // Get the loadgroup for the HTTP channel
      rv = aHTTPChannel->GetLoadGroup( getter_AddRefs( pLoadGroup ) );

      if (NS_SUCCEEDED( rv ) && pLoadGroup) {
        // Get the default load channel
        rv = pLoadGroup->GetDefaultLoadRequest( getter_AddRefs( pRequest ) );

        if (NS_SUCCEEDED( rv ) && pRequest) {

          pChannel = do_QueryInterface( pRequest,
                                       &rv );

          if (NS_SUCCEEDED( rv ) && pChannel) {
            // Get the notification callbacks for the default channel
            rv = pChannel->GetNotificationCallbacks( getter_AddRefs( pNotificationCallbacks ) );
          }
        }
      }
      else if (NS_FAILED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PHTTPNotify:  GetDocShellTreeItem, aHTTPChannel->GetLoadGroup failed - %X.\n", rv) );
      }
    }

    if (NS_SUCCEEDED( rv ) && pNotificationCallbacks) {
      // Check if the notification callback is a DocumentLoader
      pDocumentLoader = do_QueryInterface( pNotificationCallbacks,
                                          &rv );

      if (pDocumentLoader) {
        // It's a DocumentLoader
        nsCOMPtr<nsISupports>  pContainer;

        // Get the container for the DocumentLoader
        rv = pDocumentLoader->GetContainer( getter_AddRefs( pContainer ) );

        if (NS_SUCCEEDED( rv )) {
          // Check if the container is a DocShellTreeItem
          pDocShellTreeItem = do_QueryInterface( pContainer,
                                                &rv );
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PHTTPNotify:  GetDocShellTreeItem, pDocumentLoader->GetContainer failed - %X.\n", rv) );
        }
      }
      else {
        // It's not a DocumentLoader, check if the notification callback is a DocShellTreeItem
        pDocShellTreeItem = do_QueryInterface( pNotificationCallbacks,
                                              &rv );
      }
    }
    else if (NS_SUCCEEDED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PHTTPNotify:  GetDocShellTreeItem, unable to obtain NotificationCallbacks.\n") );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PHTTPNotify:  GetDocShellTreeItem, aHTTPChannel->GetNotificationCallbacks failed - %X.\n", rv) );
  }


  if (NS_SUCCEEDED( rv )) {

    if (pDocShellTreeItem) {
      *aDocShellTreeItem = pDocShellTreeItem;
      NS_ADDREF( *aDocShellTreeItem );
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PHTTPNotify:  GetDocShellTreeItem, unable to obtain DocShellTreeItem.\n") );

      rv = NS_ERROR_FAILURE;
    }
  }

  return rv;
}

// P3P HTTP Notify: DeleteCookies
//
// Function:  Deletes any "Set-Cookie" response headers.
//
// Parms:     1. In     The HTTPChannel object of the request
//
NS_METHOD
nsP3PHTTPNotify::DeleteCookies( nsIHTTPChannel  *aHTTPChannel ) {

  nsresult        rv;

  nsXPIDLCString  xcsSetCookieValues;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PHTTPNotify:  DeleteCookies, deleting \"Set-Cookie\" response header.\n") );

  // Look for the "set-cookie" response header
  rv = aHTTPChannel->GetResponseHeader( mSetCookieHeader,
                                        getter_Copies( xcsSetCookieValues ) );

  if (NS_SUCCEEDED( rv ) && xcsSetCookieValues) {
#ifdef DEBUG_P3P
    printf( "P3P:  Set-Cookie: %s\n", (const char *)xcsSetCookieValues );
#endif

    // Clear the "set-cookie" response header
    aHTTPChannel->SetResponseHeader( mSetCookieHeader,
                                     nsnull );
#ifdef DEBUG_P3P
    {
      aHTTPChannel->GetResponseHeader( mSetCookieHeader,
                                       getter_Copies( xcsSetCookieValues ) );
      if (xcsSetCookieValues) {
        printf( "P3P:  Set-Cookie: %s\n", (const char *)xcsSetCookieValues );
      }
    }
#endif
  }
  else if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PHTTPNotify:  DeleteCookies, aHTTPChannel->GetResponseHeader failed - %X.\n", rv) );
  }

  return rv;
}

// P3P HTTP Notify: GetP3PService
//
// Function:  Obtain the P3P service and determine if it is enabled.
//
// Parms:     None
//
NS_METHOD
nsP3PHTTPNotify::GetP3PService( ) {

  nsresult  rv = NS_OK;


  if (!mP3PService) {
    mP3PService = do_GetService( NS_P3PSERVICE_CONTRACTID,
                                &rv );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PHTTPNotify:  GetP3PService, do_GetService for P3P service failed - %X.\n", rv) );
    }
  }

  if (mP3PService) {
    rv = mP3PService->P3PIsEnabled(&mP3PIsEnabled );

    if (NS_SUCCEEDED( rv ) && !mP3PIsEnabled) {
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PHTTPNotify:  GetP3PService, P3P Service is not enabled.\n") );
    }
    else if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PHTTPNotify:  GetP3PService, mP3PService->P3PIsEnabled failed - %X.\n", rv) );
    }
  }

  return rv;
}
