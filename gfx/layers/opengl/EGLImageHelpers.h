/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EGLIMAGEHELPERS_H_
#define EGLIMAGEHELPERS_H_

#include "mozilla/gfx/Point.h"

typedef void* EGLImage;

namespace mozilla {
namespace gl {
    class GLContext;
}

namespace layers {

EGLImage EGLImageCreateFromNativeBuffer(gl::GLContext* aGL, void* aBuffer, const gfx::IntSize& aCropSize);
void EGLImageDestroy(gl::GLContext* aGL, EGLImage aImage);

} // namespace layers
} // namespace mozilla

#endif // EGLIMAGEHELPERS_H_
