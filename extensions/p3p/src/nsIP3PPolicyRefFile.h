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

#ifndef nsIP3PPolicyRefFile_h__
#define nsIP3PPolicyRefFile_h__

#include "nsP3PXMLProcessor.h"
#include "nsIP3PPolicy.h"


#define NS_IP3PPOLICYREFFILE_IID_STR "31430e59-d43d-11d3-9781-002035aee991"
#define NS_IP3PPOLICYREFFILE_IID     {0x31430e59, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

// Class: nsIP3PPolicyRefFile
//
//        This class builds upon the nsP3PXMLProcessor class and defines interfaces
//        required to process and support a PolicyRefFile.
//
class nsIP3PPolicyRefFile : public nsP3PXMLProcessor {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IP3PPOLICYREFFILE_IID)

// Function:  Returns the Policy URI spec associated with the method/path combination.
//
// Parms:     1. In     An indicator as to whether this check is for an embedded object
//            2. In     The method used
//            3. In     The path
//            4. Out    The reading PolicyRefFile URI spec
//            5. Out    The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            6. Out    The expired PolicyRefFile URI spec
//            7. Out    The point where the reference was made (ie. HTTP header, HTML <LINK> tag)
//            8. Out    The Policy object
  NS_IMETHOD            GetPolicy( PRBool         aEmbedCheck,
                                   nsString&      aURIMethod,
                                   nsString&      aURIPath,
                                   nsString&      aReadingPolicyRefFileURISpec,
                                   PRInt32&       aReadingReferencePoint,
                                   nsString&      aExpiredPolicyRefFileURISpec,
                                   PRInt32&       aExpiredReferencePoint,
                                   nsIP3PPolicy **aPolicy ) = 0;
};

#define NS_DECL_NSIP3PPOLICYREFFILE                                             \
  NS_IMETHOD            GetPolicy( PRBool         aEmbedCheck,                  \
                                   nsString&      aURIMethod,                   \
                                   nsString&      aURIPath,                     \
                                   nsString&      aReadingPolicyRefFileURISpec, \
                                   PRInt32&       aReadingReferencePoint,       \
                                   nsString&      aExpiredPolicyRefFileURISpec, \
                                   PRInt32&       aExpiredReferencePoint,       \
                                   nsIP3PPolicy **aPolicy );

#endif                                           /* nsIP3PPolicyRefFile_h__   */
