/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLQuery.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "mozilla/StaticPrefs_webgl.h"
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
    : WebGLContextBoundObject(webgl),
      mGLName(GenQuery(mContext->gl)),
      mTarget(0),
      mActiveSlot(nullptr) {}

WebGLQuery::~WebGLQuery() {
  if (!mContext) return;
  mContext->gl->fDeleteQueries(1, &mGLName);
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

void WebGLQuery::BeginQuery(GLenum target, RefPtr<WebGLQuery>& slot) {
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
}

Maybe<double> WebGLQuery::GetQueryParameter(GLenum pname) const {
  switch (pname) {
    case LOCAL_GL_QUERY_RESULT_AVAILABLE:
    case LOCAL_GL_QUERY_RESULT:
      break;

    default:
      mContext->ErrorInvalidEnumInfo("pname", pname);
      return Nothing();
  }

  if (!mTarget) {
    mContext->ErrorInvalidOperation("Query has never been active.");
    return Nothing();
  }

  if (mActiveSlot) {
    mContext->ErrorInvalidOperation("Query is still active.");
    return Nothing();
  }

  // End of validation
  ////

  const auto& gl = mContext->gl;

  uint64_t val = 0;
  switch (pname) {
    case LOCAL_GL_QUERY_RESULT_AVAILABLE:
      gl->fGetQueryObjectuiv(mGLName, pname, (GLuint*)&val);
      return Some(static_cast<bool>(val));

    case LOCAL_GL_QUERY_RESULT:
      switch (mTarget) {
        case LOCAL_GL_TIME_ELAPSED_EXT:
        case LOCAL_GL_TIMESTAMP_EXT:
          if (mContext->Has64BitTimestamps()) {
            gl->fGetQueryObjectui64v(mGLName, pname, &val);
            break;
          }
          [[fallthrough]];

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
          return Some(val);
      }
      MOZ_CRASH("Bad `mTarget`.");

    default:
      MOZ_CRASH("Bad `pname`.");
  }
}

void WebGLQuery::QueryCounter() {
  const GLenum target = LOCAL_GL_TIMESTAMP_EXT;
  if (mTarget && target != mTarget) {
    mContext->ErrorInvalidOperation("Queries cannot change targets.");
    return;
  }

  mTarget = target;
  mCanBeAvailable = false;

  const auto& gl = mContext->gl;
  gl->fQueryCounter(mGLName, mTarget);
}

}  // namespace mozilla
