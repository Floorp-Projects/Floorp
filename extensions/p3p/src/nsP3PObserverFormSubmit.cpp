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
#include "nsP3PLogging.h"

#include "nsP3PObserverFormSubmit.h"

#include "nsIServiceManager.h"
#include "nsObserverService.h"
#include "nsIPresShell.h"

#include "nsString.h"


// ****************************************************************************
// nsP3PObserverFormSubmit Implementation routines
// ****************************************************************************

static NS_DEFINE_CID( kP3PServiceCID, NS_P3PSERVICE_CID );

// P3P Observer Form Submit: nsISupports
NS_IMPL_ISUPPORTS2( nsP3PObserverFormSubmit, nsIObserver,
                                             nsIFormSubmitObserver );

// P3P Observer Form Submit: Creation routine
NS_METHOD
NS_NewP3PObserverFormSubmit( nsIObserver **aObserverFormSubmit ) {

  nsresult                 rv;

  nsP3PObserverFormSubmit *pNewObserverFormSubmit = nsnull;


#ifdef DEBUG_P3P
  printf("P3P:  ObserverFormSubmit initializing.\n");
#endif

  NS_ENSURE_ARG_POINTER( aObserverFormSubmit );

  *aObserverFormSubmit = nsnull;

  pNewObserverFormSubmit = new nsP3PObserverFormSubmit( );

  if (pNewObserverFormSubmit) {
    NS_ADDREF( pNewObserverFormSubmit );

    rv = pNewObserverFormSubmit->Init( );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewObserverFormSubmit->QueryInterface( NS_GET_IID( nsIObserver ),
                                          (void **)aObserverFormSubmit );
    }

    NS_RELEASE( pNewObserverFormSubmit );
  }
  else {
    NS_ASSERTION( 0, "P3P:  ObserverFormSubmit unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Observer Form Submit: Constructor
nsP3PObserverFormSubmit::nsP3PObserverFormSubmit( )
  : mP3PIsEnabled( PR_FALSE ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Observer Form Submit: Destructor
nsP3PObserverFormSubmit::~nsP3PObserverFormSubmit( ) {

  if (mObserverService) {
    mObserverService->RemoveObserver( this, NS_FORMSUBMIT_SUBJECT );
  }
}

// P3P Observer Form Submit: Init
//
// Function:  Initialization routine for the Form Submission observer.
//
// Parms:     None
//
NS_METHOD
nsP3PObserverFormSubmit::Init( ) {

  nsresult  rv;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PObserverFormSubmit:  Init, initializing.\n") );

  // Get the Observer service
  mObserverService = do_GetService( NS_OBSERVERSERVICE_CONTRACTID,
                                   &rv );

  if (NS_SUCCEEDED( rv )) {
    // Register to observe form submissions
    rv = mObserverService->AddObserver( this, NS_FORMSUBMIT_SUBJECT, PR_FALSE);

    if (NS_FAILED( rv )) {
#ifdef DEBUG_P3P
      printf( "P3P:  Unable to register with Observer Service for form submission topic: %X\n", rv );
#endif

      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PObserverFormSubmit:  Init, mObserverService->AddObserver failed - %X.\n", rv) );
    }
  }
  else {
#ifdef DEBUG_P3P
    printf( "P3P:  Unable to obtain Observer Service: %X\n", rv );
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PObserverFormSubmit:  Init, do_GetService for Observer service failed - %X.\n", rv) );
  }

  return rv;
}


// ****************************************************************************
// nsIObserver routines
// ****************************************************************************

// P3P Observer Form Submit: Observe
//
// Function:  Not used, only here to satisfy the interface.
//
NS_IMETHODIMP
nsP3PObserverFormSubmit::Observe( nsISupports      *aSubject,
                                  const PRUnichar  *aTopic,
                                  const PRUnichar  *aSomeData ) {

  return NS_OK;
}

// ****************************************************************************
// nsIFormSubmitObserver routines
// ****************************************************************************

// P3P Observer Form Submit: Notify
//
// Function:  Allows for determination of whether to allow the form submission.
//
// Parms:     1. In     The <FORM> Content object
//            2. In     The DOMWindowInternal associated with the form submission
//            3. In     The URI object of the form submission target
//
NS_IMETHODIMP
nsP3PObserverFormSubmit::Notify( nsIContent            *aContent,
                                 nsIDOMWindowInternal  *aDOMWindowInternal,
                                 nsIURI                *aURI,
                                 PRBool                *aCancelSubmit ) {

  nsresult  rv;


  // Initially allow the submit to occur
  *aCancelSubmit = PR_FALSE;

  // Get the P3P service and check if it is enabled
  rv = GetP3PService( );

  if (NS_SUCCEEDED( rv ) && mP3PIsEnabled) {
#ifdef DEBUG_P3P
    printf( "P3P:  ObserverFormSubmit has been notified\n" );
    aContent->List( );
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PObserverFormSubmit:  Notify, form submission privacy check in progress.\n") );

    // Check the privacy
    rv = mP3PService->CheckPrivacyFormSubmit( aURI,
                                              aDOMWindowInternal,
                                              aContent,
                                              aCancelSubmit );

#ifdef DEBUG_P3P
    printf( "P3P:    Submission %s\n", *aCancelSubmit ? "CANCELLED" : "ALLOWED" );
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PObserverFormSubmit:  Notify, form submission %s.\n", *aCancelSubmit ? "CANCELLED" : "ALLOWED") );
  }

  return NS_OK;
}


// ****************************************************************************
// nsP3PObserverFormSubmit routines
// ****************************************************************************

// P3P Observer Form Submit: GetP3PService
//
// Function:  Obtain the P3P service and determine if it is enabled.
//
// Parms:     None
//
NS_METHOD
nsP3PObserverFormSubmit::GetP3PService( ) {

  nsresult  rv = NS_OK;


  if (!mP3PService) {
    mP3PService = do_GetService( NS_P3PSERVICE_CONTRACTID,
                                &rv );
    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PObserverFormSubmit:  GetP3PService, do_GetService for P3P service failed - %X.\n", rv) );
    }
  }

  if (mP3PService) {
    rv = mP3PService->P3PIsEnabled(&mP3PIsEnabled );

    if (NS_SUCCEEDED( rv ) && !mP3PIsEnabled) {
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PObserverFormSubmit:  GetP3PService, P3P Service is not enabled.\n") );
    }
    else if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PObserverFormSubmit:  GetP3PService, mP3PService->P3PIsEnabled failed - %X.\n", rv) );
    }
  }

  return rv;
}
