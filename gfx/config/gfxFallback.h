/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_gfx_config_gfxFallback_h
#define mozilla_gfx_config_gfxFallback_h

#include <stdint.h>
#include "gfxTelemetry.h"

namespace mozilla {
namespace gfx {

#define GFX_FALLBACK_MAP(_)        \
  /* Name */                       \
  _(NO_CONSTANT_BUFFER_OFFSETTING) \
  /* Add new entries above this comment */

enum class Fallback : uint32_t {
#define MAKE_ENUM(name) name,
  GFX_FALLBACK_MAP(MAKE_ENUM)
#undef MAKE_ENUM
      NumValues
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_config_gfxFallback_h
