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

#ifndef nsIP3PPolicy_h__
#define nsIP3PPolicy_h__

#include "nsP3PXMLProcessor.h"

#include <nsIRDFDataSource.h>


#define NS_IP3PPOLICY_IID_STR "31430e5b-d43d-11d3-9781-002035aee991"
#define NS_IP3PPOLICY_IID     {0x31430e5b, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

// Class: nsIP3PPolicy
//
//        This class builds upon the nsP3PXMLProcessor class and defines interfaces
//        required to process and support a Policy.
//
class nsIP3PPolicy : public nsP3PXMLProcessor {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IP3PPOLICY_IID)

// Function:  Compares the Policy representation to the user's preferences.
//
// Parms:     None
  NS_IMETHOD            ComparePolicyToPrefs( PRBool&  aPolicyExpired ) = 0;

// Function:  Gets the RDF description of the Policy information.
//
// Parms:     1. Out    The "about" attribute to be used to access the Policy information
//            2. Out    The "title" attribute to be used to describe the Policy information
//            3. Out    The RDFDataSource describing the Policy information
  NS_IMETHOD            GetPolicyDescription( nsString&          aAbout,
                                              nsString&          aTitle,
                                              nsIRDFDataSource **aDataSource ) = 0;
};

#define NS_DECL_NSIP3PPOLICY                                                    \
  NS_IMETHOD            ComparePolicyToPrefs( PRBool&  aPolicyExpired );        \
  NS_IMETHOD            GetPolicyDescription( nsString&          aAbout,        \
                                              nsString&          aTitle,        \
                                              nsIRDFDataSource **aDataSource );

#endif                                           /* nsIP3Policy_h__           */
