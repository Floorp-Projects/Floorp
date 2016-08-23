/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ShadowLayerUtilsX11_h
#define mozilla_layers_ShadowLayerUtilsX11_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/GfxMessageUtils.h"
#include "nsCOMPtr.h"                   // for already_AddRefed

#define MOZ_HAVE_SURFACEDESCRIPTORX11
#define MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS

typedef unsigned long XID;
typedef XID Drawable;

class gfxXlibSurface;

namespace IPC {
class Message;
}

namespace mozilla {
namespace layers {

struct SurfaceDescriptorX11 {
  SurfaceDescriptorX11()
  { }

  explicit SurfaceDescriptorX11(gfxXlibSurface* aSurf, bool aForwardGLX = false);

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
  MOZ_INIT_OUTSIDE_CTOR XID mFormat; // either a PictFormat or VisualID
  gfx::IntSize mSize;
  MOZ_INIT_OUTSIDE_CTOR Drawable mGLXPixmap; // used to prevent multiple bindings to the same GLXPixmap in-process
};

} // namespace layers
} // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::SurfaceDescriptorX11> {
  typedef mozilla::layers::SurfaceDescriptorX11 paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mId);
    WriteParam(aMsg, aParam.mSize);
    WriteParam(aMsg, aParam.mFormat);
    WriteParam(aMsg, aParam.mGLXPixmap);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult) {
    return (ReadParam(aMsg, aIter, &aResult->mId) &&
            ReadParam(aMsg, aIter, &aResult->mSize) &&
            ReadParam(aMsg, aIter, &aResult->mFormat) &&
            ReadParam(aMsg, aIter, &aResult->mGLXPixmap)
            );
  }
};

} // namespace IPC

#endif  // mozilla_layers_ShadowLayerUtilsX11_h
