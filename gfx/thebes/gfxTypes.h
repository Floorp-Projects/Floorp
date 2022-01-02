/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TYPES_H
#define GFX_TYPES_H

#include <stdint.h>
#include "mozilla/TypedEnumBits.h"

namespace mozilla {
enum class StyleGenericFontFamily : uint32_t;
}

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_user_data_key cairo_user_data_key_t;

typedef void (*thebes_destroy_func_t)(void* data);

/**
 * Currently needs to be 'double' for Cairo compatibility. Could
 * become 'float', perhaps, in some configurations.
 */
typedef double gfxFloat;

/**
 * Priority of a line break opportunity.
 *
 * eNoBreak       The line has no break opportunities
 * eWordWrapBreak The line has a break opportunity only within a word. With
 *                overflow-wrap|word-wrap: break-word we will break at this
 *                point only if there are no other break opportunities in the
 *                line.
 * eNormalBreak   The line has a break opportunity determined by the standard
 *                line-breaking algorithm.
 *
 * Future expansion: split eNormalBreak into multiple priorities, e.g.
 *                    punctuation break and whitespace break (bug 389710).
 *                   As and when we implement it, text-wrap: unrestricted will
 *                    mean that priorities are ignored and all line-break
 *                    opportunities are equal.
 *
 * @see gfxTextRun::BreakAndMeasureText
 * @see nsLineLayout::NotifyOptionalBreakPosition
 */
enum class gfxBreakPriority { eNoBreak = 0, eWordWrapBreak, eNormalBreak };

enum class gfxSurfaceType {
  Image,
  PDF,
  PS,
  Xlib,
  Xcb,
  Glitz,  // unused, but needed for cairo parity
  Quartz,
  Win32,
  BeOS,
  DirectFB,  // unused, but needed for cairo parity
  SVG,
  OS2,
  Win32Printing,
  QuartzImage,
  Script,
  QPainter,
  Recording,
  VG,
  GL,
  DRM,
  Tee,
  XML,
  Skia,
  Subsurface,
  Max
};

enum class gfxContentType {
  COLOR = 0x1000,
  ALPHA = 0x2000,
  COLOR_ALPHA = 0x3000,
  SENTINEL = 0xffff
};

enum class gfxAlphaType {
  Opaque,
  Premult,
  NonPremult,
};

/**
 * Type used to record how a particular font is selected during the font-
 * matching process, so that this can be exposed to the Inspector.
 */
struct FontMatchType {
  enum class Kind : uint8_t {
    kUnspecified = 0,
    kFontGroup = 1,
    kPrefsFallback = 1 << 1,
    kSystemFallback = 1 << 2,
  };

  inline FontMatchType& operator|=(const FontMatchType& aOther);

  bool operator==(const FontMatchType& aOther) const {
    return kind == aOther.kind && generic == aOther.generic;
  }

  bool operator!=(const FontMatchType& aOther) const {
    return !(*this == aOther);
  }

  MOZ_IMPLICIT FontMatchType() = default;
  MOZ_IMPLICIT FontMatchType(Kind aKind) : kind(aKind) {}
  FontMatchType(Kind aKind, mozilla::StyleGenericFontFamily aGeneric)
      : kind(aKind), generic(aGeneric) {}

  Kind kind = static_cast<Kind>(0);
  // Using 0 to avoid pulling ServoStyleConsts.h everywhere.
  mozilla::StyleGenericFontFamily generic = mozilla::StyleGenericFontFamily(0);
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(FontMatchType::Kind)

FontMatchType& FontMatchType::operator|=(const FontMatchType& aOther) {
  kind |= aOther.kind;
  // We only keep track of one generic.
  if (generic != aOther.generic) {
    generic = mozilla::StyleGenericFontFamily(0);
  }
  return *this;
}

// Installation status (base system / langpack / user-installed) may determine
// whether the font is visible to CSS font-family or src:local() lookups.
// (Exactly what these mean and how accurate they are may be vary across
// platforms -- e.g. on Linux there is no clear "base" set of fonts.)
enum class FontVisibility : uint8_t {
  Unknown = 0,   // No categorization of families available on this system
  Base = 1,      // Standard part of the base OS installation
  LangPack = 2,  // From an optional OS component such as language support
  User = 3,      // User-installed font (or installed by another app, etc)
  Hidden = 4,    // Internal system font, should never exposed to users
  Webfont = 5,   // Webfont defined by @font-face
  Count = 6,     // Count of values, for IPC serialization
};

struct HwStretchingSupport {
  uint32_t mBoth;
  uint32_t mWindowOnly;
  uint32_t mFullScreenOnly;
  uint32_t mNone;
  uint32_t mError;

  HwStretchingSupport()
      : mBoth(0), mWindowOnly(0), mFullScreenOnly(0), mNone(0), mError(0) {}

  bool IsFullySupported() const {
    return mBoth > 0 && mWindowOnly == 0 && mFullScreenOnly == 0 &&
           mNone == 0 && mError == 0;
  }
};

#endif /* GFX_TYPES_H */
