/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLTYPES_H_
#define WEBGLTYPES_H_

#include "mozilla/TypedEnum.h"

// Most WebIDL typedefs are identical to their OpenGL counterparts.
#include "GLTypes.h"

// Manual reflection of WebIDL typedefs that are different from their
// OpenGL counterparts.
typedef int64_t WebGLsizeiptr;
typedef int64_t WebGLintptr;
typedef bool WebGLboolean;

namespace mozilla {

enum FakeBlackStatus { DoNotNeedFakeBlack, DoNeedFakeBlack, DontKnowIfNeedFakeBlack };

struct VertexAttrib0Status {
    enum { Default, EmulatedUninitializedArray, EmulatedInitializedArray };
};

/*
 * Enum to track the status of image data (renderbuffer or texture image) presence
 * and initialization.
 *
 * - NoImageData is the initial state before any image data is allocated.
 * - InitializedImageData is the state after image data is allocated and initialized.
 * - UninitializedImageData is an intermediate state where data is allocated but not
 *   initialized. It is the state that renderbuffers are in after a renderbufferStorage call,
 *   and it is the state that texture images are in after a texImage2D call with null data.
 */
MOZ_BEGIN_ENUM_CLASS(WebGLImageDataStatus, int)
    NoImageData,
    UninitializedImageData,
    InitializedImageData
MOZ_END_ENUM_CLASS(WebGLImageDataStatus)

namespace WebGLTexelConversions {

/*
 * The formats that may participate, either as source or destination formats,
 * in WebGL texture conversions. This includes:
 *  - all the formats accepted by WebGL.texImage2D, e.g. RGBA4444
 *  - additional formats provided by extensions, e.g. RGB32F
 *  - additional source formats, depending on browser details, used when uploading
 *    textures from DOM elements. See gfxImageSurface::Format().
 */
enum WebGLTexelFormat
{
    // dummy error code returned by GetWebGLTexelFormat in error cases,
    // after assertion failure (so this never happens in debug builds)
    BadFormat,
    // dummy pseudo-format meaning "use the other format".
    // For example, if SrcFormat=Auto and DstFormat=RGB8, then the source
    // is implicitly treated as being RGB8 itself.
    Auto,
    // 1-channel formats
    R8,
    A8,
    D16, // used for WEBGL_depth_texture extension
    D32, // used for WEBGL_depth_texture extension
    R32F, // used for OES_texture_float extension
    A32F, // used for OES_texture_float extension
    // 2-channel formats
    RA8,
    RA32F,
    D24S8, // used for WEBGL_depth_texture extension
    // 3-channel formats
    RGB8,
    BGRX8, // used for DOM elements. Source format only.
    RGB565,
    RGB32F, // used for OES_texture_float extension
    // 4-channel formats
    RGBA8,
    BGRA8, // used for DOM elements
    RGBA5551,
    RGBA4444,
    RGBA32F // used for OES_texture_float extension
};

} // end namespace WebGLTexelConversions

} // namespace mozilla

#endif
