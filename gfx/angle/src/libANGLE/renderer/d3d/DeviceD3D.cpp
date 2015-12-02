//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DeviceD3D.cpp: D3D implementation of egl::Device

#include "libANGLE/renderer/d3d/DeviceD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"

#include "libANGLE/Device.h"
#include "libANGLE/Display.h"

#include <EGL/eglext.h>

namespace rx
{

DeviceD3D::DeviceD3D(void *device, EGLint deviceType) : mDevice(device), mDeviceType(deviceType)
{
}

egl::Error DeviceD3D::getDevice(void **outValue)
{
    *outValue = mDevice;
    return egl::Error(EGL_SUCCESS);
}

EGLint DeviceD3D::getType()
{
    return mDeviceType;
}

void DeviceD3D::generateExtensions(egl::DeviceExtensions *outExtensions) const
{
    outExtensions->deviceD3D = true;
}

}
