/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@mozilla.org>
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
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
}

nsJSRuntimeServiceImpl::~nsJSRuntimeServiceImpl() {
    if(mRuntime)
    {
        JS_DestroyRuntime(mRuntime);
        JS_ShutDown();
#ifdef DEBUG_shaver
        fprintf(stderr, "nJRSI: destroyed runtime %p\n", (void *)mRuntime);
#endif
    }
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsJSRuntimeServiceImpl,
                              nsIJSRuntimeService,
                              nsISupportsWeakReference);

static nsJSRuntimeServiceImpl* gJSRuntimeService = nsnull;

nsJSRuntimeServiceImpl*
nsJSRuntimeServiceImpl::GetSingleton()
{
    if(!gJSRuntimeService)
    {
        gJSRuntimeService = new nsJSRuntimeServiceImpl();
        // hold an extra reference to lock it down
        NS_IF_ADDREF(gJSRuntimeService);
    }
    NS_IF_ADDREF(gJSRuntimeService);

    return gJSRuntimeService;
}

void
nsJSRuntimeServiceImpl::FreeSingleton()
{
    NS_IF_RELEASE(gJSRuntimeService);
}

const uint32 gGCSize = 4L * 1024L * 1024L; /* pref? */

/* attribute JSRuntime runtime; */
NS_IMETHODIMP
nsJSRuntimeServiceImpl::GetRuntime(JSRuntime **runtime)
{
    if(!runtime)
        return NS_ERROR_NULL_POINTER;

    if(!mRuntime)
    {
        mRuntime = JS_NewRuntime(gGCSize);
        if(!mRuntime)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    *runtime = mRuntime;
#ifdef DEBUG_shaver
    fprintf(stderr, "nJRSI: returning %p\n", (void *)mRuntime);
#endif
    return NS_OK;
}
