/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLQuery.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "nsContentUtils.h"
#include "WebGLContext.h"

namespace mozilla {

class AvailableRunnable final : public Runnable
{
    const RefPtr<WebGLQuery> mQuery;

public:
    explicit AvailableRunnable(WebGLQuery* query)
        : mQuery(query)
    { }

    NS_IMETHOD Run() override {
        mQuery->mCanBeAvailable = true;
        return NS_OK;
    }
};

////

static GLuint
GenQuery(gl::GLContext* gl)
{
    gl->MakeCurrent();

    GLuint ret = 0;
    gl->fGenQueries(1, &ret);
    return ret;
}

WebGLQuery::WebGLQuery(WebGLContext* webgl)
    : WebGLRefCountedObject(webgl)
    , mGLName(GenQuery(mContext->gl))
    , mTarget(0)
    , mActiveSlot(nullptr)
    , mCanBeAvailable(false)
{
    mContext->mQueries.insertBack(this);
}

void
WebGLQuery::Delete()
{
    mContext->MakeContextCurrent();
    mContext->gl->fDeleteQueries(1, &mGLName);
    LinkedListElement<WebGLQuery>::removeFrom(mContext->mQueries);
}

////

static GLenum
TargetForDriver(const gl::GLContext* gl, GLenum target)
{
    switch (target) {
    case LOCAL_GL_ANY_SAMPLES_PASSED:
    case LOCAL_GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        break;

    default:
        return target;
    }

    if (gl->IsSupported(gl::GLFeature::occlusion_query_boolean))
        return target;

    if (gl->IsSupported(gl::GLFeature::occlusion_query2))
        return LOCAL_GL_ANY_SAMPLES_PASSED;

    return LOCAL_GL_SAMPLES_PASSED;
}

void
WebGLQuery::BeginQuery(GLenum target, WebGLRefPtr<WebGLQuery>& slot)
{
    const char funcName[] = "beginQuery";

    if (mTarget && target != mTarget) {
        mContext->ErrorInvalidOperation("%s: Queries cannot change targets.", funcName);
        return;
    }

    ////

    mTarget = target;
    mActiveSlot = &slot;
    *mActiveSlot = this;

    ////

    const auto& gl = mContext->gl;
    gl->MakeCurrent();

    const auto driverTarget = TargetForDriver(gl, mTarget);
    gl->fBeginQuery(driverTarget, mGLName);
}

void
WebGLQuery::EndQuery()
{
    *mActiveSlot = nullptr;
    mActiveSlot = nullptr;
    mCanBeAvailable = false;

    ////

    const auto& gl = mContext->gl;
    gl->MakeCurrent();

    const auto driverTarget = TargetForDriver(gl, mTarget);
    gl->fEndQuery(driverTarget);

    ////

    NS_DispatchToCurrentThread(new AvailableRunnable(this));
}

void
WebGLQuery::GetQueryParameter(GLenum pname, JS::MutableHandleValue retval) const
{
    const char funcName[] = "getQueryParameter";

    switch (pname) {
    case LOCAL_GL_QUERY_RESULT_AVAILABLE:
    case LOCAL_GL_QUERY_RESULT:
        break;

    default:
        mContext->ErrorInvalidEnumArg(funcName, "pname", pname);
        return;
    }

    if (!mTarget) {
        mContext->ErrorInvalidOperation("%s: Query has never been active.", funcName);
        return;
    }

    if (mActiveSlot)
        return mContext->ErrorInvalidOperation("%s: Query is still active.", funcName);

    // End of validation
    ////

    // We must usually wait for an event loop before the query can be available.
    const bool canBeAvailable = (mCanBeAvailable || gfxPrefs::WebGLImmediateQueries());
    if (!canBeAvailable) {
        if (pname == LOCAL_GL_QUERY_RESULT_AVAILABLE) {
            retval.set(JS::BooleanValue(false));
        }
        return;
    }

    const auto& gl = mContext->gl;
    gl->MakeCurrent();

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
            retval.set(JS::BooleanValue(bool(val)));
            break;

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

bool
WebGLQuery::IsQuery() const
{
    MOZ_ASSERT(!IsDeleted());

    if (!mTarget)
        return false;

    return true;
}

void
WebGLQuery::DeleteQuery()
{
    MOZ_ASSERT(!IsDeleteRequested());

    if (mActiveSlot) {
        EndQuery();
    }

    RequestDelete();
}

void
WebGLQuery::QueryCounter(const char* funcName, GLenum target)
{
    if (target != LOCAL_GL_TIMESTAMP_EXT) {
        mContext->ErrorInvalidEnum("%s: `target` must be TIMESTAMP_EXT.", funcName,
                                   target);
        return;
    }

    if (mTarget && target != mTarget) {
        mContext->ErrorInvalidOperation("%s: Queries cannot change targets.", funcName);
        return;
    }

    mTarget = target;
    mCanBeAvailable = false;

    const auto& gl = mContext->gl;
    gl->MakeCurrent();
    gl->fQueryCounter(mGLName, mTarget);

    NS_DispatchToCurrentThread(new AvailableRunnable(this));
}

////

JSObject*
WebGLQuery::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLQueryBinding::Wrap(cx, this, givenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLQuery)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLQuery, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLQuery, Release)

} // namespace mozilla
