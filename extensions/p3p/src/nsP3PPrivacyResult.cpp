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

#include "nsP3PPrivacyResult.h"
#include "nsP3PSimpleEnumerator.h"
#include "nsP3PLogging.h"

#include <nsIServiceManager.h>
#include <nsIComponentManager.h>

#include <rdf.h>
#include <nsRDFCID.h>

#include <nsXPIDLString.h>
#include <prprf.h>


// ****************************************************************************
// nsP3PPrivacyResult Implementation routines
// ****************************************************************************

static NS_DEFINE_CID( kRDFServiceCID,             NS_RDFSERVICE_CID );
static NS_DEFINE_CID( kRDFContainerCID,           NS_RDFCONTAINER_CID );
static NS_DEFINE_CID( kRDFContainerUtilsCID,      NS_RDFCONTAINERUTILS_CID );
static NS_DEFINE_CID( kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID );
static NS_DEFINE_CID( kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID );

// P3P Privacy Result: nsISupports
NS_IMPL_ISUPPORTS1( nsP3PPrivacyResult, nsIP3PPrivacyResult );

// P3P Privacy Result: Creation routine
NS_METHOD
NS_NewP3PPrivacyResult( nsString&             aURISpec,
                        nsIP3PPrivacyResult **aPrivacyResult ) {

  nsresult            rv;

  nsP3PPrivacyResult *pNewPrivacyResult = nsnull;


  NS_ENSURE_ARG_POINTER( aPrivacyResult );

  *aPrivacyResult = nsnull;

  pNewPrivacyResult = new nsP3PPrivacyResult( );

  if (pNewPrivacyResult) {
    NS_ADDREF( pNewPrivacyResult );

    rv = pNewPrivacyResult->Init( aURISpec );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewPrivacyResult->QueryInterface( NS_GET_IID( nsIP3PPrivacyResult ),
                                     (void **)aPrivacyResult );
    }

    NS_RELEASE( pNewPrivacyResult );
  }
  else {
    NS_ASSERTION( 0, "P3P:  PrivacyResult unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Privacy Result: Constructor
nsP3PPrivacyResult::nsP3PPrivacyResult( )
  : mIndirectlyCreated( PR_FALSE ),
    mPrivacyResult( P3P_PRIVACY_IN_PROGRESS ),
    mChildrenPrivacyResult( P3P_PRIVACY_IN_PROGRESS ),
    mOverallPrivacyResult( P3P_PRIVACY_IN_PROGRESS ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Privacy Result: Destructor
nsP3PPrivacyResult::~nsP3PPrivacyResult( ) {
}

// P3P Privacy Result: Init
//
// Function:  Initialization routine for the PrivacyResult object.
//
// Parms:     1. In     The URI spec that this object represents
//
NS_METHOD
nsP3PPrivacyResult::Init( nsString&  aURISpec ) {

  nsresult  rv = NS_OK;


#ifdef DEBUG_P3P
  printf("P3P:  PrivacyResult initializing.\n");
#endif

  mURISpec = aURISpec;
  mcsURISpec.AssignWithConversion( aURISpec );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PPrivacyResult:  %s Init, initializing.\n", (const char *)mcsURISpec) );

  // Create an array to hold the child PrivacyResults
  mChildren = do_CreateInstance( NS_SUPPORTSARRAY_CONTRACTID,
                                &rv );

  if (NS_SUCCEEDED( rv )) {
    // Get the RDF service
    mRDFService = do_GetService( kRDFServiceCID,
                                &rv );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPrivacyResult:  %s Init, do_GetService for RDF service failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the RDF Container Utils service
    mRDFContainerUtils = do_GetService( kRDFContainerUtilsCID,
                                       &rv );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPrivacyResult:  %s Init, do_GetService for RDF Container Utils service failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the RDF "type" resource
    rv = mRDFService->GetResource( RDF_NAMESPACE_URI "type",
                                   getter_AddRefs( mRDFResourceRDFType) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPrivacyResult:  %s Init, mRDFService->GetResource for %stype failed - %X.\n", (const char *)mcsURISpec, RDF_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PEntries" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PEntries",
                                   getter_AddRefs( mRDFResourceP3PEntries ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPrivacyResult:  %s Init, mRDFService->GetResource for %sP3PEntries failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PPolicy" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PPolicy",
                                   getter_AddRefs( mRDFResourceP3PPolicy ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPrivacyResult:  %s Init, mRDFService->GetResource for %sP3PPolicy failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PTitle" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PTitle",
                                   getter_AddRefs( mRDFResourceP3PTitle ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPrivacyResult:  %s Init, mRDFService->GetResource for %sP3PTitle failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the NC "P3PValue" resource
    rv = mRDFService->GetResource( NC_NAMESPACE_URI "P3PValue",
                                   getter_AddRefs( mRDFResourceP3PValue ) );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPrivacyResult:  %s Init, mRDFService->GetResource for %sP3PValue failed - %X.\n", (const char *)mcsURISpec, NC_NAMESPACE_URI, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Get the P3P service
    mP3PService = do_GetService( NS_P3PSERVICE_CONTRACTID,
                                &rv );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPrivacyResult:  %s Init, do_GetService for the P3P service failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  return rv;
}


// ****************************************************************************
// nsIP3PPrivacyResult routines
// ****************************************************************************

// P3P Privacy Result: SetURISpec
//
// Function:  Sets the URI spec associated with this PrivacyResult object
//
// Parms:     1. In     The URI spec
NS_IMETHODIMP_( void )
nsP3PPrivacyResult::SetURISpec( nsString&  aURISpec ) {

  nsCString  csOldURISpec = mcsURISpec;


  mURISpec = aURISpec;
  mcsURISpec.AssignWithConversion( aURISpec );

  // Cause the RDF Description to be rebuilt
  mRDFCombinedDataSource = nsnull;
  mRDFDataSource         = nsnull;

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PPrivacyResult:  %s SetURISpec, setting a new URI spec - %s.\n", (const char *)csOldURISpec, (const char *)mcsURISpec) );

  return;
}

// P3P Privacy Result: SetPrivacyResult
//
// Function:  Sets the Privacy information.
//
// Parms:     1. In     The result of the comparison between the Policy and the user's preferences
//            2. In     The Policy object used
//
NS_IMETHODIMP
nsP3PPrivacyResult::SetPrivacyResult( nsresult       aPrivacyResult,
                                      nsIP3PPolicy  *aPolicy ) {
                                      
  nsresult      rv = NS_OK;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PPrivacyResult:  %s SetPrivacyResult, setting the privacy result - %X.\n", (const char *)mcsURISpec, aPrivacyResult) );

  mPolicy        = aPolicy;
  mPrivacyResult = aPrivacyResult;

  rv = SetOverallPrivacyResult( aPrivacyResult,
                                PR_FALSE );

  return rv;
}

// P3P Privacy Result: SetOverallPrivacyResult
//
// Function:  Sets the overall Privacy result based upon the supplied Privacy result.
//
// Parms:     1. In     The Privacy result
//
NS_IMETHODIMP
nsP3PPrivacyResult::SetOverallPrivacyResult( nsresult   aPrivacyResult,
                                             PRBool     aChildCalling ) {

  nsresult  rv,
            previousResult;

  PRUint32  uiChildren;


  previousResult = mOverallPrivacyResult;

  if (aChildCalling) {
    // Set the overall children's privacy result
    if (aPrivacyResult != P3P_PRIVACY_IN_PROGRESS) {

      if (NS_FAILED( aPrivacyResult )) {
        mChildrenPrivacyResult = aPrivacyResult;
      }
      else if (NS_SUCCEEDED( mChildrenPrivacyResult ) && (mChildrenPrivacyResult != P3P_PRIVACY_NOT_MET)) {
        // The result can be updated
        if (aPrivacyResult == P3P_PRIVACY_NOT_MET) {
          mChildrenPrivacyResult = P3P_PRIVACY_NOT_MET;
        }
        else {
          if (mChildrenPrivacyResult == P3P_PRIVACY_NONE) {

            if ((aPrivacyResult == P3P_PRIVACY_PARTIAL) ||
                (aPrivacyResult == P3P_PRIVACY_MET)) {
              mChildrenPrivacyResult = P3P_PRIVACY_PARTIAL;
            }
          }
          else if (mChildrenPrivacyResult == P3P_PRIVACY_IN_PROGRESS) {
            mChildrenPrivacyResult = aPrivacyResult;
          }
          else if (mChildrenPrivacyResult == P3P_PRIVACY_MET) {

            if ((aPrivacyResult == P3P_PRIVACY_NONE) ||
                (aPrivacyResult == P3P_PRIVACY_PARTIAL)) {
              mChildrenPrivacyResult = P3P_PRIVACY_PARTIAL;
            }
          }
        }
      }
    }
  }

  rv = mChildren->Count(&uiChildren );

  if (NS_SUCCEEDED( rv )) {

    if (uiChildren == 0) {
      // No children, overall result is this objects result
      mOverallPrivacyResult = mPrivacyResult;
    }
    else {
      // We have children, calculate the result
      if ((aPrivacyResult         == P3P_PRIVACY_IN_PROGRESS) ||
          (mPrivacyResult         == P3P_PRIVACY_IN_PROGRESS) ||
          (mChildrenPrivacyResult == P3P_PRIVACY_IN_PROGRESS)) {
        mOverallPrivacyResult = P3P_PRIVACY_IN_PROGRESS;
      }
      else if (NS_FAILED( mPrivacyResult )) {
        mOverallPrivacyResult = mPrivacyResult;
      }
      else if (NS_FAILED( mChildrenPrivacyResult )) {
        mOverallPrivacyResult = mChildrenPrivacyResult;
      }
      else if ((mPrivacyResult         == P3P_PRIVACY_NOT_MET) ||
               (mChildrenPrivacyResult == P3P_PRIVACY_NOT_MET)) {
        mOverallPrivacyResult = P3P_PRIVACY_NOT_MET;
      }
      else {
        if (mPrivacyResult == P3P_PRIVACY_NONE) {

          if (mChildrenPrivacyResult == P3P_PRIVACY_NONE) {
            mOverallPrivacyResult = P3P_PRIVACY_NONE;
          }
          else {
            // Left with PARTIAL or MET
            if (mParent) {
              // We have a parent, so indicate PARTIAL
              mOverallPrivacyResult = P3P_PRIVACY_PARTIAL;
            }
            else {
              mOverallPrivacyResult = P3P_PRIVACY_NONE;
            }
          }
        }
        else {
          // Left with MET (mPrivacyResult can never be PARTIAL)
          if ((mChildrenPrivacyResult == P3P_PRIVACY_NONE) ||
              (mChildrenPrivacyResult == P3P_PRIVACY_PARTIAL)) {
            mOverallPrivacyResult = P3P_PRIVACY_PARTIAL;
          }
          else {
            // Left with MET
            mOverallPrivacyResult = P3P_PRIVACY_MET;
          }
        }
      }
    }
  }
  else {
    mOverallPrivacyResult = NS_ERROR_FAILURE;
  }

#ifdef DEBUG_P3P
  { printf( "P3P:    Setting overall privacy result for: %s  Old: %08X  New: %08X\n", (const char *)mcsURISpec, previousResult, mOverallPrivacyResult );
  }
#endif

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PPrivacyResult:  %s SetOverallPrivacyResult, setting the overall privacy result - Old: %X  New: %X.\n", (const char *)mcsURISpec, previousResult, mOverallPrivacyResult) );

  if (mParent) {
    // Report the result up the parent chain
    rv = mParent->SetOverallPrivacyResult( mOverallPrivacyResult,
                                           PR_TRUE );
  }
  else {
    rv = mOverallPrivacyResult;
  }

  return rv;
}

// P3P Privacy Result: GetPrivacyInfo
//
// Function:  Gets the RDF description of the Privacy information.
//
// Parms:     1. Out    The RDFDataSource describing the Privacy information
//
NS_IMETHODIMP
nsP3PPrivacyResult::GetPrivacyInfo( nsIRDFDataSource **aDataSource ) {

  nsresult                    rv;

  nsAutoString                sObjectAbout,
                              sObjectTitle;

  nsCOMPtr<nsIRDFDataSource>  pDataSource;

  nsAutoString                sName,
                              sTitle,
                              sValue;

  nsCOMPtr<nsIRDFResource>    pResource;


  NS_ENSURE_ARG_POINTER( aDataSource );

  *aDataSource = nsnull;

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PPrivacyResult:  %s GetPrivacyInfo, retrieving \"root\" privacy information.\n", (const char *)mcsURISpec) );

  // Create the RDF description for this PrivacyResult
  rv = CreateRDFDescription( sObjectAbout,
                             sObjectTitle,
                             getter_AddRefs( pDataSource ) );

  if (NS_SUCCEEDED( rv )) {
    sName.AssignWithConversion( "NC:P3PPolicyRoot" );
    sTitle.Truncate( );
    sValue.Truncate( );

    // Create the "NC:P3PPolicyRoot" description resource - the start of the tree
    rv = AddRDFEntry( sName,
                      sTitle,
                      sValue,
                      nsnull,
                      nsnull,
                      getter_AddRefs( mRootDescription ) );

    if (NS_SUCCEEDED( rv )) {
      sName.Truncate( );
      sTitle.Truncate( );
      sValue.Truncate( );

      // Create the container resource
      rv = AddRDFEntry( sName,
                        sTitle,
                        sValue,
                        nsnull,
                        nsnull,
                        getter_AddRefs( mRootContainer ) );

      if (NS_SUCCEEDED( rv )) {
        // Make the container resource a sequential container
        rv = mRDFContainerUtils->MakeSeq( mRDFDataSource,
                                          mRootContainer,
                                          nsnull );

        if (NS_SUCCEEDED( rv )) {
          sValue.Truncate( );

          // Create the privacy information resource
          rv = AddRDFEntry( sObjectAbout,
                            sObjectTitle,
                            sValue,
                            nsnull,
                            mRootContainer,
                            getter_AddRefs( pResource ) );

          if (NS_SUCCEEDED( rv )) {
            // Assert the privacy information into the datasource
            rv = mRDFDataSource->Assert( mRootDescription,
                                         mRDFResourceP3PEntries,
                                         mRootContainer,
                                         PR_TRUE );

            if (NS_FAILED( rv )) {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PPrivacyResult:  %s GetPrivacyInfo, mRDFDataSource->Assert failed - %X.\n", (const char *)mcsURISpec, rv) );
            }
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PPrivacyResult:  %s GetPrivacyInfo, creation of the privacy information resource failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPrivacyResult:  %s GetPrivacyInfo, mRDFContainerUtils->MakeSeq failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPrivacyResult:  %s GetPrivacyInfo, creation of the container resource failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPrivacyResult:  %s GetPrivacyInfo, creation of \"root\" information resource failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    *aDataSource = mRDFCombinedDataSource;
    NS_ADDREF( *aDataSource );
  }

  return rv;
}

// P3P Privacy Result: CreateRDFDescription
//
// Function:  Creates the RDF description of the Privacy information.
//
// Parms:     1. In     The "about" attribute to be used to access the Privacy information
//            2. In     The "title" attribute to be used to describe the Privacy information
//            3. Out    The RDFDataSource describing the Privacy information
//
NS_IMETHODIMP
nsP3PPrivacyResult::CreateRDFDescription( nsString&          aAbout,
                                          nsString&          aTitle,
                                          nsIRDFDataSource **aDataSource ) {

  nsresult                       rv = NS_OK;

  char                          *csPrivacyResult     = nsnull;

  nsAutoString                   sName,
                                 sTitle,
                                 sValue;

  nsAutoString                   sPolicyAbout,
                                 sPolicyTitle,
                                 sChildAbout,
                                 sChildTitle;

  nsCOMPtr<nsIRDFDataSource>     pDataSource;

  nsCOMPtr<nsIRDFResource>       pResource;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PPrivacyResult>  pPrivacyResult;

  PRBool                         bMorePrivacyResults;


  NS_ENSURE_ARG_POINTER( aDataSource );

  *aDataSource = nsnull;

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PPrivacyResult:  %s CreateRDFDescription, getting/creating RDF description of privacy information.\n", (const char *)mcsURISpec) );

  if (!mRDFCombinedDataSource || !mRDFDataSource) {
    csPrivacyResult = PR_smprintf( "%X", this );

    if (csPrivacyResult) {
      // Create a unique "about" attribute
      mRDFAbout.AssignWithConversion( "#__" );
      mRDFAbout += mURISpec;
      mRDFAbout.AppendWithConversion( " @ " );
      mRDFAbout.AppendWithConversion( csPrivacyResult );
      PR_smprintf_free( csPrivacyResult );

      // Create a Composite datasource to hold all datasources
      mRDFCombinedDataSource = do_CreateInstance( kRDFCompositeDataSourceCID,
                                                 &rv );

      if (NS_SUCCEEDED( rv )) {
        // Create an InMemory datasource for the privacy information
        mRDFDataSource = do_CreateInstance( kRDFInMemoryDataSourceCID,
                                           &rv );

        if (NS_SUCCEEDED( rv )) {
          // Add the InMemory datasource to the Composite datasource
          rv = mRDFCombinedDataSource->AddDataSource( mRDFDataSource );

          if (NS_SUCCEEDED( rv )) {
            sName = mRDFAbout;
            sTitle.Truncate( );
            sValue.Truncate( );

            // Create the PrivacyResult information resource
            rv = AddRDFEntry( sName,
                              sTitle,
                              sValue,
                              nsnull,
                              nsnull,
                              getter_AddRefs( mDescription ) );

            if (NS_SUCCEEDED( rv )) {
              sName.Truncate( );
              sTitle.Truncate( );
              sValue.Truncate( );

              // Create the PrivacyResult container resource
              rv = AddRDFEntry( sName,
                                sTitle,
                                sValue,
                                nsnull,
                                nsnull,
                                getter_AddRefs( mContainer ) );

              if (NS_SUCCEEDED( rv )) {
                // Make the PrivacyResult container a sequential container
                rv = mRDFContainerUtils->MakeSeq( mRDFDataSource,
                                                  mContainer,
                                                  nsnull );

                if (NS_SUCCEEDED( rv )) {

                  if (mPolicy) {
                    // Get the Policy RDF datasource
                    rv = mPolicy->GetPolicyDescription( sPolicyAbout,
                                                        sPolicyTitle,
                                                        getter_AddRefs( pDataSource ) );

                    if (NS_SUCCEEDED( rv )) {

                      if (pDataSource) {
                        // Add the Policy datasource to the Composite datasource
                        rv = mRDFCombinedDataSource->AddDataSource( pDataSource );

                        if (NS_FAILED( rv )) {
                          PR_LOG( gP3PLogModule,
                                  PR_LOG_ERROR,
                                  ("P3PPrivacyResult:  %s CreateRDFDescription, mRDFCombinedDataSource->AddDataSource for Policy failed - %X.\n", (const char *)mcsURISpec, rv) );
                        }
                      }
                      else {
                        // No Policy datasource returned, error condition
                        PR_LOG( gP3PLogModule,
                                PR_LOG_ERROR,
                                ("P3PPrivacyResult:  %s CreateRDFDescription, mPolicy->GetPolicyDescription returned a null datasource.\n", (const char *)mcsURISpec) );

                        rv = NS_ERROR_FAILURE;
                      }
                    }
                    else {
                      PR_LOG( gP3PLogModule,
                              PR_LOG_ERROR,
                              ("P3PPrivacyResult:  %s CreateRDFDescription, mPolicy->GetPolicyDescription failed - %X.\n", (const char *)mcsURISpec, rv) );
                    }
                  }
                  else {
                    // There is no Policy associated with the object
                    PR_LOG( gP3PLogModule,
                            PR_LOG_ERROR,
                            ("P3PPrivacyResult:  %s CreateRDFDescription, no Policy associated.\n", (const char *)mcsURISpec) );

                    sPolicyAbout = mRDFAbout;
                    sPolicyAbout.AppendWithConversion( "NoPolicy" );
                    rv = mP3PService->GetLocaleString( "NoPolicy",
                                                       sPolicyTitle );
                  }

                  if (NS_FAILED( rv )) {
                    // An error occurred obtaining the Policy information, let it be known
                    sPolicyAbout = mRDFAbout;
                    sPolicyAbout.AppendWithConversion( "ErrorObtainingPolicy" );
                    rv = mP3PService->GetLocaleString( "ErrorObtainingPolicy",
                                                       sPolicyTitle );
                  }

                  if (NS_SUCCEEDED( rv )) {
                    // Create the Policy description resource
                    sValue.Truncate( );
                    rv = AddRDFEntry( sPolicyAbout,
                                      sPolicyTitle,
                                      sValue,
                                      mRDFResourceP3PPolicy,
                                      mContainer,
                                      getter_AddRefs( pResource ) );

                    if (NS_SUCCEEDED( rv )) {
                      // Get all the children RDF descriptions
                      sValue.Truncate( );
                      rv = NS_NewP3PSimpleEnumerator( mChildren,
                                                      getter_AddRefs( pEnumerator ) );

                      if (NS_SUCCEEDED( rv )) {
                        rv = pEnumerator->HasMoreElements(&bMorePrivacyResults );

                        while (NS_SUCCEEDED( rv ) && bMorePrivacyResults) {
                          rv = pEnumerator->GetNext( getter_AddRefs( pPrivacyResult ) );

                          if (NS_SUCCEEDED( rv )) {
                            // Get the child PrivacyResult RDF datasource
                            rv = pPrivacyResult->CreateRDFDescription( sChildAbout,
                                                                       sChildTitle,
                                                                       getter_AddRefs( pDataSource ) );

                            if (NS_SUCCEEDED( rv )) {

                              if (pDataSource) {
                                // Add the PrivacyResult datasource to the Composite datasource
                                rv = mRDFCombinedDataSource->AddDataSource( pDataSource );
                              }

                              if (NS_SUCCEEDED( rv )) {
                                // Create the child PrivacyResult description resource
                                rv = AddRDFEntry( sChildAbout,
                                                  sChildTitle,
                                                  sValue,
                                                  mRDFResourceP3PPolicy,
                                                  mContainer,
                                                  getter_AddRefs( pResource ) );

                                if (NS_FAILED( rv )) {
                                  PR_LOG( gP3PLogModule,
                                          PR_LOG_ERROR,
                                          ("P3PPrivacyResult:  %s CreateRDFDescription, creation of child PrivacyResult description resource failed - %X.\n", (const char *)mcsURISpec, rv) );
                                }
                              }
                              else {
                                PR_LOG( gP3PLogModule,
                                        PR_LOG_ERROR,
                                        ("P3PPrivacyResult:  %s CreateRDFDescription, mRDFCombinedDataSource->AddDataSource for child PrivacyResult failed - %X.\n", (const char *)mcsURISpec, rv) );
                              }
                            }
                            else {
                              PR_LOG( gP3PLogModule,
                                      PR_LOG_ERROR,
                                      ("P3PPrivacyResult:  %s CreateRDFDescription, pPrivacyResult->CreateRDFDescription failed - %X.\n", (const char *)mcsURISpec, rv) );
                            }
                          }

                          if (NS_SUCCEEDED( rv )) {
                            rv = pEnumerator->HasMoreElements(&bMorePrivacyResults );
                          }
                        }
                      }
                      else {
                        PR_LOG( gP3PLogModule,
                                PR_LOG_ERROR,
                                ("P3PPrivacyResult:  %s CreateRDFDescription, NS_NewP3PSimpleEnumerator failed - %X.\n", (const char *)mcsURISpec, rv) );
                      }
                    }
                    else {
                      PR_LOG( gP3PLogModule,
                              PR_LOG_ERROR,
                              ("P3PPrivacyResult:  %s CreateRDFDescription, creation of Policy description resource failed - %X.\n", (const char *)mcsURISpec, rv) );
                    }
                  }

                  if (NS_SUCCEEDED( rv )) {
                    rv = mRDFDataSource->Assert( mDescription,
                                                 mRDFResourceP3PEntries,
                                                 mContainer,
                                                 PR_TRUE );
                  }
                }
                else {
                  PR_LOG( gP3PLogModule,
                          PR_LOG_ERROR,
                          ("P3PPrivacyResult:  %s CreateRDFDescription, mRDFContainerUtils->MakeSeq failed - %X.\n", (const char *)mcsURISpec, rv) );
                }
              }
              else {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PPrivacyResult:  %s CreateRDFDescription, creation of the PrivacyResult container resource failed - %X.\n", (const char *)mcsURISpec, rv) );
              }
            }
            else {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PPrivacyResult:  %s CreateRDFDescription, creation of the PrivacyResult description resource failed - %X.\n", (const char *)mcsURISpec, rv) );
            }
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PPrivacyResult:  %s CreateRDFDescription, mRDFCombinedDataSource->AddDataSource failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPrivacyResult:  %s CreateRDFDescription, do_CreateInstance of in-memory datasource failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPrivacyResult:  %s CreateRDFDescription, do_CreateInstance of composite datasource failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      rv = NS_ERROR_OUT_OF_MEMORY;

      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PPrivacyResult:  %s CreateRDFDescription, PR_smprintf failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    aAbout       = mRDFAbout;
    aTitle       = mURISpec;
    *aDataSource = mRDFCombinedDataSource;
    NS_ADDREF( *aDataSource );
  }
  else {
    mRDFCombinedDataSource = nsnull;
    mRDFDataSource         = nsnull;
  }

  return rv;
}

// P3P Privacy Result: AddChild
//
// Function:  Adds the PrivacyResult object as a child.
//
// Parms:     1. In     The child PrivacyResult object to add
//
NS_IMETHODIMP
nsP3PPrivacyResult::AddChild( nsIP3PPrivacyResult  *aChild ) {

  nsresult  rv = NS_OK;


  if (mChildren->IndexOf( aChild ) < 0) {

    // Cause the RDF Description to be rebuilt
    mRDFCombinedDataSource = nsnull;
    mRDFDataSource         = nsnull;

    if (mChildren->AppendElement( aChild )) {
      aChild->SetParent( this );
    }
    else {
      rv = NS_ERROR_FAILURE;
    }
  }

  return rv;
}

// P3P Privacy Result: RemoveChild
//
// Function:  Removes the PrivacyResult object as a child.
//
// Parms:     1. In     The child PrivacyResult object to remove
//
NS_IMETHODIMP
nsP3PPrivacyResult::RemoveChild( nsIP3PPrivacyResult  *aChild ) {

  // Cause the RDF Description to be rebuilt
  mRDFCombinedDataSource = nsnull;
  mRDFDataSource         = nsnull;

  aChild->SetParent( nsnull );
  mChildren->RemoveElement( aChild );

  return NS_OK;
}

// P3P Privacy Result: RemoveChildren
//
// Function:  Removes all the PrivacyResult object children.
//
// Parms:     None
//
NS_IMETHODIMP_( void )
nsP3PPrivacyResult::RemoveChildren( ) {

  nsresult                       rv;

  PRUint32                       iCount,
                                 iChildren;

  nsCOMPtr<nsIP3PPrivacyResult>  pChild;


  mRDFCombinedDataSource = nsnull;
  mRDFDataSource         = nsnull;

  rv = mChildren->Count(&iChildren );

  if (NS_SUCCEEDED( rv )) {
    for (iCount = 0;
           iCount < iChildren;
             iCount++) {
      rv = mChildren->GetElementAt( iCount,
                                    getter_AddRefs( pChild ) );

      if (NS_SUCCEEDED( rv )) {
        pChild->SetParent( nsnull );
      }
    }
  }

  mChildren->Clear( );

  return;
}

// P3P Privacy Result: GetChildren
//
// Function:  Gets the PrivacyResult object children.
//
// Parms:     1. Out    The PrivacyResult object children
//
NS_IMETHODIMP
nsP3PPrivacyResult::GetChildren( nsISupportsArray **aChildren ) {

  nsresult                    rv;

  nsCOMPtr<nsISupportsArray>  pChildren;


  NS_ENSURE_ARG_POINTER( aChildren );

  *aChildren = nsnull;

  rv = mChildren->Clone( aChildren );

  return rv;
}

// P3P Privacy Result: SetParent
//
// Function:  Sets the parent of the PrivacyResult object.
//
// Parms:     1. In     The parent PrivacyResult object
//
NS_IMETHODIMP_( void )
nsP3PPrivacyResult::SetParent( nsIP3PPrivacyResult  *aParent ) {

  mParent = aParent;

  return;
}

// P3P Privacy Result: GetParent
//
// Function:  Gets the parent PrivacyResult object.
//
// Parms:     1. Out    The parent PrivacyResult object
//
NS_IMETHODIMP
nsP3PPrivacyResult::GetParent( nsIP3PPrivacyResult **aParent ) {

  NS_ENSURE_ARG_POINTER( aParent );

  *aParent = mParent;
  NS_IF_ADDREF( *aParent );

  return NS_OK;
}

// P3P Privacy Result: IndirectlyCreated
//
// Function:  Sets the indicator that this PrivacyResult object was indirectly created.
//
// Parms:     None
//
NS_IMETHODIMP_( void )
nsP3PPrivacyResult::IndirectlyCreated( ) {

  mIndirectlyCreated = PR_TRUE;
}


// ****************************************************************************
// nsP3PPrivacyResult routines
// ****************************************************************************

// P3P Privacy Result: AddRDFEntry
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
nsP3PPrivacyResult::AddRDFEntry( nsString&        aName,
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
    // Create an anonymouse resource
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
                ("P3PPrivacyResult:  %s AddRDFEntry, mRDFDataSource->Assert for RDFType failed - %X.\n", (const char *)mcsURISpec, rv) );
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
                  ("P3PPrivacyResult:  %s AddRDFEntry, mRDFDataSource->Assert for P3PTitle failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPrivacyResult:  %s AddRDFEntry, mRDFDataService->GetLiteral for P3PTitle failed - %X.\n", (const char *)mcsURISpec, rv) );
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
                  ("P3PPrivacyResult:  %s AddRDFEntry, mRDFDataSource->Assert for P3PValue failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPrivacyResult:  %s AddRDFEntry, mRDFDataService->GetLiteral for P3PValue failed - %X.\n", (const char *)mcsURISpec, rv) );
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
                    ("P3PPrivacyResult:  %s AddRDFEntry, pContainer->AppendElement failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PPrivacyResult:  %s AddRDFEntry, pContainer->Init failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PPrivacyResult:  %s AddRDFEntry, do_CreateInstance for RDF container failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PPrivacyResult:  %s AddRDFEntry, creation of RDF resource failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}
