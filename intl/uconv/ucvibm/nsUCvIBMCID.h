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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsUCvIBMCID_h___
#define nsUCvIBMCID_h___

#include "nsISupports.h"


// Class ID for our CP850ToUnicode charset converter
// {2D524FD0-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_CP850TOUNICODE_CID \
  { 0x2d524fd0, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our CP852ToUnicode charset converter
// {2D524FD1-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_CP852TOUNICODE_CID \
  { 0x2d524fd1, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our CP855ToUnicode charset converter
// {2D524FD2-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_CP855TOUNICODE_CID \
  { 0x2d524fd2, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our CP857ToUnicode charset converter
// {2D524FD3-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_CP857TOUNICODE_CID \
  { 0x2d524fd3, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our CP862ToUnicode charset converter
// {2D524FD4-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_CP862TOUNICODE_CID \
  { 0x2d524fd4, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our CP864ToUnicode charset converter
// {2D524FD5-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_CP864TOUNICODE_CID \
  { 0x2d524fd5, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our UnicodeToCP850 charset converter
// {2D524FD6-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_UNICODETOCP850_CID \
  { 0x2d524fd6, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our UnicodeToCP852 charset converter
// {2D524FD7-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_UNICODETOCP852_CID \
  { 0x2d524fd7, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our UnicodeToCP855 charset converter
// {2D524FD8-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_UNICODETOCP855_CID \
  { 0x2d524fd8, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our UnicodeToCP857 charset converter
// {2D524FD9-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_UNICODETOCP857_CID \
  { 0x2d524fd9, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our UnicodeToCP862 charset converter
// {2D524FDA-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_UNICODETOCP862_CID \
  { 0x2d524fda, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our UnicodeToCP864 charset converter
// {2D524FDB-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_UNICODETOCP864_CID \
  { 0x2d524fdb, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}
  
// Class ID for our CP864iToUnicode charset converter
// {2D524FDC-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_CP864ITOUNICODE_CID \
  { 0x2d524fdc, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

// Class ID for our UnicodeToCP864i charset converter
// {2D524FDD-AE74-11d3-ABF7-0004ACEEFA51}
#define NS_UNICODETOCP864I_CID \
  { 0x2d524fdd, 0xae74, 0x11d3, {0xab, 0xf7, 0x0, 0x4, 0xac, 0xee, 0xfa, 0x51}}

#endif /* nsUCvIBMCID_h___ */
