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

#ifndef nsIP3PDataSchema_h__
#define nsIP3PDataSchema_h__

#include "nsP3PXMLProcessor.h"
#include "nsIP3PTag.h"
#include "nsIP3PXMLListener.h"
#include "nsIP3PDataStruct.h"


#define NS_IP3PDATASCHEMA_IID_STR "31430e5c-d43d-11d3-9781-002035aee991"
#define NS_IP3PDATASCHEMA_IID     {0x31430e5c, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

// Class: nsIP3PDataSchema
//
//        This class builds upon the nsP3PXMLProcessor class and defines interfaces
//        required to process and support a DataSchema.
//
class nsIP3PDataSchema : public nsP3PXMLProcessor {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IP3PDATASCHEMA_IID)

// Function:  Returns the DataStruct object for a given DataStruct
//
// Parms:     1. In     The DataStruct name
//            2. Out    The DataStruct object
  NS_IMETHOD            FindDataStruct( nsString&          aDataStructName,
                                        nsIP3PDataStruct **aDataStruct ) = 0;
};

#define NS_DECL_NSIP3PDATASCHEMA                                            \
  NS_IMETHOD            FindDataStruct( nsString&          aDataStructName, \
                                        nsIP3PDataStruct **aDataStruct );

#endif                                           /* nsIP3DataSchema_h__       */
