/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * XPCTL : nsCtlCIID.h
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
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc.  Portions created by SUN are Copyright (C) 2000 SUN
 * Microsystems, Inc. All Rights Reserved.
 *
 * This module 'XPCTL Interface' is based on Pango (www.pango.org)
 * by Red Hat Software. Portions created by Redhat are Copyright (C)
 * 1999 Red Hat Software.
 *
 * Contributor(s):
 *   Prabhat Hegde (prabhat.hegde@sun.com)
 */

#ifndef nsCtlCIID_h___
#define nsCtlCIID_h___

// Class ID for UnicodeToSunIndia font encoding converter for *nix
#define NS_UNICODETOSUNINDIC_CID \
{ 0xc270e4e7, 0x3915, 0x43fe, \
 { 0xbc, 0xb0, 0x57, 0x4e, 0x68, 0xaf, 0x6b, 0xaf } }

// Class ID for UnicodeToTIS620 charset converter for *nix
#define NS_UNICODETOTIS620_CID \
{ 0xa2297171, 0x41ee, 0x498a, \
 { 0x83, 0x12, 0x1a, 0x63, 0x27, 0xd0, 0x44, 0x3a } }

// Class ID for UnicodeToThaiTTF charset converter 
#define NS_UNICODETOTHAITTF_CID \
{ 0xf15aec07, 0xd606, 0x452b, \
 { 0x96, 0xd6, 0x55, 0xa6, 0x84, 0x3f, 0x97, 0xf6 } }

// 1B285478-11B7-4EA3-AF47-2A7D117845AC
#define NS_ILE_IID \
{ 0x1b285478, 0x11b7, 0x4ea3, \
{ 0xaf, 0x47, 0x2a, 0x7d, 0x11, 0x78, 0x45, 0xac } }

#define NS_ILE_PROGID "@mozilla.org/extensions/ctl;1"

// A47B6D8A-CB2D-439E-B186-AC40F73B8252
#define NS_ULE_CID \
{ 0xa47b6d8a, 0xcb2d, 0x439e, \
  { 0xb1, 0x86, 0xac, 0x40, 0xf7, 0x3b, 0x82, 0x52 } }

// 6898A17D-5933-4D49-9CB4-CB261206FCC0
#define NS_ULE_IID \
{ 0x6898a17d, 0x5933, 0x4d49, \
  { 0x9c, 0xb4, 0xcb, 0x26, 0x12, 0x06, 0xfc, 0xc0 } }

#define NS_ULE_PROGID "@mozilla.org/intl/extensions/nsULE;1"

// D9E30F46-0EB5-4763-A7BD-26DECB30952F
#define NS_ISHAPEDTEXT_IID \
{ 0xd9e30f46, 0x0eb5, 0x4763, \
{ 0xa7, 0xbd, 0x26, 0xde, 0xcb, 0x30, 0x95, 0x2f } }

// {2997A657-AD7B-4036-827C-FBB3B443845B}
#define NS_SHAPEDTEXT_CID \
{ 0x2997a657, 0xad7b, 0x4036, \
  { 0x82, 0x7c, 0xfb, 0xb3, 0xb4, 0x43, 0x84, 0x5b } }

// {A47B6D8A-CB2D-439E-B186-AC40F73B8252}
#define NS_SHAPEDTEXT_IID \
{ 0xa47b6d8a, 0xcb2d, 0x439e, \
  { 0xb1, 0x86, 0xac, 0x40, 0xf7, 0x3b, 0x82, 0x52 } }

#define NS_SHAPEDTEXT_PROGID "component://netscape/extensions/ctl/nsShapedText"

#endif /* !nsCtlCIID_h___ */
