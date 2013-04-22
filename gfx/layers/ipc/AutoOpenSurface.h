/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AutoOpenSurface_h
#define mozilla_layers_AutoOpenSurface_h 1

#include "base/basictypes.h"

#include "gfxASurface.h"
#include "mozilla/layers/PLayers.h"
#include "ShadowLayers.h"

namespace mozilla {
namespace layers {

/**
 * Some surface types can be fairly expensive to open.  This helper
 * tries to put off opening surfaces as long as it can, until
 * ahsolutely necessary.  And after being forced to open, it remembers
 * the mapping so it doesn't need to be redone.
 */
class MOZ_STACK_CLASS AutoOpenSurface
{
public:
  typedef gfxASurface::gfxContentType gfxContentType;

  /** |aDescriptor| must be valid while AutoOpenSurface is
   * in scope. */
  AutoOpenSurface(OpenMode aMode, const SurfaceDescriptor& aDescriptor);

  ~AutoOpenSurface();

  /**
   * These helpers do not necessarily need to open the descriptor to
   * return an answer.
   */
  gfxContentType ContentType();
  gfxIntSize Size();

  /** This can't escape the scope of AutoOpenSurface. */
  gfxASurface* Get();

  /**
   * This can't escape the scope of AutoOpenSurface.
   *
   * This method is currently just a convenience wrapper around
   * gfxASurface::GetAsImageSurface() --- it returns a valid surface
   * exactly when this->Get()->GetAsImageSurface() would.  Clients
   * that need guaranteed (fast) ImageSurfaces should allocate the
   * underlying descriptor with capability MAP_AS_IMAGE_SURFACE, in
   * which case this helper is guaranteed to succeed.
   */
  gfxImageSurface* GetAsImage();


private:
  SurfaceDescriptor mDescriptor;
  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<gfxImageSurface> mSurfaceAsImage;
  OpenMode mMode;

  AutoOpenSurface(const AutoOpenSurface&) MOZ_DELETE;
  AutoOpenSurface& operator=(const AutoOpenSurface&) MOZ_DELETE;
};

} // namespace layers
} // namespace mozilla

#endif // ifndef mozilla_layers_AutoOpenSurface_h
