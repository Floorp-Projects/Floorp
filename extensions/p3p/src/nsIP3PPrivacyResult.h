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

#ifndef nsIP3PPrivacyResult_h__
#define nsIP3PPrivacyResult_h__

#include "nsIP3PPolicy.h"

#include <nsCOMPtr.h>
#include <nsISupports.h>

#include <nsIRDFDataSource.h>

#include <nsISupportsArray.h>

#include <nsString.h>


#define NS_IP3PPRIVACYRESULT_IID_STR "31430e5d-d43d-11d3-9781-002035aee991"
#define NS_IP3PPRIVACYRESULT_IID     {0x31430e5d, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

// Class: nsIP3PPrivacyResult
//
//        This class represents the Privacy results and information associated with
//        a URI and DocShellTreeItem.
//
class nsIP3PPrivacyResult : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IP3PPRIVACYRESULT_IID)

// Function:  Sets the URI spec associated with this PrivacyResult object
//
// Parms:     1. In     The URI spec
  NS_IMETHOD_( void )   SetURISpec( nsString&  aURISpec ) = 0;

// Function:  Sets the Privacy information.
//
// Parms:     1. In     The result of the comparison between the Policy and the user's preferences
//            2. In     The Policy object used
  NS_IMETHOD            SetPrivacyResult( nsresult       aPrivacyResult,
                                          nsIP3PPolicy  *aPolicy  ) = 0;

// Function:  Sets the overall Privacy result based upon the supplied Privacy result.
//
// Parms:     1. In     The Privacy result
  NS_IMETHOD            SetOverallPrivacyResult( nsresult   aPrivacyResult,
                                                 PRBool     aChildCalling ) = 0;

// Function:  Gets the RDF description of the Privacy information.
//
// Parms:     1. Out    The RDFDataSource describing the Privacy information
  NS_IMETHOD            GetPrivacyInfo( nsIRDFDataSource **aDataSource ) = 0;

// Function:  Creates the RDF description of the Privacy information.
//
// Parms:     1. In     The "about" attribute to be used to access the Privacy information
//            2. In     The "title" attribute to be used to describe the Privacy information
//            3. Out    The RDFDataSource describing the Privacy information
  NS_IMETHOD            CreateRDFDescription( nsString&          aAbout,
                                              nsString&          aTitle,
                                              nsIRDFDataSource **aDataSource ) = 0;

// Function:  Adds the PrivacyResult object as a child.
//
// Parms:     1. In     The child PrivacyResult object to add
  NS_IMETHOD            AddChild( nsIP3PPrivacyResult  *aChild ) = 0;

// Function:  Removes the PrivacyResult object as a child.
//
// Parms:     1. In     The child PrivacyResult object to remove
  NS_IMETHOD            RemoveChild( nsIP3PPrivacyResult  *aChild ) = 0;

// Function:  Removes all PrivacyResult object children
//
// Parms:     1. None
  NS_IMETHOD_( void )   RemoveChildren( ) = 0;

// Function:  Gets the PrivacyResult object children.
//
// Parms:     1. Out    The PrivacyResult object children
  NS_IMETHOD            GetChildren( nsISupportsArray **aChildren ) = 0;

// Function:  Sets the parent of the PrivacyResult object.
//
// Parms:     1. In     The parent PrivacyResult object
  NS_IMETHOD_( void )   SetParent( nsIP3PPrivacyResult  *aParent ) = 0;

// Function:  Gets the parent PrivacyResult object.
//
// Parms:     1. Out    The parent PrivacyResult object
  NS_IMETHOD            GetParent( nsIP3PPrivacyResult **aParent ) = 0;

// Function:  Sets the indicator that this PrivacyResult object was indirectly created.
//
// Parms:     None
  NS_IMETHOD_( void )   IndirectlyCreated( ) = 0;
};

#define NS_DECL_NSIP3PPRIVACYRESULT                                             \
  NS_IMETHOD_( void )   SetURISpec( nsString&  aURISpec );                      \
  NS_IMETHOD            SetPrivacyResult( nsresult       aPrivacyResult,        \
                                          nsIP3PPolicy  *aPolicy  );            \
  NS_IMETHOD            SetOverallPrivacyResult( nsresult   aPrivacyResult,     \
                                                 PRBool     aChildCalling  );   \
  NS_IMETHOD            GetPrivacyInfo( nsIRDFDataSource **aDataSource );       \
  NS_IMETHOD            CreateRDFDescription( nsString&          aAbout,        \
                                              nsString&          aTitle,        \
                                              nsIRDFDataSource **aDataSource ); \
  NS_IMETHOD            AddChild( nsIP3PPrivacyResult  *aChild );               \
  NS_IMETHOD            RemoveChild( nsIP3PPrivacyResult  *aChild );            \
  NS_IMETHOD_( void )   RemoveChildren( );                                      \
  NS_IMETHOD            GetChildren( nsISupportsArray **aChildren );            \
  NS_IMETHOD_( void )   SetParent( nsIP3PPrivacyResult  *aParent );             \
  NS_IMETHOD            GetParent( nsIP3PPrivacyResult **aParent );             \
  NS_IMETHOD_( void )   IndirectlyCreated( );

#endif                                           /* nsIP3PrivacyResult_h__    */
