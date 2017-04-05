/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERTEXTUREHOST_H
#define MOZILLA_GFX_RENDERTEXTUREHOST_H

#include "nsISupportsImpl.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace wr {

class RenderBufferTextureHost;
class RenderTextureHostOGL;

class RenderTextureHost
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RenderTextureHost)

public:
  RenderTextureHost();

  virtual bool Lock() = 0;
  virtual void Unlock() = 0;

  virtual gfx::IntSize GetSize() const = 0;
  virtual gfx::SurfaceFormat GetFormat() const = 0;

  virtual RenderBufferTextureHost* AsBufferTextureHost() { return nullptr; }
  virtual RenderTextureHostOGL* AsTextureHostOGL() { return nullptr; }

protected:
  virtual ~RenderTextureHost();
};

} // namespace wr
} // namespace mozilla

#endif // MOZILLA_GFX_RENDERTEXTUREHOST_H
