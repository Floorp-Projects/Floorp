/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLVertexArray.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLVertexArrayGL.h"
#include "WebGLVertexArrayFake.h"

namespace mozilla {

WebGLVertexArray* WebGLVertexArray::Create(WebGLContext* webgl) {
  if (webgl->gl->IsSupported(gl::GLFeature::vertex_array_object)) {
    return new WebGLVertexArrayGL(webgl);
  }
  return new WebGLVertexArrayFake(webgl);
}

WebGLVertexArray::WebGLVertexArray(WebGLContext* const webgl)
    : WebGLContextBoundObject(webgl) {
  const webgl::VertAttribPointerDesc defaultDesc;
  const webgl::VertAttribPointerCalculated defaultCalc;
  for (const auto i : IntegerRange(mContext->MaxVertexAttribs())) {
    AttribPointer(i, nullptr, defaultDesc, defaultCalc);
  }
}

WebGLVertexArray::~WebGLVertexArray() = default;

Maybe<double> WebGLVertexArray::GetVertexAttrib(const uint32_t index,
                                                const GLenum pname) const {
  const auto& binding = mBindings[index];
  const auto& desc = mDescs[index];

  switch (pname) {
    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      return Some(desc.byteStrideOrZero);

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_SIZE:
      return Some(desc.channels);

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE:
      return Some(desc.type);

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_INTEGER:
      return Some(desc.intFunc);

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
      return Some(binding.layout.divisor);

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      return Some(binding.layout.isArray);

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      return Some(desc.normalized);

    case LOCAL_GL_VERTEX_ATTRIB_ARRAY_POINTER:
      return Some(binding.layout.byteOffset);

    default:
      return {};
  }
}

void WebGLVertexArray::DoAttribDivisor(const uint32_t index) const {
  const auto& binding = mBindings.at(index);
  auto driverDivisor = binding.layout.divisor;
  if (!binding.layout.isArray) {
    driverDivisor = 0;
  }
  mContext->gl->fVertexAttribDivisor(index, driverDivisor);
}

}  // namespace mozilla
