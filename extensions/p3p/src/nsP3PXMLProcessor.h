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

#ifndef nsP3PXMLProcessor_h__
#define nsP3PXMLProcessor_h__

#include "nsIP3PCService.h"
#include "nsIP3PTag.h"
#include "nsIP3PXMLListener.h"

#include <nsCOMPtr.h>
#include <nsISupports.h>
#include <nsIDOMEventListener.h>
#include <nsIHttpNotify.h>

#include <nsAutoLock.h>

#include <nsIXMLHttpRequest.h>

#include <nsIDOMDocument.h>
#include <nsIDOMNode.h>

#include <nsINetModuleMgr.h>
#include <nsIURI.h>
#include <nsIHTTPChannel.h>

#include <nsISupportsArray.h>

#include <nsString.h>

#include <prtime.h>


class nsP3PXMLProcessor : public nsISupports {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsP3PXMLProcessor methods
  nsP3PXMLProcessor( );
  virtual ~nsP3PXMLProcessor( );

  // Non-virtual functions
  NS_METHOD             Init( nsString&  aURI );

  NS_METHOD             Read( PRInt32             aReadType,
                              nsIURI             *aReadForURI,
                              nsIP3PXMLListener  *aXMLListener,
                              nsISupports        *aReaderData );

  NS_METHOD             InlineRead( nsIDOMNode         *aDOMNode,
                                    PRBool              aRedirected,
                                    PRTime              aExpirationTime,
                                    PRInt32             aReadType,
                                    nsIURI             *aReadForURI,
                                    nsIP3PXMLListener  *aXMLListener,
                                    nsISupports        *aReaderData );

  NS_METHOD             ModifyRequest( nsISupports  *aContext );

  NS_METHOD             AsyncExamineResponse( nsISupports  *aContext );

  NS_METHOD             HandleEvent( nsIDOMEvent  *aEvent );

  NS_METHOD_( void )    CalculateExpiration( PRInt32   aDefaultLifeTime );

  NS_METHOD_( void )    NotifyReaders( nsISupportsArray  *aReadRequests );

  // Virtual functions
  NS_IMETHOD            PostInit( nsString& aURISpec );

  NS_IMETHOD            PreRead( PRInt32   aReadType,
                                 nsIURI   *aReadForURI );

  NS_IMETHOD            ProcessContent( PRInt32   aReadType );

  NS_IMETHOD_( void )   CheckForComplete( );

protected:
  NS_METHOD             AsyncRead( nsIP3PXMLListener  *aXMLListener,
                                   nsISupports        *aReaderData );

  NS_METHOD             SyncRead( );

  NS_METHOD             LocalRead( PRInt32   aReadType );

  NS_METHOD             RegisterHTTPNotify( );

  NS_METHOD_( void )    UnregisterHTTPNotify( );

  NS_METHOD             AddDOMEventListener( );

  NS_METHOD_( void )    RemoveDOMEventListener( );

  NS_METHOD_( void )    ClearSafeZoneHeaders( nsIHTTPChannel  *aHTTPChannel );

  NS_METHOD_( void )    RequestFreshCopy( nsIHTTPChannel  *aHTTPChannel );

  NS_METHOD_( void )    GetCacheRelatedHeaders( );

  NS_METHOD             ProcessReadComplete( PRInt32    aReadType,
                                             nsresult   aReadResult );


  PRInt32                         mDocumentType;      // The type of document this represents (ie. Policy)

  PRBool                          mFirstUse;          // Indicator of whether the document has been used yet

  PRBool                          mP3PRelatedURIAdded,  // The URI has been added as a P3P related URI
                                  mReadInProgress,      // A read of the URI is in progress
                                  mProcessComplete;     // The URI read is complete

  PRLock                         *mLock;              // A lock to serialize on the read requests

  PRTime                          mTime;              // Time initialized

  nsCOMPtr<nsINetModuleMgr>       mNetModuleMgr;      // The Network Module Manager Service

  nsCOMPtr<nsIAtom>               mUserAgentHeader,   // Various request and response header types
                                  mRefererHeader,
                                  mCookieHeader,
                                  mCacheControlHeader,
                                  mPragmaHeader;

  nsString                        mDateValue,         // The value of the "Date" header
                                  mCacheControlValue, // The value of the "Cache-Control" header
                                  mExpiresValue,      // The value of the "Expires" header
                                  mPragmaValue;       // The value of the "Pragma" header

  PRBool                          mUseDOMParser;      // Indicator to use the DOM parser rather than XML HttpRequest

  PRBool                          mInlineRead;        // Indicator that DOMDocument input is already available

  nsString                        mURISpec;           // The URI of the XML document
  nsCString                       mcsURISpec;         // ... for logging
  nsString                        mReadURISpec;       // The Alternate URI to read (ie. local copy of base dataschema)
  nsCString                       mcsReadURISpec;     // ... for logging
  nsCOMPtr<nsIURI>                mURI;               // The URI object of the XML document (based on mURISpec)

  nsCOMPtr<nsIXMLHttpRequest>     mXMLHttpRequest;    // XML HttpRequest object
  nsCOMPtr<nsIDOMDocument>        mDOMDocument;       // XML DOMDocument object
  nsCOMPtr<nsIDOMNode>            mDOMNode;           // Root DOMNode object for the XML document
  nsCOMPtr<nsIP3PTag>             mTag;               // Root Tag object for the XML document

  nsCOMPtr<nsISupportsArray>      mReadRequests;      // A list of read requests

  nsCOMPtr<nsIP3PCService>        mP3PService;        // The P3P Service

  nsCOMPtr<nsIHTTPNotify>         mHTTPNotify;        // The HTTP Notify listener
  nsCOMPtr<nsIDOMEventListener>   mDOMEventListener;  // The DOM Event listener

  PRTime                          mExpirationTime;    // The date/time when the document expires

  PRBool                          mRedirected;        // An indicator as to whether the request has been redirected to another URI

  PRBool                          mContentValid,      // An indicator of the validity of the content
                                  mDocumentValid;     // An indicator of the validity of the document
};

class nsP3PXMLProcessorReadRequest : public nsISupports {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsP3PXMLProcessorReadRequest methods
  nsP3PXMLProcessorReadRequest( );
  virtual ~nsP3PXMLProcessorReadRequest( );

  nsString                        mURISpec;           // The URI spec of the XML Document
  nsCOMPtr<nsIP3PXMLListener>     mXMLListener;       // The XML Listener
  nsCOMPtr<nsISupports>           mReaderData;        // The XML Listener's data
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PXMLProcessorReadRequest( nsString&           aURISpec,
                                                          nsIP3PXMLListener  *aXMLListener,
                                                          nsISupports        *aReaderData,
                                                          nsISupports       **aXMLProcessorReadRequest );

PRBool                  NotifyXMLProcessorReaders( nsISupports  *aElement,
                                                   void         *aData );


class nsP3PXMLProcessorHTTPNotify : public nsIHTTPNotify {
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIHTTPNotify methods
  NS_DECL_NSIHTTPNOTIFY

  // nsP3PXMLProcessorReadRequest methods
  nsP3PXMLProcessorHTTPNotify( nsP3PXMLProcessor  *aXMLProcessor );
  virtual ~nsP3PXMLProcessorHTTPNotify( );

protected:
  nsP3PXMLProcessor    *mXMLProcessor;                // The XMLProcessor object that created this object
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PXMLProcessorHTTPNotify( nsP3PXMLProcessor  *aXMLProcessor,
                                                         nsIHTTPNotify     **aXMLProcessorHTTPNotify );


class nsP3PXMLProcessorDOMEventListener : public nsIDOMEventListener {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMEventListener
  nsresult              HandleEvent( nsIDOMEvent  *aEvent );

  // nsP3PXMLProcessorReadRequest methods
  nsP3PXMLProcessorDOMEventListener( nsP3PXMLProcessor  *aXMLProcessor );
  virtual ~nsP3PXMLProcessorDOMEventListener( );

protected:
  nsP3PXMLProcessor    *mXMLProcessor;                // The XMLProcessor object that created this object
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PXMLProcessorDOMEventListener( nsP3PXMLProcessor    *aXMLProcessor,
                                                               nsIDOMEventListener **aXMLProcessorDOMEventListener );

#endif                                           /* nsP3XMLProcessor_h__      */
