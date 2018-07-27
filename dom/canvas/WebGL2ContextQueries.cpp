/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"
#include "WebGLQuery.h"
#include "gfxPrefs.h"
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

WebGLRefPtr<WebGLQuery>*
WebGLContext::ValidateQuerySlotByTarget(GLenum target)
{
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

already_AddRefed<WebGLQuery>
WebGLContext::CreateQuery()
{
    const FuncScope funcScope(*this, "createQuery");
    if (IsContextLost())
        return nullptr;

    RefPtr<WebGLQuery> globj = new WebGLQuery(this);
    return globj.forget();
}

void
WebGLContext::DeleteQuery(WebGLQuery* query)
{
    const FuncScope funcScope(*this, "deleteQuery");
    if (!ValidateDeleteObject(query))
        return;

    query->DeleteQuery();
}

void
WebGLContext::BeginQuery(GLenum target, WebGLQuery& query)
{
    const FuncScope funcScope(*this, "beginQuery");
    if (IsContextLost())
        return;

    if (!ValidateObject("query", query))
        return;

    const auto& slot = ValidateQuerySlotByTarget(target);
    if (!slot)
        return;

    if (*slot)
        return ErrorInvalidOperation("Query target already active.");

    ////

    query.BeginQuery(target, *slot);
}

void
WebGLContext::EndQuery(GLenum target)
{
    const FuncScope funcScope(*this, "endQuery");
    if (IsContextLost())
        return;

    const auto& slot = ValidateQuerySlotByTarget(target);
    if (!slot)
        return;

    const auto& query = *slot;
    if (!query)
        return ErrorInvalidOperation("Query target not active.");

    query->EndQuery();
}

void
WebGLContext::GetQuery(JSContext* cx, GLenum target, GLenum pname,
                       JS::MutableHandleValue retval)
{
    const FuncScope funcScope(*this, "getQuery");

    retval.setNull();
    if (IsContextLost())
        return;

    switch (pname) {
    case LOCAL_GL_CURRENT_QUERY_EXT:
        {
            if (IsExtensionEnabled(WebGLExtensionID::EXT_disjoint_timer_query) &&
                target == LOCAL_GL_TIMESTAMP)
            {
                // Doesn't seem illegal to ask about, but is always null.
                // TIMESTAMP has no slot, so ValidateQuerySlotByTarget would generate
                // INVALID_ENUM.
                return;
            }

            const auto& slot = ValidateQuerySlotByTarget(target);
            if (!slot || !*slot)
                return;

            const auto& query = *slot;
            if (target != query->Target())
                return;

            JS::Rooted<JS::Value> v(cx);
            dom::GetOrCreateDOMReflector(cx, slot->get(), &v);
            retval.set(v);
        }
        return;

    case LOCAL_GL_QUERY_COUNTER_BITS_EXT:
        if (!IsExtensionEnabled(WebGLExtensionID::EXT_disjoint_timer_query))
            break;

        if (target != LOCAL_GL_TIME_ELAPSED_EXT &&
            target != LOCAL_GL_TIMESTAMP_EXT)
        {
            ErrorInvalidEnumInfo("target", target);
            return;
        }

        {
            GLint bits = 0;
            gl->fGetQueryiv(target, pname, &bits);

            if (!Has64BitTimestamps() && bits > 32) {
                bits = 32;
            }
            retval.set(JS::Int32Value(bits));
        }
        return;

    default:
        break;
    }

    ErrorInvalidEnumInfo("pname", pname);
}

void
WebGLContext::GetQueryParameter(JSContext*, const WebGLQuery& query, GLenum pname,
                                JS::MutableHandleValue retval)
{
    const FuncScope funcScope(*this, "getQueryParameter");
    retval.setNull();
    if (IsContextLost())
        return;

    if (!ValidateObject("query", query))
        return;

    query.GetQueryParameter(pname, retval);
}

} // namespace mozilla
