/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_SHADER_PRECISION_FORMAT_H_
#define WEBGL_SHADER_PRECISION_FORMAT_H_

#include "WebGLObjectModel.h"

namespace mozilla {

class WebGLShaderPrecisionFormat final
    : public WebGLContextBoundObject
{
public:
    WebGLShaderPrecisionFormat(WebGLContext* context, GLint rangeMin,
                               GLint rangeMax, GLint precision)
        : WebGLContextBoundObject(context)
        , mRangeMin(rangeMin)
        , mRangeMax(rangeMax)
        , mPrecision(precision)
    { }

    bool WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto, JS::MutableHandle<JSObject*> reflector);

    // WebIDL WebGLShaderPrecisionFormat API
    GLint RangeMin() const {
        return mRangeMin;
    }

    GLint RangeMax() const {
        return mRangeMax;
    }

    GLint Precision() const {
        return mPrecision;
    }

    NS_INLINE_DECL_REFCOUNTING(WebGLShaderPrecisionFormat)

private:
    // Private destructor, to discourage deletion outside of Release():
    ~WebGLShaderPrecisionFormat() { }

    GLint mRangeMin;
    GLint mRangeMax;
    GLint mPrecision;
};

} // namespace mozilla

#endif // WEBGL_SHADER_PRECISION_FORMAT_H_
