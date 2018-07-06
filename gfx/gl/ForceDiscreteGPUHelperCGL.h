/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ForceDiscreteGPUHelperCGL_h_
#define ForceDiscreteGPUHelperCGL_h_

#include <OpenGL/OpenGL.h>

/** This RAII helper guarantees that we're on the discrete GPU during its lifetime.
 * 
 * As long as any ForceDiscreteGPUHelperCGL object is alive, we're on the discrete GPU.
 */
class ForceDiscreteGPUHelperCGL
{
    CGLPixelFormatObj mPixelFormatObj;

public:
    ForceDiscreteGPUHelperCGL()
        : mPixelFormatObj(nullptr)
    {
        // the code in this function is taken from Chromium, src/ui/gfx/gl/gl_context_cgl.cc, r122013
        // BSD-style license, (c) The Chromium Authors
        CGLPixelFormatAttribute attribs[1];
        attribs[0] = static_cast<CGLPixelFormatAttribute>(0);
        GLint num_pixel_formats = 0;
        CGLChoosePixelFormat(attribs, &mPixelFormatObj, &num_pixel_formats);
    }

    ~ForceDiscreteGPUHelperCGL()
    {
        CGLReleasePixelFormat(mPixelFormatObj);
    }
};

#endif // ForceDiscreteGPUHelperCGL_h_
