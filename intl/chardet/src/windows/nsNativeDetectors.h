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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsNativeDetectors_h__
#define nsNativeDetectors_h__

#include "nsIFactory.h"

// {4FE25140-3944-11d3-9142-006008A6EDF6}
#define NS_JA_NATIVE_DETECTOR_CID \
{ 0x4fe25140, 0x3944, 0x11d3, { 0x91, 0x42, 0x0, 0x60, 0x8, 0xa6, 0xed, 0xf6 } };

// {57296530-3644-11d3-9142-006008A6EDF6}
#define NS_JA_NATIVE_STRING_DETECTOR_CID \
{ 0x57296530, 0x3644, 0x11d3, { 0x91, 0x42, 0x0, 0x60, 0x8, 0xa6, 0xed, 0xf6 } }

// {4FE25141-3944-11d3-9142-006008A6EDF6}
#define NS_KO_NATIVE_DETECTOR_CID \
{ 0x4fe25141, 0x3944, 0x11d3, { 0x91, 0x42, 0x0, 0x60, 0x8, 0xa6, 0xed, 0xf6 } };

// {4FE25142-3944-11d3-9142-006008A6EDF6}
#define NS_KO_NATIVE_STRING_DETECTOR_CID \
{ 0x4fe25142, 0x3944, 0x11d3, { 0x91, 0x42, 0x0, 0x60, 0x8, 0xa6, 0xed, 0xf6 } };

nsIFactory* NEW_JA_NATIVEDETECTOR_FACTORY();
nsIFactory* NEW_JA_STRING_NATIVEDETECTOR_FACTORY();
nsIFactory* NEW_KO_NATIVEDETECTOR_FACTORY();
nsIFactory* NEW_KO_STRING_NATIVEDETECTOR_FACTORY();

#endif /* nsNativeDetectors_h__ */
