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

#define  P3P_LOG_DEFINITION

#include "nsP3PDefines.h"

#include "nsP3PService.h"

#include "nsP3PPreferences.h"
#include "nsP3PHTTPNotify.h"
#include "nsP3PObserverHTML.h"
#include "nsP3PObserverXML.h"
#include "nsP3PObserverLayout.h"
#include "nsP3PObserverFormSubmit.h"
#include "nsP3PURIInformation.h"
#include "nsP3PPolicyRefFile.h"
#include "nsP3PReference.h"
#include "nsP3PPolicy.h"
#include "nsP3PDataSchema.h"
#include "nsP3PPrivacyResult.h"
#include "nsP3PLogging.h"

#include <nsIGenericFactory.h>

#include <nsIHTTPProtocolHandler.h>

#include <nsLayoutCID.h>
#include <nsINameSpaceManager.h>

#include <nsNetUtil.h>

#include <nsIDOMNode.h>

#include <nsIWebNavigation.h>

#include <nsString.h>
#include <nsXPIDLString.h>

#include <prprf.h>


// ****************************************************************************
// nsP3PService Implementation routines
// ****************************************************************************

static NS_DEFINE_CID( kNameSpaceManagerCID,      NS_NAMESPACEMANAGER_CID );
static NS_DEFINE_CID( kStringBundleServiceCID,   NS_STRINGBUNDLESERVICE_CID );

// P3P Service generic factory constructor
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT( nsP3PService, Init );

// P3P Service category registration procedure
//
// Function:  Regsiter the P3P Service to start on the first HTTP request.
//
NS_METHOD
nsP3PService::CategoryRegister( nsIComponentManager          *aComponentManager,
                                nsIFile                      *aPath,
                                const char                   *aRegistryLocation,
                                const char                   *aComponentType,
                                const nsModuleComponentInfo  *aInfo ) {

  nsresult                      rv;

  nsCOMPtr<nsICategoryManager>  pCategoryManager;

  nsXPIDLCString                xcsPrevEntry;


#ifdef DEBUG_P3P
  { printf("P3P:  P3PService category registration.\n");
  }
#endif

  pCategoryManager = do_GetService( NS_CATEGORYMANAGER_CONTRACTID,
                                   &rv );
  if (NS_SUCCEEDED( rv )) {
    rv = pCategoryManager->AddCategoryEntry( NS_HTTP_STARTUP_CATEGORY,
                                             NS_P3PSERVICE_CLASSNAME,
                                             NS_P3PSERVICE_CONTRACTID,
                                             PR_TRUE,
                                             PR_TRUE,
                                             getter_Copies( xcsPrevEntry ) );

    if (NS_FAILED( rv )) {
      NS_ASSERTION( 0, "P3P:  P3PService category registration failed.\n" );
    }
  }
  else {
    NS_ASSERTION( 0, "P3P:  P3PService unable to obtain CategoryManager service.\n" );
  }

  return rv;
}

// P3P Service category unregistration procedure
//
// Function:  Unregsiter the P3P Service to start on the first HTTP request.
//
NS_METHOD
nsP3PService::CategoryUnregister( nsIComponentManager          *aComponentManager,
                                  nsIFile                      *aPath,
                                  const char                   *aRegistryLocation,
                                  const nsModuleComponentInfo  *aInfo ) {

  nsresult                      rv;

  nsCOMPtr<nsICategoryManager>  pCategoryManager;

  nsXPIDLCString                xcsPrevEntry;


  pCategoryManager = do_GetService( NS_CATEGORYMANAGER_CONTRACTID,
                                   &rv );

  if (NS_SUCCEEDED( rv )) {
    rv = pCategoryManager->DeleteCategoryEntry( NS_HTTP_STARTUP_CATEGORY,
                                                NS_P3PSERVICE_CONTRACTID,
                                                PR_TRUE,
                                                getter_Copies( xcsPrevEntry ) );
  }

  return rv;
}

// P3P Service nsISupports
NS_IMPL_ISUPPORTS3( nsP3PService, nsIP3PCService,
                                  nsIP3PService,
                                  nsIP3PXMLListener );

// P3P Service Constructor
nsP3PService::nsP3PService( )
  : mInitialized( PR_FALSE ),
    mP3PRelatedURISpecs( 256, PR_TRUE ),
    mReferences( 256, PR_TRUE ),
    mPolicies( 256, PR_TRUE ),
    mDataSchemas( 256, PR_TRUE ),
    mBrowserWindowMap( 32, PR_TRUE ),
    mBrowserWindowBusyMap( 32, PR_TRUE ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Service Destructor
nsP3PService::~nsP3PService( ) {
}

// P3P Service create instance routine
NS_METHOD
nsP3PService::Create( nsISupports  *aOuter,
                      REFNSIID      aIID,
                      void        **aResult ) {

  return nsP3PServiceConstructor( aOuter,
                                  aIID,
                                  aResult );
}

// P3P Service: Init
//
// Function:  Initialization routine for the P3P Service.
//
// Parms:     None
//
NS_METHOD
nsP3PService::Init( ) {

  nsresult                          rv = NS_OK;

  PRExplodedTime                    explodedTime;

  nsCOMPtr<nsINameSpaceManager>     pNameSpaceManager;
  nsAutoString                      sP3PNameSpace;
  PRInt32                           iP3PNameSpaceID;

  nsCOMPtr<nsIURI>                  pURI;
  nsCOMPtr<nsIStringBundleService>  pStringBundleService;
  nsXPIDLCString                    xcsSpec;


#ifdef DEBUG_P3P
  printf("P3P:  P3PService initializing.\n");
#endif

  // Only initialize once...
  if (!mInitialized) {
    mInitialized = PR_TRUE;

#ifdef PR_LOGGING
    gP3PLogModule = PR_NewLogModule( "P3P" );

    if (gP3PLogModule == nsnull) {
      rv = NS_ERROR_FAILURE;
    }
#endif

    if (NS_SUCCEEDED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PService:  Initializing.\n") );

      // Obtain the service start time (GMT)
      PR_ExplodeTime( PR_Now( ),
                      PR_GMTParameters,
                     &explodedTime );
      PR_FormatTimeUSEnglish( mGMTStartTime,
                              sizeof( mGMTStartTime ),
                              "%a, %d-%b-%Y %H:%M:%S GMT",
                             &explodedTime );

      // Get the Cookie service
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PService:  Getting the cookie service.\n") );

      mCookieService = do_GetService( NS_COOKIESERVICE_CONTRACTID,
                                     &rv );

      // Create the Preferences service
      if (NS_SUCCEEDED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  Creating Preference Service.\n") );

        rv = NS_NewP3PPreferences( getter_AddRefs( mPreferences ) );
      }

      // Create the UI service
      if (NS_SUCCEEDED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  Creating UI Service.\n") );

        mUIService = do_GetService( NS_P3PUISERVICE_CONTRACTID,
                                   &rv );
      }

      // Create the HTTP Notify service so that HTTP headers can be examined
      if (NS_SUCCEEDED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  Creating HTTP Notify Service.\n") );

        rv = NS_NewP3PHTTPNotify( getter_AddRefs( mHTTPNotify ) );
      }

      // Register the HTML Element Observer
      if (NS_SUCCEEDED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  Creating HTML Observer.\n") );

        rv = NS_NewP3PObserverHTML( getter_AddRefs( mObserverHTML ) );
      }

      // Register the XML Element Observer
      if (NS_SUCCEEDED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  Creating XML Observer.\n") );

        rv = NS_NewP3PObserverXML( getter_AddRefs( mObserverXML ) );
      }

      // Register the Layout Observer
      if (NS_SUCCEEDED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  Registering Layout Observer.\n") );

        rv = NS_NewP3PObserverLayout( getter_AddRefs( mObserverLayout ) );
      }

      // Register the Form Submission Observer
      if (NS_SUCCEEDED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  Registering Form Submit Observer.\n") );

        rv = NS_NewP3PObserverFormSubmit( getter_AddRefs( mObserverFormSubmit ) );
      }

      // Register the NameSpace
      if (NS_SUCCEEDED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  Creating the NameSpace Manager.\n") );

        pNameSpaceManager = do_CreateInstance( kNameSpaceManagerCID,
                                              &rv );

        if (NS_SUCCEEDED( rv )) {
          PR_LOG( gP3PLogModule,
                  PR_LOG_NOTICE,
                  ("P3PService:  Registering the P3P Name Space.\n") );

          sP3PNameSpace.AssignWithConversion( P3P_P3P_NAMESPACE );
          rv = pNameSpaceManager->RegisterNameSpace( sP3PNameSpace,
                                                     iP3PNameSpaceID );
        }
      }

      if (NS_SUCCEEDED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  Creating string bundle URI.\n") );

        rv = NS_NewURI( getter_AddRefs( pURI ),
                        P3P_PROPERTIES_FILE );

        if (NS_SUCCEEDED( rv )) {
          PR_LOG( gP3PLogModule,
                  PR_LOG_NOTICE,
                  ("P3PService:  Getting the string bundle service.\n") );

          pStringBundleService = do_GetService( kStringBundleServiceCID,
                                               &rv );

          if (NS_SUCCEEDED( rv )) {
            rv = pURI->GetSpec( getter_Copies( xcsSpec ) );

            if (NS_SUCCEEDED( rv )) {
              PR_LOG( gP3PLogModule,
                      PR_LOG_NOTICE,
                      ("P3PService:  Creating string bundle.\n") );

              rv = pStringBundleService->CreateBundle((const char *)xcsSpec,
                                                       getter_AddRefs( mStringBundle ) );
            }
          }
        }
      }
    }
  } 
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_WARNING,
            ("P3PService initialized more than once.\n") );

    NS_ASSERTION( 0, "P3P:  P3PService initialized more than once.\n" );
    rv = NS_ERROR_ALREADY_INITIALIZED;
  }

  if (NS_SUCCEEDED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PService:  Initialization successful.\n") );
  }
  else {
#ifdef DEBUG_P3P
    printf( "P3P:  Unable to initialize P3P Service: %X\n", rv );
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_ALWAYS,
            ("P3PService:  Initialization failed:  %X.\n", rv) );
  }

  return rv;
}


// ****************************************************************************
// nsIP3PService routines
// ****************************************************************************

// P3P Service check if enabled routine
//
// Function:  Set the supplied parameter with the current state of the P3P Service.
//
// Parms:     1. Out:             The current state of the P3P Service
//
// Return:    NS_OK                    OK
//            NS_ERROR_INVALID_POINTER Parameter error
NS_IMETHODIMP
nsP3PService::P3PIsEnabled( PRBool *aEnabled ) {

  nsresult  rv;


  NS_ENSURE_ARG_POINTER( aEnabled );

  rv = mPreferences->GetBoolPref( P3P_PREF_ENABLED,
                                  aEnabled );

  return rv;
}


// ****************************************************************************
// nsIP3PCService routines
// ****************************************************************************

// P3P Service: Enabled
//
// Function:  Notification of the enabling of P3P (preference change).
//
// Parms:     None
//
NS_IMETHODIMP
nsP3PService::Enabled( void ) {

  // Nothing to do.  It will all kick in on the next load.
  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PService:  Enabled, P3P Serviced enabled.\n") );

  return NS_OK;
}

// P3P Service: Disabled
//
// Function:  Notification of the disabling of P3P (preference change).
//
// Parms:     None
//
NS_IMETHODIMP
nsP3PService::Disabled( void ) {

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PService:  Disabled, P3P Serviced disabled.\n") );

  if (mUIService) {
    mBrowserWindowMap.Enumerate( MarkNoP3P, mUIService);
  }

  return NS_OK;
}

// P3P Service: LoadingObject
//
// Function:  Notification that a request to load an object has been initiated by the user.
//
// Parms:     1. In     The URI being loaded
//            2. In     The DocShellTreeItem that represents the browser UI instance
//            3. In     The DocShellTreeItem where the URI is being loaded (possibly a frame)
//            4. In     An indicator if this is the first or initial load
//
NS_IMETHODIMP
nsP3PService::LoadingObject( nsIURI               *aLoadingURI,
                             nsIDocShellTreeItem  *aDocShellTreeItemMain,
                             nsIDocShellTreeItem  *aDocShellTreeItemCurrent,
                             PRBool                aInitialLoad ) {

  nsresult                       rv = NS_OK;

  PRBool                         bP3PIsEnabled               = PR_FALSE;

  nsVoidKey                      keyDocShellTreeItemMain( aDocShellTreeItemMain ),
                                 keyDocShellTreeItemCurrent( aDocShellTreeItemCurrent );

  nsCOMPtr<nsISupports>          pBrowserWindowData;

  nsP3PDocShellTreeItemData     *pDocShellTreeItemData       = nsnull,
                                *pDocShellTreeItemDataParent = nsnull;

  nsAutoString                   sLoadingURISpec,
                                 sNull;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItemParent;

  nsCOMPtr<nsIP3PPrivacyResult>  pPrivacyResult;


  rv = P3PIsEnabled(&bP3PIsEnabled );

  if (bP3PIsEnabled) {
    rv = GetURIComponents( aLoadingURI,
                           sLoadingURISpec,
                           sNull,
                           sNull,
                           sNull );

    if (NS_SUCCEEDED( rv )) {
      nsCAutoString  csLoadingURISpec; csLoadingURISpec.AssignWithConversion( sLoadingURISpec );

      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PService:  LoadingObject, loading object - %s.\n", (const char *)csLoadingURISpec) );

      if (aInitialLoad) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  LoadingObject, initial load indicated.\n") );

        // Create a new BrowserWindowData object
        //   (replace the current BrowserWindowData object even if unsuccessful)
        NS_NewP3PBrowserWindowData( getter_AddRefs( pBrowserWindowData ) );
        mBrowserWindowBusyMap.Put(&keyDocShellTreeItemMain,
                                   pBrowserWindowData );

        // Create a new DocShellTreeItemData object and PrivacyResult object
        rv = NS_NewP3PDocShellTreeItemData(&mBrowserWindowMap,
                           (nsISupports **)&pDocShellTreeItemData );

        if (NS_SUCCEEDED( rv )) {
          pDocShellTreeItemData->mPrivacyResultURISpec = sLoadingURISpec;
          rv = NS_NewP3PPrivacyResult( sLoadingURISpec,
                                       getter_AddRefs( pDocShellTreeItemData->mPrivacyResult ) );

          if (NS_SUCCEEDED( rv )) {
            nsStringKey    keyLoadingURISpec( sLoadingURISpec );

            mBrowserWindowMap.Put(&keyDocShellTreeItemCurrent,
                                   pDocShellTreeItemData );

            pDocShellTreeItemData->mPrivacyResultMap.Put(&keyLoadingURISpec,
                                                          pDocShellTreeItemData->mPrivacyResult );

            if (aDocShellTreeItemCurrent != aDocShellTreeItemMain) {
              rv = aDocShellTreeItemCurrent->GetParent( getter_AddRefs( pDocShellTreeItemParent ) );

              if (NS_SUCCEEDED( rv )) {
                nsVoidKey  keyDocShellTreeItemParent( pDocShellTreeItemParent );

                pDocShellTreeItemDataParent = (nsP3PDocShellTreeItemData *)mBrowserWindowMap.Get(&keyDocShellTreeItemParent );

                if (pDocShellTreeItemDataParent) {
                  pDocShellTreeItemDataParent->mDocShellTreeItemDataChildMap.Put(&keyDocShellTreeItemCurrent,
                                                                                  pDocShellTreeItemData );

                  rv = pDocShellTreeItemDataParent->mPrivacyResult->AddChild( pDocShellTreeItemData->mPrivacyResult );

                  NS_RELEASE( pDocShellTreeItemDataParent );
                }
              }
              else {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PService:  LoadingObject, aDocShellTreeItemCurrent->GetParent failed - %X.\n", rv) );
              }
            }
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PService:  LoadingObject, NS_NewP3PPrivacyResult failed - %X.\n", rv) );
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PService:  LoadingObject, NS_NewP3PDocShellTreeItemData failed - %X.\n", rv) );
        }

        NS_RELEASE( pDocShellTreeItemData );
      }
      else {
        // Not the first load
        if (!sLoadingURISpec.EqualsWithConversion( "about:layout-dummy-request" )) {
          // We don't have a dummy layout request

          // Make sure a DocShellTreeItemData object exists
          if (!mBrowserWindowMap.Exists(&keyDocShellTreeItemCurrent )) {
            // Create a new DocShellTreeItemData object for the load
            rv = NS_NewP3PDocShellTreeItemData(&mBrowserWindowMap,
                               (nsISupports **)&pDocShellTreeItemData );

            if (NS_SUCCEEDED( rv )) {
              rv = NS_NewP3PPrivacyResult( sLoadingURISpec,
                                           getter_AddRefs( pDocShellTreeItemData->mPrivacyResult ) );

              if (NS_SUCCEEDED( rv )) {
                nsStringKey  keyLoadingURISpec( sLoadingURISpec );

                mBrowserWindowMap.Put(&keyDocShellTreeItemCurrent,
                                       pDocShellTreeItemData );

                pDocShellTreeItemData->mPrivacyResultMap.Put(&keyLoadingURISpec,
                                                              pDocShellTreeItemData->mPrivacyResult );

                if (aDocShellTreeItemCurrent != aDocShellTreeItemMain) {
                  rv = aDocShellTreeItemCurrent->GetParent( getter_AddRefs( pDocShellTreeItemParent ) );

                  if (NS_SUCCEEDED( rv )) {
                    nsVoidKey  keyDocShellTreeItemParent( pDocShellTreeItemParent );

                    pDocShellTreeItemDataParent = (nsP3PDocShellTreeItemData *)mBrowserWindowMap.Get(&keyDocShellTreeItemParent );

                    if (pDocShellTreeItemDataParent) {
                      pDocShellTreeItemDataParent->mDocShellTreeItemDataChildMap.Put(&keyDocShellTreeItemCurrent,
                                                                                      pDocShellTreeItemData );

                      rv = pDocShellTreeItemDataParent->mPrivacyResult->AddChild( pDocShellTreeItemData->mPrivacyResult );

                      NS_RELEASE( pDocShellTreeItemDataParent );
                    }
                  }
                  else {
                    PR_LOG( gP3PLogModule,
                            PR_LOG_ERROR,
                            ("P3PService:  LoadingObject, aDocShellTreeItemCurrent->GetParent failed - %X.\n", rv) );
                  }
                }
              }
              else {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PService:  LoadingObject, NS_NewP3PPrivacyResult failed - %X.\n", rv) );
              }

              NS_RELEASE( pDocShellTreeItemData );
            }
            else {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PService:  LoadingObject, NS_NewP3PDocShellTreeItemData failed - %X.\n", rv) );
            }
          }
          else {
            pDocShellTreeItemData = (nsP3PDocShellTreeItemData *)mBrowserWindowMap.Get(&keyDocShellTreeItemCurrent );

            if (pDocShellTreeItemData) {
              nsStringKey  keyLoadingURISpec( sLoadingURISpec );

              if (!pDocShellTreeItemData->mPrivacyResultMap.Exists(&keyLoadingURISpec )) {
                rv = NS_NewP3PPrivacyResult( sLoadingURISpec,
                                             getter_AddRefs( pPrivacyResult ) );

                if (NS_SUCCEEDED( rv )) {
                  pDocShellTreeItemData->mPrivacyResultMap.Put(&keyLoadingURISpec,
                                                                pPrivacyResult );

                  pDocShellTreeItemData->mPrivacyResult->AddChild( pPrivacyResult );
                }
                else {
                  PR_LOG( gP3PLogModule,
                          PR_LOG_ERROR,
                          ("P3PService:  LoadingObject, NS_NewP3PPrivacyResult failed - %X.\n", rv) );
                }
              }

              NS_RELEASE( pDocShellTreeItemData );
            }
          }
        }
      }
    }
  }

  return rv;
}

// P3P Service: ModifyPrivacyResult
//
// Function:  Modifies the URI associated with a PrivacyResult object (used for redirections, etc.)
//
// Parms:     1. In     The original URI
//            2. In     The new URI
//            3. In     The DocShellTreeItem where the URI is being loaded
//
NS_IMETHODIMP
nsP3PService::ModifyPrivacyResult( nsIURI               *aOriginalURI,
                                   nsIURI               *aNewURI,
                                   nsIDocShellTreeItem  *aDocShellTreeItem ) {

  nsresult                       rv;

  PRBool                         bP3PIsEnabled           = PR_FALSE;

  nsVoidKey                      keyDocShellTreeItem( aDocShellTreeItem );

  nsAutoString                   sOriginalURISpec,
                                 sNewURISpec,
                                 sNull;

  nsP3PDocShellTreeItemData     *pDocShellTreeItemData   = nsnull;

  nsCOMPtr<nsIP3PPrivacyResult>  pPrivacyResult;


  rv = P3PIsEnabled(&bP3PIsEnabled );

  if (bP3PIsEnabled) {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PService:  ModifyPrivacyResult, modifying privacy result object URI spec.\n") );

    rv = GetURIComponents( aOriginalURI,
                           sOriginalURISpec,
                           sNull,
                           sNull,
                           sNull );

    if (NS_SUCCEEDED( rv )) {
      rv = GetURIComponents( aNewURI,
                             sNewURISpec,
                             sNull,
                             sNull,
                             sNull );

      if (NS_SUCCEEDED( rv )) {
        nsCAutoString  csOldURISpec,
                       csNewURISpec;

        csOldURISpec.AssignWithConversion( sOriginalURISpec );
        csNewURISpec.AssignWithConversion( sNewURISpec );
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  ModifyPrivacyResult, from - %s   to - %s.\n", (const char *)csOldURISpec, (const char *)csNewURISpec) );

        pDocShellTreeItemData = (nsP3PDocShellTreeItemData *)mBrowserWindowMap.Get(&keyDocShellTreeItem );

        if (pDocShellTreeItemData) {
          nsCOMPtr<nsISupports>  pEntry;
          nsStringKey            keyOriginalURISpec( sOriginalURISpec ),
                                 keyNewURISpec( sNewURISpec );

          pEntry         = getter_AddRefs( pDocShellTreeItemData->mPrivacyResultMap.Get(&keyOriginalURISpec ) );
          pPrivacyResult = do_QueryInterface( pEntry,
                                             &rv );

          if (NS_SUCCEEDED( rv )) {
            // Change the URI spec and place it in the hashtable under the new key
            pPrivacyResult->SetURISpec( sNewURISpec );
            pDocShellTreeItemData->mPrivacyResultMap.Put(&keyNewURISpec,
                                                          pPrivacyResult );
          }

          NS_RELEASE( pDocShellTreeItemData );
        }
      }
    }
  }
  
  return rv;
}

// P3P Service: DocumentComplete
//
// Function:  Notification that a document has been completed in a browser window instance.
//
// Parms:     1. In     The DocShellTreeItem that represents the browser UI instance
//
NS_IMETHODIMP
nsP3PService::DocumentComplete( nsIDocShellTreeItem  *aDocShellTreeItemMain ) {

  nsresult                rv;

  PRBool                  bP3PIsEnabled           = PR_FALSE;

  nsVoidKey               keyDocShellTreeItemMain( aDocShellTreeItemMain );

  nsP3PBrowserWindowData *pBrowserWindowData      = nsnull;


  rv = P3PIsEnabled(&bP3PIsEnabled );

  if (bP3PIsEnabled) {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PService:  DocumentComplete, document load complete.\n") );

    pBrowserWindowData = (nsP3PBrowserWindowData *)mBrowserWindowBusyMap.Get(&keyDocShellTreeItemMain );

    if (pBrowserWindowData) {
      pBrowserWindowData->mDocumentComplete = PR_TRUE;

      if (pBrowserWindowData->mReadsInProgress.Count( ) == 0) {
        UpdateBrowserWindow( aDocShellTreeItemMain,
                             pBrowserWindowData->mOverallPrivacyResult );
      }

      NS_RELEASE( pBrowserWindowData );
    }
  }

  return NS_OK;
}

// P3P Service: ClosingBrowserWindow
//
// Function:  Notification that a browser window instance is closing.
//
// Parms:     1. In     The DocShellTreeItem that represents the browser UI instance
//
NS_IMETHODIMP
nsP3PService::ClosingBrowserWindow( nsIDocShellTreeItem  *aDocShellTreeItemMain ) {

  nsVoidKey                      keyDocShellTreeItemMain( aDocShellTreeItemMain );


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PService:  ClosingBrowserWindow, browser window instance closing.\n") );

  mBrowserWindowMap.Remove(&keyDocShellTreeItemMain );

  return NS_OK;
}

// P3P Service: SetRefererHeader
//
// Function:  Set/Save the referer information associated with a URI.
//
// Parms:     1. In     The DocShellTreeItem that the URI is associated with
//            2. In     The URI being loaded
//            3. In     The "Referer" request header value
//
NS_IMETHODIMP
nsP3PService::SetRefererHeader( nsIDocShellTreeItem  *aDocShellTreeItem,
                                nsIURI               *aReferencingURI,
                                nsString&             aRefererHeader ) {

  nsresult                        rv;

  nsAutoString                    sReferencingURISpec,
                                  sNull;

  nsCOMPtr<nsIP3PURIInformation>  pURIInformation;


  rv = GetURIComponents( aReferencingURI,
                         sReferencingURISpec,
                         sNull,
                         sNull,
                         sNull );

  if (NS_SUCCEEDED( rv )) {
    rv = GetURIInformation( aDocShellTreeItem,
                            sReferencingURISpec,
                            getter_AddRefs( pURIInformation ) );

    if (NS_SUCCEEDED( rv ) && !pURIInformation) {
      rv = CreateURIInformation( aDocShellTreeItem,
                                 sReferencingURISpec,
                                 getter_AddRefs( pURIInformation ) );
    }

    if (NS_SUCCEEDED( rv )) {
      pURIInformation->SetRefererHeader( aRefererHeader );
    }
  }

  return rv;
}

// P3P Service: SetPolicyRefFileURISpec
//
// Function:  Set the URI of the PolicyRefFile specified.
//
// Parms:     1. In     The DocShellTreeItem that the URI is associated with
//            2. In     The URI that has specified the PolicyRefFile
//            3. In     The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            4. In     The URI spec of the PolicyRefFile
NS_IMETHODIMP
nsP3PService::SetPolicyRefFileURISpec( nsIDocShellTreeItem  *aDocShellTreeItem,
                                       nsIURI               *aReferencingURI,
                                       PRInt32               aReferencePoint,
                                       nsString&             aPolicyRefFileURISpec ) {

  nsresult                        rv;

  nsAutoString                    sReferencingURISpec,
                                  sNull;

  nsCOMPtr<nsIP3PURIInformation>  pURIInformation;


  rv = GetURIComponents( aReferencingURI,
                         sReferencingURISpec,
                         sNull,
                         sNull,
                         sNull );

  if (NS_SUCCEEDED( rv )) {
    rv = GetURIInformation( aDocShellTreeItem,
                            sReferencingURISpec,
                            getter_AddRefs( pURIInformation ) );

    if (NS_SUCCEEDED( rv ) && !pURIInformation) {
      rv = CreateURIInformation( aDocShellTreeItem,
                                 sReferencingURISpec,
                                 getter_AddRefs( pURIInformation ) );
    }

    if (NS_SUCCEEDED( rv )) {
      pURIInformation->SetPolicyRefFileURISpec( aReferencePoint,
                                                aPolicyRefFileURISpec );
    }
  }

  return rv;
}

// P3P Service: SetCookieHeader
//
// Function:  Set/Save the cookie information associated with a URI.
//
// Parms:     1. In     The DocShellTreeItem that the URI is associated with
//            2. In     The URI being loaded
//            3. In     The "Set-Cookie" response header value
//
NS_IMETHODIMP
nsP3PService::SetCookieHeader( nsIDocShellTreeItem  *aDocShellTreeItem,
                               nsIURI               *aReferencingURI,
                               nsString&             aCookieHeader ) {

  nsresult                        rv;

  nsAutoString                    sReferencingURISpec,
                                  sNull;

  nsCOMPtr<nsIP3PURIInformation>  pURIInformation;


  rv = GetURIComponents( aReferencingURI,
                         sReferencingURISpec,
                         sNull,
                         sNull,
                         sNull );

  if (NS_SUCCEEDED( rv )) {
    rv = GetURIInformation( aDocShellTreeItem,
                            sReferencingURISpec,
                            getter_AddRefs( pURIInformation ) );

    if (NS_SUCCEEDED( rv ) && !pURIInformation) {
      rv = CreateURIInformation( aDocShellTreeItem,
                                 sReferencingURISpec,
                                 getter_AddRefs( pURIInformation ) );
    }

    if (NS_SUCCEEDED( rv )) {
      pURIInformation->SetCookieHeader( aCookieHeader );
    }
  }

  return rv;
}

// P3P Service: CheckPrivacy
//
// Function:  Check the Privacy of a URI (an internet resource).
//
// Parms:     1. In     The method used on the URI (corresponds to the Policy Ref File <METHOD>)
//            2. In     The URI to check
//            3. In     The DocShellTreeItem that the URI is associated with
//            4. In     The data to be used during processing because of asynchronous read events
//                      that result in multiple calls to this function
//
NS_IMETHODIMP
nsP3PService::CheckPrivacy( nsString&             aURIMethod,
                            nsIURI               *aCheckURI,
                            nsIDocShellTreeItem  *aDocShellTreeItem,
                            nsISupports          *aReadData ) {

  nsresult                       rv,
                                 overallPrivacyResult;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItemMain;

  PRBool                         bP3PIsEnabled         = PR_FALSE;

  nsCAutoString                  csURISpec,
                                 csMethod;

  nsAutoString                   sURISpec,
                                 sURIScheme,
                                 sURIHostPort,
                                 sURIPath;

  nsP3PServiceReadData          *pReadData             = nsnull;

  nsP3PBrowserWindowData        *pBrowserWindowData    = nsnull;

  nsCOMPtr<nsIP3PPolicy>         pPolicy;


  rv = P3PIsEnabled(&bP3PIsEnabled );

  if (bP3PIsEnabled) {

    rv = GetDocShellTreeItemMain( aDocShellTreeItem,
                                  getter_AddRefs( pDocShellTreeItemMain ) );

    if (NS_SUCCEEDED( rv )) {

      if (IsBrowserWindow( pDocShellTreeItemMain )) {
        nsVoidKey  keyDocShellTreeItem( aDocShellTreeItem ),
                   keyDocShellTreeItemMain( pDocShellTreeItemMain );

        rv = GetURIComponents( aCheckURI,
                               sURISpec,
                               sURIScheme,
                               sURIHostPort,
                               sURIPath );

        if (NS_SUCCEEDED( rv ) && IsHTTPProtocol( aCheckURI )) {
          csURISpec.AssignWithConversion( sURISpec );
          csMethod.AssignWithConversion( aURIMethod );

#ifdef DEBUG_P3P
          { printf( "P3P:  Checking Privacy for %s using %s\n", (const char *)csURISpec, (const char *)csMethod );
          }
#endif

          PR_LOG( gP3PLogModule,
                  PR_LOG_NOTICE,
                  ("P3PService:  CheckPrivacy, checking privacy for - %s using %s.\n", (const char *)csURISpec, (const char *)csMethod) );

          if (aReadData) {
            rv = aReadData->QueryInterface( NS_GET_IID( nsISupports ),
                                  (void **)&pReadData );
          }
          else {
            rv = NS_NewP3PServiceReadData( aURIMethod,
                                           aCheckURI,
                                           aDocShellTreeItem,
                          (nsISupports **)&pReadData );

            if (NS_FAILED( rv )) {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PService:  CheckPrivacy, NS_NewP3PServiceReadData failed - %X.\n", rv) );
            }
          }

          if (NS_SUCCEEDED( rv )) {

            if (!pReadData->mEmbedDetermined) {
              rv = CheckForEmbedding( pReadData,
                                      getter_AddRefs( pReadData->mEmbeddingURI ) );

              if (NS_SUCCEEDED( rv )) {
                pReadData->mEmbedDetermined = PR_TRUE;
              }
            }

            if (NS_SUCCEEDED( rv )) {
              // Initialize the return value in case embedding processing isn't performed
              rv = P3P_PRIVACY_NONE;

              if (pReadData->mEmbeddingURI && !pReadData->mEmbedCheckComplete) {
                rv = PrivacyStateMachine( pReadData,
                                          PR_TRUE,
                                          P3P_ASYNC_READ,
                                          P3P_OBJECT_CHECK );

                if (rv != P3P_PRIVACY_IN_PROGRESS) {
                  // Indicate the embed check is complete
                  pReadData->mEmbedCheckComplete = PR_TRUE;

                  if (rv == P3P_PRIVACY_NONE) {
                    // No Policy was found for the embedded scenario, reset the state and perform normal check
                    pReadData->mCheckState = P3P_CHECK_INITIAL_STATE;
                  }
                }
              }

              if (rv == P3P_PRIVACY_NONE) {
                rv = PrivacyStateMachine( pReadData,
                                          PR_FALSE,
                                          P3P_ASYNC_READ,
                                          P3P_OBJECT_CHECK );
              }

              if (rv == P3P_PRIVACY_NOT_MET) {
                DeleteCookies( aDocShellTreeItem,
                               sURISpec,
                               aCheckURI );
              }
            }
          }
        }
        else if (NS_SUCCEEDED( rv )) {
          rv = P3P_PRIVACY_NONE;
        }

        if (pReadData) {
          pPolicy = pReadData->mPolicy;
        }

        overallPrivacyResult = SetPrivacyResult( aCheckURI,
                                                 aDocShellTreeItem,
                                                 rv,
                                                 pPolicy );

#ifdef DEBUG_P3P
        { printf( "P3P:  Privacy result for %s: %08X   Overall privacy result: %08X\n", (const char *)csURISpec, rv, overallPrivacyResult );
        }
#endif

        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  CheckPrivacy, privacy result for - %s: %X  Overall privacy result %X.\n", (const char *)csURISpec, rv, overallPrivacyResult) );

        pBrowserWindowData = (nsP3PBrowserWindowData *)mBrowserWindowBusyMap.Get(&keyDocShellTreeItemMain );

        if (pBrowserWindowData) {

          pBrowserWindowData->mOverallPrivacyResult = overallPrivacyResult;

          if (rv == P3P_PRIVACY_IN_PROGRESS) {

            if (pBrowserWindowData->mReadsInProgress.IndexOf( sURISpec ) < 0) {
              pBrowserWindowData->mReadsInProgress.AppendString( sURISpec );
            }
          }
          else {
            pBrowserWindowData->mReadsInProgress.RemoveString( sURISpec );
          }

          if (!pBrowserWindowData->mDocumentComplete || pBrowserWindowData->mReadsInProgress.Count( )) {
            UpdateBrowserWindow( pDocShellTreeItemMain,
                                 P3P_PRIVACY_IN_PROGRESS );
          }
          else {
            UpdateBrowserWindow( pDocShellTreeItemMain,
                                 overallPrivacyResult );
          }

          NS_RELEASE( pBrowserWindowData );
        }
        else {
          UpdateBrowserWindow( pDocShellTreeItemMain,
                               overallPrivacyResult );
        }
      }
      else {
        rv = P3P_PRIVACY_NONE;
      }
    }
  }

  NS_IF_RELEASE( pReadData );

  return rv;
}

// P3P Service: CheckPrivacyFormSubmit
//
// Function:  Check the Privacy of the form submission URI target.
//
// Parms:     1. In     The URI of the submission
//            2. In     The DOMWindowInternal associated with the form submission
//            3. In     The Content of the <FORM>
//            4. Out    The indicator of whether to cancel the form submission
//
NS_IMETHODIMP
nsP3PService::CheckPrivacyFormSubmit( nsIURI                *aCheckURI,
                                      nsIDOMWindowInternal  *aDOMWindowInternal,
                                      nsIContent            *aFormContent,
                                      PRBool                *aCancelSubmit ) {

  nsresult              rv;

  PRBool                bP3PIsEnabled = PR_FALSE;

  nsCAutoString         csURISpec,
                        csMethod;

  nsAutoString          sURISpec,
                        sURIScheme,
                        sURIHostPort,
                        sURIPath;

  nsAutoString          sURIMethod;

  nsP3PServiceReadData *pReadData     = nsnull;

  PRBool                bAllowSubmit  = PR_TRUE;


  rv = P3PIsEnabled(&bP3PIsEnabled );

  if (bP3PIsEnabled) {

    if (IsHTTPProtocol( aCheckURI )) {
      rv = GetFormRequestMethod( aFormContent,
                                 sURIMethod );

      if (NS_SUCCEEDED( rv )) {
        rv = GetURIComponents( aCheckURI,
                               sURISpec,
                               sURIScheme,
                               sURIHostPort,
                               sURIPath );

        if (NS_SUCCEEDED( rv )) {
          csURISpec.AssignWithConversion( sURISpec );
          csMethod.AssignWithConversion( sURIMethod );

#ifdef DEBUG_P3P
          { printf( "P3P:  Checking Form Submit Privacy for %s using %s\n", (const char *)csURISpec, (const char *)csMethod );
          }
#endif

          PR_LOG( gP3PLogModule,
                  PR_LOG_NOTICE,
                  ("P3PService:  CheckPrivacyFormSubmit, checking form submission privacy for - %s using %s.\n", (const char *)csURISpec, (const char *)csMethod) );

          rv = NS_NewP3PServiceReadData( sURIMethod,
                                         aCheckURI,
                                         nsnull,
                        (nsISupports **)&pReadData );

          if (NS_SUCCEEDED( rv )) {
            rv = PrivacyStateMachine( pReadData,
                                      PR_FALSE,
                                      P3P_SYNC_READ,
                                      P3P_FORM_SUBMISSION_CHECK );

            NS_RELEASE( pReadData );
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PService:  CheckPrivacyFormSubmit, NS_NewP3PServiceReadData failed - %X.\n", rv) );
          }
        }
      }

#ifdef DEBUG_P3P
      { printf( "P3P:  Privacy result for %s: %08X\n", (const char *)csURISpec, rv );
      }
#endif

      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PService:  CheckPrivacyFormSubmit, privacy result for - %s: %X.\n", (const char *)csURISpec, rv) );

      // Display submit dialog...
      if (NS_SUCCEEDED( rv )) {

        if (rv == P3P_PRIVACY_NONE) {
          // Submitting to site without a privacy Policy
          mUIService->WarningPostToNoPolicy( aDOMWindowInternal,
                                            &bAllowSubmit );
        }
        else if (rv == P3P_PRIVACY_NOT_MET) {
          // Submitting to site that does not meet privacy
          mUIService->WarningPostToNotPrivate( aDOMWindowInternal,
                                              &bAllowSubmit );
        }
        else if (rv == P3P_PRIVACY_MET) {
          // Submitting to site that meets privacy, do nothing...
          bAllowSubmit = PR_TRUE;
        }
      }
      else {
        // Submitting to site with broken Policy
        mUIService->WarningPostToBrokenPolicy( aDOMWindowInternal,
                                              &bAllowSubmit );
      }
    }
  }

  if (bAllowSubmit) {
    *aCancelSubmit = PR_FALSE;
  }
  else {
    *aCancelSubmit = PR_TRUE;
  }

  return rv;
}

// P3P Service: GetCachedPolicy
//
// Function:  Get a cached Policy object.
//
// Parms:     1. In     The URI spec of the Policy
//            2. Out    The Policy object (null if the Policy is not cached)
//
NS_IMETHODIMP
nsP3PService::GetCachedPolicy( nsString&      aPolicyURISpec,
                               nsIP3PPolicy **aPolicy ) {

  nsresult     rv = NS_OK;

  nsStringKey  keyPolicyURISpec( aPolicyURISpec );


  NS_ENSURE_ARG_POINTER( aPolicy );

  *aPolicy = nsnull;

  if (mPolicies.Exists(&keyPolicyURISpec )) {
    *aPolicy = (nsIP3PPolicy *)mPolicies.Get(&keyPolicyURISpec );
  }

  return rv;
}


// P3P Service: CachePolicy
//
// Function:  Cache a Policy object.
//
// Parms:     1. In     The URI spec of the Policy
//            2. In     The Policy object to cache
//
NS_IMETHODIMP_( void )
nsP3PService::CachePolicy( nsString&      aPolicyURISpec,
                           nsIP3PPolicy  *aPolicy ) {

  nsStringKey  keyPolicyURISpec( aPolicyURISpec );


  if (!mPolicies.Exists(&keyPolicyURISpec )) {
    // Policy not currently cached, cache it
    mPolicies.Put(&keyPolicyURISpec,
                   aPolicy );
  }

  return;
}

// P3P Service: UncachePolicy
//
// Function:  Remove a Policy object from the cache.
//
// Parms:     1. In     The URI spec of the Policy
//            2. In     The Policy object to remove from the cache
NS_IMETHODIMP_( void )
nsP3PService::UncachePolicy( nsString&      aPolicyURISpec,
                             nsIP3PPolicy  *aPolicy ) {

  nsStringKey             keyPolicyURISpec( aPolicyURISpec );

  nsCOMPtr<nsIP3PPolicy>  pPolicy;


  if (mPolicies.Exists(&keyPolicyURISpec )) {
    // A Policy has been cached
    nsCOMPtr<nsISupports>  pEntry;

    pEntry  = getter_AddRefs( mPolicies.Get(&keyPolicyURISpec ) );
    pPolicy = do_QueryInterface( pEntry );

    if ((nsIP3PPolicy *)pPolicy == aPolicy) {
      // ...and it's the same Policy object
      mPolicies.Remove(&keyPolicyURISpec );
    }
  }

  return;
}

// P3P Service: GetCachedDataSchema
//
// Function:  Get a cached DataSchema object.
//
// Parms:     1. In     The URI spec of the DataSchema
//            2. Out    The DataSchema object (null if the DataSchema is not cached)
//
NS_IMETHODIMP
nsP3PService::GetCachedDataSchema( nsString&          aDataSchemaURISpec,
                                   nsIP3PDataSchema **aDataSchema ) {

  nsresult     rv = NS_OK;

  nsStringKey  keyDataSchemaURISpec( aDataSchemaURISpec );


  NS_ENSURE_ARG_POINTER( aDataSchema );

  *aDataSchema = nsnull;

  if (mDataSchemas.Exists(&keyDataSchemaURISpec )) {
    *aDataSchema = (nsIP3PDataSchema *)mDataSchemas.Get(&keyDataSchemaURISpec );
  }

  return rv;
}


// P3P Service: CacheDataSchema
//
// Function:  Cache a DataSchema object.
//
// Parms:     1. In     The URI spec of the DataSchema
//            2. In     The DataSchema object to cache
//
NS_IMETHODIMP_( void )
nsP3PService::CacheDataSchema( nsString&          aDataSchemaURISpec,
                               nsIP3PDataSchema  *aDataSchema ) {

  nsStringKey  keyDataSchemaURISpec( aDataSchemaURISpec );


  if (!mDataSchemas.Exists(&keyDataSchemaURISpec )) {
    // DataSchema not currently cached, cache it
    mDataSchemas.Put(&keyDataSchemaURISpec,
                      aDataSchema );
  }

  return;
}

// P3P Service: UncacheDataSchema
//
// Function:  Remove a DataSchema object from the cache.
//
// Parms:     1. In     The URI spec of the DataSchema
//            2. In     The DataSchema object to remove from the cache
NS_IMETHODIMP_( void )
nsP3PService::UncacheDataSchema( nsString&          aDataSchemaURISpec,
                                 nsIP3PDataSchema  *aDataSchema ) {

  nsStringKey                 keyDataSchemaURISpec( aDataSchemaURISpec );

  nsCOMPtr<nsIP3PDataSchema>  pDataSchema;


  if (mDataSchemas.Exists(&keyDataSchemaURISpec )) {
    // A DataSchema has been cached
    nsCOMPtr<nsISupports>  pEntry;

    pEntry      = getter_AddRefs( mDataSchemas.Get(&keyDataSchemaURISpec ) );
    pDataSchema = do_QueryInterface( pEntry );

    if ((nsIP3PDataSchema *)pDataSchema == aDataSchema) {
      // ...and it's the same DataSchema object
      mDataSchemas.Remove(&keyDataSchemaURISpec );
    }
  }

  return;
}

// P3P Service: GetPrivacyInfo
//
// Function:  Get the RDF description of the Privacy information
//
// Parms:     1. In     The DocShellTreeItem (browser instance) corresponding to the Privacy information to obtain
//            2. Out    The RDFDataSource describing the Privacy information
//
NS_IMETHODIMP
nsP3PService::GetPrivacyInfo( nsIDocShellTreeItem  *aDocShellTreeItem,
                              nsIRDFDataSource    **aDataSource ) {

  nsresult                       rv = NS_OK;

  nsVoidKey                      keyDocShellTreeItem( aDocShellTreeItem );

  nsP3PDocShellTreeItemData     *pDocShellTreeItemData = nsnull;


  NS_ENSURE_ARG_POINTER( aDataSource );

  *aDataSource = nsnull;

  if (mBrowserWindowMap.Exists(&keyDocShellTreeItem )) {
    pDocShellTreeItemData = (nsP3PDocShellTreeItemData *)mBrowserWindowMap.Get(&keyDocShellTreeItem );

    if (pDocShellTreeItemData) {
      rv = pDocShellTreeItemData->mPrivacyResult->GetPrivacyInfo( aDataSource );

      NS_RELEASE( pDocShellTreeItemData );
    }
  }

  return rv;
}

// P3P Service: IsHTTPProtocol
//
// Function:  Determines if the URI represents the HTTP protocol
//
// Parms:     1. In     The URI to check
//
NS_IMETHODIMP_( PRBool )
nsP3PService::IsHTTPProtocol( nsIURI  *aURI ) {

  nsresult      rv;

  nsAutoString  sURISpec,
                sURIScheme,
                sURIHostPort,
                sURIPath;


  PRBool        bHTTPProtocol = PR_FALSE;


  rv = GetURIComponents( aURI,
                         sURISpec,
                         sURIScheme,
                         sURIHostPort,
                         sURIPath );

  if (NS_SUCCEEDED( rv )) {

    if (sURIScheme.EqualsIgnoreCase( "http", 4 )) {
      bHTTPProtocol = PR_TRUE;
    }
  }

  return bHTTPProtocol;
}

// P3P Service: AddP3PRelatedURISpec
//
// Function:  Adds a URI spec to the list of P3P related URIs
//
// Parms:     1. In     The P3P related URI spec
//
NS_IMETHODIMP_( void )
nsP3PService::AddP3PRelatedURISpec( nsString&  aP3PRelatedURISpec ) {

  nsStringKey  keyP3PRelatedURISpec( aP3PRelatedURISpec );


  mP3PRelatedURISpecs.Put(&keyP3PRelatedURISpec,
                           nsnull );

  return;
}

// P3P Service: RemoveP3PRelatedURISpec
//
// Function:  Removes a URI spec from the list of P3P related URIs
//
// Parms:     1. In     The P3P related URI spec
//
NS_IMETHODIMP_( void )
nsP3PService::RemoveP3PRelatedURISpec( nsString&  aP3PRelatedURISpec ) {

  nsStringKey  keyP3PRelatedURISpec( aP3PRelatedURISpec );


  mP3PRelatedURISpecs.Remove(&keyP3PRelatedURISpec );

  return;
}

// P3P Service: IsP3PRelatedURI
//
// Function:  Checks if the URI is in the list of P3P related URIs
//
// Parms:     1. In     The URI
//
NS_IMETHODIMP_( PRBool )
nsP3PService::IsP3PRelatedURI( nsIURI  *aURI ) {

  nsresult        rv;

  nsXPIDLCString  xcsURISpec;

  nsAutoString    sURISpec;

  PRBool          bP3PRelatedURI = PR_FALSE;


  rv = aURI->GetSpec( getter_Copies( xcsURISpec ) );

  if (NS_SUCCEEDED( rv )) {
    sURISpec.AssignWithConversion((const char *)xcsURISpec );

    nsStringKey  keyP3PRelatedURISpec( sURISpec );

    bP3PRelatedURI = mP3PRelatedURISpecs.Exists(&keyP3PRelatedURISpec );
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PService:  IsP3PRelatedURI, aURI->GetSpec failed - %X.\n", rv) );
  }

  return bP3PRelatedURI;
}

// P3P Service: GetTimePrefsLastChanged
//
// Function:  Returns the time the P3P preferences were last changed.
//
// Parms:     1. Out    The time the P3P preferences were last changed
//
NS_IMETHODIMP
nsP3PService::GetTimePrefsLastChanged( PRTime  *aTime ) {

  return mPreferences->GetTimePrefsLastChanged( aTime );
}

// P3P Service: PrefsChanged
//
// Function:  Checks if the preferences have changed since a given time.
//
// Parms:     1. In     The time to perform the check against
//
NS_IMETHODIMP_( PRBool )
nsP3PService::PrefsChanged( PRTime   aTime ) {

  return mPreferences->PrefsChanged( aTime );
}

// P3P Service: GetBoolPref
//
// Function:  Return the value of the boolean preference combination.
//
// Parms:     1. In     The Policy <PURPOSE>
//            2. In     The Policy <RECIPIENT>
//            3. In     The Policy <CATEGORY>
//            4. Out    The indicator as to whether the boolean preference combination is set
//
NS_IMETHODIMP
nsP3PService::GetBoolPref( nsString&  aPurposeValue,
                           nsString&  aRecipientValue,
                           nsString&  aCategoryValue,
                           PRBool    *aResult ) {

  return mPreferences->GetBoolPref( aCategoryValue,
                                    aPurposeValue,
                                    aRecipientValue,
                                    aResult );
}

// P3P Service: GetLocaleString
//
// Function:  Return a string from a string bundle.
//
// Parms:     1. In     The key used to locate the string within the string bundle
//            2. Out    The string contained in the string bundle
//
NS_IMETHODIMP
nsP3PService::GetLocaleString( const char  *aKey,
                               nsString&    aValue ) {

  nsAutoString  sKey;


  sKey.AssignWithConversion( aKey );

  return GetLocaleString( sKey,
                          aValue );
}

// P3P Service: GetLocaleString
//
// Function:  Return a string from a string bundle.
//
// Parms:     1. In     The key used to locate the string within the string bundle
//            2. Out    The string contained in the string bundle
//
NS_IMETHODIMP
nsP3PService::GetLocaleString( nsString&  aKey,
                               nsString&  aValue ) {

  nsresult       rv;

  nsXPIDLString  xsValue;


  rv = mStringBundle->GetStringFromName( aKey.GetUnicode( ),
                                         getter_Copies( xsValue ) );

  if (NS_SUCCEEDED( rv )) {
    aValue = (const PRUnichar *)xsValue;
  }

  return rv;
}


// ****************************************************************************
// nsP3PService routines
// ****************************************************************************

// P3P Service: GetDocShellTreeItemMain
//
// Function:  Obtain the "Main" DocShellTreeItem which represents a browser window.
//
// Parms:     1. In     The DocShellTreeItem object
//            2. Out    The browser window DocShellTreeItem object
//
NS_METHOD
nsP3PService::GetDocShellTreeItemMain( nsIDocShellTreeItem  *aDocShellTreeItem,
                                       nsIDocShellTreeItem **aDocShellTreeItemMain ) {

  nsresult  rv;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItem,
                                 pDocShellTreeItemParent,
                                 pDocShellTreeItemRoot;


  NS_ENSURE_ARG_POINTER( aDocShellTreeItemMain );

  *aDocShellTreeItemMain = nsnull;

  rv = aDocShellTreeItem->GetRootTreeItem( getter_AddRefs( pDocShellTreeItemRoot ) );

  if (NS_SUCCEEDED( rv )) {
    pDocShellTreeItemParent = aDocShellTreeItem;
    do {
      pDocShellTreeItem = pDocShellTreeItemParent;

      rv = pDocShellTreeItem->GetParent( getter_AddRefs( pDocShellTreeItemParent ) );
    } while (NS_SUCCEEDED( rv ) && pDocShellTreeItemParent && (pDocShellTreeItemParent != pDocShellTreeItemRoot));

    if (NS_SUCCEEDED( rv )) {
      *aDocShellTreeItemMain = pDocShellTreeItem;
      NS_ADDREF( *aDocShellTreeItemMain );
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PService:  GetDocShellTreeItemMain, pDocShellTreeItem->GetParent failed - %X.\n", rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PService:  GetDocShellTreeItemMain, aDocShellTreeItem->GetRootTreeItem failed - %X.\n", rv) );
  }

  return rv;
}

// P3P Service: IsBrowserWindow
//
// Function:  Determines if the DocShellTreeItem object represents a browser window.
//
// Parms:     1. In     The DocShellTreeItem object
//
NS_METHOD_( PRBool )
nsP3PService::IsBrowserWindow( nsIDocShellTreeItem  *aDocShellTreeItemMain ) {

  nsVoidKey  keyDocShellTreeItemMain( aDocShellTreeItemMain );

  return mBrowserWindowMap.Exists(&keyDocShellTreeItemMain );
}

// P3P Service: GetURIComponents
//
// Function:  Return the components of the URI.
//
// Parms:     1. In     The URI object
//            2. Out    The URI spec
//            3. Out    The URI scheme
//            4. Out    The URI host name and port combination
//            5. Out    The URI path
//
NS_METHOD
nsP3PService::GetURIComponents( nsIURI    *aURI,
                                nsString&  aURISpec,
                                nsString&  aURIScheme,
                                nsString&  aURIHostPort,
                                nsString&  aURIPath ) {

  nsresult        rv;

  nsXPIDLCString  xcsBuffer;

  PRInt32         iPort;


  aURISpec.AssignWithConversion( "" );
  rv = aURI->GetSpec( getter_Copies( xcsBuffer ) );

  if (NS_SUCCEEDED( rv )) {
    aURISpec.AssignWithConversion((const char *)xcsBuffer );
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PService:  GetURIComponents, aURI->GetSpec failed - %X.\n", rv) );
  }

  if (NS_SUCCEEDED( rv )) {
    aURIScheme.AssignWithConversion( "" );
    rv = aURI->GetScheme( getter_Copies( xcsBuffer ) );

    if (NS_SUCCEEDED( rv )) {
      aURIScheme.AssignWithConversion((const char *)xcsBuffer );
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PService:  GetURIComponents, aURI->GetScheme failed - %X.\n", rv) );
    }
  }

  if (NS_SUCCEEDED( rv ) && !aURIScheme.EqualsIgnoreCase( "about" )) {
    aURIHostPort.AssignWithConversion( "" );
    rv = aURI->GetHost( getter_Copies( xcsBuffer ) );

    if (NS_SUCCEEDED( rv )) {
      aURIHostPort.AssignWithConversion((const char *)xcsBuffer );
      rv = aURI->GetPort(&iPort );

      if (NS_SUCCEEDED( rv ) && (iPort >= 0) && (iPort != 80)) {
        aURIHostPort.AppendWithConversion( ":" );
        aURIHostPort.AppendInt( iPort );
      }
      else if (NS_FAILED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PService:  GetURIComponents, aURI->GetPort failed - %X.\n", rv) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PService:  GetURIComponents, aURI->GetHost failed - %X.\n", rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    aURIPath.AssignWithConversion( "" );
    rv = aURI->GetPath( getter_Copies( xcsBuffer ) );

    if (NS_SUCCEEDED( rv )) {
      aURIPath.AssignWithConversion((const char *)xcsBuffer );
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PService:  GetURIComponents, aURI->GetPath failed - %X.\n", rv) );
    }
  }

  return rv;
}

// P3P Service: GetURIInformation
//
// Function:  Get a URIInformation object.
//
// Parms:     1. In     The DocShellTreeItem object
//            2. In     The URI spec
//            3. Out    The URIInformation object
//
NS_METHOD
nsP3PService::GetURIInformation( nsIDocShellTreeItem   *aDocShellTreeItem,
                                 nsString&              aURISpec,
                                 nsIP3PURIInformation **aURIInformation ) {

  nsresult                       rv;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItemMain;

  nsP3PDocShellTreeItemData     *pDocShellTreeItemData = nsnull;

  nsStringKey                    keyURISpec( aURISpec );


  NS_ENSURE_ARG_POINTER( aURIInformation );

  *aURIInformation = nsnull;

  rv = GetDocShellTreeItemMain( aDocShellTreeItem,
                                getter_AddRefs( pDocShellTreeItemMain ) );

  if (NS_SUCCEEDED( rv )) {
    nsVoidKey  keyDocShellTreeItemMain( pDocShellTreeItemMain );

    pDocShellTreeItemData = (nsP3PDocShellTreeItemData *)mBrowserWindowMap.Get(&keyDocShellTreeItemMain );

    if (pDocShellTreeItemData) {

      if (pDocShellTreeItemData->mURIInformations.Exists(&keyURISpec )) {
        *aURIInformation = (nsIP3PURIInformation *)pDocShellTreeItemData->mURIInformations.Get(&keyURISpec );
      }

      NS_RELEASE( pDocShellTreeItemData );
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PService:  GetURIInformation, pDocShellTreeItemData is null.\n") );

      rv = NS_ERROR_FAILURE;
    }
  }

  return rv;
}

// P3P Service: CreateURIInformation
//
// Function:  Create a URIInformation object.
//
// Parms:     1. In     The DocShellTreeItem object
//            2. In     The URI spec
//            3. Out    The URIInformation object
//
NS_METHOD
nsP3PService::CreateURIInformation( nsIDocShellTreeItem   *aDocShellTreeItem,
                                    nsString&              aURISpec,
                                    nsIP3PURIInformation **aURIInformation ) {

  nsresult                       rv;

  nsCOMPtr<nsIDocShellTreeItem>  pDocShellTreeItemMain;

  nsP3PDocShellTreeItemData     *pDocShellTreeItemData = nsnull;

  nsStringKey                    keyURISpec( aURISpec );


  NS_ENSURE_ARG_POINTER( aURIInformation );

  *aURIInformation = nsnull;

  nsCAutoString  csURISpec; csURISpec.AssignWithConversion( aURISpec );

#ifdef DEBUG_P3P
  { printf( "P3P:  Creating URIInformation Object for %s\n", (const char *)csURISpec );
  }
#endif

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PService:  CreateURIInformation, creating uriinformation for - %s.\n", (const char *)csURISpec) );

  rv = GetDocShellTreeItemMain( aDocShellTreeItem,
                                getter_AddRefs( pDocShellTreeItemMain ) );

  if (NS_SUCCEEDED( rv )) {
    nsVoidKey  keyDocShellTreeItemMain( pDocShellTreeItemMain );

    pDocShellTreeItemData = (nsP3PDocShellTreeItemData *)mBrowserWindowMap.Get(&keyDocShellTreeItemMain );

    if (pDocShellTreeItemData) {
      rv = NS_NewP3PURIInformation( aURISpec,
                                    aURIInformation );

      if (NS_SUCCEEDED( rv )) {
        pDocShellTreeItemData->mURIInformations.Put(&keyURISpec,
                                                     *aURIInformation );
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PService:  CreateURIInformation, NS_NewP3PURIInformation failed - %X.\n", rv) );
      }

      NS_RELEASE( pDocShellTreeItemData );
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PService:  CreateURIInformation, pDocShellTreeItemData is null.\n") );

      rv = NS_ERROR_FAILURE;
    }
  }

  return rv;
}

// P3P Service: GetReference
//
// Function:  Get a Reference object.
//
// Parms:     1. In     The host name and port combination
//            2. Out    The Reference object
//
NS_METHOD
nsP3PService::GetReference( nsString&         aURIHostPort,
                            nsIP3PReference **aReference ) {

  nsresult     rv = NS_OK;

  nsStringKey  keyReference( aURIHostPort );


  NS_ENSURE_ARG_POINTER( aReference );

  *aReference = nsnull;

  if (mReferences.Exists(&keyReference )) {
    *aReference = (nsIP3PReference *)mReferences.Get(&keyReference );
  }

  return rv;
}

// P3P Service: CreateReference
//
// Function:  Create a Reference object.
//
// Parms:     1. In     The host name and port combination
//            2. Out    The Reference object
//
NS_METHOD
nsP3PService::CreateReference( nsString&         aURIHostPort,
                               nsIP3PReference **aReference ) {

  nsresult     rv = NS_OK;

  nsStringKey  keyReference( aURIHostPort );


  NS_ENSURE_ARG_POINTER( aReference );

  nsCAutoString  csURIHostPort; csURIHostPort.AssignWithConversion( aURIHostPort );

#ifdef DEBUG_P3P
  { printf( "P3P:  Creating Reference Object for %s\n", (const char *)csURIHostPort );
  }
#endif

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PService:  CreateReference, creating reference for - %s.\n", (const char *)csURIHostPort) );

  rv = NS_NewP3PReference( aURIHostPort,
                           aReference );

  if (NS_SUCCEEDED( rv )) {
    mReferences.Put(&keyReference,
                     *aReference );
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PService:  CreateReference, NS_NewP3PReference failed - %X.\n", rv) );
  }

  return rv;
}

// P3P Service: CheckForEmbedding
//
// Function:  Determines if the object being checked is embedded (according to P3P spec).
//
// Parms:     1. In     The P3P Service read data
//            2. Out    The URI object of the embedding document
//
NS_METHOD
nsP3PService::CheckForEmbedding( nsP3PServiceReadData  *aReadData,
                                 nsIURI               **aEmbeddingURI ) {

  nsresult                        rv;

  nsAutoString                    sURISpec,
                                  sURIScheme,
                                  sURIHostPort,
                                  sURIPath;

  nsCOMPtr<nsIP3PURIInformation>  pURIInformation;

  nsAutoString                    sRefererHeader;

  nsCOMPtr<nsIURI>                pURI;

  nsAutoString                    sRefererURISpec,
                                  sRefererURIScheme,
                                  sRefererURIHostPort,
                                  sRefererURIPath;


  NS_ENSURE_ARG_POINTER( aEmbeddingURI );

  *aEmbeddingURI = nsnull;

  rv = GetURIComponents( aReadData->mCheckURI,
                         sURISpec,
                         sURIScheme,
                         sURIHostPort,
                         sURIPath );

  if (NS_SUCCEEDED( rv )) {
    rv = GetURIInformation( aReadData->mDocShellTreeItem,
                            sURISpec,
                            getter_AddRefs( pURIInformation ) );

    if (NS_SUCCEEDED( rv ) && pURIInformation) {
      pURIInformation->GetRefererHeader( sRefererHeader );

      if (sRefererHeader.Length( ) > 0) {
        // We have a "Referer" header, check if it references a different host name and port combination
        nsCAutoString  csURI;

        csURI.AssignWithConversion( sRefererHeader );
        rv = NS_NewURI( getter_AddRefs( pURI ),
          (const char *)csURI );

        if (NS_SUCCEEDED( rv )) {
          rv = GetURIComponents( pURI,
                                 sRefererURISpec,
                                 sRefererURIScheme,
                                 sRefererURIHostPort,
                                 sRefererURIPath );

          if (NS_SUCCEEDED( rv )) {

            if (sURIHostPort != sRefererURIHostPort) {
              // Different host name and port combination, it's considered embedded
              PR_LOG( gP3PLogModule,
                      PR_LOG_NOTICE,
                      ("P3PService:  CheckForEmbedding, embedding requirements present.\n") );

              *aEmbeddingURI = pURI;
              NS_ADDREF( *aEmbeddingURI );
            }
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PService:  CheckForEmbedding, NS_NewURI failed - %X.\n", rv) );
        }
      }
    }
  }

  return rv;
}

// P3P Service: PrivacyStateMachine
//
// Function:  Performs the steps necessary to obtain a privacy result.
//
// Parms:     1. In     The P3P Service read data
//            2. In     The type of read to be used
//            3. In     The type of privacy check being performed
//
NS_METHOD
nsP3PService::PrivacyStateMachine( nsP3PServiceReadData  *aReadData,
                                   PRBool                 aEmbedCheck,
                                   PRInt32                aReadType,
                                   PRInt32                aCheckType ) {

  nsresult      rv;

  nsAutoString  sURISpec,
                sURIScheme,
                sURIHostPort,
                sURIPath;

  nsAutoString  sEmbeddingURISpec,
                sEmbeddingURIScheme,
                sEmbeddingURIHostPort,
                sEmbeddingURIPath;

  PRBool        bPolicyExpired;


  rv = GetURIComponents( aReadData->mCheckURI,
                         sURISpec,
                         sURIScheme,
                         sURIHostPort,
                         sURIPath );

  if (NS_SUCCEEDED( rv ) && aReadData->mEmbeddingURI) {
    rv = GetURIComponents( aReadData->mEmbeddingURI,
                           sEmbeddingURISpec,
                           sEmbeddingURIScheme,
                           sEmbeddingURIHostPort,
                           sEmbeddingURIPath );
  }

  nsCAutoString  csURISpec; csURISpec.AssignWithConversion( sURISpec );
  nsCAutoString  csPolicyRefFileURISpec; 

  while (NS_SUCCEEDED( rv ) && (aReadData->mCheckState != P3P_CHECK_COMPLETE) && (rv != P3P_PRIVACY_IN_PROGRESS)) {

#ifdef DEBUG_P3P
    { printf( "P3P:    In state %i for %s\n", aReadData->mCheckState, (const char *)csURISpec );
    }
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PService:  PrivacyStateMachine, in state %i for - %s.\n", aReadData->mCheckState, (const char *)csURISpec) );

    switch (aReadData->mCheckState) {

      case P3P_CHECK_WELLKNOWN_POLICYREFFILE:
        // Process the "Well Known Location" PolicyRefFile
        rv = GetWellKnownLocationPolicyRefFile( aEmbedCheck ? aReadData->mEmbeddingURI : aReadData->mCheckURI,
                                                aEmbedCheck ? sEmbeddingURIHostPort : sURIHostPort,
                                                aReadData,
                                                aReadType );

        // An invalid "Well Known Location" PolicyRefFile is not an error condition
        if (NS_FAILED( rv ) || (rv != P3P_PRIVACY_IN_PROGRESS)) {
          rv = NS_OK;

          switch (aCheckType) {
            case P3P_OBJECT_CHECK:
              aReadData->mCheckState = P3P_CHECK_SPECIFIED_POLICYREFFILE;
              break;

            case P3P_FORM_SUBMISSION_CHECK:
              aReadData->mCheckState = P3P_CHECK_POLICY_REFERENCE;
              break;

            default:
              rv = NS_ERROR_FAILURE;
              break;
          }
        }
        break;

      case P3P_CHECK_SPECIFIED_POLICYREFFILE:
        // Process the specified PolicyRefFile
        rv = GetSpecifiedPolicyRefFile( aEmbedCheck ? sEmbeddingURISpec : sURISpec,
                                        aEmbedCheck ? sEmbeddingURIHostPort : sURIHostPort,
                                        aReadData,
                                        aReadType );

        if (NS_SUCCEEDED( rv )) {
          
          if (rv != P3P_PRIVACY_IN_PROGRESS) {
            aReadData->mCheckState = P3P_CHECK_POLICY_REFERENCE;
          }
        }
        break;

      case P3P_CHECK_READING_POLICYREFFILE:
        // Process the specified PolicyRefFile
        csPolicyRefFileURISpec.AssignWithConversion( aReadData->mReadingPolicyRefFileURISpec );
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  PrivacyStateMachine, reading - checking for completion of - %s.\n", (const char *)csPolicyRefFileURISpec) );

        rv = GetPolicyRefFile( aEmbedCheck ? sEmbeddingURIHostPort : sURIHostPort,
                               aReadData->mReadingPolicyRefFileURISpec,
                               aReadData->mReadingReferencePoint,
                               aReadData,
                               aReadType );
                               
        if (NS_SUCCEEDED( rv )) {

          if (rv != P3P_PRIVACY_IN_PROGRESS) {
            aReadData->mCheckState = P3P_CHECK_POLICY_REFERENCE;
          }
        }
        break;

      case P3P_CHECK_EXPIRED_POLICYREFFILE:
        // Process the specified PolicyRefFile
        csPolicyRefFileURISpec.AssignWithConversion( aReadData->mExpiredPolicyRefFileURISpec );
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PService:  PrivacyStateMachine, expired - checking for completion of - %s.\n", (const char *)csPolicyRefFileURISpec) );

        rv = GetPolicyRefFile( aEmbedCheck ? sEmbeddingURIHostPort : sURIHostPort,
                               aReadData->mExpiredPolicyRefFileURISpec,
                               aReadData->mExpiredReferencePoint,
                               aReadData,
                               aReadType );
                               
        if (NS_SUCCEEDED( rv )) {

          if (rv != P3P_PRIVACY_IN_PROGRESS) {
            aReadData->mCheckState = P3P_CHECK_POLICY_REFERENCE;
          }
        }
        break;

      case P3P_CHECK_POLICY_REFERENCE:
        // Get the Policy (use the full URI spec if this is an embedded third-party object)
        rv = GetPolicy( aEmbedCheck,
                        aEmbedCheck ? sEmbeddingURIHostPort : sURIHostPort,
                        aReadData->mMethod,
                        aEmbedCheck ? sURISpec : sURIPath,
                        aReadData->mReadingPolicyRefFileURISpec,
                        aReadData->mReadingReferencePoint,
                        aReadData->mExpiredPolicyRefFileURISpec,
                        aReadData->mExpiredReferencePoint,
                        getter_AddRefs( aReadData->mPolicy ) );

        if (NS_SUCCEEDED( rv )) {

          if (aReadData->mReadingPolicyRefFileURISpec.Length( )) {
            aReadData->mCheckState = P3P_CHECK_READING_POLICYREFFILE;
          }
          else if (aReadData->mExpiredPolicyRefFileURISpec.Length( )) {
            rv = ReplacePolicyRefFile( aEmbedCheck ? sEmbeddingURIHostPort : sURIHostPort,
                                       aReadData->mExpiredPolicyRefFileURISpec,
                                       aReadData->mExpiredReferencePoint );

            aReadData->mCheckState = P3P_CHECK_EXPIRED_POLICYREFFILE;
          }
          else {
            aReadData->mCheckState = P3P_CHECK_POLICY;
          }
        }
        break;

      case P3P_CHECK_POLICY:
        // Process the Policy file
        if (aReadData->mPolicy) {
          rv = aReadData->mPolicy->Read( aReadType,
                                         aReadData->mCheckURI,
                                         this,
                                         aReadData );

          if (NS_SUCCEEDED( rv )) {

            if (rv != P3P_PRIVACY_IN_PROGRESS) {
              rv = aReadData->mPolicy->ComparePolicyToPrefs( bPolicyExpired );

              if (NS_SUCCEEDED( rv )) {

                if (bPolicyExpired) {
                  // Policy has expired, try again
                  aReadData->mCheckState = P3P_CHECK_POLICY_REFERENCE;
                }
                else {
                  // Policy has not expired, we're done
                  aReadData->mCheckState = P3P_CHECK_COMPLETE;
                }
              }
            }
          }
        }
        else {
          aReadData->mCheckState = P3P_CHECK_COMPLETE;
          rv = P3P_PRIVACY_NONE;
        }
        break;

        default:
          // An invalid state has been encountered
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PService:  PrivacyStateMachine, invalid state encountered.\n") );

          rv = NS_ERROR_FAILURE;
          break;
    }
  }

  return rv;
}

// P3P Service: GetWellKnownLocationPolicyRefFile
//
// Function:  Returns the "well known location" PolicyReferencFile object.
//
// Parms:     1. In     The host name and port combination
//            2. In     The read data structure
//            3. In     The type of read to be used
//
NS_METHOD
nsP3PService::GetWellKnownLocationPolicyRefFile( nsIURI                *aBaseURI,
                                                 nsString&              aURIHostPort,
                                                 nsP3PServiceReadData  *aReadData,
                                                 PRInt32                aReadType ) {

  nsresult          rv;

  nsCOMPtr<nsIURI>  pPolicyRefFileURI;

  nsXPIDLCString    xcsPolicyRefFileURISpec;

  nsAutoString      sPolicyRefFileURISpec;


  rv = NS_NewURI( getter_AddRefs( pPolicyRefFileURI ),
                  P3P_WELL_KNOWN_LOCATION,
                  aBaseURI );

  if (NS_SUCCEEDED( rv )) {
    rv = pPolicyRefFileURI->GetSpec( getter_Copies( xcsPolicyRefFileURISpec ) );

    if (NS_SUCCEEDED( rv )) {
      sPolicyRefFileURISpec.AssignWithConversion((const char *)xcsPolicyRefFileURISpec );

      rv = GetPolicyRefFile( aURIHostPort,
                             sPolicyRefFileURISpec,
                             P3P_REFERENCE_WELL_KNOWN,
                             aReadData,
                             aReadType );
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PService:  GetWellKnownLocationPolicyRefFile, pPolicyRefFileURI->GetSpec failed - %X.\n", rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PService:  GetWellKnownLocationPolicyRefFile, NS_NewURI failed - %X.\n", rv) );
  }

  return rv;
}

// P3P Service: GetSpecifiedPolicyRefFile
//
// Function:  Returns the PolicyRefFile object associated with the specified URI (if present).
//
// Parms:     1. In     The URI spec
//            2. In     The host name and port combination
//            3. In     The read data structure
//            4. In     The type of read to be used
//
NS_METHOD
nsP3PService::GetSpecifiedPolicyRefFile( nsString&              aURISpec,
                                         nsString&              aURIHostPort,
                                         nsP3PServiceReadData  *aReadData,
                                         PRInt32                aReadType ) {

  nsresult                        rv;

  nsCOMPtr<nsIP3PURIInformation>  pURIInformation;

  nsStringArray                   sPolicyRefFileURISpecs;

  nsAutoString                    sPolicyRefFileURISpec;

  PRInt32                         iReferencePoint;


  rv = GetURIInformation( aReadData->mDocShellTreeItem,
                          aURISpec,
                          getter_AddRefs( pURIInformation ) );

  if (NS_SUCCEEDED( rv ) && pURIInformation) {
    pURIInformation->GetPolicyRefFileURISpecs(&sPolicyRefFileURISpecs );

    for (PRInt32 iCount = 0;
           NS_SUCCEEDED( rv ) && (rv != P3P_PRIVACY_IN_PROGRESS) && (iCount < sPolicyRefFileURISpecs.Count( ));
             iCount++) {
      sPolicyRefFileURISpecs.StringAt( iCount,
                                       sPolicyRefFileURISpec );

      pURIInformation->GetPolicyRefFileReferencePoint( sPolicyRefFileURISpec,
                                                       iReferencePoint );

      if (sPolicyRefFileURISpec.Length( ) > 0) {
        rv = GetPolicyRefFile( aURIHostPort,
                               sPolicyRefFileURISpec,
                               iReferencePoint,
                               aReadData,
                               aReadType );
      }
    }
  }

  return rv;
}

// P3P Service: GetPolicyRefFile
//
// Function:  Returns the PolicyReferencFile object (creating one if necessary).
//
// Parms:     1. In     The host name and port combination
//            2. In     The PolicyRefFile URI spec
//            3. In     The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            4. In     The read data structure
//            5. In     The type of read to be used
//
NS_METHOD
nsP3PService::GetPolicyRefFile( nsString&              aURIHostPort,
                                nsString&              aPolicyRefFileURISpec,
                                PRInt32                aReferencePoint,
                                nsP3PServiceReadData  *aReadData,
                                PRInt32                aReadType ) {

  nsresult                       rv;

  nsCOMPtr<nsIP3PReference>      pReference;

  nsCOMPtr<nsIP3PPolicyRefFile>  pPolicyRefFile;


  nsCAutoString  csURIHostPort; csURIHostPort.AssignWithConversion( aURIHostPort );
  nsCAutoString  csURISpec; csURISpec.AssignWithConversion( aPolicyRefFileURISpec );
  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PService:  GetPolicyRefFile, getting PolicyRefFile - %s for %s.\n", (const char *)csURISpec, (const char *)csURIHostPort) );

  rv = GetReference( aURIHostPort,
                     getter_AddRefs( pReference ) );

  if (NS_SUCCEEDED( rv ) && !pReference) {
    rv = CreateReference( aURIHostPort,
                          getter_AddRefs( pReference ) );
  }

  if (NS_SUCCEEDED( rv )) {
    rv = pReference->GetPolicyRefFile( aPolicyRefFileURISpec,
                                       getter_AddRefs( pPolicyRefFile ) );

    if (NS_SUCCEEDED( rv ) && !pPolicyRefFile) {
      rv = NS_NewP3PPolicyRefFile( aPolicyRefFileURISpec,
                                   aReferencePoint,
                                   getter_AddRefs( pPolicyRefFile ) );

      if (NS_SUCCEEDED( rv )) {
        rv = pReference->AddPolicyRefFile( aPolicyRefFileURISpec,
                                           pPolicyRefFile );

      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PService:  GetPolicyRefFile, NS_NewP3PPolicyRefFile failed - %X.\n", rv) );
      }
    }

    if (NS_SUCCEEDED( rv )) {
      rv = pPolicyRefFile->Read( aReadType,
                                 aReadData->mCheckURI,
                                 this,
                                 aReadData );
    }
  }

  return rv;
}

// P3P Service: ReplacePolicyRefFile
//
// Function:  Returns the PolicyReferencFile object after creating a new as a replacement.
//
// Parms:     1. In     The host name and port combination
//            2. In     The PolicyRefFile URI spec
//            3. In     The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//
NS_METHOD
nsP3PService::ReplacePolicyRefFile( nsString&  aURIHostPort,
                                    nsString&  aPolicyRefFileURISpec,
                                    PRInt32    aReferencePoint ) {

  nsresult                       rv;

  nsCOMPtr<nsIP3PReference>      pReference;

  nsCOMPtr<nsIP3PPolicyRefFile>  pPolicyRefFile;


  nsCAutoString  csURIHostPort; csURIHostPort.AssignWithConversion( aURIHostPort );
  nsCAutoString  csURISpec; csURISpec.AssignWithConversion( aPolicyRefFileURISpec );
  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PService:  ReplacePolicyRefFile, replacing PolicyRefFile - %s for %s.\n", (const char *)csURISpec, (const char *)csURIHostPort) );

  rv = GetReference( aURIHostPort,
                     getter_AddRefs( pReference ) );

  if (NS_SUCCEEDED( rv ) && !pReference) {
    rv = CreateReference( aURIHostPort,
                          getter_AddRefs( pReference ) );
  }

  if (NS_SUCCEEDED( rv )) {
    rv = NS_NewP3PPolicyRefFile( aPolicyRefFileURISpec,
                                 aReferencePoint,
                                 getter_AddRefs( pPolicyRefFile ) );

    if (NS_SUCCEEDED( rv )) {
      rv = pReference->AddPolicyRefFile( aPolicyRefFileURISpec,
                                         pPolicyRefFile );
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PService:  ReplacePolicyRefFile, NS_NewP3PPolicyRefFile failed - %X.\n", rv) );
    }
  }

  return rv;
}

// P3P Service: GetPolicy
//
// Function:  Returns the Policy object associated with a given host name and port combination, method
//            and path.
//
// Parms:     1. In     An indicator as to whether this check is for an embedded object
//            2. In     The host name and port combination
//            3. In     The access method being used
//            4. In     The path
//            5. Out    The reading PolicyRefFile URI spec
//            6. Out    The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            7. Out    The expired PolicyRefFile URI spec
//            8. Out    The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            9. Out    The Policy object
//
NS_METHOD
nsP3PService::GetPolicy( PRBool         aEmbedCheck,
                         nsString&      aURIHostPort,
                         nsString&      aURIMethod,
                         nsString&      aURIPath,
                         nsString&      aReadingPolicyRefFileURISpec,
                         PRInt32&       aReadingReferencePoint,
                         nsString&      aExpiredPolicyRefFileURISpec,
                         PRInt32&       aExpiredReferencePoint,
                         nsIP3PPolicy **aPolicy ) {

  nsresult                   rv;

  nsCOMPtr<nsIP3PReference>  pReference;


  NS_ENSURE_ARG_POINTER( aPolicy );

  aExpiredPolicyRefFileURISpec.Truncate( );
  *aPolicy = nsnull;

  rv = GetReference( aURIHostPort,
                     getter_AddRefs( pReference ) );

  if (NS_SUCCEEDED( rv ) && pReference) {
    // We have a Reference object, continue on
    rv = pReference->GetPolicy( aEmbedCheck,
                                aURIMethod,
                                aURIPath,
                                aReadingPolicyRefFileURISpec,
                                aReadingReferencePoint,
                                aExpiredPolicyRefFileURISpec,
                                aExpiredReferencePoint,
                                aPolicy );
  }

  return rv;
}

// P3P Service: SetPrivacyResult
//
// Function:  Sets and maintains the PrivacyResult object for the root document.
//
// Parms:     1. In     The URI object that the privacy check was performed for
//            2. In     The DocShellTreeItem object containing the URI object
//            3. In     The result of the comparison of the Policy to the user's preferences
//            4. In     The Policy object used in the comparison
//
NS_METHOD
nsP3PService::SetPrivacyResult( nsIURI               *aCheckURI,
                                nsIDocShellTreeItem  *aDocShellTreeItem,
                                nsresult              aPrivacyResult,
                                nsIP3PPolicy         *aPolicy ) {

  nsresult                       rv;

  nsVoidKey                      keyDocShellTreeItem( aDocShellTreeItem );

  nsP3PDocShellTreeItemData     *pDocShellTreeItemData = nsnull;

  nsAutoString                   sCheckURISpec,
                                 sNull;

  nsCOMPtr<nsIP3PPrivacyResult>  pPrivacyResult;


  pDocShellTreeItemData = (nsP3PDocShellTreeItemData *)mBrowserWindowMap.Get(&keyDocShellTreeItem );

  if (pDocShellTreeItemData) {
    rv = GetURIComponents( aCheckURI,
                           sCheckURISpec,
                           sNull,
                           sNull,
                           sNull );

    if (NS_SUCCEEDED( rv )) {
      nsCOMPtr<nsISupports>  pEntry;
      nsStringKey            keyCheckURISpec( sCheckURISpec );

      pEntry         = getter_AddRefs( pDocShellTreeItemData->mPrivacyResultMap.Get(&keyCheckURISpec ) );
      pPrivacyResult = do_QueryInterface( pEntry,
                                         &rv );

      if (NS_SUCCEEDED( rv )) {
        rv = pPrivacyResult->SetPrivacyResult( aPrivacyResult,
                                               aPolicy );
      }
    }

    NS_RELEASE( pDocShellTreeItemData );
  }

  return rv;
}

// P3P Service: GetDocShellTreeItemURI
//
// Function:  Returns the URI object associated with a DocShellTreeItem object.
//
// Parms:     1. In     The DocShellTreeItem object
//            2. Out    The URI object
//
NS_METHOD
nsP3PService::GetDocShellTreeItemURI( nsIDocShellTreeItem  *aDocShellTreeItem,
                                      nsIURI              **aURI ) {

  nsresult                    rv;

  nsCOMPtr<nsIWebNavigation>  pWebNavigation;


  NS_ENSURE_ARG_POINTER( aURI );

  *aURI = nsnull;

  pWebNavigation = do_QueryInterface( aDocShellTreeItem,
                                     &rv );

  if (NS_SUCCEEDED( rv )) {
    rv = pWebNavigation->GetCurrentURI( aURI );

    if (NS_SUCCEEDED( rv ) && !aURI) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PService:  GetDocShellTreeItemURI, no URI obtained from the DocShellTreeItem.\n") );

      rv = NS_ERROR_FAILURE;
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PService:  GetDocShellTreeItemURI, pWebNavigation->GetCurrentURI failed - %X.\n", rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PService:  GetDocShellTreeItemURI, do_QueryInterface for nsIWebNavigation failed - %X.\n", rv) );
  }

  return rv;
}

// P3P Service: DeleteCookies
//
// Function:  Delete the cookies contained in the "Set-Cookie" header related to a URI.
//
// Parms:     1. In     The DocShellTreeItem of the URI
//            2. In     The URI spec
//            3. In     The URI object
//
NS_METHOD
nsP3PService::DeleteCookies( nsIDocShellTreeItem  *aDocShellTreeItem,
                             nsString&             aURISpec,
                             nsIURI               *aURI ) {

  nsresult                        rv;

  nsCOMPtr<nsIP3PURIInformation>  pURIInformation;

  nsAutoString                    sSetCookieValues,
                                  sWork;

  PRInt32                         iCharOffset;


  nsCAutoString  csURISpec; csURISpec.AssignWithConversion( aURISpec );
  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PService:  DeleteCookies, deleting cookies associated with - %s.\n", (const char *)csURISpec) );

  rv = GetURIInformation( aDocShellTreeItem,
                          aURISpec,
                          getter_AddRefs( pURIInformation ) );

  if (NS_SUCCEEDED( rv ) && pURIInformation) {
    pURIInformation->GetCookieHeader( sSetCookieValues );

    if (sSetCookieValues.Length( ) > 0) {
      do {
        iCharOffset = sSetCookieValues.Find( "\n" );
        sSetCookieValues.Left( sWork,
                               iCharOffset );
        sSetCookieValues.Cut( 0,
                              iCharOffset + 1 );

        rv = DeleteCookie( aURI,
                           sWork );

      } while (NS_SUCCEEDED( rv ) && (iCharOffset != kNotFound));
    }
  }

  return rv;
}

// P3P Service: DeleteCookie
//
// Function:  Deletes a specific cookie (by setting an "old" expire date).
//
// Parms:     1. In     The URI object
//            2. In     The "Set-Cookie" response header value
//
NS_METHOD
nsP3PService::DeleteCookie( nsIURI    *aURI,
                            nsString&  aSetCookieValue ) {

  nsresult      rv;

  nsAutoString  sExpires,
                sWork( aSetCookieValue );

  PRInt32       iExpiresOffset,
                iExpiresEnd;


  sExpires.AssignWithConversion( "expires=" );
  sExpires.AppendWithConversion( mGMTStartTime );

  sWork.Trim( " " );

  iExpiresOffset = sWork.Find( "expires=",
                               PR_TRUE );

  if (iExpiresOffset >= 0) {
    iExpiresEnd = sWork.Find( ";",
                              PR_FALSE,
                              iExpiresOffset );

    if (iExpiresEnd >= 0) {
      sWork.Cut( iExpiresOffset,
                 iExpiresEnd - iExpiresOffset );
    }
    else {
      sWork.Truncate( iExpiresOffset );
    }

    sWork.Insert( sExpires, iExpiresOffset );
  }
  else {
    char  lastChar = (char)sWork.Last( );

    if (lastChar != ';') {
      sWork.AppendWithConversion( ";" );
    }

    sWork.AppendWithConversion( " " );
    sWork += sExpires;
  }

  nsCAutoString  csCookie; csCookie.AssignWithConversion( sWork );
  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PService:  DeleteCookie, deleting cookie - %s.\n", (const char *)csCookie) );

  rv = mCookieService->SetCookieString( aURI,
                                        nsnull,
                                        csCookie );

  return rv;
}

// P3P Service: UpdateBrowserWindow
//
// Function:  Updates the browser window instance with the specified result.
//
// Parms:     1. In     The DocShellTreeItem that represents the browser window instance to be udpated
//            2. In     The privacy result
//
NS_METHOD_( void )
nsP3PService::UpdateBrowserWindow( nsIDocShellTreeItem  *aDocShellTreeItemMain,
                                   nsresult              aPrivacyResult ) {

  if (NS_SUCCEEDED( aPrivacyResult )) {

    if (aPrivacyResult == P3P_PRIVACY_NONE) {
      mUIService->MarkNoPrivacy( aDocShellTreeItemMain );
    }
    else if (aPrivacyResult == P3P_PRIVACY_IN_PROGRESS) {
      mUIService->MarkInProgress( aDocShellTreeItemMain );
    }
    else if (aPrivacyResult == P3P_PRIVACY_NOT_MET) {
      mUIService->MarkNotPrivate( aDocShellTreeItemMain );
    }
    else if (aPrivacyResult == P3P_PRIVACY_PARTIAL) {
      mUIService->MarkPartialPrivacy( aDocShellTreeItemMain );
    }
    else if (aPrivacyResult == P3P_PRIVACY_MET) {
      mUIService->MarkPrivate( aDocShellTreeItemMain );
    }
  }
  else {
    mUIService->MarkPrivacyBroken( aDocShellTreeItemMain );
  }

  return;
}

// P3P Service: GetFormRequestMethod
//
// Function:  Returns the method used for a form submission.
//
// Parms:     1. In     The Content of the <FORM> tag
//            2. Out    The method used to submit the form
//
NS_METHOD
nsP3PService::GetFormRequestMethod( nsIContent  *aFormContent,
                                    nsString&    aURIMethod ) {

  nsresult           rv;

  PRInt32            iAttributeCount,
                     iCount;

  PRBool             bAttributeFound;

  PRInt32            iNameSpaceID;

  nsCOMPtr<nsIAtom>  pName,
                     pInclude;

  nsAutoString       sName;


  rv = aFormContent->GetAttributeCount( iAttributeCount );

  if (NS_SUCCEEDED( rv )) {
    for (iCount = 0, bAttributeFound = PR_FALSE;
           NS_SUCCEEDED( rv ) && (iCount < iAttributeCount);
             iCount++) {
      rv = aFormContent->GetAttributeNameAt( iCount,
                                             iNameSpaceID,
                                             *getter_AddRefs( pName ),
                                             *getter_AddRefs( pInclude ) );

      if (NS_SUCCEEDED( rv )) {
        rv = pName->ToString( sName );

        if (NS_SUCCEEDED( rv )) {
        
          if (sName.EqualsIgnoreCase( "method" )) {
            bAttributeFound = PR_TRUE;

            rv = aFormContent->GetAttribute( iNameSpaceID,
                                             pName,
                                             *getter_AddRefs( pInclude ),
                                             aURIMethod );

            if (NS_FAILED( rv )) {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PService:  GetFormRequestMethod, aFormContent->GetAttribute failed - %X.\n", rv) );
            }
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PService:  GetFormRequestMethod, pName->ToString failed - %X.\n", rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PService:  GetFormRequestMethod, aFormContent->GetAttributeNameAt failed - %X.\n", rv) );
      }
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PService:  GetFormRequestMethod, aFormContent->GetAttributeCount failed - %X.\n", rv) );
  }

  if (NS_SUCCEEDED( rv ) && !bAttributeFound) {
    aURIMethod.AssignWithConversion( "GET" );
  }

  return rv;
}


// ****************************************************************************
// nsSupportsHashtable Enumeration Functor
// ****************************************************************************

// P3P Service: MarkNoP3P
//
// Function:  Hashtable enumerator functor that turns off the P3P privacy UI indicator.
//
// Parms:     1. In     The HashKey
//            2. In     The nsSupportsHashtable entry
//            3. In     The P3P Service
//
PRBool PR_CALLBACK MarkNoP3P( nsHashKey  *aKey,
                              void       *aValue,
                              void       *aClosure ) {

  nsIP3PUIService     *pUIService        = nsnull;

  nsIDocShellTreeItem *pDocShellTreeItem = nsnull;


  pUIService        = (nsIP3PUIService *)aClosure;
  pDocShellTreeItem = (nsIDocShellTreeItem *)aKey->HashCode( );

  pUIService->MarkNoP3P( pDocShellTreeItem );

  return PR_TRUE;
}


// ****************************************************************************
// nsP3PBrowserWindowData Implementation routines
// ****************************************************************************

// P3P Browser Window Data: nsISupports
NS_IMPL_ISUPPORTS1( nsP3PBrowserWindowData, nsISupports );

// P3P Browser Window Data: Creation routine
NS_METHOD
NS_NewP3PBrowserWindowData( nsISupports **aBrowserWindowData ) {

  nsresult                rv;

  nsP3PBrowserWindowData *pNewBrowserWindowData = nsnull;


  NS_ENSURE_ARG_POINTER( aBrowserWindowData );

  *aBrowserWindowData = nsnull;

  pNewBrowserWindowData = new nsP3PBrowserWindowData( );

  if (pNewBrowserWindowData) {
    NS_ADDREF( pNewBrowserWindowData );

    rv = pNewBrowserWindowData->QueryInterface( NS_GET_IID( nsISupports ),
                                       (void **)aBrowserWindowData );

    NS_RELEASE( pNewBrowserWindowData );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Unable to create BrowserWindowData object\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Browser Window Data: Constructor
nsP3PBrowserWindowData::nsP3PBrowserWindowData( )
: mDocumentComplete( PR_FALSE ),
  mOverallPrivacyResult( P3P_PRIVACY_IN_PROGRESS ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Browser Window Data: Destructor
nsP3PBrowserWindowData::~nsP3PBrowserWindowData( ) {
}


// ****************************************************************************
// nsP3PDocShellTreeItemData Implementation routines
// ****************************************************************************

// P3P DocShell Tree Item Data: nsISupports
NS_IMPL_ISUPPORTS1( nsP3PDocShellTreeItemData, nsISupports );

// P3P DocShell Tree Item Data: Creation routine
NS_METHOD
NS_NewP3PDocShellTreeItemData( nsSupportsHashtable  *aBrowserWindowMap,
                               nsISupports         **aDocShellTreeItemData ) {

  nsresult                   rv;

  nsP3PDocShellTreeItemData *pNewDocShellTreeItemData = nsnull;


  NS_ENSURE_ARG_POINTER( aDocShellTreeItemData );

  *aDocShellTreeItemData = nsnull;

  pNewDocShellTreeItemData = new nsP3PDocShellTreeItemData( aBrowserWindowMap );

  if (pNewDocShellTreeItemData) {
    NS_ADDREF( pNewDocShellTreeItemData );

    rv = pNewDocShellTreeItemData->QueryInterface( NS_GET_IID( nsISupports ),
                                          (void **)aDocShellTreeItemData );

    NS_RELEASE( pNewDocShellTreeItemData );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Unable to create DocShellTreeItemData object\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P DocShell Tree Item Data: Constructor
nsP3PDocShellTreeItemData::nsP3PDocShellTreeItemData( nsSupportsHashtable  *aBrowserWindowMap )
  : mURIInformations( 64, PR_TRUE ),
    mDocShellTreeItemDataChildMap( 16, PR_TRUE ),
    mPrivacyResultMap( 64, PR_TRUE ),
    mBrowserWindowMap( aBrowserWindowMap ) {

  NS_INIT_ISUPPORTS( );
}

// P3P DocShell Tree Item Data: Destructor
nsP3PDocShellTreeItemData::~nsP3PDocShellTreeItemData( ) {

  nsCOMPtr<nsIP3PPrivacyResult>  pPrivacyResultParent;


  mDocShellTreeItemDataChildMap.Enumerate( RemoveBrowserWindow, mBrowserWindowMap );

  if (mPrivacyResult) {
    mPrivacyResult->RemoveChildren( );

    mPrivacyResult->GetParent( getter_AddRefs( pPrivacyResultParent ) );

    if (pPrivacyResultParent) {
      pPrivacyResultParent->RemoveChild( mPrivacyResult );
    }
  }
}


// ****************************************************************************
// nsSupportsHashtable Enumeration Functor
// ****************************************************************************

// P3P Service: RemoveBrowserWindow
//
// Function:  Hashtable enumerator functor that removes the DocShellTreeItem from
//            the map of browser windows
//
// Parms:     1. In     The HashKey
//            2. In     The DocShellTreeItemData entry
//            3. In     The P3P Service
//
PRBool PR_CALLBACK RemoveBrowserWindow( nsHashKey  *aKey,
                                        void       *aValue,
                                        void       *aClosure ) {

  nsIDocShellTreeItem *pDocShellTreeItem = nsnull;

  nsSupportsHashtable *pWindowBrowserMap = nsnull;


  pDocShellTreeItem = (nsIDocShellTreeItem *)aKey->HashCode( );
  pWindowBrowserMap = (nsSupportsHashtable *)aClosure;

  nsVoidKey  keyDocShellTreeItem( pDocShellTreeItem );
  pWindowBrowserMap->Remove(&keyDocShellTreeItem );

  return PR_TRUE;
}


// ****************************************************************************
// nsP3PServiceReadData Implementation routines
// ****************************************************************************

// P3P Service Read Data: nsISupports
NS_IMPL_ISUPPORTS1( nsP3PServiceReadData, nsISupports );

// P3P Service Read Data: Creation routine
NS_METHOD
NS_NewP3PServiceReadData( nsString&             aURIMethod,
                          nsIURI               *aCheckURI,
                          nsIDocShellTreeItem  *aDocShellTreeItem,
                          nsISupports         **aServiceReadData ) {

  nsresult              rv;

  nsP3PServiceReadData *pNewServiceReadData = nsnull;


  NS_ENSURE_ARG_POINTER( aServiceReadData );

  *aServiceReadData = nsnull;

  pNewServiceReadData = new nsP3PServiceReadData( );

  if (pNewServiceReadData) {
    NS_ADDREF( pNewServiceReadData );

    pNewServiceReadData->mMethod           = aURIMethod;
    pNewServiceReadData->mCheckURI         = aCheckURI;
    pNewServiceReadData->mDocShellTreeItem = aDocShellTreeItem;

    rv = pNewServiceReadData->QueryInterface( NS_GET_IID( nsISupports ),
                                     (void **)aServiceReadData );

    NS_RELEASE( pNewServiceReadData );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Unable to create ServiceReadData object\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Service Read Data: Constructor
nsP3PServiceReadData::nsP3PServiceReadData( )
  : mCheckState( P3P_CHECK_INITIAL_STATE ),
    mEmbedDetermined( PR_FALSE ),
    mEmbedCheckComplete( PR_FALSE ),
    mReadingReferencePoint( 0 ),
    mExpiredReferencePoint( 0 ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Service Read Data: Destructor
nsP3PServiceReadData::~nsP3PServiceReadData( ) {
}


// ****************************************************************************
// nsIP3PXMLListener routines
// ****************************************************************************

// P3P Service: Notification of completed XML document
//
// Function:  Notification of the completion of a PolicyRefFile or Policy URI.
//
// Parms:     1. In     The URI spec that has completed
//            2. In     The data supplied on the intial processing request
//
NS_IMETHODIMP
nsP3PService::DocumentComplete( nsString&     aProcessedURISpec,
                                nsISupports  *aReaderData ) {

  nsP3PServiceReadData *pReadData = nsnull;


  nsCAutoString  csURISpec; csURISpec.AssignWithConversion( aProcessedURISpec );
  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PService:  DocumentComplete, %s complete.\n", (const char *)csURISpec) );

  pReadData = (nsP3PServiceReadData *)aReaderData;

  CheckPrivacy( pReadData->mMethod,
                pReadData->mCheckURI,
                pReadData->mDocShellTreeItem,
                pReadData );

  return NS_OK;
}
