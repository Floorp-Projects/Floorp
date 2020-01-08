/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_VERTEX_ARRAY_FAKE_H_
#define WEBGL_VERTEX_ARRAY_FAKE_H_

#include "WebGLVertexArray.h"

namespace mozilla {

class WebGLVertexArrayFake final : public WebGLVertexArray {
 public:
  explicit WebGLVertexArrayFake(WebGLContext* webgl);
  ~WebGLVertexArrayFake() override {}

  virtual void BindVertexArray() override;
};

}  // namespace mozilla

#endif  // WEBGL_VERTEX_ARRAY_FAKE_H_
