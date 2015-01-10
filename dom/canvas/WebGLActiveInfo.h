/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_ACTIVE_INFO_H_
#define WEBGL_ACTIVE_INFO_H_

#include "js/TypeDecls.h"
#include "nsString.h"
#include "WebGLObjectModel.h"

namespace mozilla {

class WebGLActiveInfo MOZ_FINAL
{
public:
    WebGLActiveInfo(GLint size, GLenum type, const nsACString& name)
        : mSize(size)
        , mType(type)
        , mName(NS_ConvertASCIItoUTF16(name))
    {}

    // WebIDL attributes

    GLint Size() const {
        return mSize;
    }

    GLenum Type() const {
        return mType;
    }

    void GetName(nsString& retval) const {
        retval = mName;
    }

    JSObject* WrapObject(JSContext* cx);

   NS_INLINE_DECL_REFCOUNTING(WebGLActiveInfo)

private:
    // Private destructor, to discourage deletion outside of Release():
    ~WebGLActiveInfo()
    {
    }

    GLint mSize;
    GLenum mType;
    nsString mName;
};

} // namespace mozilla

#endif // WEBGL_ACTIVE_INFO_H_
