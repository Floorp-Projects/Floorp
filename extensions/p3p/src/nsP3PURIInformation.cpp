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

#include "nsP3PURIInformation.h"
#include "nsP3PLogging.h"

#include <nsNetUtil.h>

#include <nsXPIDLString.h>


// ****************************************************************************
// nsP3PURIInformation Implementation routines
// ****************************************************************************

// P3P URI Information nsISupports
NS_IMPL_ISUPPORTS1( nsP3PURIInformation, nsIP3PURIInformation );

// P3P URI Information creation routine
NS_METHOD
NS_NewP3PURIInformation( nsString&              aURISpec,
                         nsIP3PURIInformation **aURIInformation ) {

  nsresult             rv;

  nsP3PURIInformation *pNewURIInformation = nsnull;


  NS_ENSURE_ARG_POINTER( aURIInformation );

  *aURIInformation = nsnull;

  pNewURIInformation = new nsP3PURIInformation( );

  if (pNewURIInformation) {
    NS_ADDREF( pNewURIInformation );

    rv = pNewURIInformation->Init( aURISpec );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewURIInformation->QueryInterface( NS_GET_IID( nsIP3PURIInformation ),
                                      (void **)aURIInformation );
    }

    NS_RELEASE( pNewURIInformation );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Unable to create URIInformation object\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P URI Information: Constructor
nsP3PURIInformation::nsP3PURIInformation( ) {

  NS_INIT_ISUPPORTS( );
}

// P3P URI Information: Destructor
nsP3PURIInformation::~nsP3PURIInformation( ) {
}

// P3P URI Information: Init
//
// Function:  Initialization routine for the URI Information object.
//
// Parms:     1. In     The URI spec this object represents
//
NS_METHOD
nsP3PURIInformation::Init( nsString&  aURISpec ) {

  nsresult  rv;


  mURISpec  = aURISpec;
  mcsURISpec.AssignWithConversion( mURISpec );

#ifdef DEBUG_P3P
  { printf( "P3P:  URIInformation initializing for %s\n", (const char *)mcsURISpec );
  }
#endif

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PURIInformation:  %s Init, initializing.\n", (const char *)mcsURISpec) );

  rv = NS_NewURI( getter_AddRefs( mURI ),
                  mURISpec );

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PURIInformation:  %s Init, NS_NewURI failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}


// ****************************************************************************
// nsIP3PURIInformation routines
// ****************************************************************************

// P3P URI Information: GetRefererHeader
//
// Function:  Gets the "Referer" request header value associated with this URI.
//
// Parms:     1. Out    The "Referer" request header value
//
NS_IMETHODIMP_( void )
nsP3PURIInformation::GetRefererHeader( nsString&  aRefererHeader ) {

  aRefererHeader = mRefererHeader;

  return;
}

// P3P URI Information: SetRefererHeader
//
// Function:  Sets the "Referer" request header value associated with this URI.
//
// Parms:     1. In     The "Referer" request header value
//
NS_IMETHODIMP
nsP3PURIInformation::SetRefererHeader( nsString&  aRefererHeader ) {

  nsCAutoString  csRefererHeader;


  csRefererHeader.AssignWithConversion( aRefererHeader );
    
#ifdef DEBUG_P3P
  { printf( "P3P:    Setting Referer Header: %s\n", (const char *)csRefererHeader );
  }
#endif

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PURIInformation:  %s SetRefererHeader, Setting \"Referer\" header - %s.\n", (const char *)mcsURISpec, (const char *)csRefererHeader) );

  mRefererHeader = aRefererHeader;

  return NS_OK;
}

// P3P URI Information: GetPolicyRefFileURISpecs
//
// Function:  Returns the PolicyRefFile URI specs associated with this URI.
//
// Parms:     1. Out    The PolicyRefFile URI specs
//
NS_IMETHODIMP_( void )
nsP3PURIInformation::GetPolicyRefFileURISpecs( nsStringArray  *aPolicyRefFileURISpecs ) {

  *aPolicyRefFileURISpecs = mPolicyRefFileURISpecs;

  return;
}

// P3P URI Information: GetPolicyRefFileReferencePoint
//
// Function:  Gets the point at which this PolicyRefFile URI spec was specified
//            (ie. HTTP header or HTML <LINK> tag).
//
// Parms:     1. In     The PolicyRefFile URI spec
//            2. Out    The place where the reference was specified
//
NS_IMETHODIMP_( void )
nsP3PURIInformation::GetPolicyRefFileReferencePoint( nsString&  aPolicyRefFileURISpec,
                                                     PRInt32&   aReferencePoint ) {

  nsStringKey  keyPolicyRefFileURISpec( aPolicyRefFileURISpec );


  // Default to the HTTP header reference point
  aReferencePoint = P3P_REFERENCE_HTTP_HEADER;

  if (mPolicyRefFileReferencePoints.Exists(&keyPolicyRefFileURISpec )) {
    // Obtain the reference point for the PolicyRefFile
    aReferencePoint = (PRInt32)mPolicyRefFileReferencePoints.Get(&keyPolicyRefFileURISpec );
  }

  return;
}

// P3P URI Information: SetPolicyRefFileURISpec
//
// Function:  Sets a PolicyRefFile URI spec associated with this URI.
//
// Parms:     2. In     The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            3. In     The PolicyRefFile URI spec
//
NS_IMETHODIMP
nsP3PURIInformation::SetPolicyRefFileURISpec( PRInt32    aReferencePoint,
                                              nsString&  aPolicyRefFileURISpec ) {

  nsresult          rv;

  nsCAutoString     csPolicyRefFileURISpec;

  nsCOMPtr<nsIURI>  pURI;

  nsXPIDLCString    xcsPolicyRefFileURISpec;

  nsAutoString      sPolicyRefFileURISpec;


  csPolicyRefFileURISpec.AssignWithConversion( aPolicyRefFileURISpec );
  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PURIInformation:  %s SetPolicyRefFileURISpec, adding PolicyRefFile URI spec - %s.\n", (const char *)mcsURISpec, (const char *)csPolicyRefFileURISpec) );

  rv = NS_NewURI( getter_AddRefs( pURI ),
                  aPolicyRefFileURISpec,
                  mURI );

  if (NS_SUCCEEDED( rv )) {
    rv = pURI->GetSpec( getter_Copies( xcsPolicyRefFileURISpec ) );

    if (NS_SUCCEEDED( rv )) {
      sPolicyRefFileURISpec.AssignWithConversion((const char *)xcsPolicyRefFileURISpec );

      if (mPolicyRefFileURISpecs.IndexOf( sPolicyRefFileURISpec ) < 0) {
#ifdef DEBUG_P3P
        { printf( "P3P:    Adding PolicyRefFile URI: %s\n", (const char *)xcsPolicyRefFileURISpec );
        }
#endif

        rv = mPolicyRefFileURISpecs.AppendString( sPolicyRefFileURISpec ) ? NS_OK : NS_ERROR_FAILURE;

        if (NS_SUCCEEDED( rv )) {
          nsStringKey  keyPolicyRefFileURISpec( sPolicyRefFileURISpec );

          mPolicyRefFileReferencePoints.Put(&keyPolicyRefFileURISpec,
                                     (void *)aReferencePoint );
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PURIInformation:  %s SetPolicyRefFileURISpec, mPolicyRefFileURISpecs.AppendString failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PURIInformation:  %s SetPolicyRefFileURISpec, PolicyRefFile URI spec already added - %s.\n", (const char *)mcsURISpec, (const char *)csPolicyRefFileURISpec) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PURIInformation:  %s SetPolicyRefFileURISpec, pURI->GetSpec failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PURIInformation:  %s SetPolicyRefFileURISpec, NS_NewURI failed - %X.\n", (const char *)mcsURISpec, rv) );
  }


  return NS_OK;
}

// P3P URI Information: GetCookieHeader
//
// Function:  Gets the "Set-Cookie" response header value associated with this URI.
//
// Parms:     1. Out    The "Set-Cookie" response header value
//
NS_IMETHODIMP_( void )
nsP3PURIInformation::GetCookieHeader( nsString&  aSetCookieHeader ) {

  aSetCookieHeader = mSetCookieHeader;

  return;
}

// P3P URI Information: SetCookieHeader
//
// Function:  Sets the "Set-Cookie" response header value associated with this URI.
//
// Parms:     1. In     The "Set-Cookie" response header value
//
NS_IMETHODIMP
nsP3PURIInformation::SetCookieHeader( nsString&  aSetCookieHeader ) {

  nsCAutoString  csSetCookieHeader;


  csSetCookieHeader.AssignWithConversion( aSetCookieHeader );
#ifdef DEBUG_P3P
  { printf( "P3P:    Setting Cookie Header: %s\n", (const char *)csSetCookieHeader );
  }
#endif

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PURIInformation:  %s SetCookieHeader, setting \"SetCookie\" header - %s.\n", (const char *)mcsURISpec, (const char *)csSetCookieHeader) );

  mSetCookieHeader = aSetCookieHeader;

  return NS_OK;
}
