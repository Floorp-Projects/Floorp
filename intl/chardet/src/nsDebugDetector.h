/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */
#ifndef nsDebugDetector_h__
#define nsDebugDetector_h__

#include "nsIFactory.h"


// {12BB8F18-2389-11d3-B3BF-00805F8A6670}
#define NS_1STBLKDBG_DETECTOR_CID \
{ 0x12bb8f18, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {12BB8F19-2389-11d3-B3BF-00805F8A6670}
#define NS_2NDBLKDBG_DETECTOR_CID \
{ 0x12bb8f19, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {12BB8F1A-2389-11d3-B3BF-00805F8A6670}
#define NS_LASTBLKDBG_DETECTOR_CID \
{ 0x12bb8f1a, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }



nsIFactory* NEW_1STBLKDBG_DETECTOR_FACTORY();
nsIFactory* NEW_2NDBLKDBG_DETECTOR_FACTORY();
nsIFactory* NEW_LASTBLKDBG_DETECTOR_FACTORY();

#endif // nsDebugDetector_h__
