//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WindowSurfaceNSGL.cpp: NSGL implementation of egl::Surface for windows

#include "libANGLE/renderer/gl/nsgl/WindowSurfaceNSGL.h"

#include "common/debug.h"

namespace rx
{

WindowSurfaceNSGL::WindowSurfaceNSGL()
    : SurfaceGL()
{
}

WindowSurfaceNSGL::~WindowSurfaceNSGL()
{
}

egl::Error WindowSurfaceNSGL::initialize()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceNSGL::makeCurrent()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceNSGL::swap()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceNSGL::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceNSGL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceNSGL::bindTexImage(EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceNSGL::releaseTexImage(EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

void WindowSurfaceNSGL::setSwapInterval(EGLint interval)
{
    UNIMPLEMENTED();
}

EGLint WindowSurfaceNSGL::getWidth() const
{
    UNIMPLEMENTED();
    return 0;
}

EGLint WindowSurfaceNSGL::getHeight() const
{
    UNIMPLEMENTED();
    return 0;
}

EGLint WindowSurfaceNSGL::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return EGL_FALSE;
}

}
