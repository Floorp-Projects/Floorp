/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsPSMDetectors_h__
#define nsPSMDetectors_h__

#include "nsIFactory.h"


// {12BB8F1B-2389-11d3-B3BF-00805F8A6670}
#define NS_JA_PSMDETECTOR_CID \
{ 0x12bb8f1b, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {12BB8F1C-2389-11d3-B3BF-00805F8A6670}
#define NS_JA_STRING_PSMDETECTOR_CID \
{ 0x12bb8f1c, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {EA06D4E1-2B3D-11d3-B3BF-00805F8A6670}
#define NS_KO_PSMDETECTOR_CID \
{ 0xea06d4e1, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {EA06D4E2-2B3D-11d3-B3BF-00805F8A6670}
#define NS_ZHCN_PSMDETECTOR_CID \
{ 0xea06d4e2, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {EA06D4E3-2B3D-11d3-B3BF-00805F8A6670}
#define NS_ZHTW_PSMDETECTOR_CID \
{ 0xea06d4e3, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


// {EA06D4E4-2B3D-11d3-B3BF-00805F8A6670}
#define NS_KO_STRING_PSMDETECTOR_CID \
{ 0xea06d4e4, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {EA06D4E5-2B3D-11d3-B3BF-00805F8A6670}
#define NS_ZHCN_STRING_PSMDETECTOR_CID \
{ 0xea06d4e5, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {EA06D4E6-2B3D-11d3-B3BF-00805F8A6670}
#define NS_ZHTW_STRING_PSMDETECTOR_CID \
{ 0xea06d4e6, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


// {FCACEF21-2B40-11d3-B3BF-00805F8A6670}
#define NS_ZH_STRING_PSMDETECTOR_CID \
{ 0xfcacef21, 0x2b40, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {FCACEF22-2B40-11d3-B3BF-00805F8A6670}
#define NS_CJK_STRING_PSMDETECTOR_CID \
{ 0xfcacef22, 0x2b40, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


// {FCACEF23-2B40-11d3-B3BF-00805F8A6670}
#define NS_ZH_PSMDETECTOR_CID \
{ 0xfcacef23, 0x2b40, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {FCACEF24-2B40-11d3-B3BF-00805F8A6670}
#define NS_CJK_PSMDETECTOR_CID \
{ 0xfcacef24, 0x2b40, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }






nsIFactory* NEW_PSMDETECTOR_FACTORY(const nsCID& aClass);

#endif // nsPSMDetectors_h__
