//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayNSGL.mm: NSOpenGL implementation of egl::Display

#include "libANGLE/renderer/gl/nsgl/DisplayNSGL.h"

#include "common/debug.h"
#include "libANGLE/renderer/gl/nsgl/WindowSurfaceNSGL.h"

namespace rx
{

DisplayNSGL::DisplayNSGL()
    : DisplayGL(),
      mEGLDisplay(nullptr)
{
}

DisplayNSGL::~DisplayNSGL()
{
}

egl::Error DisplayNSGL::initialize(egl::Display *display)
{
    UNIMPLEMENTED();
    mEGLDisplay = display;
    return DisplayGL::initialize(display);
}

void DisplayNSGL::terminate()
{
    UNIMPLEMENTED();
    DisplayGL::terminate();
}

SurfaceImpl *DisplayNSGL::createWindowSurface(const egl::Config *configuration,
                                             EGLNativeWindowType window,
                                             const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return new WindowSurfaceNSGL();
}

SurfaceImpl *DisplayNSGL::createPbufferSurface(const egl::Config *configuration,
                                              const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

SurfaceImpl* DisplayNSGL::createPbufferFromClientBuffer(const egl::Config *configuration,
                                                       EGLClientBuffer shareHandle,
                                                       const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

SurfaceImpl *DisplayNSGL::createPixmapSurface(const egl::Config *configuration,
                                             NativePixmapType nativePixmap,
                                             const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

egl::Error DisplayNSGL::getDevice(DeviceImpl **device)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_DISPLAY);
}

egl::ConfigSet DisplayNSGL::generateConfigs() const
{
    UNIMPLEMENTED();
    egl::ConfigSet configs;
    return configs;
}

bool DisplayNSGL::isDeviceLost() const
{
    UNIMPLEMENTED();
    return false;
}

bool DisplayNSGL::testDeviceLost()
{
    UNIMPLEMENTED();
    return false;
}

egl::Error DisplayNSGL::restoreLostDevice()
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_DISPLAY);
}

bool DisplayNSGL::isValidNativeWindow(EGLNativeWindowType window) const
{
    UNIMPLEMENTED();
    return true;
}

std::string DisplayNSGL::getVendorString() const
{
    UNIMPLEMENTED();
    return "";
}

const FunctionsGL *DisplayNSGL::getFunctionsGL() const
{
    UNIMPLEMENTED();
    return nullptr;
}

void DisplayNSGL::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    UNIMPLEMENTED();
}

void DisplayNSGL::generateCaps(egl::Caps *outCaps) const
{
    UNIMPLEMENTED();
}
}
