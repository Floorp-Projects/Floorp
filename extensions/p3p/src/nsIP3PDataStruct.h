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

#ifndef nsIP3PDataStruct_h__
#define nsIP3PDataStruct_h__

#include "nsIP3PTag.h"

#include <nsCOMPtr.h>
#include <nsISupports.h>

#include <nsString.h>

#include <nsISupportsArray.h>
#include <nsISimpleEnumerator.h>


#define NS_IP3PDATASTRUCT_IID_STR "31430e5f-d43d-11d3-9781-002035aee991"
#define NS_IP3PDATASTRUCT_IID     {0x31430e5f, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

// Class: nsIP3PDataStruct
//
//        This class represents a DataSchema <DATA-DEF> or <DATA-STRUCT> definition.
//          All of the associated attributes (such as "structref", etc.) and 
//          representations of valid child tags (such as <CATEGORIES>, etc.) are saved.
//
class nsIP3PDataStruct : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR( NS_IP3PDATASTRUCT_IID )

// Function:  Returns an indicator as to whether the DataStruct object was explicitly created.
//
//            A DataStruct is not explicitly created when a parent has to be implicitly created for the DataStruct tree.
//              ie. dynamic.http.useragent requires dynamic and http to be implicitly created if they do not exist,
//                                         but useragent is explicitly created.
//
// Parms:     None
  NS_IMETHOD_( PRBool ) IsExplicitlyCreated( ) = 0;

// Function:  Gets the name of the DataStruct object.
//
// Parms:     1. Out    The name of the DataStruct object
  NS_IMETHOD_( void )   GetName( nsString&  aName ) = 0;

// Function:  Gets the full name of the DataStruct object (this includes the parent DataStruct names).
//
// Parms:     1. Out    The full name of the DataStruct object
  NS_IMETHOD_( void )   GetFullName( nsString&  aName ) = 0;

// Function:  Gets the "structref" attribute of the DataStruct object.
//
// Parms:     1. Out    The "structref" attribute of the DataStruct object
  NS_IMETHOD_( void )   GetStructRef( nsString&  aStructRef ) = 0;

// Function:  Gets the DataSchema URI spec portion of the "structref" attribute of the DataStruct object.
//
// Parms:     1. Out    The DataSchema URI spec of the "structref" attribute of the DataStruct object
  NS_IMETHOD_( void )   GetDataSchemaURISpec( nsString&  aDataSchemaURISpec ) = 0;

// Function:  Gets the DataSchema structure of the "structref" attribute of the DataStruct object.
//
// Parms:     1. Out    The DataSchema structure of the "structref" attribute of the DataStruct object
  NS_IMETHOD_( void )   GetDataStructName( nsString&  aDataStructName ) = 0;

// Function:  Gets the "short-description" attribute of the DataStruct object.
//
// Parms:     1. Out    The "short-description" attribute of the DataStruct object
  NS_IMETHOD_( void )   GetShortDescription( nsString&  aShortDescription ) = 0;

// Function:  Gets the category value Tag objects of the DataStruct object.
//
// Parms:     1. Out    The category value Tag objects of the DataStruct object
  NS_IMETHOD            GetCategoryValues( nsISupportsArray **aCategoryValueTags ) = 0;

// Function:  Function pair to add category value Tag objects to the DataStruct object.
//
// Parms:     1. In     The category value Tag objects to add to the DataStruct object
  NS_IMETHOD            AddCategoryValues( nsISupportsArray  *aCategoryValueTags ) = 0;
  NS_IMETHOD            AddCategoryValues( nsIP3PTag  *aCategoryValueTag ) = 0;

// Function:  Gets the <LONG-DESCRIPTION> Tag object of the DataStruct object.
//
// Parms:     1. Out    The <LONG-DESCRIPTION> Tag object of the DataStruct object
  NS_IMETHOD            GetLongDescription( nsIP3PTag **aLongDescriptionTag ) = 0;

// Function:  Gets the parent of the DataStruct object.
//
// Parms:     1. Out    The parent DataStruct object
  NS_IMETHOD            GetParent( nsIP3PDataStruct **aParent ) = 0;

// Function:  Sets the parent of the DataStruct object.
//
// Parms:     1. In     The parent DataStruct object
  NS_IMETHOD_( void )   SetParent( nsIP3PDataStruct  *aParent ) = 0;

// Function:  Gets the DataStruct object children.
//
// Parms:     1. Out    The DataStruct object children
  NS_IMETHOD            GetChildren( nsISupportsArray **aChildren ) = 0;

// Function:  Adds a child to the DataStruct object.
//
// Parms:     1. In     The child DataStruct object to add
  NS_IMETHOD            AddChild( nsIP3PDataStruct  *aChild ) = 0;

// Function:  Removes a child from the DataStruct object.
//
// Parms:     1. In     The child DataStruct object to remove
  NS_IMETHOD            RemoveChild( nsIP3PDataStruct  *aChild ) = 0;

// Function:  Removes all children from the DataStruct object.
//
// Parms:     None
  NS_IMETHOD_( void )   RemoveChildren( ) = 0;

// Function:  Creates a copy of the DataStruct object (at the node level only).
//
// Parms:     1. In     The DataStruct object to make the parent of the cloned DataStruct object
//            2. Out    The cloned DataStruct object
  NS_IMETHOD            CloneNode( nsIP3PDataStruct  *aParent,
                                   nsIP3PDataStruct **aNewDataStruct ) = 0;

// Function:  Creates a copy of the DataStruct object and it's children.
//
// Parms:     1. In     The DataStruct object to make the parent of the cloned DataStruct object
//            2. Out    The cloned DataStruct object
  NS_IMETHOD            CloneTree( nsIP3PDataStruct  *aParent,
                                   nsIP3PDataStruct **aNewDataStruct ) = 0;

// Function:  Creates a copy of the children of the DataStruct object and their children.
//
// Parms:     1. In     The DataStruct object to make the parent of the cloned children
  NS_IMETHOD            CloneChildren( nsIP3PDataStruct  *aParent ) = 0;

// Function:  Removes all connections (parent/children) between DataStruct objects.
//
// Parms:     None
  NS_IMETHOD_( void )   DestroyTree( ) = 0;

// Function:  Sets the indicator that says this DataStruct has resolved it's reference.
//
// Parms:     None
  NS_IMETHOD_( void )   SetReferenceResolved( ) = 0;

// Function:  Returns the indicator that says whether or not the reference for this DataStruct
//            has been resolved.
//
// Parms:     None
  NS_IMETHOD_( PRBool ) IsReferenceResolved( ) = 0;
};

#define NS_DECL_NSIP3PDATASTRUCT                                                    \
  NS_IMETHOD_( PRBool ) IsExplicitlyCreated( );                                     \
  NS_IMETHOD_( void )   GetName( nsString&  aName );                                \
  NS_IMETHOD_( void )   GetFullName( nsString&  aName );                            \
  NS_IMETHOD_( void )   GetStructRef( nsString&  aStructRef );                      \
  NS_IMETHOD_( void )   GetDataSchemaURISpec( nsString&  aDataSchemURISpec );       \
  NS_IMETHOD_( void )   GetDataStructName( nsString&  aDataStructName );            \
  NS_IMETHOD_( void )   GetShortDescription( nsString&  aShortDescription );        \
  NS_IMETHOD            GetCategoryValues( nsISupportsArray **aCategoryValueTags ); \
  NS_IMETHOD            AddCategoryValues( nsISupportsArray  *aCategoryValueTags ); \
  NS_IMETHOD            AddCategoryValues( nsIP3PTag  *aCategoryValueTag );         \
  NS_IMETHOD            GetLongDescription( nsIP3PTag **aLongDescriptionTag );      \
  NS_IMETHOD            AddChild( nsIP3PDataStruct  *aChild );                      \
  NS_IMETHOD            RemoveChild( nsIP3PDataStruct  *aChild );                   \
  NS_IMETHOD_( void )   RemoveChildren( );                                          \
  NS_IMETHOD            GetChildren( nsISupportsArray **aChildren );                \
  NS_IMETHOD            GetParent( nsIP3PDataStruct **aParent );                    \
  NS_IMETHOD_( void )   SetParent( nsIP3PDataStruct  *aParent );                    \
  NS_IMETHOD            CloneNode( nsIP3PDataStruct  *aParent,                      \
                                   nsIP3PDataStruct **aNewDataStruct );             \
  NS_IMETHOD            CloneTree( nsIP3PDataStruct  *aParent,                      \
                                   nsIP3PDataStruct **aNewDataStruct );             \
  NS_IMETHOD            CloneChildren( nsIP3PDataStruct  *aParent );                \
  NS_IMETHOD_( void )   DestroyTree( );                                             \
  NS_IMETHOD_( void )   SetReferenceResolved( );                                    \
  NS_IMETHOD_( PRBool ) IsReferenceResolved( );

#endif                                           /* nsIP3PDataStruct_h___     */
