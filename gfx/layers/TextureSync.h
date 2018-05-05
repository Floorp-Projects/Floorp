/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_TEXTURESYNC_H
#define MOZILLA_LAYERS_TEXTURESYNC_H

#include "base/process.h"

#include "nsTArray.h"
#include "mozilla/layers/TextureSourceProvider.h"

#include "SharedMemory.h"

class MachReceiveMessage;

namespace mozilla {
namespace ipc {
  struct MemoryPorts;
} // namespace ipc

namespace layers {

class TextureSync
{
public:
  static void RegisterTextureSourceProvider(layers::TextureSourceProvider* aTextureSourceProvider);
  static void UnregisterTextureSourceProvider(layers::TextureSourceProvider* aTextureSourceProvider);
  static void DispatchCheckTexturesForUnlock();
  static void HandleWaitForTexturesMessage(MachReceiveMessage* rmsg, ipc::MemoryPorts* ports);
  static void UpdateTextureLocks(base::ProcessId aProcessId);
  static bool WaitForTextures(base::ProcessId aProcessId, const nsTArray<uint64_t>& aTextureIds);
  static void SetTexturesLocked(base::ProcessId aProcessId, const nsTArray<uint64_t>& aTextureIds);
  static void SetTexturesUnlocked(base::ProcessId aProcessId, const nsTArray<uint64_t>& aTextureIds);
  static void Shutdown();
  static void CleanupForPid(base::ProcessId aProcessId);
};

} // namespace layers
} // namespace mozilla

#endif