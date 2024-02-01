/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GpuProcessD3D11QueryMap.h"

#include "mozilla/webrender/RenderThread.h"

namespace mozilla {

namespace layers {

StaticAutoPtr<GpuProcessD3D11QueryMap> GpuProcessD3D11QueryMap::sInstance;

/* static */
void GpuProcessD3D11QueryMap::Init() {
  MOZ_ASSERT(XRE_IsGPUProcess());
  sInstance = new GpuProcessD3D11QueryMap();
}

/* static */
void GpuProcessD3D11QueryMap::Shutdown() {
  MOZ_ASSERT(XRE_IsGPUProcess());
  sInstance = nullptr;
}

GpuProcessD3D11QueryMap::GpuProcessD3D11QueryMap()
    : mMonitor("GpuProcessD3D11QueryMap::mMonitor") {}

GpuProcessD3D11QueryMap::~GpuProcessD3D11QueryMap() {}

void GpuProcessD3D11QueryMap::Register(GpuProcessQueryId aQueryId,
                                       ID3D11Query* aQuery) {
  MOZ_RELEASE_ASSERT(aQuery);

  MonitorAutoLock lock(mMonitor);
  mD3D11QueriesById[aQueryId] = aQuery;
}

void GpuProcessD3D11QueryMap::Unregister(GpuProcessQueryId aQueryId) {
  MonitorAutoLock lock(mMonitor);

  auto it = mD3D11QueriesById.find(aQueryId);
  if (it == mD3D11QueriesById.end()) {
    return;
  }
  mD3D11QueriesById.erase(it);
}

RefPtr<ID3D11Query> GpuProcessD3D11QueryMap::GetQuery(
    GpuProcessQueryId aQueryId) {
  MOZ_ASSERT(wr::RenderThread::IsInRenderThread());

  MonitorAutoLock lock(mMonitor);

  auto it = mD3D11QueriesById.find(aQueryId);
  if (it == mD3D11QueriesById.end()) {
    return nullptr;
  }

  return it->second;
}

}  // namespace layers
}  // namespace mozilla
