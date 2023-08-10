/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozJSLoaderUtils_h
#define mozJSLoaderUtils_h

#include "nsString.h"

#include "js/experimental/JSStencil.h"
#include "js/CompileOptions.h"  // JS::ReadOnlyDecodeOptions

namespace mozilla {
namespace scache {
class StartupCache;
}  // namespace scache
}  // namespace mozilla

nsresult ReadCachedStencil(mozilla::scache::StartupCache* cache,
                           nsACString& cachePath, JSContext* cx,
                           const JS::ReadOnlyDecodeOptions& options,
                           JS::Stencil** stencilOut);

nsresult WriteCachedStencil(mozilla::scache::StartupCache* cache,
                            nsACString& cachePath, JSContext* cx,
                            JS::Stencil* stencil);

#endif /* mozJSLoaderUtils_h */
