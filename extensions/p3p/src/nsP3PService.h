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

#ifndef nsP3PService_h__
#define nsP3PService_h__

#include "nsIP3PCService.h"
#include "nsIP3PPolicyRefFile.h"
#include "nsIP3PPolicy.h"
#include "nsIP3PPrivacyResult.h"
#include "nsIP3PUIService.h"
#include "nsIP3PXMLListener.h"
#include "nsIP3PTag.h"
#include "nsIP3PPreferences.h"

#include <nsCOMPtr.h>
#include <nsISupports.h>
#include <nsIComponentManager.h>

#include <nsICategoryManager.h>

#include <nsICookieService.h>

#include <nsIObserver.h>

#include <nsIHTTPChannel.h>
#include <nsIHttpNotify.h>

#include <nsDeque.h>
#include <nsHashtable.h>

#include <nsIStringBundle.h>

#include <prlog.h>

class nsP3PServiceReadData;


class nsP3PService : public nsIP3PCService,
                     public nsIP3PXMLListener {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIP3PService methods
  NS_DECL_NSIP3PSERVICE

  // nsIP3PCService methods
  NS_DECL_NSIP3PCSERVICE

  // nsIP3PService methods
  NS_DECL_NSIP3PXMLLISTENER

  // nsP3PService methods
  nsP3PService( );
  virtual ~nsP3PService( );

  static
  NS_METHOD             Create( nsISupports  *aOuter,
                                REFNSIID      aIID,
                                void        **aResult );

  NS_METHOD             Init( );

  static
  NS_METHOD             CategoryRegister( nsIComponentManager  *aComponentManager,
                                          nsIFile              *aPath,
                                          const char           *aRegistryLocation,
                                          const char           *aComponentType );
  static
  NS_METHOD             CategoryUnregister( nsIComponentManager  *aComponentManager,
                                            nsIFile              *aPath,
                                            const char           *aRegistryLocation );

protected:
  NS_METHOD             GetDocShellTreeItemMain( nsIDocShellTreeItem  *aDocShellTreeItem,
                                                 nsIDocShellTreeItem **aDocShellTreeItemMain );

  NS_METHOD_( PRBool )  IsBrowserWindow( nsIDocShellTreeItem  *aDocShellTreeItemMain );

  NS_METHOD             GetURIComponents( nsIURI    *aURI,
                                          nsString&  aURISpec,
                                          nsString&  aURIScheme,
                                          nsString&  aURIHostPort,
                                          nsString&  aURIPath );

  NS_METHOD             GetURIInformation( nsIDocShellTreeItem   *aDocShellTreeItem,
                                           nsString&              aReferencingURISpec,
                                           nsIP3PURIInformation **aURIInformation );
  NS_METHOD             CreateURIInformation( nsIDocShellTreeItem   *aDocShellTreeItem,
                                              nsString&              aReferencingURISpec,
                                              nsIP3PURIInformation **aURIInformation );

  NS_METHOD             GetReference( nsString&         aURIHostPort,
                                      nsIP3PReference **aReference );
  NS_METHOD             CreateReference( nsString&         aURIHostPort,
                                         nsIP3PReference **aReference );

  NS_METHOD             CheckForEmbedding( nsP3PServiceReadData  *aReadData,
                                           nsIURI               **aEmbeddingURI );

  NS_METHOD             PrivacyStateMachine( nsP3PServiceReadData  *aReadData,
                                             PRBool                 aEmbedCheck,
                                             PRInt32                aReadType,
                                             PRInt32                aCheckType );

  NS_METHOD             GetWellKnownLocationPolicyRefFile( nsIURI                *aBaseURI,
                                                           nsString&              aURIHostPort,
                                                           nsP3PServiceReadData  *aReadData,
                                                           PRInt32                aReadType );

  NS_METHOD             GetSpecifiedPolicyRefFile( nsString&              aURISpec,
                                                   nsString&              aURIHostPort,
                                                   nsP3PServiceReadData  *aReadData,
                                                   PRInt32                aReadType );

  NS_METHOD             GetPolicyRefFile( nsString&              aURIHostPort,
                                          nsString&              aPolicyRefFileURISpec,
                                          PRInt32                aReferencePoint,
                                          nsP3PServiceReadData  *aReadData,
                                          PRInt32                aReadType );

  NS_METHOD             ReplacePolicyRefFile( nsString&  aURIHostPort,
                                              nsString&  aPolicyRefFileURISpec,
                                              PRInt32    aReferencePoint );

  NS_METHOD             GetPolicy( PRBool         aEmbedCheck,
                                   nsString&      aURIHostPort,
                                   nsString&      aURIMethod,
                                   nsString&      aURIPath,
                                   nsString&      aReadingPolicyRefFileURISpec,
                                   PRInt32&       aReadingReferencePoint,
                                   nsString&      aExpiredPolicyRefFileURISpec,
                                   PRInt32&       aExpiredReferencePoint,
                                   nsIP3PPolicy **aPolicy );

  NS_METHOD             SetPrivacyResult( nsIURI               *aCheckURI,
                                          nsIDocShellTreeItem  *aDocShellTreeItem,
                                          nsresult              aPrivacyResult,
                                          nsIP3PPolicy         *aPolicy );

  NS_METHOD             GetDocShellTreeItemURI( nsIDocShellTreeItem  *aDocShellTreeItem,
                                                nsIURI              **aURI );

  NS_METHOD             DeleteCookies( nsIDocShellTreeItem  *aDocShellTreeItem,
                                       nsString&             aURISpec,
                                       nsIURI               *aURI );
  NS_METHOD             DeleteCookie( nsIURI    *aURI,
                                      nsString&  aSetCookieValue );

  NS_METHOD_( void )    UpdateBrowserWindow( nsIDocShellTreeItem  *aDocShellTreeItemMain,
                                             nsresult              aPrivacyResult );

  NS_METHOD             GetFormRequestMethod( nsIContent  *aFormContent,
                                              nsString&    aURIMethod );


  PRBool                          mInitialized;       // Indicator that the P3P Service has initialized

  char                            mGMTStartTime[256]; // The time the P3P Service started

  nsCOMPtr<nsICookieService>      mCookieService;     // The Cookie Service

  nsCOMPtr<nsIP3PPreferences>     mPreferences;       // The Preference Service

  nsCOMPtr<nsIP3PUIService>       mUIService;         // The UI Service

  nsCOMPtr<nsIStringBundle>       mStringBundle;      // The String Bundle

  nsHashtable                     mP3PRelatedURISpecs;// Table of URI specs related to P3P (Policy, DataSchema, etc.)

  nsSupportsHashtable             mReferences;        // Reference object cache
  nsSupportsHashtable             mPolicies;          // Policy object cache
  nsSupportsHashtable             mDataSchemas;       // DataSchema object cache

  nsSupportsHashtable             mBrowserWindowMap;     // "Main" DocShellTreeItem object to DocShellTreeItemData object mapping

  nsSupportsHashtable             mBrowserWindowBusyMap; // "Busy" DocShellTreeItem object to BrowserWindowData object mapping

  nsCOMPtr<nsIHTTPNotify>         mHTTPNotify;        // The HTTP Notify Listener
  nsCOMPtr<nsIObserver>           mObserverHTML;      // The HTML Tag observer
  nsCOMPtr<nsIObserver>           mObserverXML;       // The XML Tag observer
  nsCOMPtr<nsIObserver>           mObserverLayout;    // The Layout observer
  nsCOMPtr<nsIObserver>           mObserverFormSubmit;// The Form Submission observer
};

PRBool PR_CALLBACK MarkNoP3P( nsHashKey  *aKey,
                              void       *aValue,
                              void       *aClosure );


class nsP3PBrowserWindowData : public nsISupports {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsP3PBrowserWindowData methods
  nsP3PBrowserWindowData( );
  virtual ~nsP3PBrowserWindowData( );

  PRBool                mDocumentComplete;            // An indicator of when the browser window document is complete

  nsStringArray         mReadsInProgress;             // An array of URI specs with P3P related reads in progress

  nsresult              mOverallPrivacyResult;        // The overall privacy result for the browser window
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PBrowserWindowData( nsISupports **aBrowserWindowData );


class nsP3PDocShellTreeItemData : public nsISupports {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsP3PDocShellTreeItemData methods
  nsP3PDocShellTreeItemData( nsSupportsHashtable  *aBrowserWindowMap );
  virtual ~nsP3PDocShellTreeItemData( );

  nsSupportsHashtable             mURIInformations;   // URIInformation object cache (will only have a value for "main" DocShellTreeItem)

  nsSupportsHashtable             mDocShellTreeItemDataChildMap;

  nsString                        mPrivacyResultURISpec;   // The URI that maps the main PrivacyResult object to the hashtable
  nsCOMPtr<nsIP3PPrivacyResult>   mPrivacyResult;          // The main PrivacyResult object for this DocShellTreeItem

  nsSupportsHashtable             mPrivacyResultMap;       // The collection of PrivacyResult objects for this DocShellTreeItem (for images, etc.)

private:
  nsSupportsHashtable            *mBrowserWindowMap;
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PDocShellTreeItemData( nsSupportsHashtable  *aBrowserWindowMap,
                                                       nsISupports         **aDocShellTreeItemData );

PRBool PR_CALLBACK RemoveBrowserWindow( nsHashKey  *aKey,
                                        void       *aValue,
                                        void       *aClosure );


class nsP3PServiceReadData : public nsISupports {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsP3PServiceReadData methods
  nsP3PServiceReadData( );
  virtual ~nsP3PServiceReadData( );

  PRInt32                         mCheckState;        // The current state of the privacy check

  nsCOMPtr<nsIURI>                mCheckURI;          // The URI object on which the privacy check is being performed
  nsString                        mMethod;            // The method used to access the URI object
  nsCOMPtr<nsIDocShellTreeItem>   mDocShellTreeItem;  // The DocShellTreeItem object containing the URI object

  PRBool                          mEmbedDetermined;   // An indicator as to whether it has been determined if the object is embedded

  nsCOMPtr<nsIURI>                mEmbeddingURI;      // The URI object of the embedding document
  PRBool                          mEmbedCheckComplete;// An indicator as to whether the privacy checking of the embedded object is complete

  nsString                        mReadingPolicyRefFileURISpec; // Holder for an reading PolicyRefFile URI spec
  PRInt32                         mReadingReferencePoint;       // Holder for where the reference to the reading PolicyRefFile occurred

  nsString                        mExpiredPolicyRefFileURISpec; // Holder for an expired PolicyRefFile URI spec
  PRInt32                         mExpiredReferencePoint;       // Holder for where the reference to the expired PolicyRefFile occurred

  nsCOMPtr<nsIP3PPolicy>          mPolicy;            // The Policy object
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PServiceReadData( nsString&             aURIMethod,
                                                  nsIURI               *aCheckURI,
                                                  nsIDocShellTreeItem  *aDocShellTreeItem,
                                                  nsISupports         **aServiceReadData );

#endif                                           /* nsP3PService_h__          */
