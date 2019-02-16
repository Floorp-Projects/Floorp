/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderManagerParent_h
#define include_dom_media_ipc_RemoteDecoderManagerParent_h
#include "mozilla/PRemoteDecoderManagerParent.h"

namespace mozilla {

class RemoteDecoderManagerThreadHolder;

class RemoteDecoderManagerParent final : public PRemoteDecoderManagerParent {
  friend class PRemoteDecoderManagerParent;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteDecoderManagerParent)

  static bool CreateForContent(
      Endpoint<PRemoteDecoderManagerParent>&& aEndpoint);

  static bool StartupThreads();
  static void ShutdownThreads();

  bool OnManagerThread();

 protected:
  PRemoteDecoderParent* AllocPRemoteDecoderParent(
      const RemoteDecoderInfoIPDL& aRemoteDecoderInfo,
      const CreateDecoderParams::OptionSet& aOptions, bool* aSuccess,
      nsCString* aErrorDescription);
  bool DeallocPRemoteDecoderParent(PRemoteDecoderParent* actor);

  void ActorDestroy(mozilla::ipc::IProtocol::ActorDestroyReason) override;

  void DeallocPRemoteDecoderManagerParent() override;

 private:
  explicit RemoteDecoderManagerParent(
      RemoteDecoderManagerThreadHolder* aThreadHolder);
  ~RemoteDecoderManagerParent();

  void Open(Endpoint<PRemoteDecoderManagerParent>&& aEndpoint);

  RefPtr<RemoteDecoderManagerThreadHolder> mThreadHolder;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderManagerParent_h
