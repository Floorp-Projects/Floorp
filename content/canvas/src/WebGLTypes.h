/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLTYPES_H_
#define WEBGLTYPES_H_

// Manual reflection of WebIDL typedefs that are different from the
// corresponding OpenGL typedefs.
typedef int64_t WebGLsizeiptr;
typedef int64_t WebGLintptr;
typedef bool WebGLboolean;

namespace mozilla {

enum FakeBlackStatus { DoNotNeedFakeBlack, DoNeedFakeBlack, DontKnowIfNeedFakeBlack };

struct VertexAttrib0Status {
    enum { Default, EmulatedUninitializedArray, EmulatedInitializedArray };
};

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
