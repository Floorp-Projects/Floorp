/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "xpcprivate.h"

/*
 * This object holds state that we don't want to lose!
 *
 * The plan is that once created this object never goes away. We do an
 * intentional extra addref at construction to keep it around even if no one
 * is using it.
 */

nsJSRuntimeServiceImpl::nsJSRuntimeServiceImpl() :
    mRuntime(0)
{
    NS_INIT_ISUPPORTS();
    NS_ADDREF_THIS();
}

nsJSRuntimeServiceImpl::~nsJSRuntimeServiceImpl() {
    if (mRuntime)
        JS_DestroyRuntime(mRuntime);
#ifdef DEBUG_shaver
    fprintf(stderr, "nJRSI: destroyed runtime %p\n", mRuntime);
#endif
}

NS_IMPL_ISUPPORTS1(nsJSRuntimeServiceImpl, nsIJSRuntimeService);

nsJSRuntimeServiceImpl*
nsJSRuntimeServiceImpl::GetSingleton()
{
    static nsJSRuntimeServiceImpl *singleton = nsnull;
    if (!singleton) {
	if ((singleton = new nsJSRuntimeServiceImpl()) != nsnull)
	    NS_ADDREF(singleton);
    }
    NS_IF_ADDREF(singleton);
    return singleton;
}

const uint32 gGCSize = 4L * 1024L * 1024L; /* pref? */

/* attribute JSRuntime runtime; */
NS_IMETHODIMP
nsJSRuntimeServiceImpl::GetRuntime(JSRuntime **runtime)
{
    if (!runtime)
	return NS_ERROR_NULL_POINTER;

    if (!mRuntime) {
        mRuntime = JS_NewRuntime(gGCSize);
        if (!mRuntime)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    *runtime = mRuntime;
#ifdef DEBUG_shaver
    fprintf(stderr, "nJRSI: returning %p\n", mRuntime);
#endif
    return NS_OK;
}
