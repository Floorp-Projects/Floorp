/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@mozilla.org>
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
}

nsJSRuntimeServiceImpl::~nsJSRuntimeServiceImpl() {
    if(mRuntime)
    {
        JS_DestroyRuntime(mRuntime);
        JS_ShutDown();
#ifdef DEBUG_shaver_off
        fprintf(stderr, "nJRSI: destroyed runtime %p\n", (void *)mRuntime);
#endif
    }
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsJSRuntimeServiceImpl,
                              nsIJSRuntimeService,
                              nsISupportsWeakReference)

nsJSRuntimeServiceImpl*
nsJSRuntimeServiceImpl::gJSRuntimeService = nsnull;

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
#ifdef DEBUG_shaver_off
    fprintf(stderr, "nJRSI: returning %p\n", (void *)mRuntime);
#endif
    return NS_OK;
}
