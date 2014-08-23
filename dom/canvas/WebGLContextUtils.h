/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCONTEXTUTILS_H_
#define WEBGLCONTEXTUTILS_H_

#include "WebGLContext.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {

bool IsGLDepthFormat(GLenum webGLFormat);
bool IsGLDepthStencilFormat(GLenum webGLFormat);
bool FormatHasAlpha(GLenum webGLFormat);
void DriverFormatsFromFormatAndType(gl::GLContext* gl, GLenum webGLFormat, GLenum webGLType,
                                    GLenum* out_driverInternalFormat, GLenum* out_driverFormat);
GLenum DriverTypeFromType(gl::GLContext* gl, GLenum webGLType);

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
    { }

    GLComponents(GLenum format);

    // Returns true iff other has all (or more) of
    // the components present in this GLComponents
    bool IsSubsetOf(const GLComponents& other) const;
};

template <typename WebGLObjectType>
JS::Value
WebGLContext::WebGLObjectAsJSValue(JSContext *cx, const WebGLObjectType *object, ErrorResult& rv) const
{
    if (!object) {
        return JS::NullValue();
    }
    MOZ_ASSERT(this == object->Context());
    JS::Rooted<JS::Value> v(cx);
    JS::Rooted<JSObject*> wrapper(cx, GetWrapper());
    JSAutoCompartment ac(cx, wrapper);
    if (!dom::WrapNewBindingObject(cx, const_cast<WebGLObjectType*>(object), &v)) {
        rv.Throw(NS_ERROR_FAILURE);
        return JS::NullValue();
    }
    return v;
}

template <typename WebGLObjectType>
JSObject*
WebGLContext::WebGLObjectAsJSObject(JSContext *cx, const WebGLObjectType *object, ErrorResult& rv) const
{
    JS::Value v = WebGLObjectAsJSValue(cx, object, rv);
    if (v.isNull()) {
        return nullptr;
    }
    return &v.toObject();
}

} // namespace mozilla

#endif // WEBGLCONTEXTUTILS_H_
