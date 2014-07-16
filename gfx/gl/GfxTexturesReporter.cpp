/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GfxTexturesReporter.h"
#include "GLDefs.h"

using namespace mozilla;
using namespace mozilla::gl;

NS_IMPL_ISUPPORTS(GfxTexturesReporter, nsIMemoryReporter)

int64_t GfxTexturesReporter::sAmount = 0;

static uint32_t
GetBitsPerTexel(GLenum format, GLenum type)
{
    // If there is no defined format or type, we're not taking up any memory
    if (!format || !type) {
        return 0;
    }

    if (format == LOCAL_GL_DEPTH_COMPONENT) {
        if (type == LOCAL_GL_UNSIGNED_SHORT)
            return 2*8;
        else if (type == LOCAL_GL_UNSIGNED_INT)
            return 4*8;
    } else if (format == LOCAL_GL_DEPTH_STENCIL) {
        if (type == LOCAL_GL_UNSIGNED_INT_24_8_EXT)
            return 4*8;
    }

    if (type == LOCAL_GL_UNSIGNED_BYTE || type == LOCAL_GL_FLOAT) {
        uint32_t multiplier = type == LOCAL_GL_FLOAT ? 32 : 8;
        switch (format) {
            case LOCAL_GL_ALPHA:
            case LOCAL_GL_LUMINANCE:
                return 1 * multiplier;
            case LOCAL_GL_LUMINANCE_ALPHA:
                return 2 * multiplier;
            case LOCAL_GL_RGB:
                return 3 * multiplier;
            case LOCAL_GL_RGBA:
                return 4 * multiplier;
            case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
            case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
                return 2;
            case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case LOCAL_GL_ATC_RGB:
            case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
            case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
            case LOCAL_GL_ETC1_RGB8_OES:
                return 4;
            case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
            case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
                return 8;
            default:
                break;
        }
    } else if (type == LOCAL_GL_UNSIGNED_SHORT_4_4_4_4 ||
               type == LOCAL_GL_UNSIGNED_SHORT_5_5_5_1 ||
               type == LOCAL_GL_UNSIGNED_SHORT_5_6_5)
    {
        return 2*8;
    }

    MOZ_ASSERT(false);
    return 0;
}

/* static */ void
GfxTexturesReporter::UpdateAmount(MemoryUse action, GLenum format,
                                  GLenum type, int32_t tileWidth,
                                  int32_t tileHeight)
{
    int64_t bitsPerTexel = GetBitsPerTexel(format, type);
    int64_t bytes = int64_t(tileWidth) * int64_t(tileHeight) * bitsPerTexel/8;
    if (action == MemoryFreed) {
        sAmount -= bytes;
    } else {
        sAmount += bytes;
    }
}
