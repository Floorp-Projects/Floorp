/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERTEXTUREHOST_H
#define MOZILLA_GFX_RENDERTEXTUREHOST_H

#include "nsISupportsImpl.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

namespace gl {
class GLContext;
}

namespace wr {

class RenderBufferTextureHost;
class RenderTextureHostOGL;

class RenderTextureHost
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RenderTextureHost)

public:
  RenderTextureHost();

  virtual wr::WrExternalImage Lock(uint8_t aChannelIndex, gl::GLContext* aGL) = 0;
  virtual void Unlock() = 0;
  virtual void ClearCachedResources() {}
protected:
  virtual ~RenderTextureHost();
};

} // namespace wr
} // namespace mozilla

#endif // MOZILLA_GFX_RENDERTEXTUREHOST_H
