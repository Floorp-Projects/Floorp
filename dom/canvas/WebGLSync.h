/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLSYNC_H_
#define WEBGLSYNC_H_

#include "WebGLObjectModel.h"

#include "nsWrapperCache.h"

#include "mozilla/LinkedList.h"

namespace mozilla {

class WebGLSync MOZ_FINAL
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLSync>
    , public LinkedListElement<WebGLSync>
    , public WebGLContextBoundObject
{
    friend class WebGL2Context;

public:

    WebGLSync(WebGLContext* context);

    void Delete();
    WebGLContext* GetParentObject() const;

    // -------------------------------------------------------------------------
    // IMPLEMENT NS
    virtual JSObject* WrapObject(JSContext* cx) MOZ_OVERRIDE;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLSync)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLSync)

private:

    ~WebGLSync();
};

} // namespace mozilla

#endif // !WEBGLSYNC_H_
