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

#ifndef nsIP3PURIInformation_h__
#define nsIP3PURIInformation_h__

#include <nsCOMPtr.h>
#include <nsISupports.h>

#include <nsIURI.h>

#include <nsString.h>
#include <nsVoidArray.h>


#define NS_IP3PURIINFORMATION_IID_STR "31430e58-d43d-11d3-9781-002035aee991"
#define NS_IP3PURIINFORMATION_IID     {0x31430e58, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

// Class: nsIP3PURIInformation (nsIP3PURIInformation)
//
//        This class represents the information associated with a request for a URI
//        (such as a PolicyRefFile or Set-Cookie header, etc.).
//
class nsIP3PURIInformation : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IP3PURIINFORMATION_IID)

// Function:  Gets the "Referer" request header value associated with this URI.
//
// Parms:     1. Out    The "Referer" request header value
  NS_IMETHOD_( void )   GetRefererHeader( nsString&  aRefererHeader ) = 0;

// Function:  Sets the "Referer" request header value associated with this URI.
//
// Parms:     1. In     The "Referer" request header value
  NS_IMETHOD            SetRefererHeader( nsString&  aRefererHeader ) = 0;

// Function:  Returns the PolicyRefFile URI specs associated with this URI.
//
// Parms:     1. Out    The PolicyRefFile URI specs
//
  NS_IMETHOD_( void )   GetPolicyRefFileURISpecs( nsStringArray  *aPolicyRefFileURISpecs ) = 0;

// Function:  Gets the point at which this PolicyRefFile URI spec was specified
//            (ie. HTTP header or HTML <LINK> tag).
//
// Parms:     1. In     The PolicyRefFile URI spec
//            2. Out    The place where the reference was specified
//
  NS_IMETHOD_( void )   GetPolicyRefFileReferencePoint( nsString&  aPolicyRefFileURISpec,
                                                        PRInt32&   aReferencePoint ) = 0;

// Function:  Sets a PolicyRefFile URI spec associated with this URI.
//
// Parms:     1. In     The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            2. In     The PolicyRefFile URI spec
//
  NS_IMETHOD            SetPolicyRefFileURISpec( PRInt32    aReferencePoint,
                                                 nsString&  aPolicyRefFileURISpec ) = 0;

// Function:  Gets the "Set-Cookie" response header value associated with this URI.
//
// Parms:     1. Out    The "Set-Cookie" response header value
  NS_IMETHOD_( void )   GetCookieHeader( nsString&  aCookieHeader ) = 0;

// Function:  Sets the "Set-Cookie" response header value associated with this URI.
//
// Parms:     1. In     The "Set-Cookie" response header value
  NS_IMETHOD            SetCookieHeader( nsString&  aCookieHeader ) = 0;
};

#define NS_DECL_NSIP3PURIINFORMATION                                                        \
  NS_IMETHOD_( void )   GetRefererHeader( nsString&  aRefererHeader );                      \
  NS_IMETHOD            SetRefererHeader( nsString&  aRefererHeader );                      \
  NS_IMETHOD_( void )   GetPolicyRefFileURISpecs( nsStringArray  *aPolicyRefFileURISpecs ); \
  NS_IMETHOD_( void )   GetPolicyRefFileReferencePoint( nsString&  aPolicyRefFileURISpec,   \
                                                        PRInt32&   aReferencePoint );       \
  NS_IMETHOD            SetPolicyRefFileURISpec( PRInt32    aReferencePoint,                \
                                                 nsString&  aPolicyRefFileURISpec );        \
  NS_IMETHOD_( void )   GetCookieHeader( nsString&  aCookieHeader );                        \
  NS_IMETHOD            SetCookieHeader( nsString&  aCookieHeader );

#endif                                           /* nsIP3PURIInformation_h__  */
