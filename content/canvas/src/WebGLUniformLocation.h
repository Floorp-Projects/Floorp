/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLUNIFORMLOCATION_H_
#define WEBGLUNIFORMLOCATION_H_

#include "WebGLObjectModel.h"
#include "WebGLUniformInfo.h"

namespace mozilla {

class WebGLProgram;

class WebGLUniformLocation MOZ_FINAL
    : public WebGLContextBoundObject
{
public:
    WebGLUniformLocation(WebGLContext *context, WebGLProgram *program, GLint location, const WebGLUniformInfo& info);

    ~WebGLUniformLocation() {
    }

    // needed for certain helper functions like ValidateObject.
    // WebGLUniformLocation's can't be 'Deleted' in the WebGL sense.
    bool IsDeleted() const { return false; }

    const WebGLUniformInfo &Info() const { return mInfo; }

    WebGLProgram *Program() const { return mProgram; }
    GLint Location() const { return mLocation; }
    uint32_t ProgramGeneration() const { return mProgramGeneration; }
    int ElementSize() const { return mElementSize; }

    JSObject* WrapObject(JSContext *cx);

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLUniformLocation)
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(WebGLUniformLocation)

protected:
    // nsRefPtr, not WebGLRefPtr, so that we don't prevent the program from being explicitly deleted.
    // we just want to avoid having a dangling pointer.
    nsRefPtr<WebGLProgram> mProgram;

    uint32_t mProgramGeneration;
    GLint mLocation;
    WebGLUniformInfo mInfo;
    int mElementSize;
    friend class WebGLProgram;
};

} // namespace mozilla

#endif
