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

#include "nsP3PPolicy.h"
#include "nsP3PDataSchema.h"
#include "nsP3PTags.h"
#include "nsP3PSimpleEnumerator.h"
#include "nsP3PLogging.h"

#include <nsIServiceManager.h>

#include <rdf.h>
#include <nsRDFCID.h>

#include <nsNetUtil.h>

#include <nsXPIDLString.h>
#include <prprf.h>


// ****************************************************************************
// nsP3PPolicy Implementation routines
// ****************************************************************************

static NS_DEFINE_CID( kRDFServiceCID,            NS_RDFSERVICE_CID );
static NS_DEFINE_CID( kRDFContainerCID,          NS_RDFCONTAINER_CID );
static NS_DEFINE_CID( kRDFContainerUtilsCID,     NS_RDFCONTAINERUTILS_CID );
static NS_DEFINE_CID( kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID );

// P3P Policy nsISupports
NS_IMPL_ISUPPORTS2( nsP3PPolicy, nsIP3PPolicy,
                                 nsIP3PXMLListener );

// P3P Policy creation routine
NS_METHOD
NS_NewP3PPolicy( nsString&      aPolicyURISpec,
                 nsIP3PPolicy **aPolicy ) {

  nsresult     rv;

  nsP3PPolicy *pNewPolicy = nsnull;


  NS_ENSURE_ARG_POINTER( aPolicy );

  *aPolicy = nsnull;

  pNewPolicy = new nsP3PPolicy( );

  if (pNewPolicy) {
    NS_ADDREF( pNewPolicy );

    rv = pNewPolicy->Init( aPolicyURISpec );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewPolicy->QueryInterface( NS_GET_IID( nsIP3PPolicy ),
                              (void **)aPolicy );
    }

    NS_RELEASE( pNewPolicy );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Policy unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Policy: Constructor
nsP3PPolicy::nsP3PPolicy( )
  : mDataSchemas( 32, PR_TRUE ),
    mPrefsTime( nsnull ),
    mPrefsResult( P3P_PRIVACY_MET ),
    mFailedDataTagRefs( 64, PR_TRUE ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Policy: Destructor
nsP3PPolicy::~nsP3PPolicy( ) {
}

// ****************************************************************************
// nsP3PXMLProcessor method overrides
// ****************************************************************************

// P3P Policy: PostInit
//
// Function:  Perform any additional intialization to the XMLProcessor's.
//
// Parms:     1. In     The URI spec of the Policy object
//
NS_IMETHODIMP
nsP3PPolicy::PostInit( nsString&  aURISpec ) {

  nsresult  rv;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PPolicy:  %s PostInit, initializing.\n", (const char *)mcsURISpec) );

  mDocumentType = P3P_DOCUMENT_POLICY;

  // Get the RDF service
  mRDFService = do_GetService( kRDFServiceCID,
                              &rv );

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicy:  %s PostInit, do_GetService for RDF service failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the RDF Container Utils service
    mRDFContainerUtils = do_GetService( kRDFContainerUtilsCID,
                                       &rv );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, do_GetService for RDF Container service failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the RDF "type" resource
    rv = mRDFService->GetResource( RDF_NAMESPACE_URI "type",
                                   getter_AddRefs( mRDFResourceRDFType) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %stype resource failed - %X.\n", (const char *)mcsURISpec, RDF_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PEntries" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PEntries",
                                   getter_AddRefs( mRDFResourceP3PEntries ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %sP3PEntries resource failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PEntity" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PEntity",
                                   getter_AddRefs( mRDFResourceP3PEntity ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %sP3PEntity resource failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PEntityInformation" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PEntityInformation",
                                   getter_AddRefs( mRDFResourceP3PEntityInformation ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %sP3PEntityInformation resource failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PPolicyURISpec" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PPolicyURISpec",
                                   getter_AddRefs( mRDFResourceP3PPolicyURISpec ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %sP3PPolicyURISpec resource failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PPolicyDescURISpec" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PPolicyDescURISpec",
                                   getter_AddRefs( mRDFResourceP3PPolicyDescriptionURISpec ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %sP3PPolicyDescURISpec resource failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PResult" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PResult",
                                   getter_AddRefs( mRDFResourceP3PResult ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %sP3PResult resource failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PDataTags" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PDataTags",
                                   getter_AddRefs( mRDFResourceP3PDataTags ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %sP3PDataTags resource failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PDataTag" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PDataTag",
                                   getter_AddRefs( mRDFResourceP3PDataTag ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %sP3PDataTag resource failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PDataCombination" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PDataCombination",
                                   getter_AddRefs( mRDFResourceP3PDataCombination ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %sP3PDataCombination resource failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PTitle" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PTitle",
                                   getter_AddRefs( mRDFResourceP3PTitle ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %sP3PTitle resource failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PValue" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PValue",
                                   getter_AddRefs( mRDFResourceP3PValue ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s PostInit, mRDFService->GetResource for %sP3PValue resource failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  return rv;
}

// P3P Policy: PreRead
//
// Function:  Cache the Policy if the Policy has not completed
//
// Parms:     1. In     The type of read performed
//            2. In     The Policy or DataSchema URI initiating the read
//
NS_IMETHODIMP
nsP3PPolicy::PreRead( PRInt32   aReadType,
                      nsIURI   *aReadForURI ) {

  if (!mProcessComplete && !mInlineRead) {
    // Cache the Policy if it hasn't been processed yet and is not an inline Policy
#ifdef DEBUG_P3P
    { printf( "P3P:  Caching Policy Object: %s\n", (const char *)mcsURISpec );
    }
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicy:  %s PreRead, caching Policy.\n", (const char *)mcsURISpec) );

    mP3PService->CachePolicy( mURISpec,
                              this );
  }

  return NS_OK;
}

// P3P Policy: ProcessContent
//
// Function:  Process the Policy.
//
// Parms:     1. In     The type of read performed
//
NS_IMETHODIMP
nsP3PPolicy::ProcessContent( PRInt32   aReadType ) {

  ProcessPolicy( aReadType );

  return NS_OK;
}

// P3P Policy: CheckForComplete
//
// Function:  Checks to see if all processing is complete for this DataSchema (this
//            includes processing other referenced DataSchemas).  If processing is
//            complete, then mProcessComplete must be set to TRUE.
//
// Parms:     None
//
NS_IMETHODIMP_( void )
nsP3PPolicy::CheckForComplete( ) {

  if (mContentValid && mDocumentValid) {

    if (mDataSchemasToRead.Count( ) == 0) {
      // All (if any) DataSchemas have been processed
      if (mDataSchemas.Count( ) > 0) {
        // There were some DataSchemas so log that information
#ifdef DEBUG_P3P
        { printf( "P3P:  All DataSchemas processed for %s\n", (const char *)mcsURISpec );
        }
#endif

        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PPolicy:  %s CheckForComplete, all DataSchemas processed.\n", (const char *)mcsURISpec) );
      }

#ifdef DEBUG_P3P
      { printf( "P3P:  Policy complete: %s\n", (const char *)mcsURISpec );
      }
#endif

      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PPolicy:  %s CheckForComplete, this Policy is complete.\n", (const char *)mcsURISpec) );

      // Compare the Policy to the user's preferences
      CompareToPrefs( );

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
// nsIP3PPolicy routines
// ****************************************************************************

// P3P Policy: ComparePolicyToPrefs
//
// Function:  Compares the Policy representation to the user's preferences.
//
// Parms:     None
//
NS_IMETHODIMP
nsP3PPolicy::ComparePolicyToPrefs( PRBool&  aPolicyExpired ) {

  nsresult  rv;


  if (mContentValid && mDocumentValid) {
    rv = CheckExpiration( aPolicyExpired );

    if (!aPolicyExpired) {
      mFirstUse = PR_FALSE;

      if (mP3PService->PrefsChanged( mPrefsTime )) {
        // Preferences have changed since last check, recheck
        PR_LOG( gP3PLogModule,
                PR_LOG_NOTICE,
                ("P3PPolicy:  %s CompareP3PPolicyToPrefs, preferences have changed - comparing.\n", (const char *)mcsURISpec) );

        CompareToPrefs( );
      }
    }
  }

  return (mContentValid && mDocumentValid) ? mPrefsResult : NS_ERROR_FAILURE;
}

// P3P Policy: GetPolicyDescription
//
// Function:  Gets the RDF description of the Policy information.
//
// Parms:     1. Out    The "about" attribute to be used to access the Policy information
//            2. Out    The "title" attribute to be used to describe the Policy information
//            3. Out    The RDFDataSource describing the Policy information
//
NS_IMETHODIMP
nsP3PPolicy::GetPolicyDescription( nsString&          aAbout,
                                   nsString&          aTitle,
                                   nsIRDFDataSource **aDataSource ) {

  nsresult      rv;

  nsAutoString  sTitle;


  NS_ENSURE_ARG_POINTER( aDataSource );

  *aDataSource = nsnull;

  rv = mP3PService->GetLocaleString( "PolicyInformation",
                                     sTitle );

  if (NS_SUCCEEDED( rv )) {
    aAbout       = mRDFAbout;
    aTitle       = sTitle;
    *aDataSource = mRDFDataSource;
    NS_IF_ADDREF( *aDataSource );
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicy:  %s GetPolicyDescription, mP3PService->GetLocaleString failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}


// ****************************************************************************
// nsP3PPolicy routines
// ****************************************************************************

// P3P Policy: ProcessPolicy
//
// Function:  Validate and process the Policy.
//
// Parms:     None
//
NS_METHOD_( void )
nsP3PPolicy::ProcessPolicy( PRInt32   aReadType ) {

  nsresult                    rv;

  nsCOMPtr<nsIP3PTag>         pTag;              // Required for macro...

  nsCOMPtr<nsIDOMNode>        pDOMNodeWithTag;   // Required for macro...


  // Look for POLICY tag and process the Policy
  NS_P3P_TAG_PROCESS_SINGLE( Policy,
                             POLICY,
                             P3P,
                             PR_FALSE );

  if (NS_SUCCEEDED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicy:  %s ProcessPolicy, <POLICY> tag successfully processed.\n", (const char *)mcsURISpec) );

    mTag = pTag;

    // Check for the <EXPIRY> tag and calculate the expiration of the Policy
    rv = ProcessExpiration( );

    if (NS_SUCCEEDED( rv )) {
      // Process all DataSchemas related to this Policy
      rv = ProcessDataSchemas( aReadType );
    }
  }

  if (NS_FAILED( rv )) {
    SetNotValid( );
  }

  return;
}

// P3P Policy: ProcessExpiration
//
// Function:  Set the expiration of the Policy.
//
// Parms:     None
//
NS_METHOD
nsP3PPolicy::ProcessExpiration( ) {

  nsresult        rv = NS_OK;

  nsP3PPolicyTag *pPolicyTag = nsnull;


  if (!mInlineRead) {
    // Not an inline Policy, calculate the expiration
    //   Inline Policies must use the expiration of the PolicyRefFile
    //   (set on the call to InlineRead)
    rv = mTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                     (void **)&pPolicyTag );

    if (NS_SUCCEEDED( rv )) {

      if (pPolicyTag->mExpiryTag) {
        rv = ProcessExpiryTagExpiry( pPolicyTag->mExpiryTag );
      }

      if (NS_SUCCEEDED( rv )) {
        CalculateExpiration( P3P_DOCUMENT_DEFAULT_LIFETIME );
      }

      NS_RELEASE( pPolicyTag );
    }
  }

  return rv;
}

// P3P Policy: ProcessExpiryTagExpiry
//
// Function:  Override any HTTP cache related header information to set the expiration
//            of the Policy.
//
// Parms:     1. In     The <EXPIRY> tag object
//
NS_METHOD
nsP3PPolicy::ProcessExpiryTagExpiry( nsIP3PTag  *aTag ) {

  nsresult        rv;

  nsP3PExpiryTag *pExpiryTag = nsnull;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pExpiryTag );

  if (NS_SUCCEEDED( rv )) {

    if (pExpiryTag->mDate.Length( ) > 0) {
      // "date" attribute specified
      mCacheControlValue.Truncate( );
      mExpiresValue = pExpiryTag->mDate;
    }
    else if (pExpiryTag->mMaxAge.Length( ) > 0) {
      // "max-age" attribute specified
      mCacheControlValue.AssignWithConversion( P3P_CACHECONTROL_MAXAGE );
      mCacheControlValue.AppendWithConversion( "=" );
      mCacheControlValue += pExpiryTag->mMaxAge;
    }

    NS_RELEASE( pExpiryTag );
  }

  return rv;
}

// P3P Policy: ProcessDataSchemas
//
// Function:  Find all DataSchemas associated with the Policy.
//
// Parms:     None
//
NS_METHOD
nsP3PPolicy::ProcessDataSchemas( PRInt32   aReadType ) {

  nsresult                       rv;

  nsP3PPolicyTag                *pPolicyTag    = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  rv = mTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pPolicyTag );

  if (NS_SUCCEEDED( rv )) {

    mDataSchemaTag = pPolicyTag->mDataSchemaTag;

    if (mDataSchemaTag) {
      // We have an inline DataSchema
      rv = AddDataSchema( mURISpec,
                          aReadType );
    }

    if (NS_SUCCEEDED( rv )) {
      rv = ProcessEntityTagDataSchemas( pPolicyTag->mEntityTag,
                                        aReadType );
    }

    if (NS_SUCCEEDED( rv )) {
      rv = NS_NewP3PSimpleEnumerator( pPolicyTag->mStatementTags,
                                      getter_AddRefs( pEnumerator ) );

      if (NS_SUCCEEDED( rv )) {
        // Process all <STATEMENT> tags
        rv = pEnumerator->HasMoreElements(&bMoreTags );

        while (NS_SUCCEEDED( rv ) && bMoreTags) {
          rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

          if (NS_SUCCEEDED( rv )) {
            // Record the DataSchema(s) related to the <STATEMENT> tag
            rv = ProcessStatementTagDataSchemas( pTag,
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
                ("P3PPolicy:  %s RecordDataSchemas, NS_NewP3PSimpleEnumerator for <STATEMENT> tags failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }

    NS_RELEASE( pPolicyTag );
  }

  return rv;
}


// P3P Policy: ProcessEntityTagDataSchemas
//
// Function:  Add the <ENTITY> tag DataSchema to the list of DataSchemas to process.
//
// Parms:     1. In     The <ENTITY> Tag object
//            2. In     The type of read performed
//
NS_METHOD
nsP3PPolicy::ProcessEntityTagDataSchemas( nsIP3PTag  *aTag,
                                          PRInt32     aReadType ) {

  nsresult           rv;

  nsP3PEntityTag    *pEntityTag    = nsnull;

  nsP3PDataGroupTag *pDataGroupTag = nsnull;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pEntityTag );

  if (NS_SUCCEEDED( rv )) {
    // <ENTITY> DataSchema is the P3P Base DataSchema - be sure to set it in the <DATA-GROUP>
    rv = pEntityTag->mDataGroupTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                                          (void **)&pDataGroupTag );

    if (NS_SUCCEEDED( rv )) {
      pDataGroupTag->mBase.AssignWithConversion( P3P_BASE_DATASCHEMA );

      rv = AddDataSchema( pDataGroupTag->mBase,
                          aReadType );

      NS_RELEASE( pDataGroupTag );
    }

    NS_RELEASE( pEntityTag );
  }

  return rv;
}

// P3P Policy: ProcessStatementTagDataSchemas
//
// Function:  Process all <DATA-GROUP> Tag objects within a <STATEMENT> Tag object.
//
// Parms:     1. In     The <STATEMENT> Tag object
//            2. In     The type of read performed
//
NS_METHOD
nsP3PPolicy::ProcessStatementTagDataSchemas( nsIP3PTag  *aTag,
                                             PRInt32     aReadType ) {

  nsresult                       rv;

  nsP3PStatementTag             *pStatementTag = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pStatementTag );

  if (NS_SUCCEEDED( rv )) {
    rv = NS_NewP3PSimpleEnumerator( pStatementTag->mDataGroupTags,
                                    getter_AddRefs( pEnumerator ) );

    if (NS_SUCCEEDED( rv )) {
      // Process all <DATA-GROUP> tags
      rv = pEnumerator->HasMoreElements(&bMoreTags );

      while (NS_SUCCEEDED( rv ) && bMoreTags) {
        rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

        if (NS_SUCCEEDED( rv )) {
          // Record the DataSchemas related to the <DATA-GROUP> tag
          rv = ProcessDataGroupTagDataSchema( pTag,
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
              ("P3PPolicy:  %s ProcessStatementTagDataSchemas, NS_NewP3PSimpleEnumerator for <DATA-GROUP> tags failed - %X.\n", (const char *)mcsURISpec, rv) );
    }

    NS_RELEASE( pStatementTag );
  }

  return rv;
}

// P3P Policy: ProcessDataGroupTagDataSchema
//
// Function:  Process any specified DataSchema and process all <DATA> Tag objects
//            within the <DATA-GROUP> Tag object.
//
// Parms:     1. In     The <DATA-GROUP> Tag object
//            2. In     The type of read performed
//
NS_METHOD
nsP3PPolicy::ProcessDataGroupTagDataSchema( nsIP3PTag  *aTag,
                                            PRInt32     aReadType ) {

  nsresult                       rv;

  nsP3PDataGroupTag             *pDataGroupTag = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pDataGroupTag );

  if (NS_SUCCEEDED( rv )) {

    if (pDataGroupTag->mBasePresent) {

      // The "base" attribute of the <DATA-GROUP> tag is present
      if (pDataGroupTag->mBase.Length( ) == 0) {

        // The "base" attribute is present but empty, there must be an inline DataSchema
        if (mDataSchemaTag) {
          pDataGroupTag->mBase = mURISpec;

          PR_LOG( gP3PLogModule,
                  PR_LOG_NOTICE,
                  ("P3PPolicy:  %s ProcessDataGroupTagDataSchemas, inline DataSchema referenced in the <DATA-GROUP> tag.\n", (const char *)mcsURISpec) );
        }
        else {
          rv = NS_ERROR_FAILURE; // DataSchema is relative to this document, but none supplied

          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicy:  %s ProcessDataGroupTagDataSchemas, inline DataSchema referenced in the <DATA-GROUP> tag is not present.\n", (const char *)mcsURISpec) );
        }
      }
      else {
        // The "base" attribute DataSchema URI spec is present, create a complete URI spec
        nsCOMPtr<nsIURI>  pURI;

        rv = NS_NewURI( getter_AddRefs( pURI ),
                        pDataGroupTag->mBase,
                        mURI );

        if (NS_SUCCEEDED( rv )) {
          nsXPIDLCString  xcsURISpec;

          rv = pURI->GetSpec( getter_Copies( xcsURISpec ) );

          if (NS_SUCCEEDED( rv )) {
            pDataGroupTag->mBase.AssignWithConversion((const char *)xcsURISpec );

            PR_LOG( gP3PLogModule,
                    PR_LOG_NOTICE,
                    ("P3PPolicy:  %s ProcessDataGroupTagDataSchemas, using %s DataSchema for the <DATA-GROUP> tag.\n", (const char *)mcsURISpec, (const char *)xcsURISpec) );
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PPolicy:  %s ProcessDataGroupTagDataSchemas, pURI->GetSpec failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicy:  %s ProcessDataGroupTagDataSchemas, NS_NewURI failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
    }
    else {
      pDataGroupTag->mBase.AssignWithConversion( P3P_BASE_DATASCHEMA );

      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PPolicy:  %s ProcessDataGroupTagDataSchemas, using P3P Base DataSchema for the <DATA-GROUP> tag.\n", (const char *)mcsURISpec) );
    }

    if (NS_SUCCEEDED( rv )) {
      // Add the <DATA-GROUP> DataSchema
      rv = AddDataSchema( pDataGroupTag->mBase,
                          aReadType );

      if (NS_SUCCEEDED( rv )) {
        rv = NS_NewP3PSimpleEnumerator( pDataGroupTag->mDataTags,
                                        getter_AddRefs( pEnumerator ) );

        // Process all <DATA> tags
        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreTags );

          while (NS_SUCCEEDED( rv ) && bMoreTags) {
            rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

            if (NS_SUCCEEDED( rv )) {
              // Record the DataSchema for the <DATA> tag
              rv = ProcessDataTagDataSchema( pTag,
                                             pDataGroupTag->mBase,
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
                  ("P3PPolicy:  %s ProcessDataGroupTagDataSchemas, NS_NewP3PSimpleEnumerator for <DATA> tags failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
    }

    NS_RELEASE( pDataGroupTag );
  }

  return rv;
}

// P3P Policy: ProcessDataTagDataSchema
//
// Function:  If a DataSchema is specified, add it to the list of DataSchemas to read
//
// Parms:     1. In     The <DATA> Tag object
//            2. In     The default DataSchema for the <DATA> tag
//            3. In     The type of read performed
//
NS_METHOD
nsP3PPolicy::ProcessDataTagDataSchema( nsIP3PTag  *aTag,
                                       nsString&   aDefaultDataSchemaURISpec,
                                       PRInt32     aReadType ) {

  nsresult      rv;

  nsP3PDataTag *pDataTag = nsnull;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pDataTag );

  if (NS_SUCCEEDED( rv )) {

    if (pDataTag->mRefDataSchemaURISpec.Length( ) > 0) {
      // DataSchema URI spec is present, create a complete URI spec
      nsCOMPtr<nsIURI>  pURI;

      rv = NS_NewURI( getter_AddRefs( pURI ),
                      pDataTag->mRefDataSchemaURISpec,
                      mURI );

      if (NS_SUCCEEDED( rv )) {
        nsXPIDLCString  xcsURISpec;

        rv = pURI->GetSpec( getter_Copies( xcsURISpec ) );

        if (NS_SUCCEEDED( rv )) {
          pDataTag->mRefDataSchemaURISpec.AssignWithConversion((const char *)xcsURISpec );

          rv = AddDataSchema( pDataTag->mRefDataSchemaURISpec,
                              aReadType );
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicy:  %s ProcessDataTagDataSchema, pURI->GetSpec failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicy:  %s ProcessDataTagDataSchema, NS_NewURI failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      pDataTag->mRefDataSchemaURISpec = aDefaultDataSchemaURISpec;
    }

    NS_RELEASE( pDataTag );
  }

  return rv;
}

// P3P Policy: AddDataSchema
//
// Function:  Process the DataSchema.
//
// Parms:     1. In     The DataSchema URI spec to be read
//            2. In     The type of read performed
//
NS_METHOD
nsP3PPolicy::AddDataSchema( nsString&  aDataSchemaURISpec,
                            PRInt32    aReadType ) {

  nsresult                    rv = NS_OK;

  nsStringKey                 keyDataSchemaURISpec( aDataSchemaURISpec );

  nsCAutoString               csDataSchemaURISpec;

  nsCOMPtr<nsIP3PDataSchema>  pDataSchema;

  nsCOMPtr<nsIDOMNode>        pDOMNode;


  if (!mDataSchemas.Exists(&keyDataSchemaURISpec )) {
    csDataSchemaURISpec.AssignWithConversion( aDataSchemaURISpec );
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicy:  %s AddDataSchema, adding DataSchema %s.\n", (const char *)mcsURISpec, (const char *)csDataSchemaURISpec) );

    rv = mP3PService->GetCachedDataSchema( aDataSchemaURISpec,
                                           getter_AddRefs( pDataSchema ) );

    if (NS_SUCCEEDED( rv ) && !pDataSchema) {
      // Don't have a cached version, create a new one
      rv = NS_NewP3PDataSchema( aDataSchemaURISpec,
                                getter_AddRefs( pDataSchema ) );

      if (NS_FAILED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicy:  %s AddDataSchema, error creating DataSchema %s.\n", (const char *)mcsURISpec, (const char *)csDataSchemaURISpec) );
      }
    }
    else if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s AddDataSchema, error getting DataSchema %s.\n", (const char *)mcsURISpec, (const char *)csDataSchemaURISpec) );
    }

    if (NS_SUCCEEDED( rv )) {
      // Add to the collection of DataSchemas
      mDataSchemas.Put(&keyDataSchemaURISpec,
                        pDataSchema );

      if (aDataSchemaURISpec == mURISpec) {
        // Process inline DataSchema
        rv = mDataSchemaTag->GetDOMNode( getter_AddRefs( pDOMNode ) );

        if (NS_SUCCEEDED( rv )) {
          rv = pDataSchema->InlineRead( pDOMNode,
                                        mRedirected,
                                        mExpirationTime,
                                        aReadType,
                                        mURI,
                                        this,
                                        nsnull );
        }
      }
      else {
        // Process remote DataSchema
        rv = pDataSchema->Read( aReadType,
                                mURI,
                                this,
                                nsnull );
      }

      if (NS_SUCCEEDED( rv )) {

        if (rv == P3P_PRIVACY_IN_PROGRESS) {
          rv = mDataSchemasToRead.AppendString( aDataSchemaURISpec ) ? NS_OK : NS_ERROR_FAILURE;

          if (NS_FAILED( rv )) {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PPolicy:  %s AddDataSchema, mDataSchemasToRead.AppendString failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
      }
    }
  }

  return rv;
}

// P3P Policy: SetNotValid
//
// Function:  Sets the not valid indicator and removes the Policy from cache (if present).
//
// Parms:     None
//
NS_METHOD_( void )
nsP3PPolicy::SetNotValid( ) {

#ifdef DEBUG_P3P
  { printf( "P3P:    Policy File %s not valid\n", (const char *)mcsURISpec );
  }
#endif

  PR_LOG( gP3PLogModule,
          PR_LOG_ERROR,
          ("P3PPolicy:  %s SetNotValid, Policy not valid.\n", (const char *)mcsURISpec) );

  mDocumentValid = PR_FALSE;

  if (!mInlineRead) {
    // Policy is not valid and may have been previously cached, "uncache" it
#ifdef DEBUG_P3P
    { printf( "P3P:  Uncaching Policy Object: %s\n", (const char *)mcsURISpec );
    }
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicy:  %s SetNotValid, uncaching Policy.\n", (const char *)mcsURISpec) );

    mP3PService->UncachePolicy( mURISpec,
                                this );
  }

  return;
}

// P3P Policy: CheckExpiration
//
// Function:  Determines if the Policy has expired and should not be removed from the cache.
//
// Parms:     1. Out    The indicator of whether the Policy is expired
//
NS_METHOD
nsP3PPolicy::CheckExpiration( PRBool&  aExpired ) {

  nsresult  rv = NS_OK;

  PRTime    currentTime = PR_Now( );


  aExpired = PR_FALSE;

  if (!mFirstUse) {

    if (mRedirected) {
      // Policy was redirected so indicate it has expired so it can be re-read
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PPolicy:  %s CheckExpiration, redirected Policy - indicate expired.\n", (const char *)mcsURISpec) );

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
              ("P3PPolicy:  %s CheckExpiration, Policy has expired - expiration time: %s, time now: %s.\n", (const char *)mcsURISpec, csExpirationTime, csCurrentTime) );

      aExpired = PR_TRUE;
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicy:  %s CheckExpiration, first use of Policy - indicate not expired.\n", (const char *)mcsURISpec) );
  }

  if (aExpired) {

    if (!mInlineRead) {
      // Policy has expired and may have been previously cached, "uncache" it
      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PPolicy:  %s CheckExpiration, uncaching Policy.\n", (const char *)mcsURISpec) );

      mP3PService->UncachePolicy( mURISpec,
                                  this );
    }
  }

  return rv;
}

// P3P Policy: CompareToPrefs
//
// Function:  Compare the Policy to the user's preferences.
//
// Parms:     None
//
NS_METHOD_( void )
nsP3PPolicy::CompareToPrefs( ) {

  nsresult                       rv;

  nsP3PPolicyTag                *pPolicyTag  = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  if (mContentValid && mDocumentValid) {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicy:  %s CompareToPrefs, comparing Policy to user's preferences.\n", (const char *)mcsURISpec) );

    // Reset any previous comparison information
    mFailedDataTagRefs.Reset( );
    mPrefsResult = P3P_PRIVACY_MET;

    // Get the preferences last changed time
    rv = mP3PService->GetTimePrefsLastChanged(&mPrefsTime );

    if (NS_SUCCEEDED( rv )) {
      rv = mTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                       (void **)&pPolicyTag );

      if (NS_SUCCEEDED( rv )) {
        rv = NS_NewP3PSimpleEnumerator( pPolicyTag->mStatementTags,
                                        getter_AddRefs( pEnumerator ) );

        // Process all the <STATEMENT> tags
        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreTags );

          while (NS_SUCCEEDED( rv ) && bMoreTags) {
            rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

            if (NS_SUCCEEDED( rv )) {
              // Process the information in the <STATEMENT> tag
              rv = CompareStatementTagToPrefs( pTag );
            }

            if (NS_SUCCEEDED( rv )) {
              rv = pEnumerator->HasMoreElements(&bMoreTags );
            }
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicy:  %s CompareToPrefs, NS_NewP3PSimpleEnumerator for <STATEMENT> tags failed - %X.\n", (const char *)mcsURISpec, rv) );
        }

        if (NS_SUCCEEDED( rv )) {
          // Create the RDF description of the Policy
          rv = CreateRDFPolicyDescription( pPolicyTag->mEntityTag,
                                           pPolicyTag->mDescURISpec );
        }

        NS_RELEASE( pPolicyTag );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s CompareToPrefs, mP3PService->GetTimePrefsLastChanged failed - %X.\n", (const char *)mcsURISpec, rv) );
    }

    if (NS_FAILED( rv )) {
      SetNotValid( );
    }
  }

  return;
}

// P3P Policy: CreateRDFPolicyDescription
//
// Function:  Create the RDF description of the Policy.
//
// Parms:     1. In     The <ENTITY> Tag object
//            2. In     The Policy Description URI spec
//
NS_METHOD
nsP3PPolicy::CreateRDFPolicyDescription( nsIP3PTag  *aEntityTag,
                                         nsString&   aDescURISpec ) {

  nsresult                  rv;

  char                     *csPolicy = nsnull;

  nsAutoString              sName,
                            sTitle,
                            sValue;

  nsCAutoString             csValueKey;

  nsCOMPtr<nsIRDFResource>  pResource;


  csPolicy = PR_smprintf( "%X", this );

  if (csPolicy) {
    // Create a unique "about" attribute
    mRDFAbout.AssignWithConversion( "#__" );
    mRDFAbout += mURISpec;
    mRDFAbout.AppendWithConversion( " @ " );
    mRDFAbout.AppendWithConversion( csPolicy );
    PR_smprintf_free( csPolicy );

    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicy:  %s CreateRDFPolicyDescription, creating the RDF description.\n", (const char *)mcsURISpec) );

    // Create the RDF datasource
    mRDFDataSource = do_CreateInstance( kRDFInMemoryDataSourceCID,
                                       &rv );

    if (NS_SUCCEEDED( rv )) {
      sName = mRDFAbout;
      sTitle.Truncate( );
      sValue.Truncate( );

      // Create the Policy description resource
      rv = AddRDFEntry( sName,
                        sTitle,
                        sValue,
                        nsnull,
                        nsnull,
                        getter_AddRefs( mPolicyDescription ) );

      if (NS_SUCCEEDED( rv )) {
        sName.Truncate( );
        sTitle.Truncate( );
        sValue.Truncate( );

        // Create the Policy container resource
        rv = AddRDFEntry( sName,
                          sTitle,
                          sValue,
                          nsnull,
                          nsnull,
                          getter_AddRefs( mPolicyContainer ) );

        if (NS_SUCCEEDED( rv )) {
          // Make the Policy container resource a sequential container
          rv = mRDFContainerUtils->MakeSeq( mRDFDataSource,
                                            mPolicyContainer,
                                            nsnull );

          if (NS_SUCCEEDED( rv )) {

            if (aEntityTag) {
              // <ENTITY> tag is present, add the entity information
              sName = mRDFAbout;
              sName.AppendWithConversion( "__Entity" );
              sValue.Truncate( );
              rv = mP3PService->GetLocaleString( "EntityInformation",
                                                 sTitle );

              if (NS_SUCCEEDED( rv )) {
                // Create an entity resource and add it to the Policy container
                rv = AddRDFEntry( sName,
                                  sTitle,
                                  sValue,
                                  mRDFResourceP3PEntity,
                                  mPolicyContainer,
                                  getter_AddRefs( pResource ) );

                if (NS_SUCCEEDED( rv )) {
                  // Add the <ENTITY> information to the datasource
                  rv = CreateRDFEntityDescription( aEntityTag );
                }
                else {
                  PR_LOG( gP3PLogModule,
                          PR_LOG_ERROR,
                          ("P3PPolicy:  %s CreateRDFPolicyDescription, creation of the entity information resource failed - %X.\n", (const char *)mcsURISpec, rv) );
                }
              }
              else {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PPolicy:  %s CreateRDFPolicyDescription, mP3PService->GetLocaleString \"EntityInformation\" failed - %X.\n", (const char *)mcsURISpec, rv) );
              }
            }

            if (NS_SUCCEEDED( rv )) {
              sName.Truncate( );
              sValue = mURISpec;
              rv = mP3PService->GetLocaleString( "PolicyURI",
                                                 sTitle );

              if (NS_SUCCEEDED( rv )) {
                // Create the Policy URI resource and add it to the Policy container
                rv = AddRDFEntry( sName,
                                  sTitle,
                                  sValue,
                                  mRDFResourceP3PPolicyURISpec,
                                  mPolicyContainer,
                                  getter_AddRefs( pResource ) );

                if (NS_FAILED( rv )) {
                  PR_LOG( gP3PLogModule,
                          PR_LOG_ERROR,
                          ("P3PPolicy:  %s CreateRDFPolicyDescription, creation of the Policy uri resource failed - %X.\n", (const char *)mcsURISpec, rv) );
                }
              }
              else {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PPolicy:  %s CreateRDFPolicyDescription, mP3PService->GetLocaleString \"PolicyURI\" failed - %X.\n", (const char *)mcsURISpec, rv) );
              }
            }

            if (NS_SUCCEEDED( rv )) {
              sName.Truncate( );
              sValue = aDescURISpec;
              rv = mP3PService->GetLocaleString( "PolicyDescURI",
                                                 sTitle );

              if (NS_SUCCEEDED( rv )) {
                // Create the Policy description URI resource and add it to the Policy container
                rv = AddRDFEntry( sName,
                                  sTitle,
                                  sValue,
                                  mRDFResourceP3PPolicyDescriptionURISpec,
                                  mPolicyContainer,
                                  getter_AddRefs( pResource ) );

                if (NS_FAILED( rv )) {
                  PR_LOG( gP3PLogModule,
                          PR_LOG_ERROR,
                          ("P3PPolicy:  %s CreateRDFPolicyDescription, creation of the Policy description uri resource failed - %X.\n", (const char *)mcsURISpec, rv) );
                }
              }
              else {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PPolicy:  %s CreateRDFPolicyDescription, mP3PService->GetLocaleString \"PolicyDescURI\" failed - %X.\n", (const char *)mcsURISpec, rv) );
              }
            }

            if (NS_SUCCEEDED( rv )) {
              sName.Truncate( );
              rv = mP3PService->GetLocaleString( "PolicyResult",
                                                 sTitle );

              if (NS_SUCCEEDED( rv )) {
                switch (mPrefsResult) {
                  case P3P_PRIVACY_NOT_MET:
                    csValueKey = "PrivacyNotMet";
                    break;

                  case P3P_PRIVACY_MET:
                    csValueKey = "PrivacyMet";
                    break;

                  default:
                    csValueKey = "PrivacyUndetermined";
                    break;
                }

                rv = mP3PService->GetLocaleString( csValueKey,
                                                   sValue );

                if (NS_SUCCEEDED( rv )) {
                  // Create the Policy/preferences comparison result resource and add it to the Policy container
                  rv = AddRDFEntry( sName,
                                    sTitle,
                                    sValue,
                                    mRDFResourceP3PResult,
                                    mPolicyContainer,
                                    getter_AddRefs( pResource ) );

                  if (NS_FAILED( rv )) {
                    PR_LOG( gP3PLogModule,
                            PR_LOG_ERROR,
                            ("P3PPolicy:  %s CreateRDFPolicyDescription, creation of the result resource failed - %X.\n", (const char *)mcsURISpec, rv) );
                  }
                }
                else {
                  PR_LOG( gP3PLogModule,
                          PR_LOG_ERROR,
                          ("P3PPolicy:  %s CreateRDFPolicyDescription, mP3PService->GetLocaleString \"%s\" failed - %X.\n", (const char *)mcsURISpec, (const char *)csValueKey, rv) );
                }
              }
              else {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PPolicy:  %s CreateRDFPolicyDescription, mP3PService->GetLocaleString \"PolicyResult\" failed - %X.\n", (const char *)mcsURISpec, rv) );
              }
            }

            if (NS_SUCCEEDED( rv ) && (mFailedDataTagRefs.Count( ) > 0)) {
              // Some <DATA> tags failed the preference comparison
              sName = mRDFAbout;
              sName.AppendWithConversion( "__DataTags" );
              sValue.Truncate( );
              rv = mP3PService->GetLocaleString( "FailedData",
                                                 sTitle );

              if (NS_SUCCEEDED( rv )) {
                // Create the Policy data tags resource and add it to the Policy container
                rv = AddRDFEntry( sName,
                                  sTitle,
                                  sValue,
                                  mRDFResourceP3PDataTags,
                                  mPolicyContainer,
                                  getter_AddRefs( pResource ) );

                if (NS_SUCCEEDED( rv )) {
                  // Add the failed <DATA>/preference combination to the datasource
                  rv = CreateRDFDataTagsDescription( );
                }
                else {
                  PR_LOG( gP3PLogModule,
                          PR_LOG_ERROR,
                          ("P3PPolicy:  %s CreateRDFPolicyDescription, creation of the data tags resource failed - %X.\n", (const char *)mcsURISpec, rv) );
                }
              }
              else {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PPolicy:  %s CreateRDFPolicyDescription, mP3PService->GetLocaleString \"FailedData\" failed - %X.\n", (const char *)mcsURISpec, rv) );
              }
            }

            if (NS_SUCCEEDED( rv )) {
              // Assert the Policy information into the datasource
              rv = mRDFDataSource->Assert( mPolicyDescription,
                                           mRDFResourceP3PEntries,
                                           mPolicyContainer,
                                           PR_TRUE );

              if (NS_FAILED( rv )) {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PPolicy:  %s CreateRDFPolicyDescription, mRDFDataSource->Assert failed - %X.\n", (const char *)mcsURISpec, rv) );
              }
            }
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PPolicy:  %s CreateRDFPolicyDescription, mRDFContainerUtils->MakeSeq failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicy:  %s CreateRDFPolicyDescription, creation of the Policy container resource failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicy:  %s CreateRDFPolicyDescription, creation of the Policy description resource failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s CreateRDFPolicyDescription, do_CreateInstance for the RDF datasource failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }
  else {
    rv = NS_ERROR_OUT_OF_MEMORY;

    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicy:  %s CreateRDFPolicyDescription, PR_smprintf failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P Policy: CreateRDFEntityDescription
//
// Function:  Creates the RDF description of the <ENTITY> Tag objects.
//
// Parms:     1. In     The <ENTITY> Tag object
//
NS_METHOD
nsP3PPolicy::CreateRDFEntityDescription( nsIP3PTag  *aEntityTag ) {

  nsresult                       rv = NS_OK;

  nsAutoString                   sName,
                                 sTitle,
                                 sValue;

  nsP3PEntityTag                *pEntityTag    = nsnull;

  nsP3PDataGroupTag             *pDataGroupTag = nsnull;

  nsCOMPtr<nsIP3PDataSchema>     pDataSchema;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsP3PDataTag                  *pDataTag      = nsnull;

  PRBool                         bMoreTags;

  nsCOMPtr<nsIP3PDataStruct>     pDataStruct;

  nsCOMPtr<nsIRDFResource>       pResource;


  // Create a "unique" about attribute
  sName = mRDFAbout;
  sName.AppendWithConversion( "__Entity" );
  sTitle.Truncate( );
  sValue.Truncate( );

  // Create the entity description resource
  rv = AddRDFEntry( sName,
                    sTitle,
                    sValue,
                    nsnull,
                    nsnull,
                    getter_AddRefs( mEntityDescription ) );

  if (NS_SUCCEEDED( rv )) {
    sName.Truncate( );
    sTitle.Truncate( );
    sValue.Truncate( );

    // Create the entity container resource
    rv = AddRDFEntry( sName,
                      sTitle,
                      sValue,
                      nsnull,
                      nsnull,
                      getter_AddRefs( mEntityContainer ) );

    if (NS_SUCCEEDED( rv )) {
      // Make the entity container resource a sequential container
      rv = mRDFContainerUtils->MakeSeq( mRDFDataSource,
                                        mEntityContainer,
                                        nsnull );

      if (NS_SUCCEEDED( rv )) {
        rv = aEntityTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                               (void **)&pEntityTag );

        if (NS_SUCCEEDED( rv )) {
          rv = pEntityTag->mDataGroupTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                                                (void **)&pDataGroupTag );

          if (NS_SUCCEEDED( rv )) {
            nsCOMPtr<nsISupports> pEntry;
            nsStringKey           keyRefDataSchemaURISpec( pDataGroupTag->mBase );

            // Get the DataSchema for the <ENTITY> tag (should be the P3P Base DataSchema)
            pEntry      = getter_AddRefs( mDataSchemas.Get(&keyRefDataSchemaURISpec ) );
            pDataSchema = do_QueryInterface( pEntry,
                                            &rv );

            if (NS_SUCCEEDED( rv )) {
              rv = NS_NewP3PSimpleEnumerator( pDataGroupTag->mDataTags,
                                              getter_AddRefs( pEnumerator ) );

              // Process each <DATA> tag within the <ENTITY> tag
              if (NS_SUCCEEDED( rv )) {
                rv = pEnumerator->HasMoreElements(&bMoreTags );

                while (NS_SUCCEEDED( rv ) && bMoreTags) {
                  rv = pEnumerator->GetNext((nsISupports **)&pDataTag );

                  if (NS_SUCCEEDED( rv )) {
#ifdef DEBUG_P3P
                    { nsCAutoString  csBuf;

                      csBuf.AssignWithConversion( pDataTag->mRefDataStructName );
                      printf( "P3P:  Processing <ENTITY> data element for RDF description: %s\n", (const char *)csBuf );
                    }
#endif

                    // Find the DataStruct for the <DATA> tag
                    rv = pDataSchema->FindDataStruct( pDataTag->mRefDataStructName,
                                                      getter_AddRefs( pDataStruct ) );

                    if (NS_SUCCEEDED( rv )) {
                      sName.Truncate( );
                      pDataStruct->GetShortDescription( sTitle );
                      if (sTitle.Length( ) == 0) {
                        sTitle = pDataTag->mRefDataStructName;
                      }
                      pDataTag->GetText( sValue );

                      // Create an entity information resource and add it to the Entity container
                      rv = AddRDFEntry( sName,
                                        sTitle,
                                        sValue,
                                        mRDFResourceP3PEntityInformation,
                                        mEntityContainer,
                                        getter_AddRefs( pResource ) );

                      if (NS_FAILED( rv )) {
                        nsCAutoString  csEntityInfo;

                        csEntityInfo.AssignWithConversion( sTitle );
                        PR_LOG( gP3PLogModule,
                                PR_LOG_ERROR,
                                ("P3PPolicy:  %s CreateRDFEntityDescription, creation of the entity information resource %s failed - %X.\n", (const char *)mcsURISpec, (const char *)csEntityInfo, rv) );
                      }
                    }
                    else {
                      nsCAutoString  csDataStructName;

                      csDataStructName.AssignWithConversion( pDataTag->mRefDataStructName );
                      PR_LOG( gP3PLogModule,
                              PR_LOG_ERROR,
                              ("P3PPolicy:  %s CreateRDFEntityDescription, pDataSchema->FindDataStruct for %s failed - %X.\n", (const char *)mcsURISpec, (const char *)csDataStructName, rv) );
                    }
                  }

                  NS_RELEASE( pDataTag );

                  if (NS_SUCCEEDED( rv )) {
                    rv = pEnumerator->HasMoreElements(&bMoreTags );
                  }
                }
              }
              else {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PPolicy:  %s CreateRDFEntityDescription, NS_NewP3PSimpleEnumerator for <DATA> tags failed - %X.\n", (const char *)mcsURISpec, rv) );
              }
            }
            else {
              nsCAutoString  csDataSchemaURISpec;

              csDataSchemaURISpec.AssignWithConversion( pDataGroupTag->mBase );
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PPolicy:  %s CreateRDFEntityDescription, unable to obtain DataSchema - %s.\n", (const char *)mcsURISpec, (const char *)csDataSchemaURISpec) );
            }

            NS_RELEASE( pDataGroupTag );
          }

          NS_RELEASE( pEntityTag );
        }

        if (NS_SUCCEEDED( rv )) {
          // Assert the entity information into the datasource
          rv = mRDFDataSource->Assert( mEntityDescription,
                                       mRDFResourceP3PEntries,
                                       mEntityContainer,
                                       PR_TRUE );

          if (NS_FAILED( rv )) {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PPolicy:  %s CreateRDFEntityDescription, mRDFDataSource->Assert failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicy:  %s CreateRDFEntityDescription, mRDFContainerUtils->MakeSeq failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s CreateRDFEntityDescription, creation of the Entity container resource failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicy:  %s CreateRDFEntityDescription, creation of the Entity description resource failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P Policy: CreateRDFDataTagsDescription
//
// Function:  Creates the RDF description of all "failed" <DATA> Tag objects.
//
// Parms:     None
//
NS_METHOD
nsP3PPolicy::CreateRDFDataTagsDescription( ) {

  nsresult      rv = NS_OK;

  nsAutoString  sName,
                sTitle,
                sValue;


  sName = mRDFAbout;
  sName.AppendWithConversion( "__DataTags" );
  sTitle.Truncate( );
  sValue.Truncate( );

  // Create the data tags description resource
  rv = AddRDFEntry( sName,
                    sTitle,
                    sValue,
                    nsnull,
                    nsnull,
                    getter_AddRefs( mDataTagsDescription ) );

  if (NS_SUCCEEDED( rv )) {
    sName.Truncate( );
    sTitle.Truncate( );
    sValue.Truncate( );

    // Create the data tags container resource
    rv = AddRDFEntry( sName,
                      sTitle,
                      sValue,
                      nsnull,
                      nsnull,
                      getter_AddRefs( mDataTagsContainer ) );

    if (NS_SUCCEEDED( rv )) {
      // Make the data tags container resource a sequential container
      rv = mRDFContainerUtils->MakeSeq( mRDFDataSource,
                                        mDataTagsContainer,
                                        nsnull );

      if (NS_SUCCEEDED( rv )) {
        // Enumerate the failed <DATA> tags to add them to the datasource and data tag container
        mFailedDataTagRefs.Enumerate( AddRDFDataTagEnumerate,
                                      this );

        // Assert the data tag information into the datasource
        rv = mRDFDataSource->Assert( mDataTagsDescription,
                                     mRDFResourceP3PEntries,
                                     mDataTagsContainer,
                                     PR_TRUE );

        if (NS_FAILED( rv )) {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicy:  %s CreateRDFDataTagsDescription, mRDFDataSource->Assert failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicy:  %s CreateRDFDataTagsDescription, mRDFContainerUtils->MakeSeq failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s CreateRDFDataTagsDescription, creation of the Data Tags container resource failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicy:  %s CreateRDFDataTagsDescription, creation of the Data Tags description resource failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P Policy: AddRDFDataTag
//
// Function:  Creates and adds an RDF description of the failed <DATA> tag object.
//
// Parms:     1. In     The <DATA> Tag object name
//            2. In     The failed preference combinations
//
NS_METHOD
nsP3PPolicy::AddRDFDataTag( nsString&       aDescription,
                            nsStringArray  *aFailedPrefsCombinations ) {

  nsresult                  rv;

  nsAutoString              sName,
                            sTitle,
                            sValue;

  nsCOMPtr<nsIRDFResource>  pResource;


#ifdef DEBUG_P3P
  { nsCAutoString  csName;

    csName.AssignWithConversion( aDescription );
    printf( "P3P:    Privacy failed for: %s\n", (const char *)csName );
  }
#endif

  // Create a unique "about" attribute for the data tag information
  sName  = mRDFAbout;
  sName.AppendWithConversion( "__DataTags__DataTag_" );
  sName += aDescription;
  sTitle = aDescription;
  sValue.Truncate( );

  // Create the data tag resource and add it to the data tag container
  rv = AddRDFEntry( sName,
                    sTitle,
                    sValue,
                    mRDFResourceP3PDataTag,
                    mDataTagsContainer,
                    getter_AddRefs( pResource ) );

  if (NS_SUCCEEDED( rv )) {
    // Add the failed preference combinations to the datasource
    rv = CreateRDFDataCombinationsDescription( sName,
                                               aFailedPrefsCombinations );

    if (NS_FAILED( rv )) {
      nsCAutoString  csDataTag;

      csDataTag.AssignWithConversion( aDescription );
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s AddRDFDataTag, creation of the Data Tag combinations resources for %s failed - %X.\n", (const char *)mcsURISpec, (const char *)csDataTag, rv) );
    }
  }
  else {
    nsCAutoString  csDataTag;

    csDataTag.AssignWithConversion( aDescription );
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicy:  %s AddRDFDataTag, creation of the Data Tag resource %s failed - %X.\n", (const char *)mcsURISpec, (const char *)csDataTag, rv) );
  }

  return rv;
}

// P3P Policy: Create the RDF description of the data combinations
//
// Function:  Creates and adds an RDF description for all preference combination.
//
// Parms:     1. In     The RDF resource name to be used
//            2. In     The failed preference combinations
//
NS_METHOD
nsP3PPolicy::CreateRDFDataCombinationsDescription( nsString&       aName,
                                                   nsStringArray  *aFailedPrefsCombinations ) {

  nsresult                  rv;

  nsAutoString              sName,
                            sTitle,
                            sValue;

  nsAutoString              sPrefsCombination;

  nsCOMPtr<nsIRDFResource>  pResource;


  sName = aName;
  sTitle.Truncate( );
  sValue.Truncate( );

  // Create the data combination description resource
  rv = AddRDFEntry( sName,
                    sTitle,
                    sValue,
                    nsnull,
                    nsnull,
                    getter_AddRefs( mDataCombinationsDescription ) );

  if (NS_SUCCEEDED( rv )) {
    sName.Truncate( );
    sTitle.Truncate( );
    sValue.Truncate( );

    // Create the data combination container resource
    rv = AddRDFEntry( sName,
                      sTitle,
                      sValue,
                      nsnull,
                      nsnull,
                      getter_AddRefs( mDataCombinationsContainer ) );

    if (NS_SUCCEEDED( rv )) {
      // Make the data combination container resource a sequential container
      rv = mRDFContainerUtils->MakeSeq( mRDFDataSource,
                                        mDataCombinationsContainer,
                                        nsnull );

      if (NS_SUCCEEDED( rv )) {
        sName.Truncate( );
        rv = mP3PService->GetLocaleString( "PreferenceCombination",
                                           sTitle );

        for (PRInt32 iCount = 0;
               NS_SUCCEEDED( rv ) && (iCount < aFailedPrefsCombinations->Count( ));
                 iCount++) {
          aFailedPrefsCombinations->StringAt( iCount,
                                              sValue );
#ifdef DEBUG_P3P
          { nsCAutoString  csCombo;

            csCombo.AssignWithConversion( sValue );
            printf( "          Preference combination: %s\n", (const char *)csCombo );
          }
#endif

          // Create a data combination resource and add it to the data combination container
          rv = AddRDFEntry( sName,
                            sTitle,
                            sValue,
                            mRDFResourceP3PDataCombination,
                            mDataCombinationsContainer,
                            getter_AddRefs( pResource ) );

          if (NS_FAILED( rv )) {
            nsCAutoString  csPrefs;

            csPrefs.AssignWithConversion( sValue );
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PPolicy:  %s CreateRDFDataCombinationsDescription, creation of the Data Combination resource %s failed - %X.\n", (const char *)mcsURISpec, (const char *)csPrefs, rv) );
          }
        }

        if (NS_SUCCEEDED( rv )) {
          // Assert the data combination information into the datasource
          rv = mRDFDataSource->Assert( mDataCombinationsDescription,
                                       mRDFResourceP3PEntries,
                                       mDataCombinationsContainer,
                                       PR_TRUE );

          if (NS_FAILED( rv )) {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PPolicy:  %s CreateRDFDataCombinationsDescription, mRDFDataSource->Assert failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicy:  %s CreateRDFDataCombinationsDescription, mRDFContainerUtils->MakeSeq failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s CreateRDFDataCombinationsDescription, creation of the Data Combinations container resource failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicy:  %s CreateRDFDataCombinationsDescription, creation of the Data Combinations description resource failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P Policy: AddRDFEntry
//
// Function:  Creates an RDF resource and adds it to the RDF data source
//
// Parms:     1. In     The RDF resource name to be used
//            2. In     The NC:Title value to be used
//            3. In     The NC:Value value to be used
//            4. In     The RDF resource type to assign to the resource
//            5. In     The RDF container to add the resource to
//            6. Out    The RDF resource
//
NS_METHOD
nsP3PPolicy::AddRDFEntry( nsString&        aName,
                          nsString&        aTitle,
                          nsString&        aValue,
                          nsIRDFResource  *aResourceType,
                          nsIRDFResource  *aContainer,
                          nsIRDFResource **aResource ) {

  nsresult                   rv;

  nsCOMPtr<nsIRDFResource>   pResource;

  nsCOMPtr<nsIRDFLiteral>    pLiteral;

  nsCOMPtr<nsIRDFContainer>  pContainer;


  NS_ENSURE_ARG_POINTER( aResource );

  *aResource = nsnull;

  if (aName.Length( ) > 0) {
    // Create a named resource
    rv = mRDFService->GetUnicodeResource( aName.GetUnicode( ),
                                          aResource );
  }
  else {
    // Create an anonymous resource
    rv = mRDFService->GetAnonymousResource( aResource );
  }

  if (NS_SUCCEEDED( rv )) {
    
    if (aResourceType) {
      // Assert the resource type information into the datasource
      rv = mRDFDataSource->Assert( *aResource,
                                   mRDFResourceRDFType,
                                   aResourceType,
                                   PR_TRUE );

      if (NS_FAILED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicy:  %s AddRDFEntry, mRDFDataSource->Assert for RDFType failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }

    if (NS_SUCCEEDED( rv ) && aTitle.Length( )) {
      // Create a literal resource to represent the attribute value
      rv = mRDFService->GetLiteral( aTitle.GetUnicode( ),
                                    getter_AddRefs( pLiteral ) );

      if (NS_SUCCEEDED( rv )) {
        // Assert the attribute into the datasource
        rv = mRDFDataSource->Assert( *aResource,
                                     mRDFResourceP3PTitle,
                                     pLiteral,
                                     PR_TRUE );

        if (NS_FAILED( rv )) {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicy:  %s AddRDFEntry, mRDFDataSource->Assert for P3PTitle failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicy:  %s AddRDFEntry, mRDFDataService->GetLiteral for P3PTitle failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }

    if (NS_SUCCEEDED( rv ) && aValue.Length( )) {
      // Create a literal resource to represent the attribute value
      rv = mRDFService->GetLiteral( aValue.GetUnicode( ),
                                    getter_AddRefs( pLiteral ) );

      if (NS_SUCCEEDED( rv )) {
        // Assert the attribute into the datasource
        rv = mRDFDataSource->Assert( *aResource,
                                     mRDFResourceP3PValue,
                                     pLiteral,
                                     PR_TRUE );

        if (NS_FAILED( rv )) {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicy:  %s AddRDFEntry, mRDFDataSource->Assert for P3PValue failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicy:  %s AddRDFEntry, mRDFDataService->GetLiteral for P3PValue failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }

    if (NS_SUCCEEDED( rv ) && aContainer) {
      // Add the resource to the container
      pContainer = do_CreateInstance( kRDFContainerCID,
                                     &rv );

      if (NS_SUCCEEDED( rv )) {
        rv = pContainer->Init( mRDFDataSource,
                               aContainer );

        if (NS_SUCCEEDED( rv )) {
          rv = pContainer->AppendElement( *aResource );

          if (NS_FAILED( rv )) {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PPolicy:  %s AddRDFEntry, pContainer->AppendElement failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicy:  %s AddRDFEntry, pContainer->Init failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPolicy:  %s AddRDFEntry, do_CreateInstance for RDF container failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicy:  %s AddRDFEntry, creation of RDF resource failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P Policy: CompareStatementTagToPrefs
//
// Function:  Process all <DATA-GROUP> Tag objects within a <STATEMENT> Tag object.
//
// Parms:     1. In     The <STATEMENT> Tag object
//
NS_METHOD
nsP3PPolicy::CompareStatementTagToPrefs( nsIP3PTag  *aTag ) {

  nsresult                       rv;

  nsP3PStatementTag             *pStatementTag = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pStatementTag );

  if (NS_SUCCEEDED( rv )) {
    rv = NS_NewP3PSimpleEnumerator( pStatementTag->mDataGroupTags,
                                    getter_AddRefs( pEnumerator ) );

    if (NS_SUCCEEDED( rv )) {
      // Process all of the <DATA-GROUP> tags within the <STATEMENT> tag
      rv = pEnumerator->HasMoreElements(&bMoreTags );

      while (NS_SUCCEEDED( rv ) && bMoreTags) {
        rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

        if (NS_SUCCEEDED( rv )) {
          // Process the <DATA-GROUP> tag
          rv = CompareDataGroupTagToPrefs( pTag,
                                           pStatementTag->mPurposeTag,
                                           pStatementTag->mRecipientTag );
        }

        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreTags );
        }
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s CompareStatementTagToPrefs, NS_NewP3PSimpleEnumerator for <DATA-GROUP> tags failed - %X.\n", (const char *)mcsURISpec, rv) );
    }

    NS_RELEASE( pStatementTag );
  }

  return rv;
}

// P3P Policy: CompareDataGroupTagToPrefs
//
// Function:  Process all <DATA> Tag objects within a <DATA-GROUP> Tag object.
//
// Parms:     1. In     The <DATA-GROUP> Tag object
//            2. In     The <PURPOSE> Tag object from the <STATEMENT> Tag object
//            3. In     The <RECIPIENT> Tag object from the <STATEMENT> Tag object
//
NS_METHOD
nsP3PPolicy::CompareDataGroupTagToPrefs( nsIP3PTag  *aTag,
                                         nsIP3PTag  *aPurposeTag,
                                         nsIP3PTag  *aRecipientTag ) {

  nsresult                       rv;

  nsP3PDataGroupTag             *pDataGroupTag = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pDataGroupTag );

  if (NS_SUCCEEDED( rv )) {

    rv = NS_NewP3PSimpleEnumerator( pDataGroupTag->mDataTags,
                                    getter_AddRefs( pEnumerator ) );

    if (NS_SUCCEEDED( rv )) {
      // Process all of the <DATA> tags within the <DATA-GROUP> tag
      rv = pEnumerator->HasMoreElements(&bMoreTags );

      while (NS_SUCCEEDED( rv ) && bMoreTags) {
        rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

        if (NS_SUCCEEDED( rv )) {
          // Process the <DATA> tag
          rv = CompareDataTagToPrefs( pTag,
                                      aPurposeTag,
                                      aRecipientTag );
        }

        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreTags );
        }
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s CompareDataGroupTagToPrefs, NS_NewP3PSimpleEnumerator for <DATA> tags failed - %X.\n", (const char *)mcsURISpec, rv) );
    }

    NS_RELEASE( pDataGroupTag );
  }

  return rv;
}

// P3P Policy: CompareDataTagToPrefs
//
// Function:  Compare the <DATA>, <PURPOSE> and <RECIPIENT> Tag objects to the user's
//            preferences.
//
// Parms:     1. In     The <DATA> Tag object
//            2. In     The <PURPOSE> Tag object from the <STATEMENT> Tag object
//            3. In     The <RECIPIENT> Tag object from the <STATEMENT> Tag object
//
NS_METHOD
nsP3PPolicy::CompareDataTagToPrefs( nsIP3PTag  *aTag,
                                    nsIP3PTag  *aPurposeTag,
                                    nsIP3PTag  *aRecipientTag ) {

  nsresult      rv;

  nsP3PDataTag *pDataTag = nsnull;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pDataTag );

  if (NS_SUCCEEDED( rv )) {
    // Compare the purpose/recipient/category combinations
    rv = CheckPref( pDataTag->mRefDataSchemaURISpec,
                    pDataTag->mRefDataStructName,
                    aPurposeTag,
                    aRecipientTag,
                    pDataTag->mCategoriesTag );

    NS_RELEASE( pDataTag );
  }

  return rv;
}

// P3P Policy: CheckPref
//
// Function:  Compare all the <PURPOSE>, <RECIPIENT> and <CATEGORIES> values combination
//            to the user's preferences.
//
// Parms:     1. In     The DataSchema URI spec of the <DATA> Tag object
//            2. In     The DataStruct name of the <DATA> Tag object
//            3. In     The <PURPOSE> Tag object from the <STATEMENT> Tag object
//            4. In     The <RECIPIENT> Tag object from the <STATEMENT> Tag object
//            5. In     The <CATEGORIES> Tag object from the <DATA> Tag object
//
NS_METHOD
nsP3PPolicy::CheckPref( nsString&   aRefDataSchemaURISpec,
                        nsString&   aRefDataStructName,
                        nsIP3PTag  *aPurposeTag,
                        nsIP3PTag  *aRecipientTag,
                        nsIP3PTag  *aCategoriesTag ) {

  nsresult                       rv;

  nsP3PPurposeTag               *pPurposeTag   = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;

  nsAutoString                   sPurposeValue;


  rv = aPurposeTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                          (void **)&pPurposeTag );

  if (NS_SUCCEEDED( rv )) {
    rv = NS_NewP3PSimpleEnumerator( pPurposeTag->mPurposeValueTags,
                                    getter_AddRefs( pEnumerator ) );

    if (NS_SUCCEEDED( rv )) {
      // Process all purpose value tags within the <PURPOSE> tag
      rv = pEnumerator->HasMoreElements(&bMoreTags );

      while (NS_SUCCEEDED( rv ) && bMoreTags) {
        rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

        if (NS_SUCCEEDED( rv )) {
          pTag->GetName( sPurposeValue );

          // Compare the purpose/recipient/category combinations
          rv = CheckPref( aRefDataSchemaURISpec,
                          aRefDataStructName,
                          sPurposeValue,
                          aRecipientTag,
                          aCategoriesTag );
        }

        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreTags );
        }
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s CheckPref, NS_NewP3PSimpleEnumerator for purpose value tags failed - %X.\n", (const char *)mcsURISpec, rv) );
    }

    NS_RELEASE( pPurposeTag );
  }

  return rv;
}

// P3P Policy: CheckPref
//
// Function:  Compare all the <PURPOSE>, <RECIPIENT> and <CATEGORIES> values combination
//            to the user's preferences.
//
// Parms:     1. In     The DataSchema URI spec of the <DATA> Tag object
//            2. In     The DataStruct name of the <DATA> Tag object
//            3. In     The purpose value Tag object from the <PURPOSE> Tag object
//            4. In     The <RECIPIENT> Tag object from the <STATEMENT> Tag object
//            5. In     The <CATEGORIES> Tag object from the <DATA> Tag object
//
NS_METHOD
nsP3PPolicy::CheckPref( nsString&   aRefDataSchemaURISpec,
                        nsString&   aRefDataStructName,
                        nsString&   aPurposeValue,
                        nsIP3PTag  *aRecipientTag,
                        nsIP3PTag  *aCategoriesTag ) {

  nsresult                       rv;

  nsP3PRecipientTag             *pRecipientTag   = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;

  nsAutoString                   sRecipientValue;


  rv = aRecipientTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                            (void **)&pRecipientTag );

  if (NS_SUCCEEDED( rv )) {
    rv = NS_NewP3PSimpleEnumerator( pRecipientTag->mRecipientValueTags,
                                    getter_AddRefs( pEnumerator ) );

    if (NS_SUCCEEDED( rv )) {
      // Process all recipient value tags with the <RECIPIENT> tag
      rv = pEnumerator->HasMoreElements(&bMoreTags );

      while (NS_SUCCEEDED( rv ) && bMoreTags) {
        rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

        if (NS_SUCCEEDED( rv )) {
          pTag->GetName( sRecipientValue );

          // Compare the purpose/recipient/category combinations
          rv = CheckPref( aRefDataSchemaURISpec,
                          aRefDataStructName,
                          aPurposeValue,
                          sRecipientValue,
                          aCategoriesTag );
        }

        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreTags );
        }
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s CheckPref, NS_NewP3PSimpleEnumerator for recipient value tags failed - %X.\n", (const char *)mcsURISpec, rv) );
    }

    NS_RELEASE( pRecipientTag );
  }

  return rv;
}

// P3P Policy: CheckPref
//
// Function:  Compare all the <PURPOSE>, <RECIPIENT> and <CATEGORIES> values combination
//            to the user's preferences.
//
// Parms:     1. In     The DataSchema URI spec of the <DATA> Tag object
//            2. In     The DataStruct name of the <DATA> Tag object
//            3. In     The purpose value Tag object from the <PURPOSE> Tag object
//            4. In     The recipient value Tag object from the <RECIPIENT> Tag object
//            5. In     The <CATEGORIES> Tag object from the <DATA> Tag object
//
NS_METHOD
nsP3PPolicy::CheckPref( nsString&   aRefDataSchemaURISpec,
                        nsString&   aRefDataStructName,
                        nsString&   aPurposeValue,
                        nsString&   aRecipientValue,
                        nsIP3PTag  *aCategoriesTag ) {

  nsresult                       rv;

  nsCOMPtr<nsISupports>          pEntry;

  nsStringKey                    keyRefDataSchemaURISpec( aRefDataSchemaURISpec );

  nsCOMPtr<nsIP3PDataSchema>     pDataSchema;

  nsCOMPtr<nsIP3PDataStruct>     pDataStruct;

  nsAutoString                   sShortDescription;

  nsCOMPtr<nsISupportsArray>     pCategoryValueTags;

  nsP3PCategoriesTag            *pCategoriesTag     = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;

  nsAutoString                   sCategoryValue;


#ifdef DEBUG_P3P
  { nsCAutoString  csBuf;

    csBuf.AssignWithConversion( aRefDataStructName );
    printf( "P3P:  Processing data element: %s\n", (const char *)csBuf );
  }
#endif

  // Get the DataSchema for the data struct
  pEntry      = getter_AddRefs( mDataSchemas.Get(&keyRefDataSchemaURISpec ) );
  pDataSchema = do_QueryInterface( pEntry,
                                  &rv );

  if (NS_SUCCEEDED( rv )) {
    // Get the data struct
    rv = pDataSchema->FindDataStruct( aRefDataStructName,
                                      getter_AddRefs( pDataStruct ) );

    if (NS_SUCCEEDED( rv )) {
      // Get the data struct description
      pDataStruct->GetShortDescription( sShortDescription );

      // Get the data struct category values, these override any specified in the <DATA> tag
      rv = pDataStruct->GetCategoryValues( getter_AddRefs( pCategoryValueTags ) );

      if (NS_SUCCEEDED( rv )) {
        PRUint32  uiCategories;

        rv = pCategoryValueTags->Count(&uiCategories );

        if (NS_SUCCEEDED( rv )) {

          if (uiCategories == 0) {
            // There are no category values associated with the data struct
            if (aCategoriesTag) {
              // Use the category values associated with the <DATA> tag
              rv = aCategoriesTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                                         (void **)&pCategoriesTag );

              if (NS_SUCCEEDED( rv )) {
                pCategoryValueTags = pCategoriesTag->mCategoryValueTags;

                NS_RELEASE( pCategoriesTag );
              }
            }
            else {
              // No category values available, error situation
              nsCAutoString  csURISpec, csStruct;

              csURISpec.AssignWithConversion( aRefDataSchemaURISpec );
              csStruct.AssignWithConversion( aRefDataStructName );
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PPolicy:  %s CheckPref, No category values associated with <DATA> item - %s#%s.\n", (const char *)mcsURISpec, (const char *)csURISpec, (const char *)csStruct) );

              rv = NS_ERROR_FAILURE;
            }
          }

          if (NS_SUCCEEDED( rv )) {
            rv = NS_NewP3PSimpleEnumerator( pCategoryValueTags,
                                            getter_AddRefs( pEnumerator ) );

            if (NS_SUCCEEDED( rv )) {
              // Process all category value tags
              rv = pEnumerator->HasMoreElements(&bMoreTags );

              while (NS_SUCCEEDED( rv ) && bMoreTags) {
                rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

                if (NS_SUCCEEDED( rv )) {
                  pTag->GetName( sCategoryValue );

                  // Compare the purpose/recipient/category combination
                  rv = CheckPref( aRefDataStructName,
                                  sShortDescription,
                                  aPurposeValue,
                                  aRecipientValue,
                                  sCategoryValue );
                }

                if (NS_SUCCEEDED( rv )) {
                  rv = pEnumerator->HasMoreElements(&bMoreTags );
                }
              }
            }
            else {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PPolicy:  %s CheckPref, NS_NewP3PSimpleEnumerator for category value tags failed - %X.\n", (const char *)mcsURISpec, rv) );
            }
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPolicy:  %s CheckPref, pCategoryValueTags->Count failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
    }
    else {
      nsCAutoString  csURISpec, csStruct;

      csURISpec.AssignWithConversion( aRefDataSchemaURISpec );
      csStruct.AssignWithConversion( aRefDataStructName );
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s CheckPref, pDataSchema->FindDataStruct failed for %s#%s - %X.\n", (const char *)mcsURISpec, (const char *)csURISpec, (const char *)csStruct, rv) );
    }
  }
  else {
    nsCAutoString  csURISpec;

    csURISpec.AssignWithConversion( aRefDataSchemaURISpec );
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicy:  %s CheckPref, %s DataSchema not present in DataSchema collection.\n", (const char *)mcsURISpec, (const char *)csURISpec) );
  }

  return rv;
}

// P3P Policy: CheckPref
//
// Function:  Compare all the <PURPOSE>, <RECIPIENT> and <CATEGORIES> values combination
//            to the user's preferences.
//
// Parms:     1. In     The DataStruct name of the <DATA> Tag object
//            2. In     The DataStruct "short-description" attribute value
//            3. In     The purpose value Tag object from the <PURPOSE> Tag object
//            4. In     The recipient value Tag object from the <RECIPIENT> Tag object
//            5. In     The category value Tag object from either the <DATA> Tag object or the DataStruct object
//
NS_METHOD
nsP3PPolicy::CheckPref( nsString&   aDataStructName,
                        nsString&   aShortDescription,
                        nsString&   aPurposeValue,
                        nsString&   aRecipientValue,
                        nsString&   aCategoryValue ) {

  nsresult                  rv;

  PRBool                    bAllowed;


  rv = mP3PService->GetBoolPref( aPurposeValue,
                                 aRecipientValue,
                                 aCategoryValue,
                                &bAllowed );

  if (NS_SUCCEEDED( rv )) {

    if (!bAllowed) {
      mPrefsResult = P3P_PRIVACY_NOT_MET;

      rv = SetFailedPrefsCombination( aDataStructName,
                                      aShortDescription,
                                      aPurposeValue,
                                      aRecipientValue,
                                      aCategoryValue );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPolicy:  %s CheckPref, mP3PService->GetBoolPref failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P Policy: SetFailedPrefsCombination
//
// Function:  Save the failed preference combination information.
//
// Parms:     1. In     The DataStruct name of the <DATA> Tag object
//            2. In     The DataStruct "short-description" attribute value
//            3. In     The purpose value Tag object from the <PURPOSE> Tag object
//            4. In     The recipient value Tag object from the <RECIPIENT> Tag object
//            5. In     The category value Tag object from either the <DATA> Tag object or the DataStruct object
//
NS_METHOD
nsP3PPolicy::SetFailedPrefsCombination( nsString&   aDataStructName,
                                        nsString&   aShortDescription,
                                        nsString&   aPurposeValue,
                                        nsString&   aRecipientValue,
                                        nsString&   aCategoryValue ) {

  nsresult                  rv = NS_OK;

  nsCAutoString             csDescription,
                            csPrefsCombination;

  nsAutoString              sDescription,
                            sPrefsCombination;

  nsStringKey               keyDataStructName( aDataStructName );

  nsP3PPolicyDataTagResult *pDataTagResult    = nsnull;


  if (mFailedDataTagRefs.Exists(&keyDataStructName )) {
    pDataTagResult = (nsP3PPolicyDataTagResult *)mFailedDataTagRefs.Get(&keyDataStructName );
  }
  else {
    if (aShortDescription.Length( ) > 0) {
      sDescription  = aShortDescription;
      sDescription.AppendWithConversion( " (" );
      sDescription += aDataStructName;
      sDescription.AppendWithConversion( ")" );
    }
    else {
      sDescription = aDataStructName;
    }

    rv = NS_NewP3PPolicyDataTagResult( sDescription,
                      (nsISupports **)&pDataTagResult );

    if (NS_SUCCEEDED( rv )) {
#ifdef DEBUG_P3P
      { nsCAutoString  csName, csDesc;

        csName.AssignWithConversion( aDataStructName );
        csDesc.AssignWithConversion( sDescription );
        printf( "P3P:    Adding DataStruct/Description to failed preferences: %s / %s\n", (const char *)csName, (const char *)csDesc );
      }
#endif

      mFailedDataTagRefs.Put(&keyDataStructName,
                              pDataTagResult );
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPolicy:  %s SetFailedPrefsCombination, NS_NewP3PPolicyDataTagResult failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    sPrefsCombination  = aCategoryValue;
    sPrefsCombination.AppendWithConversion( ", " );
    sPrefsCombination += aPurposeValue;
    sPrefsCombination.AppendWithConversion( ", " );
    sPrefsCombination += aRecipientValue;

    pDataTagResult->mFailedPrefsCombinations.AppendString( sPrefsCombination );

    csDescription.AssignWithConversion( sDescription );
    csPrefsCombination.AssignWithConversion( sPrefsCombination );
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PPolicy:  %s SetFailedPrefsCombination, category/purpose/recipient combination not allowed - %s / %s.\n", (const char *)mcsURISpec, (const char *)csDescription, (const char *)csPrefsCombination) );

  }

  NS_IF_RELEASE( pDataTagResult );

  return rv;
}


// ****************************************************************************
// nsIP3PXMLListener routines
// ****************************************************************************

// P3P Policy: DocumentComplete
//
// Function:  Notification of the completion of a DataSchema URI.
//
// Parms:     1. In     The DataSchema URI spec that has completed
//            2. In     The data supplied on the intial processing request
//
NS_IMETHODIMP
nsP3PPolicy::DocumentComplete( nsString&     aProcessedURISpec,
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

    // Remove the DataSchema from the list of processing DataSchemas
    mDataSchemasToRead.RemoveString( aProcessedURISpec );

    // Check if this Policy is now complete
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


// ****************************************************************************
// nsSupportsHashtable Enumeration Functor
// ****************************************************************************

// P3P Policy: Add RDF data tag elements
//
// Function:  Enumeration function for processing "failed" <DATA> Tag objects.
//
// Parms:     1. In     The HashKey of the element
//            2. In     The PolicyDataTagResult object
//            3. In     The Policy object
//
PRBool PR_CALLBACK AddRDFDataTagEnumerate( nsHashKey  *aHashKey,
                                           void       *aData,
                                           void       *aClosure ) {

  nsresult                  rv;

  nsP3PPolicyDataTagResult *pDataTagResult = nsnull;

  nsP3PPolicy              *pPolicy        = nsnull;


  pDataTagResult = (nsP3PPolicyDataTagResult *)aData;
  pPolicy        = (nsP3PPolicy *)aClosure;

  rv = pPolicy->AddRDFDataTag( pDataTagResult->mDescription,
                              &pDataTagResult->mFailedPrefsCombinations );

  return PR_TRUE;
}


// ****************************************************************************
// nsP3PPolicyDataTagResult Implementation routines
// ****************************************************************************

// P3P Policy Data Tag Result: nsISupports
NS_IMPL_ISUPPORTS1( nsP3PPolicyDataTagResult, nsISupports );

// P3P Policy Data Tag Result: Creation routine
NS_METHOD
NS_NewP3PPolicyDataTagResult( nsString&     aDescription,
                              nsISupports **aPolicyDataTagResult ) {

  nsresult                  rv;

  nsP3PPolicyDataTagResult *pNewPolicyDataTagResult = nsnull;


  NS_ENSURE_ARG_POINTER( aPolicyDataTagResult );

  *aPolicyDataTagResult = nsnull;

  pNewPolicyDataTagResult = new nsP3PPolicyDataTagResult( );

  if (pNewPolicyDataTagResult) {
    NS_ADDREF( pNewPolicyDataTagResult );

    pNewPolicyDataTagResult->mDescription = aDescription;

    rv = pNewPolicyDataTagResult->QueryInterface( NS_GET_IID( nsISupports ),
                                         (void **)aPolicyDataTagResult );

    NS_RELEASE( pNewPolicyDataTagResult );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Unable to create PolicyDataTagResult object\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Policy Data Tag Result: Constructor
nsP3PPolicyDataTagResult::nsP3PPolicyDataTagResult( ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Policy Data Tag Result: Destructor
nsP3PPolicyDataTagResult::~nsP3PPolicyDataTagResult( ) {
}
