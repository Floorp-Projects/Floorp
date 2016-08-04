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
#include "WebGLTimerQuery.h"

namespace mozilla {

WebGLExtensionDisjointTimerQuery::WebGLExtensionDisjointTimerQuery(WebGLContext* webgl)
  : WebGLExtensionBase(webgl)
  , mActiveQuery(nullptr)
{
  MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
}

WebGLExtensionDisjointTimerQuery::~WebGLExtensionDisjointTimerQuery()
{
}

already_AddRefed<WebGLTimerQuery>
WebGLExtensionDisjointTimerQuery::CreateQueryEXT()
{
  if (mIsLost)
    return nullptr;

  RefPtr<WebGLTimerQuery> query = WebGLTimerQuery::Create(mContext);
  return query.forget();
}

void
WebGLExtensionDisjointTimerQuery::DeleteQueryEXT(WebGLTimerQuery* query)
{
  if (mIsLost)
    return;

  if (!mContext->ValidateObject("deleteQueryEXT", query))
    return;

  query->RequestDelete();
}

bool
WebGLExtensionDisjointTimerQuery::IsQueryEXT(WebGLTimerQuery* query)
{
  if (!query)
    return false;

  if (!mContext->ValidateObjectAllowDeleted("isQueryEXT", query))
    return false;

  if (query->IsDeleted())
    return false;

  return true;
}

void
WebGLExtensionDisjointTimerQuery::BeginQueryEXT(GLenum target,
                                                WebGLTimerQuery* query)
{
  if (mIsLost)
    return;

  if (!mContext->ValidateObject("beginQueryEXT", query))
    return;

  if (query->HasEverBeenBound() && query->Target() != target) {
    mContext->ErrorInvalidOperation("beginQueryEXT: Query is already bound"
                                    " to a different target.");
    return;
  }

  if (target != LOCAL_GL_TIME_ELAPSED_EXT) {
    mContext->ErrorInvalidEnumInfo("beginQueryEXT: Can only begin on target"
                                   " TIME_ELAPSED_EXT.", target);
    return;
  }

  if (mActiveQuery) {
    mContext->ErrorInvalidOperation("beginQueryEXT: A query is already"
                                    " active.");
    return;
  }

  mContext->MakeContextCurrent();
  gl::GLContext* gl = mContext->GL();
  gl->fBeginQuery(target, query->mGLName);
  query->mTarget = LOCAL_GL_TIME_ELAPSED_EXT;
  mActiveQuery = query;
}

void
WebGLExtensionDisjointTimerQuery::EndQueryEXT(GLenum target)
{
  if (mIsLost)
    return;

  if (target != LOCAL_GL_TIME_ELAPSED_EXT) {
    mContext->ErrorInvalidEnumInfo("endQueryEXT: Can only end on"
                                   " TIME_ELAPSED_EXT.", target);
    return;
  }

  if (!mActiveQuery) {
    mContext->ErrorInvalidOperation("endQueryEXT: A query is not active.");
    return;
  }

  mContext->MakeContextCurrent();
  mContext->GL()->fEndQuery(target);
  mActiveQuery->QueueAvailablity();
  mActiveQuery = nullptr;
}

void
WebGLExtensionDisjointTimerQuery::QueryCounterEXT(WebGLTimerQuery* query,
                                                  GLenum target)
{
  if (mIsLost)
    return;

  if (!mContext->ValidateObject("queryCounterEXT", query))
    return;

  if (target != LOCAL_GL_TIMESTAMP_EXT) {
    mContext->ErrorInvalidEnumInfo("queryCounterEXT: requires"
                                   " TIMESTAMP_EXT.", target);
    return;
  }

  mContext->MakeContextCurrent();
  mContext->GL()->fQueryCounter(query->mGLName, target);
  query->mTarget = LOCAL_GL_TIMESTAMP_EXT;
  query->QueueAvailablity();
}

void
WebGLExtensionDisjointTimerQuery::GetQueryEXT(JSContext* cx, GLenum target,
                                              GLenum pname,
                                              JS::MutableHandle<JS::Value> retval)
{
  if (mIsLost)
    return;

  mContext->MakeContextCurrent();
  switch (pname) {
  case LOCAL_GL_CURRENT_QUERY_EXT: {
    if (target != LOCAL_GL_TIME_ELAPSED_EXT) {
      mContext->ErrorInvalidEnumInfo("getQueryEXT: Invalid query target.",
                                     target);
      return;
    }
    if (mActiveQuery) {
      JS::Rooted<JS::Value> v(cx);
      dom::GetOrCreateDOMReflector(cx, mActiveQuery.get(), &v);
      retval.set(v);
    } else {
      retval.set(JS::NullValue());
    }
    break;
  }
  case LOCAL_GL_QUERY_COUNTER_BITS_EXT: {
    if (target != LOCAL_GL_TIME_ELAPSED_EXT &&
        target != LOCAL_GL_TIMESTAMP_EXT) {
      mContext->ErrorInvalidEnumInfo("getQueryEXT: Invalid query target.",
                                     target);
      return;
    }
    GLint bits = 0;
    if (mContext->HasTimestampBits()) {
      mContext->GL()->fGetQueryiv(target, pname, &bits);
    }
    retval.set(JS::Int32Value(int32_t(bits)));
    break;
  }
  default:
    mContext->ErrorInvalidEnumInfo("getQueryEXT: Invalid query property.",
                                   pname);
    break;
  }
}

void
WebGLExtensionDisjointTimerQuery::GetQueryObjectEXT(JSContext* cx,
                                                    WebGLTimerQuery* query,
                                                    GLenum pname,
                                                    JS::MutableHandle<JS::Value> retval)
{
  if (mIsLost)
    return;

  if (!mContext->ValidateObject("getQueryObjectEXT", query))
    return;

  if (query == mActiveQuery.get()) {
    mContext->ErrorInvalidOperation("getQueryObjectEXT: Query must not be"
                                    " active.");
  }

  mContext->MakeContextCurrent();
  // XXX: Note that the query result *may change* within the same task!
  // This does not follow the specification, which states that all calls
  // checking query results must return the same value until the event loop
  // is empty.
  switch (pname) {
  case LOCAL_GL_QUERY_RESULT_EXT: {
    GLuint64 result = 0;
    mContext->GL()->fGetQueryObjectui64v(query->mGLName,
                                         LOCAL_GL_QUERY_RESULT_EXT,
                                         &result);
    retval.set(JS::NumberValue(result));
    break;
  }
  case LOCAL_GL_QUERY_RESULT_AVAILABLE_EXT: {
    GLuint avail = 0;
    mContext->GL()->fGetQueryObjectuiv(query->mGLName,
                                       LOCAL_GL_QUERY_RESULT_AVAILABLE_EXT,
                                       &avail);
    bool canBeAvailable = query->CanBeAvailable() || gfxPrefs::WebGLImmediateQueries();
    retval.set(JS::BooleanValue(bool(avail) && canBeAvailable));
    break;
  }
  default:
    mContext->ErrorInvalidEnumInfo("getQueryObjectEXT: Invalid query"
                                   " property.", pname);
    break;
  }
}

bool
WebGLExtensionDisjointTimerQuery::IsSupported(const WebGLContext* webgl)
{
  webgl->MakeContextCurrent();
  gl::GLContext* gl = webgl->GL();
  return gl->IsSupported(gl::GLFeature::query_objects) &&
         gl->IsSupported(gl::GLFeature::get_query_object_i64v) &&
         gl->IsSupported(gl::GLFeature::query_counter); // provides GL_TIMESTAMP
}

void
WebGLExtensionDisjointTimerQuery::OnMarkLost()
{
  mActiveQuery = nullptr;
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionDisjointTimerQuery, EXT_disjoint_timer_query)

} // namespace mozilla
