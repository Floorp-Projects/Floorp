/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *
 * This Original Code has been modified by Roger B. Sidje.
 * Modifications made by Roger B. Sidje described herein are
 * Copyright (C) 2000 The University Of Queensland.
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date            Modified by     Description of modification
 * 08/March/2000   RBS.            Support for Mathematical fonts.
 */

#ifndef nsUCvMathCID_h___
#define nsUCvMathCID_h___

#include "nsISupports.h"

// Class ID for our UnicodeToTeXCMSYttf (TeX Symbol -TTF) charset converter
// {e332db00-e076-11d3-b32f-004005a7a7e4}
NS_DECLARE_ID(kUnicodeToTeXCMSYttfCID, 
  0xe332db00, 0xe076, 0x11d3, 0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4);
#define NS_UNICODETOTEXCMSYTTF_CID \
  { 0xe332db00, 0xe076, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToTeXCMEXttf (TeX Extension -TTF) charset converter
// {e91f9440-e076-11d3-b32f-004005a7a7e4}
NS_DECLARE_ID(kUnicodeToTeXCMEXttfCID, 
  0xe91f9440, 0xe076, 0x11d3, 0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4);
#define NS_UNICODETOTEXCMEXTTF_CID \
  { 0xe91f9440, 0xe076, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToTeXCMSYt1 (TeX Symbol -Type1) charset converter
// {e768ebef-70f9-4fe3-8835-4f4f3fd996e2}
NS_DECLARE_ID(kUnicodeToTeXCMSYt1CID, 
  0xe768ebef, 0x70f9, 0x4fe3, 0x88, 0x35, 0x4f, 0x4f, 0x3f, 0xd9, 0x96, 0xe2);
#define NS_UNICODETOTEXCMSYT1_CID \
  { 0xe768ebef, 0x70f9, 0x4fe3, {0x88, 0x35, 0x4f, 0x4f, 0x3f, 0xd9, 0x96, 0xe2}}

// Class ID for our UnicodeToTeXCMEXt1 (TeX Extension -Type1) charset converter
// {f01cb3e7-4ace-414e-a2b7-eaba03e9c86c}
NS_DECLARE_ID(kUnicodeToTeXCMEXt1CID, 
  0xf01cb3e7, 0x4ace, 0x414e, 0xa2, 0xb7, 0xea, 0xba, 0x03, 0xe9, 0xc8, 0x6c);
#define NS_UNICODETOTEXCMEXT1_CID \
  { 0xf01cb3e7, 0x4ace, 0x414e, {0xa2, 0xb7, 0xea, 0xba, 0x03, 0xe9, 0xc8, 0x6c}}

// Class ID for our UnicodeToMathematica1 charset converter
// {758e4f20-e2f2-11d3-b32f-004005a7a7e4}
NS_DECLARE_ID(kUnicodeToMathematica1CID, 
  0x758e4f20, 0xe2f2, 0x11d3, 0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4);
#define NS_UNICODETOMATHEMATICA1_CID \
  { 0x758e4f20, 0xe2f2, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToMathematica2 charset converter
// {7e6e57c0-e2f2-11d3-b32f-004005a7a7e4}
NS_DECLARE_ID(kUnicodeToMathematica2CID, 
  0x7e6e57c0, 0xe2f2, 0x11d3, 0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4);
#define NS_UNICODETOMATHEMATICA2_CID \
  { 0x7e6e57c0, 0xe2f2, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToMathematica3 charset converter
// {830b5f80-11d3-b32f-004005a7a7e4}
NS_DECLARE_ID(kUnicodeToMathematica3CID, 
  0x830b5f80, 0xe2f2, 0x11d3, 0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4);
#define NS_UNICODETOMATHEMATICA3_CID \
  { 0x830b5f80, 0xe2f2, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToMathematica4 charset converter
// {87ed1160-e2f2-11d3-b32f-004005a7a7e4}
NS_DECLARE_ID(kUnicodeToMathematica4CID, 
  0x87ed1160, 0xe2f2, 0x11d3, 0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4);
#define NS_UNICODETOMATHEMATICA4_CID \
  { 0x87ed1160, 0xe2f2, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToMathematica5 charset converter
// {8a0dce80-e2f2-11d3-b32f-004005a7a7e4}
NS_DECLARE_ID(kUnicodeToMathematica5CID, 
  0x8a0dce80, 0xe2f2, 0x11d3, 0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4);
#define NS_UNICODETOMATHEMATICA5_CID \
  { 0x8a0dce80, 0xe2f2, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToMTExtra charset converter
// {0cb0d9a0-f503-11d3-b32f-004005a7a7e4}
NS_DECLARE_ID(kUnicodeToMTExtraCID,
  0x0cb0d9a0, 0xf503, 0x11d3, 0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4);
#define NS_UNICODETOMTEXTRA_CID \
  { 0x0cb0d9a0, 0xf503, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

#endif /* nsUCvMathCID_h___ */
