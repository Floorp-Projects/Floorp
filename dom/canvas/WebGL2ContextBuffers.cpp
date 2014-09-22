/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::dom;

// -------------------------------------------------------------------------
// Buffer objects

void
WebGL2Context::CopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset,
                                 GLintptr writeOffset, GLsizeiptr size)
{
    MakeContextCurrent();
    gl->fCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

void
WebGL2Context::GetBufferSubData(GLenum target, GLintptr offset, const dom::ArrayBuffer& returnedData)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::GetBufferSubData(GLenum target, GLintptr offset, const dom::ArrayBufferView& returnedData)
{
    MOZ_CRASH("Not Implemented.");
}
