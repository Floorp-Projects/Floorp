/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLObjectModel.h"

#include "WebGLContext.h"

namespace mozilla {

WebGLContextBoundObject::WebGLContextBoundObject(WebGLContext* webgl)
    : mContext(webgl)
    , mContextGeneration(webgl->Generation())
{
}

bool
WebGLContextBoundObject::IsCompatibleWithContext(const WebGLContext* other) const
{
    return (mContext == other &&
            mContextGeneration == other->Generation());
}

} // namespace mozilla
