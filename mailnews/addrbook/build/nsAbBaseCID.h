/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsAbBaseCID_h__
#define nsAbBaseCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define NS_ABDIRECTORYDATASOURCE_CID					\
{ /* 0A79186D-F754-11d2-A2DA-001083003D0C */		\
    0xa79186d, 0xf754, 0x11d2,						\
    {0xa2, 0xda, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

#define NS_ABDIRECTORYRESOURCE_CID                    \
{ /* {6C21831D-FCC2-11d2-A2E2-001083003D0C}*/		\
	0x6c21831d, 0xfcc2, 0x11d2,						\
	{0xa2, 0xe2, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

#define NS_ABCARDDATASOURCE_CID					\
{ /* 1920E486-0709-11d3-A2EC-001083003D0C */		\
    0x1920e486, 0x709, 0x11d3,						\
    {0xa2, 0xec, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

#define NS_ABCARDRESOURCE_CID                    \
{ /* {1920E487-0709-11d3-A2EC-001083003D0C}*/		\
	0x1920e487, 0x709, 0x11d3,						\
	{0xa2, 0xec, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

#endif // nsAbBaseCID_h__
