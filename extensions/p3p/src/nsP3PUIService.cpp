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
#include "nsP3PUIService.h"
#include "nsIP3PCUI.h"
#include "nsP3PLogging.h"

#include <nsIGenericFactory.h>
#include <nsIP3PUI.h>
#include <nsIScriptGlobalObject.h>
#include <nsIScriptGlobalObjectOwner.h>
#include <nsLayoutCID.h>
#include <nsINetSupportDialogService.h>
#include <nsICommonDialogs.h>

// ****************************************************************************
// nsP3PUIService Implementation routines
// ****************************************************************************

static NS_DEFINE_CID( kNameSpaceManagerCID, NS_NAMESPACEMANAGER_CID );
static NS_DEFINE_CID( kCommonDialogsCID,    NS_CommonDialog_CID );

// P3P UI Service generic factory constructor
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT( nsP3PUIService, Init );

// P3P UI Service nsISupports
NS_IMPL_ISUPPORTS1( nsP3PUIService, nsIP3PUIService);

// P3P UI Service Constructor
nsP3PUIService::nsP3PUIService( )
  : mInitialized( PR_FALSE ),
    mP3PUIs( 256, PR_TRUE ) {

  NS_INIT_ISUPPORTS( );
}

// P3P UI Service Destructor
nsP3PUIService::~nsP3PUIService( void ) {
}

// P3P UI Service create instance routine
NS_METHOD
nsP3PUIService::Create( nsISupports  *aOuter,
                        REFNSIID      aIID,
                        void        **aResult ) {

#ifdef DEBUG_P3P
  printf("P3P:  P3PUIService::Create()\n");
#endif

  return nsP3PUIServiceConstructor( aOuter,
                                    aIID,
                                    aResult );
}

// P3P UI Service initialization routine
//
// Function:  Initialize the P3P UI Service.
//
NS_METHOD
nsP3PUIService::Init( ) {

  nsresult  rv = NS_OK;

#ifdef DEBUG_P3P
  printf("P3P:  P3PUIService::Init()\n");
#endif

  // Only initialize once.
  if (!mInitialized) {
    mInitialized = PR_TRUE;

    // nsP3PService creates gP3PLogModule.  nsP3PService creates this
    // nsP3PUIService after it creates gP3PLogModule, so we know
    // gP3PLogModule is valid.

    PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  Initializing.\n") );

    // Get the preference service.
    PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  Get the P3P Preference Service.\n") );
    mPrefServ = do_GetService( NS_PREF_CONTRACTID, &rv );

    PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  Initialization successful.\n") );
  } else {
    NS_ASSERTION( 0, "P3P:  P3PUIService initialized more than once.\n" );
    rv = NS_ERROR_ALREADY_INITIALIZED;
  }

#ifdef DEBUG_P3P
  if (NS_FAILED( rv )) {
    printf( "P3P:  Unable to initialize P3P UI Service: %X\n", rv );
  }
#endif

  return rv;
}

// ****************************************************************************
// nsIP3PUIService routines
// ****************************************************************************

// P3P UI Service:  Register UI Instance
//
// Function:  Register the instance of the UI handler.
//
// Parms:     1. In:  nsIDOMWindowInternal of the UI handler
//

NS_IMETHODIMP
nsP3PUIService::RegisterUIInstance(nsIDocShellTreeItem * aDocShellTreeItem, nsIP3PCUI * aP3PUI)
{
  nsresult rv = NS_OK;

  nsISupportsKey  keyDocShellTreeItem( aDocShellTreeItem );

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUIService::RegisterUIInstance()\n");
#endif

  // nsP3PService creates gP3PLogModule.  nsP3PService creates this
  // nsP3PPreferences after it creates gP3PLogModule, so we know
  // gP3PLogModule is valid.

  PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  RegisterUIInstance(%X, %X)\n", aDocShellTreeItem, aP3PUI) );

  if (!mP3PService) {
    mP3PService = do_GetService( NS_P3PSERVICE_CONTRACTID, &rv );
  }

  // Add the UI instance to the hash table.
  if (NS_SUCCEEDED(rv)) {
    mP3PUIs.Put(&keyDocShellTreeItem, aP3PUI );
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::DeregisterUIInstance(nsIDocShellTreeItem * aDocShellTreeItem, nsIP3PCUI * aP3PUI)
{
  nsresult rv = NS_OK;

  nsISupportsKey  keyDocShellTreeItem( aDocShellTreeItem );

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUIService::DeregisterUIInstance()\n");
#endif

  PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  DeregisterUIInstance(%X, %X)\n", aDocShellTreeItem, aP3PUI) );

  // Remove the UI instance to the hash table.
  mP3PUIs.Remove(&keyDocShellTreeItem);

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::MarkNoP3P(nsIDocShellTreeItem * aDocShellTreeItem)
{
  nsresult rv;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUIService::MarkNoP3P()\n");
#endif

  nsISupportsKey  keyDocShellTreeItem( aDocShellTreeItem );

  if (mP3PUIs.Exists(&keyDocShellTreeItem)) {

    nsCOMPtr<nsIP3PCUI> pP3PUI;
    pP3PUI = getter_AddRefs((nsIP3PCUI *)mP3PUIs.Get(&keyDocShellTreeItem));
    rv = pP3PUI->MarkNoP3P();

  } else {
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::MarkNoPrivacy(nsIDocShellTreeItem * aDocShellTreeItem)
{
  nsresult rv;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUIService::MarkNoPrivacy()\n");
#endif

  nsISupportsKey  keyDocShellTreeItem( aDocShellTreeItem );

  if (mP3PUIs.Exists(&keyDocShellTreeItem)) {

    nsCOMPtr<nsIP3PCUI> pP3PUI;
    pP3PUI = getter_AddRefs((nsIP3PCUI *)mP3PUIs.Get(&keyDocShellTreeItem));
    rv = pP3PUI->MarkNoPrivacy();

  } else {
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::MarkPrivate(nsIDocShellTreeItem * aDocShellTreeItem)
{
  nsresult rv;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUIService::MarkPrivate()\n");
#endif

  nsISupportsKey  keyDocShellTreeItem( aDocShellTreeItem );

  if (mP3PUIs.Exists(&keyDocShellTreeItem)) {

    nsCOMPtr<nsIP3PCUI> pP3PUI;
    pP3PUI = getter_AddRefs((nsIP3PCUI *)mP3PUIs.Get(&keyDocShellTreeItem));
    rv = pP3PUI->MarkPrivate();

  } else {
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::MarkNotPrivate(nsIDocShellTreeItem * aDocShellTreeItem)
{
  nsresult rv;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUIService::MarkNotPrivate()\n");
#endif

  nsISupportsKey  keyDocShellTreeItem( aDocShellTreeItem );

  if (mP3PUIs.Exists(&keyDocShellTreeItem)) {

    nsCOMPtr<nsIP3PCUI> pP3PUI;
    pP3PUI = getter_AddRefs((nsIP3PCUI *)mP3PUIs.Get(&keyDocShellTreeItem));
    rv = pP3PUI->MarkNotPrivate();

  } else {
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::MarkPartialPrivacy(nsIDocShellTreeItem * aDocShellTreeItem)
{
  nsresult rv;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUIService::MarkPartialPrivacy()\n");
#endif

  nsISupportsKey  keyDocShellTreeItem( aDocShellTreeItem );

  if (mP3PUIs.Exists(&keyDocShellTreeItem)) {

    nsCOMPtr<nsIP3PCUI> pP3PUI;
    pP3PUI = getter_AddRefs((nsIP3PCUI *)mP3PUIs.Get(&keyDocShellTreeItem));
    rv = pP3PUI->MarkPartialPrivacy();

  } else {
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::MarkPrivacyBroken(nsIDocShellTreeItem * aDocShellTreeItem)
{
  nsresult rv;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUIService::MarkPrivacyBroken()\n");
#endif

  nsISupportsKey  keyDocShellTreeItem( aDocShellTreeItem );

  if (mP3PUIs.Exists(&keyDocShellTreeItem)) {

    nsCOMPtr<nsIP3PCUI> pP3PUI;
    pP3PUI = getter_AddRefs((nsIP3PCUI *)mP3PUIs.Get(&keyDocShellTreeItem));
    rv = pP3PUI->MarkPrivacyBroken();

  } else {
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::MarkInProgress(nsIDocShellTreeItem * aDocShellTreeItem)
{
  nsresult rv;

#ifdef DEBUG_P3P
  printf("P3P:  nsP3PUIService::MarkInProgress()\n");
#endif

  nsISupportsKey  keyDocShellTreeItem( aDocShellTreeItem );

  if (mP3PUIs.Exists(&keyDocShellTreeItem)) {

    nsCOMPtr<nsIP3PCUI> pP3PUI;
    pP3PUI = getter_AddRefs((nsIP3PCUI *)mP3PUIs.Get(&keyDocShellTreeItem));
    rv = pP3PUI->MarkInProgress();

  } else {
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::WarningNotPrivate(nsIDocShellTreeItem * aDocShellTreeItem)
{
  nsresult rv;
  nsCOMPtr<nsIDOMWindowInternal> pDOMWindowInternal;

  PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  WarningNotPrivate(%X)\n", aDocShellTreeItem) );

  rv = DocShellTreeItemToDOMWindowInternal(aDocShellTreeItem, getter_AddRefs(pDOMWindowInternal));
  if (NS_SUCCEEDED(rv)) {
    rv = WarningNotPrivate(pDOMWindowInternal);
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::WarningNotPrivate(nsIDOMWindowInternal * aDOMWindowInternal)
{
  nsresult rv = NS_OK;
  PRBool   bWarnPref = PR_FALSE;

  PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  WarningNotPrivate(%X)\n", aDOMWindowInternal ) );

  mPrefServ->GetBoolPref( P3P_PREF_WARNINGNOTPRIVATE, &bWarnPref );
  if (bWarnPref) {

    NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
    if (NS_SUCCEEDED(rv)) {

      nsAutoString windowTitle, message, showAgain;

      mP3PService->GetLocaleString( "P3PTitle", windowTitle );
      mP3PService->GetLocaleString( "WarningNotPrivate", message );
      mP3PService->GetLocaleString( "ShowAgain", showAgain );

      PRBool outCheckValue = PR_TRUE;
      dialog->AlertCheck( aDOMWindowInternal,
                          windowTitle.GetUnicode(),
                          message.GetUnicode(),
                          showAgain.GetUnicode(),
                          &outCheckValue);

      if (!outCheckValue) {
        mPrefServ->SetBoolPref( P3P_PREF_WARNINGNOTPRIVATE, PR_FALSE );
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::WarningPartialPrivacy(nsIDocShellTreeItem * aDocShellTreeItem)
{
  nsresult rv;
  nsCOMPtr<nsIDOMWindowInternal> pDOMWindowInternal;

  PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  WarningPartialPrivacy(%X)\n", aDocShellTreeItem) );

  rv = DocShellTreeItemToDOMWindowInternal(aDocShellTreeItem, getter_AddRefs(pDOMWindowInternal));
  if (NS_SUCCEEDED(rv)) {
    rv = WarningPartialPrivacy(pDOMWindowInternal);
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::WarningPartialPrivacy(nsIDOMWindowInternal * aDOMWindowInternal)
{
  nsresult rv = NS_OK;
  PRBool   bWarnPref = PR_FALSE;

  PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  WarningPartialPrivacy(%X)\n", aDOMWindowInternal ) );

  mPrefServ->GetBoolPref( P3P_PREF_WARNINGPARTIALPRIVACY, &bWarnPref );
  if (bWarnPref) {

    NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
    if (NS_SUCCEEDED(rv)) {

      nsAutoString windowTitle, message, showAgain;

      mP3PService->GetLocaleString( "P3PTitle", windowTitle );
      mP3PService->GetLocaleString( "WarningPartialPrivacy", message );
      mP3PService->GetLocaleString( "ShowAgain", showAgain );

      PRBool outCheckValue = PR_TRUE;
      dialog->AlertCheck( aDOMWindowInternal,
                          windowTitle.GetUnicode(),
                          message.GetUnicode(),
                          showAgain.GetUnicode(),
                          &outCheckValue);

      if (!outCheckValue) {
        mPrefServ->SetBoolPref( P3P_PREF_WARNINGPARTIALPRIVACY, PR_FALSE );
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::WarningPostToNotPrivate(nsIDOMWindowInternal * aDOMWindowInternal, PRBool * aResult)
{
  nsresult rv = NS_OK;
  PRBool   bWarnPref = PR_FALSE;

  PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  WarningPostToNotPrivate(%X, %X)\n", aDOMWindowInternal, aResult ) );

  *aResult = PR_TRUE;

  mPrefServ->GetBoolPref( P3P_PREF_WARNINGPOSTTONOTPRIVATE, &bWarnPref );
  if (bWarnPref) {

    NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
    if (NS_SUCCEEDED(rv)) {

      nsAutoString windowTitle, message, showAgain;

      mP3PService->GetLocaleString( "P3PTitle", windowTitle );
      mP3PService->GetLocaleString( "WarningPostToNotPrivate", message );
      mP3PService->GetLocaleString( "ShowAgain", showAgain );

      PRBool outCheckValue = PR_TRUE;
      dialog->ConfirmCheck( aDOMWindowInternal,
                            windowTitle.GetUnicode(),
                            message.GetUnicode(),
                            showAgain.GetUnicode(),
                            &outCheckValue,
                            aResult);

      if (!outCheckValue) {
        mPrefServ->SetBoolPref( P3P_PREF_WARNINGPOSTTONOTPRIVATE, PR_FALSE );
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::WarningPostToBrokenPolicy(nsIDOMWindowInternal * aDOMWindowInternal, PRBool * aResult)
{
  nsresult rv = NS_OK;
  PRBool   bWarn2Pref = PR_FALSE;

  PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  WarningPostToBrokenPolicy(%X, %X)\n", aDOMWindowInternal, aResult ) );

  *aResult = PR_TRUE;

  mPrefServ->GetBoolPref( P3P_PREF_WARNINGPOSTTOBROKENPOLICY, &bWarn2Pref );
  if (bWarn2Pref) {

    NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
    if (NS_SUCCEEDED(rv)) {

      nsAutoString windowTitle, message, showAgain;

      mP3PService->GetLocaleString( "P3PTitle", windowTitle );
      mP3PService->GetLocaleString( "WarningPostToBrokenPolicy", message );
      mP3PService->GetLocaleString( "ShowAgain", showAgain );

      PRBool outCheckValue = PR_TRUE;
      dialog->ConfirmCheck( aDOMWindowInternal,
                            windowTitle.GetUnicode(),
                            message.GetUnicode(),
                            showAgain.GetUnicode(),
                            &outCheckValue,
                            aResult);

      if (!outCheckValue) {
        mPrefServ->SetBoolPref( P3P_PREF_WARNINGPOSTTOBROKENPOLICY, PR_FALSE );
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsP3PUIService::WarningPostToNoPolicy(nsIDOMWindowInternal * aDOMWindowInternal, PRBool * aResult)
{
  nsresult rv = NS_OK;
  PRBool   bWarn3Pref = PR_FALSE;

  PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  WarningPostToNoPolicy(%X, %X)\n", aDOMWindowInternal, aResult ) );

  *aResult = PR_TRUE;

  mPrefServ->GetBoolPref( P3P_PREF_WARNINGPOSTTONOPOLICY, &bWarn3Pref );
  if (bWarn3Pref) {

    NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
    if (NS_SUCCEEDED(rv)) {

      nsAutoString windowTitle, message, showAgain;

      mP3PService->GetLocaleString( "P3PTitle", windowTitle );
      mP3PService->GetLocaleString( "WarningPostToNoPolicy", message );
      mP3PService->GetLocaleString( "ShowAgain", showAgain );

      PRBool outCheckValue = PR_TRUE;
      dialog->ConfirmCheck( aDOMWindowInternal,
                            windowTitle.GetUnicode(),
                            message.GetUnicode(),
                            showAgain.GetUnicode(),
                            &outCheckValue,
                            aResult);

      if (!outCheckValue) {
        mPrefServ->SetBoolPref( P3P_PREF_WARNINGPOSTTONOPOLICY, PR_FALSE );
      }
    }
  }

  return rv;
}

//
// nsP3PUIService routines
//

NS_METHOD
nsP3PUIService::DocShellTreeItemToDOMWindowInternal( nsIDocShellTreeItem * aDocShellTreeItem, nsIDOMWindowInternal ** aResult)
{
  nsresult rv;

  *aResult = nsnull;

  nsCOMPtr<nsIScriptGlobalObjectOwner> pScriptGlobalObjectOwner = do_QueryInterface(aDocShellTreeItem, &rv);
  if (NS_SUCCEEDED(rv)) {

    nsCOMPtr<nsIScriptGlobalObject> pScriptGlobalObject;
    rv = pScriptGlobalObjectOwner->GetScriptGlobalObject(getter_AddRefs(pScriptGlobalObject));
    if (NS_SUCCEEDED(rv)) {
      if (pScriptGlobalObject != nsnull) {

        nsCOMPtr<nsIDOMWindowInternal> pDOMWindowInternal = do_QueryInterface(pScriptGlobalObject, &rv);
        if (NS_SUCCEEDED(rv)) {

          *aResult = pDOMWindowInternal;
          NS_IF_ADDREF(*aResult);
        }
      } else {
        rv = NS_ERROR_NULL_POINTER;
      }
    }
  }

  if (NS_FAILED(rv)) {
    PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PUIService:  Failed to find DOMWindowInternal for DocShellTreeItem %X.  Return value is %X.\n", aDocShellTreeItem, rv) );
  }

  return rv;
}
