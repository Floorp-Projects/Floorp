/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EGLImageHelpers.h"
#include "GLContext.h"
#include "GLLibraryEGL.h"

namespace mozilla
{
namespace layers {

using namespace gl;

EGLImage
EGLImageCreateFromNativeBuffer(GLContext* aGL, void* aBuffer, const gfx::IntSize& aCropSize)
{
    EGLint attrs[] = {
        LOCAL_EGL_IMAGE_PRESERVED, LOCAL_EGL_TRUE,
        LOCAL_EGL_NONE, LOCAL_EGL_NONE,
    };

    EGLint cropAttrs[] = {
        LOCAL_EGL_IMAGE_PRESERVED, LOCAL_EGL_TRUE,
        LOCAL_EGL_IMAGE_CROP_LEFT_ANDROID, 0,
        LOCAL_EGL_IMAGE_CROP_TOP_ANDROID, 0,
        LOCAL_EGL_IMAGE_CROP_RIGHT_ANDROID, aCropSize.width,
        LOCAL_EGL_IMAGE_CROP_BOTTOM_ANDROID, aCropSize.height,
        LOCAL_EGL_NONE, LOCAL_EGL_NONE,
    };

    bool hasCropRect = (aCropSize.width != 0 && aCropSize.height != 0);
    EGLint* usedAttrs = attrs;
    if (hasCropRect && sEGLLibrary.IsExtensionSupported(GLLibraryEGL::EGL_ANDROID_image_crop)) {
        usedAttrs = cropAttrs;
    }

    return sEGLLibrary.fCreateImage(sEGLLibrary.Display(),
                                     EGL_NO_CONTEXT,
                                     LOCAL_EGL_NATIVE_BUFFER_ANDROID,
                                     aBuffer, usedAttrs);
}

void
EGLImageDestroy(GLContext* aGL, EGLImage aImage)
{
    sEGLLibrary.fDestroyImage(sEGLLibrary.Display(), aImage);
}

} // namespace layers
} // namespace mozilla
