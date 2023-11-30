/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FontPreloader.h"

#include "mozilla/FontLoaderUtils.h"
#include "gfxPlatform.h"

namespace mozilla {

FontPreloader::FontPreloader()
    : FetchPreloader(nsIContentPolicy::TYPE_INTERNAL_FONT_PRELOAD) {}

nsresult FontPreloader::CreateChannel(
    nsIChannel** aChannel, nsIURI* aURI, const CORSMode aCORSMode,
    const dom::ReferrerPolicy& aReferrerPolicy, dom::Document* aDocument,
    nsILoadGroup* aLoadGroup, nsIInterfaceRequestor* aCallbacks,
    uint64_t aEarlyHintPreloaderId, int32_t aSupportsPriorityValue) {
  // Don't preload fonts if they've been preffed-off.
  if (!gfxPlatform::GetPlatform()->DownloadableFontsEnabled()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return FontLoaderUtils::BuildChannel(
      aChannel, aURI, aCORSMode, aReferrerPolicy, nullptr, nullptr, aDocument,
      aLoadGroup, aCallbacks, true, aSupportsPriorityValue);
}

}  // namespace mozilla
