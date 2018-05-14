/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CANVASRENDERER_H
#define GFX_CANVASRENDERER_H

#include <stdint.h>                     // for uint32_t
#include "GLContextTypes.h"             // for GLContext
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxTypes.h"
#include "gfxPlatform.h"                // for gfxImageFormat
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc

namespace mozilla {
namespace layers {

class AsyncCanvasRenderer;
class ClientCanvasRenderer;
class CopyableCanvasRenderer;
class PersistentBufferProvider;
class WebRenderCanvasRendererAsync;

struct CanvasInitializeData {
  CanvasInitializeData()
    : mBufferProvider(nullptr)
    , mGLContext(nullptr)
    , mRenderer(nullptr)
    , mPreTransCallback(nullptr)
    , mPreTransCallbackData(nullptr)
    , mDidTransCallback(nullptr)
    , mDidTransCallbackData(nullptr)
    , mFrontbufferGLTex(0)
    , mSize(0,0)
    , mHasAlpha(false)
    , mIsGLAlphaPremult(true)
  { }

  // One of these three must be specified for Canvas2D, but never more than one
  PersistentBufferProvider* mBufferProvider; // A BufferProvider for the Canvas contents
  mozilla::gl::GLContext* mGLContext; // or this, for GL.
  AsyncCanvasRenderer* mRenderer; // or this, for OffscreenCanvas

  typedef void (* TransactionCallback)(void* closureData);
  TransactionCallback mPreTransCallback;
  void* mPreTransCallbackData;
  TransactionCallback mDidTransCallback;
  void* mDidTransCallbackData;

  // Frontbuffer override
  uint32_t mFrontbufferGLTex;

  // The size of the canvas content
  gfx::IntSize mSize;

  // Whether the canvas drawingbuffer has an alpha channel.
  bool mHasAlpha;

  // Whether mGLContext contains data that is alpha-premultiplied.
  bool mIsGLAlphaPremult;
};

// Based class which used for canvas rendering. There are many derived classes for
// different purposes. such as:
//
// CopyableCanvasRenderer provides a utility which readback canvas content to a
// SourceSurface. BasicCanvasLayer uses CopyableCanvasRenderer.
//
// ShareableCanvasRenderer provides IPC capabilities that allow sending canvas
// content over IPC. This is pure virtual class because the IPC handling is
// different in different LayerManager. So that we have following classes inherite
// ShareableCanvasRenderer.
//
// ClientCanvasRenderer inherites ShareableCanvasRenderer and be used in
// ClientCanvasLayer.
// WebRenderCanvasRenderer inherites ShareableCanvasRenderer and provides all
// functionality that WebRender uses.
// WebRenderCanvasRendererAsync inherites WebRenderCanvasRenderer and be used in
// layers-free mode of WebRender.
//
// class diagram shows below:
//
//                       +--------------+
//                       |CanvasRenderer|
//                       +-------+------+
//                               ^
//                               |
//                   +----------------------+
//                   |CopyableCanvasRenderer|
//                   +----------------------+
//                               ^
//                               |
//                   +-----------+-----------+
//                   |ShareableCanvasRenderer|
//                   +-----+-----------------+
//                         ^      ^
//           +-------------+      +-------+
//           |                            |
// +--------------------+       +---------+-------------+
// |ClientCanvasRenderer|       |WebRenderCanvasRenderer|
// +--------------------+       +-----------+-----------+
//                                          ^
//                                          |
//                           +-------------+--------------+
//                           |WebRenderCanvasRendererAsync|
//                           +----------------------------+
class CanvasRenderer
{
public:
  CanvasRenderer();
  virtual ~CanvasRenderer();

public:
  virtual void Initialize(const CanvasInitializeData& aData);
  virtual bool IsDataValid(const CanvasInitializeData& aData) { return true; }

  virtual void ClearCachedResources() { }
  virtual void Destroy() { }

  const gfx::IntSize& GetSize() const { return mSize; }

  void SetDirty() { mDirty = true; }
  void ResetDirty() { mDirty = false; }
  bool IsDirty() const { return mDirty; }

  virtual CopyableCanvasRenderer* AsCopyableCanvasRenderer() { return nullptr; }
  virtual ClientCanvasRenderer* AsClientCanvasRenderer() { return nullptr; }
  virtual WebRenderCanvasRendererAsync* AsWebRenderCanvasRendererAsync() { return nullptr; }

protected:
  void FirePreTransactionCallback()
  {
    if (mPreTransCallback) {
      mPreTransCallback(mPreTransCallbackData);
    }
  }

  void FireDidTransactionCallback()
  {
    if (mDidTransCallback) {
      mDidTransCallback(mDidTransCallbackData);
    }
  }

  typedef void (* TransactionCallback)(void* closureData);
  TransactionCallback mPreTransCallback;
  void* mPreTransCallbackData;
  TransactionCallback mDidTransCallback;
  void* mDidTransCallbackData;
  gfx::IntSize mSize;

private:
  /**
   * Set to true in Updated(), cleared during a transaction.
   */
  bool mDirty;
};

} // namespace layers
} // namespace mozilla

#endif
