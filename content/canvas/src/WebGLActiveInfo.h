/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLACTIVEINFO_H_
#define WEBGLACTIVEINFO_H_

#include "WebGLObjectModel.h"
#include "nsString.h"
#include "js/TypeDecls.h"

namespace mozilla {

class WebGLActiveInfo MOZ_FINAL
{
public:
    WebGLActiveInfo(GLint size, GLenum type, const nsACString& name) :
        mSize(size),
        mType(type),
        mName(NS_ConvertASCIItoUTF16(name))
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

    JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> scope);

   NS_INLINE_DECL_REFCOUNTING(WebGLActiveInfo)

protected:
    GLint mSize;
    GLenum mType;
    nsString mName;
};

} // namespace mozilla

#endif
