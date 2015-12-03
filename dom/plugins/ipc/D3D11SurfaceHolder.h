/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_dom_plugins_ipc_D3D11SurfaceHolder_h__
#define _include_dom_plugins_ipc_D3D11SurfaceHolder_h__

#include "ipc/IPCMessageUtils.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
namespace layers {
class D3D11ShareHandleImage;
class TextureClient;
} // namespace layers

namespace plugins {

class D3D11SurfaceHolder
{
public:
  D3D11SurfaceHolder(ID3D11Texture2D* back, gfx::SurfaceFormat format, const gfx::IntSize& size);

  NS_INLINE_DECL_REFCOUNTING(D3D11SurfaceHolder);

  bool IsValid();
  bool CopyToTextureClient(layers::TextureClient* aClient);

  gfx::SurfaceFormat GetFormat() const {
    return mFormat;
  }
  const gfx::IntSize& GetSize() const {
    return mSize;
  }

private:
  ~D3D11SurfaceHolder();

private:
  RefPtr<ID3D11Device> mDevice11;
  RefPtr<ID3D11Texture2D> mBack;
  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;
};

} // namespace plugins
} // namespace mozilla

#endif // _include_dom_plugins_ipc_D3D11nSurfaceHolder_h__
