/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "WebGLContext.h"
#include "WebGLQuery.h"

namespace mozilla {

WebGLExtensionDisjointTimerQuery::WebGLExtensionDisjointTimerQuery(
    WebGLContext* webgl)
    : WebGLExtensionBase(webgl) {
  MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
}

WebGLExtensionDisjointTimerQuery::~WebGLExtensionDisjointTimerQuery() {}

already_AddRefed<WebGLQuery> WebGLExtensionDisjointTimerQuery::CreateQueryEXT()
    const {
  if (mIsLost) return nullptr;
  const WebGLContext::FuncScope funcScope(*mContext, "createQueryEXT");
  MOZ_ASSERT(!mContext->IsContextLost());

  return mContext->CreateQuery();
}

void WebGLExtensionDisjointTimerQuery::DeleteQueryEXT(WebGLQuery* query) const {
  if (mIsLost || !mContext) return;
  const WebGLContext::FuncScope funcScope(*mContext, "deleteQueryEXT");
  MOZ_ASSERT(!mContext->IsContextLost());

  mContext->DeleteQuery(query);
}

bool WebGLExtensionDisjointTimerQuery::IsQueryEXT(
    const WebGLQuery* query) const {
  if (mIsLost || !mContext) return false;
  const WebGLContext::FuncScope funcScope(*mContext, "isQueryEXT");
  MOZ_ASSERT(!mContext->IsContextLost());

  return mContext->IsQuery(query);
}

void WebGLExtensionDisjointTimerQuery::BeginQueryEXT(GLenum target,
                                                     WebGLQuery& query) const {
  if (mIsLost || !mContext) return;
  const WebGLContext::FuncScope funcScope(*mContext, "beginQueryEXT");
  MOZ_ASSERT(!mContext->IsContextLost());

  mContext->BeginQuery(target, query);
}

void WebGLExtensionDisjointTimerQuery::EndQueryEXT(GLenum target) const {
  if (mIsLost || !mContext) return;
  const WebGLContext::FuncScope funcScope(*mContext, "endQueryEXT");
  MOZ_ASSERT(!mContext->IsContextLost());

  mContext->EndQuery(target);
}

void WebGLExtensionDisjointTimerQuery::QueryCounterEXT(WebGLQuery& query,
                                                       GLenum target) const {
  if (mIsLost || !mContext) return;
  const WebGLContext::FuncScope funcScope(*mContext, "queryCounterEXT");
  MOZ_ASSERT(!mContext->IsContextLost());

  if (!mContext->ValidateObject("query", query)) return;

  query.QueryCounter(target);
}

MaybeWebGLVariant WebGLExtensionDisjointTimerQuery::GetQueryEXT(
    GLenum target, GLenum pname) const {
  if (mIsLost || !mContext) return {};
  const WebGLContext::FuncScope funcScope(*mContext, "getQueryEXT");
  MOZ_ASSERT(!mContext->IsContextLost());

  return mContext->GetQuery(target, pname);
}

MaybeWebGLVariant WebGLExtensionDisjointTimerQuery::GetQueryObjectEXT(
    const WebGLQuery& query, GLenum pname) const {
  if (mIsLost || !mContext) return {};
  const WebGLContext::FuncScope funcScope(*mContext, "getQueryObjectEXT");
  MOZ_ASSERT(!mContext->IsContextLost());

  return mContext->GetQueryParameter(query, pname);
}

bool WebGLExtensionDisjointTimerQuery::IsSupported(const WebGLContext* webgl) {
  gl::GLContext* gl = webgl->GL();
  return gl->IsSupported(gl::GLFeature::query_objects) &&
         gl->IsSupported(gl::GLFeature::get_query_object_i64v) &&
         gl->IsSupported(
             gl::GLFeature::query_counter);  // provides GL_TIMESTAMP
}

}  // namespace mozilla
