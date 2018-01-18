/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorANGLE.h"

#include "GLContext.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/HelpersD3D11.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/widget/CompositorWidget.h"

#include <d3d11.h>

namespace mozilla {
namespace wr {

/* static */ UniquePtr<RenderCompositor>
RenderCompositorANGLE::Create(RefPtr<widget::CompositorWidget>&& aWidget)
{
  UniquePtr<RenderCompositorANGLE> compositor = MakeUnique<RenderCompositorANGLE>(Move(aWidget));
  if (!compositor->Initialize()) {
    return nullptr;
  }
  return compositor;
}

RenderCompositorANGLE::RenderCompositorANGLE(RefPtr<widget::CompositorWidget>&& aWidget)
  : RenderCompositor(Move(aWidget))
{
}

RenderCompositorANGLE::~RenderCompositorANGLE()
{
}

bool
RenderCompositorANGLE::Initialize()
{
  mDevice = gfx::DeviceManagerDx::Get()->GetCompositorDevice();
  if (!mDevice) {
    gfxCriticalNote << "[D3D11] failed to get compositor device.";
    return false;
  }

  mDevice->GetImmediateContext(getter_AddRefs(mCtx));
  if (!mCtx) {
    gfxCriticalNote << "[D3D11] failed to get immediate context.";
    return false;
  }

  mSyncObject = layers::SyncObjectHost::CreateSyncObjectHost(mDevice);
  if (!mSyncObject->Init()) {
    // Some errors occur. Clear the mSyncObject here.
    // Then, there will be no texture synchronization.
    return false;
  }

  mGL = gl::GLContextProviderEGL::CreateForCompositorWidget(mWidget, true);
  if (!mGL || !mGL->IsANGLE()) {
    gfxCriticalNote << "Failed ANGLE GL context creation for WebRender: " << gfx::hexa(mGL.get());
    return false;
  }
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed GL context creation for WebRender: " << gfx::hexa(mGL.get());
    return false;
  }

  return true;
}

bool
RenderCompositorANGLE::Destroy()
{
  return true;
}

bool
RenderCompositorANGLE::BeginFrame()
{
  if (!mGL->MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

  if (mSyncObject) {
    // XXX: if the synchronization is failed, we should handle the device reset.
    mSyncObject->Synchronize();
  }
  return true;
}

void
RenderCompositorANGLE::EndFrame()
{
  InsertPresentWaitQuery();

  mGL->SwapBuffers();

  // Note: this waits on the query we inserted in the previous frame,
  // not the one we just inserted now. Example:
  //   Insert query #1
  //   Present #1
  //   (first frame, no wait)
  //   Insert query #2
  //   Present #2
  //   Wait for query #1.
  //   Insert query #3
  //   Present #3
  //   Wait for query #2.
  //
  // This ensures we're done reading textures before swapping buffers.
  WaitForPreviousPresentQuery();
}

void
RenderCompositorANGLE::Pause()
{
}

bool
RenderCompositorANGLE::Resume()
{
  return true;
}

LayoutDeviceIntSize
RenderCompositorANGLE::GetClientSize()
{
  return mWidget->GetClientSize();
}

void
RenderCompositorANGLE::InsertPresentWaitQuery()
{
  CD3D11_QUERY_DESC desc(D3D11_QUERY_EVENT);
  HRESULT hr = mDevice->CreateQuery(&desc, getter_AddRefs(mNextWaitForPresentQuery));
  if (FAILED(hr) || !mNextWaitForPresentQuery) {
    gfxWarning() << "Could not create D3D11_QUERY_EVENT: " << hexa(hr);
    return;
  }

  mCtx->End(mNextWaitForPresentQuery);
}

void
RenderCompositorANGLE::WaitForPreviousPresentQuery()
{
  if (mWaitForPresentQuery) {
    BOOL result;
    layers::WaitForGPUQuery(mDevice, mCtx, mWaitForPresentQuery, &result);
  }
  mWaitForPresentQuery = mNextWaitForPresentQuery.forget();
}


} // namespace wr
} // namespace mozilla
