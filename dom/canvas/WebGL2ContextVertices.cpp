/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

#include "mozilla/Casting.h"

namespace mozilla {

bool
WebGL2Context::ValidateAttribPointerType(bool integerMode, GLenum type,
                                         uint32_t* out_alignment, const char* info)
{
  MOZ_ASSERT(out_alignment);

  switch (type) {
  case LOCAL_GL_BYTE:
  case LOCAL_GL_UNSIGNED_BYTE:
    *out_alignment = 1;
    return true;

  case LOCAL_GL_SHORT:
  case LOCAL_GL_UNSIGNED_SHORT:
    *out_alignment = 2;
    return true;

  case LOCAL_GL_INT:
  case LOCAL_GL_UNSIGNED_INT:
    *out_alignment = 4;
    return true;
  }

  if (!integerMode) {
    switch (type) {
    case LOCAL_GL_HALF_FLOAT:
      *out_alignment = 2;
      return true;

    case LOCAL_GL_FLOAT:
    case LOCAL_GL_FIXED:
    case LOCAL_GL_INT_2_10_10_10_REV:
    case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
      *out_alignment = 4;
      return true;
    }
  }

  ErrorInvalidEnum("%s: invalid enum value 0x%x", info, type);
  return false;
}

// -------------------------------------------------------------------------
// Vertex Attributes

void
WebGL2Context::VertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                                    GLintptr offset)
{
  if (IsContextLost())
    return;

  if (!ValidateAttribIndex(index, "vertexAttribIPointer"))
    return;

  if (!ValidateAttribPointer(true, index, size, type, LOCAL_GL_FALSE, stride, offset,
                             "vertexAttribIPointer"))
  {
    return;
  }

  MOZ_ASSERT(mBoundVertexArray);

  InvalidateBufferFetching();

  MakeContextCurrent();
  gl->fVertexAttribIPointer(index, size, type, stride, reinterpret_cast<void*>(offset));

  WebGLVertexAttribData& vd = mBoundVertexArray->mAttribs[index];
  const bool integerFunc = true;
  const bool normalized = false;
  vd.VertexAttribPointer(integerFunc, mBoundArrayBuffer, size, type, normalized, stride,
                         offset);
}

} // namespace mozilla
