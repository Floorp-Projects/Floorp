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

#include "nsP3PDataStruct.h"
#include "nsP3PTags.h"
#include "nsP3PSimpleEnumerator.h"

#include <nsIComponentManager.h>


// ****************************************************************************
// nsP3PDataStruct Implementation routines
// ****************************************************************************

// P3P Data Struct: nsISupports
NS_IMPL_ISUPPORTS1( nsP3PDataStruct, nsIP3PDataStruct );

// P3P Data Struct: Creation routine
NS_METHOD
NS_NewP3PDataStruct( PRBool             aExplicitlyCreated,
                     nsString&          aName,
                     nsString&          aStructRef,
                     nsString&          aStructRefDataSchemaURISpec,
                     nsString&          aStructRefDataStructName,
                     nsString&          aShortDescription,
                     nsISupportsArray  *aCategoryValueTags,
                     nsIP3PTag         *aLongDescriptionTag,
                     nsIP3PDataStruct **aDataStruct ) {

  nsresult         rv;

  nsP3PDataStruct *pNewDataStruct = nsnull;


  NS_ENSURE_ARG_POINTER( aDataStruct );

  pNewDataStruct = new nsP3PDataStruct( );

  if (pNewDataStruct) {
    NS_ADDREF( pNewDataStruct );

    rv = pNewDataStruct->Init( aExplicitlyCreated,
                               aName,
                               aStructRef,
                               aStructRefDataSchemaURISpec,
                               aStructRefDataStructName,
                               aShortDescription,
                               aCategoryValueTags,
                               aLongDescriptionTag );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewDataStruct->QueryInterface( NS_GET_IID( nsIP3PDataStruct ),
                                  (void **)aDataStruct );
    }

    NS_RELEASE( pNewDataStruct );
  }
  else {
    NS_ASSERTION( 0, "P3P:  DataStruct unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Data Struct: Constructor
nsP3PDataStruct::nsP3PDataStruct( )
  : mExplicitlyCreated( PR_FALSE ),
    mReferenceResolved( PR_FALSE ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Data Struct: Destructor
nsP3PDataStruct::~nsP3PDataStruct( ) {
}

// P3P Data Struct: Init
//
// Function:  Initalizes the DataStruct object.
//
//            A DataStruct is not explicitly created when a parent has to be implicitly created for the DataStruct tree.
//              ie. dynamic.http.useragent requires dynamic and http to be implicitly created if they do not exist,
//                                         but useragent is explicitly created.
//
// Parms:     1. In     An indicator as to whether this DataStruct is being explicitly created
//            2. In     The DataStruct name
//            3. In     The "structref" attribute value
//            4. In     The DataSchema URI spec portion of the "structref" attribute value
//            5. In     The DataStruct name  portion of the "structref" attribute value
//            6. In     The "short-description" attribute value
//            7. In     The category value Tag objects
//            8. In     The <LONG-DESCRIPTION> Tag object
//            9. In     The Parent DataStruct object
//
NS_METHOD
nsP3PDataStruct::Init( PRBool             aExplicitlyCreated,
                       nsString&          aName,
                       nsString&          aStructRef,
                       nsString&          aStructRefDataSchemaURISpec,
                       nsString&          aStructRefDataStructName,
                       nsString&          aShortDescription,
                       nsISupportsArray  *aCategoryValueTags,
                       nsIP3PTag         *aLongDescriptionTag ) {

  nsresult  rv;


  mExplicitlyCreated  = aExplicitlyCreated;
  mName               = aName;
  mStructRef          = aStructRef;
  mDataSchemaURISpec  = aStructRefDataSchemaURISpec;
  mDataStructName     = aStructRefDataStructName;
  mShortDescription   = aShortDescription;
  mLongDescriptionTag = aLongDescriptionTag;

  // Set the reference resolved indicator
  mReferenceResolved = IsBasicStruct( );

  // Create a container for the data struct categories
  mCategoryValueTags = do_CreateInstance( NS_SUPPORTSARRAY_CONTRACTID,
                                         &rv );

  if (NS_SUCCEEDED( rv ) && aCategoryValueTags) {
    // Save the categories
    rv = AddCategoryValues( aCategoryValueTags );
  }

  if (NS_SUCCEEDED( rv )) {
    // Create a container for the children
    mChildren = do_CreateInstance( NS_SUPPORTSARRAY_CONTRACTID,
                                  &rv );
  }

  return rv;
}


// ****************************************************************************
// nsIP3PDataStruct routines
// ****************************************************************************

// P3P Data Struct: IsExplicitlyCreated
//
// Function:  Returns an indicator as to whether the DataStruct object was explicitly created.
//
//            A DataStruct is not explicitly created when a parent has to be implicitly created for the DataStruct tree.
//              ie. dynamic.http.useragent requires dynamic and http to be implicitly created if they do not exist,
//                                         but useragent is explicitly created.
//
// Parms:     None
//
NS_IMETHODIMP_( PRBool )
nsP3PDataStruct::IsExplicitlyCreated( ) {
  
  return mExplicitlyCreated;
}

// P3P Data Struct: GetName
//
// Function:  Gets the name of the DataStruct object.
//
// Parms:     1. Out    The name of the DataStruct object
//
NS_IMETHODIMP_( void )
nsP3PDataStruct::GetName( nsString&  aName ) {
  
  aName = mName;

  return;
}

// P3P Data Struct: GetFullName
//
// Function:  Gets the full name of the DataStruct object (this includes the parent DataStruct names).
//
// Parms:     1. Out    The full name of the DataStruct object
//
NS_IMETHODIMP_( void )
nsP3PDataStruct::GetFullName( nsString&  aName ) {
  
  if (mParent) {
    mParent->GetFullName( aName );

    if (aName.Length( ) > 0) {
      aName.AppendWithConversion( "." );
    }

    aName += mName;
  }
  else {
    aName = mName;
  }

  return;
}

// P3P Data Struct: GetStructRef
//
// Function:  Gets the "structref" attribute of the DataStruct object.
//
// Parms:     1. Out    The "structref" attribute of the DataStruct object
//
NS_IMETHODIMP_( void )
nsP3PDataStruct::GetStructRef( nsString&  aStructRef ) {
  
  aStructRef = mStructRef;

  return;
}

// P3P Data Struct: GetDataSchemaURISpec
//
// Function:  Gets the DataSchema URI spec portion of the "structref" attribute of the DataStruct object.
//
// Parms:     1. Out    The DataSchema URI spec of the "structref" attribute of the DataStruct object
//
NS_IMETHODIMP_( void )
nsP3PDataStruct::GetDataSchemaURISpec( nsString&  aDataSchemaURISpec ) {
  
  aDataSchemaURISpec = mDataSchemaURISpec;

  return;
}

// P3P Data Struct: GetDataStruct
//
// Function:  Gets the DataSchema struct of the "structref" attribute of the DataStruct object.
//
// Parms:     1. Out    The DataSchema struct of the "structref" attribute of the DataStruct object
//
NS_IMETHODIMP_( void )
nsP3PDataStruct::GetDataStructName( nsString&  aDataStructName ) {

  aDataStructName = mDataStructName;

  return;
}

// P3P Data Struct: GetShortDescription
//
// Function:  Gets the "short-description" attribute of the DataStruct object.
//
// Parms:     1. Out    The "short-description" attribute of the DataStruct object
//
NS_IMETHODIMP_( void )
nsP3PDataStruct::GetShortDescription( nsString&  aShortDescription ) {
  
  aShortDescription = mShortDescription;

  return;
}

// P3P Data Struct: GetCategoryValues
//
// Function:  Gets the category value Tag objects of the DataStruct object.
//
// Parms:     1. Out    The category value Tag objects of the DataStruct object
//
NS_IMETHODIMP
nsP3PDataStruct::GetCategoryValues( nsISupportsArray **aCategoryValueTags ) {

  NS_ENSURE_ARG_POINTER( aCategoryValueTags );

  *aCategoryValueTags = mCategoryValueTags;

  NS_IF_ADDREF( *aCategoryValueTags );

  return NS_OK;
}

// P3P Data Struct: AddCategoryValues
//
// Function:  Add category value Tag objects to the DataStruct object.
//
// Parms:     1. In     The category value Tag objects to add to the DataStruct object
//
NS_IMETHODIMP
nsP3PDataStruct::AddCategoryValues( nsISupportsArray  *aCategoryValueTags ) {

  nsresult  rv;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  // Loop through the list of categories and add them to this data struct
  rv = NS_NewP3PSimpleEnumerator( aCategoryValueTags,
                                  getter_AddRefs( pEnumerator ) );

  if (NS_SUCCEEDED( rv )) {
    rv = pEnumerator->HasMoreElements(&bMoreTags );

    while (NS_SUCCEEDED( rv ) && bMoreTags) {
      rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

      if (NS_SUCCEEDED( rv )) {
        rv = AddCategoryValues( pTag );
      }

      if (NS_SUCCEEDED( rv )) {
        rv = pEnumerator->HasMoreElements(&bMoreTags );
      }
    }
  }


  return rv;
}

// P3P Data Struct: AddCategoryValues
//
// Function:  Add category value Tag objects to the DataStruct object.
//
// Parms:     1. In     The category value Tag objects to add to the DataStruct object
//
NS_IMETHODIMP
nsP3PDataStruct::AddCategoryValues( nsIP3PTag  *aCategoryValueTag ) {

  nsresult                       rv;

  nsAutoString                   sName,
                                 sNewName;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag,
                                 pNewTag;

  PRBool                         bMoreTags,
                                 bFound;


  aCategoryValueTag->GetName( sNewName );

  // Loop through the list of this data struct's categories to see if the
  // category is already present
  rv = NS_NewP3PSimpleEnumerator( mCategoryValueTags,
                                  getter_AddRefs( pEnumerator ) );

  if (NS_SUCCEEDED( rv )) {
    rv = pEnumerator->HasMoreElements(&bMoreTags );

    bFound = PR_FALSE;
    while (NS_SUCCEEDED( rv ) && !bFound && bMoreTags) {
      rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

      if (NS_SUCCEEDED( rv )) {
        pTag->GetName( sName );

        if (sName == sNewName) {
          bFound = PR_TRUE;
        }
      }

      if (NS_SUCCEEDED( rv )) {
        rv = pEnumerator->HasMoreElements(&bMoreTags );
      }
    }

    if (NS_SUCCEEDED( rv ) && !bFound) {
      // Copy the category value and add it to the list
      aCategoryValueTag->CloneTree( nsnull,
                                    getter_AddRefs( pNewTag ) );
      if (!mCategoryValueTags->AppendElement( pNewTag )) {
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  return rv;
}

// P3P Data Struct: GetLongDescription
//
// Function:  Gets the <LONG-DESCRIPTION> Tag object of the DataStruct object.
//
// Parms:     1. Out    The <LONG-DESCRIPTION> Tag object of the DataStruct object
//
NS_IMETHODIMP
nsP3PDataStruct::GetLongDescription( nsIP3PTag **aLongDescriptionTag ) {

  NS_ENSURE_ARG_POINTER( aLongDescriptionTag );

  *aLongDescriptionTag = mLongDescriptionTag;

  NS_IF_ADDREF( *aLongDescriptionTag );

  return NS_OK;
}

// P3P Data Struct: AddChild
//
// Function:  Adds a child to the DataStruct object.
//
// Parms:     1. In     The child DataStruct object to add
//
NS_IMETHODIMP
nsP3PDataStruct::AddChild( nsIP3PDataStruct  *aChild ) {

  nsresult  rv = NS_OK;


  if (mChildren->AppendElement( aChild )) {
    aChild->SetParent( this );
  }
  else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

// P3P Data Struct: RemoveChild
//
// Function:  Removes a child from the DataStruct object.
//
// Parms:     1. In     The child DataStruct object to remove
//
NS_IMETHODIMP
nsP3PDataStruct::RemoveChild( nsIP3PDataStruct  *aChild ) {

  aChild->SetParent( nsnull );
  mChildren->RemoveElement( aChild );

  return NS_OK;
}

// P3P Data Struct: RemoveChildren
//
// Function:  Removes all children from the DataStruct object.
//
// Parms:     None
//
NS_IMETHODIMP_( void )
nsP3PDataStruct::RemoveChildren( ) {

  nsresult                       rv;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PDataStruct>     pDataStruct;

  PRBool                         bMoreDataStructs;


  // Loop through all of the children and set their parent to null
  rv = NS_NewP3PSimpleEnumerator( mChildren,
                                  getter_AddRefs( pEnumerator ) );

  if (NS_SUCCEEDED( rv )) {
    rv = pEnumerator->HasMoreElements(&bMoreDataStructs );

    while (NS_SUCCEEDED( rv ) && bMoreDataStructs) {
      rv = pEnumerator->GetNext( getter_AddRefs( pDataStruct ) );

      if (NS_SUCCEEDED( rv )) {
        pDataStruct->SetParent( nsnull );
      }

      if (NS_SUCCEEDED( rv )) {
        rv = pEnumerator->HasMoreElements(&bMoreDataStructs );
      }
    }
  }

  mChildren->Clear( );

  return;
}

// P3P Data Struct: GetChildren
//
// Function:  Gets the DataStruct object children.
//
// Parms:     1. Out    The DataStruct object children
//
NS_IMETHODIMP
nsP3PDataStruct::GetChildren( nsISupportsArray **aChildren ) {

  NS_ENSURE_ARG_POINTER( aChildren );

  *aChildren = mChildren;

  NS_IF_ADDREF( *aChildren );

  return NS_OK;
}

// P3P Data Struct: GetParent
//
// Function:  Gets the parent of the DataStruct object.
//
// Parms:     1. Out    The parent DataStruct object
//
NS_IMETHODIMP
nsP3PDataStruct::GetParent( nsIP3PDataStruct **aParent ) {

  NS_ENSURE_ARG_POINTER( aParent );

  *aParent = mParent;

  NS_IF_ADDREF( *aParent );

  return NS_OK;
}

// P3P Data Struct: SetParent
//
// Function:  Sets the parent of the DataStruct object.
//
// Parms:     1. In     The parent DataStruct object
//
NS_IMETHODIMP_( void )
nsP3PDataStruct::SetParent( nsIP3PDataStruct  *aParent ) {

  mParent = aParent;

  return;
}

// P3P Data Struct: CloneNode
//
// Function:  Creates a copy of the DataStruct object (at the node level only).
//
// Parms:     1. In     The DataStruct object to make the parent of the cloned DataStruct object
//            2. Out    The cloned DataStruct object
//
NS_IMETHODIMP
nsP3PDataStruct::CloneNode( nsIP3PDataStruct  *aParent,
                            nsIP3PDataStruct **aNewDataStruct ) {

  nsresult             rv = NS_OK;

  nsCOMPtr<nsIP3PTag>  pLongDescriptionTag;


  NS_ENSURE_ARG_POINTER( aNewDataStruct );

  *aNewDataStruct = nsnull;

  if (mLongDescriptionTag) {
    // Copy the <LONG-DESCRIPTION> Tag object for the new data struct
    rv = mLongDescriptionTag->CloneTree( nsnull,
                                         getter_AddRefs( pLongDescriptionTag ) );
  }

  if (NS_SUCCEEDED( rv )) {
    // Create a copy of the data struct
    rv = NS_NewP3PDataStruct( mExplicitlyCreated,
                              mName,
                              mStructRef,
                              mDataSchemaURISpec,
                              mDataStructName,
                              mShortDescription,
                              mCategoryValueTags,
                              pLongDescriptionTag,
                              aNewDataStruct );

    if (NS_SUCCEEDED( rv )) {

      if (aParent) {
        // Parent specified, add the child to the parent
        rv = aParent->AddChild( *aNewDataStruct );
      }

      if (NS_SUCCEEDED( rv )) {

        if (mReferenceResolved) {
          // Set the reference resolved flag
          (*aNewDataStruct)->SetReferenceResolved( );
        }
      }
    }
  }

  return rv;
}

// P3P Data Struct: CloneTree
//
// Function:  Creates a copy of the DataStruct object and it's children.
//
// Parms:     1. In     The DataStruct object to make the parent of the cloned DataStruct object
//            2. Out    The cloned DataStruct object
//
NS_IMETHODIMP
nsP3PDataStruct::CloneTree( nsIP3PDataStruct  *aParent,
                            nsIP3PDataStruct **aNewDataStruct ) {

  nsresult                       rv;

  nsCOMPtr<nsIP3PDataStruct>     pNewDataStruct;

  nsCOMPtr<nsISupportsArray>     pChildren;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PDataStruct>     pDataStruct,
                                 pDataStructTree;

  PRBool                         bMoreDataStructs;


  NS_ENSURE_ARG_POINTER( aNewDataStruct );

  *aNewDataStruct = nsnull;

  // Copy the node
  rv = CloneNode( aParent,
                  getter_AddRefs( pNewDataStruct ) );

  if (NS_SUCCEEDED( rv )) {
    rv = GetChildren( getter_AddRefs( pChildren ) );

    if (NS_SUCCEEDED( rv )) {
      // Loop through all of the children and copy them
      rv = NS_NewP3PSimpleEnumerator( pChildren,
                                      getter_AddRefs( pEnumerator ) );

      if (NS_SUCCEEDED( rv )) {
        rv = pEnumerator->HasMoreElements(&bMoreDataStructs );

        while (NS_SUCCEEDED( rv ) && bMoreDataStructs) {
          rv = pEnumerator->GetNext( getter_AddRefs( pDataStruct ) );

          if (NS_SUCCEEDED( rv )) {
            rv = pDataStruct->CloneTree( pNewDataStruct,
                                         getter_AddRefs( pDataStructTree ) );
          }

          if (NS_SUCCEEDED( rv )) {
            rv = pEnumerator->HasMoreElements(&bMoreDataStructs );
          }
        }
      }
    }
  }

  if (NS_SUCCEEDED( rv )) {
    rv = pNewDataStruct->QueryInterface( NS_GET_IID( nsIP3PDataStruct ),
                                (void **)aNewDataStruct );
  }

  return rv;
}

// P3P Data Struct: CloneChildren
//
// Function:  Creates a copy of the children of the DataStruct object and their children.
//
// Parms:     1. In     The DataStruct object to make the parent of the cloned children
//
NS_IMETHODIMP
nsP3PDataStruct::CloneChildren( nsIP3PDataStruct  *aParent ) {

  nsresult                       rv;

  nsCOMPtr<nsISupportsArray>     pChildren;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PDataStruct>     pDataStruct,
                                 pDataStructTree;

  PRBool                         bMoreDataStructs;


  rv = GetChildren( getter_AddRefs( pChildren ) );

  if (NS_SUCCEEDED( rv )) {
    // Loop through all the children and copy them
    rv = NS_NewP3PSimpleEnumerator( pChildren,
                                    getter_AddRefs( pEnumerator ) );

    if (NS_SUCCEEDED( rv )) {
      rv = pEnumerator->HasMoreElements(&bMoreDataStructs );

      while (NS_SUCCEEDED( rv ) && bMoreDataStructs) {
        rv = pEnumerator->GetNext( getter_AddRefs( pDataStruct ) );

        if (NS_SUCCEEDED( rv )) {
          rv = pDataStruct->CloneTree( aParent,
                                       getter_AddRefs( pDataStructTree ) );
        }

        if (NS_SUCCEEDED( rv )) {
          rv = pEnumerator->HasMoreElements(&bMoreDataStructs );
        }
      }
    }
  }

  return rv;
}

// P3P Data Struct: DestroyTree
//
// Function:  Removes all connections (parent/children) between DataStruct objects.
//
// Parms:     None
NS_IMETHODIMP_( void )
nsP3PDataStruct::DestroyTree( ) {

  nsresult                       rv;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PDataStruct>     pDataStruct;

  PRBool                         bMoreDataStructs;


  // Loop through all of the children and destroy their tree
  rv = NS_NewP3PSimpleEnumerator( mChildren,
                                  getter_AddRefs( pEnumerator ) );

  if (NS_SUCCEEDED( rv )) {
    rv = pEnumerator->HasMoreElements(&bMoreDataStructs );

    while (NS_SUCCEEDED( rv ) && bMoreDataStructs) {
      rv = pEnumerator->GetNext( getter_AddRefs( pDataStruct ) );

      if (NS_SUCCEEDED( rv )) {
        pDataStruct->DestroyTree( );
      }

      if (NS_SUCCEEDED( rv )) {
        rv = pEnumerator->HasMoreElements(&bMoreDataStructs );
      }
    }
  }

  RemoveChildren( );

  return;
}

// P3P Data Struct: ReferenceResolved
//
// Function:  Sets the indicator that says this datastruct has resolved it's reference.
//
// Parms:     None
NS_IMETHODIMP_( void )
nsP3PDataStruct::SetReferenceResolved( ) {

  mReferenceResolved = PR_TRUE;

  return;
}

// P3P Data Struct: IsReferenceResolved
//
// Function:  Returns the indicator that says whether or not the reference for this datastruct
//            has been resolved.
//
// Parms:     None
NS_IMETHODIMP_( PRBool )
nsP3PDataStruct::IsReferenceResolved( ) {

  return mReferenceResolved;
}


// ****************************************************************************
// nsP3PDataStruct routines
// ****************************************************************************

// P3P Data Struct: IsPrimitiveStruct
//
// Function:  Determines whether the DataStruct object is a  basic DataStruct
//
// Parms:     None
//
NS_METHOD_( PRBool )
nsP3PDataStruct::IsBasicStruct( ) {

  return (mDataStructName.Length( ) == 0);
}
