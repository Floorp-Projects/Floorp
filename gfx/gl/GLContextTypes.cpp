/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextTypes.h"
#include <cstring>

using namespace mozilla::gl;

GLFormats::GLFormats()
{
    std::memset(this, 0, sizeof(GLFormats));
}

PixelBufferFormat::PixelBufferFormat()
{
    std::memset(this, 0, sizeof(PixelBufferFormat));
}
