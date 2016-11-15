/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_QUERY_H_
#define WEBGL_QUERY_H_

#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"

#include "WebGLObjectModel.h"
#include "nsThreadUtils.h"

namespace mozilla {

class WebGLQuery final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLQuery>
    , public LinkedListElement<WebGLQuery>
    , public WebGLContextBoundObject
{
    friend class AvailableRunnable;
    friend class WebGLRefCountedObject<WebGLQuery>;

public:
    const GLuint mGLName;
private:
    GLenum mTarget;
    WebGLRefPtr<WebGLQuery>* mActiveSlot;

    bool mCanBeAvailable; // Track whether the event loop has spun

    ////
public:
    GLenum Target() const { return mTarget; }
    bool IsActive() const { return bool(mActiveSlot); }

    ////

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLQuery)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLQuery)

    explicit WebGLQuery(WebGLContext* webgl);

private:
    ~WebGLQuery() {
        DeleteOnce();
    };

    // WebGLRefCountedObject
    void Delete();

public:
    WebGLContext* GetParentObject() const { return mContext; }
    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

    ////

    void BeginQuery(GLenum target, WebGLRefPtr<WebGLQuery>& slot);
    void DeleteQuery();
    void EndQuery();
    void GetQueryParameter(GLenum pname, JS::MutableHandleValue retval) const;
    bool IsQuery() const;
    void QueryCounter(const char* funcName, GLenum target);
};

} // namespace mozilla

#endif // WEBGL_QUERY_H_
