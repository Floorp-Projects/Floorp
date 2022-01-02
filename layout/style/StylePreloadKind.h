/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_css_StylePreloadKind_h
#define mozilla_css_StylePreloadKind_h

#include <stdint.h>

namespace mozilla::css {

enum class StylePreloadKind : uint8_t {
  // Not a preload.
  None,
  // An speculative load from the parser for a <link rel="stylesheet"> or
  // @import stylesheet.
  FromParser,
  // A preload (speculative or not) for a <link rel="preload" as="style">
  // element.
  FromLinkRelPreloadElement,
  // A preload for a "Link" rel=preload response header.
  FromLinkRelPreloadHeader,
};

inline bool IsLinkRelPreload(StylePreloadKind aKind) {
  return aKind == StylePreloadKind::FromLinkRelPreloadElement ||
         aKind == StylePreloadKind::FromLinkRelPreloadHeader;
}

}  // namespace mozilla::css

#endif  // mozilla_css_StylePreloadKind_h
