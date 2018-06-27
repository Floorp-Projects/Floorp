/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLVertexArrayObject.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {
namespace dom {

WebGLVertexArray*
WebGLVertexArrayObject::Create(WebGLContext* webgl)
{
  // WebGL 2: This is core in GL ES 3. If support is missing something
  // is very wrong.
  bool vaoSupport = webgl->GL()->IsSupported(gl::GLFeature::vertex_array_object);
  MOZ_RELEASE_ASSERT(vaoSupport, "GFX: Vertex Array Objects aren't supported.");
  if (vaoSupport)
    return new WebGLVertexArrayObject(webgl);

  return nullptr;
}

JSObject*
WebGLVertexArrayObject::WrapObject(JSContext* cx,
                                   JS::Handle<JSObject*> givenProto)
{
  return dom::WebGLVertexArrayObject_Binding::Wrap(cx, this, givenProto);
}

} // namespace dom
} // namespace mozilla
