/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_VERTEX_ARRAY_H_
#define WEBGL_VERTEX_ARRAY_H_

#include <vector>

#include "CacheInvalidator.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"
#include "WebGLVertexAttribData.h"

namespace mozilla {

class WebGLVertexArrayFake;
namespace webgl {
struct LinkedProgramInfo;
}

class WebGLVertexArray : public WebGLContextBoundObject,
                         public CacheInvalidator {
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(WebGLVertexArray, override)

 public:
  static WebGLVertexArray* Create(WebGLContext* webgl);

 protected:
  explicit WebGLVertexArray(WebGLContext* webgl);

 public:
  virtual void BindVertexArray() = 0;
  bool mHasBeenBound = false;

  bool ShouldCache() const {
    if (!mPreventCacheCooldown) return true;
    mPreventCacheCooldown -= 1;
    return false;
  }

  void InvalidateCaches() const {
    // The cost of creating and destroying caches is about 10x the cost of doing
    // the validation. We do want VAO caching to be the fast-path, but
    // cache-thrashing is too slow to do it naively.
    mPreventCacheCooldown = 10;

    CacheInvalidator::InvalidateCaches();
  }

 protected:
  std::vector<WebGLVertexAttribData> mAttribs;
  RefPtr<WebGLBuffer> mElementArrayBuffer;
  mutable uint32_t mPreventCacheCooldown = 0;

  friend class ScopedDrawHelper;
  friend class WebGLContext;
  friend class WebGLVertexArrayFake;
  friend class WebGL2Context;
  friend struct webgl::LinkedProgramInfo;
};

}  // namespace mozilla

#endif  // WEBGL_VERTEX_ARRAY_H_
