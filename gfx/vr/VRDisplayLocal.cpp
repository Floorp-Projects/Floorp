/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRDisplayLocal.h"
#include "gfxPrefs.h"
#include "gfxVR.h"
#include "ipc/VRLayerParent.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/dom/GamepadBinding.h" // For GamepadMappingType
#include "VRThread.h"

#if defined(XP_WIN)

#include <d3d11.h>
#include "gfxWindowsPlatform.h"
#include "../layers/d3d11/CompositorD3D11.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/TextureD3D11.h"

#elif defined(XP_MACOSX)

#include "mozilla/gfx/MacIOSurface.h"

#endif

#if defined(MOZ_WIDGET_ANDROID)
#include "mozilla/layers/CompositorThread.h"
#endif // defined(MOZ_WIDGET_ANDROID)

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

VRDisplayLocal::VRDisplayLocal(VRDeviceType aType)
  : VRDisplayHost(aType)
{
  MOZ_COUNT_CTOR_INHERITED(VRDisplayLocal, VRDisplayHost);
}

VRDisplayLocal::~VRDisplayLocal()
{
  MOZ_COUNT_DTOR_INHERITED(VRDisplayLocal, VRDisplayHost);
}

bool
VRDisplayLocal::SubmitFrame(const layers::SurfaceDescriptor &aTexture,
                            uint64_t aFrameId,
                            const gfx::Rect& aLeftEyeRect,
                            const gfx::Rect& aRightEyeRect)
{
#if !defined(MOZ_WIDGET_ANDROID)
  MOZ_ASSERT(mSubmitThread->GetThread() == NS_GetCurrentThread());
#endif // !defined(MOZ_WIDGET_ANDROID)

  switch (aTexture.type()) {

#if defined(XP_WIN)
    case SurfaceDescriptor::TSurfaceDescriptorD3D10: {
      if (!CreateD3DObjects()) {
        return false;
      }
      const SurfaceDescriptorD3D10& surf = aTexture.get_SurfaceDescriptorD3D10();
      RefPtr<ID3D11Texture2D> dxTexture;
      HRESULT hr = mDevice->OpenSharedResource((HANDLE)surf.handle(),
        __uuidof(ID3D11Texture2D),
        (void**)(ID3D11Texture2D**)getter_AddRefs(dxTexture));
      if (FAILED(hr) || !dxTexture) {
        NS_WARNING("Failed to open shared texture");
        return false;
      }

      // Similar to LockD3DTexture in TextureD3D11.cpp
      RefPtr<IDXGIKeyedMutex> mutex;
      dxTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
      if (mutex) {
        HRESULT hr = mutex->AcquireSync(0, 1000);
        if (hr == WAIT_TIMEOUT) {
          gfxDevCrash(LogReason::D3DLockTimeout) << "D3D lock mutex timeout";
        }
        else if (hr == WAIT_ABANDONED) {
          gfxCriticalNote << "GFX: D3D11 lock mutex abandoned";
        }
        if (FAILED(hr)) {
          NS_WARNING("Failed to lock the texture");
          return false;
        }
      }
      bool success = SubmitFrame(dxTexture, surf.size(),
                                 aLeftEyeRect, aRightEyeRect);
      if (mutex) {
        HRESULT hr = mutex->ReleaseSync(0);
        if (FAILED(hr)) {
          NS_WARNING("Failed to unlock the texture");
        }
      }
      return success;
    }
#elif defined(XP_MACOSX)
    case SurfaceDescriptor::TSurfaceDescriptorMacIOSurface: {
      const auto& desc = aTexture.get_SurfaceDescriptorMacIOSurface();
      RefPtr<MacIOSurface> surf = MacIOSurface::LookupSurface(desc.surfaceId(),
                                                              desc.scaleFactor(),
                                                              !desc.isOpaque());
      if (!surf) {
        NS_WARNING("VRDisplayHost::SubmitFrame failed to get a MacIOSurface");
        return false;
      }
      IntSize texSize = gfx::IntSize(surf->GetDevicePixelWidth(),
                                     surf->GetDevicePixelHeight());
      if (!SubmitFrame(surf, texSize, aLeftEyeRect, aRightEyeRect)) {
        return false;
      }
      return true;
    }
#elif defined(MOZ_WIDGET_ANDROID)
    case SurfaceDescriptor::TSurfaceTextureDescriptor: {
      const SurfaceTextureDescriptor& desc = aTexture.get_SurfaceTextureDescriptor();
      if (!SubmitFrame(desc, aLeftEyeRect, aRightEyeRect)) {
        return false;
      }
      return true;
    }
#endif
    default: {
      NS_WARNING("Unsupported SurfaceDescriptor type for VR layer texture");
      return false;
    }
  }
}
