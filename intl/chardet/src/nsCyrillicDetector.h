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
#ifndef nsCyrillicDetector_h__
#define nsCyrillicDetector_h__


#include "nsIFactory.h"



// {2002F781-3960-11d3-B3C3-00805F8A6670}
#define NS_RU_PROBDETECTOR_CID \
{ 0x2002f781, 0x3960, 0x11d3, { 0xb3, 0xc3, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


// {2002F782-3960-11d3-B3C3-00805F8A6670}
#define NS_UK_PROBDETECTOR_CID \
{ 0x2002f782, 0x3960, 0x11d3, { 0xb3, 0xc3, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {2002F783-3960-11d3-B3C3-00805F8A6670}
#define NS_RU_STRING_PROBDETECTOR_CID \
{ 0x2002f783, 0x3960, 0x11d3, { 0xb3, 0xc3, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


// {2002F784-3960-11d3-B3C3-00805F8A6670}
#define NS_UK_STRING_PROBDETECTOR_CID \
{ 0x2002f784, 0x3960, 0x11d3, { 0xb3, 0xc3, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

nsIFactory* NEW_PROBDETECTOR_FACTORY(const nsCID& aClass);

#endif
