/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLQuery.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "nsContentUtils.h"
#include "WebGLContext.h"

namespace mozilla {

JSObject*
WebGLQuery::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLQueryBinding::Wrap(cx, this, givenProto);
}

WebGLQuery::WebGLQuery(WebGLContext* webgl)
    : WebGLContextBoundObject(webgl)
    , mCanBeAvailable(false)
    , mGLName(0)
    , mType(0)
{
    mContext->mQueries.insertBack(this);

    mContext->MakeContextCurrent();
    mContext->gl->fGenQueries(1, &mGLName);
}

void
WebGLQuery::Delete()
{
    mContext->MakeContextCurrent();
    mContext->gl->fDeleteQueries(1, &mGLName);
    LinkedListElement<WebGLQuery>::removeFrom(mContext->mQueries);
}

bool
WebGLQuery::IsActive() const
{
    if (!HasEverBeenActive())
        return false;

    WebGLRefPtr<WebGLQuery>& targetSlot = mContext->GetQuerySlotByTarget(mType);

    return targetSlot.get() == this;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLQuery)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLQuery, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLQuery, Release)

} // namespace mozilla
