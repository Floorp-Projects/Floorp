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

#define NS_ADDRESSBOOK_CID							\
{ /* {D60B84F2-2A8C-11d3-9E07-00A0C92B5F0D} */		\
  0xd60b84f2, 0x2a8c, 0x11d3,						\
	{ 0x9e, 0x7, 0x0, 0xa0, 0xc9, 0x2b, 0x5f, 0xd }	\
}

#define NS_ABDIRECTORYDATASOURCE_CID				\
{ /* 0A79186D-F754-11d2-A2DA-001083003D0C */		\
    0xa79186d, 0xf754, 0x11d2,						\
    {0xa2, 0xda, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

#define NS_ABDIRECTORYRESOURCE_CID                  \
{ /* {6C21831D-FCC2-11d2-A2E2-001083003D0C}*/		\
	0x6c21831d, 0xfcc2, 0x11d2,						\
	{0xa2, 0xe2, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

#define NS_ABCARDDATASOURCE_CID						\
{ /* 1920E486-0709-11d3-A2EC-001083003D0C */		\
    0x1920e486, 0x709, 0x11d3,						\
    {0xa2, 0xec, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

#define NS_ABCARDRESOURCE_CID						\
{ /* {1920E487-0709-11d3-A2EC-001083003D0C}*/		\
	0x1920e487, 0x709, 0x11d3,						\
	{0xa2, 0xec, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

#define NS_ADDRESSBOOKDB_CID						\
{ /* 63187917-1D19-11d3-A302-001083003D0C */		\
    0x63187917, 0x1d19, 0x11d3,						\
    {0xa3, 0x2, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

#define NS_ABCARDPROPERTY_CID						\
{ /* 2B722171-2CEA-11d3-9E0B-00A0C92B5F0D */		\
    0x2b722171, 0x2cea, 0x11d3,						\
    {0x9e, 0xb, 0x0, 0xa0, 0xc9, 0x2b, 0x5f, 0xd}	\
}

#define NS_ADDRBOOKSESSION_CID						\
{ /* C5339442-303F-11d3-9E13-00A0C92B5F0D */		\
    0xc5339442, 0x303f, 0x11d3,						\
    {0x9e, 0x13, 0x0, 0xa0, 0xc9, 0x2b, 0x5f, 0xd}	\
}

#define NS_ABDIRPROPERTY_CID						\
{ /* 6FD8EC67-3965-11d3-A316-001083003D0C */		\
    0x6fd8ec67, 0x3965, 0x11d3,						\
    {0xa3, 0x16, 0x0, 0x10, 0x83, 0x0, 0x3d, 0xc}	\
}

#define NS_ABAUTOCOMPLETESESSION_CID				\
{ /* 138DE9BD-362B-11d3-988E-001083010E9B */		\
    0x138de9bd, 0x362b, 0x11d3,						\
    {0x98, 0x8e, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b}	\
}

#define NS_ABADDRESSCOLLECTER_CID \
{	/* fe04c8e6-501e-11d3-a527-0060b0fc04b7 */		\
	0xfe04c8e6, 0x501e, 0x11d3,						\
	{0xa5, 0x27, 0x0, 0x60, 0xb0, 0xfc, 0x4, 0xb7}	\
}

#endif // nsAbBaseCID_h__
