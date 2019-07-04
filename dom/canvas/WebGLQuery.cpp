/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLQuery.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "nsContentUtils.h"
#include "WebGLContext.h"

namespace mozilla {

////

static GLuint GenQuery(gl::GLContext* gl) {
  GLuint ret = 0;
  gl->fGenQueries(1, &ret);
  return ret;
}

WebGLQuery::WebGLQuery(WebGLContext* webgl)
    : WebGLRefCountedObject(webgl),
      mGLName(GenQuery(mContext->gl)),
      mTarget(0),
      mActiveSlot(nullptr) {
  mContext->mQueries.insertBack(this);
}

void WebGLQuery::Delete() {
  mContext->gl->fDeleteQueries(1, &mGLName);
  LinkedListElement<WebGLQuery>::removeFrom(mContext->mQueries);
}

////

static GLenum TargetForDriver(const gl::GLContext* gl, GLenum target) {
  switch (target) {
    case LOCAL_GL_ANY_SAMPLES_PASSED:
    case LOCAL_GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
      break;

    default:
      return target;
  }

  if (gl->IsSupported(gl::GLFeature::occlusion_query_boolean)) return target;

  if (gl->IsSupported(gl::GLFeature::occlusion_query2))
    return LOCAL_GL_ANY_SAMPLES_PASSED;

  return LOCAL_GL_SAMPLES_PASSED;
}

void WebGLQuery::BeginQuery(GLenum target, WebGLRefPtr<WebGLQuery>& slot) {
  if (mTarget && target != mTarget) {
    mContext->ErrorInvalidOperation("Queries cannot change targets.");
    return;
  }

  ////

  mTarget = target;
  mActiveSlot = &slot;
  *mActiveSlot = this;

  ////

  const auto& gl = mContext->gl;

  const auto driverTarget = TargetForDriver(gl, mTarget);
  gl->fBeginQuery(driverTarget, mGLName);
}

void WebGLQuery::EndQuery() {
  *mActiveSlot = nullptr;
  mActiveSlot = nullptr;
  mCanBeAvailable = false;

  ////

  const auto& gl = mContext->gl;

  const auto driverTarget = TargetForDriver(gl, mTarget);
  gl->fEndQuery(driverTarget);

  ////

  const auto& availRunnable = mContext->EnsureAvailabilityRunnable();
  availRunnable->mQueries.push_back(this);
}

void WebGLQuery::GetQueryParameter(GLenum pname,
                                   JS::MutableHandleValue retval) const {
  switch (pname) {
    case LOCAL_GL_QUERY_RESULT_AVAILABLE:
    case LOCAL_GL_QUERY_RESULT:
      break;

    default:
      mContext->ErrorInvalidEnumInfo("pname", pname);
      return;
  }

  if (!mTarget) {
    mContext->ErrorInvalidOperation("Query has never been active.");
    return;
  }

  if (mActiveSlot)
    return mContext->ErrorInvalidOperation("Query is still active.");

  // End of validation
  ////

  // We must usually wait for an event loop before the query can be available.
  const bool canBeAvailable =
      (mCanBeAvailable || StaticPrefs::webgl_allow_immediate_queries());
  if (!canBeAvailable) {
    if (pname == LOCAL_GL_QUERY_RESULT_AVAILABLE) {
      retval.set(JS::BooleanValue(false));
    }
    return;
  }

  const auto& gl = mContext->gl;

  uint64_t val = 0;
  switch (pname) {
    case LOCAL_GL_QUERY_RESULT_AVAILABLE:
      gl->fGetQueryObjectuiv(mGLName, pname, (GLuint*)&val);
      retval.set(JS::BooleanValue(bool(val)));
      return;

    case LOCAL_GL_QUERY_RESULT:
      switch (mTarget) {
        case LOCAL_GL_TIME_ELAPSED_EXT:
        case LOCAL_GL_TIMESTAMP_EXT:
          if (mContext->Has64BitTimestamps()) {
            gl->fGetQueryObjectui64v(mGLName, pname, &val);
            break;
          }
          MOZ_FALLTHROUGH;

        default:
          gl->fGetQueryObjectuiv(mGLName, LOCAL_GL_QUERY_RESULT, (GLuint*)&val);
          break;
      }

      switch (mTarget) {
        case LOCAL_GL_ANY_SAMPLES_PASSED:
        case LOCAL_GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        case LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        case LOCAL_GL_TIME_ELAPSED_EXT:
        case LOCAL_GL_TIMESTAMP_EXT:
          retval.set(JS::NumberValue(val));
          break;

        default:
          MOZ_CRASH("Bad `mTarget`.");
      }
      return;

    default:
      MOZ_CRASH("Bad `pname`.");
  }
}

bool WebGLQuery::IsQuery() const {
  MOZ_ASSERT(!IsDeleted());

  if (!mTarget) return false;

  return true;
}

void WebGLQuery::DeleteQuery() {
  MOZ_ASSERT(!IsDeleteRequested());

  if (mActiveSlot) {
    EndQuery();
  }

  RequestDelete();
}

void WebGLQuery::QueryCounter(GLenum target) {
  if (target != LOCAL_GL_TIMESTAMP_EXT) {
    mContext->ErrorInvalidEnum("`target` must be TIMESTAMP_EXT.");
    return;
  }

  if (mTarget && target != mTarget) {
    mContext->ErrorInvalidOperation("Queries cannot change targets.");
    return;
  }

  mTarget = target;
  mCanBeAvailable = false;

  const auto& gl = mContext->gl;
  gl->fQueryCounter(mGLName, mTarget);

  const auto& availRunnable = mContext->EnsureAvailabilityRunnable();
  availRunnable->mQueries.push_back(this);
}

////

JSObject* WebGLQuery::WrapObject(JSContext* cx,
                                 JS::Handle<JSObject*> givenProto) {
  return dom::WebGLQuery_Binding::Wrap(cx, this, givenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLQuery)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLQuery, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLQuery, Release)

}  // namespace mozilla
