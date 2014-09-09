//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// formatutils.h: Queries for GL image formats.

#ifndef LIBGLESV2_FORMATUTILS_H_
#define LIBGLESV2_FORMATUTILS_H_

#include "angle_gl.h"

#include "libGLESv2/Caps.h"
#include "libGLESv2/angletypes.h"

typedef void (*MipGenerationFunction)(unsigned int sourceWidth, unsigned int sourceHeight, unsigned int sourceDepth,
                                      const unsigned char *sourceData, int sourceRowPitch, int sourceDepthPitch,
                                      unsigned char *destData, int destRowPitch, int destDepthPitch);

typedef void (*LoadImageFunction)(int width, int height, int depth,
                                  const void *input, unsigned int inputRowPitch, unsigned int inputDepthPitch,
                                  void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch);

typedef void (*InitializeTextureDataFunction)(int width, int height, int depth,
                                              void *output, unsigned int outputRowPitch, unsigned int outputDepthPitch);

typedef void (*ColorReadFunction)(const void *source, void *dest);
typedef void (*ColorWriteFunction)(const void *source, void *dest);
typedef void (*ColorCopyFunction)(const void *source, void *dest);

typedef void (*VertexCopyFunction)(const void *input, size_t stride, size_t count, void *output);

namespace gl
{

typedef std::set<GLenum> FormatSet;

bool IsValidInternalFormat(GLenum internalFormat, const Extensions &extensions, GLuint clientVersion);
bool IsValidFormat(GLenum format, const Extensions &extensions, GLuint clientVersion);
bool IsValidType(GLenum type, const Extensions &extensions, GLuint clientVersion);

bool IsValidFormatCombination(GLenum internalFormat, GLenum format, GLenum type, const Extensions &extensions, GLuint clientVersion);
bool IsValidCopyTexImageCombination(GLenum textureInternalFormat, GLenum frameBufferInternalFormat, GLuint readBufferHandle, GLuint clientVersion);

bool IsSizedInternalFormat(GLenum internalFormat);
GLenum GetSizedInternalFormat(GLenum format, GLenum type);

GLuint GetPixelBytes(GLenum internalFormat);
GLuint GetAlphaBits(GLenum internalFormat);
GLuint GetRedBits(GLenum internalFormat);
GLuint GetGreenBits(GLenum internalFormat);
GLuint GetBlueBits(GLenum internalFormat);
GLuint GetLuminanceBits(GLenum internalFormat);
GLuint GetDepthBits(GLenum internalFormat);
GLuint GetStencilBits(GLenum internalFormat);

GLuint GetTypeBytes(GLenum type);
bool IsSpecialInterpretationType(GLenum type);
bool IsFloatOrFixedComponentType(GLenum type);

GLenum GetFormat(GLenum internalFormat);
GLenum GetType(GLenum internalFormat);

GLenum GetComponentType(GLenum internalFormat);
GLuint GetComponentCount(GLenum internalFormat);
GLenum GetColorEncoding(GLenum internalFormat);

GLuint GetRowPitch(GLenum internalFormat, GLenum type, GLsizei width, GLint alignment);
GLuint GetDepthPitch(GLenum internalFormat, GLenum type, GLsizei width, GLsizei height, GLint alignment);
GLuint GetBlockSize(GLenum internalFormat, GLenum type, GLsizei width, GLsizei height);

bool IsFormatCompressed(GLenum internalFormat);
GLuint GetCompressedBlockWidth(GLenum internalFormat);
GLuint GetCompressedBlockHeight(GLenum internalFormat);

const FormatSet &GetAllSizedInternalFormats();

ColorWriteFunction GetColorWriteFunction(GLenum format, GLenum type);

}

#endif LIBGLESV2_FORMATUTILS_H_
