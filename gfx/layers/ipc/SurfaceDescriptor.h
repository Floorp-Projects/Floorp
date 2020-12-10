/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_SurfaceDescriptor_h
#define IPC_SurfaceDescriptor_h

#if defined(XP_MACOSX)
#  define MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS
#endif

#if defined(MOZ_X11)
#  include "mozilla/AlreadyAddRefed.h"
#  include "mozilla/gfx/Point.h"

#  define MOZ_HAVE_SURFACEDESCRIPTORX11
#  define MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS

typedef unsigned long XID;
typedef XID Drawable;

class gfxXlibSurface;

namespace mozilla {
namespace layers {

struct SurfaceDescriptorX11 {
  SurfaceDescriptorX11() = default;

  explicit SurfaceDescriptorX11(gfxXlibSurface* aSurf,
                                bool aForwardGLX = false);

  SurfaceDescriptorX11(Drawable aDrawable, XID aFormatID,
                       const gfx::IntSize& aSize);

  // Default copy ctor and operator= are OK

  bool operator==(const SurfaceDescriptorX11& aOther) const {
    // Define == as two descriptors having the same XID for now,
    // ignoring size and render format.  If the two indeed refer to
    // the same valid XID, then size/format are "actually" the same
    // anyway, regardless of the values of the fields in
    // SurfaceDescriptorX11.
    return mId == aOther.mId;
  }

  already_AddRefed<gfxXlibSurface> OpenForeign() const;

  MOZ_INIT_OUTSIDE_CTOR Drawable mId;
  MOZ_INIT_OUTSIDE_CTOR XID mFormat;  // either a PictFormat or VisualID
  MOZ_INIT_OUTSIDE_CTOR gfx::IntSize mSize;
  MOZ_INIT_OUTSIDE_CTOR Drawable
      mGLXPixmap;  // used to prevent multiple bindings to the same GLXPixmap
                   // in-process
};

}  // namespace layers
}  // namespace mozilla
#else
namespace mozilla {
namespace layers {
struct SurfaceDescriptorX11 {
  bool operator==(const SurfaceDescriptorX11&) const { return false; }
};
}  // namespace layers
}  // namespace mozilla
#endif

#endif  // IPC_SurfaceDescriptor_h
