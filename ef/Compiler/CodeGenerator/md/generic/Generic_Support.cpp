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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*
	Generic_Support
*/

#include "NativeCodeCache.h"
#include <stdio.h>
#include "Fundamentals.h"

void* backPatchMethod(void* /*inMethodAddress*/, void* /*inLastPC*/, void* /*inUserDefined*/)
{
	trespass("not implemented");
	return (NULL);
}

static void dummyStub();

static void dummyStub()
{
	printf("dummyStub #1\n");
}

void* 
generateCompileStub(NativeCodeCache& /*inCache*/, const CacheEntry& /*inCacheEntry*/)
{
#ifndef GENERATE_FOR_PPC
	return ((void*) dummyStub); 
#else
	return (*(void**) dummyStub);
#endif
}
