/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLQuery.h"

using namespace mozilla;

/*
 * We fake ANY_SAMPLES_PASSED and ANY_SAMPLES_PASSED_CONSERVATIVE with
 * SAMPLES_PASSED on desktop.
 *
 * OpenGL ES 3.0 spec 4.1.6
 *  If the target of the query is ANY_SAMPLES_PASSED_CONSERVATIVE, an implementation
 *  may choose to use a less precise version of the test which can additionally set
 *  the samples-boolean state to TRUE in some other implementation-dependent cases.
 */

static const char*
GetQueryTargetEnumString(WebGLenum target)
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

    if (gl->IsExtensionSupported(gl::GLContext::XXX_occlusion_query_boolean)) {
        return target;
    } else if (gl->IsExtensionSupported(gl::GLContext::XXX_occlusion_query2)) {
        return LOCAL_GL_ANY_SAMPLES_PASSED;
    }

    return LOCAL_GL_SAMPLES_PASSED;
}

already_AddRefed<WebGLQuery>
WebGLContext::CreateQuery()
{
    if (!IsContextStable())
        return nullptr;

    if (mActiveOcclusionQuery && !gl->IsGLES2()) {
        /* http://www.opengl.org/registry/specs/ARB/occlusion_query.txt
         * Calling either GenQueriesARB or DeleteQueriesARB while any query of
         * any target is active causes an INVALID_OPERATION error to be
         * generated.
         */
        GenerateWarning("createQuery: the WebGL 2 prototype might generate INVALID_OPERATION"
                        "when creating a query object while one other is active.");
        /*
         * We *need* to lock webgl2 to GL>=3.0 on desktop, but we don't have a good
         * mechanism to do this yet. See bug 898404.
         */
    }

    nsRefPtr<WebGLQuery> globj = new WebGLQuery(this);

    return globj.forget();
}

void
WebGLContext::DeleteQuery(WebGLQuery *query)
{
    if (!IsContextStable())
        return;

    if (!query)
        return;

    if (query->IsDeleted())
        return;

    if (query->IsActive()) {
        EndQuery(query->mType);
    }

    if (mActiveOcclusionQuery && !gl->IsGLES2()) {
        /* http://www.opengl.org/registry/specs/ARB/occlusion_query.txt
         * Calling either GenQueriesARB or DeleteQueriesARB while any query of
         * any target is active causes an INVALID_OPERATION error to be
         * generated.
         */
        GenerateWarning("deleteQuery: the WebGL 2 prototype might generate INVALID_OPERATION"
                        "when deleting a query object while one other is active.");
    }

    query->RequestDelete();
}

void
WebGLContext::BeginQuery(WebGLenum target, WebGLQuery *query)
{
    if (!IsContextStable())
        return;

    if (!ValidateQueryTargetParameter(target, "beginQuery")) {
        return;
    }

    if (!query) {
        /* SPECS BeginQuery.1
         * http://www.khronos.org/registry/gles/extensions/EXT/EXT_occlusion_query_boolean.txt
         * BeginQueryEXT sets the active query object name for the query type given
         * by <target> to <id>. If BeginQueryEXT is called with an <id> of zero, if
         * the active query object name for <target> is non-zero (for the targets
         * ANY_SAMPLES_PASSED_EXT and ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, if the
         * active query for either target is non-zero), if <id> is the name of an
         * existing query object whose type does not match <target>, or if <id> is the
         * active query object name for any query type, the error INVALID_OPERATION is
         * generated.
         */
        ErrorInvalidOperation("beginQuery: query should not be null");
        return;
    }

    if (query->IsDeleted()) {
        /* http://www.khronos.org/registry/gles/extensions/EXT/EXT_occlusion_query_boolean.txt
         * BeginQueryEXT fails and an INVALID_OPERATION error is generated if <id>
         * is not a name returned from a previous call to GenQueriesEXT, or if such
         * a name has since been deleted with DeleteQueriesEXT.
         */
        ErrorInvalidOperation("beginQuery: query has been deleted");
        return;
    }

    if (query->HasEverBeenActive() &&
        query->mType != target)
    {
        /*
         * See SPECS BeginQuery.1
         */
        ErrorInvalidOperation("beginQuery: target doesn't match with the query type");
        return;
    }

    if (GetActiveQueryByTarget(target)) {
        /*
         * See SPECS BeginQuery.1
         */
        ErrorInvalidOperation("beginQuery: an other query already active");
        return;
    }

    if (!query->HasEverBeenActive()) {
        query->mType = target;
    }

    MakeContextCurrent();

    if (target == LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN) {
        gl->fBeginQuery(LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, query->mGLName);
    } else {
        gl->fBeginQuery(SimulateOcclusionQueryTarget(gl, target), query->mGLName);
    }

    GetActiveQueryByTarget(target) = query;
}

void
WebGLContext::EndQuery(WebGLenum target)
{
    if (!IsContextStable())
        return;

    if (!ValidateQueryTargetParameter(target, "endQuery")) {
        return;
    }

    if (!GetActiveQueryByTarget(target) ||
        target != GetActiveQueryByTarget(target)->mType)
    {
        /* http://www.khronos.org/registry/gles/extensions/EXT/EXT_occlusion_query_boolean.txt
         * marks the end of the sequence of commands to be tracked for the query type
         * given by <target>. The active query object for <target> is updated to
         * indicate that query results are not available, and the active query object
         * name for <target> is reset to zero. When the commands issued prior to
         * EndQueryEXT have completed and a final query result is available, the
         * query object active when EndQueryEXT is called is updated by the GL. The
         * query object is updated to indicate that the query results are available
         * and to contain the query result. If the active query object name for
         * <target> is zero when EndQueryEXT is called, the error INVALID_OPERATION
         * is generated.
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

    GetActiveQueryByTarget(target) = nullptr;
}

bool
WebGLContext::IsQuery(WebGLQuery *query)
{
    if (!IsContextStable())
        return false;

    if (!query)
        return false;

    return ValidateObjectAllowDeleted("isQuery", query) &&
           !query->IsDeleted() &&
           query->HasEverBeenActive();
}

already_AddRefed<WebGLQuery>
WebGLContext::GetQuery(WebGLenum target, WebGLenum pname)
{
    if (!IsContextStable())
        return nullptr;

    if (!ValidateQueryTargetParameter(target, "getQuery")) {
        return nullptr;
    }

    if (pname != LOCAL_GL_CURRENT_QUERY) {
        /* OpenGL ES 3.0 spec 6.1.7
         *  pname must be CURRENT_QUERY.
         */
        ErrorInvalidEnum("getQuery: pname must be CURRENT_QUERY");
        return nullptr;
    }

    nsRefPtr<WebGLQuery> tmp = GetActiveQueryByTarget(target).get();
    return tmp.forget();
}

JS::Value
WebGLContext::GetQueryObject(JSContext* cx, WebGLQuery *query, WebGLenum pname)
{
    if (!IsContextStable())
        return JS::NullValue();

    if (!query) {
        /* OpenGL ES 3.0 spec 6.1.7 (spec getQueryObject 1)
         *  If id is not the name of a query object, or if the query object named by id is
         *  currently active, then an INVALID_OPERATION error is generated. pname must be
         *  QUERY_RESULT or QUERY_RESULT_AVAILABLE.
         */
        ErrorInvalidOperation("getQueryObject: query should not be null");
        return JS::NullValue();
    }

    if (query->IsDeleted()) {
        // See (spec getQueryObject 1)
        ErrorInvalidOperation("getQueryObject: query has been deleted");
        return JS::NullValue();
    }

    if (query->IsActive()) {
        // See (spec getQueryObject 1)
        ErrorInvalidOperation("getQueryObject: query is active");
        return JS::NullValue();
    }

    if (!query->HasEverBeenActive()) {
        /* See (spec getQueryObject 1)
         *  If this instance of WebGLQuery has never been active before, that mean that
         *  query->mGLName is not a query object yet.
         */
        ErrorInvalidOperation("getQueryObject: query has never been active");
        return JS::NullValue();
    }

    switch (pname)
    {
        case LOCAL_GL_QUERY_RESULT_AVAILABLE:
        {
            GLuint returned = 0;

            MakeContextCurrent();
            gl->fGetQueryObjectuiv(query->mGLName, LOCAL_GL_QUERY_RESULT_AVAILABLE, &returned);

            return JS::BooleanValue(returned != 0);
        }

        case LOCAL_GL_QUERY_RESULT:
        {
            GLuint returned = 0;

            MakeContextCurrent();
            gl->fGetQueryObjectuiv(query->mGLName, LOCAL_GL_QUERY_RESULT, &returned);

            if (query->mType == LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN) {
                return JS::NumberValue(uint32_t(returned));
            }

            /*
             * test (returned != 0) is important because ARB_occlusion_query on desktop drivers
             * return the number of samples drawed when the OpenGL ES extension
             * ARB_occlusion_query_boolean return only a boolean if a sample has been drawed.
             */
            return JS::BooleanValue(returned != 0);
        }

        default:
            break;
    }

    ErrorInvalidEnum("getQueryObject: pname must be QUERY_RESULT{_AVAILABLE}");
    return JS::NullValue();
}

bool
WebGLContext::ValidateQueryTargetParameter(WebGLenum target, const char* infos)
{
    switch (target) {
        case LOCAL_GL_ANY_SAMPLES_PASSED:
        case LOCAL_GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        case LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
            return true;
    }

    ErrorInvalidEnum("%s: unknown query target", infos);
    return false;
}

WebGLRefPtr<WebGLQuery>&
WebGLContext::GetActiveQueryByTarget(WebGLenum target)
{
    MOZ_ASSERT(ValidateQueryTargetParameter(target, "private WebGLContext::GetActiveQueryByTarget"));

    switch (target) {
        case LOCAL_GL_ANY_SAMPLES_PASSED:
        case LOCAL_GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
            return mActiveOcclusionQuery;
        case LOCAL_GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
            return mActiveTransformFeedbackQuery;
    }

    MOZ_ASSERT(false, "WebGLContext::GetActiveQueryByTarget is not compatible with "
                      "WebGLContext::ValidateQueryTargetParameter");
    return mActiveOcclusionQuery;
}


