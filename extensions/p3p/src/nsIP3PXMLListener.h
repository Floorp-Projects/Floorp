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

#ifndef nsIP3PXMLListener_h__
#define nsIP3PXMLListener_h__

#include <nsCOMPtr.h>
#include <nsISupports.h>

#include <nsString.h>


#define NS_IP3PXMLLISTENER_IID_STR "31430e61-d43d-11d3-9781-002035aee991"
#define NS_IP3PXMLLISTENER_IID     {0x31430e61, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

// Class: nsIP3PXMLListener
//
//        This class represents the callback interface for when a URI processed by an XMLProcessor
//        (such as nsP3PPolicy, etc.) has been completed.
//
class nsIP3PXMLListener : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR( NS_IP3PXMLLISTENER_IID )

// Function:  Notification of the completion of a URI.
//
// Parms:     1. In     The URI spec that has completed
//            2. In     The data supplied on the intial processing request
  NS_IMETHOD            DocumentComplete( nsString&     aProcessedURISpec,
                                          nsISupports  *aReaderData ) = 0;
};

#define NS_DECL_NSIP3PXMLLISTENER                                          \
  NS_IMETHOD            DocumentComplete( nsString&     aProcessedURISpec, \
                                          nsISupports  *aReaderData );

#endif                                           /* nsIP3PXMLListener_h___    */
