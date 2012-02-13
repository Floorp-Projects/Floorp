/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsString.h"
#include "nsIObjectOutputStream.h"
#include "nsIObjectInputStream.h"
#include "nsJSPrincipals.h"
#include "plstr.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "jsapi.h"
#include "jsxdrapi.h"
#include "nsIJSRuntimeService.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsStringBuffer.h"

// for mozilla::dom::workers::kJSPrincipalsDebugToken
#include "mozilla/dom/workers/Workers.h"

/* static */ JSBool
nsJSPrincipals::Subsume(JSPrincipals *jsprin, JSPrincipals *other)
{
    bool result;
    nsresult rv = nsJSPrincipals::get(jsprin)->Subsumes(nsJSPrincipals::get(other), &result);
    return NS_SUCCEEDED(rv) && result;
}

/* static */ void
nsJSPrincipals::Destroy(JSPrincipals *jsprin)
{
    // The JS runtime can call this method during the last GC when
    // nsScriptSecurityManager is destroyed. So we must not assume here that
    // the security manager still exists.

    nsJSPrincipals *nsjsprin = nsJSPrincipals::get(jsprin);

    // We need to destroy the nsIPrincipal. We'll do this by adding
    // to the refcount and calling release

#ifdef NS_BUILD_REFCNT_LOGGING
    // The refcount logging considers AddRef-to-1 to indicate creation,
    // so trick it into thinking it's otherwise, but balance the
    // Release() we do below.
    nsjsprin->refcount++;
    nsjsprin->AddRef();
    nsjsprin->refcount--;
#else
    nsjsprin->refcount++;
#endif
    nsjsprin->Release();
}

#ifdef DEBUG

// Defined here so one can do principals->dump() in the debugger
JS_EXPORT_API(void)
JSPrincipals::dump()
{
    if (debugToken == nsJSPrincipals::DEBUG_TOKEN) {
        static_cast<nsJSPrincipals *>(this)->dumpImpl();
    } else if (debugToken == mozilla::dom::workers::kJSPrincipalsDebugToken) {
        fprintf(stderr, "Web Worker principal singleton (%p)\n", this);
    } else {
        fprintf(stderr,
                "!!! JSPrincipals (%p) is not nsJSPrincipals instance - bad token: "
                "actual=0x%x expected=0x%x\n",
                this, unsigned(debugToken), unsigned(nsJSPrincipals::DEBUG_TOKEN));
    }
}

#endif 
