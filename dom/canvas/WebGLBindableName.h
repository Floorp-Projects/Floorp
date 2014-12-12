/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLBINDABLENAME_H_
#define WEBGLBINDABLENAME_H_

#include "WebGLTypes.h"

#include "GLDefs.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Assertions.h"

namespace mozilla {

/** Represents a binding to a GL binding point
 */
template<typename T>
class WebGLBindable
{
public:
    WebGLBindable() : mTarget(LOCAL_GL_NONE) { }
    bool HasEverBeenBound() const { return mTarget != LOCAL_GL_NONE; }

    void BindTo(T target) {
        MOZ_ASSERT(target != LOCAL_GL_NONE, "Can't bind to GL_NONE.");
        MOZ_ASSERT(!HasEverBeenBound() || mTarget == target, "Rebinding is illegal.");

        bool targetChanged = (target != mTarget);
        mTarget = target;
        if (targetChanged)
            OnTargetChanged();
    }

    T Target() const {
        MOZ_ASSERT(HasEverBeenBound());
        return mTarget;
    }

protected:
    //! Called after mTarget has been changed by BindTo(target).
    virtual void OnTargetChanged() {}

    T mTarget;
};


/** Represents a GL name that can be bound to a target.
 */
template<typename T>
class WebGLBindableName
    : public WebGLBindable<T>
{
public:

    explicit WebGLBindableName(GLuint aName)
        : WebGLBindable<T>()
        , mGLName(aName)
    { }
    GLuint GLName() const { return mGLName; }

protected:
    const GLuint mGLName;
};


} // namespace mozilla

#endif // !WEBGLBINDABLENAME_H_
