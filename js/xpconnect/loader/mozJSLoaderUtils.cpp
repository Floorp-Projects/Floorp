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
 * The Original Code is mozilla.org
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010-2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <mwu@mozilla.com>
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

#include "nsAutoPtr.h"
#include "nsScriptLoader.h"

#include "jsapi.h"
#include "jsdbgapi.h"
#include "jsxdrapi.h"

#include "nsJSPrincipals.h"

#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"

using namespace mozilla::scache;

// We only serialize scripts with system principals. So we don't serialize the
// principals when writing a script. Instead, when reading it back, we set the
// principals to the system principals.
nsresult
ReadCachedScript(StartupCache* cache, nsACString &uri, JSContext *cx,
                 nsIPrincipal *systemPrincipal, JSScript **script)
{
    nsAutoArrayPtr<char> buf;
    PRUint32 len;
    nsresult rv = cache->GetBuffer(PromiseFlatCString(uri).get(),
                                   getter_Transfers(buf), &len);
    if (NS_FAILED(rv)) {
        return rv; // don't warn since NOT_AVAILABLE is an ok error
    }

    JSXDRState *xdr = ::JS_XDRNewMem(cx, JSXDR_DECODE);
    if (!xdr) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    ::JS_XDRMemSetData(xdr, buf, len);
    ::JS_XDRSetPrincipals(xdr, nsJSPrincipals::get(systemPrincipal), nsnull);

    JSBool ok = ::JS_XDRScript(xdr, script);
    
    // Prevent XDR from automatically freeing the buffer.
    ::JS_XDRMemSetData(xdr, NULL, 0);
    ::JS_XDRDestroy(xdr);

    return ok ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
WriteCachedScript(StartupCache* cache, nsACString &uri, JSContext *cx,
                  nsIPrincipal *systemPrincipal, JSScript *script)
{
    MOZ_ASSERT(JS_GetScriptPrincipals(script) == nsJSPrincipals::get(systemPrincipal));
    MOZ_ASSERT(JS_GetScriptOriginPrincipals(script) == nsJSPrincipals::get(systemPrincipal));

    JSXDRState *xdr = ::JS_XDRNewMem(cx, JSXDR_ENCODE);
    if (!xdr) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv;
    if (!::JS_XDRScript(xdr, &script)) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        uint32_t size;
        char* data = static_cast<char *>(::JS_XDRMemGetData(xdr, &size));
        MOZ_ASSERT(size);
        rv = cache->PutBuffer(PromiseFlatCString(uri).get(), data, size);
    }

    ::JS_XDRDestroy(xdr);
    return rv;
}
