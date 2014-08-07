/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLBINDABLENAME_H_
#define WEBGLBINDABLENAME_H_

#include "WebGLTypes.h"

namespace mozilla {

/** Represents a GL name that can be bound to a target.
 */
class WebGLBindableName
{
public:
    WebGLBindableName();
    void BindTo(GLenum target);

    bool HasEverBeenBound() const { return mTarget != 0; }
    GLuint GLName() const { return mGLName; }
    GLenum Target() const { return mTarget; }

protected:

    //! Called after mTarget has been changed by BindTo(target).
    virtual void OnTargetChanged() {}

    GLuint mGLName;
    GLenum mTarget;
};

} // namespace mozilla

#endif // !WEBGLBINDABLENAME_H_
