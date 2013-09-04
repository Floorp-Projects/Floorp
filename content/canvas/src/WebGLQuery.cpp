/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLQuery.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "nsContentUtils.h"

using namespace mozilla;

JSObject*
WebGLQuery::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope) {
    return dom::WebGLQueryBinding::Wrap(cx, scope, this);
}

WebGLQuery::WebGLQuery(WebGLContext* context)
    : WebGLContextBoundObject(context)
    , mGLName(0)
    , mType(0)
{
    SetIsDOMBinding();
    mContext->mQueries.insertBack(this);

    mContext->MakeContextCurrent();
    mContext->gl->fGenQueries(1, &mGLName);
}

void WebGLQuery::Delete() {
    mContext->MakeContextCurrent();
    mContext->gl->fDeleteQueries(1, &mGLName);
    LinkedListElement<WebGLQuery>::removeFrom(mContext->mQueries);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLQuery)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLQuery, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLQuery, Release)
