/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_DISPLAY_LOCAL_H
#define GFX_VR_DISPLAY_LOCAL_H

#include "gfxVR.h"
#include "VRDisplayHost.h"

#if defined(XP_WIN)
#include <d3d11_1.h>
#elif defined(XP_MACOSX)
class MacIOSurface;
#endif

namespace mozilla {
namespace gfx {
class VRThread;

class VRDisplayLocal : public VRDisplayHost
{
public:

#if defined(XP_WIN)
  // Subclasses should override this SubmitFrame function.
  // Returns true if the SubmitFrame call will block as necessary
  // to control timing of the next frame and throttle the render loop
  // for the needed framerate.
  virtual bool SubmitFrame(ID3D11Texture2D* aSource,
                           const IntSize& aSize,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) = 0;
#elif defined(XP_MACOSX)
  virtual bool SubmitFrame(MacIOSurface* aMacIOSurface,
                           const IntSize& aSize,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) = 0;
#elif defined(MOZ_WIDGET_ANDROID)
  virtual bool SubmitFrame(const mozilla::layers::SurfaceTextureDescriptor& aSurface,
                           const gfx::Rect& aLeftEyeRect,
                           const gfx::Rect& aRightEyeRect) = 0;
#endif

protected:
  explicit VRDisplayLocal(VRDeviceType aType);
  virtual ~VRDisplayLocal();

private:
  bool SubmitFrame(const layers::SurfaceDescriptor& aTexture,
                   uint64_t aFrameId,
                   const gfx::Rect& aLeftEyeRect,
                   const gfx::Rect& aRightEyeRect) final;
};

} // namespace gfx
} // namespace mozilla

#endif // GFX_VR_DISPLAY_LOCAL_H
