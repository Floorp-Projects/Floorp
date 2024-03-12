/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <UIKit/UIKit.h>
#include "IOSPlatformFontList.h"

IOSPlatformFontList::IOSPlatformFontList() : CoreTextFontList() {}

IOSPlatformFontList::~IOSPlatformFontList() {}

void IOSPlatformFontList::LookupSystemFont(
    mozilla::LookAndFeel::FontID aSystemFontID, nsACString& aSystemFontName,
    gfxFontStyle& aFontStyle) {
  MOZ_CRASH("UNIMPLEMENTED");
}

nsTArray<std::pair<const char**, uint32_t>>
IOSPlatformFontList::GetFilteredPlatformFontLists() {
  nsTArray<std::pair<const char**, uint32_t>> fontLists;

  return fontLists;
}
