/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_VERTEX_ATTRIB_DATA_H_
#define WEBGL_VERTEX_ATTRIB_DATA_H_

#include "GLDefs.h"
#include "WebGLFormats.h"
#include "WebGLObjectModel.h"

namespace mozilla {

class WebGLBuffer;

class WebGLVertexAttribData final {
 public:
  uint32_t mDivisor = 0;
  bool mEnabled = false;

 private:
  bool mIntegerFunc = false;

 public:
  RefPtr<WebGLBuffer> mBuf;

 private:
  // This and other members below are initialized to dummies here, and later
  // fully initialized in the ctor.
  GLenum mType = 0;
  webgl::AttribBaseType mBaseType = webgl::AttribBaseType::Float;
  uint8_t mSize = 0;  // num of mType vals per vert
  uint8_t mBytesPerVertex = 0;
  bool mNormalized = false;
  uint32_t mStride = 0;  // bytes
  uint32_t mExplicitStride = 0;
  uint64_t mByteOffset = 0;

 public:
#define GETTER(X) \
  const decltype(m##X)& X() const { return m##X; }

  GETTER(IntegerFunc)
  GETTER(Type)
  GETTER(BaseType)
  GETTER(Size)
  GETTER(BytesPerVertex)
  GETTER(Normalized)
  GETTER(Stride)
  GETTER(ExplicitStride)
  GETTER(ByteOffset)

#undef GETTER

  // note that these initial values are what GL initializes vertex attribs to
  WebGLVertexAttribData() {
    VertexAttribPointer(false, nullptr, 4, LOCAL_GL_FLOAT, false, 0, 0);
  }

  void VertexAttribPointer(bool integerFunc, WebGLBuffer* buf, uint8_t size,
                           GLenum type, bool normalized, uint32_t stride,
                           uint64_t byteOffset);

  void DoVertexAttribPointer(gl::GLContext* gl, GLuint index) const;
};

}  // namespace mozilla

#endif  // WEBGL_VERTEX_ATTRIB_DATA_H_
