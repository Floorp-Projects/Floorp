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

#ifndef nsIP3PCService_h__
#define nsIP3PCService_h__

#include "nsIP3PXMLListener.h"
#include "nsIP3PTag.h"
#include "nsIP3PDataStruct.h"

#include <nsCOMPtr.h>
#include <nsIP3PService.h>

#include <nsIURI.h>
#include <nsIDocShellTreeItem.h>
#include <nsIDOMWindowInternal.h>
#include <nsIContent.h>

#include <nsIRDFDataSource.h>

#include <nsString.h>
#include <nsVoidArray.h>


#define NS_IP3PCSERVICE_IID_STR "31430e56-d43d-11d3-9781-002035aee991"
#define NS_IP3PCSERVICE_IID     {0x31430e56, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

class nsIP3PURIInformation;
class nsIP3PPolicyRefFile;
class nsIP3PReference;
class nsIP3PPolicy;
class nsIP3PDataSchema;
class nsIP3PPrivacyResult;

// Class: nsIP3PCService
//
//        This class represents the P3P Service interfaces that are only to be exposed
//        to C++ callers (ie. not JavaScript)
//
class nsIP3PCService : public nsIP3PService {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR( NS_IP3PCSERVICE_IID );

// Function:  Notification of the enabling of P3P (preference change).
//
// Parms:     None
  NS_IMETHOD            Enabled( ) = 0;

// Function:  Notification of the disabling of P3P (preference change).
//
// Parms:     None
  NS_IMETHOD            Disabled( ) = 0;

// Function:  Notification that a request to load a page has been initiated by the user.
//
// Parms:     1. In     The URI being loaded
//            2. In     The DocShellTreeItem that represents the browser UI instance
//            3. In     The DocShellTreeItem where the URI is being loaded (possibly a frame)
  NS_IMETHOD            LoadingObject( nsIURI               *aLoadingURI,
                                       nsIDocShellTreeItem  *aDocShellTreeItemMain,
                                       nsIDocShellTreeItem  *aDocShellTreeItemCurrent,
                                       PRBool                aInitialLoad ) = 0;

// Function:  Modifies the URI associated with a PrivacyResult object (used for redirections, etc.)
//
// Parms:     1. In     The original URI
//            2. In     The new URI
//            3. In     The DocShellTreeItem where the URI is being loaded
//
  NS_IMETHOD            ModifyPrivacyResult( nsIURI               *aOriginalURI,
                                             nsIURI               *aNewURI,
                                             nsIDocShellTreeItem  *aDocShellTreeItem ) = 0;

// Function:  Notification that a document has been completed in a browser window instance.
//
// Parms:     1. In     The DocShellTreeItem that represents the browser UI instance
//
  NS_IMETHOD            DocumentComplete( nsIDocShellTreeItem  *aDocShellTreeItemMain ) = 0;

// Function:  Notification that a browser window instance is closing.
//
// Parms:     1. In     The DocShellTreeItem that represents the browser UI instance
//
  NS_IMETHOD            ClosingBrowserWindow( nsIDocShellTreeItem  *aDocShellTreeItemMain ) = 0;

// Function:  Set/Save the referer information associated with a URI.
//
// Parms:     1. In     The DocShellTreeItem that the URI is associated with
//            2. In     The URI being loaded
//            2. In     The "Referer" response header value
  NS_IMETHOD            SetRefererHeader( nsIDocShellTreeItem  *aDocShellTreeItem,
                                          nsIURI               *aReferencingURI,
                                          nsString&             aRefererHeader ) = 0;

// Function:  Set the URI spec of the PolicyRefFile specified.
//
// Parms:     1. In     The DocShellTreeItem that the URI is associated with
//            2. In     The URI that has specified the PolicyRefFile
//            3. In     The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            4. In     The URI spec of the PolicyRefFile
  NS_IMETHOD            SetPolicyRefFileURISpec( nsIDocShellTreeItem  *aDocShellTreeItem,
                                                 nsIURI               *aReferencingURI,
                                                 PRInt32               aReferencePoint,
                                                 nsString&             aPolicyRefFileURISpec ) = 0;

// Function:  Set/Save the cookie information associated with a URI.
//
// Parms:     1. In     The DocShellTreeItem that the URI is associated with
//            2. In     The URI being loaded
//            2. In     The "Set-Cookie" response header value
  NS_IMETHOD            SetCookieHeader( nsIDocShellTreeItem  *aDocShellTreeItem,
                                         nsIURI               *aReferencingURI,
                                         nsString&             aCookieHeader ) = 0;

// Function:  Check the Privacy of a URI (an internet resource).
//
// Parms:     1. In     The method used on the URI (corresponds to the Policy Ref File <METHOD>)
//            2. In     The URI to check
//            3. In     The DocShellTreeItem that the URI is associated with
//            4. In     The data to be used during processing because of asynchronous read events
//                      that result in multiple calls to this function
  NS_IMETHOD            CheckPrivacy( nsString&             aMethod,
                                      nsIURI               *aCheckURI,
                                      nsIDocShellTreeItem  *aDocShellTreeItem,
                                      nsISupports          *aReadData ) = 0;

// Function:  Check the Privacy of the form submission URI target.
//
// Parms:     1. In     The URI of the submission
//            2. In     The DOMWindow associated with the form submission
//            3. In     The Content of the <FORM>
//            4. Out    The indicator of whether to cancel the form submission
  NS_IMETHOD            CheckPrivacyFormSubmit( nsIURI                *aCheckURI,
                                                nsIDOMWindowInternal  *aDOMWindowInternal,
                                                nsIContent            *aFormContent,
                                                PRBool                *aCancelSubmit ) = 0;

// Function:  Get a cached Policy object.
//
// Parms:     1. In     The URI spec of the Policy
//            2. Out    The Policy object (null if the Policy is not cached)
  NS_IMETHOD            GetCachedPolicy( nsString&      aPolicyURISpec,
                                         nsIP3PPolicy **aPolicy ) = 0;

// Function:  Cache a Policy object.
//
// Parms:     1. In     The URI spec of the Policy
//            2. In     The Policy object to cache
  NS_IMETHOD_( void )   CachePolicy( nsString&      aPolicyURISpec,
                                     nsIP3PPolicy  *aPolicy ) = 0;

// Function:  Remove a Policy object from the cache.
//
// Parms:     1. In     The URI spec of the Policy
//            2. In     The Policy object to remove from the cache
  NS_IMETHOD_( void )   UncachePolicy( nsString&      aPolicyURISpec,
                                       nsIP3PPolicy  *aPolicy ) = 0;

// Function:  Get a cached DataSchema object.
//
// Parms:     1. In     The URI spec of the DataSchema
//            2. Out    The DataSchema object (null if the DataSchema is not cached)
  NS_IMETHOD            GetCachedDataSchema( nsString&          aDataSchemaURISpec,
                                             nsIP3PDataSchema **aDataSchema ) = 0;

// Function:  Cache a DataSchema object.
//
// Parms:     1. In     The URI spec of the DataSchema
//            2. In     The DataSchema object to cache
  NS_IMETHOD_( void )   CacheDataSchema( nsString&          aDataSchemaURISpec,
                                         nsIP3PDataSchema  *aDataSchema ) = 0;

// Function:  Remove a DataSchema object from the cache.
//
// Parms:     1. In     The URI spec of the DataSchema
//            2. In     The DataSchema object to remove from the cache
  NS_IMETHOD_( void )   UncacheDataSchema( nsString&          aDataSchemaURISpec,
                                           nsIP3PDataSchema  *aDataSchema ) = 0;

// Function:  Get the RDF description of the Privacy information
//
// Parms:     1. In     The DocShellTreeItem (browser instance) corresponding to the Privacy information to obtain
//            2. Out    The RDFDataSource describing the Privacy information
  NS_IMETHOD            GetPrivacyInfo( nsIDocShellTreeItem  *aDocShellTreeItem,
                                        nsIRDFDataSource    **aDataSource ) = 0;

// Function:  Determines if the URI represents the HTTP protocol
//
// Parms:     1. In     The URI to check
  NS_IMETHOD_( PRBool ) IsHTTPProtocol( nsIURI  *aURI ) = 0;

// Function:  Adds a URI spec to the list of P3P related URIs
//
// Parms:     1. In     The P3P related URI spec
  NS_IMETHOD_( void )   AddP3PRelatedURISpec( nsString&  aP3PRelatedURISpec ) = 0;

// Function:  Removes a URI spec from the list of P3P related URIs
//
// Parms:     1. In     The P3P related URI spec
  NS_IMETHOD_( void )   RemoveP3PRelatedURISpec( nsString&  aP3PRelatedURISpec ) = 0;

// Function:  Checks if the URI is in the list of P3P related URIs
//
// Parms:     1. In     The URI
  NS_IMETHOD_( PRBool ) IsP3PRelatedURI( nsIURI  *aURI ) = 0;

// Function:  Returns the time the P3P preferences were last changed.
//
// Parms:     1. Out    The time the P3P preferences were last changed
  NS_IMETHOD            GetTimePrefsLastChanged( PRTime  *aTime ) = 0;

// Function:  Checks if the preferences have changed since a given time.
//
// Parms:     1. In     The time to perform the check against
  NS_IMETHOD_( PRBool ) PrefsChanged( PRTime   aTime ) = 0;

// Function:  Return the value of the boolean preference combination.
//
// Parms:     1. In     The Policy <PURPOSE>
//            2. In     The Policy <RECIPIENT>
//            3. In     The Policy <CATEGORY>
//            4. Out    The indicator as to whether the boolean preference combination is set
  NS_IMETHOD            GetBoolPref( nsString&  aPurposeValue,
                                     nsString&  aRecipientValue,
                                     nsString&  aCategoryValue,
                                     PRBool    *aResult ) = 0;

// Function:  Function pair to return a string from a string bundle.
//
// Parms:     1. In     The key used to locate the string within the string bundle
//            2. Out    The string contained in the string bundle
  NS_IMETHOD            GetLocaleString( const char  *aKey,
                                         nsString&    aValue ) = 0;
  NS_IMETHOD            GetLocaleString( nsString&  aKey,
                                         nsString&  aValue ) = 0;
};

#define NS_DECL_NSIP3PCSERVICE                                                                  \
  NS_IMETHOD            Enabled( );                                                             \
  NS_IMETHOD            Disabled( );                                                            \
  NS_IMETHOD            LoadingObject( nsIURI               *aLoadingURI,                       \
                                       nsIDocShellTreeItem  *aDocShellTreeItemMain,             \
                                       nsIDocShellTreeItem  *aDocShellTreeItemCurrent,          \
                                       PRBool                aInitialLoad );                    \
  NS_IMETHOD            ModifyPrivacyResult( nsIURI               *aOriginalURI,                \
                                             nsIURI               *aNewURI,                     \
                                             nsIDocShellTreeItem  *aDocShellTreeItem );         \
  NS_IMETHOD            DocumentComplete( nsIDocShellTreeItem  *aDocShellTreeItemMain );        \
  NS_IMETHOD            ClosingBrowserWindow( nsIDocShellTreeItem  *aDocShellTreeItemMain );    \
  NS_IMETHOD            SetRefererHeader( nsIDocShellTreeItem  *aDocShellTreeItem,              \
                                          nsIURI               *aReferencingURI,                \
                                          nsString&             aRefererHeader );               \
  NS_IMETHOD            SetPolicyRefFileURISpec( nsIDocShellTreeItem  *aDocShellTreeItem,       \
                                                 nsIURI               *aReferencingURI,         \
                                                 PRInt32               aReferencePoint,         \
                                                 nsString&             aPolicyRefFileURISpec ); \
  NS_IMETHOD            SetCookieHeader( nsIDocShellTreeItem  *aDocShellTreeItem,               \
                                         nsIURI               *aReferencingURI,                 \
                                         nsString&             aCookieHeader );                 \
  NS_IMETHOD            CheckPrivacy( nsString&             aMethod,                            \
                                      nsIURI               *aCheckURI,                          \
                                      nsIDocShellTreeItem  *aDocShellTreeItem,                  \
                                      nsISupports          *aReadData );                        \
  NS_IMETHOD            CheckPrivacyFormSubmit( nsIURI                *aCheckURI,               \
                                                nsIDOMWindowInternal  *aDOMWindowInternal,      \
                                                nsIContent            *aFormContent,            \
                                                PRBool                *aCancelSubmit );         \
  NS_IMETHOD            GetCachedPolicy( nsString&      aPolicyURISpec,                         \
                                         nsIP3PPolicy **aPolicy );                              \
  NS_IMETHOD_( void )   CachePolicy( nsString&      aPolicyURISpec,                             \
                                     nsIP3PPolicy  *aPolicy );                                  \
  NS_IMETHOD_( void )   UncachePolicy( nsString&      aPolicyURISpec,                           \
                                       nsIP3PPolicy  *aPolicy );                                \
  NS_IMETHOD            GetCachedDataSchema( nsString&          aDataSchemaURISpec,             \
                                             nsIP3PDataSchema **aDataSchema );                  \
  NS_IMETHOD_( void )   CacheDataSchema( nsString&          aDataSchemaURISpec,                 \
                                         nsIP3PDataSchema  *aDataSchema );                      \
  NS_IMETHOD_( void )   UncacheDataSchema( nsString&          aDataSchemaURISpec,               \
                                           nsIP3PDataSchema  *aDataSchema );                    \
  NS_IMETHOD            GetPrivacyInfo( nsIDocShellTreeItem  *aDocShellTreeItem,                \
                                        nsIRDFDataSource    **aDataSource );                    \
  NS_IMETHOD_( PRBool ) IsHTTPProtocol( nsIURI  *aURI );                                        \
  NS_IMETHOD_( void )   AddP3PRelatedURISpec( nsString&  aP3PRelatedURISpec );                  \
  NS_IMETHOD_( void )   RemoveP3PRelatedURISpec( nsString&  aP3PRelatedURISpec );               \
  NS_IMETHOD_( PRBool ) IsP3PRelatedURI( nsIURI  *aURI );                                       \
  NS_IMETHOD            GetTimePrefsLastChanged( PRTime  *aTime );                              \
  NS_IMETHOD_( PRBool ) PrefsChanged( PRTime   aTime );                                         \
  NS_IMETHOD            GetBoolPref( nsString&  aCategoryValue,                                 \
                                     nsString&  aPurposeValue,                                  \
                                     nsString&  aRecipientValue,                                \
                                     PRBool    *aResult );                                      \
  NS_IMETHOD            GetLocaleString( const char  *aKey,                                     \
                                         nsString&    aValue );                                 \
  NS_IMETHOD            GetLocaleString( nsString&  aKey,                                       \
                                         nsString&  aValue );

#endif                                           /* nsP3PCService_h__         */
