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

#include "nsP3PXMLProcessor.h"
#include "nsP3PXMLUtils.h"
#include "nsP3PTags.h"
#include "nsP3PLogging.h"

#include "nsIDOMElement.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMEvent.h"

#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIDOMParser.h"

#include "nsIHttpChannel.h"

#include "nsLayoutCID.h"

#include "nsNetUtil.h"

#include "nsIDocument.h"


// ****************************************************************************
// nsP3PXMLProcessor Implementation routines
// ****************************************************************************

static NS_DEFINE_CID( kXMLDocumentCID,  NS_XMLDOCUMENT_CID );
static NS_DEFINE_CID( kNetModuleMgrCID, NS_NETMODULEMGR_CID );

// P3P XML Processor: nsISupports
NS_IMPL_ISUPPORTS1( nsP3PXMLProcessor, nsISupports );

// P3P XML Processor: Constructor
nsP3PXMLProcessor::nsP3PXMLProcessor( )
  : mDocumentType( nsnull ),
    mFirstUse( PR_TRUE ),
    mP3PRelatedURIAdded( PR_FALSE ),
    mReadInProgress( PR_FALSE ),
    mProcessComplete( PR_FALSE ),
    mLock( PR_NewLock( ) ),
    mUseDOMParser( PR_FALSE ),
    mInlineRead( PR_FALSE ),
    mRedirected( PR_FALSE ),
    mContentValid( PR_FALSE ),
    mDocumentValid( PR_TRUE ) {

  NS_INIT_ISUPPORTS( );
}

// P3P XML Processor: Destructor
nsP3PXMLProcessor::~nsP3PXMLProcessor( ) {

  if (mP3PRelatedURIAdded) {
    mP3PService->RemoveP3PRelatedURISpec( mURISpec );
  }

  if (mTag) {
    mTag->DestroyTree( );
  }

  PR_DestroyLock( mLock );
}


// ****************************************************************************
// nsP3PXMLProcessor routines
// ****************************************************************************

// P3P XML Processor: Init
//
// Function:  Initialization routine for the XML Processor object.
//
// Parms:     None
//
NS_METHOD
nsP3PXMLProcessor::Init( nsString&  aURISpec ) {

  nsresult  rv;


#ifdef DEBUG_P3P
  printf("P3P:  XMLProcessor initializing.\n");
#endif

  mReadURISpec = aURISpec;
  mcsReadURISpec.AssignWithConversion( aURISpec );
  mURISpec     = aURISpec;
  mcsURISpec.AssignWithConversion( aURISpec );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PXMLProcessor:  %s Init, initializing.\n", (const char *)mcsURISpec) );

  mTime = PR_Now( );

  rv = NS_NewURI( getter_AddRefs( mURI ),
                  mURISpec );

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s Init, NS_NewURI failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  if (NS_SUCCEEDED( rv )) {
    mP3PService = do_GetService( NS_P3PSERVICE_CONTRACTID,
                                &rv );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PXMLProcessor:  %s Init, do_GetService for P3P service failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    mNetModuleMgr = do_GetService( kNetModuleMgrCID,
                                  &rv );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PXMLProcessor:  %s Init, do_GetService for NetModuleMgr service failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    mReadRequests = do_CreateInstance( NS_SUPPORTSARRAY_CONTRACTID,
                                      &rv );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PXMLProcessor:  %s Init, do_CreateInstance for read request supports array failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    mP3PService->AddP3PRelatedURISpec( aURISpec );
    mP3PRelatedURIAdded = PR_TRUE;

    rv = PostInit( aURISpec );
  }

  return rv;
}

// P3P XML Processor: Read
//
// Function:  Initiates the read of the URI based upon the type of read specified.
//
// Parms:     1. In     The type of read to be performed
//            2. In     The URI object for whom the read is being performed
//            2. In     The XML Listener object to notify when the read is complete
//            3. In     The data to be supplied back to the XML Listener object
//
NS_METHOD
nsP3PXMLProcessor::Read( PRInt32             aReadType,
                         nsIURI             *aReadForURI,
                         nsIP3PXMLListener  *aXMLListener,
                         nsISupports        *aReaderData ) {

  nsresult                    rv;

  PRBool                      bNotifyReaders = PR_FALSE;

  nsCOMPtr<nsISupportsArray>  pReadRequests;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PXMLProcessor:  %s Read, read initiated.\n", (const char *)mcsURISpec) );

  rv = PreRead( aReadType,
                aReadForURI );

  if (NS_SUCCEEDED( rv )) {
    nsAutoLock  lock( mLock );

    if (!mProcessComplete) {
      // A read has not completed yet for this document
      switch (aReadType) {
        case P3P_ASYNC_READ:
          rv = AsyncRead( aXMLListener,
                          aReaderData );
          break;

        case P3P_SYNC_READ:
          rv = SyncRead( );
          break;

        default:
          rv = NS_ERROR_FAILURE;
          break;
      }

      if (mProcessComplete && (aReadType == P3P_ASYNC_READ)) {
        // An async read is complete
        mReadInProgress = PR_FALSE;
        bNotifyReaders  = PR_TRUE;
        rv = mReadRequests->Clone( getter_AddRefs( pReadRequests ) );

        if (NS_SUCCEEDED( rv )) {
          mReadRequests->Clear( );
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PXMLProcessor:  %s Read, mReadRequests->Clone failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s Read, PreRead failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  if (NS_SUCCEEDED( rv ) && bNotifyReaders) {
    // Async process is complete, notify all readers that are waiting (without holding the lock)
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PXMLProcessor:  %s Read, async process is complete, notifying waiting readers.\n", (const char *)mcsURISpec) );

    NotifyReaders( pReadRequests );
  }

  return rv;
}

// P3P XML Processor: InlineRead
//
// Function:  Processes the data that has already been read.
//
// Parms:     1. In     The DOMNode of the inline data
//            2. In     An indicator as to whether the document containing the data was redirected
//            3. In     The type of read to be performed
//            4. In     The URI object for whom the read is being performed
//            5. In     The XML Listener object to notify when the read is complete
//            6. In     The data to be supplied back to the XML Listener object
//
NS_METHOD
nsP3PXMLProcessor::InlineRead( nsIDOMNode         *aDOMNode,
                               PRBool              aRedirected,
                               PRTime              aExpirationTime,
                               PRInt32             aReadType,
                               nsIURI             *aReadForURI,
                               nsIP3PXMLListener  *aXMLListener,
                               nsISupports        *aReaderData ) {

  nsresult                    rv;

  PRBool                      bNotifyReaders = PR_FALSE;

  nsCOMPtr<nsISupportsArray>  pReadRequests;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PXMLProcessor:  %s InlineRead, inline read initiated.\n", (const char *)mcsURISpec) );

  // Set the inline read indicator
  mInlineRead = PR_TRUE;

  rv = PreRead( aReadType,
                aReadForURI );

  if (NS_SUCCEEDED( rv )) {
    nsAutoLock  lock( mLock );

    if (!mProcessComplete) {
      // A read has not completed yet for this document
      //   Set the document variables based on the containing document
      mDOMNode        = aDOMNode;
      mRedirected     = aRedirected;
      mExpirationTime = aExpirationTime;

      switch (aReadType) {
        case P3P_ASYNC_READ:
          rv = AsyncRead( aXMLListener,
                          aReaderData );
          break;

        case P3P_SYNC_READ:
          rv = SyncRead( );
          break;

        default:
          rv = NS_ERROR_FAILURE;
          break;
      }

      if (mProcessComplete && (aReadType == P3P_ASYNC_READ)) {
        mReadInProgress = PR_FALSE;
        bNotifyReaders  = PR_TRUE;
        rv = mReadRequests->Clone( getter_AddRefs( pReadRequests ) );

        if (NS_SUCCEEDED( rv )) {
          mReadRequests->Clear( );
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PXMLProcessor:  %s InlineRead, mReadRequests->Clone failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s InlineRead, PreRead failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  if (NS_SUCCEEDED( rv ) && bNotifyReaders) {
    // An "async" inline process is complete, notify all readers that are waiting (without holding the lock)
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PXMLProcessor:  %s Read, async process is complete, notifying waiting readers.\n", (const char *)mcsURISpec) );

    NotifyReaders( pReadRequests );
  }

  return rv;
}

// P3P XML Processor: OnModifyRequest
//
// Function:  Allows modification to the request headers before they are sent to the server.
//
// Parms:     1. In     The HTTPChannel object of the request
//
NS_IMETHODIMP
nsP3PXMLProcessor::OnModifyRequest( nsIHttpChannel *aHttpChannel ) {

  nsresult                  rv;

  nsCOMPtr<nsIHttpChannel>  pRequestHTTPChannel;

  nsCOMPtr<nsIChannel> channel;
  rv = mXMLHttpRequest->GetChannel( getter_AddRefs( channel ) );
  pRequestHTTPChannel = do_QueryInterface(channel);

  if (NS_SUCCEEDED( rv ) && (aHttpChannel == pRequestHTTPChannel)) {
#ifdef DEBUG_P3P
    { printf( "P3P:  Clearing \"Safe Zone\" headers and requesting \"no-cache\" for P3P request\n" );
    }
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PXMLProcessor:  ModifyRequest, clearing safe zone headers and requesting fresh copies for P3P URI request - %s.\n", (const char *)mcsURISpec) );

    // Clear certain request headers as part of the P3P safe zone
    ClearSafeZoneHeaders( aHttpChannel );

    // Request a non-cached version of the P3P document
    RequestFreshCopy( aHttpChannel );

#ifdef DEBUG_P3P
    { P3P_PRINT_REQUEST_HEADERS( aHttpChannel );
    }
#endif
  }
  else if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s ModifyRequest, mXMLHttpRequest->GetChannel failed - %X.\n", (const char *)mcsURISpec, rv) );
    }

  return NS_OK;
}

// P3P XML Processor: AsyncExamineResponse
//
// Function:  Allows examination of the response headers received from a server.
//
// Parms:     1. In     The HTTPChannel object of the response
//
NS_METHOD
nsP3PXMLProcessor::OnExamineResponse( nsIHttpChannel *aHttpChannel ) {

  nsresult                  rv;

  nsCOMPtr<nsIHttpChannel>  pRequestHTTPChannel;

  PRUint32                  uiResponseStatus,
                            uiResponseClass;


  nsCOMPtr<nsIChannel> channel;
  rv = mXMLHttpRequest->GetChannel( getter_AddRefs( channel ) );
  pRequestHTTPChannel = do_QueryInterface(channel);

  if (NS_SUCCEEDED( rv ) && (aHttpChannel == pRequestHTTPChannel)) {
#ifdef DEBUG_P3P
    { P3P_PRINT_RESPONSE_HEADERS( aHttpChannel );
    }
#endif

    rv = aHttpChannel->GetResponseStatus(&uiResponseStatus );

    if (NS_SUCCEEDED( rv )) {
      uiResponseClass = uiResponseStatus / 100;

      // XXX this is not the correct way to determine if a redirect occured --darin
      switch (uiResponseClass) {
        case 3:
          switch (uiResponseStatus) {
            case 301:
            case 302:
            case 303:
            case 307:
              PR_LOG( gP3PLogModule,
                      PR_LOG_NOTICE,
                      ("P3PXMLProcessor:  %s AsyncExamineResponse, request has been redirected.\n", (const char *)mcsURISpec) );

              mRedirected = PR_TRUE;
              break;

            default:
              break;
          }

        default:
          break;
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PXMLProcessor:  %s AsyncExamineResponse, aHttpChannel-GetResponseStatus failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }
  else if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s AsyncExamineResponse, mXMLHttpRequest->GetChannel failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return NS_OK;
}

// P3P XML Processor: HandleEvent
//
// Function:  Notification of the completion of an event (used to indicate the
//            completion of an asynchronous read).
//
// Parms:     1. In     The DOMEvent object
//
NS_METHOD
nsP3PXMLProcessor::HandleEvent( nsIDOMEvent  *aEvent ) {

  nsresult                    rv;

  nsString                    sEventType;

  PRBool                      bNotifyReaders = PR_FALSE;

  nsCOMPtr<nsISupportsArray>  pReadRequests;


  rv = aEvent->GetType( sEventType );

  if (NS_SUCCEEDED( rv )) {
    nsAutoLock  lock( mLock );
    
    if (sEventType.EqualsWithConversion( "load" )) {
      // "Load" event
#ifdef DEBUG_P3P
      { printf( "P3P:  Read complete for %s (read as %s)\n", (const char *)mcsURISpec, (const char *)mcsReadURISpec );
      }
#endif

      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PXMLProcessor:  %s HandleEvent, read complete for %s.\n", (const char *)mcsURISpec, (const char *)mcsReadURISpec) );

      rv = NS_OK;
    }
    else {
      // "Error" is the remaining event
#ifdef DEBUG_P3P
      { printf( "P3P:  Error encountered reading %s (read as %s)\n", (const char *)mcsURISpec, (const char *)mcsReadURISpec );
      }
#endif

      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PXMLProcessor:  %s HandleEvent, error encountered reading %s.\n", (const char *)mcsURISpec, (const char *)mcsReadURISpec) );

      rv = NS_ERROR_FAILURE;
    }

    ProcessReadComplete( P3P_ASYNC_READ,
                         rv );

    if (mProcessComplete) {
      // Async read is complete
      mReadInProgress = PR_FALSE;
      bNotifyReaders  = PR_TRUE;
      rv = mReadRequests->Clone( getter_AddRefs( pReadRequests ) );

      if (NS_SUCCEEDED( rv )) {
        mReadRequests->Clear( );
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PXMLProcessor:  %s HandleEvent, mReadRequests->Clone failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
  }

  if (NS_SUCCEEDED( rv ) && bNotifyReaders) {
    // Async read is complete, notify all readers that are waiting (without holding the lock)
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PXMLProcessor:  %s HandleEvent, async process is complete, notifying waiting readers.\n", (const char *)mcsURISpec) );

    NotifyReaders( pReadRequests );
  }

  return NS_OK;
}

// P3P XML Processor: CalculateExpiration
//
// Function:  Calculates the expiration of the document.
//
// Parms:     1. In     The default lifetime of the document (in seconds)
//
NS_METHOD_( void )
nsP3PXMLProcessor::CalculateExpiration( PRInt32   aDefaultLifeTime ) {

  PRStatus       tStatus = PR_SUCCESS;

  PRTime         tTime;

  PRBool         bExpirationSet;


  // Use current time as default
  tTime = PR_Now( );

  if (mDateValue.Length( ) > 0) {
    // "Date" header supplied, update the time
    nsCAutoString  csDate;

    csDate.AssignWithConversion( mDateValue );
    tStatus = PR_ParseTimeString((const char *)csDate,
                                  PR_TRUE,
                                 &tTime );

    if (tStatus != PR_SUCCESS) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PXMLProcessor:  %s CalculateExpiration, PR_ParseTimeString failed for %s.\n", (const char *)mcsURISpec, (const char *)csDate) );
    }
  }

  if (tStatus == PR_SUCCESS) {
    bExpirationSet = PR_FALSE;

    if (mCacheControlValue.Length( ) > 0) {
      // "Cache-Control" overrides "Expires" and "Pragma"
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PXMLProcessor:  %s CalculateExpiration, checking \"Cache-Control\" header.\n", (const char *)mcsURISpec) );

      nsCAutoString  csValues, csWork;
      PRInt32        iCommaOffset;

      // Loop through all the "Cache-Control" values
      csValues.AssignWithConversion( mCacheControlValue );
      do {
        iCommaOffset = csValues.FindChar( ',' );
        csValues.Left( csWork,
                       iCommaOffset );
        csValues.Cut( 0,
                      iCommaOffset + 1 );

        if (csWork.Length( ) > 0) {
          nsCAutoString  csDirective;
          PRInt32        iEqualOffset;

          iEqualOffset = csWork.FindChar( '=' );
          csWork.Left( csDirective,
                       iEqualOffset );
          csWork.Cut( 0,
                      iEqualOffset + 1 );

          if (csDirective.EqualsIgnoreCase( P3P_CACHECONTROL_NOCACHE )) {
            // "no-cache" directive, use start time to indicate already expired
            PR_LOG( gP3PLogModule,
                    PR_LOG_NOTICE,
                    ("P3PXMLProcessor:  %s CalculateExpiration, \"no-cache\" directive found, expiration set.\n", (const char *)mcsURISpec) );

            mExpirationTime = mTime;
            bExpirationSet  = PR_TRUE;
          }
          else if (csDirective.EqualsIgnoreCase( P3P_CACHECONTROL_MAXAGE )) {
            // "max-age" directive, calculate the expiration time
            PR_LOG( gP3PLogModule,
                    PR_LOG_NOTICE,
                    ("P3PXMLProcessor:  %s CalculateExpiration, \"max-age\" directive found, expiration set.\n", (const char *)mcsURISpec) );

            PRInt64  iMaxAge, iMicroSecondsPerSecond, iMaxAgeMicroSeconds;

            LL_I2L( iMaxAge, atoi((const char *)csWork ) );
            LL_I2L( iMicroSecondsPerSecond, PR_USEC_PER_SEC );
            LL_MUL( iMaxAgeMicroSeconds, iMaxAge, iMicroSecondsPerSecond );
            LL_ADD( mExpirationTime, tTime, iMaxAgeMicroSeconds );
            bExpirationSet  = PR_TRUE;
          }
        }
      } while (!bExpirationSet && (iCommaOffset != kNotFound));
    }

    if (!bExpirationSet && (mExpiresValue.Length( ) > 0)) {
      // "Expires" overrides "Pragma"
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PXMLProcessor:  %s CalculateExpiration, checking \"Expires\" header.\n", (const char *)mcsURISpec) );

      nsCAutoString  csExpires;

      csExpires.AssignWithConversion( mExpiresValue );
      tStatus = PR_ParseTimeString((const char *)csExpires,
                                    PR_TRUE,
                                   &mExpirationTime );

      if (tStatus == PR_SUCCESS) {
        // Expiration time successfully parsed
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PXMLProcessor:  %s CalculateExpiration, expiration set.\n", (const char *)mcsURISpec) );
      }
      else {
        // Error during parsing, use start time to indicate already expired
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PXMLProcessor:  %s CalculateExpiration, PR_ParseTimeString failed for %s.\n", (const char *)mcsURISpec, (const char *)csExpires) );

        mExpirationTime = mTime;
      }

      bExpirationSet = PR_TRUE;
    }

    if (!bExpirationSet && (mPragmaValue.Length( ) > 0)) {
      // Use "Pragma" value
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PXMLProcessor:  %s CalculateExpiration, checking \"Pragma\" header.\n", (const char *)mcsURISpec) );

      nsCAutoString  csValues, csWork;
      PRInt32        iCharOffset;

      // Loop through all the "Pragma" values
      csValues.AssignWithConversion( mPragmaValue );
      do {
        iCharOffset = csValues.FindChar( ',' );
        csValues.Left( csWork,
                       iCharOffset );
        csValues.Cut( 0,
                      iCharOffset + 1 );

        if (csWork.EqualsIgnoreCase( P3P_PRAGMA_NOCACHE )) {
          // "no-cache" directive, use start time to indicate already expired
          PR_LOG( gP3PLogModule,
                  PR_LOG_NOTICE,
                  ("P3PXMLProcessor:  %s CalculateExpiration, \"no-cache\" directive found, expiration set.\n", (const char *)mcsURISpec) );

          mExpirationTime = mTime;
          bExpirationSet  = PR_TRUE;
        }
      } while (!bExpirationSet && (iCharOffset != kNotFound));
    }

    if (!bExpirationSet) {
      // No cache related information, use the default lifetime
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PXMLProcessor:  %s CalculateExpiration, no cache related information found, using the default lifetime.\n", (const char *)mcsURISpec) );

      PRInt64  iDefaultLifeTime, iMicroSecondsPerSecond, iDefaultLifeTimeMicroSeconds;

      LL_I2L( iDefaultLifeTime, aDefaultLifeTime );
      LL_I2L( iMicroSecondsPerSecond, PR_USEC_PER_SEC );
      LL_MUL( iDefaultLifeTimeMicroSeconds, iDefaultLifeTime, iMicroSecondsPerSecond );
      LL_ADD( mExpirationTime, PR_Now( ), iDefaultLifeTimeMicroSeconds );
      bExpirationSet  = PR_TRUE;
    }
  }
  else {
    // Error during parsing, use start time to indicate already expired
    mExpirationTime = mTime;
    bExpirationSet  = PR_TRUE;
  }

  return;
}


// ****************************************************************************
// nsP3PXMLProcessor routines (virtual)
// ****************************************************************************

// P3P XML Processor: PostInit
//
// Function:  Allows for additional initialization in derived classes.
//
// Parms:     1. In     The URI spec being processed
//
NS_IMETHODIMP
nsP3PXMLProcessor::PostInit( nsString&  aURISpec ) {

  return NS_OK;
}

// P3P XML Processor: PreRead
//
// Function:  Allows for additional processing to be performed before the read.
//
// Parms:     1. In     An indicator as to whether the read is Asynchronous or Synchronous
//            2. In     The URI object that the read is being performed for
NS_IMETHODIMP
nsP3PXMLProcessor::PreRead( PRInt32   aReadType,
                            nsIURI   *aReadForURI ) {

  return NS_OK;
}

// P3P XML Processor: ProcessContent
//
// Function:  Allows for the processing of the content read.
//
// Parms:     1. In     The type of read performed
//
NS_IMETHODIMP
nsP3PXMLProcessor::ProcessContent( PRInt32   aReadType ) {

  return NS_OK;
}

// P3P XML Processor: CheckForComplete
//
// Function:  Allows for the determination of whether the processing of the URI is complete.
//
// Parms:     None
//
NS_IMETHODIMP_( void )
nsP3PXMLProcessor::CheckForComplete( ) {

  mProcessComplete = PR_TRUE;

  return;
}


// ****************************************************************************
// nsP3PXMLProcessor routines
// ****************************************************************************

// P3P XML Processor: AsyncRead
//
// Function:  Performs an asynchronous read of the URI.
//
// Parms:     1. In     The XML Listener object to notify when the read is complete
//            2. In     The data to be supplied back to the XML Listener object
//
NS_METHOD
nsP3PXMLProcessor::AsyncRead( nsIP3PXMLListener  *aXMLListener,
                              nsISupports        *aReaderData ) {

  nsresult                     rv = NS_OK;

  nsCOMPtr<nsISupports>        pReadRequest;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PXMLProcessor:  %s AsyncRead, async read initiated.\n", (const char *)mcsURISpec) );

  rv = NS_NewP3PXMLProcessorReadRequest( mURISpec,
                                         aXMLListener,
                                         aReaderData,
                                         getter_AddRefs( pReadRequest ) );

  if (NS_SUCCEEDED( rv )) {

    if (mInlineRead) {
      // Data contained inline with a document already read in and parsed
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PXMLProcessor:  %s AsyncRead, inline read requested.\n", (const char *)mcsURISpec) );

      rv = ProcessReadComplete( P3P_ASYNC_READ,
                                NS_OK );

      if (NS_SUCCEEDED( rv ) && !mProcessComplete) {
        // The inline content needed some other non-inline files
        rv = mReadRequests->AppendElement( pReadRequest ) ? NS_OK : NS_ERROR_FAILURE;

        if (NS_SUCCEEDED( rv )) {
          rv = P3P_PRIVACY_IN_PROGRESS;
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PXMLProcessor:  %s AsyncRead, (InlineRead) mReadRequests->AppendElement failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
    }
    else if (mUseDOMParser) {
      // Local request (such as local copy of P3P Base DataSchema for performance)
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PXMLProcessor:  %s AsyncRead, local read requested.\n", (const char *)mcsURISpec) );

      rv = LocalRead( P3P_ASYNC_READ );

      if (NS_SUCCEEDED( rv ) && !mProcessComplete) {
        // The local file needed some other non-local files
        rv = mReadRequests->AppendElement( pReadRequest ) ? NS_OK : NS_ERROR_FAILURE;

        if (NS_SUCCEEDED( rv )) {
          rv = P3P_PRIVACY_IN_PROGRESS;
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PXMLProcessor:  %s AsyncRead, (UseDOMParser) mReadRequests->AppendElement failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
    }
    else {
      // Network request
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PXMLProcessor:  %s AsyncRead, network read requested.\n", (const char *)mcsURISpec) );

      rv = mReadRequests->AppendElement( pReadRequest ) ? NS_OK : NS_ERROR_FAILURE;

      if (NS_SUCCEEDED( rv )) {

        if (!mReadInProgress) {
          // Read the XML file
#ifdef DEBUG_P3P
          { printf( "P3P:  Initiating read for %s (read as %s)\n", (const char *)mcsURISpec, (const char *)mcsReadURISpec );
          }
#endif

          mReadInProgress  = PR_TRUE;

          mXMLHttpRequest = do_CreateInstance( NS_XMLHTTPREQUEST_CONTRACTID,
                                              &rv );

          if (NS_SUCCEEDED( rv )) {
            rv = mXMLHttpRequest->OpenRequest( "GET",
                                               (const char *)mcsReadURISpec,
                                               PR_TRUE,
                                               nsnull, nsnull );

            if (NS_SUCCEEDED( rv )) {
              rv = RegisterHTTPNotify( );

              if (NS_SUCCEEDED( rv )) {
                rv = AddDOMEventListener( );

                if (NS_SUCCEEDED( rv )) {
                  rv = mXMLHttpRequest->Send( nsnull );

                  if (NS_SUCCEEDED( rv )) {
                    rv = P3P_PRIVACY_IN_PROGRESS;
                  }
                  else {
                    PR_LOG( gP3PLogModule,
                            PR_LOG_ERROR,
                            ("P3PXMLProcessor:  %s AsyncRead, mXMLHttpRequest->Send failed - %X.\n", (const char *)mcsURISpec, rv) );
                  }
                }
              }
            }
            else {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PXMLProcessor:  %s AsyncRead, mXMLHttpRequest->OpenRequest failed - %X.\n", (const char *)mcsURISpec, rv) );
            }
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PXMLProcessor:  %s AsyncRead, do_CreateInstance of XMLHttpRequest failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
        else {
#ifdef DEBUG_P3P
          { printf( "P3P:  Read of %s is already in progress (as %s)\n", (const char *)mcsURISpec, (const char *)mcsReadURISpec );
          }
#endif

          PR_LOG( gP3PLogModule,
                  PR_LOG_NOTICE,
                  ("P3PXMLProcessor:  %s AsyncRead, read already in progress.\n", (const char *)mcsURISpec) );

          rv = P3P_PRIVACY_IN_PROGRESS;
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PXMLProcessor:  %s AsyncRead, (AsyncRead) mReadRequests->AppendElement failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s AsyncRead, NS_NewP3PXMLProcessorReadRequest failed - %X.\n", (const char *)mcsURISpec, rv) );
  }


  return rv;
}

// P3P XML Processor: SyncRead
//
// Function:  Performs an synchronous read of the URI.
//
// Parms:     None
//
NS_METHOD
nsP3PXMLProcessor::SyncRead( ) {

  nsresult                 rv = NS_OK;

  nsCOMPtr<nsIDOMElement>  pDOMElement;


  if (mInlineRead) {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PXMLProcessor:  %s SyncRead, inline read requested.\n", (const char *)mcsURISpec) );

    // Data contained inline with a document already read in and parsed, process it
    rv = ProcessReadComplete( P3P_SYNC_READ,
                              NS_OK );
  }
  else if (mUseDOMParser) {
    // Local request (such as P3P Base DataSchema for performance)
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PXMLProcessor:  %s SyncRead, local read requested.\n", (const char *)mcsURISpec) );

    rv = LocalRead( P3P_SYNC_READ );
  }
  else {
    // Network request
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PXMLProcessor:  %s SyncRead, network read requested.\n", (const char *)mcsURISpec) );

    // Create the XML Http Request
    mXMLHttpRequest = do_CreateInstance( NS_XMLHTTPREQUEST_CONTRACTID,
                                        &rv );

    if (NS_SUCCEEDED( rv )) {
      // Set the request parameters
      rv = mXMLHttpRequest->OpenRequest( "GET",
                                         (const char *)mcsReadURISpec,
                                         PR_FALSE,
                                         nsnull, nsnull );

      if (NS_SUCCEEDED( rv )) {
        // Register the HTTPNotify listeners
        rv = RegisterHTTPNotify( );

        if (NS_SUCCEEDED( rv )) {
          // Send the XML Http Request
          rv = mXMLHttpRequest->Send( nsnull );

          if (NS_FAILED( rv )) {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PXMLProcessor:  %s LocalRead, mXMLHttpRequest->Send failed - %X.\n", (const char *)mcsURISpec, rv) );
          }

          // Process the data
          rv = ProcessReadComplete( P3P_SYNC_READ,
                                    rv );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PXMLProcessor:  %s SyncRead, mXMLHttpRequest->OpenRequest failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PXMLProcessor:  %s SyncRead, do_CreateInstance of XMLHttpRequest failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  return rv;
}

// P3P XML Processor: LocalRead
//
// Function:  Performs an local read of a URI using the DOM Parser.
//
// Parms:     None
//
NS_METHOD
nsP3PXMLProcessor::LocalRead( PRInt32   aReadType ) {

  nsresult                  rv;

  nsCOMPtr<nsIURI>          pURI;

  nsCOMPtr<nsIChannel>      pChannel;

  nsCOMPtr<nsIInputStream>  pInputStream;

  PRUint32                  uiContentLength;

  nsCOMPtr<nsIDOMParser>    pDOMParser;


  // Create a URI object
  rv = NS_NewURI( getter_AddRefs( pURI ),
    (const char *)mcsReadURISpec );

  if (NS_SUCCEEDED( rv )) {
    // Create a channel for the URI
    rv = NS_OpenURI( getter_AddRefs( pChannel ),
                     pURI,
                     nsnull,
                     nsnull );

    if (NS_SUCCEEDED( rv )) {
      // Obtain the channel's input stream
      rv = pChannel->Open( getter_AddRefs( pInputStream ) );

      if (NS_SUCCEEDED( rv )) {
        rv = pInputStream->Available(&uiContentLength );

        if (NS_SUCCEEDED( rv )) {
          // Create a DOMParser object
          pDOMParser = do_CreateInstance( NS_DOMPARSER_CONTRACTID,
                                         &rv );

          if (NS_SUCCEEDED( rv )) {
            // Set the URI associated with the document
            rv = pDOMParser->SetBaseURI( pURI );

            if (NS_SUCCEEDED( rv )) {
              // Parse the input stream
              rv = pDOMParser->ParseFromStream( pInputStream,
                                                "UTF-8",
                                                uiContentLength,
                                                "text/xml",
                                                getter_AddRefs( mDOMDocument ) );

              if (NS_FAILED( rv )) {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PXMLProcessor:  %s LocalRead, pDOMParser->ParseFromStream failed - %X.\n", (const char *)mcsURISpec, rv) );
              }

              // Process the data
              rv = ProcessReadComplete( aReadType,
                                        rv );
            }
            else {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PXMLProcessor:  %s LocalRead, pDOMParser->SetBaseURI failed - %X.\n", (const char *)mcsURISpec, rv) );
            }
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PXMLProcessor:  %s LocalRead, do_CreateInstance of DOMParser failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PXMLProcessor:  %s LocalRead, pInputStream->Available failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PXMLProcessor:  %s LocalRead, pChannel->OpenInputStream failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PXMLProcessor:  %s LocalRead, NS_OpenURI failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s LocalRead, NS_NewURI failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P XML Processor: RegisterHTTPNotify
//
// Function:  Registers the HTTP Notify functions.
//
// Parms:     None
//
NS_METHOD
nsP3PXMLProcessor::RegisterHTTPNotify( ) {

  nsresult  rv;


  rv = NS_NewP3PXMLProcessorHTTPNotify( this,
                                        getter_AddRefs( mHTTPNotify ) );

  if (NS_SUCCEEDED( rv )) {
    // Register this object to listen for request headers
    rv = mNetModuleMgr->RegisterModule( NS_NETWORK_MODULE_MANAGER_HTTP_REQUEST_CONTRACTID,
                                        mHTTPNotify );

    if (NS_SUCCEEDED( rv )) {
      // Register this object to listen for response headers
      rv = mNetModuleMgr->RegisterModule( NS_NETWORK_MODULE_MANAGER_HTTP_RESPONSE_CONTRACTID,
                                          mHTTPNotify );

      if (NS_FAILED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PXMLProcessor:  %s RegisterHTTPNotify, mNetModuleMgr->RegisterModule for responses failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PXMLProcessor:  %s RegisterHTTPNotify, mNetModuleMgr->RegisterModule for requests failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s RegisterHTTPNotify, NS_NewP3PXMLProcessorHTTPNotify failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P XML Processor: UnregisterHTTPNotify
//
// Function:  Unregisters the HTTP Notify functions.
//
// Parms:     None
//
NS_METHOD_( void )
nsP3PXMLProcessor::UnregisterHTTPNotify( ) {

  nsresult  rv;


  // Unregister this object to listen for request headers
  rv = mNetModuleMgr->UnregisterModule( NS_NETWORK_MODULE_MANAGER_HTTP_REQUEST_CONTRACTID,
                                        mHTTPNotify );

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s UnregisterHTTPNotify, mNetModuleMgr->UnregisterModule for requests failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  // Unregister this object to listen for response headers
  rv = mNetModuleMgr->UnregisterModule( NS_NETWORK_MODULE_MANAGER_HTTP_RESPONSE_CONTRACTID,
                                        mHTTPNotify );

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s UnregisterHTTPNotify, mNetModuleMgr->UnregisterModule for responses failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  mHTTPNotify = nsnull;

  return;
}

// P3P XML Processor: AddDOMEventListener
//
// Function:  Adds the DOM Event listener.
//
// Parms:     None
//
NS_METHOD
nsP3PXMLProcessor::AddDOMEventListener( ) {

  nsresult  rv;


  rv = NS_NewP3PXMLProcessorDOMEventListener( this,
                                              getter_AddRefs( mDOMEventListener ) );

  if (NS_SUCCEEDED( rv )) {
    nsCOMPtr<nsIDOMEventTarget> pDOMEventTarget(do_QueryInterface(mXMLHttpRequest,&rv));

    if(NS_SUCCEEDED(rv)) {
      // Add the load event listener
      rv = pDOMEventTarget->AddEventListener( NS_LITERAL_STRING("load"),
                                              mDOMEventListener,
                                              PR_FALSE );

      if (NS_SUCCEEDED( rv )) {
        rv = pDOMEventTarget->AddEventListener( NS_LITERAL_STRING("error"),
                                                mDOMEventListener,
                                                PR_FALSE );

        if (NS_FAILED( rv )) {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PXMLProcessor:  %s AddDOMEventListener, pDOMEventTarget->AddEventListener for errors failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PXMLProcessor:  %s AddDOMEventListener, pDOMEventTarget->AddEventListener for loads failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s AddDOMEventListener, NS_NewP3PXMLProcessorHTTPNotify failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P XML Processor: RemoveDOMEventListener
//
// Function:  Removes the DOM Event listener.
//
// Parms:     None
//
NS_METHOD_( void )
nsP3PXMLProcessor::RemoveDOMEventListener( ) {

  nsresult  rv;
  
  nsCOMPtr<nsIDOMEventTarget> pDOMEventTarget(do_QueryInterface(mXMLHttpRequest,&rv));

  if(NS_SUCCEEDED(rv)) {
    // Remove the load event listener
    rv = pDOMEventTarget->RemoveEventListener( NS_LITERAL_STRING("load"),
                                               mDOMEventListener,
                                               PR_FALSE );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PXMLProcessor:  %s RemoveDOMEventListener, pDOMEventTarget->RemoveEventListener for loads failed - %X.\n", (const char *)mcsURISpec, rv) );
    }

    // Remove the error event listener
    rv = pDOMEventTarget->RemoveEventListener( NS_LITERAL_STRING("error"),
                                               mDOMEventListener,
                                               PR_FALSE );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PXMLProcessor:  %s RemoveDOMEventListener, pDOMEventTarget->RemoveEventListener for errors failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  mDOMEventListener = nsnull;

  return;
}

// P3P XML Processor: ClearSafeZoneHeaders
//
// Function:  Delete all request headers not allowed in the safe zone.
//
// Parms:     1. In     The HTTPChannel object of the request
//
NS_METHOD_( void )
nsP3PXMLProcessor::ClearSafeZoneHeaders( nsIHttpChannel  *aHTTPChannel ) {

  aHTTPChannel->SetRequestHeader( P3P_HTTP_USERAGENT_KEY,
                                  nsnull );

  aHTTPChannel->SetRequestHeader( P3P_HTTP_REFERER_KEY,
                                  nsnull );

  aHTTPChannel->SetRequestHeader( P3P_HTTP_COOKIE_KEY,
                                  nsnull );

  return;
}

// P3P XML Processor: RequestFreshCopy
//
// Function:  Set the request headers required to tell the server not to
//            use a cached copy
//
// Parms:     1. In     The HTTPChannel object of the request
//
NS_METHOD_( void )
nsP3PXMLProcessor::RequestFreshCopy( nsIHttpChannel  *aHTTPChannel ) {

  aHTTPChannel->SetRequestHeader( P3P_HTTP_PRAGMA_KEY,
                                  P3P_PRAGMA_NOCACHE );

  aHTTPChannel->SetRequestHeader( P3P_HTTP_CACHECONTROL_KEY,
                                  P3P_CACHECONTROL_NOCACHE );

  return;
}

// P3P XML Processor: ProcessReadComplete
//
// Function:  Performs the processing required when a read of a URI has completed.
//
// Parms:     1. In     The type of read performed
//            2. In     The result of the operation so far
//
NS_METHOD
nsP3PXMLProcessor::ProcessReadComplete( PRInt32    aReadType,
                                        nsresult   aReadResult ) {

  nsresult                 rv;

  nsCOMPtr<nsIDOMElement>  pDOMElement;


  if (!mUseDOMParser && !mInlineRead) {
    // Unregister the HTTPNotify listener
    UnregisterHTTPNotify( );

    if (aReadType == P3P_ASYNC_READ) {
      // Remove the DOMEvent listener
      RemoveDOMEventListener( );
    }

    GetCacheRelatedHeaders( );
  }

  // Set the passed in read result
  rv = aReadResult;

  if (NS_SUCCEEDED( rv )) {

    if (!mInlineRead) {

      if (!mDOMNode) {

        if (!mUseDOMParser) {
          // We don't have the DOMDocument yet
          rv = mXMLHttpRequest->GetResponseXML( getter_AddRefs( mDOMDocument ) );
        }

        if (NS_SUCCEEDED( rv )) {
          rv = mDOMDocument->GetDocumentElement( getter_AddRefs( pDOMElement ) );

          if (NS_SUCCEEDED( rv )) {

            if (pDOMElement) {
              // XML Document successfully read and parsed
              mDOMNode = do_QueryInterface( pDOMElement,
                                           &rv );
            }
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PXMLProcessor:  %s ProcessReadComplete, mDOMDocument->GetDocumentElement failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PXMLProcessor:  %s ProcessReadComplete, mXMLHttpRequest->GetResponseXML failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
    }

    if (NS_SUCCEEDED( rv )) {
      
      if (mDOMNode) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PXMLProcessor:  %s ProcessReadComplete, XML document successfully read.\n", (const char *)mcsURISpec) );

        mContentValid = PR_TRUE;

        ProcessContent( aReadType );
      }
      else {
#ifdef DEBUG_P3P
        { printf( "P3P:    No content for %s (read as %s)\n", (const char *)mcsURISpec, (const char *)mcsReadURISpec );
        }
#endif

        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PXMLProcessor:  %s ProcessReadComplete, XML content does not exist.\n", (const char *)mcsURISpec) );
      }
    }
  }

  CheckForComplete( );

  return rv;
}

// P3P XML Processor: GetCacheRelatedHeaders
//
// Function:  Retrieves the headers related to cache control.
//
// Parms:     None
//
NS_METHOD_( void )
nsP3PXMLProcessor::GetCacheRelatedHeaders( ) {

  nsresult        rv;

  nsXPIDLCString  xcsHeaderValue;


  rv = mXMLHttpRequest->GetResponseHeader( P3P_HTTP_DATE_KEY,
                                           getter_Copies( xcsHeaderValue ) );

  if (NS_SUCCEEDED( rv )) {
    mDateValue.AssignWithConversion((const char *)xcsHeaderValue );
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s GetCacheRelatedHeaders, mXMLHttpRequest->GetResponseHeader for \"Date\" failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  rv = mXMLHttpRequest->GetResponseHeader( P3P_HTTP_CACHECONTROL_KEY,
                                           getter_Copies( xcsHeaderValue ) );

  if (NS_SUCCEEDED( rv )) {
    mCacheControlValue.AssignWithConversion((const char *)xcsHeaderValue );
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s GetCacheRelatedHeaders, mXMLHttpRequest->GetResponseHeader for \"Cache-Control\" failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  rv = mXMLHttpRequest->GetResponseHeader( P3P_HTTP_EXPIRES_KEY,
                                           getter_Copies( xcsHeaderValue ) );

  if (NS_SUCCEEDED( rv )) {
    mExpiresValue.AssignWithConversion((const char *)xcsHeaderValue );
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s GetCacheRelatedHeaders, mXMLHttpRequest->GetResponseHeader for \"Expires\" failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  rv = mXMLHttpRequest->GetResponseHeader( P3P_HTTP_PRAGMA_KEY,
                                           getter_Copies( xcsHeaderValue ) );

  if (NS_SUCCEEDED( rv )) {
    mPragmaValue.AssignWithConversion((const char *)xcsHeaderValue );
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PXMLProcessor:  %s GetCacheRelatedHeaders, mXMLHttpRequest->GetResponseHeader for \"Pragma\" failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return;
}

// P3P XML Processor: NotifyReaders
//
// Function:  Causes the notification of all waiting readers (XML Listeners) that the URI processing is complete.
//
// Parms:     None
//
NS_METHOD_( void )
nsP3PXMLProcessor::NotifyReaders( nsISupportsArray  *aReadRequests ) {

  aReadRequests->EnumerateForwards( NotifyXMLProcessorReaders,
                                    nsnull );

  aReadRequests->Clear( );

  return;
}


// ****************************************************************************
// nsVoidArray Enumeration Functor
// ****************************************************************************

// P3P XML Processor: NotifyXMLProcessorReaders
//
// Function:  Notifies the XML Listener that the read of the URI is complete.
//
// Parms:     1. In     The XMLProcessorReadRequest object
//            2. In     Null
//
PRBool
NotifyXMLProcessorReaders( nsISupports  *aElement,
                           void         *aData ) {

  nsP3PXMLProcessorReadRequest *pXMLProcessorReadRequest = nsnull;


  pXMLProcessorReadRequest = (nsP3PXMLProcessorReadRequest *)aElement;

  if (pXMLProcessorReadRequest->mXMLListener) {
    pXMLProcessorReadRequest->mXMLListener->DocumentComplete( pXMLProcessorReadRequest->mURISpec,
                                                              pXMLProcessorReadRequest->mReaderData );
  }

  return PR_TRUE;
}


// ****************************************************************************
// nsP3PXMLProcessorReadRequest Implementation routines
// ****************************************************************************

// P3P XML Processor Read Request: nsISupports
NS_IMPL_ISUPPORTS1( nsP3PXMLProcessorReadRequest, nsISupports );

// P3P XML Processor Read Request: Creation routine
NS_METHOD
NS_NewP3PXMLProcessorReadRequest( nsString&           aURISpec,
                                  nsIP3PXMLListener  *aXMLListener,
                                  nsISupports        *aReaderData,
                                  nsISupports       **aXMLProcessorReadRequest ) {

  nsresult                      rv;

  nsP3PXMLProcessorReadRequest *pNewXMLProcessorReadRequest = nsnull;


  NS_ENSURE_ARG_POINTER( aXMLProcessorReadRequest );

  *aXMLProcessorReadRequest = nsnull;

  pNewXMLProcessorReadRequest = new nsP3PXMLProcessorReadRequest( );

  if (pNewXMLProcessorReadRequest) {
    NS_ADDREF( pNewXMLProcessorReadRequest );

    pNewXMLProcessorReadRequest->mURISpec     = aURISpec;
    pNewXMLProcessorReadRequest->mXMLListener = aXMLListener;
    pNewXMLProcessorReadRequest->mReaderData  = aReaderData;

    rv = pNewXMLProcessorReadRequest->QueryInterface( NS_GET_IID( nsISupports ),
                                             (void **)aXMLProcessorReadRequest );

    NS_RELEASE( pNewXMLProcessorReadRequest );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Unable to create XMLProcessorReadRequest object\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P XML Processor Read Request: Constructor
nsP3PXMLProcessorReadRequest::nsP3PXMLProcessorReadRequest( ) {

  NS_INIT_ISUPPORTS( );
}

// P3P XML Processor Read Request: Destructor
nsP3PXMLProcessorReadRequest::~nsP3PXMLProcessorReadRequest( ) {
}


// ****************************************************************************
// nsP3PXMLProcessorHTTPNotify Implementation routines
// ****************************************************************************

// P3P XML Processor HTTP Notify: nsISupports
NS_IMPL_ISUPPORTS2( nsP3PXMLProcessorHTTPNotify, nsIHttpNotify,
                                                 nsINetNotify );

// P3P XML Processor HTTP Notify: Creation routine
NS_METHOD
NS_NewP3PXMLProcessorHTTPNotify( nsP3PXMLProcessor  *aXMLProcessor,
                                 nsIHttpNotify     **aXMLProcessorHTTPNotify ) {

  nsresult                     rv;

  nsP3PXMLProcessorHTTPNotify *pNewXMLProcessorHTTPNotify = nsnull;


  NS_ENSURE_ARG_POINTER( aXMLProcessorHTTPNotify );

  *aXMLProcessorHTTPNotify = nsnull;

  pNewXMLProcessorHTTPNotify = new nsP3PXMLProcessorHTTPNotify( aXMLProcessor );

  if (pNewXMLProcessorHTTPNotify) {
    NS_ADDREF( pNewXMLProcessorHTTPNotify );

    rv = pNewXMLProcessorHTTPNotify->QueryInterface( NS_GET_IID( nsIHttpNotify ),
                                            (void **)aXMLProcessorHTTPNotify );

    NS_RELEASE( pNewXMLProcessorHTTPNotify );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Unable to create XMLProcessorHTTPNotify object\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P XML Processor HTTP Notify: Constructor
nsP3PXMLProcessorHTTPNotify::nsP3PXMLProcessorHTTPNotify( nsP3PXMLProcessor  *aXMLProcessor )
  : mXMLProcessor( aXMLProcessor ) {

  NS_INIT_ISUPPORTS( );

  NS_IF_ADDREF( mXMLProcessor );
}

// P3P XML Processor HTTP Notify: Destructor
nsP3PXMLProcessorHTTPNotify::~nsP3PXMLProcessorHTTPNotify( ) {

  NS_IF_RELEASE( mXMLProcessor );
}


// ****************************************************************************
// nsIHttpNotify routines
// ****************************************************************************

// P3P XML Processor HTTP Notify: OnModifyRequest
NS_IMETHODIMP
nsP3PXMLProcessorHTTPNotify::OnModifyRequest( nsIHttpChannel *aHttpChannel ) {

  mXMLProcessor->OnModifyRequest( aHttpChannel );

  return NS_OK;
}

// P3P XML Processor HTTP Notify: OnExamineResponse
NS_IMETHODIMP
nsP3PXMLProcessorHTTPNotify::OnExamineResponse( nsIHttpChannel *aHttpChannel ) {

  mXMLProcessor->OnExamineResponse( aHttpChannel );

  return NS_OK;
}


// ****************************************************************************
// nsP3PXMLProcessorDOMEventListener Implementation routines
// ****************************************************************************

// P3P XML Processor DOM Event Listener: nsISupports
NS_IMPL_ISUPPORTS1( nsP3PXMLProcessorDOMEventListener, nsIDOMEventListener );

// P3P XML Processor DOM Event Listener: Creation routine
NS_METHOD
NS_NewP3PXMLProcessorDOMEventListener( nsP3PXMLProcessor    *aXMLProcessor,
                                       nsIDOMEventListener **aXMLProcessorDOMEventListener ) {

  nsresult                           rv;

  nsP3PXMLProcessorDOMEventListener *pNewXMLProcessorDOMEventListener = nsnull;


  NS_ENSURE_ARG_POINTER( aXMLProcessorDOMEventListener );

  *aXMLProcessorDOMEventListener = nsnull;

  pNewXMLProcessorDOMEventListener = new nsP3PXMLProcessorDOMEventListener( aXMLProcessor );

  if (pNewXMLProcessorDOMEventListener) {
    NS_ADDREF( pNewXMLProcessorDOMEventListener );

    rv = pNewXMLProcessorDOMEventListener->QueryInterface( NS_GET_IID( nsIDOMEventListener ),
                                                  (void **)aXMLProcessorDOMEventListener );

    NS_RELEASE( pNewXMLProcessorDOMEventListener );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Unable to create XMLProcessorDOMEventListener object\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P XML Processor DOM Event Listener: Constructor
nsP3PXMLProcessorDOMEventListener::nsP3PXMLProcessorDOMEventListener( nsP3PXMLProcessor  *aXMLProcessor )
  : mXMLProcessor( aXMLProcessor ) {

  NS_INIT_ISUPPORTS( );

  NS_IF_ADDREF( mXMLProcessor );
}

// P3P XML Processor DOM Event Listener: Destructor
nsP3PXMLProcessorDOMEventListener::~nsP3PXMLProcessorDOMEventListener( ) {

  NS_IF_RELEASE( mXMLProcessor );
}


// ****************************************************************************
// nsIDOMEventListener routines
// ****************************************************************************

// P3P XML Processor DOM Event Listener: HandleEvent
NS_IMETHODIMP
nsP3PXMLProcessorDOMEventListener::HandleEvent( nsIDOMEvent  *aEvent ) {

  mXMLProcessor->HandleEvent( aEvent );

  return NS_OK;
}
