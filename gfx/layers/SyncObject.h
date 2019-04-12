/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_LAYERS_SYNCOBJECT_H
#define MOZILLA_GFX_LAYERS_SYNCOBJECT_H

#include "mozilla/RefCounted.h"

struct ID3D11Device;

namespace mozilla {
namespace layers {

#ifdef XP_WIN
typedef void* SyncHandle;
#else
typedef uintptr_t SyncHandle;
#endif  // XP_WIN

class SyncObjectHost : public RefCounted<SyncObjectHost> {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SyncObjectHost)
  virtual ~SyncObjectHost() = default;

  static already_AddRefed<SyncObjectHost> CreateSyncObjectHost(
#ifdef XP_WIN
      ID3D11Device* aDevice = nullptr
#endif
  );

  virtual bool Init() = 0;

  virtual SyncHandle GetSyncHandle() = 0;

  // Return false for failed synchronization.
  virtual bool Synchronize(bool aFallible = false) = 0;

 protected:
  SyncObjectHost() {}
};

class SyncObjectClient : public external::AtomicRefCounted<SyncObjectClient> {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SyncObjectClient)
  virtual ~SyncObjectClient() = default;

  static already_AddRefed<SyncObjectClient> CreateSyncObjectClient(
      SyncHandle aHandle
#ifdef XP_WIN
      ,
      ID3D11Device* aDevice = nullptr
#endif
  );

  enum class SyncType {
    D3D11,
  };

  virtual SyncType GetSyncType() = 0;

  // Return false for failed synchronization.
  virtual bool Synchronize(bool aFallible = false) = 0;

  virtual bool IsSyncObjectValid() = 0;

 protected:
  SyncObjectClient() {}
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_LAYERS_SYNCOBJECT_H
