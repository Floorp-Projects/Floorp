//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// renderer_utils:
//   Helper methods pertaining to most or all back-ends.
//

#ifndef LIBANGLE_RENDERER_RENDERER_UTILS_H_
#define LIBANGLE_RENDERER_RENDERER_UTILS_H_

#include <cstdint>

#include <map>

#include "libANGLE/angletypes.h"

namespace gl
{
struct FormatType;
struct InternalFormat;
}

namespace rx
{

typedef void (*ColorReadFunction)(const uint8_t *source, uint8_t *dest);
typedef void (*ColorWriteFunction)(const uint8_t *source, uint8_t *dest);
typedef void (*ColorCopyFunction)(const uint8_t *source, uint8_t *dest);

typedef std::map<gl::FormatType, ColorCopyFunction> FastCopyFunctionMap;

struct PackPixelsParams
{
    PackPixelsParams();
    PackPixelsParams(const gl::Rectangle &area,
                     GLenum format,
                     GLenum type,
                     GLuint outputPitch,
                     const gl::PixelPackState &pack,
                     ptrdiff_t offset);

    gl::Rectangle area;
    GLenum format;
    GLenum type;
    GLuint outputPitch;
    gl::Buffer *packBuffer;
    gl::PixelPackState pack;
    ptrdiff_t offset;
};

void PackPixels(const PackPixelsParams &params,
                const gl::InternalFormat &sourceFormatInfo,
                const FastCopyFunctionMap &fastCopyFunctionsMap,
                ColorReadFunction colorReadFunction,
                int inputPitch,
                const uint8_t *source,
                uint8_t *destination);

ColorWriteFunction GetColorWriteFunction(const gl::FormatType &formatType);
ColorCopyFunction GetFastCopyFunction(const FastCopyFunctionMap &fastCopyFunctions,
                                      const gl::FormatType &formatType);

}  // namespace rx

#endif  // LIBANGLE_RENDERER_RENDERER_UTILS_H_
