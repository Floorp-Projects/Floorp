/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SHAREDSURFACESPARENT_H
#define MOZILLA_GFX_SHAREDSURFACESPARENT_H

#include <stdint.h>                     // for uint32_t
#include "mozilla/Attributes.h"         // for override
#include "mozilla/StaticPtr.h"          // for StaticAutoPtr
#include "mozilla/RefPtr.h"             // for already_AddRefed
#include "mozilla/ipc/SharedMemory.h"   // for SharedMemory, etc
#include "mozilla/gfx/2D.h"             // for SurfaceFormat
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/LayersSurfaces.h"    // for SurfaceDescriptorShared
#include "mozilla/webrender/WebRenderTypes.h" // for wr::ExternalImageId
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace gfx {
class DataSourceSurface;
class SourceSurfaceSharedData;
class SourceSurfaceSharedDataWrapper;
} // namespace gfx

namespace layers {

class SharedSurfacesChild;

class SharedSurfacesParent final
{
public:
  static void Initialize();
  static void Shutdown();

  // Get without increasing the consumer count.
  static already_AddRefed<gfx::DataSourceSurface>
  Get(const wr::ExternalImageId& aId);

  // Get but also increase the consumer count. Must call Release after finished.
  static already_AddRefed<gfx::DataSourceSurface>
  Acquire(const wr::ExternalImageId& aId);

  static bool Release(const wr::ExternalImageId& aId);

  static void Add(const wr::ExternalImageId& aId,
                  const SurfaceDescriptorShared& aDesc,
                  base::ProcessId aPid);

  static void Remove(const wr::ExternalImageId& aId);

  static void DestroyProcess(base::ProcessId aPid);

  ~SharedSurfacesParent();

private:
  friend class SharedSurfacesChild;

  SharedSurfacesParent();

  static void AddSameProcess(const wr::ExternalImageId& aId,
                             gfx::SourceSurfaceSharedData* aSurface);
  static void RemoveSameProcess(const wr::ExternalImageId& aId);

  static StaticAutoPtr<SharedSurfacesParent> sInstance;

  nsRefPtrHashtable<nsUint64HashKey, gfx::SourceSurfaceSharedDataWrapper> mSurfaces;
};

} // namespace layers
} // namespace mozilla

#endif
