/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXT_TYPES_H_
#define GLCONTEXT_TYPES_H_

#include "GLTypes.h"
#include "mozilla/TypedEnumBits.h"

namespace mozilla {
namespace gl {

class GLContext;

enum class GLContextType { Unknown, WGL, CGL, GLX, EGL, EAGL };

enum class OriginPos : uint8_t { TopLeft, BottomLeft };

struct GLFormats {
  GLenum color_texInternalFormat = 0;
  GLenum color_texFormat = 0;
  GLenum color_texType = 0;
  GLenum color_rbFormat = 0;

  GLenum depthStencil = 0;
  GLenum depth = 0;
  GLenum stencil = 0;
};

enum class CreateContextFlags : uint8_t {
  NONE = 0,
  REQUIRE_COMPAT_PROFILE = 1 << 0,
  // Force the use of hardware backed GL, don't allow software implementations.
  FORCE_ENABLE_HARDWARE = 1 << 1,
  /* Don't force discrete GPU to be used (if applicable) */
  ALLOW_OFFLINE_RENDERER = 1 << 2,
  // Ask for ES3 if possible
  PREFER_ES3 = 1 << 3,

  NO_VALIDATION = 1 << 4,
  PREFER_ROBUSTNESS = 1 << 5,

  HIGH_POWER = 1 << 6,

  PROVOKING_VERTEX_DONT_CARE = 1 << 7,
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(CreateContextFlags)

} /* namespace gl */
} /* namespace mozilla */

#endif /* GLCONTEXT_TYPES_H_ */
