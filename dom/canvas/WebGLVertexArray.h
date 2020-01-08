/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_VERTEX_ARRAY_H_
#define WEBGL_VERTEX_ARRAY_H_

#include <vector>

#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"

#include "CacheInvalidator.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"
#include "WebGLVertexAttribData.h"

namespace mozilla {

class WebGLVertexArrayFake;
namespace webgl {
struct LinkedProgramInfo;
}

class WebGLVertexArray : public WebGLRefCountedObject<WebGLVertexArray>,
                         public LinkedListElement<WebGLVertexArray>,
                         public CacheInvalidator {
 public:
  static WebGLVertexArray* Create(WebGLContext* webgl);

  void Delete();

  NS_INLINE_DECL_REFCOUNTING(WebGLVertexArray)

 protected:
  WebGLVertexArray(WebGLContext* webgl, GLuint name);
  virtual ~WebGLVertexArray();

  virtual void BindVertexArray() = 0;
  virtual void DeleteImpl() = 0;

 public:
  const GLuint mGLName;
  bool mHasBeenBound = false;

 protected:
  std::vector<WebGLVertexAttribData> mAttribs;
  WebGLRefPtr<WebGLBuffer> mElementArrayBuffer;

  friend class ScopedDrawHelper;
  friend class WebGLContext;
  friend class WebGLVertexArrayFake;
  friend class WebGL2Context;
  friend struct webgl::LinkedProgramInfo;
};

}  // namespace mozilla

#endif  // WEBGL_VERTEX_ARRAY_H_
