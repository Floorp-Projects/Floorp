/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "gfxPrefs.h"
#include "GLContext.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "WebGLContext.h"
#include "WebGLQuery.h"

namespace mozilla {

WebGLExtensionDisjointTimerQuery::WebGLExtensionDisjointTimerQuery(WebGLContext* webgl)
  : WebGLExtensionBase(webgl)
{
    MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
}

WebGLExtensionDisjointTimerQuery::~WebGLExtensionDisjointTimerQuery()
{
}

already_AddRefed<WebGLQuery>
WebGLExtensionDisjointTimerQuery::CreateQueryEXT() const
{
    const char funcName[] = "createQueryEXT";
    if (mIsLost)
        return nullptr;

    return mContext->CreateQuery(funcName);
}

void
WebGLExtensionDisjointTimerQuery::DeleteQueryEXT(WebGLQuery* query) const
{
    const char funcName[] = "deleteQueryEXT";
    if (mIsLost)
        return;

    mContext->DeleteQuery(query, funcName);
}

bool
WebGLExtensionDisjointTimerQuery::IsQueryEXT(const WebGLQuery* query) const
{
    const char funcName[] = "isQueryEXT";
    if (mIsLost)
        return false;

    return mContext->IsQuery(query, funcName);
}

void
WebGLExtensionDisjointTimerQuery::BeginQueryEXT(GLenum target, WebGLQuery& query) const
{
    const char funcName[] = "beginQueryEXT";
    if (mIsLost)
        return;

    mContext->BeginQuery(target, query, funcName);
}

void
WebGLExtensionDisjointTimerQuery::EndQueryEXT(GLenum target) const
{
    const char funcName[] = "endQueryEXT";
    if (mIsLost)
        return;

    mContext->EndQuery(target, funcName);
}

void
WebGLExtensionDisjointTimerQuery::QueryCounterEXT(WebGLQuery& query, GLenum target) const
{
    const char funcName[] = "queryCounterEXT";
    if (mIsLost)
        return;

    if (!mContext->ValidateObject(funcName, query))
        return;

    query.QueryCounter(funcName, target);
}

void
WebGLExtensionDisjointTimerQuery::GetQueryEXT(JSContext* cx, GLenum target, GLenum pname,
                                              JS::MutableHandleValue retval) const
{
    const char funcName[] = "getQueryEXT";
    retval.setNull();
    if (mIsLost)
        return;

    mContext->GetQuery(cx, target, pname, retval, funcName);
}

void
WebGLExtensionDisjointTimerQuery::GetQueryObjectEXT(JSContext* cx,
                                                    const WebGLQuery& query, GLenum pname,
                                                    JS::MutableHandleValue retval) const
{
    const char funcName[] = "getQueryObjectEXT";
    retval.setNull();
    if (mIsLost)
        return;

    mContext->GetQueryParameter(cx, query, pname, retval, funcName);
}

bool
WebGLExtensionDisjointTimerQuery::IsSupported(const WebGLContext* webgl)
{
    gl::GLContext* gl = webgl->GL();
    return gl->IsSupported(gl::GLFeature::query_objects) &&
           gl->IsSupported(gl::GLFeature::get_query_object_i64v) &&
           gl->IsSupported(gl::GLFeature::query_counter); // provides GL_TIMESTAMP
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionDisjointTimerQuery, EXT_disjoint_timer_query)

} // namespace mozilla
