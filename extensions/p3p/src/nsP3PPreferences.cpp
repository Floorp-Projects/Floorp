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
#include "nsP3PPreferences.h"
#include "nsP3PLogging.h"

#include <nsNetUtil.h>
#include <nsString.h>


// ****************************************************************************
// nsP3PPreferences Implementation routines
// ****************************************************************************

// P3P Preferences creation routine
NS_METHOD
NS_NewP3PPreferences( nsIP3PPreferences **aP3PPreferences ) {
  nsresult     rv;

  nsP3PPreferences *pNewP3PPreferences;


  NS_ENSURE_ARG_POINTER( aP3PPreferences );

  pNewP3PPreferences = new nsP3PPreferences( );

  if (pNewP3PPreferences) {
    NS_ADDREF( pNewP3PPreferences );

    rv = pNewP3PPreferences->Init();

    if (NS_SUCCEEDED( rv )) {
      rv = pNewP3PPreferences->QueryInterface( NS_GET_IID( nsIP3PPreferences ),
                                               (void **)aP3PPreferences );
    }
  }
  else {
    NS_ASSERTION( 0, "P3P:  P3PPreferences unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Service nsISupports
NS_IMPL_ISUPPORTS2( nsP3PPreferences, nsIP3PPreferences,
                                      nsISupports );

// P3P Preferences constructor
nsP3PPreferences::nsP3PPreferences( )
  : mInitialized( PR_FALSE ),
    mP3Penabled( PR_FALSE ) {
  mPrefCache = nsnull;
  NS_INIT_ISUPPORTS( );
}

// P3P Preferences destructor
nsP3PPreferences::~nsP3PPreferences( ) {
  if (mPrefServ) {
    mPrefServ->UnregisterCallback( P3P_PREF,
                                   (PrefChangedFunc)PrefChangedCallback,
                                   (void *)this );
  }
}

// ****************************************************************************
// nsP3PPreferences routines
// ****************************************************************************

// P3P Preferences:  preference change callback routine
//
// Function:  This is a callback routine for the Prefs service.  This routine
//            is called whenever one of the preferences for which the P3P service
//            is registered is changed.
//
// Parms:     1. In:              The preference name that has changed
//            2. In:              The instance data (P3P Preferences)
//
// Return:    NS_OK               OK
PRInt32
PrefChangedCallback( const char  *aPrefName,
                     void        *aInstanceData ) {
#ifdef DEBUG_P3P
  printf("P3P:  PrefChangedCallback(\"%s\", %x)\n", aPrefName, aInstanceData);
#endif
  nsP3PPreferences *pP3PPreferences = (nsP3PPreferences *)aInstanceData;


  NS_ASSERTION( pP3PPreferences != nsnull, "P3P:  Preference Callback bad instance data.\n" );
  if (pP3PPreferences) {
    pP3PPreferences->PrefChanged( aPrefName );
  }

  return 0;
}

// nsIP3PPreferences methods
NS_IMETHODIMP
nsP3PPreferences::Init( ) {
  nsresult  rv;

#ifdef DEBUG_P3P
  printf("P3P:  P3PPreferences initializing.\n");
#endif
  // Only initialize once...
  if (!mInitialized) {
    mInitialized = PR_TRUE;

    // nsP3PService creates gP3PLogModule.  nsP3PService creates this
    // nsP3PPreferences after it creates gP3PLogModule, so we know
    // gP3PLogModule is valid.

    PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PPreferences:  Initializing.") );

    // Initialize the P3P preferences
    mPrefServ = do_GetService( NS_PREF_CONTRACTID,
                               &rv );
    if (NS_SUCCEEDED( rv )) {

#ifdef DEBUG_P3P
      printf("P3P:  P3PPreferences:  Preference Service is at %x.\n", mPrefServ);
#endif
      rv = mPrefServ->RegisterCallback( P3P_PREF,
                                        (PrefChangedFunc)PrefChangedCallback,
                                        (void *)this );

      if (NS_SUCCEEDED( rv )) {

        // Create the hashtable that caches our preferences.
        mPrefCache = new nsSupportsHashtable( 256, PR_TRUE );
        NS_ASSERTION( mPrefCache, "P3P:  P3PPreferences unable to create preference cache hashtable\n" );

        if (mPrefCache) {

          // Initialize the preferences.
          rv = PrefChanged( );

        } else {
          rv = NS_ERROR_OUT_OF_MEMORY;
        }

      } else {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  } else {
    NS_ASSERTION( 0, "Initializing P3P Preferences more than once.\n" );
    rv = NS_ERROR_ALREADY_INITIALIZED;
  }

  return rv;
}


// P3P Preferences:  preference change routine
//
// Function:  Process the preference that has been changed (or all preferences
//            if no parameter has been supplied).
//
// Parms:     1. In:              The preference name that has changed or null
//
// Return:    NS_OK               OK
NS_IMETHODIMP
nsP3PPreferences::PrefChanged( const char *aPrefName ) {
  nsresult  rv;

  PRBool    bAllPrefs = (aPrefName) ? PR_FALSE : PR_TRUE;

  PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PPreferenceService:  PrefChanged(\"%s\")\n", aPrefName) );

  if (bAllPrefs) {
#ifdef DEBUG_P3P
    printf( "P3P:  PrefChanged():  Retrieving all preferences\n" );
#endif
    rv = mPrefServ->GetBoolPref( P3P_PREF_ENABLED, &mP3Penabled );
    if (NS_FAILED( rv )) {
      mP3Penabled = PR_FALSE;
    }
  } else {
#ifdef DEBUG_P3P
    printf( "P3P:  PrefChanged():  %s preference changed\n", aPrefName );
#endif
    if (!mP3PService) {
      mP3PService = do_GetService( NS_P3PSERVICE_CONTRACTID, &rv );
    }

    if (PL_strcmp(aPrefName, P3P_PREF_ENABLED) == 0) {
      rv = mPrefServ->GetBoolPref( P3P_PREF_ENABLED, &mP3Penabled );
      if (NS_FAILED( rv )) {
        mP3Penabled = PR_FALSE;
      }

      if (mP3Penabled) {
        mP3PService->Enabled();
      } else {
        mP3PService->Disabled();
      }
    }
  }

  mTimePrefsLastChanged = PR_Now();
#ifdef DEBUG_P3P
  printf( "P3P:  PrefChanged():  Current time is %lx.\n", mTimePrefsLastChanged );
#endif

  return NS_OK;
}

// P3P Preferences:  get the last time the preferences changed
//
// Function:  Return the time the preference were last changed
//
// Parms:     1. Out:             Time the preference were last changed
//
// Return:    NS_OK               OK
NS_IMETHODIMP
nsP3PPreferences::GetTimePrefsLastChanged( PRTime *aResult ) {

  *aResult = mTimePrefsLastChanged;

  return NS_OK;
}

// P3P Preferences:  check if preferences changed since a given time
//
// Function:  Return boolean that says if prefs have changed since the
//            specified time
//
// Parms:     1. In:              Time the preference were last changed
//
// Return:    PRBool              Prefs changed or not
NS_IMETHODIMP_( PRBool )
nsP3PPreferences::PrefsChanged( PRTime time ) {

  return (time < mTimePrefsLastChanged);
}

// P3P Preferences:  boolean preference retreival routine
//
// Function:  Call the preference service to get the requested boolean preference
//
// Parms:     1. In:              The preference name
//            2. Out:             Current value of the boolean preference
//
// Return:    NS_OK               OK
NS_IMETHODIMP
nsP3PPreferences::GetBoolPref( const char  *aPrefName, PRBool *aResult ) {

  nsresult  rv;

#ifdef DEBUG_P3P
  printf("P3P:  P3PPreferences:  GetBoolPref(%s)  ", aPrefName);
#endif

  if (PL_strcmp(aPrefName, P3P_PREF_ENABLED) == 0) {
    *aResult = mP3Penabled;
    rv = NS_OK;

  } else {
    rv = mPrefServ->GetBoolPref( aPrefName, aResult);
  }

  if (NS_FAILED(rv)) {
    PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PPreferenceService:  GetBoolPref(%s) failed with return value %x.\n", aPrefName, rv) );
#ifdef DEBUG_P3P
    printf("failed with return value %x\n", rv);
#endif
  }
#ifdef DEBUG_P3P
  else {
    printf("%s is %s.\n", aPrefName, (*aResult) ? "true" : "false");
  }
#endif

  return rv;
}


// P3P Preferences:  boolean preference retreival routine
//
// Function:  Call the preference service to get the requested boolean preference
//
// Parms:     1. In:              category name
//            2. In:              purpose name
//            3. In:              recipient name
//            4. Out:             permitted or not
//
// Return:    NS_OK               OK
NS_IMETHODIMP
nsP3PPreferences::GetBoolPref( nsString aCategory,
                               nsString aPurpose,
                               nsString aRecipient,
                               PRBool * aResult ) {

  nsCString nscsCategory;   nscsCategory.AssignWithConversion(aCategory);
  nsCString nscsPurpose;    nscsPurpose.AssignWithConversion(aPurpose);
  nsCString nscsRecipient;  nscsRecipient.AssignWithConversion(aRecipient);

  return GetBoolPref( nscsCategory, nscsPurpose, nscsRecipient, aResult );
}

// P3P Preferences:  boolean preference retreival routine
//
// Function:  Call the preference service to get the requested boolean preference
//
// Parms:     1. In:              category name
//            2. In:              purpose name
//            3. In:              recipient name
//            4. Out:             permitted or not
//
// Return:    NS_OK               OK
NS_IMETHODIMP
nsP3PPreferences::GetBoolPref( nsCString aCategory,
                               nsCString aPurpose,
                               nsCString aRecipient,
                               PRBool *  aResult ) {

  nsresult  rv;
  nsCString pPrefName;

  PRBool    bPurpose;
  PRBool    bRecipient;

#ifdef DEBUG_P3P
  printf("P3P:  P3PPreferences:  GetBoolPref(%s, %s, %s)\n", (const char *) aCategory, (const char *) aPurpose, (const char *) aRecipient);
#endif

  pPrefName = P3P_PREF;
  pPrefName += aCategory;
  pPrefName += ".";
  pPrefName += aPurpose;

  rv = mPrefServ->GetBoolPref( pPrefName, &bPurpose);

  if (NS_SUCCEEDED( rv )) {
#ifdef DEBUG_P3P
    printf("P3P:  P3PPreferences:  Setting for %s is %s.\n",
           (const char *) pPrefName, (*aResult) ? "true" : "false");
#endif
    if (aRecipient.Equals("ours")) {
      bRecipient = PR_TRUE;
    } else {
      pPrefName = P3P_PREF;
      pPrefName += aCategory;
      pPrefName += ".sharing";

      rv = mPrefServ->GetBoolPref( pPrefName, &bRecipient);
#ifdef DEBUG_P3P
      if (NS_SUCCEEDED( rv )) {
        printf("P3P:  P3PPreferences:  Setting for %s is %s.\n",
               (const char *) pPrefName, (*aResult) ? "true" : "false");
      }
#endif
    }

    if (NS_SUCCEEDED( rv )) {
      *aResult = bPurpose && bRecipient;
    }
  }

  if (NS_FAILED(rv)) {
    PR_LOG( gP3PLogModule, PR_LOG_NOTICE, ("P3PPreferenceService:  GetBoolPref(%s, %s, %s) failed with return value %X.\n", (const char *) aCategory, (const char *) aPurpose, (const char *) aRecipient, rv) );
#ifdef DEBUG_P3P
    printf("P3P:  P3PPreferences:  GetBoolPref() failed with return value %x.\n", rv);
#endif
  }
#ifdef DEBUG_P3P
  else {
    printf("P3P:  P3PPreferences:  Setting for %s, %s, %s is %s.\n",
           (const char *) aCategory, (const char *) aPurpose, (const char *) aRecipient, (*aResult) ? "true" : "false");
  }
#endif

  return rv;
}

