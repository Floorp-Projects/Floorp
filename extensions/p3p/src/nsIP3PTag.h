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

#ifndef nsIP3PTag_h__
#define nsIP3PTag_h__

#include <nsCOMPtr.h>
#include <nsISupports.h>

#include <nsIDOMNode.h>

#include <nsString.h>

#include <nsISupportsArray.h>
#include <nsISimpleEnumerator.h>


#define NS_IP3PTAG_IID_STR "31430e5e-d43d-11d3-9781-002035aee991"
#define NS_IP3PTAG_IID     {0x31430e5e, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

// Class: nsIP3PTag
//
//        This class represents a P3P related XML tag (such as <POLICY>, etc.).
//          All of the associated attributes for each tag (such as "discuri", etc.) and 
//          representations of valid child tags (such as <STATEMENT>, etc.) are saved.
//
class nsIP3PTag : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR( NS_IP3PTAG_IID )

// Function:  Validates and saves the information related to the P3P XML tag.
//
// Parms:     None
  NS_IMETHOD            ProcessTag( ) = 0;

// Function:  Gets the DOMNode associated with this Tag object.
//
// Parms:     1. Out    The DOMNode associated with this Tag object
  NS_IMETHOD            GetDOMNode( nsIDOMNode **aDOMNode ) = 0;

// Function:  Gets the name of this Tag object (the name of the P3P XML tag).
//
// Parms:     1. Out    The name of this Tag object
  NS_IMETHOD_( void )   GetName( nsString&  aName ) = 0;

// Function:  Gets the namespace of this Tag object (the namespace of the P3P XML tag).
//
// Parms:     1. Out    The namespace of this Tag object
  NS_IMETHOD_( void )   GetNameSpace( nsString&  aNameSpace ) = 0;

// Function:  Gets the text of this Tag object (the text of the P3P XML tag).
//
// Parms:     1. Out    The text of this Tag object
  NS_IMETHOD_( void )   GetText( nsString&  aText ) = 0;

// Function:  Functions to get the value of an attribute of this Tag object (the
//            attribute of the P3P XML tag) with the optional indicator as to whether
//            the attribute was present (vs. just empty).
//
// Parms:     1. In     The name of the attribute
//            2. In     The namespace of the attribute
//            3. Out    The value of the attribute
//            4. Out    An indicator as to whether or not the attribute was present
  NS_IMETHOD_( void )   GetAttribute( const char  *aAttributeName,
                                      const char  *aAttributeNameSpace,
                                      nsString&    aAttributeValue,
                                      PRBool      *aAttributePresent = 0 ) = 0;
  NS_IMETHOD_( void )   GetAttribute( nsString&  aAttributeName,
                                      nsString&  aAttributeNameSpace,
                                      nsString&  aAttributeValue,
                                      PRBool    *aAttributePresent = 0 ) = 0;

// Function:  Gets the parent Tag object.
//
// Parms:     1. Out    The parent of this Tag object
  NS_IMETHOD            GetParent( nsIP3PTag **aTagParent ) = 0;

// Function:  Sets the parent Tag object.
//
// Parms:     1. In     The parent of this Tag object
  NS_IMETHOD_( void )   SetParent( nsIP3PTag  *aTagParent ) = 0;

// Function:  Adds a child to the Tag object.
//
// Parms:     1. In     The child Tag object to add
  NS_IMETHOD            AddChild( nsIP3PTag  *aChild ) = 0;

// Function:  Removes all the children of the Tag object.
//
// Parms:     None
  NS_IMETHOD_( void )   RemoveChildren( ) = 0;

// Function:  Gets the Tag object children.
//
// Parms:     1. Out    The Tag object children
  NS_IMETHOD            GetChildren( nsISupportsArray **aChildren ) = 0;

// Function:  Creates a copy of the Tag object (at the node level only).
//
// Parms:     1. In     The Tag object to make the parent of the cloned Tag object
//            2. Out    The cloned Tag object
  NS_IMETHOD            CloneNode( nsIP3PTag  *aParent,
                                   nsIP3PTag **aNewTag ) = 0;

// Function:  Creates a copy of the Tag object and it's children.
//
// Parms:     1. In     The Tag object to make the parent of the cloned Tag object
//            2. Out    The cloned Tag object
  NS_IMETHOD            CloneTree( nsIP3PTag  *aParent,
                                   nsIP3PTag **aNewTag ) = 0;

// Function:  Removes all connections (parent/children) between Tag objects.
//
// Parms:     None
  NS_IMETHOD_( void )   DestroyTree( ) = 0;
};

#define NS_DECL_NSIP3PTAG                                                   \
  NS_IMETHOD            ProcessTag( );                                      \
  NS_IMETHOD            GetDOMNode( nsIDOMNode **aDOMNode );                \
  NS_IMETHOD_( void )   GetName( nsString&  aName );                        \
  NS_IMETHOD_( void )   GetNameSpace( nsString&  aNameSpace );              \
  NS_IMETHOD_( void )   GetText( nsString&  aText );                        \
  NS_IMETHOD_( void )   GetAttribute( const char  *aAttributeName,          \
                                      const char  *aAttributeNameSpace,     \
                                      nsString&    aAttributeValue,         \
                                      PRBool      *aAttributePresent = 0 ); \
  NS_IMETHOD_( void )   GetAttribute( nsString&  aAttributeName,            \
                                      nsString&  aAttributeNameSpace,       \
                                      nsString&  aAttributeValue,           \
                                      PRBool    *aAttributePresent = 0 );   \
  NS_IMETHOD            GetParent( nsIP3PTag **aTagParent );                \
  NS_IMETHOD_( void )   SetParent( nsIP3PTag  *aTagParent );                \
  NS_IMETHOD            AddChild( nsIP3PTag  *aChild );                     \
  NS_IMETHOD_( void )   RemoveChildren( );                                  \
  NS_IMETHOD            GetChildren( nsISupportsArray **aChildren );        \
  NS_IMETHOD            CloneNode( nsIP3PTag  *aParent,                     \
                                   nsIP3PTag **aNewTag );                   \
  NS_IMETHOD            CloneTree( nsIP3PTag  *aParent,                     \
                                   nsIP3PTag **aNewTag );                   \
  NS_IMETHOD_( void )   DestroyTree( );

#endif                                           /* nsIP3PTag_h___            */
