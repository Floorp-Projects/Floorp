/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLTRANSFORMFEEDBACK_H_
#define WEBGLTRANSFORMFEEDBACK_H_

#include "WebGLBindableName.h"
#include "WebGLObjectModel.h"

#include "nsWrapperCache.h"

#include "mozilla/LinkedList.h"

namespace mozilla {

class WebGLTransformFeedback MOZ_FINAL
    : public WebGLBindableName<GLenum>
    , public nsWrapperCache
    , public WebGLRefCountedObject<WebGLTransformFeedback>
    , public LinkedListElement<WebGLTransformFeedback>
    , public WebGLContextBoundObject
{
    friend class WebGLContext;

public:

    WebGLTransformFeedback(WebGLContext* context);

    void Delete();
    WebGLContext* GetParentObject() const;

    // -------------------------------------------------------------------------
    // IMPLEMENT NS
    virtual JSObject* WrapObject(JSContext* cx) MOZ_OVERRIDE;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLTransformFeedback)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLTransformFeedback)

private:

    ~WebGLTransformFeedback();
};

}

#endif // !WEBGLTRANSFORMFEEDBACK_H_
