/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
