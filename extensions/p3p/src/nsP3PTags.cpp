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

#include "nsP3PTags.h"
#include "nsP3PSimpleEnumerator.h"
#include "nsP3PLogging.h"

#include <nsIComponentManager.h>


// ****************************************************************************
// nsP3PTag Implementation routines
// ****************************************************************************

// P3P Tag: nsISupports
NS_IMPL_ISUPPORTS1( nsP3PTag, nsIP3PTag );

// P3P Tag: Creation routine
NS_METHOD
NS_NewP3PTag( nsIDOMNode  *aDOMNode,
              nsIP3PTag   *aParent,
              nsIP3PTag  **aTag ) {

  nsresult   rv;

  nsP3PTag  *pNewTag = nsnull;


  NS_ENSURE_ARG_POINTER( aTag );

  pNewTag = new nsP3PTag( );

  if (pNewTag) {
    NS_ADDREF( pNewTag );

    rv = pNewTag->Init( aDOMNode,
                        aParent );

    if (NS_SUCCEEDED( rv )) {
      rv = pNewTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                           (void **)aTag );
    }

    NS_RELEASE( pNewTag );
  }
  else {
    NS_ASSERTION( 0, "P3P:  Tag unable to be created.\n" );
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}

// P3P Tag: Constructor
nsP3PTag::nsP3PTag( ) {

  NS_INIT_ISUPPORTS( );
}

// P3P Tag: Destructor
nsP3PTag::~nsP3PTag( ) {
}

// P3P Tag: Init
//
// Function:  Initialization routine for the P3P Tag object.
//
// Parms:     None
//
NS_IMETHODIMP
nsP3PTag::Init( nsIDOMNode  *aDOMNode,
                nsIP3PTag   *aParent ) {

  nsresult  rv;


  mDOMNode     = aDOMNode;
  mParent      = aParent;
  mChildren    = do_CreateInstance( NS_SUPPORTSARRAY_CONTRACTID,
                                   &rv );

  if (NS_SUCCEEDED( rv )) {

    if (mParent) {
      rv = mParent->AddChild( this );
    }
  }

  return rv;
}

// ****************************************************************************
// nsIP3PTag routines
// ****************************************************************************

// P3P Tag: ProcessTag
//
// Function:  Validates and saves the information related to the P3P XML tag.
//              This needs to be implemented by each derived class.
//
// Parms:     None
//
NS_IMETHODIMP
nsP3PTag::ProcessTag( ) {

  nsresult              rv;


  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing \"generic\" tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing \"generic\" tag.\n") );
  }

  return rv;
}

// P3P Tag: GetDOMNode
//
// Function:  Gets the DOMNode associated with this Tag object.
//
// Parms:     1. Out    The DOMNode associated with this Tag object
//
NS_IMETHODIMP
nsP3PTag::GetDOMNode( nsIDOMNode **aDOMNode ) {

  NS_ENSURE_ARG_POINTER( aDOMNode );

  *aDOMNode = mDOMNode;

  NS_IF_ADDREF( *aDOMNode );

  return NS_OK;
}

// P3P Tag: GetName
//
// Function:  Gets the name of this Tag object (the name of the P3P XML tag).
//
// Parms:     1. Out    The name of this Tag object
//
NS_IMETHODIMP_( void )
nsP3PTag::GetName( nsString&  aTagName ) {
  
  aTagName = mTagName;

  return;
}

// P3P Tag: GetNameSpace
//
// Function:  Gets the namespace of this Tag object (the namespace of the P3P XML tag).
//
// Parms:     1. Out    The namespace of this Tag object
//
NS_IMETHODIMP_( void )
nsP3PTag::GetNameSpace( nsString&  aTagNameSpace ) {
  
  aTagNameSpace = mTagNameSpace;

  return;
}

// P3P Tag: GetText
//
// Function:  Gets the text of this Tag object (the text of the P3P XML tag).
//
// Parms:     1. Out    The text of this Tag object
//
NS_IMETHODIMP_( void )
nsP3PTag::GetText( nsString&  aText ) {

  aText = mText;

  return;
}

// P3P Tag: GetAttribute
//
// Function:  Functions to get the value of an attribute of this Tag object (the
//            attribute of the P3P XML tag) with the optional indicator as to whether
//            the attribute was present (vs. just empty).
//
// Parms:     1. In     The name of the attribute
//            2. In     The namespace of the attribute
//            3. Out    The value of the attribute
//            4. Out    An indicator as to whether or not the attribute was present
//
NS_IMETHODIMP_( void )
nsP3PTag::GetAttribute( const char  *aAttributeName,
                        const char  *aAttributeNameSpace,
                        nsString&    aAttributeValue,
                        PRBool      *aAttributePresent ) {

  nsAutoString  sAttributeName,
                sAttributeNameSpace;


  sAttributeName.AssignWithConversion( aAttributeName );
  sAttributeNameSpace.AssignWithConversion( aAttributeNameSpace );

  GetAttribute( sAttributeName,
                sAttributeNameSpace,
                aAttributeValue,
                aAttributePresent );

  return;
}

// P3P Tag: GetAttribute
//
// Function:  Functions to get the value of an attribute of this Tag object (the
//            attribute of the P3P XML tag) with the optional indicator as to whether
//            the attribute was present (vs. just empty).
//
// Parms:     1. In     The name of the attribute
//            2. In     The namespace of the attribute
//            3. Out    The value of the attribute
//            4. Out    An indicator as to whether or not the attribute was present
//
NS_IMETHODIMP_( void )
nsP3PTag::GetAttribute( nsString&  aAttributeName,
                        nsString&  aAttributeNameSpace,
                        nsString&  aAttributeValue,
                        PRBool    *aAttributePresent ) {

  PRInt32       iIndex;

  nsAutoString  sAttributeNameSpace;


  aAttributeValue.AssignWithConversion( "" );
  if (aAttributePresent) {
    *aAttributePresent = PR_FALSE;
  }

  iIndex = mAttributeNames.IndexOfIgnoreCase( aAttributeName );
  if (iIndex >= 0) {
    mAttributeNameSpaces.StringAt( iIndex,
                                   sAttributeNameSpace );
    if ((sAttributeNameSpace.Length( ) == 0)         ||
        (sAttributeNameSpace == aAttributeNameSpace)) {
      mAttributeValues.StringAt( iIndex,
                                 aAttributeValue );

      if (aAttributePresent) {
        *aAttributePresent = PR_TRUE;
      }
    }
  }

  return;
}

// P3P Tag: GetParent
//
// Function:  Gets the parent Tag object.
//
// Parms:     1. Out    The parent of this Tag object
//
NS_IMETHODIMP
nsP3PTag::GetParent( nsIP3PTag **aParent ) {

  NS_ENSURE_ARG_POINTER( aParent );

  *aParent = mParent;

  NS_IF_ADDREF( *aParent );

  return NS_OK;
}

// P3P Tag: SetParent
//
// Function:  Sets the parent Tag object.
//
// Parms:     1. In     The parent of this Tag object
//
NS_IMETHODIMP_( void )
nsP3PTag::SetParent( nsIP3PTag  *aParent ) {

  mParent = aParent;

  return;
}

// P3P Tag: AddChild
//
// Function:  Adds a child to the Tag object.
//
// Parms:     1. In     The child Tag object to add
//
NS_IMETHODIMP
nsP3PTag::AddChild( nsIP3PTag  *aChild ) {

  nsresult  rv = NS_OK;


  if (!mChildren->AppendElement( aChild )) {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

// P3P Tag: RemoveChildren
//
// Function:  Removes all the children of the Tag object.
//
// Parms:     None
//
NS_IMETHODIMP_( void )
nsP3PTag::RemoveChildren( ) {

  nsresult                       rv;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  // Loop through all of the children and set their parent to null
  rv = NS_NewP3PSimpleEnumerator( mChildren,
                                  getter_AddRefs( pEnumerator ) );

  if (NS_SUCCEEDED( rv )) {
    rv = pEnumerator->HasMoreElements(&bMoreTags );

    while (NS_SUCCEEDED( rv ) && bMoreTags) {
      rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

      if (NS_SUCCEEDED( rv )) {
        pTag->SetParent( nsnull );
      }

      if (NS_SUCCEEDED( rv )) {
        rv = pEnumerator->HasMoreElements(&bMoreTags );
      }
    }
  }

  mChildren->Clear( );

  return;
}

// P3P Tag: GetChildren
//
// Function:  Gets the Tag object children.
//
// Parms:     1. Out    The Tag object children
//
NS_IMETHODIMP
nsP3PTag::GetChildren( nsISupportsArray **aChildren ) {

  NS_ENSURE_ARG_POINTER( aChildren );

  *aChildren = mChildren;

  NS_IF_ADDREF( *aChildren );

  return NS_OK;
}

// P3P Tag: CloneNode
//
// Function:  Creates a copy of the Tag object (at the node level only).
//              This needs to be implemented by each derived class.
//
// Parms:     1. In     The Tag object to make the parent of the cloned Tag object
//            2. Out    The cloned Tag object
//
NS_IMETHODIMP
nsP3PTag::CloneNode( nsIP3PTag  *aParent,
                     nsIP3PTag **aNewTag ) {

  nsresult             rv;

  nsCOMPtr<nsIP3PTag>  pTag;


  NS_ENSURE_ARG_POINTER( aNewTag );

  *aNewTag = nsnull;

  rv = NS_NewP3PTag( mDOMNode,
                     aParent,
                     getter_AddRefs( pTag ) );

  if (NS_SUCCEEDED( rv )) {
    rv = nsP3PTag::ProcessTag( );

    if (NS_SUCCEEDED( rv )) {
      rv = pTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                        (void **)aNewTag );
    }
  }

  return rv;
}

// P3P Tag Macro: CloneTree
//
// Function:  Creates a copy of the Tag object and it's children.
//              This needs to be implemented by each derived class.
//
// Parms:     1. In     The Tag object to make the parent of the cloned Tag object
//            2. Out    The cloned Tag object
//
NS_IMETHODIMP
nsP3PTag::CloneTree( nsIP3PTag  *aParent,
                     nsIP3PTag **aNewTag ) {

  nsresult             rv;

  nsCOMPtr<nsIP3PTag>  pTag;


  NS_ENSURE_ARG_POINTER( aNewTag );

  *aNewTag = nsnull;

  rv = NS_NewP3PTag( mDOMNode,
                     aParent,
                     getter_AddRefs( pTag ) );

  if (NS_SUCCEEDED( rv )) {
    rv = pTag->ProcessTag( );

    if (NS_SUCCEEDED( rv )) {
      rv = pTag->QueryInterface( NS_GET_IID( nsIP3PTag ),
                        (void **)aNewTag );
    }
  }

  return rv;
}

// P3P Tag: DestroyTree
//
// Function:  Removes all connections (parent/children) between Tag objects.
//
// Parms:     None
NS_IMETHODIMP_( void )
nsP3PTag::DestroyTree( ) {

  nsresult                       rv;

  nsCOMPtr<nsISimpleEnumerator>  pEnumerator;

  nsCOMPtr<nsIP3PTag>            pTag;

  PRBool                         bMoreTags;


  // Loop through all of the children and destroy their tree
  rv = NS_NewP3PSimpleEnumerator( mChildren,
                                  getter_AddRefs( pEnumerator ) );

  if (NS_SUCCEEDED( rv )) {
    rv = pEnumerator->HasMoreElements(&bMoreTags );

    while (NS_SUCCEEDED( rv ) && bMoreTags) {
      rv = pEnumerator->GetNext( getter_AddRefs( pTag ) );

      if (NS_SUCCEEDED( rv )) {
        pTag->DestroyTree( );
      }

      if (NS_SUCCEEDED( rv )) {
        rv = pEnumerator->HasMoreElements(&bMoreTags );
      }
    }
  }

  RemoveChildren( );

  return;
}


// ****************************************************************************
// nsP3PMetaTag Implementation routines
// ****************************************************************************

// P3P Meta Tag: Creation routine
NS_P3P_TAG_NEWTAG( Meta );

// P3P Meta Tag: Process the META tag
NS_P3P_TAG_PROCESS_BEGIN( Meta );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <META> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mMeta );
  }

  // Look for mandatory POLICY-REFERENCES tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( PolicyReferences,
                                     POLICYREFERENCES,
                                     P3P,
                                     P3P_TAG_MANDATORY );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <META> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Meta );

// P3P Meta Tag: Clone the META tag node
NS_P3P_TAG_CLONE_NODE( Meta );

// P3P Meta Tag: Clone the META tag tree
NS_P3P_TAG_CLONE_TREE( Meta );


// ****************************************************************************
// nsP3PPolicyReferencesTag Implementation routines
// ****************************************************************************

// P3P PolicyReferences Tag: Creation routine
NS_P3P_TAG_NEWTAG( PolicyReferences );

// P3P PolicyReferences Tag: Process the POLICY-REFERENCES tag
NS_P3P_TAG_PROCESS_BEGIN( PolicyReferences );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <POLICY-REFERENCES> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the POLICY-REF tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( PolicyReferences,
                                      PolicyRef );
  }

  // Look for the optional EXPIRY tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Expiry,
                                     EXPIRY,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  // Look for optional POLICY-REF tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( PolicyRef,
                                       POLICYREF,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for the optional POLICIES tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Policies,
                                     POLICIES,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <POLICY-REFERENCES> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( PolicyReferences );

// P3P PolicyReferences Tag: Clone the POLICY-REFERENCES tag node
NS_P3P_TAG_CLONE_NODE( PolicyReferences );

// P3P PolicyReferences Tag: Clone the POLICY-REFERENCES tag tree
NS_P3P_TAG_CLONE_TREE( PolicyReferences );


// ****************************************************************************
// nsP3PPolicyRefTag Implementation routines
// ****************************************************************************

// P3P PolicyRef Tag: Creation routine
NS_P3P_TAG_NEWTAG( PolicyRef );

// P3P PolicyRef Tag: Process the POLICY-REF tag
NS_P3P_TAG_PROCESS_BEGIN( PolicyRef );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <POLICY-REF> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the INCLUDE tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( PolicyRef,
                                      Include );
  }

  // Create an nsISupportsArray to hold the EXCLUDE tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( PolicyRef,
                                      Exclude );
  }

  // Create an nsISupportsArray to hold the EMBEDDED-INCLUDE tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( PolicyRef,
                                      EmbeddedInclude );
  }

  // Create an nsISupportsArray to hold the EMBEDDED-EXCLUDE tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( PolicyRef,
                                      EmbeddedExclude );
  }

  // Create an nsISupportsArray to hold the METHOD tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( PolicyRef,
                                      Method );
  }

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_ABOUT_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mAbout );
  }

  // Check for mandatory about attribute
  if (NS_SUCCEEDED( rv )) {

    if (mAbout.Length( ) == 0) {
      rv = NS_ERROR_FAILURE;
    }
  }

  // Look for optional INCLUDE tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Include,
                                       INCLUDE,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for optional EXCLUDE tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Exclude,
                                       EXCLUDE,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for optional EMBEDDED-INCLUDE tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( EmbeddedInclude,
                                       EMBEDDEDINCLUDE,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for optional EMBEDDED-EXCLUDE tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( EmbeddedExclude,
                                       EMBEDDEDEXCLUDE,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for optional METHOD tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Method,
                                       METHOD,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <POLICY-REF> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( PolicyRef );

// P3P PolicyRef Tag: Clone the POLICY-REF tag node
NS_P3P_TAG_CLONE_NODE( PolicyRef );

// P3P PolicyRef Tag: Clone the POLICY-REF tag tree
NS_P3P_TAG_CLONE_TREE( PolicyRef );


// ****************************************************************************
// nsP3PPoliciesTag Implementation routines
// ****************************************************************************

// P3P Policies Tag: Creation routine
NS_P3P_TAG_NEWTAG( Policies );

// P3P Policies Tag: Process the POLICIES tag
NS_P3P_TAG_PROCESS_BEGIN( Policies );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <POLICIES> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the POLICY tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Policies,
                                      Policy );
  }

  // Look for optional POLICY tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Policy,
                                       POLICY,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <POLICIES> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Policies );

// P3P Policies Tag: Clone the POLICIES tag node
NS_P3P_TAG_CLONE_NODE( Policies );

// P3P Policies Tag: Clone the POLICIES tag tree
NS_P3P_TAG_CLONE_TREE( Policies );


// ****************************************************************************
// nsP3PIncludeTag Implementation routines
// ****************************************************************************

// P3P Include Tag: Creation routine
NS_P3P_TAG_NEWTAG( Include );

// P3P Include Tag: Process the INCLUDE tag
NS_P3P_TAG_PROCESS_BEGIN( Include );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <INCLUDE> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mInclude );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <INCLUDE> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Include );

// P3P Include Tag: Clone the INCLUDE tag node
NS_P3P_TAG_CLONE_NODE( Include );

// P3P Include Tag: Clone the INCLUDE tag tree
NS_P3P_TAG_CLONE_TREE( Include );


// ****************************************************************************
// nsP3PExcludeTag Implementation routines
// ****************************************************************************

// P3P Exclude Tag: Creation routine
NS_P3P_TAG_NEWTAG( Exclude );

// P3P Exclude Tag: Process the EXCLUDE tag
NS_P3P_TAG_PROCESS_BEGIN( Exclude );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <EXCLUDE> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mExclude );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <EXCLUDE> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Exclude );

// P3P Exclude Tag: Clone the EXCLUDE tag node
NS_P3P_TAG_CLONE_NODE( Exclude );

// P3P Exclude Tag: Clone the EXCLUDE tag tree
NS_P3P_TAG_CLONE_TREE( Exclude );


// ****************************************************************************
// nsP3PEmbeddedIncludeTag Implementation routines
// ****************************************************************************

// P3P EmbeddedInclude Tag: Creation routine
NS_P3P_TAG_NEWTAG( EmbeddedInclude );

// P3P EmbeddedInclude Tag: Process the EMBEDDED-INCLUDE tag
NS_P3P_TAG_PROCESS_BEGIN( EmbeddedInclude );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <EMBEDDED-INCLUDE> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mEmbeddedInclude );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <EMBEDDED-INCLUDE> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( EmbeddedInclude );

// P3P EmbeddedInclude Tag: Clone the EMBEDDED-INCLUDE tag node
NS_P3P_TAG_CLONE_NODE( EmbeddedInclude );

// P3P EmbeddedInclude Tag: Clone the EMBEDDED-INCLUDE tag tree
NS_P3P_TAG_CLONE_TREE( EmbeddedInclude );


// ****************************************************************************
// nsP3PEmbeddedExcludeTag Implementation routines
// ****************************************************************************

// P3P EmbeddedExclude Tag: Creation routine
NS_P3P_TAG_NEWTAG( EmbeddedExclude );

// P3P EmbeddedExclude Tag: Process the EMBEDDED-EXCLUDE tag
NS_P3P_TAG_PROCESS_BEGIN( EmbeddedExclude );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <EMBEDDED-EXCLUDE> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mEmbeddedExclude );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <EMBEDDED-EXCLUDE> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( EmbeddedExclude );

// P3P EmbeddedExclude Tag: Clone the EMBEDDED-EXCLUDE tag node
NS_P3P_TAG_CLONE_NODE( EmbeddedExclude );

// P3P EmbeddedExclude Tag: Clone the EMBEDDED-EXCLUDE tag tree
NS_P3P_TAG_CLONE_TREE( EmbeddedExclude );


// ****************************************************************************
// nsP3PMethodTag Implementation routines
// ****************************************************************************

// P3P Method Tag: Creation routine
NS_P3P_TAG_NEWTAG( Method );

// P3P Method Tag: Process the METHOD tag
NS_P3P_TAG_PROCESS_BEGIN( Method );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <METHOD> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mMethod );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <METHOD> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Method );

// P3P Method Tag: Clone the METHOD tag node
NS_P3P_TAG_CLONE_NODE( Method );

// P3P Method Tag: Clone the METHOD tag tree
NS_P3P_TAG_CLONE_TREE( Method );


// ****************************************************************************
// nsP3PPolicyTag Implementation routines
// ****************************************************************************

// P3P Policy Tag: Creation routine
NS_P3P_TAG_NEWTAG( Policy );

// P3P Policy Tag: Process the POLICY tag
NS_P3P_TAG_PROCESS_BEGIN( Policy );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <POLICY> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Policy,
                                      Extension );
  }

  // Create an nsISupportsArray to hold the STATEMENT tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Policy,
                                      Statement );
  }

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_DISCURI_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mDescURISpec );

    GetAttribute( P3P_OPTURI_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mOptURISpec );

    GetAttribute( P3P_NAME_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mName );
  }

  // Check for mandatory discuri attribute
  if (NS_SUCCEEDED( rv )) {

    if (mDescURISpec.Length( ) == 0) {
      rv = NS_ERROR_FAILURE;
    }
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for optional EXPIRY tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Expiry,
                                     EXPIRY,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  // Look for optional DATASCHEMA tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( DataSchema,
                                     DATASCHEMA,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  // Look for mandatory ENTITY tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Entity,
                                     ENTITY,
                                     P3P,
                                     P3P_TAG_MANDATORY );
  }

  // Look for mandatory ACCESS tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Access,
                                     ACCESS,
                                     P3P,
                                     P3P_TAG_MANDATORY );
  }

  // Look for optional DISPUTES-GROUP tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( DisputesGroup,
                                     DISPUTESGROUP,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  // Look for mandatory STATEMENT tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Statement,
                                       STATEMENT,
                                       P3P,
                                       P3P_TAG_MANDATORY );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <POLICY> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Policy );

// P3P Policy Tag: Clone the POLICY tag node
NS_P3P_TAG_CLONE_NODE( Policy );

// P3P Policy Tag: Clone the POLICY tag tree
NS_P3P_TAG_CLONE_TREE( Policy );


// ****************************************************************************
// nsP3PEntityTag Implementation routines
// ****************************************************************************

// P3P Entity Tag: Creation routine
NS_P3P_TAG_NEWTAG( Entity );

// P3P Entity Tag: Process the ENTITY tag
NS_P3P_TAG_PROCESS_BEGIN( Entity );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <ENTITY> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Entity,
                                      Extension );
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for mandatory DATA-GROUP tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( DataGroup,
                                     DATAGROUP,
                                     P3P,
                                     P3P_TAG_MANDATORY );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <ENTITY> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Entity );

// P3P Entity Tag: Clone the ENTITY tag node
NS_P3P_TAG_CLONE_NODE( Entity );

// P3P Entity Tag: Clone the ENTITY tag tree
NS_P3P_TAG_CLONE_TREE( Entity );


// ****************************************************************************
// nsP3PAccessTag Implementation routines
// ****************************************************************************

// P3P Access Tag: Creation routine
NS_P3P_TAG_NEWTAG( Access );

// P3P Access Tag: Process the ACCESS tag
NS_P3P_TAG_PROCESS_BEGIN( Access );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <ACCESS> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Access,
                                      Extension );
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for mandatory access disclosure tag
  if (NS_SUCCEEDED( rv )) {
    nsString       tagName;

    nsStringArray  tagChoices;

    tagName.AssignWithConversion( P3P_ACCESS_NONIDENT );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_ACCESS_IDENT_CONTACT );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_ACCESS_OTHER_IDENT );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_ACCESS_CONTACT_AND_OTHER );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_ACCESS_ALL );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_ACCESS_NONE );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED( rv )) {
      NS_P3P_TAG_PROCESS_SINGLE_CHOICE_CHILD( AccessValue,
                                              tagChoices,
                                              P3P,
                                              P3P_TAG_MANDATORY );
    }
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <ACCESS> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Access );

// P3P Access Tag: Clone the ACCESS tag node
NS_P3P_TAG_CLONE_NODE( Access );

// P3P Access Tag: Clone the ACCESS tag tree
NS_P3P_TAG_CLONE_TREE( Access );


// ****************************************************************************
// nsP3PDisputesGroupTag Implementation routines
// ****************************************************************************

// P3P DisputesGroup Tag: Creation routine
NS_P3P_TAG_NEWTAG( DisputesGroup );

// P3P DisputesGroup Tag: Process the DISPUTES-GROUP tag
NS_P3P_TAG_PROCESS_BEGIN( DisputesGroup );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <DISPUTES-GROUP> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( DisputesGroup,
                                      Extension );
  }

  // Create an nsISupportsArray to hold the DISPUTES tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( DisputesGroup,
                                      Disputes );
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for mandatory DISPUTE tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Disputes,
                                       DISPUTES,
                                       P3P,
                                       P3P_TAG_MANDATORY );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <DISPUTES-GROUP> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( DisputesGroup );

// P3P DisputesGroup Tag: Clone the DISPUTES-GROUP tag node
NS_P3P_TAG_CLONE_NODE( DisputesGroup );

// P3P DisputesGroup Tag: Clone the DISPUTES-GROUP tag tree
NS_P3P_TAG_CLONE_TREE( DisputesGroup );


// ****************************************************************************
// nsP3PDisputesTag Implementation routines
// ****************************************************************************

// P3P Disputes Tag: Creation routine
NS_P3P_TAG_NEWTAG( Disputes );

// P3P Disputes Tag: Process the DISPUTES tag
NS_P3P_TAG_PROCESS_BEGIN( Disputes );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <DISPUTES> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Disputes,
                                      Extension );
  }

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_RESOLUTIONTYPE_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mResolutionType );

    GetAttribute( P3P_SERVICE_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mService );

    GetAttribute( P3P_VERIFICATION_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mVerification );

    GetAttribute( P3P_SHORTDESCRIPTION_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mShortDescription );
  }

  // Check for mandatory resolution-type attribute
  if (NS_SUCCEEDED( rv )) {

    if (!(mResolutionType.EqualsWithConversion( P3P_SERVICE_VALUE )     ||
          mResolutionType.EqualsWithConversion( P3P_INDEPENDENT_VALUE ) ||
          mResolutionType.EqualsWithConversion( P3P_COURT_VALUE )       ||
          mResolutionType.EqualsWithConversion( P3P_LAW_VALUE ))) {
      rv = NS_ERROR_FAILURE;
    }
  }

  // Check for mandatory service attribute
  if (NS_SUCCEEDED( rv )) {

    if (mService.Length( ) == 0) {
      rv = NS_ERROR_FAILURE;
    }
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for optional LONG-DESCRIPTION tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( LongDescription,
                                     LONGDESCRIPTION,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  // Look for optional IMG tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Img,
                                     IMG,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  // Look for optional REMEDIES tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Remedies,
                                     REMEDIES,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <DISPUTES> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Disputes );

// P3P Disputes Tag: Clone the DISPUTES tag node
NS_P3P_TAG_CLONE_NODE( Disputes );

// P3P Disputes Tag: Clone the DISPUTES tag tree
NS_P3P_TAG_CLONE_TREE( Disputes );


// ****************************************************************************
// nsP3PImgTag Implementation routines
// ****************************************************************************

// P3P Img Tag: Creation routine
NS_P3P_TAG_NEWTAG( Img );

// P3P Img Tag: Process the IMG tag
NS_P3P_TAG_PROCESS_BEGIN( Img );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <IMG> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_SRC_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mSrc );

    GetAttribute( P3P_WIDTH_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mWidth );

    GetAttribute( P3P_HEIGHT_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mHeight );

    GetAttribute( P3P_ALT_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mAlt );
  }

  // Check for mandatory src attribute
  if (NS_SUCCEEDED( rv )) {

    if (mSrc.Length( ) == 0) {
      rv = NS_ERROR_FAILURE;
    }
  }

  // Check for mandatory alt attribute
  if (NS_SUCCEEDED( rv )) {

    if (mAlt.Length( ) == 0) {
      rv = NS_ERROR_FAILURE;
    }
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <IMG> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Img );

// P3P Img Tag: Clone the IMG tag node
NS_P3P_TAG_CLONE_NODE( Img );

// P3P Img Tag: Clone the IMG tag tree
NS_P3P_TAG_CLONE_TREE( Img );


// ****************************************************************************
// nsP3PRemediesTag Implementation routines
// ****************************************************************************

// P3P Remedies Tag: Creation routine
NS_P3P_TAG_NEWTAG( Remedies );

// P3P Remedies Tag: Process the REMEDIES tag
NS_P3P_TAG_PROCESS_BEGIN( Remedies );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <REMEDIES> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Remedies,
                                      Extension );
  }

  // Create an nsISupportsArray to hold the remedies tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Remedies,
                                      RemedyValue );
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for mandatory remedy tag(s)
  if (NS_SUCCEEDED( rv )) {
    nsString       tagName;

    nsStringArray  tagChoices;

    tagName.AssignWithConversion( P3P_REMEDIES_CORRECT );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_REMEDIES_MONEY );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_REMEDIES_LAW );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED( rv )) {
      NS_P3P_TAG_PROCESS_MULTIPLE_CHOICE_CHILD( RemedyValue,
                                                tagChoices,
                                                P3P,
                                                P3P_TAG_MANDATORY );
    }
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <REMEDIES> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Remedies );

// P3P Remedies Tag: Clone the REMEDIES tag node
NS_P3P_TAG_CLONE_NODE( Remedies );

// P3P Remedies Tag: Clone the REMEDIES tag tree
NS_P3P_TAG_CLONE_TREE( Remedies );


// ****************************************************************************
// nsP3PStatementTag Implementation routines
// ****************************************************************************

// P3P Statement Tag: Creation routine
NS_P3P_TAG_NEWTAG( Statement );

// P3P Statement Tag: Process the STATEMENT tag
NS_P3P_TAG_PROCESS_BEGIN( Statement );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <STATEMENT> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Statement,
                                      Extension );
  }

  // Create an nsISupportsArray to hold the DATA-GROUP information
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Statement,
                                      DataGroup );
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for optional CONSEQUENCE tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Consequence,
                                     CONSEQUENCE,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  // Look for mandatory PURPOSE tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Purpose,
                                     PURPOSE,
                                     P3P,
                                     P3P_TAG_MANDATORY );
  }

  // Look for mandatory RECIPIENT tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Recipient,
                                     RECIPIENT,
                                     P3P,
                                     P3P_TAG_MANDATORY );
  }

  // Look for mandatory RETENTION tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Retention,
                                     RETENTION,
                                     P3P,
                                     P3P_TAG_MANDATORY );
  }

  // Look for mandatory DATA-GROUP tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( DataGroup,
                                       DATAGROUP,
                                       P3P,
                                       P3P_TAG_MANDATORY );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <STATEMENT> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Statement );

// P3P Statement Tag: Clone the STATEMENT tag node
NS_P3P_TAG_CLONE_NODE( Statement );

// P3P Statement Tag: Clone the STATEMENT tag tree
NS_P3P_TAG_CLONE_TREE( Statement );


// ****************************************************************************
// nsP3PConsequenceTag Implementation routines
// ****************************************************************************

// P3P Consequence Tag: Creation routine
NS_P3P_TAG_NEWTAG( Consequence );

// P3P Consequence Tag: Process the CONSEQUENCE tag
NS_P3P_TAG_PROCESS_BEGIN( Consequence );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <CONSEQUENCE> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mConsequence );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <CONSEQUENCE> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Consequence );

// P3P Consequence Tag: Clone the CONSEQUENCE tag node
NS_P3P_TAG_CLONE_NODE( Consequence );

// P3P Consequence Tag: Clone the CONSEQUENCE tag tree
NS_P3P_TAG_CLONE_TREE( Consequence );


// ****************************************************************************
// nsP3PPurposeTag Implementation routines
// ****************************************************************************

// P3P Purpose Tag: Creation routine
NS_P3P_TAG_NEWTAG( Purpose );

// P3P Purpose Tag: Process the PURPOSE tag
NS_P3P_TAG_PROCESS_BEGIN( Purpose );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <PURPOSE> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Purpose,
                                      Extension );
  }

  // Create an nsISupportsArray to hold the purpose tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Purpose,
                                      PurposeValue );
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for mandatory purpose tag(s)
  if (NS_SUCCEEDED( rv )) {
    nsString       tagName;

    nsStringArray  tagChoices;

    tagName.AssignWithConversion( P3P_PURPOSE_CURRENT );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_ADMIN );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_DEVELOP );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_CUSTOMIZATION );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_TAILORING );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_PSEUDOANALYSIS );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_PSEUDODECISION );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_INDIVIDUALANALYSIS );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_INDIVIDUALDECISION );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_CONTACT );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_HISTORICAL );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_TELEMARKETING );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_PURPOSE_OTHERPURPOSE );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED( rv )) {
      NS_P3P_TAG_PROCESS_MULTIPLE_CHOICE_CHILD( PurposeValue,
                                                tagChoices,
                                                P3P,
                                                P3P_TAG_MANDATORY );
    }
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <PURPOSE> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Purpose );

// P3P Purpose Tag: Clone the PURPOSE tag node
NS_P3P_TAG_CLONE_NODE( Purpose );

// P3P Purpose Tag: Clone the PURPOSE tag tree
NS_P3P_TAG_CLONE_TREE( Purpose );


// ****************************************************************************
// nsP3PRecipientTag Implementation routines
// ****************************************************************************

// P3P Recipient Tag: Creation routine
NS_P3P_TAG_NEWTAG( Recipient );

// P3P Recipient Tag: Process the RECIPIENT tag
NS_P3P_TAG_PROCESS_BEGIN( Recipient );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <RECIPIENT> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Recipient,
                                      Extension );
  }

  // Create an nsISupportsArray to hold the recipient tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Recipient,
                                      RecipientValue );
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for mandatory recipient tag(s)
  if (NS_SUCCEEDED( rv )) {
    nsString       tagName;

    nsStringArray  tagChoices;

    tagName.AssignWithConversion( P3P_RECIPIENT_OURS );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_RECIPIENT_SAME );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_RECIPIENT_OTHERRECIPIENT );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_RECIPIENT_DELIVERY );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_RECIPIENT_PUBLIC );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_RECIPIENT_UNRELATED );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED( rv )) {
      NS_P3P_TAG_PROCESS_MULTIPLE_CHOICE_CHILD( RecipientValue,
                                                tagChoices,
                                                P3P,
                                                P3P_TAG_MANDATORY );
    }
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <RECIPIENT> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Recipient );

// P3P Recipient Tag: Clone the RECIPIENT tag node
NS_P3P_TAG_CLONE_NODE( Recipient );

// P3P Recipient Tag: Clone the RECIPIENT tag tree
NS_P3P_TAG_CLONE_TREE( Recipient );


// ****************************************************************************
// nsP3PRetentionTag Implementation routines
// ****************************************************************************

// P3P Retention Tag: Creation routine
NS_P3P_TAG_NEWTAG( Retention );

// P3P Retention Tag: Process the RETENTION tag
NS_P3P_TAG_PROCESS_BEGIN( Retention );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <RETENTION> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Retention,
                                      Extension );
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for mandatory retention tag
  if (NS_SUCCEEDED( rv )) {
    nsString       tagName;

    nsStringArray  tagChoices;

    tagName.AssignWithConversion( P3P_RETENTION_NORETENTION );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_RETENTION_STATEDPURPOSE );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_RETENTION_LEGALREQUIREMENT );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_RETENTION_INDEFINITELY );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_RETENTION_BUSINESSPRACTICES );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED( rv )) {
      NS_P3P_TAG_PROCESS_SINGLE_CHOICE_CHILD( RetentionValue,
                                              tagChoices,
                                              P3P,
                                              P3P_TAG_MANDATORY );
    }
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <RETENTION> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Retention );

// P3P Retention Tag: Clone the RETENTION tag node
NS_P3P_TAG_CLONE_NODE( Retention );

// P3P Retention Tag: Clone the RETENTION tag tree
NS_P3P_TAG_CLONE_TREE( Retention );


// ****************************************************************************
// nsP3PDataGroupTag Implementation routines
// ****************************************************************************

// P3P DataGroup Tag: Creation routine
NS_P3P_TAG_NEWTAG( DataGroup );

// P3P DataGroup Tag: Process the DATA-GROUP tag
NS_P3P_TAG_PROCESS_BEGIN( DataGroup );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <DATA-GROUP> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( DataGroup,
                                      Extension );
  }

  // Create an nsISupportsArray to hold the DATA tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( DataGroup,
                                      Data );
  }

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_BASE_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mBase,
                 &mBasePresent );
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for mandatory DATA tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Data,
                                       DATA,
                                       P3P,
                                       P3P_TAG_MANDATORY );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <DATA-GROUP> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( DataGroup );

// P3P DataGroup Tag: Clone the DATA-GROUP tag node
NS_P3P_TAG_CLONE_NODE( DataGroup );

// P3P DataGroup Tag: Clone the DATA-GROUP tag tree
NS_P3P_TAG_CLONE_TREE( DataGroup );


// ****************************************************************************
// nsP3PDataTag Implementation routines
// ****************************************************************************

// P3P Data Tag: Creation routine
NS_P3P_TAG_NEWTAG( Data );

// P3P Data Tag: Process the DATA tag
NS_P3P_TAG_PROCESS_BEGIN( Data );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <DATA> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_REF_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mRef );

    GetAttribute( P3P_OPTIONAL_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mOptional );
  }

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mData );
  }

  // Check for mandatory ref attribute
  if (NS_SUCCEEDED( rv )) {

    if (mRef.Length( ) > 0) {
      PRInt32  iFragmentID = mRef.Find( "#" );

      if (iFragmentID >= 0) {
        mRef.Left( mRefDataSchemaURISpec,
                   iFragmentID );
        mRef.Right( mRefDataStructName,
                    mRef.Length( ) - (iFragmentID + 1) );
      }
      else {
        rv = NS_ERROR_FAILURE;
      }
    }
    else {
      rv = NS_ERROR_FAILURE;
    }
  }

  // Check the value of the optional attribute
  if (NS_SUCCEEDED( rv )) {

    if (mOptional.Length( ) > 0) {

      if (!(mOptional.EqualsWithConversion( P3P_YES_VALUE ) ||
            mOptional.EqualsWithConversion( P3P_NO_VALUE ))) {
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  // Look for optional CATEGORIES tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Categories,
                                     CATEGORIES,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <DATA> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Data );

// P3P Data Tag: Clone the DATA tag node
NS_P3P_TAG_CLONE_NODE( Data );

// P3P Data Tag: Clone the DATA tag tree
NS_P3P_TAG_CLONE_TREE( Data );


// ****************************************************************************
// nsP3PDataSchemaTag Implementation routines
// ****************************************************************************

// P3P DataSchema Tag: Creation routine
NS_P3P_TAG_NEWTAG( DataSchema );

// P3P DataSchema Tag: Process the DATASCHEMA tag
NS_P3P_TAG_PROCESS_BEGIN( DataSchema );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <DATASCHEMA> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the EXTENSION tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( DataSchema,
                                      Extension );
  }

  // Create an nsISupportsArray to hold the DATASTRUCT tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( DataSchema,
                                      DataStruct );
  }

  // Create an nsISupportsArray to hold the DATADEF tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( DataSchema,
                                      DataDef );
  }

  // Look for optional EXTENSION tag(s)
  // (optional unless explicitly stated in the tag - handled in EXTENSION tag class)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( Extension,
                                       EXTENSION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for optional DATA-STRUCT tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( DataStruct,
                                       DATASTRUCT,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  // Look for optional DATA-DEF tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( DataDef,
                                       DATADEF,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <DATASCHEMA> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( DataSchema );

// P3P DataSchema Tag: Clone the DATASCHEMA tag node
NS_P3P_TAG_CLONE_NODE( DataSchema );

// P3P DataSchema Tag: Clone the DATASCHEMA tag tree
NS_P3P_TAG_CLONE_TREE( DataSchema );


// ****************************************************************************
// nsP3PDataStructTag Implementation routines
// ****************************************************************************

// P3P DataStruct Tag: Creation routine
NS_P3P_TAG_NEWTAG( DataStruct );

// P3P DataStruct Tag: Process the DATA-STRUCT tag
NS_P3P_TAG_PROCESS_BEGIN( DataStruct );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <DATA-STRUCT> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_NAME_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mName );

    GetAttribute( P3P_STRUCTREF_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mStructRef );

    GetAttribute( P3P_SHORTDESCRIPTION_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mShortDescription );
  }

  // Check for mandatory name attribute
  if (NS_SUCCEEDED( rv )) {

    if (mName.Length( ) == 0) {
      rv = NS_ERROR_FAILURE;
    }
  }

  // Check the value of the structref attribute
  if (NS_SUCCEEDED( rv )) {

    if (mStructRef.Length( ) > 0) {
      // Make sure the structref attribute is valid
      PRInt32  iFragmentID = mStructRef.Find( "#" );

      if (iFragmentID >= 0) {
        mStructRef.Left( mStructRefDataSchemaURISpec,
                         iFragmentID );
        mStructRef.Right( mStructRefDataStructName,
                          mStructRef.Length( ) - (iFragmentID + 1) );
      }
      else {
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  // Look for optional CATEGORIES tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Categories,
                                     CATEGORIES,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  // Look for optional LONG-DESCRIPTION tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( LongDescription,
                                     LONGDESCRIPTION,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <DATA-STRUCT> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( DataStruct );

// P3P DataStruct Tag: Clone the DATA-STRUCT tag node
NS_P3P_TAG_CLONE_NODE( DataStruct );

// P3P DataStruct Tag: Clone the DATA-STRUCT tag tree
NS_P3P_TAG_CLONE_TREE( DataStruct );


// ****************************************************************************
// nsP3PDataDefTag Implementation routines
// ****************************************************************************

// P3P DataDef Tag: Creation routine
NS_P3P_TAG_NEWTAG( DataDef );

// P3P DataDef Tag: Process the DATA-DEF tag
NS_P3P_TAG_PROCESS_BEGIN( DataDef );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <DATA-DEF> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_NAME_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mName );

    GetAttribute( P3P_STRUCTREF_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mStructRef );

    GetAttribute( P3P_SHORTDESCRIPTION_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mShortDescription );
  }

  // Check for mandatory name attribute
  if (NS_SUCCEEDED( rv )) {

    if (mName.Length( ) == 0) {
      rv = NS_ERROR_FAILURE;
    }
  }

  // Check the value of the structref attribute
  if (NS_SUCCEEDED( rv )) {

    if (mStructRef.Length( ) > 0) {
      // Make sure the structref attribute is valid
      PRInt32  iFragmentID = mStructRef.Find( "#" );

      if (iFragmentID >= 0) {
        mStructRef.Left( mStructRefDataSchemaURISpec,
                         iFragmentID );
        mStructRef.Right( mStructRefDataStructName,
                          mStructRef.Length( ) - (iFragmentID + 1) );
      }
      else {
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  // Look for optional CATEGORIES tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( Categories,
                                     CATEGORIES,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  // Look for optional LONG-DESCRIPTION tag
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SINGLE_CHILD( LongDescription,
                                     LONGDESCRIPTION,
                                     P3P,
                                     P3P_TAG_OPTIONAL );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <DATA-DEF> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( DataDef );

// P3P DataDef Tag: Clone the DATA-DEF tag node
NS_P3P_TAG_CLONE_NODE( DataDef );

// P3P DataDef Tag: Clone the DATA-DEF tag tree
NS_P3P_TAG_CLONE_TREE( DataDef );


// ****************************************************************************
// nsP3PExtensionTag Implementation routines
// ****************************************************************************

// P3P Extension Tag: Creation routine
NS_P3P_TAG_NEWTAG( Extension );

// P3P Extension Tag: Process the EXTENSION tag
NS_P3P_TAG_PROCESS_BEGIN( Extension );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <EXTENSION> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_OPTIONAL_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mOptional );
  }

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mExtension );
  }

  // Check the value of the optional attribute
  if (NS_SUCCEEDED( rv )) {

    if (mOptional.Length( ) > 0) {

      if (!(mOptional.EqualsWithConversion( P3P_YES_VALUE ) ||
            mOptional.EqualsWithConversion( P3P_NO_VALUE ))) {
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  // Check if the EXTENSION tag is optional, fail if not
  if (NS_SUCCEEDED( rv )) {

    if (mOptional.Length( ) > 0) {

      if (mOptional.EqualsWithConversion( P3P_NO_VALUE )) {
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <EXTENSION> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Extension );

// P3P Extension Tag: Clone the EXTENSION tag node
NS_P3P_TAG_CLONE_NODE( Extension );

// P3P Extension Tag: Clone the EXTENSION tag tree
NS_P3P_TAG_CLONE_TREE( Extension );


// ****************************************************************************
// nsP3PExpiryTag Implementation routines
// ****************************************************************************

// P3P Expiry Tag: Creation routine
NS_P3P_TAG_NEWTAG( Expiry );

// P3P Expiry Tag: Process the EXPRIY tag
NS_P3P_TAG_PROCESS_BEGIN( Expiry );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <EXPIRY> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_DATE_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mDate );

    GetAttribute( P3P_MAXAGE_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mMaxAge );
  }

  // Check for mutually exclusive mandatory date/max-age attribute
  if (NS_SUCCEEDED( rv )) {

    if (mDate.Length( ) && mMaxAge.Length( )) {
      rv = NS_ERROR_FAILURE;
    }
    else if (!mDate.Length( ) && !mMaxAge.Length( )) {
      rv = NS_ERROR_FAILURE;
    }
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <EXPIRY> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Expiry );

// P3P Expiry Tag: Clone the EXPIRY tag node
NS_P3P_TAG_CLONE_NODE( Expiry );

// P3P Expiry Tag: Clone the EXPIRY tag tree
NS_P3P_TAG_CLONE_TREE( Expiry );


// ****************************************************************************
// nsP3PLongDescriptionTag Implementation routines
// ****************************************************************************

// P3P LongDescription Tag: Creation routine
NS_P3P_TAG_NEWTAG( LongDescription );

// P3P LongDescription Tag: Process the LONG-DESCRIPTION tag
NS_P3P_TAG_PROCESS_BEGIN( LongDescription );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <LONG-DESCRIPTION> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mLongDescription );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <LONG-DESCRIPTION> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( LongDescription );

// P3P LongDescription Tag: Clone the LONG-DESCRIPTION tag node
NS_P3P_TAG_CLONE_NODE( LongDescription );

// P3P LongDescription Tag: Clone the LONG-DESCRIPTION tag tree
NS_P3P_TAG_CLONE_TREE( LongDescription );


// ****************************************************************************
// nsP3PCategoriesTag Implementation routines
// ****************************************************************************

// P3P Categories Tag: Creation routine
NS_P3P_TAG_NEWTAG( Categories );

// P3P Categories Tag: Process the CATEGORIES tag
NS_P3P_TAG_PROCESS_BEGIN( Categories );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <CATEGORIES> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Create an nsISupportsArray to hold the category tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( Categories,
                                      CategoryValue );
  }

  // Look for mandatory categories tag(s)
  if (NS_SUCCEEDED( rv )) {
    nsString       tagName;

    nsStringArray  tagChoices;

    tagName.AssignWithConversion( P3P_CATEGORIES_PHYSICAL );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_ONLINE );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_UNIQUEID );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_PURCHASE );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_FINANCIAL );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_COMPUTER );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_NAVIGATION );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_INTERACTIVE );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_DEMOGRAPHIC );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_CONTENT );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_STATE );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_POLITICAL );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_HEALTH );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_PREFERENCE );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_LOCATION );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_GOVERNMENT );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }
    tagName.AssignWithConversion( P3P_CATEGORIES_OTHER );
    if (!tagChoices.AppendString( tagName )) {
      rv = NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED( rv )) {
      NS_P3P_TAG_PROCESS_MULTIPLE_CHOICE_CHILD( CategoryValue,
                                                tagChoices,
                                                P3P,
                                                P3P_TAG_MANDATORY );
    }
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <CATEGORIES> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( Categories );

// P3P Categories Tag: Clone the CATEGORIES tag node
NS_P3P_TAG_CLONE_NODE( Categories );

// P3P Categories Tag: Clone the CATEGORIES tag tree
NS_P3P_TAG_CLONE_TREE( Categories );


// ****************************************************************************
// nsP3PAccessValueTag Implementation routines
// ****************************************************************************

// P3P AccessValue Tag: Creation routine
NS_P3P_TAG_NEWTAG( AccessValue );

// P3P AccessValue Tag: Process the access value tag
NS_P3P_TAG_PROCESS_BEGIN( AccessValue );

  NS_P3P_TAG_PROCESS_COMMON;

  nsCAutoString  csTagName; csTagName.AssignWithConversion( mTagName );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing access value <%s> tag.\n", (const char *)csTagName) );

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing access value <%s> tag.\n", (const char *)csTagName) );
  }

NS_P3P_TAG_PROCESS_END( AccessValue );

// P3P AccessValue Tag: Clone the access value tag node
NS_P3P_TAG_CLONE_NODE( AccessValue );

// P3P AccessValue Tag: Clone the access value tag tree
NS_P3P_TAG_CLONE_TREE( AccessValue );


// ****************************************************************************
// nsP3PRemedyValueTag Implementation routines
// ****************************************************************************

// P3P RemedyValue Tag: Creation routine
NS_P3P_TAG_NEWTAG( RemedyValue );

// P3P RemedyValue Tag: Process the remedy value tag
NS_P3P_TAG_PROCESS_BEGIN( RemedyValue );

  NS_P3P_TAG_PROCESS_COMMON;

  nsCAutoString  csTagName; csTagName.AssignWithConversion( mTagName );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing remedy value <%s> tag.\n", (const char *)csTagName) );

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing remedy value <%s> tag.\n", (const char *)csTagName) );
  }

NS_P3P_TAG_PROCESS_END( RemedyValue );

// P3P RemedyValue Tag: Clone the remedy value tag node
NS_P3P_TAG_CLONE_NODE( RemedyValue );

// P3P RemedyValue Tag: Clone the remedy value tag tree
NS_P3P_TAG_CLONE_TREE( RemedyValue );


// ****************************************************************************
// nsP3PPurposeValueTag Implementation routines
// ****************************************************************************

// P3P PurposeValue Tag: Creation routine
NS_P3P_TAG_NEWTAG( PurposeValue );

// P3P PurposeValue Tag: Process the purpose value tag
NS_P3P_TAG_PROCESS_BEGIN( PurposeValue );

  NS_P3P_TAG_PROCESS_COMMON;

  nsCAutoString  csTagName; csTagName.AssignWithConversion( mTagName );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing purpose value <%s> tag.\n", (const char *)csTagName) );

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_REQUIRED_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mRequired );
  }

  // Check the value of the required attribute
  if (NS_SUCCEEDED( rv )) {

    if (mRequired.Length( ) > 0) {

      if (!(mRequired.EqualsWithConversion( P3P_ALWAYS_VALUE ) ||
            mRequired.EqualsWithConversion( P3P_OPTIN_VALUE )  ||
            mRequired.EqualsWithConversion( P3P_OPTOUT_VALUE ))) {
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mPurposeValue );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing purpose value <%s> tag.\n", (const char *)csTagName) );
  }

NS_P3P_TAG_PROCESS_END( PurposeValue );

// P3P PurposeValue Tag: Clone the purpose value tag node
NS_P3P_TAG_CLONE_NODE( PurposeValue );

// P3P PurposeValue Tag: Clone the purpose value tag tree
NS_P3P_TAG_CLONE_TREE( PurposeValue );


// ****************************************************************************
// nsP3PRecipientValueTag Implementation routines
// ****************************************************************************

// P3P RecipientValue Tag: Creation routine
NS_P3P_TAG_NEWTAG( RecipientValue );

// P3P RecipientValue Tag: Process the recipient value tag
NS_P3P_TAG_PROCESS_BEGIN( RecipientValue );

  NS_P3P_TAG_PROCESS_COMMON;

  nsCAutoString  csTagName; csTagName.AssignWithConversion( mTagName );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing recipient value <%s> tag.\n", (const char *)csTagName) );

  // Create an nsISupportsArray to hold the recipient-description tags
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_SUPPORTSARRAY( RecipientValue,
                                      RecipientDescription );
  }

  // Look for all of the tag attributes
  if (NS_SUCCEEDED( rv )) {
    GetAttribute( P3P_REQUIRED_ATTRIBUTE,
                  P3P_P3P_NAMESPACE,
                  mRequired );
  }

  // Check the value of the required attribute
  if (NS_SUCCEEDED( rv )) {

    if (mRequired.Length( ) > 0) {

      if (!mTagName.EqualsWithConversion( P3P_RECIPIENT_OURS )) {

        if (!(mRequired.EqualsWithConversion( P3P_ALWAYS_VALUE ) ||
              mRequired.EqualsWithConversion( P3P_OPTIN_VALUE )  ||
              mRequired.EqualsWithConversion( P3P_OPTOUT_VALUE ))) {
          rv = NS_ERROR_FAILURE;
        }
      }
      else {
        // <ours> tag does not support the required attribute
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  // Look for optional recipient-description tag(s)
  if (NS_SUCCEEDED( rv )) {
    NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( RecipientDescription,
                                       RECIPIENTDESCRIPTION,
                                       P3P,
                                       P3P_TAG_OPTIONAL );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing recipient value <%s> tag.\n", (const char *)csTagName) );
  }

NS_P3P_TAG_PROCESS_END( RecipientValue );

// P3P RecipientValue Tag: Clone the recipient value tag node
NS_P3P_TAG_CLONE_NODE( RecipientValue );

// P3P RecipientValue Tag: Clone the recipient value tag tree
NS_P3P_TAG_CLONE_TREE( RecipientValue );


// ****************************************************************************
// nsP3PRecipientDescriptionTag Implementation routines
// ****************************************************************************

// P3P RecipientDescription Tag: Creation routine
NS_P3P_TAG_NEWTAG( RecipientDescription );

// P3P RecipientDescription Tag: Process the recipient-description tag
NS_P3P_TAG_PROCESS_BEGIN( RecipientDescription );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing <recipient-description> tag.\n") );

  NS_P3P_TAG_PROCESS_COMMON;

  // Get the actual data of the tag
  if (NS_SUCCEEDED( rv )) {
    GetText( mRecipientDescription );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing <recipient-description> tag.\n") );
  }

NS_P3P_TAG_PROCESS_END( RecipientDescription );

// P3P RecipientDescription Tag: Clone the recipient-description tag node
NS_P3P_TAG_CLONE_NODE( RecipientDescription );

// P3P RecipientDescription Tag: Clone the recipient-description tag tree
NS_P3P_TAG_CLONE_TREE( RecipientDescription );


// ****************************************************************************
// nsP3PRetentionValueTag Implementation routines
// ****************************************************************************

// P3P RetentionValue Tag: Creation routine
NS_P3P_TAG_NEWTAG( RetentionValue );

// P3P RetentionValue Tag: Process the retention value tag
NS_P3P_TAG_PROCESS_BEGIN( RetentionValue );

  NS_P3P_TAG_PROCESS_COMMON;

  nsCAutoString  csTagName; csTagName.AssignWithConversion( mTagName );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing retention value <%s> tag.\n", (const char *)csTagName) );

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing retention value <%s> tag.\n", (const char *)csTagName) );
  }

NS_P3P_TAG_PROCESS_END( RetentionValue );

// P3P RetentionValue Tag: Clone the retention value tag node
NS_P3P_TAG_CLONE_NODE( RetentionValue );

// P3P RetentionValue Tag: Clone the retention value tag tree
NS_P3P_TAG_CLONE_TREE( RetentionValue );


// ****************************************************************************
// nsP3PCategoryValueTag Implementation routines
// ****************************************************************************

// P3P CategoryValue Tag: Creation routine
NS_P3P_TAG_NEWTAG( CategoryValue );

// P3P CategoryValue Tag: Process the category value tag
NS_P3P_TAG_PROCESS_BEGIN( CategoryValue );

  NS_P3P_TAG_PROCESS_COMMON;

  nsCAutoString  csTagName; csTagName.AssignWithConversion( mTagName );

  PR_LOG( gP3PLogModule,
          PR_LOG_NOTICE,
          ("P3PTag:  ProcessTag, processing category value <%s> tag.\n", (const char *)csTagName) );

  // Get the actual data of the tag (only valid for other tag)
  if (NS_SUCCEEDED( rv )) {
    GetText( mCategoryValue );
  }

  if (NS_FAILED( rv )) {
    PR_LOG( gP3PLogModule,
            PR_LOG_ERROR,
            ("P3PTag:  ProcessTag, failure processing category value <%s> tag.\n", (const char *)csTagName) );
  }

NS_P3P_TAG_PROCESS_END( CategoryValue );

// P3P CategoryValue Tag: Clone the category value tag node
NS_P3P_TAG_CLONE_NODE( CategoryValue );

// P3P CategoryValue Tag: Clone the category value tag tree
NS_P3P_TAG_CLONE_TREE( CategoryValue );
