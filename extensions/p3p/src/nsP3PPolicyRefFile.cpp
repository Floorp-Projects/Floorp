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

#include "nsP3PPolicyRefFile.h"
#include "nsP3PPolicy.h"
#include "nsP3PTags.h"
#include "nsP3PSimpleEnumerator.h"
#include "nsP3PLogging.h"

#include <nsNetUtil.h>

#include <nsXPIDLString.h>


// ****************************************************************************
// nsP3PPolicyRefFile Implementation routines
// ****************************************************************************

// P3P Policy Ref File nsISupports
NS_IMPL_ISUPPORTS2( nsP3PPolicyRefFile, nsIP3PPolicyRefFile,
                                        nsIP3PXMLListener );

// P3P Policy Ref File creation routine
NS_METHOD
NS_NewP3PPolicyRefFile( nsString&             aPolicyRefFileURISpec,
                        PRInt32               aReferencePoint,
                        nsIP3PPolicyRefFile **aPolicyRefFile ) {

  nsresult            rv;

  nsP3PPolicyRefFile *pNewPolicyRefFile = nsnull;


  NS_ENSURE_ARG_POINTER( aPolicyRefFile );

  *aPolicyRefFile = nsnull;

  pNewPolicyRefFile = new nsP3PPolicyRefFile( aReferencePoint );

  if (pNewPolicyRefFile) {
    NS_ADDREF( pNewPolicyRefFile );

    rv = pNewPolicyRefFile->Init( aPolicyRefFileURISpec );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewPolicyRefFile->QueryInterface( NS_GET_IID( nsIP3PPolicyRefFile ),
                                     (void **)aPolicyRefFile );
    }

    NS_RELEASE( pNewPolicyRefFile );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Unable to create PolicyRefFile object\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Policy Ref File: Constructor
nsP3PPolicyRefFile::nsP3PPolicyRefFile( PRInt32   aReferencePoint )
  : mReferencePoint( aReferencePoint ),
    mInlinePolicies( 16, PR_TRUE ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Policy Ref File: Destructor
nsP3PPolicyRefFile::~nsP3PPolicyRefFile( ) {
}

// P3P Policy Ref File: mURILegalEscapeTable
//
// This table defines the URI legal characters.  Characters are to be escaped
// if they cannot be represented by these URI legal characters.
//
//   According to URI specification these are:
//     ABCDEFGHIJKLMNOPQRSTUVWXYZ
//     abcdefghijklmnopqrstuvwxyz
//     0123456789
//     -_.!~*'()
//
char
nsP3PPolicyRefFile::mURILegalEscapeTable[] = {
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0, '!',   0,   0,   0,   0,   0,'\'', '(', ')', '*',   0,   0, '-', '.',   0,
   '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',   0,   0,   0,   0,   0,   0,
     0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
   'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',   0,   0,   0,   0, '_',
     0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
   'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',   0,   0,   0, '~',   0 };


// ****************************************************************************
// nsP3PXMLProcessor method overrides
// ****************************************************************************

// P3P Policy Ref File: PostInit
//
// Function:  Perform any additional intialization to the XMLProcessor's.
//
// Parms:     1. In     The PolicyRefFile URI spec
//
NS_IMETHODIMP
nsP3PPolicyRefFile::PostInit( nsString&  aURISpec ) {

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PPolicyRefFile:  %s PostInit, initializing.\n", (const char *)mcsURISpec) );

  mDocumentType = P3P_DOCUMENT_POLICYREFFILE;

  return NS_OK;
}

// P3P Policy Ref File: ProcessContent
//
// Function:  Process the PolicyRefFile
//
// Parms:     1. In     The type of read performed
//
NS_IMETHODIMP
nsP3PPolicyRefFile::ProcessContent( PRInt32   aReadType ) {

  ProcessPolicyRefFile( aReadType );

  return NS_OK;
}

// P3P Policy Ref File: CheckForComplete
//
// Function:  Checks to see if all processing is complete for this PolicyRefFile (this
//            includes processing inline Policies).  If processing is complete, then
//            mProcessComplete must be set to TRUE.
//
// Parms:     None
//
NS_IMETHODIMP_( void )
nsP3PPolicyRefFile::CheckForComplete( ) {

  if (mContentValid && mDocumentValid) {

    if (mInlinePoliciesToRead.Count( ) == 0) {
      // All (if any) inline Policies have been processed
      if (mInlinePolicies.Count( ) > 0) {
        // There were some inline Policies so log that information
#ifdef DEBUG_P3P
        { printf( "P3P:  All inline Policies processed for %s\n", (const char *)mcsURISpec );
        }
#endif

        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PPolicyRefFile:  %s CheckForComplete, all inline Policies processed.\n", (const char *)mcsURISpec) );
      }

#ifdef DEBUG_P3P
      { printf( "P3P:  PolicyRefFile complete: %s\n", (const char *)mcsURISpec );
      }
#endif

      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PPolicyRefFile:  %s CheckForComplete, this PolicyRefFile is complete.\n", (const char *)mcsURISpec) );

      // Indicate this Policy is complete
      mProcessComplete = PR_TRUE;
    }
  }
  else {
    // We don't have a valid document, indicate this Policy is complete
    mProcessComplete = PR_TRUE;
  }

  return;
}


// ****************************************************************************
// nsIP3PPolicyRefFile routines
// ****************************************************************************

// Function:  Returns the Policy object associated with the method/path combination.
//
// Parms:     1. In     An indicator as to whether this check is for an embedded object
//            2. In     The method used
//            3. In     The URI string
//            4. Out    The reading PolicyRefFile URI spec
//            5. Out    The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            6. Out    The expired PolicyRefFile URI spec
//            7. Out    The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            8. Out    The Policy object
NS_IMETHODIMP
nsP3PPolicyRefFile::GetPolicy( PRBool         aEmbedCheck,
                               nsString&      aURIMethod,
                               nsString&      aURIString,
                               nsString&      aReadingPolicyRefFileURISpec,
                               PRInt32&       aReadingReferencePoint,
                               nsString&      aExpiredPolicyRefFileURISpec,
                               PRInt32&       aExpiredReferencePoint,
                               nsIP3PPolicy **aPolicy ) {

  nsresult                       rv = NS_OK;

  PRBool                         bExpired;

  nsP3PMetaTag                  *pMetaTag             = nsnull;

  nsP3PPolicyReferencesTag      *pPolicyReferencesTag = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;

  nsAutoString                   sPolicyURISpec;


  NS_ENSURE_ARG_POINTER( aPolicy );

  aReadingPolicyRefFileURISpec.Truncate( );
  aExpiredPolicyRefFileURISpec.Truncate( );
  *aPolicy = nsnull;

  if (mProcessComplete) {

    if (mContentValid && mDocumentValid) {
      rv = CheckExpiration( bExpired );

      if (!bExpired) {
        mFirstUse = PR_FALSE;

        if (MatchingAllowed( aEmbedCheck, aURIString )) {
          rv = mTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                           (void **)&pMetaTag );

          if (NS_SUCCEEDED( rv )) {
            rv = pMetaTag->mPolicyReferencesTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                                                       (void **)&pPolicyReferencesTag );

            if (NS_SUCCEEDED( rv )) {
              rv = NS_NewP3PSimpleEnumerator( pPolicyReferencesTag->mPolicyRefTags,
                                              getter_AddRefs( pEnumerator ) );

              if (NS_SUCCEEDED( rv )) {
                rv = pEnumerator->HasMoreElements(&bMoreTags );

                // Process all <POLICY-REF> tags until a match is found
                while (NS_SUCCEEDED( rv ) && bMoreTags && (sPolicyURISpec.Length( ) == 0)) {
                  rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

                  if (NS_SUCCEEDED( rv )) {
                    rv = MatchWithPolicyRefTag( pTag,
                                                aEmbedCheck,
                                                aURIMethod,
                                                aURIString,
                                                sPolicyURISpec );
                  } 

                  if (NS_SUCCEEDED( rv )) {
                    rv = pEnumerator->HasMoreElements(&bMoreTags );
                  }
                }

                if (NS_SUCCEEDED( rv )) {

                  if (sPolicyURISpec.Length( ) > 0) {
                    // We have a Policy URI spec
                    nsStringKey  keyPolicyURISpec( sPolicyURISpec );

                    if (mInlinePolicies.Exists(&keyPolicyURISpec )) {
                      // We have an inline Policy
                      *aPolicy = (nsIP3PPolicy *)mInlinePolicies.Get(&keyPolicyURISpec );
                    }
                    else {
                      // Look for an existing Policy
                      rv = mP3PService->GetCachedPolicy( sPolicyURISpec,
                                                         aPolicy );

                      if (NS_SUCCEEDED( rv ) && !*aPolicy) {
                        rv = NS_NewP3PPolicy( sPolicyURISpec,
                                              aPolicy );

                        if (NS_FAILED( rv )) {
                          PR_LOG( gP3PLogModule,
                                  PR_LOG_ERROR,
                                  ("P3PPolicyRefFile:  %s GetPolicy, NS_NewP3PPolicy failed - %X.\n", (const char *)mcsURISpec, rv) );
                        }
                      }
                      else if (NS_FAILED( rv )) {
                        PR_LOG( gP3PLogModule,
                                PR_LOG_ERROR,
                                ("P3PPolicyRefFile:  %s GetPolicy, mP3PService->GetCachedPolicy failed - %X.\n", (const char *)mcsURISpec, rv) );
                      }
                    }
                  }
                }
              }
              else {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PPolicyRefFile:  %s GetPolicy, NS_NewP3PSimpleEnumertor failed - %X.\n", (const char *)mcsURISpec, rv) );
              }

              NS_RELEASE( pPolicyReferencesTag );
            }

            NS_RELEASE( pMetaTag );
          }
        }
      }
      else {
        // Expired
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PPolicyRefFile:  %s GetPolicy, document expired.\n", (const char *)mcsURISpec) );

        aExpiredPolicyRefFileURISpec = mURISpec;
        aExpiredReferencePoint       = mReferencePoint;
      }
    }
    else {
      if (mContentValid) {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicyRefFile:  %s GetPolicy, document not valid.\n", (const char *)mcsURISpec) );

        rv = NS_ERROR_FAILURE;
      }
    }
  }
  else {
    // Read not complete
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicyRefFile:  %s GetPolicy, document not yet read.\n", (const char *)mcsURISpec) );

    aReadingPolicyRefFileURISpec = mURISpec;
    aReadingReferencePoint       = mReferencePoint;
  }

  return rv;
}


// ****************************************************************************
// nsP3PPolicyRefFile routines
// ****************************************************************************

// P3P Policy Ref File: ProcessPolicyRefFile
//
// Function:  Validate and process the PolicyRefFile
//
// Parms:     1. In     The type of read performed
//
NS_METHOD_( void )
nsP3PPolicyRefFile::ProcessPolicyRefFile( PRInt32   aReadType ) {

  nsresult                       rv;

  nsCOMPtr<nsIP3PTag>            pTag;                // Required for macro...

  nsCOMPtr<nsIDOMNode>           pDOMNodeWithTag;     // Required for macro...


  // Look for META tag
  NS_P3P_TAG_PROCESS_SINGLE( Meta,
                             META,
                             P3P,
                             PR_FALSE );

  if (NS_SUCCEEDED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicyRefFile:  %s ProcessPolicyRefFile, <META> tag successfully processed.\n", (const char *)mcsURISpec) );

    mTag = pTag;

    // Check for the <EXPIRY> tag and calculate the expiration of the PolicyRefFile
    rv = ProcessExpiration( );

    if (NS_SUCCEEDED( rv )) {
      // Record all the inline Policies
      rv = ProcessInlinePolicies( aReadType );
    }
  }

  if (NS_FAILED( rv )) {
#ifdef DEBUG_P3P
    { printf( "P3P:    Policy Ref File %s not valid\n", (const char *)mcsURISpec );
    }
#endif

    mDocumentValid = PR_FALSE;
  }

  return;
}

// P3P Policy Ref File: ProcessExpiration
//
// Function:  Set the expiration of the PolicyRefFile.
//
// Parms:     None
//
NS_METHOD
nsP3PPolicyRefFile::ProcessExpiration( ) {

  nsresult      rv;

  nsP3PMetaTag *pMetaTag = nsnull;


  rv = mTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pMetaTag );

  if (NS_SUCCEEDED( rv )) {
    rv = ProcessPolicyReferencesTagExpiry( pMetaTag->mPolicyReferencesTag );

    if (NS_SUCCEEDED( rv )) {
      CalculateExpiration( P3P_DOCUMENT_DEFAULT_LIFETIME );
    }

    NS_RELEASE( pMetaTag );
  }

  return rv;
}

// P3P Policy Ref File: ProcessPolicyReferencesTagExpiry
//
// Function:  Process the <EXPIRY> tag to set the expiration of the PolicyRefFile.
//
// Parms:     1. In     The <POLICY-REFERENCES> tag object
//
NS_METHOD
nsP3PPolicyRefFile::ProcessPolicyReferencesTagExpiry( nsIP3PTag  *aTag ) {

  nsresult                  rv;

  nsP3PPolicyReferencesTag *pPolicyReferencesTag = nsnull;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pPolicyReferencesTag );

  if (NS_SUCCEEDED( rv )) {

    if (pPolicyReferencesTag->mExpiryTag) {
      rv = ProcessExpiryTagExpiry( pPolicyReferencesTag->mExpiryTag );
    }

    NS_RELEASE( pPolicyReferencesTag );
  }

  return rv;
}

// P3P Policy Ref File: ProcessExpiryTagExpiry
//
// Function:  Override any HTTP cache related header information to set the expiration
//            of the PolicyRefFile.
//
// Parms:     1. In     The <EXPIRY> tag object
//
NS_METHOD
nsP3PPolicyRefFile::ProcessExpiryTagExpiry( nsIP3PTag  *aTag ) {

  nsresult        rv;

  nsP3PExpiryTag *pExpiryTag = nsnull;

  nsCAutoString   csAttr;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pExpiryTag );

  if (NS_SUCCEEDED( rv )) {

    if (pExpiryTag->mDate.Length( ) > 0) {
      // "date" attribute specified
      csAttr.AssignWithConversion( pExpiryTag->mDate );
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PPolicyRefFile:  %s ProcessExpiryTagExpiry, date attribute specified: %s.\n", (const char *)mcsURISpec, (const char *)csAttr) );

      mCacheControlValue.Truncate( );
      mExpiresValue = pExpiryTag->mDate;
    }
    else if (pExpiryTag->mMaxAge.Length( ) > 0) {
      csAttr.AssignWithConversion( pExpiryTag->mMaxAge );
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PPolicyRefFile:  %s ProcessExpiryTagExpiry, max-age attribute specified: %s.\n", (const char *)mcsURISpec, (const char *)csAttr) );

      // "max-age" attribute specified
      mCacheControlValue.AssignWithConversion( P3P_CACHECONTROL_MAXAGE );
      mCacheControlValue.AppendWithConversion( "=" );
      mCacheControlValue += pExpiryTag->mMaxAge;
    }

    NS_RELEASE( pExpiryTag );
  }

  return rv;
}

// P3P Policy Ref File: ProcessInlinePolicies
//
// Function:  Find all the inline <POLICY> tags.
//
// Parms:     1. In     The type of read performed
//
NS_METHOD
nsP3PPolicyRefFile::ProcessInlinePolicies( PRInt32   aReadType ) {

  nsresult      rv;

  nsP3PMetaTag *pMetaTag = nsnull;


  rv = mTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pMetaTag );

  if (NS_SUCCEEDED( rv )) {
    rv = ProcessPolicyReferencesTagPolicies( pMetaTag->mPolicyReferencesTag,
                                             aReadType );

    NS_RELEASE( pMetaTag );
  }

  return rv;
}

// P3P Policy Ref File: ProcessPolicyReferencesTagPolicies
//
// Function:  Processes all <POLICIES> tag looking for inline Policies.
//
// Parms:     1. In     The <POLICY-REFERENCES> tag object
//            2. In     The type of read performed
//
NS_METHOD
nsP3PPolicyRefFile::ProcessPolicyReferencesTagPolicies( nsIP3PTag  *aTag,
                                                        PRInt32     aReadType ) {

  nsresult                  rv;

  nsP3PPolicyReferencesTag *pPolicyReferencesTag = nsnull;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pPolicyReferencesTag );

  if (NS_SUCCEEDED( rv )) {

    if (pPolicyReferencesTag->mPoliciesTag) {
      rv = ProcessPoliciesTagPolicies( pPolicyReferencesTag->mPoliciesTag,
                                       aReadType );
    }

    NS_RELEASE( pPolicyReferencesTag );
  }

  return rv;
}

// P3P Policy Ref File: ProcessPoliciesTagPolicy
//
// Function:  Processes all inline <POLICY> tags.
//
// Parms:     1. In     The <POLICIES> tag object
//            2. In     The type of read performed
//
NS_METHOD
nsP3PPolicyRefFile::ProcessPoliciesTagPolicies( nsIP3PTag  *aTag,
                                                PRInt32     aReadType ) {

  nsresult                       rv;

  nsP3PPoliciesTag              *pPoliciesTag = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pPoliciesTag );

  if (NS_SUCCEEDED( rv )) {
    rv = NS_NewP3PSimpleEnumerator( pPoliciesTag->mPolicyTags,
                                    getter_AddRefs( pEnumerator ) );

    if (NS_SUCCEEDED( rv )) {
      rv = pEnumerator->HasMoreElements(&bMoreTags );

      // Process all the <POLICY> tags
      while (NS_SUCCEEDED( rv ) && bMoreTags) {
        rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

        if (NS_SUCCEEDED( rv )) {
          rv = AddInlinePolicy( pTag,
                                aReadType );
        }

        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreTags );
        }
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicyRefFile:  %s ProcessPoliciesTagPolicies, NS_NewP3PSimpleEnumerator failed - %X.\n", (const char *)mcsURISpec, rv) );
    }


    NS_RELEASE( pPoliciesTag );
  }

  return rv;
}

// P3P Policy Ref File: AddInlinePolicy
//
// Function:  Add the inline Policy to the list of Policies to be read.
//
// Parms:     1. In     The inline <POLICY> tag object
//            2. In     The type of read performed
//
NS_METHOD
nsP3PPolicyRefFile::AddInlinePolicy( nsIP3PTag  *aTag,
                                     PRInt32     aReadType ) {

  nsresult                rv = NS_OK;

  nsP3PPolicyTag         *pPolicyTag      = nsnull;

  nsAutoString            sPolicyName,
                          sPolicyURISpec;

  nsCAutoString           csPolicyURISpec;

  nsCOMPtr<nsIP3PPolicy>  pPolicy;

  nsCOMPtr<nsIDOMNode>    pDOMNode;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pPolicyTag );

  if (NS_SUCCEEDED( rv )) {
    sPolicyName.AssignWithConversion( "#" );
    sPolicyName += pPolicyTag->mName;

    rv = CreatePolicyURISpec( sPolicyName,
                              sPolicyURISpec );

    if (NS_SUCCEEDED( rv )) {
      nsStringKey  keyPolicyURISpec( sPolicyURISpec );

      if (!mInlinePolicies.Exists(&keyPolicyURISpec )) {
        csPolicyURISpec.AssignWithConversion( sPolicyURISpec );
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PPolicyRefFile:  %s AddInlinePolicy, adding inline Policy %s.\n", (const char *)mcsURISpec, (const char *)csPolicyURISpec) );

        // Create a new Policy object
        rv = NS_NewP3PPolicy( sPolicyURISpec,
                              getter_AddRefs( pPolicy ) );

        if (NS_SUCCEEDED( rv )) {
          // Add to the collection of Policies
          mInlinePolicies.Put(&keyPolicyURISpec,
                               pPolicy );

          rv = aTag->GetDOMNode( getter_AddRefs( pDOMNode ) );

          if (NS_SUCCEEDED( rv )) {
            rv = pPolicy->InlineRead( pDOMNode,
                                      mRedirected,
                                      mExpirationTime,
                                      aReadType,
                                      mURI,
                                      this,
                                      nsnull );

            if (NS_SUCCEEDED( rv )) {

              if (rv == P3P_PRIVACY_IN_PROGRESS) {
                rv = mInlinePoliciesToRead.AppendString( sPolicyURISpec ) ? NS_OK : NS_ERROR_FAILURE;

                if (NS_FAILED( rv )) {
                  PR_LOG( gP3PLogModule,
                          PR_LOG_ERROR,
                          ("P3PPolicyRefFile:  %s AddInlinePolicy, mInlinePoliciesToRead.AppendString failed - %X.\n", (const char *)mcsURISpec, rv) );
                }
              }
            }
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicyRefFile:  %s AddInlinePolicy, NS_NewP3PPolicy failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
    }

    NS_RELEASE( pPolicyTag );
  }

  return rv;
}

// P3P Policy Ref File: CheckExpiration
//
// Function:  Determines if the PolicyRefFile has expired.
//
// Parms:     1. Out    The indicator of whether the PolicyRefFile is expired
//
NS_METHOD
nsP3PPolicyRefFile::CheckExpiration( PRBool&  aExpired ) {

  nsresult  rv = NS_OK;

  PRTime    currentTime = PR_Now( );


  aExpired = PR_FALSE;

  if (!mFirstUse) {

    if (mRedirected) {
      // PolicyRefFile was redirected so indicate it has expired so it can be re-read
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PPolicyRefFile:  %s CheckExpiration, redirected PolicyRefFile - indicate expired.\n", (const char *)mcsURISpec) );

      aExpired = PR_TRUE;
    }
    else if (LL_CMP( currentTime, >, mExpirationTime )) {
      // PolicyRefFile has expired
      PRExplodedTime  explodedTime;
      char            csExpirationTime[64],
                      csCurrentTime[64];

      PR_ExplodeTime( mExpirationTime,
                      PR_GMTParameters,
                     &explodedTime );
      PR_FormatTimeUSEnglish( csExpirationTime,
                              sizeof( csExpirationTime ),
                              "%a, %d-%b-%Y %H:%M:%S GMT",
                             &explodedTime );

      PR_ExplodeTime( currentTime,
                      PR_GMTParameters,
                     &explodedTime );
      PR_FormatTimeUSEnglish( csCurrentTime,
                              sizeof( csCurrentTime ),
                              "%a, %d-%b-%Y %H:%M:%S GMT",
                             &explodedTime );

      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PPolicyRefFile:  %s CheckExpiration, PolicyRefFile has expired - expiration time: %s, time now: %s.\n", (const char *)mcsURISpec, csExpirationTime, csCurrentTime) );

      aExpired = PR_TRUE;
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicyRefFile:  %s CheckExpiration, first use of PolicyRefFile - indicate not expired.\n", (const char *)mcsURISpec) );
  }

  return rv;
}

// P3P Policy Ref File: MatchWithPolicyRefTag
//
// Function:  Validate and process the PolicyRefFile.
//
// Parms:     1. In     An indicator as to whether this check is for an embedded object
//            2. In     The URI string to perform the match against
//
NS_METHOD_( PRBool )
nsP3PPolicyRefFile::MatchingAllowed( PRBool     aEmbedCheck,
                                     nsString&  aURIString ) {

  nsresult        rv;

  PRBool          bContinue = PR_TRUE;

  nsXPIDLCString  xcsURIString;

  PRInt32         iLastSlash;

  nsAutoString    sURIString,
                  sURIPrefix;


  if (!aEmbedCheck) {
    // Not an embedded check so we must further evaluate

    if (mReferencePoint == P3P_REFERENCE_LINK_TAG) {
      // Referenced via a <LINK> tag so we must further evaluate
      rv = mURI->GetPath( getter_Copies( xcsURIString ) );

      if (NS_SUCCEEDED( rv )) {
        sURIString.AssignWithConversion((const char *)xcsURIString );

        iLastSlash = sURIString.RFindChar( '/' );

        if (iLastSlash >= 0) {
          // Create the URI prefix including the slash
          sURIString.Left( sURIPrefix,
                           iLastSlash + 1 );

          if (aURIString.Length( ) >= sURIPrefix.Length( )) {

            if (!aURIString.EqualsWithConversion( sURIPrefix, PR_FALSE, sURIPrefix.Length( ) )) {
              // If the path doesn't match the prefix of this PolicyRefFile then it can't be used
              PR_LOG( gP3PLogModule,
                      PR_LOG_NOTICE,
                      ("P3PPolicyRefFile:  %s MatchingAllowed, path to be checked is not part of URI prefix - matching not allowed.\n", (const char *)mcsURISpec) );

              bContinue = PR_FALSE;
            }
          }
          else {
            // The path is less than the prefix, can't possibly match so it can't be used
            PR_LOG( gP3PLogModule,
                    PR_LOG_NOTICE,
                    ("P3PPolicyRefFile:  %s MatchingAllowed, path to be checked is smaller than URI prefix - matching not allowed.\n", (const char *)mcsURISpec) );

            bContinue = PR_FALSE;
          }
        }
        else {
          // Path should always have a "/" but this one seems to not
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicyRefFile:  %s MatchingAllowed, URI path does not contain a \"/\" - matching not allowed.\n", (const char *)mcsURISpec) );

          bContinue = PR_FALSE;
        }
      }
      else {
        // Could not obtain the path from the URI object
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicyRefFile:  %s MatchingAllowed, mURI->GetPath failed - %X - matching not allowed.\n", (const char *)mcsURISpec, rv) );

        bContinue = PR_FALSE;
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PPolicyRefFile:  %s MatchingAllowed, PolicyRefFile specified via HTTP header - matching allowed.\n", (const char *)mcsURISpec) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicyRefFile:  %s MatchingAllowed, an embedded check is being performed - matching allowed.\n", (const char *)mcsURISpec) );
  }

  return bContinue;
}

// P3P Policy Ref File: MatchWithPolicyRefTag
//
// Function:  Validate and process the PolicyRefFile.
//
// Parms:     1. In     The <POLICY-REF> tag object
//            2. In     An indicator as to whether this check is for an embedded object
//            3. In     The method to perform the match against
//            4. In     The URI string to perform the match against
//            5. Out    The Policy URI spec if a match is found
//
NS_METHOD
nsP3PPolicyRefFile::MatchWithPolicyRefTag( nsIP3PTag  *aTag,
                                           PRBool      aEmbedCheck,
                                           nsString&   aURIMethod,
                                           nsString&   aURIString,
                                           nsString&   aPolicyURISpec ) {

  nsresult            rv;

  nsAutoString        sURIString( aURIString );

  nsP3PPolicyRefTag  *pPolicyRefTag = nsnull;

  PRBool              bMethodMatch   = PR_FALSE,
                      bIncludeMatch  = PR_FALSE,
                      bExcludeMatch  = PR_FALSE;


  aPolicyURISpec.Truncate( );

  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pPolicyRefTag );

  if (NS_SUCCEEDED( rv )) {

    rv = MethodMatch( pPolicyRefTag->mMethodTags,
                      aURIMethod,
                      bMethodMatch );

    if (NS_SUCCEEDED( rv )) {

      if (bMethodMatch) {
        // The URI method matched a <METHOD> tag or thare are no <METHOD> tags
        nsCAutoString  csURIString;

        UnEscapeURILegalCharacters( sURIString );

        csURIString.AssignWithConversion( sURIString );
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PPolicyRefFile:  %s MatchWithPolicyRefTag, checking for an \"INCLUDE\" match for - %s.\n", (const char *)mcsURISpec, (const char *)csURIString) );

        rv = URIStringMatch( aEmbedCheck ? pPolicyRefTag->mEmbeddedIncludeTags : pPolicyRefTag->mIncludeTags,
                             sURIString,
                             bIncludeMatch );

        if (NS_SUCCEEDED( rv )) {

          if (bIncludeMatch) {
            // The URI path matched an <INCLUDE>/<EMBEDDED-INCLUDE> tag
            PR_LOG( gP3PLogModule,
                    PR_LOG_NOTICE,
                    ("P3PPolicyRefFile:  %s MatchWithPolicyRefTag, checking for an \"EXCLUDE\" match for - %s.\n", (const char *)mcsURISpec, (const char *)csURIString) );

            rv = URIStringMatch( aEmbedCheck ? pPolicyRefTag->mEmbeddedExcludeTags : pPolicyRefTag->mExcludeTags,
                                 sURIString,
                                 bExcludeMatch );

            if (NS_SUCCEEDED( rv )) {

              if (!bExcludeMatch) {
                // The URI path did not match an <EXCLUDE>/<EMBEDDED-EXCLUDE> tag, get the Policy URI spec
                rv = CreatePolicyURISpec( pPolicyRefTag->mAbout,
                                          aPolicyURISpec );

                if (NS_SUCCEEDED( rv )) {
                  // Policy URI spec successfully created
                  csURIString.AssignWithConversion( aPolicyURISpec );
                  PR_LOG( gP3PLogModule,
                          PR_LOG_NOTICE,
                          ("P3PPolicyRefFile:  %s MatchWithPolicyRefTag, match found, Policy is - %s.\n", (const char *)mcsURISpec, (const char *)csURIString) );
                }
              }
            }
          }
        }
      }
    }

    NS_RELEASE( pPolicyRefTag );
  }

  return rv;
}

// P3P Policy Ref File:  MethodMatch
//
// Function:  Searches through the list of <METHOD> tags looking for a match with
//            the supplied method.  If no <METHOD> tags are present, the method is
//            also considered matched.
//
// Parms:     1. In     The list of <METHOD> tags
//            2. In     The method to match against
//            3. Out    An indicator of whether there was a match
//
NS_METHOD
nsP3PPolicyRefFile::MethodMatch( nsISupportsArray  *aTags,
                                 nsString&          aURIMethod,
                                 PRBool&            aMatch ) {

  nsresult                       rv;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;

  nsAutoString                   sTagText;

  PRBool                         bTagPresent = PR_FALSE,
                                 bMatch      = PR_FALSE;


  aMatch = PR_FALSE;

  rv = NS_NewP3PSimpleEnumerator( aTags,
                                  getter_AddRefs( pEnumerator ) );

  if (NS_SUCCEEDED( rv )) {
    rv = pEnumerator->HasMoreElements(&bMoreTags );

    // Process all the <METHOD> tags looking for a match
    while (NS_SUCCEEDED( rv ) && !bMatch && bMoreTags) {
      bTagPresent = PR_TRUE;

      rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

      if (NS_SUCCEEDED( rv )) {
        pTag->GetText( sTagText );

        if (sTagText.EqualsIgnoreCase( aURIMethod )) {
          // Methods match
          bMatch = PR_TRUE;
        }
      }

      if (NS_SUCCEEDED( rv )) {
        rv = pEnumerator->HasMoreElements(&bMoreTags );
      }
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicyRefFile:  %s MethodMatch, NS_NewP3PSimpleEnumerator failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  if (NS_SUCCEEDED( rv )) {
    nsCAutoString  csURIMethod;

    csURIMethod.AssignWithConversion( aURIMethod );
    if (bMatch || !bTagPresent) {
      // Indicate a match
      if (bMatch) {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PPolicyRefFile:  %s MethodMatch, <METHOD> tag match found for %s.\n", (const char *)mcsURISpec, (const char *)csURIMethod) );
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PPolicyRefFile:  %s MethodMatch, no <METHOD> tags so considered a match.\n", (const char *)mcsURISpec) );
      }

      aMatch = PR_TRUE;
    }
    else {
      // No match found
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicyRefFile:  %s MethodMatch, no matching <METHOD> tag found.\n", (const char *)mcsURISpec) );
    }
  }

  return rv;
}

// P3P Policy Ref File:  URIStringMatch
//
// Function:  Searches through the list of supplied tags looking for a match
//            with the supplied URI string (path/spec).
//
// Parms:     1. In     The list of tags
//            2. In     The URI string to match against (path/spec)
//            3. Out    An indicator of whether there was a match
//
NS_METHOD
nsP3PPolicyRefFile::URIStringMatch( nsISupportsArray  *aTags,
                                    nsString&          aURIString,
                                    PRBool&            aMatch ) {

  nsresult                       rv;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;

  nsAutoString                   sTagText;

  PRBool                         bMatch      = PR_FALSE;


  aMatch = PR_FALSE;

  rv = NS_NewP3PSimpleEnumerator( aTags,
                                  getter_AddRefs( pEnumerator ) );

  if (NS_SUCCEEDED( rv )) {
    rv = pEnumerator->HasMoreElements(&bMoreTags );

    // Process all the tags looking for a match
    while (NS_SUCCEEDED( rv ) && !bMatch && bMoreTags) {
      rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

      if (NS_SUCCEEDED( rv )) {
        nsCAutoString  csURIString, csTagText;

        pTag->GetText( sTagText );

        csURIString.AssignWithConversion( aURIString );
        csTagText.AssignWithConversion( sTagText );
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PPolicyRefFile:  %s URIStringMatch, checking for match of %s against %s.\n", (const char *)mcsURISpec, (const char *)csURIString, (const char *)csTagText) );

        bMatch = PatternMatch( sTagText,
                               aURIString );
      }

      if (NS_SUCCEEDED( rv )) {
        rv = pEnumerator->HasMoreElements(&bMoreTags );
      }
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicyRefFile:  %s URIStringMatch, NS_NewP3PSimpleEnumerator failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  if (NS_SUCCEEDED( rv ) && bMatch) {
    // Indicate a match
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicyRefFile:  %s URIStringMatch, match successful.\n", (const char *)mcsURISpec) );

    aMatch = PR_TRUE;
  }

  return rv;
}

// P3P Policy Ref File:  UnEscapeURILegalCharacters
//
// Function:  Replaces "escaped" URI legal characters with the actual character.
//            (see mURILegalEscapeTable)
//
// Parms:     1. In     The string to be modified
//
NS_METHOD_( void )
nsP3PPolicyRefFile::UnEscapeURILegalCharacters( nsString&  aString ) {

  PRInt32       iPercentOffset = -1;

  nsAutoString  sEscapeSequence,
                sCharacter;


  do {
    iPercentOffset = aString.FindChar( '%', PR_FALSE, iPercentOffset + 1 );

    if (iPercentOffset > 0) {
      // Found an escaped character

      if (aString.Length( ) > (PRUint32)(iPercentOffset + 2)) {
        // Enough characters remain for a valid escape
        PRUnichar  ucOne = CharToNum( GetCharAt( aString, iPercentOffset + 1 ) ),
                   ucTwo = CharToNum( GetCharAt( aString, iPercentOffset + 2 ) );

        if ((ucOne < 16) && (ucTwo < 16)) {
          // 0-9 or A-F or a-f characters found
          PRInt32  iIndex = (ucOne << 4) + ucTwo;

          if (mURILegalEscapeTable[iIndex]) {
            aString.Mid( sEscapeSequence,
                         iPercentOffset, 3 );

            sCharacter = (PRUnichar)mURILegalEscapeTable[iIndex];

            UnEscape( aString,
                      sEscapeSequence,
                      sCharacter );
          }
        }
      }
    }
  } while (iPercentOffset != kNotFound);

  return;
}

// P3P Policy Ref File:  UnEscapeAsterisks
//
// Function:  Replaces "escaped" "*" characters with actual "*" character.
//
// Parms:     1. In     The string to be modified
//
NS_METHOD_( void )
nsP3PPolicyRefFile::UnEscapeAsterisks( nsString&  aString ) {

  nsAutoString  sAsterisk,
                sAsteriskEscapeSequence;

  sAsterisk.AssignWithConversion( "*" );

  sAsteriskEscapeSequence.AssignWithConversion( "%2a" );
  UnEscape( aString,
            sAsteriskEscapeSequence,
            sAsterisk );

  sAsteriskEscapeSequence.AssignWithConversion( "%2A" );
  UnEscape( aString,
            sAsteriskEscapeSequence,
            sAsterisk );

  return;
}

// P3P Policy Ref File:  UnEscape
//
// Function:  Replaces specified "escaped" characters with specified character.
//
// Parms:     1. In     The string to be modified
//            2. In     The escape sequence
//            3. In     The character used to replace the escape sequence
//
NS_METHOD_( void )
nsP3PPolicyRefFile::UnEscape( nsString&  aString,
                              nsString&  aEscapeSequence,
                              nsString&  aCharacter ) {

  aString.ReplaceSubstring( aEscapeSequence, aCharacter );

  return;
}

// P3P Policy Ref File: CharToNum
//
// Function:  Takes a character representation of a valid hex number and returns the numerical value.
//
// Parms:     1. In     The character to converted
//
NS_METHOD_( PRUnichar )
nsP3PPolicyRefFile::CharToNum( PRUnichar   aChar ) {

  PRUnichar  ucResult = 16;


  if ((aChar >= '0') && (aChar <= '9')) {
    ucResult = aChar - '0';
  }
  else if ((aChar >= 'A') && (aChar <= 'F')) {
    ucResult = aChar - 'A' + 10;
  }
  else if ((aChar >= 'a') && (aChar <= 'f')) {
    ucResult = aChar - 'a' + 10;
  }

  return ucResult;
}

// P3P Policy Ref File:  PatternMatch
//
// Function:  Determine if the URI pattern matches a URI Path.
//
// Parms:     1. In     The pattern
//            2. In     The path
//
NS_METHOD_( PRBool )
nsP3PPolicyRefFile::PatternMatch( nsString&  aURIPattern,
                                  nsString&  aURIString ) {

  nsAutoString  sURIPattern( aURIPattern ),
                sURIString( aURIString );

  nsAutoString  sMatchURIPattern,
                sNewURIPattern,
                sNewURIPath;

  nsAutoString  sWorkString;

  PRInt32       iWildCard;

  PRInt32       iMatchURIPattern;

  PRBool        bMatch;


  if ((sURIPattern.Length( ) == 0) && (sURIString.Length( ) == 0)) {
    // Both the pattern and the string are empty - match
    bMatch = PR_TRUE;
  }
  else if ((sURIPattern.Length( ) == 0) && (sURIString.Length( ) > 0)) {
    // The pattern is empty but there is still some string left - no match
    bMatch = PR_FALSE;
  }
  else {
    iWildCard = sURIPattern.FindChar( '*' );

    if (iWildCard == 0) {
      // First character of pattern is an "*"

      // Skip over consecutive "*" characters
      do {
        sURIPattern.Right( sWorkString,
                           sURIPattern.Length( ) - 1 );
        sURIPattern = sWorkString;
      } while (GetCharAt( sURIPattern, 0 ) == '*' );

      if (sURIPattern.Length( ) == 0) {
        // Only an "*" was in the pattern
        bMatch = PR_TRUE;
      }
      else {
        // Extract pattern to perform match against
        sURIPattern.Left( sMatchURIPattern,
                          sURIPattern.FindChar( '*' ) );

        // Change any "escaped" "*" characters to "*"
        UnEscapeAsterisks( sMatchURIPattern );

        // Look for the pattern in the path
        iMatchURIPattern = sURIString.Find( sMatchURIPattern );

        if (iMatchURIPattern >= 0) {
          // Pattern was found, adjust the pattern and the path for the match
          if (sURIPattern.FindChar( '*' ) >= 0) {
            sURIPattern.Right( sNewURIPattern,
                               sURIPattern.Length( ) - sURIPattern.FindChar( '*' ) );
          }
          sURIString.Right( sNewURIPath,
                            sURIString.Length( ) - (iMatchURIPattern + sMatchURIPattern.Length( )) );

          // Check for a match
          bMatch = PatternMatch( sNewURIPattern,
                                 sNewURIPath );
        }
        else {
          // Pattern was not found
          bMatch = PR_FALSE;
        }
      }
    }

    else if (iWildCard > 0) {
      // First character of pattern is not an "*"

      // Extract pattern to perform match against
      sURIPattern.Left( sMatchURIPattern,
                        iWildCard );

      // Change any "escaped" "*" characters to "*"
      UnEscapeAsterisks( sMatchURIPattern );

      if ((sURIString.Length( ) >= sMatchURIPattern.Length( )) &&
          sURIString.EqualsWithConversion( sMatchURIPattern, PR_FALSE, iWildCard )) {
          // Pattern was found, adjust the pattern and the path for the match
        sURIPattern.Right( sNewURIPattern,
                           sURIPattern.Length( ) - iWildCard );
        sURIString.Right( sNewURIPath,
                          sURIString.Length( ) - sMatchURIPattern.Length( ) );

        // Check for a match
        bMatch = PatternMatch( sNewURIPattern,
                               sNewURIPath );
      }
      else {
        // Pattern was not found
        bMatch = PR_FALSE;
      }
    }

    else {
      // No "*" found, just compare the strings

      // Change any "escaped" "*" characters to "*"
      UnEscapeAsterisks( sURIPattern );

      if (sURIPattern.EqualsWithConversion( sURIString )) {
        bMatch = PR_TRUE;
      }
      else {
        bMatch = PR_FALSE;
      }
    }
  }

  return bMatch;
}

// P3P Policy Ref File:  CreatePolicyURISpec
//
// Function:  Return a fully qualified Policy URI.
//
// Parms:     1. In     The Policy URI spec
//            2. Out    The absolute Policy URI spec
//
NS_METHOD
nsP3PPolicyRefFile::CreatePolicyURISpec( nsString&   aInPolicyURISpec,
                                         nsString&   aOutPolicyURISpec ) {

  nsresult          rv = NS_OK;

  nsCOMPtr<nsIURI>  pURI;

  nsXPIDLCString    xcsURISpec;


  rv = NS_NewURI( getter_AddRefs( pURI ),
                  aInPolicyURISpec,
                  mURI );

  if (NS_SUCCEEDED( rv )) {
    rv = pURI->GetSpec( getter_Copies( xcsURISpec ) );

    if (NS_SUCCEEDED( rv )) {
      aOutPolicyURISpec.AssignWithConversion((const char *)xcsURISpec );
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicyRefFile:  %s CreatePolicyURISpec, pURI->GetSpec failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicyRefFile:  %s CreatePolicyURISpec, NS_NewURI failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

#ifdef DEBUG_P3P
  { printf( "P3P:        Policy URI: %s\n", (const char *)xcsURISpec );
  }
#endif

  return rv;
}


// ****************************************************************************
// nsIP3PXMLListener routines
// ****************************************************************************

// P3P Policy Ref File: DocumentComplete
//
// Function:  Notification of the completion of a Policy.
//
// Parms:     1. In     The Policy URI spec that has completed
//            2. In     The data supplied on the intial processing request
//
NS_IMETHODIMP
nsP3PPolicyRefFile::DocumentComplete( nsString&     aProcessedURISpec,
                                      nsISupports  *aReaderData ) {

  nsresult                    rv = NS_OK;

  nsCAutoString               csURISpec;

  PRBool                      bNotifyReaders = PR_FALSE;

  nsCOMPtr<nsISupportsArray>  pReadRequests;


  if (aProcessedURISpec.Length( ) > 0) {
    nsAutoLock  lock( mLock );

    csURISpec.AssignWithConversion( aProcessedURISpec );
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicy:  %s DocumentComplete, %s complete.\n", (const char *)mcsURISpec, (const char *)csURISpec) );

    // Remove the Policy from the list of processing Policies
    mInlinePoliciesToRead.RemoveString( aProcessedURISpec );

    // Check if this PolicyRefFile is now complete
    CheckForComplete( );

    if (mProcessComplete) {
      mReadInProgress = PR_FALSE;
      bNotifyReaders  = PR_TRUE;
      rv = mReadRequests->Clone( getter_AddRefs( pReadRequests ) );

      if (NS_SUCCEEDED( rv )) {
        mReadRequests->Clear( );
      }
    }
  }

  if (NS_SUCCEEDED( rv ) && bNotifyReaders) {
    NotifyReaders( pReadRequests );
  }

  return NS_OK;
}
