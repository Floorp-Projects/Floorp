/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
