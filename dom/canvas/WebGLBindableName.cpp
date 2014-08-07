/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLBindableName.h"
#include "GLConsts.h"
#include "mozilla/Assertions.h"

using namespace mozilla;

WebGLBindableName::WebGLBindableName()
    : mGLName(LOCAL_GL_NONE)
    , mTarget(LOCAL_GL_NONE)
{ }

void
WebGLBindableName::BindTo(GLenum target)
{
    MOZ_ASSERT(target != LOCAL_GL_NONE, "Can't bind to GL_NONE.");
    MOZ_ASSERT(mTarget == LOCAL_GL_NONE || mTarget == target, "Rebinding is illegal.");

    bool targetChanged = (target != mTarget);
    mTarget = target;
    if (targetChanged)
        OnTargetChanged();
}
