/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GpuProcessD3D11QueryMap_H
#define MOZILLA_GFX_GpuProcessD3D11QueryMap_H

#include <d3d11.h>
#include <unordered_map>

#include "mozilla/gfx/2D.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace layers {

/**
 * A class to manage ID3D11Queries that is shared in GPU process.
 */
class GpuProcessD3D11QueryMap {
 public:
  static void Init();
  static void Shutdown();
  static GpuProcessD3D11QueryMap* Get() { return sInstance; }

  GpuProcessD3D11QueryMap();
  ~GpuProcessD3D11QueryMap();

  void Register(GpuProcessQueryId aQueryId, ID3D11Query* aQuery);
  void Unregister(GpuProcessQueryId aQueryId);

  RefPtr<ID3D11Query> GetQuery(GpuProcessQueryId aQueryId);

 private:
  mutable Monitor mMonitor MOZ_UNANNOTATED;

  std::unordered_map<GpuProcessQueryId, RefPtr<ID3D11Query>,
                     GpuProcessQueryId::HashFn>
      mD3D11QueriesById;

  static StaticAutoPtr<GpuProcessD3D11QueryMap> sInstance;
};

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_GpuProcessD3D11QueryMap_H */
