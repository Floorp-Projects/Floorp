/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CANVASRENDERER_H
#define GFX_CANVASRENDERER_H

#include <memory>            // for weak_ptr
#include <stdint.h>          // for uint32_t
#include "GLContextTypes.h"  // for GLContext
#include "gfxContext.h"      // for gfxContext, etc
#include "gfxTypes.h"
#include "gfxPlatform.h"          // for gfxImageFormat
#include "mozilla/Assertions.h"   // for MOZ_ASSERT, etc
#include "mozilla/Preferences.h"  // for Preferences
#include "mozilla/RefPtr.h"       // for RefPtr
#include "mozilla/gfx/2D.h"       // for DrawTarget
#include "mozilla/mozalloc.h"     // for operator delete, etc
#include "mozilla/WeakPtr.h"      // for WeakPtr
#include "nsISupportsImpl.h"      // for MOZ_COUNT_CTOR, etc
#include "nsICanvasRenderingContextInternal.h"

namespace mozilla {
namespace layers {

class KnowsCompositor;
class PersistentBufferProvider;
class WebRenderCanvasRendererAsync;

TextureType TexTypeForWebgl(KnowsCompositor*);

struct CanvasRendererData final {
  CanvasRendererData();
  ~CanvasRendererData();

  WeakPtr<nsICanvasRenderingContextInternal> mContext;

  // The size of the canvas content
  gfx::IntSize mSize = {0, 0};

  bool mDoPaintCallbacks = false;
  bool mIsOpaque = true;
  bool mIsAlphaPremult = true;

  gl::OriginPos mOriginPos = gl::OriginPos::TopLeft;

  nsICanvasRenderingContextInternal* GetContext() const {
    return mContext.get();
  }
};

// Based class which used for canvas rendering. There are many derived classes
// for different purposes. such as:
//
// ShareableCanvasRenderer provides IPC capabilities that allow sending canvas
// content over IPC. This is pure virtual class because the IPC handling is
// different in different LayerManager. So that we have following classes
// inherit ShareableCanvasRenderer.
//
// WebRenderCanvasRenderer inherits ShareableCanvasRenderer and provides all
// functionality that WebRender uses.
// WebRenderCanvasRendererAsync inherits WebRenderCanvasRenderer and be used in
// layers-free mode of WebRender.
//
// class diagram:
//
//                       +--------------+
//                       |CanvasRenderer|
//                       +-------+------+
//                               ^
//                               |
//                   +-----------+-----------+
//                   |ShareableCanvasRenderer|
//                   +-----+-----------------+
//                               ^
//                               |
//                   +-----------+-----------+
//                   |WebRenderCanvasRenderer|
//                   +-----------+-----------+
//                               ^
//                               |
//                 +-------------+--------------+
//                 |WebRenderCanvasRendererAsync|
//                 +----------------------------+

class BorrowedSourceSurface final {
 public:
  const WeakPtr<PersistentBufferProvider> mReturnTo;
  const RefPtr<gfx::SourceSurface> mSurf;  /// non-null

  BorrowedSourceSurface(PersistentBufferProvider*, RefPtr<gfx::SourceSurface>);
  ~BorrowedSourceSurface();
};

// -

class CanvasRenderer : public RefCounted<CanvasRenderer> {
  friend class CanvasRendererSourceSurface;

 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(CanvasRenderer)

 private:
  bool mDirty = false;

 protected:
  CanvasRendererData mData;

 public:
  explicit CanvasRenderer();
  virtual ~CanvasRenderer();

 public:
  virtual void Initialize(const CanvasRendererData&);
  virtual bool IsDataValid(const CanvasRendererData&) const;

  virtual void ClearCachedResources() {}
  virtual void DisconnectClient() {}

  const gfx::IntSize& GetSize() const { return mData.mSize; }
  bool IsOpaque() const { return mData.mIsOpaque; }
  bool YIsDown() const { return mData.mOriginPos == gl::OriginPos::TopLeft; }

  void SetDirty() { mDirty = true; }
  void ResetDirty() { mDirty = false; }
  bool IsDirty() const { return mDirty; }

  virtual WebRenderCanvasRendererAsync* AsWebRenderCanvasRendererAsync() {
    return nullptr;
  }

  std::shared_ptr<BorrowedSourceSurface> BorrowSnapshot(
      bool requireAlphaPremult = true) const;

  void FirePreTransactionCallback() const;
  void FireDidTransactionCallback() const;
};

}  // namespace layers
}  // namespace mozilla

#endif
