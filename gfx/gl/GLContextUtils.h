/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTUTILS_H_
#define GLCONTEXTUTILS_H_

#include "GLContextTypes.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
namespace gfx {
  class DataSourceSurface;
}
}

namespace mozilla {
namespace gl {

TemporaryRef<gfx::DataSourceSurface>
ReadBackSurface(GLContext* aContext, GLuint aTexture, bool aYInvert, gfx::SurfaceFormat aFormat);

} // namespace gl
} // namespace mozilla

#endif /* GLCONTEXTUTILS_H_ */
