/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLTEXTUREUPLOAD_H_
#define WEBGLTEXTUREUPLOAD_H_

#include "WebGLTypes.h"

namespace mozilla {
struct SurfaceFromElementResult;

namespace dom {
class Element;
class OffscreenCanvas;
}  // namespace dom

namespace webgl {

Maybe<TexUnpackBlobDesc> FromImageBitmap(GLenum target, Maybe<uvec3> size,
                                         const dom::ImageBitmap& imageBitmap,
                                         ErrorResult* const out_rv);

Maybe<TexUnpackBlobDesc> FromOffscreenCanvas(const ClientWebGLContext& webgl,
                                             GLenum target, Maybe<uvec3> size,
                                             const dom::OffscreenCanvas& canvas,
                                             ErrorResult* const out_error);

Maybe<TexUnpackBlobDesc> FromVideoFrame(const ClientWebGLContext& webgl,
                                        GLenum target, Maybe<uvec3> size,
                                        const dom::VideoFrame& videoFrame,
                                        ErrorResult* const out_error);

Maybe<TexUnpackBlobDesc> FromDomElem(const ClientWebGLContext& webgl,
                                     GLenum target, Maybe<uvec3> size,
                                     const dom::Element& elem,
                                     ErrorResult* const out_error);

Maybe<TexUnpackBlobDesc> FromSurfaceFromElementResult(
    const ClientWebGLContext& webgl, GLenum target, Maybe<uvec3> size,
    SurfaceFromElementResult& sfer, ErrorResult* const out_error);

}  // namespace webgl
}  // namespace mozilla

#endif
