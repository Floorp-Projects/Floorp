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
#pragma once
#include <stdlib.h>
#include "xp_core.h"

typedef enum MemoryPriority { fcNone, fcLow, fcMedium, fcHigh, fcTop } MemoryPriority;

#define NEW( CLASS )		( new CLASS )
#define DELETE( OBJECT )	delete OBJECT

XP_BEGIN_PROTOS

extern void*		Flush_Allocate( size_t size, int zero );
extern void*		Flush_Reallocate( void* item, size_t size );
extern void			Flush_Free( void* item );
	
XP_END_PROTOS
