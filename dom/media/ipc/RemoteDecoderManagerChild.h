/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderManagerChild_h
#define include_dom_media_ipc_RemoteDecoderManagerChild_h

#include "GPUVideoImage.h"
#include "PDMFactory.h"
#include "ipc/EnumSerializer.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/PRemoteDecoderManagerChild.h"
#include "mozilla/layers/VideoBridgeUtils.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"

namespace mozilla {

class PMFMediaEngineChild;
class RemoteDecoderChild;

enum class RemoteDecodeIn {
  Unspecified,
  RddProcess,
  GpuProcess,
  UtilityProcess_Generic,
  UtilityProcess_AppleMedia,
  UtilityProcess_WMF,
  UtilityProcess_MFMediaEngineCDM,
  SENTINEL,
};

enum class TrackSupport {
  None,
  Audio,
  Video,
};
using TrackSupportSet = EnumSet<TrackSupport, uint8_t>;

class RemoteDecoderManagerChild final
    : public PRemoteDecoderManagerChild,
      public mozilla::ipc::IShmemAllocator,
      public mozilla::layers::IGPUVideoSurfaceManager {
  friend class PRemoteDecoderManagerChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteDecoderManagerChild, override)

  // Can only be called from the manager thread
  static RemoteDecoderManagerChild* GetSingleton(RemoteDecodeIn aLocation);

  static void Init();
  static void SetSupported(RemoteDecodeIn aLocation,
                           const media::MediaCodecsSupported& aSupported);

  // Can be called from any thread.
  static bool Supports(RemoteDecodeIn aLocation,
                       const SupportDecoderParams& aParams,
                       DecoderDoctorDiagnostics* aDiagnostics);
  static RefPtr<PlatformDecoderModule::CreateDecoderPromise> CreateAudioDecoder(
      const CreateDecoderParams& aParams, RemoteDecodeIn aLocation);
  static RefPtr<PlatformDecoderModule::CreateDecoderPromise> CreateVideoDecoder(
      const CreateDecoderParams& aParams, RemoteDecodeIn aLocation);

  // Can be called from any thread.
  static nsISerialEventTarget* GetManagerThread();

  // Return the track support information based on the location of the remote
  // process. Thread-safe.
  static TrackSupportSet GetTrackSupport(RemoteDecodeIn aLocation);

  // Can be called from any thread, dispatches the request to the IPDL thread
  // internally and will be ignored if the IPDL actor has been destroyed.
  already_AddRefed<gfx::SourceSurface> Readback(
      const SurfaceDescriptorGPUVideo& aSD) override;
  void DeallocateSurfaceDescriptor(
      const SurfaceDescriptorGPUVideo& aSD) override;

  bool AllocShmem(size_t aSize, mozilla::ipc::Shmem* aShmem) override {
    return PRemoteDecoderManagerChild::AllocShmem(aSize, aShmem);
  }
  bool AllocUnsafeShmem(size_t aSize, mozilla::ipc::Shmem* aShmem) override {
    return PRemoteDecoderManagerChild::AllocUnsafeShmem(aSize, aShmem);
  }

  // Can be called from any thread, dispatches the request to the IPDL thread
  // internally and will be ignored if the IPDL actor has been destroyed.
  bool DeallocShmem(mozilla::ipc::Shmem& aShmem) override;

  // Main thread only
  static void InitForGPUProcess(
      Endpoint<PRemoteDecoderManagerChild>&& aVideoManager);
  static void Shutdown();

  // Run aTask (on the manager thread) when we next attempt to create a new
  // manager (even if creation fails). Intended to be called from ActorDestroy
  // when we get notified that the old manager is being destroyed. Can only be
  // called from the manager thread.
  void RunWhenGPUProcessRecreated(already_AddRefed<Runnable> aTask);

  RemoteDecodeIn Location() const { return mLocation; }

  // A thread-safe method to launch the utility process if it hasn't launched
  // yet.
  static RefPtr<GenericNonExclusivePromise> LaunchUtilityProcessIfNeeded(
      RemoteDecodeIn aLocation);

 protected:
  void InitIPDL();

  void ActorDealloc() override;

  void HandleFatalError(const char* aMsg) const override;

  PRemoteDecoderChild* AllocPRemoteDecoderChild(
      const RemoteDecoderInfoIPDL& aRemoteDecoderInfo,
      const CreateDecoderParams::OptionSet& aOptions,
      const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
      const Maybe<uint64_t>& aMediaEngineId);
  bool DeallocPRemoteDecoderChild(PRemoteDecoderChild* actor);

  PMFMediaEngineChild* AllocPMFMediaEngineChild();
  bool DeallocPMFMediaEngineChild(PMFMediaEngineChild* actor);

 private:
  explicit RemoteDecoderManagerChild(RemoteDecodeIn aLocation);
  ~RemoteDecoderManagerChild() = default;
  static RefPtr<PlatformDecoderModule::CreateDecoderPromise> Construct(
      RefPtr<RemoteDecoderChild>&& aChild, RemoteDecodeIn aLocation);

  static void OpenRemoteDecoderManagerChildForProcess(
      Endpoint<PRemoteDecoderManagerChild>&& aEndpoint,
      RemoteDecodeIn aLocation);

  // A thread-safe method to launch the RDD process if it hasn't launched yet.
  static RefPtr<GenericNonExclusivePromise> LaunchRDDProcessIfNeeded();

  RefPtr<RemoteDecoderManagerChild> mIPDLSelfRef;
  // The location for decoding, Rdd or Gpu process.
  const RemoteDecodeIn mLocation;
};

}  // namespace mozilla

namespace IPC {
template <>
struct ParamTraits<mozilla::RemoteDecodeIn>
    : public ContiguousEnumSerializer<mozilla::RemoteDecodeIn,
                                      mozilla::RemoteDecodeIn::Unspecified,
                                      mozilla::RemoteDecodeIn::SENTINEL> {};
}  // namespace IPC

#endif  // include_dom_media_ipc_RemoteDecoderManagerChild_h
