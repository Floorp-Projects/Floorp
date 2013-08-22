/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozJSLoaderUtils_h
#define mozJSLoaderUtils_h

#include "nsString.h"

class nsIURI;
namespace mozilla {
namespace scache {
class StartupCache;
}
}

nsresult
ReadCachedScript(mozilla::scache::StartupCache* cache, nsACString &uri,
                 JSContext *cx, nsIPrincipal *systemPrincipal,
                 JSScript **script);

nsresult
ReadCachedFunction(mozilla::scache::StartupCache* cache, nsACString &uri,
                   JSContext *cx, nsIPrincipal *systemPrincipal,
                   JSFunction **function);

nsresult
WriteCachedScript(mozilla::scache::StartupCache* cache, nsACString &uri,
                  JSContext *cx, nsIPrincipal *systemPrincipal,
                  JSScript *script);
nsresult
WriteCachedFunction(mozilla::scache::StartupCache* cache, nsACString &uri,
                    JSContext *cx, nsIPrincipal *systemPrincipal,
                    JSFunction *function);

#endif /* mozJSLoaderUtils_h */
