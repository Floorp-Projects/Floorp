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

static const char*
GetQueryTargetEnumString(GLenum target)
{
    switch (target)
    {
    case LOCAL_GL_ANY_SAMPLES_PASSED:
        return "ANY_SAMPLES_PASSED";
    case LOCAL_GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        return "ANY_SAMPLES_PASSED_CONSERVATIVE";
    case LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        return "TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN";
    default:
        break;
    }

    MOZ_ASSERT(false, "Unknown query `target`.");
    return "UNKNOWN_QUERY_TARGET";
}

static inline GLenum
SimulateOcclusionQueryTarget(const gl::GLContext* gl, GLenum target)
{
    MOZ_ASSERT(target == LOCAL_GL_ANY_SAMPLES_PASSED ||
               target == LOCAL_GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
               "unknown occlusion query target");

    if (gl->IsSupported(gl::GLFeature::occlusion_query_boolean)) {
        return target;
    } else if (gl->IsSupported(gl::GLFeature::occlusion_query2)) {
        return LOCAL_GL_ANY_SAMPLES_PASSED;
    }

    return LOCAL_GL_SAMPLES_PASSED;
}

WebGLRefPtr<WebGLQuery>&
WebGLContext::GetQuerySlotByTarget(GLenum target)
{
    /* This function assumes that target has been validated for either
     * WebGL1 or WebGL2.
     */
    switch (target) {
    case LOCAL_GL_ANY_SAMPLES_PASSED:
    case LOCAL_GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        return mActiveOcclusionQuery;

    case LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        return mActiveTransformFeedbackQuery;

    default:
        MOZ_CRASH("GFX: Should not get here.");
    }
}


// -------------------------------------------------------------------------
// Query Objects

already_AddRefed<WebGLQuery>
WebGL2Context::CreateQuery()
{
    if (IsContextLost())
        return nullptr;

    if (mActiveOcclusionQuery && !gl->IsGLES()) {
        /* http://www.opengl.org/registry/specs/ARB/occlusion_query.txt
         *
         * Calling either GenQueriesARB or DeleteQueriesARB while any query of
         * any target is active causes an INVALID_OPERATION error to be
         * generated.
         */
        GenerateWarning("createQuery: The WebGL 2 prototype might generate"
                        " INVALID_OPERATION when creating a query object while"
                        " one other is active.");
        /*
         * We *need* to lock webgl2 to GL>=3.0 on desktop, but we don't have a
         * good mechanism to do this yet. See bug 898404.
         */
    }

    RefPtr<WebGLQuery> globj = new WebGLQuery(this);

    return globj.forget();
}

void
WebGL2Context::DeleteQuery(WebGLQuery* query)
{
    if (IsContextLost())
        return;

    if (!query)
        return;

    if (query->IsDeleted())
        return;

    if (query->IsActive())
        EndQuery(query->mType);

    if (mActiveOcclusionQuery && !gl->IsGLES()) {
        /* http://www.opengl.org/registry/specs/ARB/occlusion_query.txt
         *
         * Calling either GenQueriesARB or DeleteQueriesARB while any query of
         * any target is active causes an INVALID_OPERATION error to be
         * generated.
         */
        GenerateWarning("deleteQuery: The WebGL 2 prototype might generate"
                        " INVALID_OPERATION when deleting a query object while"
                        " one other is active.");
    }

    query->RequestDelete();
}

bool
WebGL2Context::IsQuery(WebGLQuery* query)
{
    if (IsContextLost())
        return false;

    if (!query)
        return false;

    return (ValidateObjectAllowDeleted("isQuery", query) &&
            !query->IsDeleted() &&
            query->HasEverBeenActive());
}

void
WebGL2Context::BeginQuery(GLenum target, WebGLQuery* query)
{
    if (IsContextLost())
        return;

    if (!ValidateQueryTarget(target, "beginQuery"))
        return;

    if (!query) {
        /* From GLES's EXT_occlusion_query_boolean:
         *     BeginQueryEXT sets the active query object name for the query
         *     type given by <target> to <id>. If BeginQueryEXT is called with
         *     an <id> of zero, if the active query object name for <target> is
         *     non-zero (for the targets ANY_SAMPLES_PASSED_EXT and
         *     ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, if the active query for
         *     either target is non-zero), if <id> is the name of an existing
         *     query object whose type does not match <target>, or if <id> is
         *     the active query object name for any query type, the error
         *     INVALID_OPERATION is generated.
         */
        ErrorInvalidOperation("beginQuery: Query should not be null.");
        return;
    }

    if (query->IsDeleted()) {
        /* From GLES's EXT_occlusion_query_boolean:
         *     BeginQueryEXT fails and an INVALID_OPERATION error is generated
         *     if <id> is not a name returned from a previous call to
         *     GenQueriesEXT, or if such a name has since been deleted with
         *     DeleteQueriesEXT.
         */
        ErrorInvalidOperation("beginQuery: Query has been deleted.");
        return;
    }

    if (query->HasEverBeenActive() &&
        query->mType != target)
    {
        ErrorInvalidOperation("beginQuery: Target doesn't match with the query"
                              " type.");
        return;
    }

    WebGLRefPtr<WebGLQuery>& querySlot = GetQuerySlotByTarget(target);
    WebGLQuery* activeQuery = querySlot.get();
    if (activeQuery)
        return ErrorInvalidOperation("beginQuery: An other query already active.");

    if (!query->HasEverBeenActive())
        query->mType = target;

    MakeContextCurrent();

    if (target == LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN) {
        gl->fBeginQuery(LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,
                        query->mGLName);
    } else {
        gl->fBeginQuery(SimulateOcclusionQueryTarget(gl, target),
                        query->mGLName);
    }

    UpdateBoundQuery(target, query);
}

void
WebGL2Context::EndQuery(GLenum target)
{
    if (IsContextLost())
        return;

    if (!ValidateQueryTarget(target, "endQuery"))
        return;

    WebGLRefPtr<WebGLQuery>& querySlot = GetQuerySlotByTarget(target);
    WebGLQuery* activeQuery = querySlot.get();

    if (!activeQuery || target != activeQuery->mType)
    {
        /* From GLES's EXT_occlusion_query_boolean:
         *     marks the end of the sequence of commands to be tracked for the
         *     query type given by <target>. The active query object for
         *     <target> is updated to indicate that query results are not
         *     available, and the active query object name for <target> is reset
         *     to zero. When the commands issued prior to EndQueryEXT have
         *     completed and a final query result is available, the query object
         *     active when EndQueryEXT is called is updated by the GL. The query
         *     object is updated to indicate that the query results are
         *     available and to contain the query result. If the active query
         *     object name for <target> is zero when EndQueryEXT is called, the
         *     error INVALID_OPERATION is generated.
         */
        ErrorInvalidOperation("endQuery: There is no active query of type %s.",
                              GetQueryTargetEnumString(target));
        return;
    }

    MakeContextCurrent();

    if (target == LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN) {
        gl->fEndQuery(LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    } else {
        gl->fEndQuery(SimulateOcclusionQueryTarget(gl, target));
    }

    UpdateBoundQuery(target, nullptr);
    NS_DispatchToCurrentThread(new WebGLQuery::AvailableRunnable(activeQuery));
}

already_AddRefed<WebGLQuery>
WebGL2Context::GetQuery(GLenum target, GLenum pname)
{
    if (IsContextLost())
        return nullptr;

    if (!ValidateQueryTarget(target, "getQuery"))
        return nullptr;

    if (pname != LOCAL_GL_CURRENT_QUERY) {
        /* OpenGL ES 3.0 spec 6.1.7:
         *     pname must be CURRENT_QUERY.
         */
        ErrorInvalidEnum("getQuery: `pname` must be CURRENT_QUERY.");
        return nullptr;
    }

    WebGLRefPtr<WebGLQuery>& targetSlot = GetQuerySlotByTarget(target);
    RefPtr<WebGLQuery> tmp = targetSlot.get();
    if (tmp && tmp->mType != target) {
        // Query in slot doesn't match target
        return nullptr;
    }

    return tmp.forget();
}

static bool
ValidateQueryEnum(WebGLContext* webgl, GLenum pname, const char* info)
{
    switch (pname) {
    case LOCAL_GL_QUERY_RESULT_AVAILABLE:
    case LOCAL_GL_QUERY_RESULT:
        return true;

    default:
        webgl->ErrorInvalidEnum("%s: invalid pname: %s", info, webgl->EnumName(pname));
        return false;
    }
}

void
WebGL2Context::GetQueryParameter(JSContext*, WebGLQuery* query, GLenum pname,
                                 JS::MutableHandleValue retval)
{
    retval.set(JS::NullValue());

    if (IsContextLost())
        return;

    if (!ValidateQueryEnum(this, pname, "getQueryParameter"))
        return;

    if (!query) {
        /* OpenGL ES 3.0 spec 6.1.7 (spec getQueryObject 1):
         *    If id is not the name of a query object, or if the query object
         *    named by id is currently active, then an INVALID_OPERATION error
         *    is generated. pname must be QUERY_RESULT or
         *    QUERY_RESULT_AVAILABLE.
         */
        ErrorInvalidOperation("getQueryObject: `query` should not be null.");
        return;
    }

    if (query->IsDeleted()) {
        // See (spec getQueryObject 1)
        ErrorInvalidOperation("getQueryObject: `query` has been deleted.");
        return;
    }

    if (query->IsActive()) {
        // See (spec getQueryObject 1)
        ErrorInvalidOperation("getQueryObject: `query` is active.");
        return;
    }

    if (!query->HasEverBeenActive()) {
        /* See (spec getQueryObject 1)
         *     If this instance of WebGLQuery has never been active before, that
         *     mean that query->mGLName is not a query object yet.
         */
        ErrorInvalidOperation("getQueryObject: `query` has never been active.");
        return;
    }

    // We must wait for an event loop before the query can be available
    if (!query->mCanBeAvailable && !gfxPrefs::WebGLImmediateQueries()) {
        if (pname == LOCAL_GL_QUERY_RESULT_AVAILABLE) {
            retval.set(JS::BooleanValue(false));
        }
        return;
    }

    MakeContextCurrent();
    GLuint returned = 0;
    switch (pname) {
    case LOCAL_GL_QUERY_RESULT_AVAILABLE:
        gl->fGetQueryObjectuiv(query->mGLName, LOCAL_GL_QUERY_RESULT_AVAILABLE, &returned);
        retval.set(JS::BooleanValue(returned != 0));
        return;

    case LOCAL_GL_QUERY_RESULT:
        gl->fGetQueryObjectuiv(query->mGLName, LOCAL_GL_QUERY_RESULT, &returned);

        if (query->mType == LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN) {
            retval.set(JS::NumberValue(returned));
            return;
        }

        /*
         * test (returned != 0) is important because ARB_occlusion_query on desktop drivers
         * return the number of samples drawed when the OpenGL ES extension
         * ARB_occlusion_query_boolean return only a boolean if a sample has been drawed.
         */
        retval.set(JS::BooleanValue(returned != 0));
        return;

    default:
        break;
    }

    ErrorInvalidEnum("getQueryObject: `pname` must be QUERY_RESULT{_AVAILABLE}.");
}

void
WebGL2Context::UpdateBoundQuery(GLenum target, WebGLQuery* query)
{
    WebGLRefPtr<WebGLQuery>& querySlot = GetQuerySlotByTarget(target);
    querySlot = query;
}

bool
WebGL2Context::ValidateQueryTarget(GLenum target, const char* info)
{
    switch (target) {
    case LOCAL_GL_ANY_SAMPLES_PASSED:
    case LOCAL_GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
    case LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        return true;

    default:
        ErrorInvalidEnumInfo(info, target);
        return false;
    }
}

} // namespace mozilla
