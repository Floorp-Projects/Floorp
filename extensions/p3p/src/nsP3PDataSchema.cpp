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

#include "nsP3PDataSchema.h"
#include "nsP3PTags.h"
#include "nsP3PDataStruct.h"
#include "nsP3PSimpleEnumerator.h"
#include "nsP3PLogging.h"

#include <nsIDirectoryService.h>
#include <nsIProperties.h>
#include <nsIFile.h>

#include <nsNetUtil.h>

#include <nsXPIDLString.h>

#include <nsIFileLocator.h>
#include <nsFileLocations.h>
#include <nsIFileSpec.h>

#include <nsAutoLock.h>


// ****************************************************************************
// nsP3PDataSchema Implementation routines
// ****************************************************************************

// P3P DataSchema nsISupports
NS_IMPL_ISUPPORTS2( nsP3PDataSchema, nsIP3PDataSchema,
                                     nsIP3PXMLListener );

// P3P DataSchema creation routine
NS_METHOD
NS_NewP3PDataSchema( nsString&          aDataSchemaURISpec,
                     nsIP3PDataSchema **aDataSchema ) {

  nsresult         rv;

  nsP3PDataSchema *pNewDataSchema = nsnull;


  NS_ENSURE_ARG_POINTER( aDataSchema );

  *aDataSchema = nsnull;

  pNewDataSchema = new nsP3PDataSchema( );

  if (pNewDataSchema) {
    NS_ADDREF( pNewDataSchema );

    rv = pNewDataSchema->Init( aDataSchemaURISpec );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewDataSchema->QueryInterface( NS_GET_IID( nsIP3PDataSchema ),
                                  (void **)aDataSchema );
    }

    NS_RELEASE( pNewDataSchema );
  }
  else {
    NS_ASSERTION( 0, "P3P:  DataSchema unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Data Schema: Constructor
nsP3PDataSchema::nsP3PDataSchema( ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Data Schema: Destructor
nsP3PDataSchema::~nsP3PDataSchema( ) {

  if (mDataStruct) {
    mDataStruct->DestroyTree( );
  }
}

// ****************************************************************************
// nsP3PXMLProcessor method overrides
// ****************************************************************************

// P3P Data Schema: PostInit
//
// Function:  Perform any additional intialization to the XMLProcessor's.
//
// Parms:     1. In     The DataSchema URI spec
//
NS_IMETHODIMP
nsP3PDataSchema::PostInit( nsString&  aURISpec ) {

  nsresult                  rv = NS_OK;

  nsCOMPtr<nsIFileLocator>  pFileLocator;

  nsCOMPtr<nsIFileSpec>     pFileSpec;

  nsXPIDLCString            xcsPath;


  mDocumentType = P3P_DOCUMENT_DATASCHEMA;

  if (aURISpec.EqualsWithConversion( P3P_BASE_DATASCHEMA )) {
    // Reset the base data schema to a file in the components directory
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PDataSchema:  %s PostInit, attempting to redirect base DataSchema.\n", (const char *)mcsURISpec) );

    // Create a file locator object
    pFileLocator = do_CreateInstance( NS_FILELOCATOR_CONTRACTID,
                                     &rv );

    if (NS_SUCCEEDED( rv )) {
      // Find the components directory
      rv = pFileLocator->GetFileLocation( nsSpecialFileSpec::App_ComponentsDirectory,
                                          getter_AddRefs( pFileSpec ) );

      if (NS_SUCCEEDED( rv )) {
        // Extract the path to the components directory
        rv = pFileSpec->GetURLString( getter_Copies( xcsPath ) );

        if (NS_SUCCEEDED( rv )) {
          // Make the local path the URI to be read
          mUseDOMParser = PR_TRUE;
          mReadURISpec.AssignWithConversion((const char *)xcsPath );
          mReadURISpec.AppendWithConversion( P3P_BASE_DATASCHEMA_LOCAL_NAME );
          mcsReadURISpec.AssignWithConversion( mReadURISpec );
          PR_LOG( gP3PLogModule,
                  PR_LOG_NOTICE,
                  ("P3PDataSchema:  %s PostInit, redirecting base DataSchema to %s.\n", (const char *)mcsURISpec, (const char *)mcsReadURISpec) );
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PDataSchema:  %s PostInit, pFileSpec->GetURLString failed - %X, using remote base DataSchema.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PDataSchema:  %s PostInit, pFileLocator->GetFileLocation failed - %X, using remote base DataSchema.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PDataSchema:  %s PostInit, CreateInstance of pFileLocator failed - %X, using remote base DataSchema.\n", (const char *)mcsURISpec, rv) );
    }

    // Don't fail if we can't obtain the "local" base DataSchema
    rv = NS_OK;
  }

  return rv;
}

// P3P Data Schema: PreRead
//
// Function:  Perform circular DataSchema reference check.
//
// Parms:     1. In     The type of read performed
//            2. In     The Policy or DataSchema URI initiating the read
//
NS_IMETHODIMP
nsP3PDataSchema::PreRead( PRInt32   aReadType,
                          nsIURI   *aReadForURI ) {

  nsresult        rv;

  nsXPIDLCString  xcsURISpec;

  nsAutoString    sURISpec;


  rv = aReadForURI->GetSpec( getter_Copies( xcsURISpec ) );

  if (NS_SUCCEEDED( rv )) {
    sURISpec.AssignWithConversion((const char *)xcsURISpec );

    if (mDataSchemasToRead.IndexOf( sURISpec ) >= 0) {
      // We are already in the process of reading the URI that is
      // requesting this read, not good.
#ifdef DEBUG_P3P
      { printf( "P3P:    Circular DataSchema reference: %s attempting to read %s\n", (const char *)xcsURISpec, (const char *)mcsURISpec );
      }
#endif

      rv = NS_ERROR_FAILURE;

      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PDataSchema:  %s PreRead, circular DataSchema reference.\n", (const char *)mcsURISpec) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PDataSchema:  %s PreRead, aReadForURI->GetSpec failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P Data Schema: ProcessContent
//
// Function:  Process the DataSchema.
//
// Parms:     1. In     The type of read performed
//
NS_IMETHODIMP
nsP3PDataSchema::ProcessContent( PRInt32   aReadType ) {

  if (!mRedirected && !mInlineRead) {
    // Cache the DataSchema if it wasn't an indirect reference or an inline DataSchema
    //   Caching is performed here (as opposed to PreRead) because there is no
    //   support for redirected (or expired should that ever be added) DataSchemas
    //   that have been cached (the ability to re-read the DataSchema and set the
    //   "In Progress" status).
#ifdef DEBUG_P3P
    { printf( "P3P:  Caching Data Schema Object: %s\n", (const char *)mcsURISpec );
    }
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PDataSchema:  %s ProcessContent, caching DataSchema.\n", (const char *)mcsURISpec) );

    mP3PService->CacheDataSchema( mURISpec,
                                  this );
  }

  ProcessDataSchema( aReadType );

  return NS_OK;
}

// P3P Data Schema: CheckForComplete
//
// Function:  Checks to see if all processing is complete for this DataSchema (this
//            includes processing other referenced DataSchemas).  If processing is
//            complete, then mProcessComplete must be set to TRUE.
//
// Parms:     None
//
NS_IMETHODIMP_( void )
nsP3PDataSchema::CheckForComplete( ) {

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
                ("P3PDataSchema:  %s CheckForComplete, all DataSchemas processed.\n", (const char *)mcsURISpec) );
      }

#ifdef DEBUG_P3P
      { printf( "P3P:  DataSchema complete: %s\n", (const char *)mcsURISpec );
      }
#endif

      PR_LOG( gP3PLogModule,
              PR_LOG_NOTICE,
              ("P3PDataSchema:  %s CheckForComplete, this DataSchema is complete.\n", (const char *)mcsURISpec) );

      // Construct a data model
      BuildDataModel( );

      // Indicate that this DataSchema is complete
      mProcessComplete = PR_TRUE;
    }
  }
  else {
    // We don't have a valid document, indicate that this DataSchema is complete
    mProcessComplete = PR_TRUE;
  }

  return;
}


// ****************************************************************************
// nsIP3PDataSchema routines
// ****************************************************************************

// P3P Data Schema: FindDataStruct
//
// Function:  Returns the DataStruct object for a given DataStruct name.
//
// Parms:     1. In     The DataStruct name
//            2. Out    The DataStruct object
//
NS_IMETHODIMP
nsP3PDataSchema::FindDataStruct( nsString&          aDataStructName,
                                 nsIP3PDataStruct **aDataStruct ) {

  nsresult                    rv;

  PRInt32                     iQualifier;

  nsCOMPtr<nsIP3PDataStruct>  pDataStruct,
                              pNewDataStruct;

  nsAutoString                sDataStructName,
                              sDataStructNodeName,
                              sDataStructWorkName;

  PRBool                      bFound;


  NS_ENSURE_ARG_POINTER( aDataStruct );

  *aDataStruct = nsnull;

  if (mContentValid && mDocumentValid) {
    mFirstUse = PR_FALSE;

    // Search through the data model tree to find the DataStruct
    pNewDataStruct      = mDataStruct;
    sDataStructWorkName = aDataStructName;
    bFound              = PR_FALSE;
    do {
      pDataStruct     = pNewDataStruct;
      sDataStructName = sDataStructWorkName;

      // Split the data struct name into a node name and the remainder
      //   ie. dynamic.http.useragent => dynamic and http.useragent
      iQualifier = sDataStructName.Find( "." );
      sDataStructName.Left( sDataStructNodeName,
                            iQualifier );
      sDataStructName.Right( sDataStructWorkName,
                             sDataStructName.Length( ) - (iQualifier + 1) );

      // Look for the node name
      rv = FindChildDataStruct( sDataStructNodeName,
                                pDataStruct,
                                getter_AddRefs( pNewDataStruct ) );

    } while (NS_SUCCEEDED( rv ) && pNewDataStruct && (iQualifier >= 0));

    if (NS_SUCCEEDED( rv )) {

      if (pNewDataStruct) {
        rv = pNewDataStruct->QueryInterface( NS_GET_IID( nsIP3PDataStruct ),
                                    (void **)aDataStruct );
      }
      else {
        nsCAutoString  csDataStructName;

        rv = NS_ERROR_FAILURE;

        csDataStructName.AssignWithConversion( aDataStructName );
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PDataSchema:  %s FindDataStruct, requested data struct not found - %s.\n", (const char *)mcsURISpec, (const char *)csDataStructName) );
      }
    }
  }
  else {
    rv = NS_ERROR_FAILURE;
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PDataSchema:  %s FindDataStruct, DataSchema is not valid.\n", (const char *)mcsURISpec) );
  }

  return rv;
}


// ****************************************************************************
// nsP3PDataSchema routines
// ****************************************************************************

// P3P Data Schema: ProcessDataSchema
//
// Function:  Validate and process the DataSchema
//
// Parms:     1. In     The type of read performed
//
NS_METHOD_( void )
nsP3PDataSchema::ProcessDataSchema( PRInt32   aReadType ) {

  nsresult                    rv;

  nsCOMPtr<nsIP3PTag>         pTag;              // Required for macro...

  nsCOMPtr<nsIDOMNode>        pDOMNodeWithTag;   // Required for macro...


  // Look for DATASCHEMA tag and process the DataSchema
  NS_P3P_TAG_PROCESS_SINGLE( DataSchema,
                             DATASCHEMA,
                             P3P,
                             PR_FALSE );

  if (NS_SUCCEEDED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PDataSchema:  %s ProcessDataSchema, <DATASCHEMA> tag successfully processed.\n", (const char *)mcsURISpec) );

    mTag = pTag;

    // Record all DataSchemas related to this DataSchema
    rv = ProcessDataSchemas( aReadType );
  }

  if (NS_FAILED( rv )) {
    SetNotValid( );
  }

  return;
}

// P3P Data Schema: ProcessDataSchemas
//
// Function:  Find all DataSchemas associated with the <DATA-STRUCT> and <DATA-DEF> Tag objects.
//
// Parms:     1. In     The type of read performed
//
NS_METHOD
nsP3PDataSchema::ProcessDataSchemas( PRInt32   aReadType ) {

  nsresult                       rv = NS_OK;

  nsP3PDataSchemaTag            *pDataSchemaTag = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  rv = mTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pDataSchemaTag );

  if (NS_SUCCEEDED( rv )) {
    rv = NS_NewP3PSimpleEnumerator( pDataSchemaTag->mDataStructTags,
                                    getter_AddRefs( pEnumerator ) );

    if (NS_SUCCEEDED( rv )) {
      // Process all <DATA-STRUCT> tags
      rv = pEnumerator->HasMoreElements(&bMoreTags );

      while (NS_SUCCEEDED( rv ) && bMoreTags) {
        rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

        if (NS_SUCCEEDED( rv )) {
          // Process the DataSchema specified in the <DATA-STRUCT> tag
          rv = ProcessDataStructTagDataSchema( pTag,
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
              ("P3PDataSchema:  %s RecordDataSchemas, NS_NewP3PSimpleEnumerator for <DATA-STRUCT> tags failed - %X.\n", (const char *)mcsURISpec, rv) );
    }

    if (NS_SUCCEEDED( rv )) {
      rv = NS_NewP3PSimpleEnumerator( pDataSchemaTag->mDataDefTags,
                                      getter_AddRefs( pEnumerator ) );

      if (NS_SUCCEEDED( rv )) {
        // Process all <DATA-DEF> tags
        rv = pEnumerator->HasMoreElements(&bMoreTags );

        while (NS_SUCCEEDED( rv ) && bMoreTags) {
          rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

          if (NS_SUCCEEDED( rv )) {
            // Process the DataSchema specified in the <DATA-DEF> tag
            rv = ProcessDataDefTagDataSchema( pTag,
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
                ("P3PDataSchema:  %s RecordDataSchemas, NS_NewP3PSimpleEnumerator for <DATA-DEF> tags failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }

    NS_RELEASE( pDataSchemaTag );
  }

  return rv;
}

// P3P Data Schema: ProcessDataStructTagDataSchema
//
// Function:  If a DataSchema is specified, add it to the list of DataSchemas to read
//
// Parms:     1. In     The <DATA-STRUCT> Tag object
//            2. In     The type of read performed
//
NS_METHOD
nsP3PDataSchema::ProcessDataStructTagDataSchema( nsIP3PTag  *aTag,
                                                 PRInt32     aReadType ) {

  nsresult            rv;

  nsP3PDataStructTag *pDataStructTag = nsnull;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pDataStructTag );

  if (NS_SUCCEEDED( rv )) {

    if (pDataStructTag->mStructRefDataSchemaURISpec.Length( ) > 0) {
      // DataSchema URI spec is present, create a complete URI spec
      nsCOMPtr<nsIURI>  pURI;

      rv = NS_NewURI( getter_AddRefs( pURI ),
                      pDataStructTag->mStructRefDataSchemaURISpec,
                      mURI );

      if (NS_SUCCEEDED( rv )) {
        nsXPIDLCString  xcsURISpec;

        rv = pURI->GetSpec( getter_Copies( xcsURISpec ) );

        if (NS_SUCCEEDED( rv )) {
          pDataStructTag->mStructRefDataSchemaURISpec.AssignWithConversion((const char *)xcsURISpec );

          rv = AddDataSchema( pDataStructTag->mStructRefDataSchemaURISpec,
                              aReadType );
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PDataSchema:  %s ProcessDataStructTagDataSchema, pURI->GetSpec failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PDataSchema:  %s ProcessDataStructTagDataSchema, NS_NewURI failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      pDataStructTag->mStructRefDataSchemaURISpec = mURISpec;
    }

    NS_RELEASE( pDataStructTag );
  }

  return rv;
}

// P3P Data Schema: ProcessDataDefTagDataSchema
//
// Function:  If a DataSchema is specified, add it to the list of DataSchemas to read
//
// Parms:     1. In     The <DATA-DEF> Tag object
//            2. In     The type of read performed
//
NS_METHOD
nsP3PDataSchema::ProcessDataDefTagDataSchema( nsIP3PTag  *aTag,
                                              PRInt32     aReadType ) {

  nsresult         rv;

  nsP3PDataDefTag *pDataDefTag = nsnull;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pDataDefTag );

  if (NS_SUCCEEDED( rv )) {

    if (pDataDefTag->mStructRefDataSchemaURISpec.Length( ) > 0) {
      // DataSchema URI spec is present, create a complete URI spec
      nsCOMPtr<nsIURI>  pURI;

      rv = NS_NewURI( getter_AddRefs( pURI ),
                      pDataDefTag->mStructRefDataSchemaURISpec,
                      mURI );

      if (NS_SUCCEEDED( rv )) {
        nsXPIDLCString  xcsURISpec;

        rv = pURI->GetSpec( getter_Copies( xcsURISpec ) );

        if (NS_SUCCEEDED( rv )) {
          pDataDefTag->mStructRefDataSchemaURISpec.AssignWithConversion((const char *)xcsURISpec );

          rv = AddDataSchema( pDataDefTag->mStructRefDataSchemaURISpec,
                              aReadType );
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PDataSchema:  %s ProcessDataStructTagDataSchema, pURI->GetSpec failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PDataSchema:  %s ProcessDataStructTagDataSchema, NS_NewURI failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      pDataDefTag->mStructRefDataSchemaURISpec = mURISpec;
    }

    NS_RELEASE( pDataDefTag );
  }

  return rv;
}

// P3P Data Schema: AddDataSchema
//
// Function:  Process the DataSchema.
//
// Parms:     1. In     The DataSchema URI spec to be read
//            2. In     The type of read performed
//
NS_METHOD
nsP3PDataSchema::AddDataSchema( nsString&  aDataSchemaURISpec,
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
            ("P3PDataSchema:  %s AddDataSchema, adding DataSchema %s.\n", (const char *)mcsURISpec, (const char *)csDataSchemaURISpec) );

    rv = mP3PService->GetCachedDataSchema( aDataSchemaURISpec,
                                           getter_AddRefs( pDataSchema ) );

    if (NS_SUCCEEDED( rv ) && !pDataSchema) {
      // Don't have a cached version, create a new one
      rv = NS_NewP3PDataSchema( aDataSchemaURISpec,
                                getter_AddRefs( pDataSchema ) );

      if (NS_FAILED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PDataSchema:  %s AddDataSchema, error creating DataSchema %s.\n", (const char *)mcsURISpec, (const char *)csDataSchemaURISpec) );
      }
    }
    else if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PDataSchema:  %s AddDataSchema, error getting DataSchema %s.\n", (const char *)mcsURISpec, (const char *)csDataSchemaURISpec) );
    }

    if (NS_SUCCEEDED( rv )) {
      // Add to the collection of DataSchemas
      mDataSchemas.Put(&keyDataSchemaURISpec,
                        pDataSchema );

      // Process remote DataSchema
      rv = pDataSchema->Read( aReadType,
                              mURI,
                              this,
                              nsnull );

      if (NS_SUCCEEDED( rv )) {

        if (rv == P3P_PRIVACY_IN_PROGRESS) {
          rv = mDataSchemasToRead.AppendString( aDataSchemaURISpec ) ? NS_OK : NS_ERROR_FAILURE;

          if (NS_FAILED( rv )) {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PDataSchema:  %s AddDataSchema, mDataSchemasToRead.AppendString failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
      }
    }
  }

  return rv;
}

// P3P DataSchema: SetNotValid
//
// Function:  Sets the not valid indicator and removes the DataSchema from cache (if present).
//
// Parms:     None
//
NS_METHOD_( void )
nsP3PDataSchema::SetNotValid( ) {

#ifdef DEBUG_P3P
  { printf( "P3P:    DataSchema File %s not valid\n", (const char *)mcsURISpec );
  }
#endif

  PR_LOG( gP3PLogModule,
          PR_LOG_ERROR,
          ("P3PDataSchema:  %s SetNotValid, DataSchema not valid.\n", (const char *)mcsURISpec) );

  mDocumentValid = PR_FALSE;

  if (!mRedirected && !mInlineRead) {
    // DataSchema is not valid and may have been previously cached, "uncache" it
#ifdef DEBUG_P3P
    { printf( "P3P:  Uncaching DataSchema Object: %s\n", (const char *)mcsURISpec );
    }
#endif

    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PDataSchema:  %s ProcessContent, uncaching DataSchema.\n", (const char *)mcsURISpec) );

    mP3PService->UncacheDataSchema( mURISpec,
                                    this );
  }

  return;
}

// P3P Data Schema: BuildDataModel
//
// Function:  Build the DataStruct model for this DataSchema.
//
// Parms:     None
//
NS_METHOD_( void )
nsP3PDataSchema::BuildDataModel( ) {

  nsresult                       rv;

  nsAutoString                   sNullString;

  nsP3PDataSchemaTag            *pDataSchemaTag = nsnull;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PDataSchema:  %s BuildDataModel, building data model.\n", (const char *)mcsURISpec) );

  // Pass 1: Construct the data model tree

  // Create the top element of the tree
  rv = NS_NewP3PDataStruct( PR_TRUE,
                            sNullString,
                            sNullString,
                            sNullString,
                            sNullString,
                            sNullString,
                            nsnull,
                            nsnull,
                            getter_AddRefs( mDataStruct ) );

  if (NS_SUCCEEDED( rv )) {
    rv = mTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                     (void **)&pDataSchemaTag );

    if (NS_SUCCEEDED( rv )) {
      rv = NS_NewP3PSimpleEnumerator( pDataSchemaTag->mDataStructTags,
                                      getter_AddRefs( pEnumerator ) );

      if (NS_SUCCEEDED( rv )) {
        rv = pEnumerator->HasMoreElements(&bMoreTags );

        while (NS_SUCCEEDED( rv ) && bMoreTags) {
          rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

          if (NS_SUCCEEDED( rv )) {
            rv = BuildDataStructTagDataStruct( pTag );
          }

          if (NS_SUCCEEDED( rv )) {
            rv = pEnumerator->HasMoreElements(&bMoreTags );
          }
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PDataSchema:  %s BuildDataModel, NS_NewP3PSimpleEnumerator for <DATA-STRUCT> tags failed - %X.\n", (const char *)mcsURISpec, rv) );
      }

      if (NS_SUCCEEDED( rv )) {
        rv = NS_NewP3PSimpleEnumerator( pDataSchemaTag->mDataDefTags,
                                        getter_AddRefs( pEnumerator ) );

        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreTags );

          while (NS_SUCCEEDED( rv ) && bMoreTags) {
            rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

            if (NS_SUCCEEDED( rv )) {
              rv = BuildDataDefTagDataStruct( pTag );
            }

            if (NS_SUCCEEDED( rv )) {
              rv = pEnumerator->HasMoreElements(&bMoreTags );
            }
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PDataSchema:  %s BuildDataModel, NS_NewP3PSimpleEnumerator for <DATA-DEF> tags failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }

      NS_RELEASE( pDataSchemaTag );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PDataSchema:  %s BuildDataModel, NS_NewP3PDataStruct failed - %X.\n", (const char *)mcsURISpec, rv) );
  }


  // Pass 2: Resolve non-primitive references

  if (NS_SUCCEEDED( rv )) {
    rv = ResolveReferences( mDataStruct );
  }

  if (NS_SUCCEEDED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PDataSchema:  %s BuildDataModel, data model created.\n", (const char *)mcsURISpec) );
  }
  else {
    SetNotValid( );
  }

  return;
}

// P3P Data Schema: BuildDataStructTagDataStruct
//
// Function:  Build a DataStruct for a <DATA-STRUCT> tag.
//
// Parms:     1. In     The <DATA-STRUCT> Tag object
//
NS_METHOD
nsP3PDataSchema::BuildDataStructTagDataStruct( nsIP3PTag  *aTag ) {

  nsresult            rv;

  nsP3PDataStructTag *pDataStructTag = nsnull;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pDataStructTag );

  if (NS_SUCCEEDED( rv )) {
    rv = CreateDataStruct( pDataStructTag->mName,
                           pDataStructTag->mStructRef,
                           pDataStructTag->mStructRefDataSchemaURISpec,
                           pDataStructTag->mStructRefDataStructName,
                           pDataStructTag->mShortDescription,
                           pDataStructTag->mCategoriesTag,
                           pDataStructTag->mLongDescriptionTag );

    NS_RELEASE( pDataStructTag );
  }

  return rv;
}

// P3P Data Schema: BuildDataDefTagDataStruct
//
// Function:  Build a DataStruct for a <DATA-DEF> tag.
//
// Parms:     1. In     The <DATA-DEF> Tag object
//
NS_METHOD
nsP3PDataSchema::BuildDataDefTagDataStruct( nsIP3PTag  *aTag ) {

  nsresult         rv;

  nsP3PDataDefTag *pDataDefTag = nsnull;


  rv = aTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                   (void **)&pDataDefTag );

  if (NS_SUCCEEDED( rv )) {
    rv = CreateDataStruct( pDataDefTag->mName,
                           pDataDefTag->mStructRef,
                           pDataDefTag->mStructRefDataSchemaURISpec,
                           pDataDefTag->mStructRefDataStructName,
                           pDataDefTag->mShortDescription,
                           pDataDefTag->mCategoriesTag,
                           pDataDefTag->mLongDescriptionTag );

    NS_RELEASE( pDataDefTag );
  }

  return rv;
}

// P3P Data Schema: CreateDataStruct
//
// Function:  Creates a complete DataStruct object tree and adds it to the DataStruct model
//
// Parms:     1. In     The DataStruct name
//            2. In     The "structref" attribute value
//            3. In     The DataSchema URI spec of the "structref" attribute value
//            4. In     The DataStruct name  of the "structref" attribute value
//            5. In     The "short-description" attribute value
//            6. In     The <CATEGORIES> Tag object
//            7. In     The <LONG-DESCRIPTION> Tag object
//
NS_METHOD
nsP3PDataSchema::CreateDataStruct( nsString&   aDataStructName,
                                   nsString&   aStructRef,
                                   nsString&   aStructRefDataSchemaURISpec,
                                   nsString&   aStructRefDataStructName,
                                   nsString&   aShortDescription,
                                   nsIP3PTag  *aCategoriesTag,
                                   nsIP3PTag  *aLongDescriptionTag ) {

  nsresult                    rv = NS_OK;

  nsCAutoString               csName;

  PRInt32                     iQualifier;

  nsP3PCategoriesTag         *pCategoriesTag     = nsnull;

  nsCOMPtr<nsISupportsArray>  pCategoryValueTags;

  nsCOMPtr<nsIP3PDataStruct>  pDataStruct,
                              pNewDataStruct,
                              pChildDataStruct;

  nsAutoString                sDataStructName,
                              sDataStructNodeName,
                              sDataStructWorkName;


  csName.AssignWithConversion( aDataStructName );
  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PDataSchema:  %s CreateDataStruct, creating data element %s.\n", (const char *)mcsURISpec, (const char *)csName) );

#ifdef DEBUG_P3P
  { printf( "P3P:    Creating data struct: %s\n", (const char *)csName );
  }
#endif

  if (aCategoriesTag) {
    rv = aCategoriesTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                               (void **)&pCategoriesTag );

    if (NS_SUCCEEDED( rv )) {
      pCategoryValueTags = pCategoriesTag->mCategoryValueTags;

      NS_RELEASE( pCategoriesTag );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Go through the data model tree to determine where to place the data struct
    // building nodes along the way
    //   ie. dynamic.http.useragent will require dynamic and http to be created
    //       before useragent can be
    pDataStruct         = mDataStruct;
    sDataStructWorkName = aDataStructName;
    do {
      sDataStructName = sDataStructWorkName;

      // Split the data struct name into a node name and the remainder
      //   ie. dynamic.http.useragent => dynamic and http.useragent
      iQualifier = sDataStructName.Find( "." );
      sDataStructName.Left( sDataStructNodeName,
                            iQualifier );
      sDataStructName.Right( sDataStructWorkName,
                             sDataStructName.Length( ) - (iQualifier + 1) );

      // Look for the node name
      rv = FindChildDataStruct( sDataStructNodeName,
                                pDataStruct,
                                getter_AddRefs( pChildDataStruct ) );

      if (NS_SUCCEEDED( rv )) {

        if (pChildDataStruct) {
          // Found an existing node, update the node with the (new) categories
          rv = UpdateDataStruct( pCategoryValueTags,
                                 pChildDataStruct );

          if (NS_SUCCEEDED( rv ) && !pChildDataStruct->IsExplicitlyCreated( ) && (iQualifier < 0)) {
            // This is an explicit creation of a previously implicitly created
            // node, so replace it (obtain category values from current node)
            rv = ReplaceDataStruct( sDataStructNodeName,
                                    aStructRef,
                                    aStructRefDataSchemaURISpec,
                                    aStructRefDataStructName,
                                    aShortDescription,
                                    aLongDescriptionTag,
                                    pDataStruct,
                                    pChildDataStruct,
                                    getter_AddRefs( pNewDataStruct ) );
          }
          else {
            pNewDataStruct = pChildDataStruct;
          }
        }
        else {
          // Unless it is the last node, we're implicitly creating all the nodes
          // in between the top of the tree and the last node
          rv = NewDataStruct( (iQualifier >= 0) ? PR_FALSE : PR_TRUE,
                              sDataStructNodeName,
                              aStructRef,
                              aStructRefDataSchemaURISpec,
                              aStructRefDataStructName,
                              aShortDescription,
                              pCategoryValueTags,
                              aLongDescriptionTag,
                              pDataStruct,
                              getter_AddRefs( pNewDataStruct ) );
        }

        pDataStruct = pNewDataStruct;
      }
    } while (NS_SUCCEEDED( rv ) && (iQualifier >= 0));
  }

  return rv;
}

// P3P Data Schema: FindChildDataStruct
//
// Function:  Searches a DataStruct object for a child with the specified DataStruct name.
//
// Parms:     1. In     The DataStruct name
//            2. In     The DataStruct object
//            3. Out    The child DataStruct object
//
NS_METHOD
nsP3PDataSchema::FindChildDataStruct( nsString&          aDataStructName,
                                      nsIP3PDataStruct  *aParent,
                                      nsIP3PDataStruct **aChild ) {

  nsresult                       rv;

  nsCOMPtr<nsISupportsArray>     pChildren;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PDataStruct>     pDataStruct;

  PRBool                         bMoreDataStructs,
                                 bFound;

  nsAutoString                   sDataStructName;


  NS_ENSURE_ARG_POINTER( aChild );

  *aChild = nsnull;

  rv = aParent->GetChildren( getter_AddRefs( pChildren ) );

  if (NS_SUCCEEDED( rv )) {
    rv = NS_NewP3PSimpleEnumerator( pChildren,
                                    getter_AddRefs( pEnumerator ) );

    if (NS_SUCCEEDED( rv )) {
      bFound = PR_FALSE;
      rv = pEnumerator->HasMoreElements(&bMoreDataStructs );

      while (NS_SUCCEEDED( rv ) && !bFound && bMoreDataStructs) {
        rv = pEnumerator->GetNext( getter_AddRefs( pDataStruct ) );

        if (NS_SUCCEEDED( rv )) {
          pDataStruct->GetName( sDataStructName );

          if (sDataStructName == aDataStructName) {
            bFound = PR_TRUE;

            rv = pDataStruct->QueryInterface( NS_GET_IID( nsIP3PDataStruct ),
                                     (void **)aChild );
          }
        }

        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreDataStructs );
        }
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PDataSchema:  %s FindChildDataStruct, NS_NewP3PSimpleEnumerator failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PDataSchema:  %s FindChildDataStruct, aParent->GetChildren failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}
                                    
// P3P Data Schema: UpdateDataStruct
//
// Function:  Updates a DataStruct object with the supplied category values.
//
// Parms:     1. In     The category value Tag objects
//            2. In     The DataStruct object to be updated
//
NS_METHOD
nsP3PDataSchema::UpdateDataStruct( nsISupportsArray  *aCategoryValueTags,
                                   nsIP3PDataStruct  *aDataStruct ) {

  nsresult                    rv = NS_OK;

  nsCOMPtr<nsIP3PDataStruct>  pParentDataStruct;


  if (aCategoryValueTags) {
    rv = aDataStruct->AddCategoryValues( aCategoryValueTags );

    if (NS_FAILED( rv )) {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PDataSchema:  %s UpdateDataStruct, aDataStruct->AddCategoryValues failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  return rv;
}

// P3P Data Schema: UpdateDataStructBelow
//
// Function:  Update a DataStruct object and it's children with the supplied category values.
//
// Parms:     1. In     The category value Tag objects
//            2. In     The DataStruct object to be updated
//
NS_METHOD
nsP3PDataSchema::UpdateDataStructBelow( nsISupportsArray  *aCategoryValueTags,
                                        nsIP3PDataStruct  *aDataStruct ) {

  nsresult                       rv = NS_OK;

  nsCOMPtr<nsIP3PTag>            pTag;

  nsCOMPtr<nsISupportsArray>     pChildren;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PDataStruct>     pDataStruct;

  PRBool                         bMoreDataStructs;


  if (aCategoryValueTags) {
    // Update this node with the category values
    rv = UpdateDataStruct( aCategoryValueTags,
                           aDataStruct );

    if (NS_SUCCEEDED( rv )) {
      // Update all of the children with the category values
      rv = aDataStruct->GetChildren( getter_AddRefs( pChildren ) );

      if (NS_SUCCEEDED( rv )) {
        rv = NS_NewP3PSimpleEnumerator( pChildren,
                                        getter_AddRefs( pEnumerator ) );

        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreDataStructs );

          while (NS_SUCCEEDED( rv ) && bMoreDataStructs) {
            rv = pEnumerator->GetNext( getter_AddRefs( pDataStruct ) );

            if (NS_SUCCEEDED( rv )) {
              rv = UpdateDataStructBelow( aCategoryValueTags,
                                          pDataStruct );
            }

            if (NS_SUCCEEDED( rv )) {
              rv = pEnumerator->HasMoreElements(&bMoreDataStructs );
            }
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PDataSchema:  %s UpdateDataStructBelow, NS_NewP3PSimpleEnumerator failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PDataSchema:  %s UpdateDataStructBelow, aDataStruct->GetChildren failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
  }

  return rv;
}

// P3P Data Schema: ReplaceDataStruct
//
// Function:  Replace a child DataStruct object.
//
// Parms:     1. In     The DataStruct name
//            2. In     The "structref" attribute value
//            3. In     The DataSchema URI spec of the "structref" attribute value
//            4. In     The DataStruct name  of the "structref" attribute value
//            5. In     The "short-description" attribute value
//            6. In     The <LONG-DESCRIPTION> Tag object
//            7. In     The parent DataStruct object
//            8. In     The existing child DataStruct object
//            9. Out    The new child DataStruct object
//
NS_METHOD
nsP3PDataSchema::ReplaceDataStruct( nsString&          aDataStructName,
                                    nsString&          aStructRef,
                                    nsString&          aStructRefDataSchemaURISpec,
                                    nsString&          aStructRefDataStructName,
                                    nsString&          aShortDescription,
                                    nsIP3PTag         *aLongDescriptionTag,
                                    nsIP3PDataStruct  *aParent,
                                    nsIP3PDataStruct  *aOldChild,
                                    nsIP3PDataStruct **aNewChild ) {

  nsresult                    rv;

  nsCOMPtr<nsISupportsArray>  pCategoryValueTags;


  NS_ENSURE_ARG_POINTER( aNewChild );

  *aNewChild = nsnull;

  // Use all of the category values existing in the implicit node
  rv = aOldChild->GetCategoryValues( getter_AddRefs( pCategoryValueTags ) );

  if (NS_SUCCEEDED( rv )) {
    // Remove the old child
    rv = aParent->RemoveChild( aOldChild );

    if (NS_SUCCEEDED( rv )) {
      // Add the new child
      rv = NewDataStruct( PR_TRUE,
                          aDataStructName,
                          aStructRef,
                          aStructRefDataSchemaURISpec,
                          aStructRefDataStructName,
                          aShortDescription,
                          pCategoryValueTags,
                          aLongDescriptionTag,
                          aParent,
                          aNewChild );
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PDataSchema:  %s ReplaceDataStruct, aParent->RemoveChild failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PDataSchema:  %s ReplaceDataStruct, aOldChild->GetCategoryValues failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P Data Schema: Create a data struct as a child
//
// Function:  Create a child DataStruct object.
//
//            A DataStruct is not explicitly created when a parent has to be implicitly created for the DataStruct tree.
//              ie. dynamic.http.useragent requires dynamic and http to be implicitly created if they do not exist,
//                                         but useragent is explicitly created.
//
// Parms:     1. In     An indicator as to whether the DataStruct is explicitly created
//            2. In     The DataStruct name
//            3. In     The "structref" attribute value
//            4. In     The DataSchema URI spec of the "structref" attribute value
//            5. In     The DataStruct name  of the "structref" attribute value
//            6. In     The "short-description" attribute value
//            7. In     The <CATEGORIES> Tag object
//            8. In     The <LONG-DESCRIPTION> Tag object
//            9. In     The parent DataStruct object
//           10. Out    The new child DataStruct object
//
NS_METHOD
nsP3PDataSchema::NewDataStruct( PRBool             aExplicitCreate,
                                nsString&          aDataStructName,
                                nsString&          aStructRef,
                                nsString&          aStructRefDataSchemaURISpec,
                                nsString&          aStructRefDataStructName,
                                nsString&          aShortDescription,
                                nsISupportsArray  *aCategoryValueTags,
                                nsIP3PTag         *aLongDescriptionTag,
                                nsIP3PDataStruct  *aParent,
                                nsIP3PDataStruct **aChild ) {

  nsresult                    rv = NS_OK;

  nsAutoString                sStructRef,
                              sStructRefDataSchemaURISpec,
                              sStructRefDataStructName,
                              sShortDescription;

  nsCOMPtr<nsIP3PTag>         pLongDescriptionTag;


  NS_ENSURE_ARG_POINTER( aChild );

  *aChild = nsnull;

  if (aExplicitCreate) {
    // If explicitly creating this node, use the tag information
    sStructRef                  = aStructRef;
    sStructRefDataSchemaURISpec = aStructRefDataSchemaURISpec;
    sStructRefDataStructName    = aStructRefDataStructName;
    sShortDescription           = aShortDescription;
    pLongDescriptionTag         = aLongDescriptionTag;
  }

  rv = NS_NewP3PDataStruct( aExplicitCreate,
                            aDataStructName,
                            sStructRef,
                            sStructRefDataSchemaURISpec,
                            sStructRefDataStructName,
                            sShortDescription,
                            aCategoryValueTags,
                            pLongDescriptionTag,
                            aChild );

  if (NS_SUCCEEDED( rv )) {

    if (aParent) {
      rv = aParent->AddChild( *aChild );

      if (NS_FAILED( rv )) {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PDataSchema:  %s NewDataStruct, aParent->AddChild failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
  }
  else {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PDataSchema:  %s NewDataStruct, NS_NewP3PDataStruct failed - %X.\n", (const char *)mcsURISpec, rv) );
  }

  return rv;
}

// P3P Data Schema: ResolveReferences
//
// Function:  Resolve a DataStruct non-primitive reference.
//
//            This is accomplished by cloning the non-primitive DataStructs references
//            and adding them as children to the DataStruct model for this DataSchema.
//            Then this function is recursively called for each child.
//
// Parms:     1. In     The DataStruct object
//
NS_METHOD
nsP3PDataSchema::ResolveReferences( nsIP3PDataStruct  *aDataStruct ) {

  nsresult                       rv;

  nsAutoString                   sName;
  nsCAutoString                  csName;

  nsAutoString                   sDataSchemaURISpec,
                                 sDataStructName;

  nsCOMPtr<nsIP3PDataSchema>     pDataSchema;

  nsCOMPtr<nsIP3PDataStruct>     pDataStruct;

  nsCOMPtr<nsISupportsArray>     pChildren;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  PRBool                         bMoreDataStructs;

  nsCOMPtr<nsISupportsArray>     pCategoryValueTags;


  if (!aDataStruct->IsReferenceResolved( )) {
    // The data struct is not a basic struct so get the referenced data struct
    // and extend the data model tree
    //   ie. user.name with a structref of personname would add all of the
    //       data structs under the personname node:
    //         user.name.prefix, user.name.given, user.name.middle, etc. 
    aDataStruct->GetFullName( sName );
    csName.AssignWithConversion( sName );
    PR_LOG( gP3PLogModule,
            PR_LOG_NOTICE,
            ("P3PDataSchema:  %s ResolveReferences, resolving references in data struct %s.\n", (const char *)mcsURISpec, (const char *)csName) );

#ifdef DEBUG_P3P
    { printf( "P3P:    Resolving references in data struct: %s\n", (const char *)csName );
    }
#endif

    aDataStruct->GetDataSchemaURISpec( sDataSchemaURISpec );
    aDataStruct->GetDataStructName( sDataStructName );

    if (sDataSchemaURISpec == mURISpec) {
      pDataSchema = this;
      rv          = NS_OK;
    }
    else {
      nsCOMPtr<nsISupports>  pEntry;
      nsStringKey            keyDataSchemaURISpec( sDataSchemaURISpec );

      pEntry      = getter_AddRefs( mDataSchemas.Get(&keyDataSchemaURISpec ) );
      pDataSchema = do_QueryInterface( pEntry,
                                      &rv );
    }

    if (NS_SUCCEEDED( rv )) {
      // Find the referenced data struct
      rv = pDataSchema->FindDataStruct( sDataStructName,
                                        getter_AddRefs( pDataStruct ) );

      if (NS_SUCCEEDED( rv )) {
        // Copy the referenced data struct tree and add it to this data struct node
        rv = pDataStruct->CloneChildren( aDataStruct );

        if (NS_SUCCEEDED( rv )) {
          // Indicate that this node has resolved it's reference
          aDataStruct->SetReferenceResolved( );

          // Get the category values and update all of the children with them
          rv = aDataStruct->GetCategoryValues( getter_AddRefs( pCategoryValueTags ) );

          if (NS_SUCCEEDED( rv )) {
            rv = pDataStruct->GetChildren( getter_AddRefs( pChildren ) );

            if (NS_SUCCEEDED( rv )) {
              rv = NS_NewP3PSimpleEnumerator( pChildren,
                                              getter_AddRefs( pEnumerator ) );

              if (NS_SUCCEEDED( rv )) {
                rv = pEnumerator->HasMoreElements(&bMoreDataStructs );

                while (NS_SUCCEEDED( rv ) && bMoreDataStructs) {
                  rv = pEnumerator->GetNext( getter_AddRefs( pDataStruct ) );

                  if (NS_SUCCEEDED( rv )) {
                    // Update the child and it's children with the category values
                    rv = UpdateDataStructBelow( pCategoryValueTags,
                                                pDataStruct );
                  }

                  if (NS_SUCCEEDED( rv )) {
                    rv = pEnumerator->HasMoreElements(&bMoreDataStructs );
                  }
                }
              }
              else {
                PR_LOG( gP3PLogModule,
                        PR_LOG_ERROR,
                        ("P3PDataSchema:  %s ResolveReferences, NS_NewP3PSimpleEnumerator failed - %X.\n", (const char *)mcsURISpec, rv) );
              }
            }
            else {
              PR_LOG( gP3PLogModule,
                      PR_LOG_ERROR,
                      ("P3PDataSchema:  %s ResolveReferences, pDataStruct->GetChildren failed - %X.\n", (const char *)mcsURISpec, rv) );
            }
          }
          else {
            PR_LOG( gP3PLogModule,
                    PR_LOG_ERROR,
                    ("P3PDataSchema:  %s ResolveReferences, aDataStruct->GetCategoryValues failed - %X.\n", (const char *)mcsURISpec, rv) );
          }
        }
        else {
          PR_LOG( gP3PLogModule,
                  PR_LOG_ERROR,
                  ("P3PDataSchema:  %s ResolveReferences, pDataStruct->CloneChildren failed - %X.\n", (const char *)mcsURISpec, rv) );
        }
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PDataSchema:  %s ResolveReferences, unable to obtain necessary DataSchema.\n", (const char *)mcsURISpec) );
    }
  }

  if (NS_SUCCEEDED( rv )) {
    // Now resolve the references within all of this data structs children
    rv = aDataStruct->GetChildren( getter_AddRefs( pChildren ) );

    if (NS_SUCCEEDED( rv )) {
      rv = NS_NewP3PSimpleEnumerator( pChildren,
                                      getter_AddRefs( pEnumerator ) );

      if (NS_SUCCEEDED( rv )) {
        rv = pEnumerator->HasMoreElements(&bMoreDataStructs );

        while (NS_SUCCEEDED( rv ) && bMoreDataStructs) {
          rv = pEnumerator->GetNext( getter_AddRefs( pDataStruct ) );

          if (NS_SUCCEEDED( rv )) {
            rv = ResolveReferences( pDataStruct );
          }

          if (NS_SUCCEEDED( rv )) {
            rv = pEnumerator->HasMoreElements(&bMoreDataStructs );
          }
        }
      }
      else {
        PR_LOG( gP3PLogModule,
                PR_LOG_ERROR,
                ("P3PDataSchema:  %s ResolveReferences, NS_NewP3PSimpleEnumerator failed - %X.\n", (const char *)mcsURISpec, rv) );
      }
    }
    else {
      PR_LOG( gP3PLogModule,
              PR_LOG_ERROR,
              ("P3PDataSchema:  %s ResolveReferences, aDataStruct->GetChildren failed - %X.\n", (const char *)mcsURISpec, rv) );
    }
  }

  return rv;
}


// ****************************************************************************
// nsIP3PXMLListener routines
// ****************************************************************************

// P3P Data Schema: DocumentComplete
//
// Function:  Notification of the completion of a DataSchema URI.
//
// Parms:     1. In     The DataSchema URI spec that has completed
//            2. In     The data supplied on the intial processing request
//
NS_IMETHODIMP
nsP3PDataSchema::DocumentComplete( nsString&     aProcessedURISpec,
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
            ("P3PDataSchema:  %s DocumentComplete, %s complete.\n", (const char *)mcsURISpec, (const char *)csURISpec) );

    // Remove the DataSchema from the list of processing DataSchemas
    mDataSchemasToRead.RemoveString( aProcessedURISpec );

    // Check if this DataSchema is now complete
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
