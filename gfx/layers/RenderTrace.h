/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a general tool that will let you visualize platform operation.
// Currently used for the layer system, the general syntax allows this
// tools to be adapted to trace other operations.
//
// For the front end see: https://github.com/staktrace/rendertrace

// Uncomment this line to enable RENDERTRACE
//#define MOZ_RENDERTRACE

#ifndef GFX_RENDERTRACE_H
#define GFX_RENDERTRACE_H

#include "nsRect.h"
#include "mozilla/gfx/Matrix.h"

namespace mozilla {
namespace layers {

class Layer;

void RenderTraceLayers(Layer *aLayer, const char *aColor, const gfx::Matrix4x4 aRootTransform = gfx::Matrix4x4(), bool aReset = true);

void RenderTraceInvalidateStart(Layer *aLayer, const char *aColor, const nsIntRect aRect);
void RenderTraceInvalidateEnd(Layer *aLayer, const char *aColor);

void renderTraceEventStart(const char *aComment, const char *aColor);
void renderTraceEventEnd(const char *aComment, const char *aColor);
void renderTraceEventEnd(const char *aColor);

struct RenderTraceScope {
public:
  RenderTraceScope(const char *aComment, const char *aColor)
    : mComment(aComment)
    , mColor(aColor)
  {
    renderTraceEventStart(mComment, mColor);
  }
  ~RenderTraceScope() {
    renderTraceEventEnd(mComment, mColor);
  }
private:
  const char *mComment;
  const char *mColor;
};

#ifndef MOZ_RENDERTRACE
inline void RenderTraceLayers(Layer *aLayer, const char *aColor, const gfx::Matrix4x4 aRootTransform, bool aReset)
{}

inline void RenderTraceInvalidateStart(Layer *aLayer, const char *aColor, const nsIntRect aRect)
{}

inline void RenderTraceInvalidateEnd(Layer *aLayer, const char *aColor)
{}

inline void renderTraceEventStart(const char *aComment, const char *aColor)
{}

inline void renderTraceEventEnd(const char *aComment, const char *aColor)
{}

inline void renderTraceEventEnd(const char *aColor)
{}

#endif // MOZ_RENDERTRACE

}
}

#endif //GFX_RENDERTRACE_H
