/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLSync.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLSync::WebGLSync(WebGLContext* webgl, GLenum condition, GLbitfield flags)
    : WebGLContextBoundObject(webgl),
      mGLName(mContext->gl->fFenceSync(condition, flags)),
      mFenceId(mContext->mNextFenceId) {
  mContext->mNextFenceId += 1;
}

WebGLSync::~WebGLSync() {
  if (!mContext) return;
  mContext->gl->fDeleteSync(mGLName);
}

void WebGLSync::MarkSignaled() const {
  if (mContext->mCompletedFenceId < mFenceId) {
    mContext->mCompletedFenceId = mFenceId;
  }
}

}  // namespace mozilla
