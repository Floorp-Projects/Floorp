/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderManagerChild.h"

#include "RemoteDecoderChild.h"
#include "VideoUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/SynchronousTask.h"

namespace mozilla {

using namespace layers;
using namespace gfx;

// Only modified on the main-thread
StaticRefPtr<nsIThread> sRemoteDecoderManagerChildThread;

// Only accessed from sRemoteDecoderManagerChildThread
static StaticRefPtr<RemoteDecoderManagerChild>
    sRemoteDecoderManagerChildForRDDProcess;

static StaticRefPtr<RemoteDecoderManagerChild>
    sRemoteDecoderManagerChildForGPUProcess;
static UniquePtr<nsTArray<RefPtr<Runnable>>> sRecreateTasks;

/* static */
void RemoteDecoderManagerChild::InitializeThread() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sRemoteDecoderManagerChildThread) {
    // We can't use a MediaThreadType::CONTROLLER as the GpuDecoderModule and
    // RemoteDecoderModule runs on it and dispatch synchronous tasks to the
    // manager thread, should more than 4 concurrent videos being instantiated
    // at the same time, we could end up in a deadlock.
    RefPtr<nsIThread> childThread;
    nsresult rv = NS_NewNamedThread("RemVidChild", getter_AddRefs(childThread));
    NS_ENSURE_SUCCESS_VOID(rv);
    sRemoteDecoderManagerChildThread = childThread;
    sRecreateTasks = MakeUnique<nsTArray<RefPtr<Runnable>>>();
  }
}

/* static */
void RemoteDecoderManagerChild::InitForRDDProcess(
    Endpoint<PRemoteDecoderManagerChild>&& aVideoManager) {
  InitializeThread();
  MOZ_ALWAYS_SUCCEEDS(sRemoteDecoderManagerChildThread->Dispatch(
      NewRunnableFunction("InitForContentRunnable", &OpenForRDDProcess,
                          std::move(aVideoManager))));
}

/* static */
void RemoteDecoderManagerChild::InitForGPUProcess(
    Endpoint<PRemoteDecoderManagerChild>&& aVideoManager) {
  InitializeThread();
  MOZ_ALWAYS_SUCCEEDS(sRemoteDecoderManagerChildThread->Dispatch(
      NewRunnableFunction("InitForContentRunnable", &OpenForGPUProcess,
                          std::move(aVideoManager))));
}

/* static */
void RemoteDecoderManagerChild::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sRemoteDecoderManagerChildThread) {
    MOZ_ALWAYS_SUCCEEDS(
        sRemoteDecoderManagerChildThread->Dispatch(NS_NewRunnableFunction(
            "dom::RemoteDecoderManagerChild::Shutdown", []() {
              if (sRemoteDecoderManagerChildForRDDProcess &&
                  sRemoteDecoderManagerChildForRDDProcess->CanSend()) {
                sRemoteDecoderManagerChildForRDDProcess->Close();
              }
              sRemoteDecoderManagerChildForRDDProcess = nullptr;
              if (sRemoteDecoderManagerChildForGPUProcess &&
                  sRemoteDecoderManagerChildForGPUProcess->CanSend()) {
                sRemoteDecoderManagerChildForGPUProcess->Close();
              }
              sRemoteDecoderManagerChildForGPUProcess = nullptr;
            })));

    sRemoteDecoderManagerChildThread->Shutdown();
    sRemoteDecoderManagerChildThread = nullptr;

    sRecreateTasks = nullptr;
  }
}

void RemoteDecoderManagerChild::RunWhenGPUProcessRecreated(
    already_AddRefed<Runnable> aTask) {
  MOZ_ASSERT(GetManagerThread() && GetManagerThread()->IsOnCurrentThread());

  // If we've already been recreated, then run the task immediately.
  if (GetGPUProcessSingleton() && GetGPUProcessSingleton() != this &&
      GetGPUProcessSingleton()->CanSend()) {
    RefPtr<Runnable> task = aTask;
    task->Run();
  } else {
    sRecreateTasks->AppendElement(aTask);
  }
}

/* static */
RemoteDecoderManagerChild* RemoteDecoderManagerChild::GetRDDProcessSingleton() {
  MOZ_ASSERT(GetManagerThread() && GetManagerThread()->IsOnCurrentThread());
  return sRemoteDecoderManagerChildForRDDProcess;
}

/* static */
RemoteDecoderManagerChild* RemoteDecoderManagerChild::GetGPUProcessSingleton() {
  MOZ_ASSERT(GetManagerThread() && GetManagerThread()->IsOnCurrentThread());
  return sRemoteDecoderManagerChildForGPUProcess;
}

/* static */
nsISerialEventTarget* RemoteDecoderManagerChild::GetManagerThread() {
  return sRemoteDecoderManagerChildThread;
}

PRemoteDecoderChild* RemoteDecoderManagerChild::AllocPRemoteDecoderChild(
    const RemoteDecoderInfoIPDL& /* not used */,
    const CreateDecoderParams::OptionSet& aOptions,
    const Maybe<layers::TextureFactoryIdentifier>& aIdentifier, bool* aSuccess,
    nsCString* /* not used */) {
  // RemoteDecoderModule is responsible for creating RemoteDecoderChild
  // classes.
  MOZ_ASSERT(false,
             "RemoteDecoderManagerChild cannot create "
             "RemoteDecoderChild classes");
  return nullptr;
}

bool RemoteDecoderManagerChild::DeallocPRemoteDecoderChild(
    PRemoteDecoderChild* actor) {
  RemoteDecoderChild* child = static_cast<RemoteDecoderChild*>(actor);
  child->IPDLActorDestroyed();
  return true;
}

RemoteDecoderManagerChild::RemoteDecoderManagerChild(
    layers::VideoBridgeSource aSource)
    : mSource(aSource) {}

void RemoteDecoderManagerChild::OpenForRDDProcess(
    Endpoint<PRemoteDecoderManagerChild>&& aEndpoint) {
  MOZ_ASSERT(GetManagerThread() && GetManagerThread()->IsOnCurrentThread());
  // Only create RemoteDecoderManagerChild, bind new endpoint and init
  // ipdl if:
  // 1) haven't init'd sRemoteDecoderManagerChild
  // or
  // 2) if ActorDestroy was called meaning the other end of the ipc channel was
  //    torn down
  if (sRemoteDecoderManagerChildForRDDProcess &&
      sRemoteDecoderManagerChildForRDDProcess->CanSend()) {
    return;
  }
  sRemoteDecoderManagerChildForRDDProcess = nullptr;
  if (aEndpoint.IsValid()) {
    RefPtr<RemoteDecoderManagerChild> manager =
        new RemoteDecoderManagerChild(VideoBridgeSource::RddProcess);
    if (aEndpoint.Bind(manager)) {
      sRemoteDecoderManagerChildForRDDProcess = manager;
      manager->InitIPDL();
    }
  }
}

void RemoteDecoderManagerChild::OpenForGPUProcess(
    Endpoint<PRemoteDecoderManagerChild>&& aEndpoint) {
  // Make sure we always dispatch everything in sRecreateTasks, even if we
  // fail since this is as close to being recreated as we will ever be.
  sRemoteDecoderManagerChildForGPUProcess = nullptr;
  if (aEndpoint.IsValid()) {
    RefPtr<RemoteDecoderManagerChild> manager =
        new RemoteDecoderManagerChild(VideoBridgeSource::GpuProcess);
    if (aEndpoint.Bind(manager)) {
      sRemoteDecoderManagerChildForGPUProcess = manager;
      manager->InitIPDL();
    }
  }
  for (Runnable* task : *sRecreateTasks) {
    task->Run();
  }
  sRecreateTasks->Clear();
}

void RemoteDecoderManagerChild::InitIPDL() { mIPDLSelfRef = this; }

void RemoteDecoderManagerChild::ActorDealloc() { mIPDLSelfRef = nullptr; }

bool RemoteDecoderManagerChild::DeallocShmem(mozilla::ipc::Shmem& aShmem) {
  if (!sRemoteDecoderManagerChildThread->IsOnCurrentThread()) {
    RefPtr<RemoteDecoderManagerChild> self = this;
    mozilla::ipc::Shmem shmem = aShmem;
    MOZ_ALWAYS_SUCCEEDS(
        sRemoteDecoderManagerChildThread->Dispatch(NS_NewRunnableFunction(
            "RemoteDecoderManagerChild::DeallocShmem", [self, shmem]() {
              if (self->CanSend()) {
                mozilla::ipc::Shmem shmemCopy = shmem;
                self->DeallocShmem(shmemCopy);
              }
            })));
    return true;
  }
  return PRemoteDecoderManagerChild::DeallocShmem(aShmem);
}

struct SurfaceDescriptorUserData {
  SurfaceDescriptorUserData(RemoteDecoderManagerChild* aAllocator,
                            SurfaceDescriptor& aSD)
      : mAllocator(aAllocator), mSD(aSD) {}
  ~SurfaceDescriptorUserData() { DestroySurfaceDescriptor(mAllocator, &mSD); }

  RefPtr<RemoteDecoderManagerChild> mAllocator;
  SurfaceDescriptor mSD;
};

void DeleteSurfaceDescriptorUserData(void* aClosure) {
  SurfaceDescriptorUserData* sd =
      reinterpret_cast<SurfaceDescriptorUserData*>(aClosure);
  delete sd;
}

already_AddRefed<SourceSurface> RemoteDecoderManagerChild::Readback(
    const SurfaceDescriptorGPUVideo& aSD) {
  // We can't use NS_DISPATCH_SYNC here since that can spin the event
  // loop while it waits. This function can be called from JS and we
  // don't want that to happen.
  SynchronousTask task("Readback sync");

  RefPtr<RemoteDecoderManagerChild> ref = this;
  SurfaceDescriptor sd;
  if (NS_FAILED(sRemoteDecoderManagerChildThread->Dispatch(
          NS_NewRunnableFunction("RemoteDecoderManagerChild::Readback", [&]() {
            AutoCompleteTask complete(&task);
            if (ref->CanSend()) {
              ref->SendReadback(aSD, &sd);
            }
          })))) {
    return nullptr;
  }

  task.Wait();

  if (!IsSurfaceDescriptorValid(sd)) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> source = GetSurfaceForDescriptor(sd);
  if (!source) {
    DestroySurfaceDescriptor(this, &sd);
    NS_WARNING("Failed to map SurfaceDescriptor in Readback");
    return nullptr;
  }

  static UserDataKey sSurfaceDescriptor;
  source->AddUserData(&sSurfaceDescriptor,
                      new SurfaceDescriptorUserData(this, sd),
                      DeleteSurfaceDescriptorUserData);

  return source.forget();
}

void RemoteDecoderManagerChild::DeallocateSurfaceDescriptor(
    const SurfaceDescriptorGPUVideo& aSD) {
  MOZ_ALWAYS_SUCCEEDS(
      sRemoteDecoderManagerChildThread->Dispatch(NS_NewRunnableFunction(
          "RemoteDecoderManagerChild::DeallocateSurfaceDescriptor",
          [ref = RefPtr{this}, sd = aSD]() {
            if (ref->CanSend()) {
              ref->SendDeallocateSurfaceDescriptorGPUVideo(sd);
            }
          })));
}

void RemoteDecoderManagerChild::HandleFatalError(const char* aMsg) const {
  dom::ContentChild::FatalErrorIfNotUsingGPUProcess(aMsg, OtherPid());
}

}  // namespace mozilla
