/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLSHADERPRECISIONFORMAT_H_
#define WEBGLSHADERPRECISIONFORMAT_H_

#include "WebGLObjectModel.h"

namespace mozilla {

class WebGLBuffer;

class WebGLShaderPrecisionFormat MOZ_FINAL
    : public nsISupports
    , public WebGLContextBoundObject
{
public:
    WebGLShaderPrecisionFormat(WebGLContext *context, WebGLint rangeMin, WebGLint rangeMax, WebGLint precision) :
        WebGLContextBoundObject(context),
        mRangeMin(rangeMin),
        mRangeMax(rangeMax),
        mPrecision(precision)
    {
    }

    JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> scope);

    NS_DECL_ISUPPORTS

    // WebIDL WebGLShaderPrecisionFormat API
    WebGLint RangeMin() const {
        return mRangeMin;
    }
    WebGLint RangeMax() const {
        return mRangeMax;
    }
    WebGLint Precision() const {
        return mPrecision;
    }

protected:
    WebGLint mRangeMin;
    WebGLint mRangeMax;
    WebGLint mPrecision;
};

} // namespace mozilla

#endif
