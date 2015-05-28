/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_CONTEXT_UTILS_H_
#define WEBGL_CONTEXT_UTILS_H_

#include "WebGLContext.h"
#include "WebGLStrongTypes.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {

bool IsGLDepthFormat(TexInternalFormat webGLFormat);
bool IsGLDepthStencilFormat(TexInternalFormat webGLFormat);
bool FormatHasAlpha(TexInternalFormat webGLFormat);

void
DriverFormatsFromEffectiveInternalFormat(gl::GLContext* gl,
                                         TexInternalFormat internalformat,
                                         GLenum* const out_driverInternalFormat,
                                         GLenum* const out_driverFormat,
                                         GLenum* const out_driverType);
TexInternalFormat
EffectiveInternalFormatFromInternalFormatAndType(TexInternalFormat internalformat,
                                                 TexType type);
TexInternalFormat
EffectiveInternalFormatFromUnsizedInternalFormatAndType(TexInternalFormat internalformat,
                                                        TexType type);
void
UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(TexInternalFormat effectiveinternalformat,
                                                        TexInternalFormat* const out_internalformat,
                                                        TexType* const out_type);
TexType TypeFromInternalFormat(TexInternalFormat internalformat);

TexInternalFormat
UnsizedInternalFormatFromInternalFormat(TexInternalFormat internalformat);

void SetLegacyTextureSwizzle(gl::GLContext* gl, GLenum target, GLenum internalformat);

size_t GetBitsPerTexel(TexInternalFormat effectiveinternalformat);

// For use with the different texture calls, i.e.
//   TexImage2D, CopyTex[Sub]Image2D, ...
// that take a "target" parameter. This parameter is not always the same as
// the texture binding location, like GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP.
// For example, cube maps would pass GL_TEXTURE_CUBE_MAP_[POS|NEG]_[X|Y|Z]
// instead of just GL_TEXTURE_CUBE_MAP.
//
// This function converts the texture image target to the texture target a.k.a.
// binding location. The returned binding location can be used to check that
// the currently bound texture is appropriate for this texImageTarget.
//
// Returns GL_NONE if passed an invalid texture image target
TexTarget TexImageTargetToTexTarget(TexImageTarget texImageTarget);

struct GLComponents
{
    unsigned char mComponents;

    enum Components {
        Red     = (1 << 0),
        Green   = (1 << 1),
        Blue    = (1 << 2),
        Alpha   = (1 << 3),
        Stencil = (1 << 4),
        Depth   = (1 << 5),
    };

    GLComponents()
        : mComponents(0)
    {}

    explicit GLComponents(TexInternalFormat format);

    // Returns true iff other has all (or more) of
    // the components present in this GLComponents
    bool IsSubsetOf(const GLComponents& other) const;
};

template <typename WebGLObjectType>
JS::Value
WebGLContext::WebGLObjectAsJSValue(JSContext* cx, const WebGLObjectType* object,
                                   ErrorResult& rv) const
{
    if (!object)
        return JS::NullValue();

    MOZ_ASSERT(this == object->Context());
    JS::Rooted<JS::Value> v(cx);
    JS::Rooted<JSObject*> wrapper(cx, GetWrapper());
    JSAutoCompartment ac(cx, wrapper);
    if (!dom::GetOrCreateDOMReflector(cx, const_cast<WebGLObjectType*>(object), &v)) {
        rv.Throw(NS_ERROR_FAILURE);
        return JS::NullValue();
    }
    return v;
}

template <typename WebGLObjectType>
JSObject*
WebGLContext::WebGLObjectAsJSObject(JSContext* cx,
                                    const WebGLObjectType* object,
                                    ErrorResult& rv) const
{
    JS::Value v = WebGLObjectAsJSValue(cx, object, rv);
    if (v.isNull())
        return nullptr;

    return &v.toObject();
}

/**
 * Return the displayable name for the texture function that is the
 * source for validation.
 */
const char* InfoFrom(WebGLTexImageFunc func, WebGLTexDimensions dims);

} // namespace mozilla

#endif // WEBGL_CONTEXT_UTILS_H_
