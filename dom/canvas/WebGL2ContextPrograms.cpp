/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLProgram.h"

namespace mozilla {

// -------------------------------------------------------------------------
// Programs and shaders

GLint
WebGL2Context::GetFragDataLocation(const WebGLProgram& prog, const nsAString& name)
{
    if (IsContextLost())
        return -1;

    if (!ValidateObject("getFragDataLocation: program", prog))
        return -1;

    return prog.GetFragDataLocation(name);
}

} // namespace mozilla
