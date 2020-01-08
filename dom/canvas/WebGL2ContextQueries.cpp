/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"
#include "WebGLQuery.h"
#include "nsThreadUtils.h"

namespace mozilla {

/*
 * We fake ANY_SAMPLES_PASSED and ANY_SAMPLES_PASSED_CONSERVATIVE with
 * SAMPLES_PASSED on desktop.
 *
 * OpenGL ES 3.0 spec 4.1.6:
 *     If the target of the query is ANY_SAMPLES_PASSED_CONSERVATIVE, an
 *     implementation may choose to use a less precise version of the test which
 *     can additionally set the samples-boolean state to TRUE in some other
 *     implementation-dependent cases.
 */

RefPtr<WebGLQuery>* WebGLContext::ValidateQuerySlotByTarget(GLenum target) {
  if (IsWebGL2()) {
    switch (target) {
      case LOCAL_GL_ANY_SAMPLES_PASSED:
      case LOCAL_GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        return &mQuerySlot_SamplesPassed;

      case LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        return &mQuerySlot_TFPrimsWritten;

      default:
        break;
    }
  }

  if (IsExtensionEnabled(WebGLExtensionID::EXT_disjoint_timer_query)) {
    switch (target) {
      case LOCAL_GL_TIME_ELAPSED_EXT:
        return &mQuerySlot_TimeElapsed;

      default:
        break;
    }
  }

  ErrorInvalidEnumInfo("target", target);
  return nullptr;
}

// -------------------------------------------------------------------------
// Query Objects

RefPtr<WebGLQuery> WebGLContext::CreateQuery() {
  const FuncScope funcScope(*this, "createQuery");
  if (IsContextLost()) return nullptr;

  return new WebGLQuery(this);
}

void WebGLContext::BeginQuery(GLenum target, WebGLQuery& query) {
  FuncScope funcScope(*this, "beginQuery");
  if (IsContextLost()) return;
  funcScope.mBindFailureGuard = true;

  const auto& slot = ValidateQuerySlotByTarget(target);
  if (!slot) return;

  if (*slot) return ErrorInvalidOperation("Query target already active.");

  const auto& curTarget = query.Target();
  if (curTarget && target != curTarget) {
    ErrorInvalidOperation("Queries cannot change targets.");
    return;
  }

  ////

  query.BeginQuery(target, *slot);

  funcScope.mBindFailureGuard = false;
}

void WebGLContext::EndQuery(GLenum target) {
  FuncScope funcScope(*this, "endQuery");
  if (IsContextLost()) return;
  funcScope.mBindFailureGuard = true;

  const auto& slot = ValidateQuerySlotByTarget(target);
  if (!slot) return;

  const auto& query = *slot;
  if (!query) return ErrorInvalidOperation("Query target not active.");

  query->EndQuery();

  funcScope.mBindFailureGuard = false;
}

Maybe<double> WebGLContext::GetQueryParameter(const WebGLQuery& query,
                                              GLenum pname) const {
  const FuncScope funcScope(*this, "getQueryParameter");
  if (IsContextLost()) return Nothing();

  return query.GetQueryParameter(pname);
}

// disjoint_timer_queries

void WebGLContext::QueryCounter(WebGLQuery& query) const {
  const WebGLContext::FuncScope funcScope(*this, "queryCounterEXT");
  if (IsContextLost()) return;

  query.QueryCounter();
}

}  // namespace mozilla
