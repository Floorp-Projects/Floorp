/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TYPES_H
#define GFX_TYPES_H

#include <stdint.h>
#include "mozilla/TypedEnumBits.h"

typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_user_data_key cairo_user_data_key_t;

typedef void (*thebes_destroy_func_t)(void *data);

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
enum class FontMatchType : uint16_t {
  // The CSS generic that mapped to this font, if any. This field of
  // the MatchType stores a FontFamilyType value as defined in the enum
  // in gfxFontFamilyList.h.
  kGenericMask = 0x00ff,

  // Flags for recording the kind of font-matching that was used.
  // Note that multiple flags may be set on a single range.
  kFontGroup = 0x0100,
  kPrefsFallback = 0x0200,
  kSystemFallback = 0x0400
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(FontMatchType)

#endif /* GFX_TYPES_H */
