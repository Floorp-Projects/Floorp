/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_FencerUtils_h
#define IPC_FencerUtils_h

#include "ipc/IPCMessageUtils.h"

/**
 * FenceHandle is used for delivering Fence object via ipc.
 */
#if MOZ_WIDGET_GONK && ANDROID_VERSION >= 17
# include "mozilla/layers/FenceUtilsGonk.h"
#else
namespace mozilla {
namespace layers {

struct FenceHandleFromChild;

struct FenceHandle {
  FenceHandle() {}
  FenceHandle(const FenceHandleFromChild& aFenceHandle) {}
  bool operator==(const FenceHandle&) const { return false; }
  bool IsValid() const { return false; }
};

struct FenceHandleFromChild {
  FenceHandleFromChild() {}
  FenceHandleFromChild(const FenceHandle& aFence) {}
  bool operator==(const FenceHandle&) const { return false; }
  bool operator==(const FenceHandleFromChild&) const { return false; }
  bool IsValid() const { return false; }
};

} // namespace layers
} // namespace mozilla
#endif // MOZ_WIDGET_GONK && ANDROID_VERSION >= 17

namespace IPC {

#if MOZ_WIDGET_GONK && ANDROID_VERSION >= 17
#else
template <>
struct ParamTraits<mozilla::layers::FenceHandle> {
  typedef mozilla::layers::FenceHandle paramType;
  static void Write(Message*, const paramType&) {}
  static bool Read(const Message*, void**, paramType*) { return false; }
};

template <>
struct ParamTraits<mozilla::layers::FenceHandleFromChild> {
  typedef mozilla::layers::FenceHandleFromChild paramType;
  static void Write(Message*, const paramType&) {}
  static bool Read(const Message*, void**, paramType*) { return false; }
};
#endif // MOZ_WIDGET_GONK && ANDROID_VERSION >= 17

} // namespace IPC

#endif // IPC_FencerUtils_h
