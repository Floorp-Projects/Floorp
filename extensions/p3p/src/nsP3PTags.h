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

#ifndef nsP3PTags_h__
#define nsP3PTags_h__

#include "nsIP3PTag.h"
#include "nsP3PXMLUtils.h"

#include <nsVoidArray.h>


class nsP3PTag : public nsIP3PTag {
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIP3PTag methods
  NS_DECL_NSIP3PTAG

  // nsP3PTag methods
  nsP3PTag( );
  virtual ~nsP3PTag( );

  NS_IMETHOD            Init( nsIDOMNode      *aDOMNode,
                              nsIP3PTag       *aParent );

protected:
  nsCOMPtr<nsIDOMNode>       mDOMNode;                // The DOM Node of the tag

  nsString                   mTagName;                // The name of the tag

  nsString                   mTagNameSpace;           // The namespace of the tag

  nsStringArray              mAttributeNames,         // The attribute information
                             mAttributeNameSpaces,
                             mAttributeValues;

  nsString                   mText;                   // The text associated with the tag

  nsCOMPtr<nsIP3PTag>        mParent;                 // The parent Tag object

  nsCOMPtr<nsISupportsArray> mChildren;               // The child Tag objects
};

extern
NS_EXPORT NS_METHOD     NS_NewP3PTag( nsIDOMNode      *aDOMNode,
                                      nsIP3PTag       *aParent,
                                      nsIP3PTag      **aP3PTag );


// ****************************************************************************
// P3P Tag Macros
// ****************************************************************************

// P3P Tag Macro: Define the Tag class in the header file
#define NS_P3P_TAG_CLASS_BEGIN( _tag )                                                          \
class nsP3P##_tag##Tag : public nsP3PTag {                                                      \
public:                                                                                         \
  NS_IMETHOD       ProcessTag( );                                                               \
                                                                                                \
  NS_IMETHOD       CloneNode( nsIP3PTag  *aParent,                                              \
                              nsIP3PTag **aNewTag );                                            \
                                                                                                \
  NS_IMETHOD       CloneTree( nsIP3PTag  *aParent,                                              \
                              nsIP3PTag **aNewTag );

#define NS_P3P_TAG_CLASS_END( _tag )                                                            \
};                                                                                              \
                                                                                                \
extern                                                                                          \
NS_EXPORT NS_METHOD     NS_NewP3P##_tag##Tag( nsIDOMNode      *aDOMNode,                        \
                                              nsIP3PTag       *aParent,                         \
                                              nsIP3PTag      **aP3PTag );

// P3P Tag Macro: Define the Tag class object creation routine
#define NS_P3P_TAG_NEWTAG( _tag )                                                               \
NS_METHOD                                                                                       \
NS_NewP3P##_tag##Tag( nsIDOMNode      *aDOMNode,                                                \
                      nsIP3PTag       *aParent,                                                 \
                      nsIP3PTag      **aP3PTag ) {                                              \
  nsresult   rv;                                                                                \
                                                                                                \
  nsP3PTag  *pNewP3PTag = nsnull;                                                               \
                                                                                                \
                                                                                                \
  NS_ENSURE_ARG_POINTER( aP3PTag );                                                             \
                                                                                                \
  pNewP3PTag = new nsP3P##_tag##Tag( );                                                         \
                                                                                                \
  if (pNewP3PTag) {                                                                             \
    NS_ADDREF( pNewP3PTag );                                                                    \
                                                                                                \
    rv = pNewP3PTag->Init( aDOMNode,                                                            \
                           aParent );                                                           \
                                                                                                \
    if (NS_SUCCEEDED( rv )) {                                                                   \
      rv = pNewP3PTag->QueryInterface( NS_GET_IID( nsIP3PTag ),                                 \
                              (void **)aP3PTag );                                               \
    }                                                                                           \
                                                                                                \
    NS_RELEASE( pNewP3PTag );                                                                   \
  }                                                                                             \
  else {                                                                                        \
    NS_ASSERTION( 0, "P3P:  P3P##_tag##Tag unable to be created.\n" );                          \
    rv = NS_ERROR_OUT_OF_MEMORY;                                                                \
  }                                                                                             \
                                                                                                \
  return rv;                                                                                    \
}

// P3P Tag Macro: Create an nsISupportsArray object
#define NS_P3P_TAG_PROCESS_SUPPORTSARRAY( _tag, _array )                                        \
  m##_array##Tags = do_CreateInstance( NS_SUPPORTSARRAY_CONTRACTID,                             \
                                      &rv );                                                    \
                                                                                                \
  if (NS_FAILED( rv )) {                                                                        \
    PR_LOG( gP3PLogModule,                                                                      \
            PR_LOG_ERROR,                                                                       \
            ("P3PTag:  ProcessTag, unable to create ##_array## tag array.\n") );                \
  }                                                                                             \
                                                                                                \
  NS_ASSERTION( m##_array##Tags,                                                                \
                "P3P:  P3P##_tag##Tag unable to create ##_array## array\n" );

// P3P Tag Macro: Define the beginning of the Tag class ProcessTag method
#define NS_P3P_TAG_PROCESS_BEGIN( _tag )                                                        \
NS_IMETHODIMP                                                                                   \
nsP3P##_tag##Tag::ProcessTag( ) {                                                               \
                                                                                                \
  nsresult              rv;                                                                     \
                                                                                                \
  nsCOMPtr<nsIP3PTag>   pTag;                                                                   \
                                                                                                \
  nsCOMPtr<nsIDOMNode>  pDOMNodeWithTag;

// P3P Tag Macro: Define the common processing of the ProcessTag method
#define NS_P3P_TAG_PROCESS_COMMON                                                               \
  rv = nsP3PXMLUtils::GetName( mDOMNode,                                                        \
                               mTagName );                                                      \
                                                                                                \
  if (NS_SUCCEEDED( rv )) {                                                                     \
    rv = nsP3PXMLUtils::GetNameSpace( mDOMNode,                                                 \
                                      mTagNameSpace );                                          \
                                                                                                \
    if (NS_SUCCEEDED( rv )) {                                                                   \
      rv = nsP3PXMLUtils::GetAttributes( mDOMNode,                                              \
                                        &mAttributeNames,                                       \
                                        &mAttributeNameSpaces,                                  \
                                        &mAttributeValues );                                    \
                                                                                                \
      if (NS_SUCCEEDED( rv )) {                                                                 \
        rv = nsP3PXMLUtils::GetText( mDOMNode,                                                  \
                                     mText );                                                   \
                                                                                                \
        if (NS_FAILED( rv )) {                                                                  \
          PR_LOG( gP3PLogModule,                                                                \
                  PR_LOG_ERROR,                                                                 \
                  ("P3PTag:  ProcessTag, unable to obtain tag text.\n") );                      \
        }                                                                                       \
      }                                                                                         \
      else {                                                                                    \
        PR_LOG( gP3PLogModule,                                                                  \
                PR_LOG_ERROR,                                                                   \
                ("P3PTag:  ProcessTag, unable to obtain tag attributes.\n") );                  \
      }                                                                                         \
    }                                                                                           \
    else {                                                                                      \
      PR_LOG( gP3PLogModule,                                                                    \
              PR_LOG_ERROR,                                                                     \
              ("P3PTag:  ProcessTag, unable to obtain tag name space.\n") );                    \
    }                                                                                           \
  }                                                                                             \
  else {                                                                                        \
    PR_LOG( gP3PLogModule,                                                                      \
            PR_LOG_ERROR,                                                                       \
            ("P3PTag:  ProcessTag, unable to obtain tag name.\n") );                            \
  }

// P3P Tag Macro: Define single tag processing
#define NS_P3P_TAG_PROCESS_SINGLE( _tag, _tagCap, _ns, _opt )                                   \
  {                                                                                             \
    pDOMNodeWithTag = nsnull;                                                                   \
    rv = nsP3PXMLUtils::FindTag( P3P_##_tagCap##_TAG,                                           \
                                 P3P_##_ns##_NAMESPACE,                                         \
                                 mDOMNode,                                                      \
                                 getter_AddRefs( pDOMNodeWithTag ) );                           \
                                                                                                \
    if (NS_SUCCEEDED( rv )) {                                                                   \
                                                                                                \
      if (pDOMNodeWithTag) {                                                                    \
        rv = NS_NewP3P##_tag##Tag( pDOMNodeWithTag,                                             \
                                   nsnull,                                                      \
                                   getter_AddRefs( pTag ) );                                    \
                                                                                                \
        if (NS_SUCCEEDED( rv )) {                                                               \
          rv = pTag->ProcessTag( );                                                             \
        }                                                                                       \
        else {                                                                                  \
          PR_LOG( gP3PLogModule,                                                                \
                  PR_LOG_ERROR,                                                                 \
                  ("P3PTag:  ProcessTag, NS_NewP3P"#_tag"Tag failed - %X.\n", rv) );            \
        }                                                                                       \
      }                                                                                         \
      else {                                                                                    \
        if (!_opt) {                                                                            \
          PR_LOG( gP3PLogModule,                                                                \
                  PR_LOG_ERROR,                                                                 \
                  ("P3PTag:  ProcessTag, <%s> tag not found.\n", P3P_##_tagCap##_TAG) );        \
                                                                                                \
          rv = NS_ERROR_FAILURE;                                                                \
        }                                                                                       \
      }                                                                                         \
    }                                                                                           \
  }

// P3P Tag Macro: Define single tag processing
#define NS_P3P_TAG_PROCESS_SINGLE_CHILD( _tag, _tagCap, _ns, _opt )                             \
  {                                                                                             \
    pDOMNodeWithTag = nsnull;                                                                   \
    rv = nsP3PXMLUtils::FindTagInChildren( P3P_##_tagCap##_TAG,                                 \
                                           P3P_##_ns##_NAMESPACE,                               \
                                           mDOMNode,                                            \
                                           getter_AddRefs( pDOMNodeWithTag ) );                 \
                                                                                                \
    if (NS_SUCCEEDED( rv )) {                                                                   \
                                                                                                \
      if (pDOMNodeWithTag) {                                                                    \
        rv = NS_NewP3P##_tag##Tag( pDOMNodeWithTag,                                             \
                                   this,                                                        \
                                   getter_AddRefs( pTag ) );                                    \
                                                                                                \
        if (NS_SUCCEEDED( rv )) {                                                               \
          m##_tag##Tag = pTag;                                                                  \
          rv = pTag->ProcessTag( );                                                             \
        }                                                                                       \
        else {                                                                                  \
          PR_LOG( gP3PLogModule,                                                                \
                  PR_LOG_ERROR,                                                                 \
                  ("P3PTag:  ProcessTag, NS_NewP3P"#_tag"Tag failed - %X.\n", rv) );            \
        }                                                                                       \
      }                                                                                         \
      else {                                                                                    \
        if (!_opt) {                                                                            \
          PR_LOG( gP3PLogModule,                                                                \
                  PR_LOG_ERROR,                                                                 \
                  ("P3PTag:  ProcessTag, <%s> tag not found.\n", P3P_##_tagCap##_TAG) );        \
                                                                                                \
          rv = NS_ERROR_FAILURE;                                                                \
        }                                                                                       \
      }                                                                                         \
    }                                                                                           \
  }

// P3P Tag Macro: Define multiple tag processing
#define NS_P3P_TAG_PROCESS_MULTIPLE_CHILD( _tag, _tagCap, _ns, _opt )                           \
  {                                                                                             \
    PRBool   bFound      = PR_FALSE;                                                            \
                                                                                                \
    PRInt32  iStartChild =  0,                                                                  \
             iFoundChild = -1;                                                                  \
                                                                                                \
    do {                                                                                        \
      iStartChild = iFoundChild + 1;                                                            \
                                                                                                \
      pDOMNodeWithTag = nsnull;                                                                 \
      rv = nsP3PXMLUtils::FindTagInChildren( P3P_##_tagCap##_TAG,                               \
                                             P3P_##_ns##_NAMESPACE,                             \
                                             mDOMNode,                                          \
                                             getter_AddRefs( pDOMNodeWithTag ),                 \
                                             iStartChild,                                       \
                                             iFoundChild );                                     \
                                                                                                \
      if (NS_SUCCEEDED( rv ) && pDOMNodeWithTag) {                                              \
        bFound = PR_TRUE;                                                                       \
                                                                                                \
        rv = NS_NewP3P##_tag##Tag( pDOMNodeWithTag,                                             \
                                   this,                                                        \
                                   getter_AddRefs( pTag ) );                                    \
                                                                                                \
        if (NS_SUCCEEDED( rv )) {                                                               \
                                                                                                \
          if (m##_tag##Tags->AppendElement( pTag )) {                                           \
            rv = pTag->ProcessTag( );                                                           \
          }                                                                                     \
          else {                                                                                \
            PR_LOG( gP3PLogModule,                                                              \
                    PR_LOG_ERROR,                                                               \
                    ("P3PTag:  ProcessTag, m##_tag##Tags->AppendElement failed - %X.\n", rv) ); \
                                                                                                \
            rv = NS_ERROR_FAILURE;                                                              \
          }                                                                                     \
        }                                                                                       \
        else {                                                                                  \
          PR_LOG( gP3PLogModule,                                                                \
                  PR_LOG_ERROR,                                                                 \
                  ("P3PTag:  ProcessTag, NS_NewP3P"#_tag"Tag failed - %X.\n", rv) );            \
        }                                                                                       \
      }                                                                                         \
    } while (NS_SUCCEEDED( rv ) && pDOMNodeWithTag);                                            \
                                                                                                \
    if (NS_SUCCEEDED( rv )) {                                                                   \
                                                                                                \
      if (!_opt && !bFound) {                                                                   \
        PR_LOG( gP3PLogModule,                                                                  \
                PR_LOG_ERROR,                                                                   \
                ("P3PTag:  ProcessTag, <%s> tag not found.\n", P3P_##_tagCap##_TAG) );          \
                                                                                                \
        rv = NS_ERROR_FAILURE;                                                                  \
      }                                                                                         \
    }                                                                                           \
  }

// P3P Tag Macro: Define single tag processing based on a choice of possible tags
#define NS_P3P_TAG_PROCESS_SINGLE_CHOICE_CHILD( _tag, _choices, _ns, _opt )                     \
  {                                                                                             \
    pDOMNodeWithTag = nsnull;                                                                   \
    rv = nsP3PXMLUtils::FindTagInChildren(&_choices,                                            \
                                           P3P_##_ns##_NAMESPACE,                               \
                                           mDOMNode,                                            \
                                           getter_AddRefs( pDOMNodeWithTag ) );                 \
                                                                                                \
    if (NS_SUCCEEDED( rv )) {                                                                   \
                                                                                                \
      if (pDOMNodeWithTag) {                                                                    \
        rv = NS_NewP3P##_tag##Tag( pDOMNodeWithTag,                                             \
                                   this,                                                        \
                                   getter_AddRefs( pTag ) );                                    \
                                                                                                \
        if (NS_SUCCEEDED( rv )) {                                                               \
          m##_tag##Tag = pTag;                                                                  \
          rv = pTag->ProcessTag( );                                                             \
        }                                                                                       \
        else {                                                                                  \
          PR_LOG( gP3PLogModule,                                                                \
                  PR_LOG_ERROR,                                                                 \
                  ("P3PTag:  ProcessTag, NS_NewP3P"#_tag"Tag failed - %X.\n", rv) );            \
        }                                                                                       \
      }                                                                                         \
      else {                                                                                    \
        if (!_opt) {                                                                            \
          PR_LOG( gP3PLogModule,                                                                \
                  PR_LOG_ERROR,                                                                 \
                  ("P3PTag:  ProcessTag, valid child tag not found.\n") );                      \
                                                                                                \
          rv = NS_ERROR_FAILURE;                                                                \
        }                                                                                       \
      }                                                                                         \
    }                                                                                           \
  }

// P3P Tag Macro: Define multiple tag processing based on a choice of possible tags
#define NS_P3P_TAG_PROCESS_MULTIPLE_CHOICE_CHILD( _tag, _choices, _ns, _opt )                   \
  {                                                                                             \
    PRBool   bFound      = PR_FALSE;                                                            \
                                                                                                \
    PRInt32  iStartChild =  0,                                                                  \
             iFoundChild = -1;                                                                  \
                                                                                                \
    do {                                                                                        \
      iStartChild = iFoundChild + 1;                                                            \
                                                                                                \
      pDOMNodeWithTag = nsnull;                                                                 \
      rv = nsP3PXMLUtils::FindTagInChildren(&_choices,                                          \
                                             P3P_##_ns##_NAMESPACE,                             \
                                             mDOMNode,                                          \
                                             getter_AddRefs( pDOMNodeWithTag ),                 \
                                             iStartChild,                                       \
                                             iFoundChild );                                     \
                                                                                                \
      if (NS_SUCCEEDED( rv ) && pDOMNodeWithTag) {                                              \
        bFound = PR_TRUE;                                                                       \
                                                                                                \
        rv = NS_NewP3P##_tag##Tag( pDOMNodeWithTag,                                             \
                                   this,                                                        \
                                   getter_AddRefs( pTag ) );                                    \
                                                                                                \
        if (NS_SUCCEEDED( rv )) {                                                               \
                                                                                                \
          if (m##_tag##Tags->AppendElement( pTag )) {                                           \
            rv = pTag->ProcessTag( );                                                           \
          }                                                                                     \
          else {                                                                                \
            PR_LOG( gP3PLogModule,                                                              \
                    PR_LOG_ERROR,                                                               \
                    ("P3PTag:  ProcessTag, m##_tag##Tags->AppendElement failed - %X.\n", rv) ); \
                                                                                                \
            rv = NS_ERROR_FAILURE;                                                              \
          }                                                                                     \
        }                                                                                       \
        else {                                                                                  \
          PR_LOG( gP3PLogModule,                                                                \
                  PR_LOG_ERROR,                                                                 \
                  ("P3PTag:  ProcessTag, NS_NewP3P"#_tag"Tag failed - %X.\n", rv) );            \
        }                                                                                       \
      }                                                                                         \
    } while (NS_SUCCEEDED( rv ) && pDOMNodeWithTag);                                            \
                                                                                                \
    if (NS_SUCCEEDED( rv )) {                                                                   \
                                                                                                \
      if (!_opt && !bFound) {                                                                   \
        PR_LOG( gP3PLogModule,                                                                  \
                PR_LOG_ERROR,                                                                   \
                ("P3PTag:  ProcessTag, valid child tag not found.\n") );                        \
                                                                                                \
        rv = NS_ERROR_FAILURE;                                                                  \
      }                                                                                         \
    }                                                                                           \
  }

// P3P Tag Macro: Define the end of the Tag class ProcessTag method
#define NS_P3P_TAG_PROCESS_END( _tag )                                                          \
  return rv;                                                                                    \
}

// P3P Tag Macro: Clone this node (no children)
#define NS_P3P_TAG_CLONE_NODE( _tag )                                                           \
NS_IMETHODIMP                                                                                   \
nsP3P##_tag##Tag::CloneNode( nsIP3PTag  *aParent,                                               \
                             nsIP3PTag **aNewTag ) {                                            \
                                                                                                \
  nsresult             rv;                                                                      \
                                                                                                \
  nsCOMPtr<nsIP3PTag>  pTag;                                                                    \
                                                                                                \
                                                                                                \
  NS_ENSURE_ARG_POINTER( aNewTag );                                                             \
                                                                                                \
  *aNewTag = nsnull;                                                                            \
                                                                                                \
  rv = NS_NewP3P##_tag##Tag( mDOMNode,                                                          \
                             aParent,                                                           \
                             getter_AddRefs( pTag ) );                                          \
                                                                                                \
  if (NS_SUCCEEDED( rv )) {                                                                     \
    rv = nsP3PTag::ProcessTag( );                                                               \
                                                                                                \
    if (NS_SUCCEEDED( rv )) {                                                                   \
      rv = pTag->QueryInterface( NS_GET_IID( nsIP3PTag ),                                       \
                        (void **)aNewTag );                                                     \
    }                                                                                           \
  }                                                                                             \
  else {                                                                                        \
    PR_LOG( gP3PLogModule,                                                                      \
            PR_LOG_ERROR,                                                                       \
            ("P3PTag:  ProcessTag, NS_NewP3P"#_tag"Tag failed - %X.\n", rv) );                  \
  }                                                                                             \
                                                                                                \
  return rv;                                                                                    \
}

// P3P Tag Macro: Clone this node and it's children
#define NS_P3P_TAG_CLONE_TREE( _tag )                                                           \
NS_IMETHODIMP                                                                                   \
nsP3P##_tag##Tag::CloneTree( nsIP3PTag  *aParent,                                               \
                             nsIP3PTag **aNewTag ) {                                            \
                                                                                                \
  nsresult             rv;                                                                      \
                                                                                                \
  nsCOMPtr<nsIP3PTag>  pTag;                                                                    \
                                                                                                \
                                                                                                \
  NS_ENSURE_ARG_POINTER( aNewTag );                                                             \
                                                                                                \
  *aNewTag = nsnull;                                                                            \
                                                                                                \
  rv = NS_NewP3P##_tag##Tag( mDOMNode,                                                          \
                             aParent,                                                           \
                             getter_AddRefs( pTag ) );                                          \
                                                                                                \
  if (NS_SUCCEEDED( rv )) {                                                                     \
    rv = pTag->ProcessTag( );                                                                   \
                                                                                                \
    if (NS_SUCCEEDED( rv )) {                                                                   \
      rv = pTag->QueryInterface( NS_GET_IID( nsIP3PTag ),                                       \
                        (void **)aNewTag );                                                     \
    }                                                                                           \
  }                                                                                             \
  else {                                                                                        \
    PR_LOG( gP3PLogModule,                                                                      \
            PR_LOG_ERROR,                                                                       \
            ("P3PTag:  ProcessTag, NS_NewP3P"#_tag"Tag failed - %X.\n", rv) );                  \
  }                                                                                             \
                                                                                                \
  return rv;                                                                                    \
}




// ****************************************************************************
// P3P Policy Ref File Tags
// ****************************************************************************

// P3P META Tag
NS_P3P_TAG_CLASS_BEGIN( Meta )

public:
  nsString                   mMeta;
  nsCOMPtr<nsIP3PTag>        mPolicyReferencesTag;

NS_P3P_TAG_CLASS_END( Meta )


// P3P POLICY-REFERENCES Tag
NS_P3P_TAG_CLASS_BEGIN( PolicyReferences )

public:
  nsCOMPtr<nsIP3PTag>        mExpiryTag;
  nsCOMPtr<nsISupportsArray> mPolicyRefTags;
  nsCOMPtr<nsIP3PTag>        mPoliciesTag;

NS_P3P_TAG_CLASS_END( PolicyReferences )


// P3P POLICIES Tag
NS_P3P_TAG_CLASS_BEGIN( Policies )

public:
  nsCOMPtr<nsISupportsArray> mPolicyTags;

NS_P3P_TAG_CLASS_END( Policies )


// P3P POLICY-REF Tag
NS_P3P_TAG_CLASS_BEGIN( PolicyRef )

public:
  nsString                   mAbout;
  nsCOMPtr<nsISupportsArray> mIncludeTags;
  nsCOMPtr<nsISupportsArray> mExcludeTags;
  nsCOMPtr<nsISupportsArray> mEmbeddedIncludeTags;
  nsCOMPtr<nsISupportsArray> mEmbeddedExcludeTags;
  nsCOMPtr<nsISupportsArray> mMethodTags;

NS_P3P_TAG_CLASS_END( PolicyRef )


// P3P INCLUDE Tag
NS_P3P_TAG_CLASS_BEGIN( Include )

public:
  nsString                   mInclude;

NS_P3P_TAG_CLASS_END( Include )


// P3P EXCLUDE Tag
NS_P3P_TAG_CLASS_BEGIN( Exclude )

public:
  nsString                   mExclude;

NS_P3P_TAG_CLASS_END( Exclude )


// P3P EMBEDDED-INCLUDE Tag
NS_P3P_TAG_CLASS_BEGIN( EmbeddedInclude )

public:
  nsString                   mEmbeddedInclude;

NS_P3P_TAG_CLASS_END( EmbeddedInclude )


// P3P EMBEDDED-EXCLUDE Tag
NS_P3P_TAG_CLASS_BEGIN( EmbeddedExclude )

public:
  nsString                   mEmbeddedExclude;

NS_P3P_TAG_CLASS_END( EmbeddedExclude )


// P3P METHOD Tag
NS_P3P_TAG_CLASS_BEGIN( Method )

public:
  nsString                   mMethod;

NS_P3P_TAG_CLASS_END( Method )


// ****************************************************************************
// P3P Policy Tags
// ****************************************************************************

// P3P POLICY Tag
NS_P3P_TAG_CLASS_BEGIN( Policy )

public:
  nsString                   mDescURISpec;
  nsString                   mOptURISpec;
  nsString                   mName;
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsIP3PTag>        mExpiryTag;
  nsCOMPtr<nsIP3PTag>        mDataSchemaTag;
  nsCOMPtr<nsIP3PTag>        mEntityTag;
  nsCOMPtr<nsIP3PTag>        mAccessTag;
  nsCOMPtr<nsIP3PTag>        mDisputesGroupTag;
  nsCOMPtr<nsISupportsArray> mStatementTags;

NS_P3P_TAG_CLASS_END( Policy )


// P3P ENTITY Tag
NS_P3P_TAG_CLASS_BEGIN( Entity )

public:
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsIP3PTag>        mDataGroupTag;

NS_P3P_TAG_CLASS_END( Entity )


// P3P ACCESS Tag
NS_P3P_TAG_CLASS_BEGIN( Access )

public:
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsIP3PTag>        mAccessValueTag;

NS_P3P_TAG_CLASS_END( Access )


// P3P DISPUTES-GROUP Tag
NS_P3P_TAG_CLASS_BEGIN( DisputesGroup )

public:
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsISupportsArray> mDisputesTags;

NS_P3P_TAG_CLASS_END( DisputesGroup )


// P3P DISPUTES Tag
NS_P3P_TAG_CLASS_BEGIN( Disputes )

public:
  nsString                   mResolutionType;
  nsString                   mService;
  nsString                   mVerification;
  nsString                   mShortDescription;
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsIP3PTag>        mImgTag;
  nsCOMPtr<nsIP3PTag>        mRemediesTag;
  nsCOMPtr<nsIP3PTag>        mLongDescriptionTag;

NS_P3P_TAG_CLASS_END( Disputes )


// P3P IMG Tag
NS_P3P_TAG_CLASS_BEGIN( Img )

public:
  nsString                   mSrc;
  nsString                   mWidth;
  nsString                   mHeight;
  nsString                   mAlt;

NS_P3P_TAG_CLASS_END( Img )


// P3P REMEDIES Tag
NS_P3P_TAG_CLASS_BEGIN( Remedies )

public:
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsISupportsArray> mRemedyValueTags;

NS_P3P_TAG_CLASS_END( Remedies )


// P3P STATEMENT Tag
NS_P3P_TAG_CLASS_BEGIN( Statement )

public:
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsIP3PTag>        mConsequenceTag;
  nsCOMPtr<nsIP3PTag>        mPurposeTag;
  nsCOMPtr<nsIP3PTag>        mRecipientTag;
  nsCOMPtr<nsIP3PTag>        mRetentionTag;
  nsCOMPtr<nsISupportsArray> mDataGroupTags;

NS_P3P_TAG_CLASS_END( Statement )


// P3P CONSEQUENCES Tag
NS_P3P_TAG_CLASS_BEGIN( Consequence )

public:
  nsString                   mConsequence;

NS_P3P_TAG_CLASS_END( Consequence )


// P3P PURPOSE Tag
NS_P3P_TAG_CLASS_BEGIN( Purpose )

public:
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsISupportsArray> mPurposeValueTags;

NS_P3P_TAG_CLASS_END( Purpose )


// P3P RECIPIENT Tag
NS_P3P_TAG_CLASS_BEGIN( Recipient )

public:
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsISupportsArray> mRecipientValueTags;

NS_P3P_TAG_CLASS_END( Recipient )


// P3P RETENTION Tag
NS_P3P_TAG_CLASS_BEGIN( Retention )

public:
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsIP3PTag>        mRetentionValueTag;

NS_P3P_TAG_CLASS_END( Retention )


// P3P DATA-GROUP Tag
NS_P3P_TAG_CLASS_BEGIN( DataGroup )

public:
  PRBool                     mBasePresent;
  nsString                   mBase;
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsISupportsArray> mDataTags;

NS_P3P_TAG_CLASS_END( DataGroup )


// P3P DATA Tag
NS_P3P_TAG_CLASS_BEGIN( Data )

public:
  nsString                   mRef;
  nsString                   mRefDataSchemaURISpec;
  nsString                   mRefDataStructName;
  nsString                   mOptional;
  nsString                   mData;
  nsCOMPtr<nsIP3PTag>        mCategoriesTag;

NS_P3P_TAG_CLASS_END( Data )


// ****************************************************************************
// P3P DataSchema Tags
// ****************************************************************************

// P3P DATASCHEMA Tag
NS_P3P_TAG_CLASS_BEGIN( DataSchema )

public:
  nsCOMPtr<nsISupportsArray> mExtensionTags;
  nsCOMPtr<nsISupportsArray> mDataStructTags;
  nsCOMPtr<nsISupportsArray> mDataDefTags;

NS_P3P_TAG_CLASS_END( DataSchema )


// P3P DATA-STRUCT Tag
NS_P3P_TAG_CLASS_BEGIN( DataStruct )

public:
  nsString                   mName;
  nsString                   mStructRef;
  nsString                   mStructRefDataSchemaURISpec;
  nsString                   mStructRefDataStructName;
  nsString                   mShortDescription;
  nsCOMPtr<nsIP3PTag>        mCategoriesTag;
  nsCOMPtr<nsIP3PTag>        mLongDescriptionTag;

NS_P3P_TAG_CLASS_END( DataStruct )


// P3P DATA-DEF Tag
NS_P3P_TAG_CLASS_BEGIN( DataDef )

public:
  nsString                   mName;
  nsString                   mStructRef;
  nsString                   mStructRefDataSchemaURISpec;
  nsString                   mStructRefDataStructName;
  nsString                   mShortDescription;
  nsCOMPtr<nsIP3PTag>        mCategoriesTag;
  nsCOMPtr<nsIP3PTag>        mLongDescriptionTag;

NS_P3P_TAG_CLASS_END( DataDef )


// ****************************************************************************
// P3P Multi-Use Tags
// ****************************************************************************

// P3P EXTENSION Tag
NS_P3P_TAG_CLASS_BEGIN( Extension )

public:
  nsString                   mOptional;
  nsString                   mExtension;

NS_P3P_TAG_CLASS_END( Extension )


// P3P EXPIRY Tag
NS_P3P_TAG_CLASS_BEGIN( Expiry )

public:
  nsString                   mDate;
  nsString                   mMaxAge;

NS_P3P_TAG_CLASS_END( Expiry )


// P3P LONG-DESCRIPTION Tag
NS_P3P_TAG_CLASS_BEGIN( LongDescription )

public:
  nsString                   mLongDescription;

NS_P3P_TAG_CLASS_END( LongDescription )


// P3P CATEGORIES Tag
NS_P3P_TAG_CLASS_BEGIN( Categories )

public:
  nsCOMPtr<nsISupportsArray> mCategoryValueTags;

NS_P3P_TAG_CLASS_END( Categories )


// ****************************************************************************
// P3P AccessValue Tag
// ****************************************************************************

NS_P3P_TAG_CLASS_BEGIN( AccessValue )

NS_P3P_TAG_CLASS_END( AccessValue )


// ****************************************************************************
// P3P RemedyValue Tags
// ****************************************************************************

NS_P3P_TAG_CLASS_BEGIN( RemedyValue )

NS_P3P_TAG_CLASS_END( RemedyValue )


// ****************************************************************************
// P3P PurposeValue Tags
// ****************************************************************************

NS_P3P_TAG_CLASS_BEGIN( PurposeValue )

public:
  nsString                   mRequired;
  nsString                   mPurposeValue;

NS_P3P_TAG_CLASS_END( PurposeValue )


// ****************************************************************************
// P3P RecipientValue Tags
// ****************************************************************************

NS_P3P_TAG_CLASS_BEGIN( RecipientValue )

public:
  nsString                   mRequired;
  nsCOMPtr<nsISupportsArray> mRecipientDescriptionTags;

NS_P3P_TAG_CLASS_END( RecipientValue )


// ****************************************************************************
// P3P RecipientDescription Tags
// ****************************************************************************

NS_P3P_TAG_CLASS_BEGIN( RecipientDescription )

public:
  nsString                   mRecipientDescription;

NS_P3P_TAG_CLASS_END( RecipientDescription )


// ****************************************************************************
// P3P RetentionValue Tags
// ****************************************************************************

NS_P3P_TAG_CLASS_BEGIN( RetentionValue )

NS_P3P_TAG_CLASS_END( RetentionValue )


// ****************************************************************************
// P3P CategoryValue Tags
// ****************************************************************************

NS_P3P_TAG_CLASS_BEGIN( CategoryValue )

public:
  nsString                   mCategoryValue;

NS_P3P_TAG_CLASS_END( CategoryValue )

#endif                                           /* nsP3PTags_h___            */
