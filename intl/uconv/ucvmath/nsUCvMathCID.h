/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef nsUCvMathCID_h___
#define nsUCvMathCID_h___

#include "nsISupports.h"

// Class ID for our UnicodeToTeXCMRttf (TeX Roman -TTF) charset converter
// {67671792-8e25-4487-b1b7-5073cfa95fee}
#define NS_UNICODETOTEXCMRTTF_CID \
  { 0x67671792, 0x8e25, 0x4487, {0xb1, 0xb7, 0x50, 0x73, 0xcf, 0xa9, 0x5f, 0xee}}

// Class ID for our UnicodeToTeXCMMIttf (TeX Math Italic -TTF) charset converter
// {73bb7c12-dbab-4ae2-aecf-a0331dec916f}
#define NS_UNICODETOTEXCMMITTF_CID \
  {0x73bb7c12, 0xdbab, 0x4ae2, {0xae, 0xcf, 0xa0, 0x33, 0x1d, 0xec, 0x91, 0x6f}}

// Class ID for our UnicodeToTeXCMSYttf (TeX Symbol -TTF) charset converter
// {e332db00-e076-11d3-b32f-004005a7a7e4}
#define NS_UNICODETOTEXCMSYTTF_CID \
  { 0xe332db00, 0xe076, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToTeXCMEXttf (TeX Extension -TTF) charset converter
// {e91f9440-e076-11d3-b32f-004005a7a7e4}
#define NS_UNICODETOTEXCMEXTTF_CID \
  { 0xe91f9440, 0xe076, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToTeXCMRt1 (TeX Roman -Type1) charset converter
// {d5eb483c-67c0-4286-a4a6-330e58a82300}
#define NS_UNICODETOTEXCMRT1_CID \
  {0xd5eb483c, 0x67c0, 0x4286, {0xa4, 0xa6, 0x33, 0x0e, 0x58, 0xa8, 0x23, 0x00}}

// Class ID for our UnicodeToTeXCMMIt1 (TeX Math Italic -Type1) charset converter
// {bd1326a6-5a14-48de-97cc-95b5195f4fb9}
#define NS_UNICODETOTEXCMMIT1_CID \
  {0xbd1326a6, 0x5a14, 0x48de,  {0x97, 0xcc, 0x95, 0xb5, 0x19, 0x5f, 0x4f, 0xb9}}

// Class ID for our UnicodeToTeXCMSYt1 (TeX Symbol -Type1) charset converter
// {e768ebef-70f9-4fe3-8835-4f4f3fd996e2}
#define NS_UNICODETOTEXCMSYT1_CID \
  { 0xe768ebef, 0x70f9, 0x4fe3, {0x88, 0x35, 0x4f, 0x4f, 0x3f, 0xd9, 0x96, 0xe2}}

// Class ID for our UnicodeToTeXCMEXt1 (TeX Extension -Type1) charset converter
// {f01cb3e7-4ace-414e-a2b7-eaba03e9c86c}
#define NS_UNICODETOTEXCMEXT1_CID \
  { 0xf01cb3e7, 0x4ace, 0x414e, {0xa2, 0xb7, 0xea, 0xba, 0x03, 0xe9, 0xc8, 0x6c}}

// Class ID for our UnicodeToMathematica1 charset converter
// {758e4f20-e2f2-11d3-b32f-004005a7a7e4}
#define NS_UNICODETOMATHEMATICA1_CID \
  { 0x758e4f20, 0xe2f2, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToMathematica2 charset converter
// {7e6e57c0-e2f2-11d3-b32f-004005a7a7e4}
#define NS_UNICODETOMATHEMATICA2_CID \
  { 0x7e6e57c0, 0xe2f2, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToMathematica3 charset converter
// {830b5f80-11d3-b32f-004005a7a7e4}
#define NS_UNICODETOMATHEMATICA3_CID \
  { 0x830b5f80, 0xe2f2, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToMathematica4 charset converter
// {87ed1160-e2f2-11d3-b32f-004005a7a7e4}
#define NS_UNICODETOMATHEMATICA4_CID \
  { 0x87ed1160, 0xe2f2, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToMathematica5 charset converter
// {8a0dce80-e2f2-11d3-b32f-004005a7a7e4}
#define NS_UNICODETOMATHEMATICA5_CID \
  { 0x8a0dce80, 0xe2f2, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

// Class ID for our UnicodeToMTExtra charset converter
// {0cb0d9a0-f503-11d3-b32f-004005a7a7e4}
#define NS_UNICODETOMTEXTRA_CID \
  { 0x0cb0d9a0, 0xf503, 0x11d3, {0xb3, 0x2f, 0x0, 0x40, 0x05, 0xa7, 0xa7, 0xe4}}

#endif /* nsUCvMathCID_h___ */
