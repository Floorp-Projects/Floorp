/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderManagerChild_h
#define include_dom_media_ipc_RemoteDecoderManagerChild_h
#include "mozilla/PRemoteDecoderManagerChild.h"

namespace mozilla {

class RemoteDecoderManagerChild final : public PRemoteDecoderManagerChild {
  friend class PRemoteDecoderManagerChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteDecoderManagerChild)

  // Can only be called from the manager thread
  static RemoteDecoderManagerChild* GetSingleton();

  // Can be called from any thread.
  static nsIThread* GetManagerThread();
  static AbstractThread* GetManagerAbstractThread();

  // Main thread only
  static void InitForContent(
      Endpoint<PRemoteDecoderManagerChild>&& aVideoManager);
  static void Shutdown();

  bool CanSend();

 protected:
  void InitIPDL();

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeallocPRemoteDecoderManagerChild() override;

  PRemoteDecoderChild* AllocPRemoteDecoderChild(
      const RemoteDecoderInfoIPDL& aRemoteDecoderInfo,
      const CreateDecoderParams::OptionSet& aOptions,
      bool* aSuccess,
      nsCString* aErrorDescription);
  bool DeallocPRemoteDecoderChild(PRemoteDecoderChild* actor);

 private:
  // Main thread only
  static void InitializeThread();

  RemoteDecoderManagerChild() = default;
  ~RemoteDecoderManagerChild() = default;

  static void Open(Endpoint<PRemoteDecoderManagerChild>&& aEndpoint);

  RefPtr<RemoteDecoderManagerChild> mIPDLSelfRef;

  // Should only ever be accessed on the manager thread.
  bool mCanSend = false;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderManagerChild_h
