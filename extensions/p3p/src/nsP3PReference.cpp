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

#include "nsP3PReference.h"
#include "nsP3PLogging.h"

#include <nsIComponentManager.h>

#include <nsXPIDLString.h>


// ****************************************************************************
// nsP3PReference Implementation routines
// ****************************************************************************

// P3P Reference nsISupports
NS_IMPL_ISUPPORTS1( nsP3PReference, nsIP3PReference );

// P3P Reference creation routine
NS_METHOD
NS_NewP3PReference( nsString&         aHost,
                    nsIP3PReference **aReference ) {

  nsresult        rv;

  nsP3PReference *pNewReference = nsnull;


  NS_ENSURE_ARG_POINTER( aReference );

  *aReference = nsnull;

  pNewReference = new nsP3PReference( );

  if (pNewReference) {
    NS_ADDREF( pNewReference );

    rv = pNewReference->Init( aHost );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewReference->QueryInterface( NS_GET_IID( nsIP3PReference ),
                                 (void **)aReference );
    }

    NS_RELEASE( pNewReference );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Reference unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Reference: Constructor
nsP3PReference::nsP3PReference( )
  : mLock( PR_NewLock( ) ),
    mPolicyRefFiles( 32, PR_TRUE ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Reference: Destructor
nsP3PReference::~nsP3PReference( ) {

  PR_DestroyLock( mLock );
}

// P3P Reference: Init
//
// Function:  Initialization routine for the Reference object.
//
// Parms:     1. In     The host name and port combination
//
NS_METHOD
nsP3PReference::Init( nsString&  aHostPort ) {

  nsresult  rv = NS_OK;


#ifdef DEBUG_P3P
  printf("P3P:  Reference initializing.\n");
#endif

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PReference:  %s Init, initializing.\n", (const char *)mcsHostPort) );

  // Save the host name and port combination
  mHostPort = aHostPort;
  mcsHostPort.AssignWithConversion( aHostPort );

  return rv;
}


// ****************************************************************************
// nsIP3PReference routines
// ****************************************************************************

// P3P Reference: GetPolicyRefFile
//
// Function:  Checks if a PolicyRefFile object is in the list of associated PolicyRefFile objects
//
// Parms:     1. In     The PolicyRefFile URI spec
NS_IMETHODIMP
nsP3PReference::GetPolicyRefFile( nsString&             aPolicyRefFileURISpec,
                                  nsIP3PPolicyRefFile **aPolicyRefFile ) {

  nsresult     rv = NS_OK;

  nsStringKey  keyPolicyRefFileURISpec( aPolicyRefFileURISpec );


  NS_ENSURE_ARG_POINTER( aPolicyRefFile );

  *aPolicyRefFile = nsnull;

  if (mPolicyRefFiles.Exists(&keyPolicyRefFileURISpec )) {
    *aPolicyRefFile = (nsIP3PPolicyRefFile *)mPolicyRefFiles.Get(&keyPolicyRefFileURISpec );
  }

  return rv;
}

// P3P Reference: AddPolicyRefFile
//
// Function:  Adds the PolicyRefFile object to the list of associated PolicyRefFile objects
//
// Parms:     1. In     The PolicyRefFile URI spec
//            2. In     The PolicyRefFile object
NS_IMETHODIMP
nsP3PReference::AddPolicyRefFile( nsString&             aPolicyRefFileURISpec,
                                  nsIP3PPolicyRefFile  *aPolicyRefFile ) {

  nsresult       rv = NS_OK;

  nsStringKey    keyPolicyRefFileURISpec( aPolicyRefFileURISpec );

  nsCAutoString  csPolicyRefFileURISpec;

  nsAutoLock     lock( mLock );


  csPolicyRefFileURISpec.AssignWithConversion( aPolicyRefFileURISpec );

#ifdef DEBUG_P3P
  { printf( "P3P:    Adding to Reference %s: PolicyRefFile %s\n", (const char *)mcsHostPort, (const char *)csPolicyRefFileURISpec );
  }
#endif

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PReference:  %s AddPolicyRefFile, adding/replacing PolicyRefFile %s.\n", (const char *)mcsHostPort, (const char *)csPolicyRefFileURISpec) );

  if (mPolicyRefFileURISpecs.IndexOf( aPolicyRefFileURISpec ) < 0) {
    // Not in the list of associated PolicyRefFile objects
    rv = mPolicyRefFileURISpecs.AppendString( aPolicyRefFileURISpec ) ? NS_OK : NS_ERROR_FAILURE;
  }

  if (NS_SUCCEEDED( rv )) {
    // Add/Replace the PolicyRefFile object
    mPolicyRefFiles.Put(&keyPolicyRefFileURISpec,
                         aPolicyRefFile );
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PReference:  %s AddPolicyRefFile, mPolicyRefFileURISpecs.AppendString failed.\n", (const char *)mcsHostPort) );

  }

  return rv;
}


// P3P Reference: GetPolicy
//
// Function:  Gets the Policy object for a given access method and path.
//              Returns the spec of an expired PolicyRefFile if encountered.
//
// Parms:     1. In     An indicator as to whether this check is for an embedded object
//            2. In     The URI access method used
//            3. In     The URI path
//            4. Out    The reading PolicyRefFile URI spec
//            5. Out    The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            6. Out    The expired PolicyRefFile URI spec
//            7. Out    The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            8. Out    The Policy object
NS_IMETHODIMP
nsP3PReference::GetPolicy( PRBool         aEmbedCheck,
                           nsString&      aMethod,
                           nsString&      aPath,
                           nsString&      aReadingPolicyRefFileURISpec,
                           PRInt32&       aReadingReferencePoint,
                           nsString&      aExpiredPolicyRefFileURISpec,
                           PRInt32&       aExpiredReferencePoint,
                           nsIP3PPolicy **aPolicy ) {

  nsresult                       rv = NS_OK;

  nsAutoString                   sPolicyRefFileURISpec;

  nsCOMPtr<nsIP3PPolicyRefFile>  pPolicyRefFile;

  nsAutoLock                     lock( mLock );


  NS_ENSURE_ARG_POINTER( aPolicy );

  aReadingPolicyRefFileURISpec.Truncate( );
  aExpiredPolicyRefFileURISpec.Truncate( );
  *aPolicy = nsnull;

  // Loop through all of the PolicyRefFile objects looking for a Policy object
  for (PRInt32 iCount = 0;
         NS_SUCCEEDED( rv ) && (aReadingPolicyRefFileURISpec.Length( ) == 0) && (aExpiredPolicyRefFileURISpec.Length( ) == 0) && !*aPolicy && (iCount < mPolicyRefFileURISpecs.Count( ));
           iCount++) {
    // Get the PolicyRefFile URI spec
    mPolicyRefFileURISpecs.StringAt( iCount,
                                     sPolicyRefFileURISpec );

    if (sPolicyRefFileURISpec.Length( ) > 0) {
      nsStringKey  keyPolicyRefFileURISpec( sPolicyRefFileURISpec );

      if (mPolicyRefFiles.Exists(&keyPolicyRefFileURISpec )) {
        nsCOMPtr<nsISupports>  pEntry;

        // Get the PolicyRefFile
        pEntry         = getter_AddRefs( mPolicyRefFiles.Get(&keyPolicyRefFileURISpec ) );
        pPolicyRefFile = do_QueryInterface( pEntry );

        if (pPolicyRefFile) {
          // Obtain the Policy object
          rv = pPolicyRefFile->GetPolicy( aEmbedCheck,
                                          aMethod,
                                          aPath,
                                          aReadingPolicyRefFileURISpec,
                                          aReadingReferencePoint,
                                          aExpiredPolicyRefFileURISpec,
                                          aExpiredReferencePoint,
                                          aPolicy );
        }
      }
    }
  }

  return rv;
}
