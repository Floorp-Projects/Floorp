/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsIMsgFilterHitNotify_H_
#define _nsIMsgFilterHitNotify_H_

#include "nscore.h"
#include "nsISupports.h"
#include "nsMsgFilterCore.h"


////////////////////////////////////////////////////////////////////////////////////////
// nsIMsgFilterHitNotify is an interface designed to make evaluating filters
// easier. Clients typically open a filter list and ask the filter list to
// evaluate the filters for a particular message, and pass in an interface pointer
// to be notified of hits. The filter list will call the ApplyFilterHit method on the
// interface pointer in case of hits, along with the desired action and value.
// applyMore is an out parameter used to indicate whether the
// filter list should continue trying to apply filters or not. 
//
////////////////////////////////////////////////////////////////////////////////////////
// c9f15174-1f3f-11d3-a51b-0060b0fc04b7

#define NS_IMSGFILTERHITNOTIFY_IID                         \
{ 0xc9f15174, 0x1f3f, 0x11d3,                 \
    { 0xa5, 0x1b, 0x0, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

class nsIMsgFilter;

class nsIMsgFilterHitNotify : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGFILTERHITNOTIFY_IID; return iid; }
	NS_IMETHOD ApplyFilterHit(nsIMsgFilter *filter, PRBool *applyMore) = 0;
};

#endif



