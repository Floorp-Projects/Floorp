/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexArray.h"

namespace mozilla {

void WebGLContext::BindVertexArray(WebGLVertexArray* array) {
  FuncScope funcScope(*this, "bindVertexArray");
  if (IsContextLost()) return;
  funcScope.mBindFailureGuard = true;

  if (array && !ValidateObject("array", *array)) return;

  if (array == nullptr) {
    array = mDefaultVertexArray;
  }

  array->BindVertexArray();

  MOZ_ASSERT(mBoundVertexArray == array);
  if (mBoundVertexArray) {
    mBoundVertexArray->mHasBeenBound = true;
  }

  funcScope.mBindFailureGuard = false;
}

RefPtr<WebGLVertexArray> WebGLContext::CreateVertexArray() {
  const FuncScope funcScope(*this, "createVertexArray");
  if (IsContextLost()) return nullptr;

  return WebGLVertexArray::Create(this);
}

}  // namespace mozilla
