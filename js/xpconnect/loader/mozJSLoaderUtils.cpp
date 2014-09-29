/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/OldDebugAPI.h"

#include "nsJSPrincipals.h"

#include "mozilla/scache/StartupCache.h"

using namespace JS;
using namespace mozilla::scache;

// We only serialize scripts with system principals. So we don't serialize the
// principals when writing a script. Instead, when reading it back, we set the
// principals to the system principals.
nsresult
ReadCachedScript(StartupCache* cache, nsACString &uri, JSContext *cx,
                 nsIPrincipal *systemPrincipal, MutableHandleScript scriptp)
{
    nsAutoArrayPtr<char> buf;
    uint32_t len;
    nsresult rv = cache->GetBuffer(PromiseFlatCString(uri).get(),
                                   getter_Transfers(buf), &len);
    if (NS_FAILED(rv))
        return rv; // don't warn since NOT_AVAILABLE is an ok error

    scriptp.set(JS_DecodeScript(cx, buf, len));
    if (!scriptp)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

nsresult
ReadCachedFunction(StartupCache* cache, nsACString &uri, JSContext *cx,
                   nsIPrincipal *systemPrincipal, JSFunction **functionp)
{
    return NS_ERROR_FAILURE;
/*  This doesn't actually work ...
    nsAutoArrayPtr<char> buf;
    uint32_t len;
    nsresult rv = cache->GetBuffer(PromiseFlatCString(uri).get(),
                                   getter_Transfers(buf), &len);
    if (NS_FAILED(rv))
        return rv; // don't warn since NOT_AVAILABLE is an ok error

    JSObject *obj = JS_DecodeInterpretedFunction(cx, buf, len, nsJSPrincipals::get(systemPrincipal), nullptr);
    if (!obj)
        return NS_ERROR_OUT_OF_MEMORY;
    JSFunction* function = JS_ValueToFunction(cx, OBJECT_TO_JSVAL(obj));
    *functionp = function;
    return NS_OK;*/
}

nsresult
WriteCachedScript(StartupCache* cache, nsACString &uri, JSContext *cx,
                  nsIPrincipal *systemPrincipal, HandleScript script)
{
    MOZ_ASSERT(JS_GetScriptPrincipals(script) == nsJSPrincipals::get(systemPrincipal));

    uint32_t size;
    void *data = JS_EncodeScript(cx, script, &size);
    if (!data)
        return NS_ERROR_OUT_OF_MEMORY;

    MOZ_ASSERT(size);
    nsresult rv = cache->PutBuffer(PromiseFlatCString(uri).get(), static_cast<char *>(data), size);
    js_free(data);
    return rv;
}

nsresult
WriteCachedFunction(StartupCache* cache, nsACString &uri, JSContext *cx,
                    nsIPrincipal *systemPrincipal, JSFunction *function)
{
    return NS_ERROR_FAILURE;
/* This doesn't actually work ...
    uint32_t size;
    void *data =
      JS_EncodeInterpretedFunction(cx, JS_GetFunctionObject(function), &size);
    if (!data)
        return NS_ERROR_OUT_OF_MEMORY;

    MOZ_ASSERT(size);
    nsresult rv = cache->PutBuffer(PromiseFlatCString(uri).get(), static_cast<char *>(data), size);
    js_free(data);
    return rv;*/
}
