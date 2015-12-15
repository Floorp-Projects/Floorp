/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_QUERY_H_
#define WEBGL_QUERY_H_

#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"

#include "WebGLObjectModel.h"

namespace mozilla {

class WebGLQuery final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLQuery>
    , public LinkedListElement<WebGLQuery>
    , public WebGLContextBoundObject
{
public:
    explicit WebGLQuery(WebGLContext* webgl);

    bool IsActive() const;

    bool HasEverBeenActive() const {
        return mType != 0;
    }

    // WebGLRefCountedObject
    void Delete();

    // nsWrapperCache
    WebGLContext* GetParentObject() const {
        return mContext;
    }

    // NS
    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLQuery)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLQuery)


private:
    ~WebGLQuery() {
        DeleteOnce();
    };

    GLuint mGLName;
    GLenum mType;

    friend class WebGL2Context;
};

} // namespace mozilla

#endif // WEBGL_QUERY_H_
