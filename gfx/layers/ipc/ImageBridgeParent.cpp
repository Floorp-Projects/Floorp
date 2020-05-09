/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageBridgeParent.h"
#include <stdint.h>            // for uint64_t, uint32_t
#include "CompositableHost.h"  // for CompositableParent, Create
#include "base/process.h"      // for ProcessId
#include "base/task.h"         // for CancelableTask, DeleteTask, etc
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/gfx/Point.h"           // for IntSize
#include "mozilla/Hal.h"                 // for hal::SetCurrentThreadPriority()
#include "mozilla/HalTypes.h"            // for hal::THREAD_PRIORITY_COMPOSITOR
#include "mozilla/ipc/MessageChannel.h"  // for MessageChannel, etc
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Transport.h"                           // for Transport
#include "mozilla/media/MediaSystemResourceManagerParent.h"  // for MediaSystemResourceManagerParent
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/CompositableTransactionParent.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersMessages.h"  // for EditReply
#include "mozilla/layers/PImageBridgeParent.h"
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "mozilla/layers/Compositor.h"
#include "mozilla/Monitor.h"
#include "mozilla/mozalloc.h"  // for operator new, etc
#include "mozilla/Unused.h"
#include "nsDebug.h"                 // for NS_ASSERTION, etc
#include "nsISupportsImpl.h"         // for ImageBridgeParent::Release, etc
#include "nsTArray.h"                // for nsTArray, nsTArray_Impl
#include "nsTArrayForwardDeclare.h"  // for nsTArray
#include "nsXULAppAPI.h"             // for XRE_GetIOMessageLoop
#include "mozilla/layers/TextureHost.h"
#include "nsThreadUtils.h"

#if defined(OS_WIN)
#  include "mozilla/layers/TextureD3D11.h"
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gfx;
using namespace mozilla::media;

ImageBridgeParent::ImageBridgeMap ImageBridgeParent::sImageBridges;

StaticAutoPtr<mozilla::Monitor> sImageBridgesLock;

static StaticRefPtr<ImageBridgeParent> sImageBridgeParentSingleton;

/* static */
void ImageBridgeParent::Setup() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sImageBridgesLock) {
    sImageBridgesLock = new Monitor("ImageBridges");
    mozilla::ClearOnShutdown(&sImageBridgesLock);
  }
}

ImageBridgeParent::ImageBridgeParent(nsISerialEventTarget* aThread,
                                     ProcessId aChildProcessId)
    : mThread(aThread),
      mClosed(false),
      mCompositorThreadHolder(CompositorThreadHolder::GetSingleton()) {
  MOZ_ASSERT(NS_IsMainThread());
  SetOtherProcessId(aChildProcessId);
}

ImageBridgeParent::~ImageBridgeParent() = default;

/* static */
ImageBridgeParent* ImageBridgeParent::CreateSameProcess() {
  base::ProcessId pid = base::GetCurrentProcId();
  RefPtr<ImageBridgeParent> parent =
      new ImageBridgeParent(CompositorThread(), pid);
  parent->mSelfRef = parent;

  {
    MonitorAutoLock lock(*sImageBridgesLock);
    MOZ_RELEASE_ASSERT(sImageBridges.count(pid) == 0);
    sImageBridges[pid] = parent;
  }

  sImageBridgeParentSingleton = parent;
  return parent;
}

/* static */
bool ImageBridgeParent::CreateForGPUProcess(
    Endpoint<PImageBridgeParent>&& aEndpoint) {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);

  nsCOMPtr<nsISerialEventTarget> compositorThread = CompositorThread();
  if (!compositorThread) {
    return false;
  }

  RefPtr<ImageBridgeParent> parent =
      new ImageBridgeParent(compositorThread, aEndpoint.OtherPid());

  compositorThread->Dispatch(NewRunnableMethod<Endpoint<PImageBridgeParent>&&>(
      "layers::ImageBridgeParent::Bind", parent, &ImageBridgeParent::Bind,
      std::move(aEndpoint)));

  sImageBridgeParentSingleton = parent;
  return true;
}

/* static */
void ImageBridgeParent::ShutdownInternal() {
  // We make a copy because we don't want to hold the lock while closing and we
  // don't want the object to get freed underneath us.
  nsTArray<RefPtr<ImageBridgeParent>> actors;
  {
    MonitorAutoLock lock(*sImageBridgesLock);
    for (const auto& iter : sImageBridges) {
      actors.AppendElement(iter.second);
    }
  }

  for (auto const& actor : actors) {
    MOZ_RELEASE_ASSERT(!actor->mClosed);
    actor->Close();
  }

  sImageBridgeParentSingleton = nullptr;
}

/* static */
void ImageBridgeParent::Shutdown() {
  CompositorThread()->Dispatch(NS_NewRunnableFunction(
      "ImageBridgeParent::Shutdown",
      []() -> void { ImageBridgeParent::ShutdownInternal(); }));
}

void ImageBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Can't alloc/dealloc shmems from now on.
  mClosed = true;
  mCompositables.clear();
  {
    MonitorAutoLock lock(*sImageBridgesLock);
    sImageBridges.erase(OtherPid());
  }
  GetThread()->Dispatch(
      NewRunnableMethod("layers::ImageBridgeParent::DeferredDestroy", this,
                        &ImageBridgeParent::DeferredDestroy));

  // It is very important that this method gets called at shutdown (be it a
  // clean or an abnormal shutdown), because DeferredDestroy is what clears
  // mSelfRef. If mSelfRef is not null and ActorDestroy is not called, the
  // ImageBridgeParent is leaked which causes the CompositorThreadHolder to be
  // leaked and CompsoitorParent's shutdown ends up spinning the event loop
  // forever, waiting for the compositor thread to terminate.
}

class MOZ_STACK_CLASS AutoImageBridgeParentAsyncMessageSender final {
 public:
  explicit AutoImageBridgeParentAsyncMessageSender(
      ImageBridgeParent* aImageBridge,
      nsTArray<OpDestroy>* aToDestroy = nullptr)
      : mImageBridge(aImageBridge), mToDestroy(aToDestroy) {
    mImageBridge->SetAboutToSendAsyncMessages();
  }

  ~AutoImageBridgeParentAsyncMessageSender() {
    mImageBridge->SendPendingAsyncMessages();
    if (mToDestroy) {
      for (const auto& op : *mToDestroy) {
        mImageBridge->DestroyActor(op);
      }
    }
  }

 private:
  ImageBridgeParent* mImageBridge;
  nsTArray<OpDestroy>* mToDestroy;
};

mozilla::ipc::IPCResult ImageBridgeParent::RecvUpdate(
    EditArray&& aEdits, OpDestroyArray&& aToDestroy,
    const uint64_t& aFwdTransactionId) {
  AUTO_PROFILER_TRACING_MARKER("Paint", "ImageBridgeTransaction", GRAPHICS);
  AUTO_PROFILER_LABEL("ImageBridgeParent::RecvUpdate", GRAPHICS);

  // This ensures that destroy operations are always processed. It is not safe
  // to early-return from RecvUpdate without doing so.
  AutoImageBridgeParentAsyncMessageSender autoAsyncMessageSender(this,
                                                                 &aToDestroy);
  UpdateFwdTransactionId(aFwdTransactionId);

  for (const auto& edit : aEdits) {
    RefPtr<CompositableHost> compositable =
        FindCompositable(edit.compositable());
    if (!compositable ||
        !ReceiveCompositableUpdate(edit.detail(), WrapNotNull(compositable))) {
      return IPC_FAIL_NO_REASON(this);
    }
    uint32_t dropped = compositable->GetDroppedFrames();
    if (dropped) {
      Unused << SendReportFramesDropped(edit.compositable(), dropped);
    }
  }

  if (!IsSameProcess()) {
    // Ensure that any pending operations involving back and front
    // buffers have completed, so that neither process stomps on the
    // other's buffer contents.
    LayerManagerComposite::PlatformSyncBeforeReplyUpdate();
  }

  return IPC_OK();
}

/* static */
bool ImageBridgeParent::CreateForContent(
    Endpoint<PImageBridgeParent>&& aEndpoint) {
  nsCOMPtr<nsISerialEventTarget> compositorThread = CompositorThread();
  if (!compositorThread) {
    return false;
  }

  RefPtr<ImageBridgeParent> bridge =
      new ImageBridgeParent(compositorThread, aEndpoint.OtherPid());
  compositorThread->Dispatch(NewRunnableMethod<Endpoint<PImageBridgeParent>&&>(
      "layers::ImageBridgeParent::Bind", bridge, &ImageBridgeParent::Bind,
      std::move(aEndpoint)));

  return true;
}

void ImageBridgeParent::Bind(Endpoint<PImageBridgeParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) return;
  mSelfRef = this;

  // If the child process ID was reused by the OS before the ImageBridgeParent
  // object was destroyed, we need to clean it up first.
  RefPtr<ImageBridgeParent> oldActor;
  {
    MonitorAutoLock lock(*sImageBridgesLock);
    ImageBridgeMap::const_iterator i = sImageBridges.find(OtherPid());
    if (i != sImageBridges.end()) {
      oldActor = i->second;
    }
  }

  // We can't hold the lock during Close because it erases itself from the map.
  if (oldActor) {
    MOZ_RELEASE_ASSERT(!oldActor->mClosed);
    oldActor->Close();
  }

  {
    MonitorAutoLock lock(*sImageBridgesLock);
    sImageBridges[OtherPid()] = this;
  }
}

mozilla::ipc::IPCResult ImageBridgeParent::RecvWillClose() {
  // If there is any texture still alive we have to force it to deallocate the
  // device data (GL textures, etc.) now because shortly after SenStop() returns
  // on the child side the widget will be destroyed along with it's associated
  // GL context.
  nsTArray<PTextureParent*> textures;
  ManagedPTextureParent(textures);
  for (unsigned int i = 0; i < textures.Length(); ++i) {
    RefPtr<TextureHost> tex = TextureHost::AsTextureHost(textures[i]);
    tex->DeallocateDeviceData();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ImageBridgeParent::RecvNewCompositable(
    const CompositableHandle& aHandle, const TextureInfo& aInfo,
    const LayersBackend& aLayersBackend) {
  bool useWebRender = aLayersBackend == LayersBackend::LAYERS_WR;
  RefPtr<CompositableHost> host = AddCompositable(aHandle, aInfo, useWebRender);
  if (!host) {
    return IPC_FAIL_NO_REASON(this);
  }

  host->SetAsyncRef(AsyncCompositableRef(OtherPid(), aHandle));
  return IPC_OK();
}

mozilla::ipc::IPCResult ImageBridgeParent::RecvReleaseCompositable(
    const CompositableHandle& aHandle) {
  ReleaseCompositable(aHandle);
  return IPC_OK();
}

PTextureParent* ImageBridgeParent::AllocPTextureParent(
    const SurfaceDescriptor& aSharedData, const ReadLockDescriptor& aReadLock,
    const LayersBackend& aLayersBackend, const TextureFlags& aFlags,
    const uint64_t& aSerial, const wr::MaybeExternalImageId& aExternalImageId) {
  return TextureHost::CreateIPDLActor(this, aSharedData, aReadLock,
                                      aLayersBackend, aFlags, aSerial,
                                      aExternalImageId);
}

bool ImageBridgeParent::DeallocPTextureParent(PTextureParent* actor) {
  return TextureHost::DestroyIPDLActor(actor);
}

PMediaSystemResourceManagerParent*
ImageBridgeParent::AllocPMediaSystemResourceManagerParent() {
  return new mozilla::media::MediaSystemResourceManagerParent();
}

bool ImageBridgeParent::DeallocPMediaSystemResourceManagerParent(
    PMediaSystemResourceManagerParent* aActor) {
  MOZ_ASSERT(aActor);
  delete static_cast<mozilla::media::MediaSystemResourceManagerParent*>(aActor);
  return true;
}

void ImageBridgeParent::SendAsyncMessage(
    const nsTArray<AsyncParentMessageData>& aMessage) {
  mozilla::Unused << SendParentAsyncMessages(aMessage);
}

class ProcessIdComparator {
 public:
  bool Equals(const ImageCompositeNotificationInfo& aA,
              const ImageCompositeNotificationInfo& aB) const {
    return aA.mImageBridgeProcessId == aB.mImageBridgeProcessId;
  }
  bool LessThan(const ImageCompositeNotificationInfo& aA,
                const ImageCompositeNotificationInfo& aB) const {
    return aA.mImageBridgeProcessId < aB.mImageBridgeProcessId;
  }
};

/* static */
bool ImageBridgeParent::NotifyImageComposites(
    nsTArray<ImageCompositeNotificationInfo>& aNotifications) {
  // Group the notifications by destination process ID and then send the
  // notifications in one message per group.
  aNotifications.Sort(ProcessIdComparator());
  uint32_t i = 0;
  bool ok = true;
  while (i < aNotifications.Length()) {
    AutoTArray<ImageCompositeNotification, 1> notifications;
    notifications.AppendElement(aNotifications[i].mNotification);
    uint32_t end = i + 1;
    MOZ_ASSERT(aNotifications[i].mNotification.compositable());
    ProcessId pid = aNotifications[i].mImageBridgeProcessId;
    while (end < aNotifications.Length() &&
           aNotifications[end].mImageBridgeProcessId == pid) {
      notifications.AppendElement(aNotifications[end].mNotification);
      ++end;
    }
    RefPtr<ImageBridgeParent> bridge = GetInstance(pid);
    if (!bridge || bridge->mClosed) {
      i = end;
      continue;
    }
    bridge->SendPendingAsyncMessages();
    if (!bridge->SendDidComposite(notifications)) {
      ok = false;
    }
    i = end;
  }
  return ok;
}

void ImageBridgeParent::DeferredDestroy() {
  mCompositorThreadHolder = nullptr;
  mSelfRef = nullptr;  // "this" ImageBridge may get deleted here.
}

already_AddRefed<ImageBridgeParent> ImageBridgeParent::GetInstance(
    ProcessId aId) {
  MOZ_ASSERT(CompositorThreadHolder::IsInCompositorThread());
  MonitorAutoLock lock(*sImageBridgesLock);
  ImageBridgeMap::const_iterator i = sImageBridges.find(aId);
  if (i == sImageBridges.end()) {
    NS_WARNING("Cannot find image bridge for process!");
    return nullptr;
  }
  RefPtr<ImageBridgeParent> bridge = i->second;
  return bridge.forget();
}

bool ImageBridgeParent::AllocShmem(size_t aSize,
                                   ipc::SharedMemory::SharedMemoryType aType,
                                   ipc::Shmem* aShmem) {
  if (mClosed) {
    return false;
  }
  return PImageBridgeParent::AllocShmem(aSize, aType, aShmem);
}

bool ImageBridgeParent::AllocUnsafeShmem(
    size_t aSize, ipc::SharedMemory::SharedMemoryType aType,
    ipc::Shmem* aShmem) {
  if (mClosed) {
    return false;
  }
  return PImageBridgeParent::AllocUnsafeShmem(aSize, aType, aShmem);
}

bool ImageBridgeParent::DeallocShmem(ipc::Shmem& aShmem) {
  if (mClosed) {
    return false;
  }
  return PImageBridgeParent::DeallocShmem(aShmem);
}

bool ImageBridgeParent::IsSameProcess() const {
  return OtherPid() == base::GetCurrentProcId();
}

void ImageBridgeParent::NotifyNotUsed(PTextureParent* aTexture,
                                      uint64_t aTransactionId) {
  RefPtr<TextureHost> texture = TextureHost::AsTextureHost(aTexture);
  if (!texture) {
    return;
  }

  if (!(texture->GetFlags() & TextureFlags::RECYCLE)) {
    return;
  }

  uint64_t textureId = TextureHost::GetTextureSerial(aTexture);
  mPendingAsyncMessage.push_back(OpNotifyNotUsed(textureId, aTransactionId));

  if (!IsAboutToSendAsyncMessages()) {
    SendPendingAsyncMessages();
  }
}

#if defined(OS_WIN)

ImageBridgeParent::PluginTextureDatas::PluginTextureDatas(
    UniquePtr<D3D11TextureData>&& aPluginTextureData,
    UniquePtr<D3D11TextureData>&& aDisplayTextureData)
    : mPluginTextureData(std::move(aPluginTextureData)),
      mDisplayTextureData(std::move(aDisplayTextureData)) {}

ImageBridgeParent::PluginTextureDatas::~PluginTextureDatas() {}

#endif  // defined(OS_WIN)

mozilla::ipc::IPCResult ImageBridgeParent::RecvMakeAsyncPluginSurfaces(
    SurfaceFormat aFormat, IntSize aSize, SurfaceDescriptorPlugin* aSD) {
#if defined(OS_WIN)
  *aSD = SurfaceDescriptorPlugin();

  RefPtr<ID3D11Device> d3dDevice =
      DeviceManagerDx::Get()->GetCompositorDevice();
  if (!d3dDevice) {
    NS_WARNING("Failed to get D3D11 device for plugin display");
    return IPC_OK();
  }

  auto pluginSurf = WrapUnique(D3D11TextureData::Create(
      aSize, aFormat, ALLOC_FOR_OUT_OF_BAND_CONTENT, d3dDevice));
  if (!pluginSurf) {
    NS_ERROR("Failed to create plugin surface");
    return IPC_OK();
  }

  auto dispSurf = WrapUnique(D3D11TextureData::Create(
      aSize, aFormat, ALLOC_FOR_OUT_OF_BAND_CONTENT, d3dDevice));
  if (!dispSurf) {
    NS_ERROR("Failed to create plugin display surface");
    return IPC_OK();
  }

  // Identify plugin surfaces with a simple non-zero 64-bit ID.
  static uint64_t sPluginSurfaceId = 1;

  SurfaceDescriptor pluginSD, dispSD;
  if ((!pluginSurf->Serialize(pluginSD)) || (!dispSurf->Serialize(dispSD))) {
    NS_ERROR("Failed to make surface descriptors for plugin");
    return IPC_OK();
  }

  if (!mPluginTextureDatas.put(
          sPluginSurfaceId, MakeUnique<PluginTextureDatas>(
                                std::move(pluginSurf), std::move(dispSurf)))) {
    NS_ERROR("Failed to add plugin surfaces to map");
    return IPC_OK();
  }

  SurfaceDescriptorPlugin sd(sPluginSurfaceId, pluginSD, dispSD);
  RefPtr<TextureHost> displayHost = CreateTextureHostD3D11(
      dispSD, this, LayersBackend::LAYERS_NONE, TextureFlags::RECYCLE);
  if (!displayHost) {
    NS_ERROR("Failed to create plugin display texture host");
    return IPC_OK();
  }

  if (!mGPUVideoTextureHosts.put(sPluginSurfaceId, displayHost)) {
    NS_ERROR("Failed to add plugin display texture host to map");
    return IPC_OK();
  }

  *aSD = sd;
  ++sPluginSurfaceId;
#endif  // defined(OS_WIN)

  return IPC_OK();
}

mozilla::ipc::IPCResult ImageBridgeParent::RecvUpdateAsyncPluginSurface(
    const SurfaceDescriptorPlugin& aSD) {
#if defined(OS_WIN)
  uint64_t surfaceId = aSD.id();
  auto itTextures = mPluginTextureDatas.lookup(surfaceId);
  if (!itTextures) {
    return IPC_OK();
  }

  auto& textures = itTextures->value();
  if (!textures->IsValid()) {
    // The display texture may be gone.  The plugin texture should never be gone
    // here.
    MOZ_ASSERT(textures->mPluginTextureData);
    return IPC_OK();
  }

  RefPtr<ID3D11Device> device = DeviceManagerDx::Get()->GetCompositorDevice();
  if (!device) {
    NS_WARNING("Failed to get D3D11 device for plugin display");
    return IPC_OK();
  }

  RefPtr<ID3D11DeviceContext> context;
  device->GetImmediateContext(getter_AddRefs(context));
  if (!context) {
    NS_WARNING("Could not get an immediate D3D11 context");
    return IPC_OK();
  }

  RefPtr<IDXGIKeyedMutex> dispMutex;
  HRESULT hr = textures->mDisplayTextureData->GetD3D11Texture()->QueryInterface(
      __uuidof(IDXGIKeyedMutex), (void**)getter_AddRefs(dispMutex));
  if (FAILED(hr) || !dispMutex) {
    NS_WARNING("Could not acquire plugin display IDXGIKeyedMutex");
    return IPC_OK();
  }

  RefPtr<IDXGIKeyedMutex> pluginMutex;
  hr = textures->mPluginTextureData->GetD3D11Texture()->QueryInterface(
      __uuidof(IDXGIKeyedMutex), (void**)getter_AddRefs(pluginMutex));
  if (FAILED(hr) || !pluginMutex) {
    NS_WARNING("Could not acquire plugin offscreen IDXGIKeyedMutex");
    return IPC_OK();
  }

  {
    AutoTextureLock lock1(dispMutex, hr);
    if (hr == WAIT_ABANDONED || hr == WAIT_TIMEOUT || FAILED(hr)) {
      NS_WARNING(
          "Could not acquire DXGI surface lock - display forgot to release?");
      return IPC_OK();
    }

    AutoTextureLock lock2(pluginMutex, hr);
    if (hr == WAIT_ABANDONED || hr == WAIT_TIMEOUT || FAILED(hr)) {
      NS_WARNING(
          "Could not acquire DXGI surface lock - plugin forgot to release?");
      return IPC_OK();
    }

    context->CopyResource(textures->mDisplayTextureData->GetD3D11Texture(),
                          textures->mPluginTextureData->GetD3D11Texture());
  }
#endif  // defined(OS_WIN)
  return IPC_OK();
}

mozilla::ipc::IPCResult ImageBridgeParent::RecvReadbackAsyncPluginSurface(
    const SurfaceDescriptorPlugin& aSD, SurfaceDescriptor* aResult) {
#if defined(OS_WIN)
  *aResult = null_t();

  auto itTextures = mPluginTextureDatas.lookup(aSD.id());
  if (!itTextures) {
    return IPC_OK();
  }

  auto& textures = itTextures->value();
  D3D11TextureData* displayTexData = textures->mDisplayTextureData.get();
  MOZ_RELEASE_ASSERT(displayTexData);
  if ((!displayTexData) || (!displayTexData->GetD3D11Texture())) {
    NS_WARNING("Error in plugin display texture");
    return IPC_OK();
  }
  MOZ_ASSERT(displayTexData->GetSurfaceFormat() == SurfaceFormat::B8G8R8A8 ||
             displayTexData->GetSurfaceFormat() == SurfaceFormat::B8G8R8X8);

  RefPtr<ID3D11Device> device;
  displayTexData->GetD3D11Texture()->GetDevice(getter_AddRefs(device));
  if (!device) {
    NS_WARNING("Failed to get D3D11 device for plugin display");
    return IPC_OK();
  }

  UniquePtr<BufferTextureData> shmemTexData(BufferTextureData::Create(
      displayTexData->GetSize(), displayTexData->GetSurfaceFormat(),
      gfx::BackendType::SKIA, LayersBackend::LAYERS_NONE,
      displayTexData->GetTextureFlags(), TextureAllocationFlags::ALLOC_DEFAULT,
      this));
  if (!shmemTexData) {
    NS_WARNING("Could not create BufferTextureData");
    return IPC_OK();
  }

  if (!gfx::Factory::ReadbackTexture(shmemTexData.get(),
                                     displayTexData->GetD3D11Texture())) {
    NS_WARNING("Failed to read plugin texture into Shmem");
    return IPC_OK();
  }

  // Take the Shmem from the TextureData.
  shmemTexData->Serialize(*aResult);
#endif  // defined(OS_WIN)
  return IPC_OK();
}

mozilla::ipc::IPCResult ImageBridgeParent::RecvRemoveAsyncPluginSurface(
    const SurfaceDescriptorPlugin& aSD, bool aIsFrontSurface) {
#if defined(OS_WIN)
  auto itTextures = mPluginTextureDatas.lookup(aSD.id());
  if (!itTextures) {
    return IPC_OK();
  }

  auto& textures = itTextures->value();
  if (aIsFrontSurface) {
    textures->mDisplayTextureData = nullptr;
  } else {
    textures->mPluginTextureData = nullptr;
  }
  if ((!textures->mDisplayTextureData) && (!textures->mPluginTextureData)) {
    mPluginTextureDatas.remove(aSD.id());
  }
#endif  // defined(OS_WIN)
  return IPC_OK();
}

#if defined(OS_WIN)
RefPtr<TextureHost> GetNullPluginTextureHost() {
  class NullPluginTextureHost : public TextureHost {
   public:
    NullPluginTextureHost() : TextureHost(TextureFlags::NO_FLAGS) {}

    ~NullPluginTextureHost() {}

    gfx::SurfaceFormat GetFormat() const override {
      return gfx::SurfaceFormat::UNKNOWN;
    }

    already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
      return nullptr;
    }

    gfx::IntSize GetSize() const override { return gfx::IntSize(); }

    bool BindTextureSource(CompositableTextureSourceRef& aTexture) override {
      return false;
    }

    const char* Name() override { return "NullPluginTextureHost"; }

    virtual bool Lock() { return false; }

    void CreateRenderTexture(
        const wr::ExternalImageId& aExternalImageId) override {}

    uint32_t NumSubTextures() override { return 0; }

    void PushResourceUpdates(wr::TransactionBuilder& aResources,
                             ResourceUpdateOp aOp,
                             const Range<wr::ImageKey>& aImageKeys,
                             const wr::ExternalImageId& aExtID) override {}

    void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                          const wr::LayoutRect& aBounds,
                          const wr::LayoutRect& aClip,
                          wr::ImageRendering aFilter,
                          const Range<wr::ImageKey>& aImageKeys,
                          const bool aPreferCompositorSurface) override {}
  };

  static StaticRefPtr<TextureHost> sNullPluginTextureHost;
  if (!sNullPluginTextureHost) {
    sNullPluginTextureHost = new NullPluginTextureHost();
    ClearOnShutdown(&sNullPluginTextureHost);
  };

  MOZ_ASSERT(sNullPluginTextureHost);
  return sNullPluginTextureHost.get();
}
#endif  // defined(OS_WIN)

RefPtr<TextureHost> ImageBridgeParent::LookupTextureHost(
    const SurfaceDescriptorPlugin& aDescriptor) {
#if defined(OS_WIN)
  auto it = mGPUVideoTextureHosts.lookup(aDescriptor.id());
  RefPtr<TextureHost> ret = it ? it->value() : nullptr;
  return ret ? ret : GetNullPluginTextureHost();
#else
  MOZ_ASSERT_UNREACHABLE("Unsupported architecture.");
  return nullptr;
#endif  // defined(OS_WIN)
}

}  // namespace layers
}  // namespace mozilla
