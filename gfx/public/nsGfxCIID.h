/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsGfxCIID_h__
#define nsGfxCIID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define NS_IMAGE_CID \
{ 0x6049b260, 0xc1e6, 0x11d1, \
{ 0xa8, 0x27, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

#define NS_RENDERING_CONTEXT_CID \
{ 0x6049b261, 0xc1e6, 0x11d1, \
{ 0xa8, 0x27, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

#define NS_DEVICE_CONTEXT_CID \
{ 0x6049b262, 0xc1e6, 0x11d1, \
{ 0xa8, 0x27, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

#define NS_FONT_METRICS_CID \
{ 0x6049b263, 0xc1e6, 0x11d1, \
{ 0xa8, 0x27, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

#define NS_FONT_ENUMERATOR_CID \
{ 0xa6cf9115, 0x15b3, 0x11d2, \
{ 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

#define NS_REGION_CID \
{ 0xe12752f0, 0xee9a, 0x11d1, \
{ 0xa8, 0x2a, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

#define NS_SCRIPTABLE_REGION_CID \
{ 0xda5b130a, 0x1dd1, 0x11b2, \
{ 0xad, 0x47, 0xf4, 0x55, 0xb1, 0x81, 0x4a, 0x78 } }

#define NS_BLENDER_CID \
{ 0x6049b264, 0xc1e6, 0x11d1, \
{ 0xa8, 0x27, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

#define NS_DRAWING_SURFACE_CID \
{ 0x199c7040, 0xcab0, 0x11d2, \
{ 0xa8, 0x49, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

#endif
