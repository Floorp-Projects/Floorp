/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SHAREDSURFACESMEMORYREPORT_H
#define MOZILLA_GFX_SHAREDSURFACESMEMORYREPORT_H

#include <cstdint>  // for uint32_t
#include <unordered_map>
#include "base/process.h"
#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/gfx/Point.h"  // for IntSize

namespace mozilla {
namespace layers {

class SharedSurfacesMemoryReport final {
 public:
  class SurfaceEntry final {
   public:
    base::ProcessId mCreatorPid;
    gfx::IntSize mSize;
    int32_t mStride;
    uint32_t mConsumers;
    bool mCreatorRef;
  };

  std::unordered_map<uint64_t, SurfaceEntry> mSurfaces;
};

}  // namespace layers
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::SharedSurfacesMemoryReport> {
  typedef mozilla::layers::SharedSurfacesMemoryReport paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mSurfaces);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mSurfaces);
  }
};

template <>
struct ParamTraits<mozilla::layers::SharedSurfacesMemoryReport::SurfaceEntry>
    : public PlainOldDataSerializer<
          mozilla::layers::SharedSurfacesMemoryReport::SurfaceEntry> {};

}  // namespace IPC

#endif
