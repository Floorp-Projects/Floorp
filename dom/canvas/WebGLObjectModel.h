/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLOBJECTMODEL_H_
#define WEBGLOBJECTMODEL_H_

#include "mozilla/RefCounted.h"
#include "mozilla/WeakPtr.h"
#include "WebGLTypes.h"

namespace mozilla {

class WebGLContext;

////

// This class is a mixin for objects that are tied to a specific
// context (which is to say, all of them).  They provide initialization
// as well as comparison with the current context.
class WebGLContextBoundObject : public VRefCounted {
 public:
  const WeakPtr<WebGLContext> mContext;

  explicit WebGLContextBoundObject(WebGLContext* webgl);

 private:
  friend class HostWebGLContext;
};

////

// this class is a mixin for GL objects that have dimensions
// that we need to track.
class WebGLRectangleObject {
 public:
  WebGLRectangleObject() : mWidth(0), mHeight(0) {}

  WebGLRectangleObject(GLsizei width, GLsizei height)
      : mWidth(width), mHeight(height) {}

  GLsizei Width() const { return mWidth; }
  void width(GLsizei value) { mWidth = value; }

  GLsizei Height() const { return mHeight; }
  void height(GLsizei value) { mHeight = value; }

  void setDimensions(GLsizei width, GLsizei height) {
    mWidth = width;
    mHeight = height;
  }

  void setDimensions(WebGLRectangleObject* rect) {
    if (rect) {
      mWidth = rect->Width();
      mHeight = rect->Height();
    } else {
      mWidth = 0;
      mHeight = 0;
    }
  }

  bool HasSameDimensionsAs(const WebGLRectangleObject& other) const {
    return Width() == other.Width() && Height() == other.Height();
  }

 protected:
  GLsizei mWidth;
  GLsizei mHeight;
};

}  // namespace mozilla

#endif
