/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_WebGLVertexArrayObject_h
#define mozilla_dom_WebGLVertexArrayObject_h

#include "WebGLVertexArrayGL.h"

namespace mozilla {
namespace dom {

/**
 * This class implements the DOM bindings for WebGL 2 VAO.
 *
 * This exists to so the object returned from gl.createVertexArray()
 * is an instance of WebGLVertexArrayObject (to match the WebGL 2
 * spec.)
 */
class WebGLVertexArrayObject final
  : public WebGLVertexArrayGL
{
public:
  static WebGLVertexArray* Create(WebGLContext* webgl);

  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

private:
  explicit WebGLVertexArrayObject(WebGLContext* webgl)
    : WebGLVertexArrayGL(webgl)
  { }

  ~WebGLVertexArrayObject() {
    DeleteOnce();
  }
};

} // namespace dom
} // namespace mozilla

#endif // !mozilla_dom_WebGLVertexArrayObject_h
