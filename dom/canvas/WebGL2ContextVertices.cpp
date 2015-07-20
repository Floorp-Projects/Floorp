/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

namespace mozilla {

typedef union { GLint i; GLfloat f; GLuint u; } fi_t;

static inline
GLfloat PuntToFloat(GLint i)
{
   fi_t tmp;
   tmp.i = i;
   return tmp.f;
}

static inline
GLfloat PuntToFloat(GLuint u)
{
   fi_t tmp;
   tmp.u = u;
   return tmp.f;
}

bool
WebGL2Context::ValidateAttribPointerType(bool integerMode, GLenum type,
                                         GLsizei* out_alignment, const char* info)
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
  mBoundVertexArray->EnsureAttrib(index);

  InvalidateBufferFetching();

  WebGLVertexAttribData& vd = mBoundVertexArray->mAttribs[index];

  vd.buf = mBoundArrayBuffer;
  vd.stride = stride;
  vd.size = size;
  vd.byteOffset = offset;
  vd.type = type;
  vd.normalized = false;
  vd.integer = true;

  MakeContextCurrent();
  gl->fVertexAttribIPointer(index, size, type, stride, reinterpret_cast<void*>(offset));
}

void
WebGL2Context::VertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
{
  if (IsContextLost())
    return;

  if (index || gl->IsGLES()) {
    MakeContextCurrent();
    gl->fVertexAttribI4i(index, x, y, z, w);
  } else {
    mVertexAttrib0Vector[0] = PuntToFloat(x);
    mVertexAttrib0Vector[1] = PuntToFloat(y);
    mVertexAttrib0Vector[2] = PuntToFloat(z);
    mVertexAttrib0Vector[3] = PuntToFloat(w);
  }
}

void
WebGL2Context::VertexAttribI4iv(GLuint index, size_t length, const GLint* v)
{
  if (!ValidateAttribArraySetter("vertexAttribI4iv", 4, length))
    return;

  if (index || gl->IsGLES()) {
    MakeContextCurrent();
    gl->fVertexAttribI4iv(index, v);
  } else {
    mVertexAttrib0Vector[0] = PuntToFloat(v[0]);
    mVertexAttrib0Vector[1] = PuntToFloat(v[1]);
    mVertexAttrib0Vector[2] = PuntToFloat(v[2]);
    mVertexAttrib0Vector[3] = PuntToFloat(v[3]);
  }
}

void
WebGL2Context::VertexAttribI4iv(GLuint index, const dom::Sequence<GLint>& v)
{
  VertexAttribI4iv(index, v.Length(), v.Elements());
}

void
WebGL2Context::VertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
  if (IsContextLost())
    return;

  if (index || gl->IsGLES()) {
    MakeContextCurrent();
    gl->fVertexAttribI4ui(index, x, y, z, w);
  } else {
    mVertexAttrib0Vector[0] = PuntToFloat(x);
    mVertexAttrib0Vector[1] = PuntToFloat(y);
    mVertexAttrib0Vector[2] = PuntToFloat(z);
    mVertexAttrib0Vector[3] = PuntToFloat(w);
  }
}

void
WebGL2Context::VertexAttribI4uiv(GLuint index, size_t length, const GLuint* v)
{
  if (IsContextLost())
    return;

  if (index || gl->IsGLES()) {
    MakeContextCurrent();
    gl->fVertexAttribI4uiv(index, v);
  } else {
    mVertexAttrib0Vector[0] = PuntToFloat(v[0]);
    mVertexAttrib0Vector[1] = PuntToFloat(v[1]);
    mVertexAttrib0Vector[2] = PuntToFloat(v[2]);
    mVertexAttrib0Vector[3] = PuntToFloat(v[3]);
  }
}

void
WebGL2Context::VertexAttribI4uiv(GLuint index, const dom::Sequence<GLuint>& v)
{
  VertexAttribI4uiv(index, v.Length(), v.Elements());
}

} // namespace mozilla
