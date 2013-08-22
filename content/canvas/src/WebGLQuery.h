/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLQUERY_H_
#define WEBGLQUERY_H_

#include "WebGLObjectModel.h"
#include "WebGLContext.h"

#include "nsWrapperCache.h"

#include "mozilla/LinkedList.h"

namespace mozilla {

class WebGLQuery MOZ_FINAL
    : public nsISupports
    , public WebGLRefCountedObject<WebGLQuery>
    , public LinkedListElement<WebGLQuery>
    , public WebGLContextBoundObject
    , public nsWrapperCache
{
// -----------------------------------------------------------------------------
// PUBLIC
public:

    // -------------------------------------------------------------------------
    // CONSTRUCTOR & DESTRUCTOR

    WebGLQuery(WebGLContext *context);

    ~WebGLQuery() {
        DeleteOnce();
    };


    // -------------------------------------------------------------------------
    // MEMBER FUNCTIONS

    bool IsActive() const
    {
        return mContext->GetActiveQueryByTarget(mType) == this;
    }

    bool HasEverBeenActive()
    {
        return mType != 0;
    }


    // -------------------------------------------------------------------------
    // IMPLEMENT WebGLRefCountedObject and WebGLContextBoundObject

    void Delete();

    WebGLContext* GetParentObject() const
    {
        return Context();
    }


    // -------------------------------------------------------------------------
    // IMPLEMENT NS
    virtual JSObject* WrapObject(JSContext *cx,
                                 JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WebGLQuery)


// -----------------------------------------------------------------------------
// PRIVATE
private:

    // -------------------------------------------------------------------------
    // MEMBERS
    WebGLuint mGLName;
    WebGLenum mType;

    // -------------------------------------------------------------------------
    // FRIENDSHIPS
    friend class WebGLContext;
};

} // namespace mozilla

#endif
