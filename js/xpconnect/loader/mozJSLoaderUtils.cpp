/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "jsapi.h"
#include "jsfriendapi.h"

#include "nsJSPrincipals.h"

#include "mozilla/scache/StartupCache.h"

using namespace JS;
using namespace mozilla::scache;
using mozilla::UniquePtr;

// We only serialize scripts with system principals. So we don't serialize the
// principals when writing a script. Instead, when reading it back, we set the
// principals to the system principals.
nsresult
ReadCachedScript(StartupCache* cache, nsACString& uri, JSContext* cx,
                 nsIPrincipal* systemPrincipal, MutableHandleScript scriptp)
{
    UniquePtr<char[]> buf;
    uint32_t len;
    nsresult rv = cache->GetBuffer(PromiseFlatCString(uri).get(), &buf, &len);
    if (NS_FAILED(rv))
        return rv; // don't warn since NOT_AVAILABLE is an ok error

    TranscodeResult code = JS_DecodeScript(cx, buf.get(), len, scriptp);
    if (code == TranscodeResult_Ok)
        return NS_OK;

    if ((code & TranscodeResult_Failure) != 0)
        return NS_ERROR_FAILURE;

    MOZ_ASSERT((code & TranscodeResult_Throw) != 0);
    JS_ClearPendingException(cx);
    return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
ReadCachedFunction(StartupCache* cache, nsACString& uri, JSContext* cx,
                   nsIPrincipal* systemPrincipal, JSFunction** functionp)
{
    // This doesn't actually work ...
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
WriteCachedScript(StartupCache* cache, nsACString& uri, JSContext* cx,
                  nsIPrincipal* systemPrincipal, HandleScript script)
{
    MOZ_ASSERT(JS_GetScriptPrincipals(script) == nsJSPrincipals::get(systemPrincipal));

    uint32_t size;
    void* data = nullptr;
    TranscodeResult code = JS_EncodeScript(cx, script, &size, &data);
    if (code != TranscodeResult_Ok) {
        if ((code & TranscodeResult_Failure) != 0)
            return NS_ERROR_FAILURE;
        MOZ_ASSERT((code & TranscodeResult_Throw) != 0);
        JS_ClearPendingException(cx);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    MOZ_ASSERT(size && data);
    nsresult rv = cache->PutBuffer(PromiseFlatCString(uri).get(), static_cast<char*>(data), size);
    js_free(data);
    return rv;
}

nsresult
WriteCachedFunction(StartupCache* cache, nsACString& uri, JSContext* cx,
                    nsIPrincipal* systemPrincipal, JSFunction* function)
{
    // This doesn't actually work ...
    return NS_ERROR_NOT_IMPLEMENTED;
}
