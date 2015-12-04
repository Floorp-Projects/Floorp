/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorChild_h
#define mozilla_layers_CompositorChild_h

#include "base/basictypes.h"            // for DISALLOW_EVIL_CONSTRUCTORS
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"         // for override
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/PCompositorChild.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsClassHashtable.h"           // for nsClassHashtable
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsHashKeys.h"                 // for nsUint64HashKey
#include "nsISupportsImpl.h"            // for NS_INLINE_DECL_REFCOUNTING
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"
#include "nsWeakReference.h"

namespace mozilla {

namespace dom {
  class TabChild;
} // namespace dom

namespace layers {

using mozilla::dom::TabChild;

class ClientLayerManager;
class CompositorParent;
struct FrameMetrics;

class CompositorChild final : public PCompositorChild
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(CompositorChild)

public:
  explicit CompositorChild(ClientLayerManager *aLayerManager);

  void Destroy();

  /**
   * Lookup the FrameMetrics shared by the compositor process with the
   * associated FrameMetrics::ViewID. The returned FrameMetrics is used
   * in progressive paint calculations.
   */
  bool LookupCompositorFrameMetrics(const FrameMetrics::ViewID aId, FrameMetrics&);

  /**
   * We're asked to create a new Compositor in response to an Opens()
   * or Bridge() request from our parent process.  The Transport is to
   * the compositor's context.
   */
  static PCompositorChild*
  Create(Transport* aTransport, ProcessId aOtherProcess);

  /**
   * Initialize the CompositorChild and open the connection in the non-multi-process
   * case.
   */
  bool OpenSameProcess(CompositorParent* aParent);

  static CompositorChild* Get();

  static bool ChildProcessHasCompositor() { return sCompositor != nullptr; }

  void AddOverfillObserver(ClientLayerManager* aLayerManager);

  virtual bool
  RecvClearCachedResources(const uint64_t& id) override;

  virtual bool
  RecvDidComposite(const uint64_t& aId, const uint64_t& aTransactionId,
                   const TimeStamp& aCompositeStart,
                   const TimeStamp& aCompositeEnd) override;

  virtual bool
  RecvInvalidateAll() override;

  virtual bool
  RecvOverfill(const uint32_t &aOverfill) override;

  virtual bool
  RecvUpdatePluginConfigurations(const LayoutDeviceIntPoint& aContentOffset,
                                 const LayoutDeviceIntRegion& aVisibleRegion,
                                 nsTArray<PluginWindowData>&& aPlugins) override;

  virtual bool
  RecvHideAllPlugins(const uintptr_t& aParentWidget) override;

  /**
   * Request that the parent tell us when graphics are ready on GPU.
   * When we get that message, we bounce it to the TabParent via
   * the TabChild
   * @param tabChild The object to bounce the note to.  Non-NULL.
   */
  void RequestNotifyAfterRemotePaint(TabChild* aTabChild);

  void CancelNotifyAfterRemotePaint(TabChild* aTabChild);

  // Beware that these methods don't override their super-class equivalent (which
  // are not virtual), they just overload them.
  // All of these Send* methods just add a sanity check (that it is not too late
  // send a message) and forward the call to the super-class's equivalent method.
  // This means that it is correct to call directly the super-class methods, but
  // you won't get the extra safety provided here.
  bool SendWillStop();
  bool SendPause();
  bool SendResume();
  bool SendNotifyHidden(const uint64_t& id);
  bool SendNotifyVisible(const uint64_t& id);
  bool SendNotifyChildCreated(const uint64_t& id);
  bool SendAdoptChild(const uint64_t& id);
  bool SendMakeSnapshot(const SurfaceDescriptor& inSnapshot, const gfx::IntRect& dirtyRect);
  bool SendFlushRendering();
  bool SendGetTileSize(int32_t* tileWidth, int32_t* tileHeight);
  bool SendStartFrameTimeRecording(const int32_t& bufferSize, uint32_t* startIndex);
  bool SendStopFrameTimeRecording(const uint32_t& startIndex, nsTArray<float>* intervals);
  bool SendNotifyRegionInvalidated(const nsIntRegion& region);
  bool SendRequestNotifyAfterRemotePaint();

private:
  // Private destructor, to discourage deletion outside of Release():
  virtual ~CompositorChild();

  virtual PLayerTransactionChild*
    AllocPLayerTransactionChild(const nsTArray<LayersBackend>& aBackendHints,
                                const uint64_t& aId,
                                TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                bool* aSuccess) override;

  virtual bool DeallocPLayerTransactionChild(PLayerTransactionChild *aChild) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool RecvSharedCompositorFrameMetrics(const mozilla::ipc::SharedMemoryBasic::Handle& metrics,
                                                const CrossProcessMutexHandle& handle,
                                                const uint64_t& aLayersId,
                                                const uint32_t& aAPZCId) override;

  virtual bool RecvReleaseSharedCompositorFrameMetrics(const ViewID& aId,
                                                       const uint32_t& aAPZCId) override;

  virtual bool
  RecvRemotePaintIsReady() override;

  // Class used to store the shared FrameMetrics, mutex, and APZCId  in a hash table
  class SharedFrameMetricsData {
  public:
    SharedFrameMetricsData(
        const mozilla::ipc::SharedMemoryBasic::Handle& metrics,
        const CrossProcessMutexHandle& handle,
        const uint64_t& aLayersId,
        const uint32_t& aAPZCId);

    ~SharedFrameMetricsData();

    void CopyFrameMetrics(FrameMetrics* aFrame);
    FrameMetrics::ViewID GetViewID();
    uint64_t GetLayersId() const;
    uint32_t GetAPZCId();

  private:
    // Pointer to the class that allows access to the shared memory that contains
    // the shared FrameMetrics
    RefPtr<mozilla::ipc::SharedMemoryBasic> mBuffer;
    CrossProcessMutex* mMutex;
    uint64_t mLayersId;
    // Unique ID of the APZC that is sharing the FrameMetrics
    uint32_t mAPZCId;
  };

  RefPtr<ClientLayerManager> mLayerManager;
  // When not multi-process, hold a reference to the CompositorParent to keep it
  // alive. This reference should be null in multi-process.
  RefPtr<CompositorParent> mCompositorParent;

  // The ViewID of the FrameMetrics is used as the key for this hash table.
  // While this should be safe to use since the ViewID is unique
  nsClassHashtable<nsUint64HashKey, SharedFrameMetricsData> mFrameMetricsTable;

  // When we're in a child process, this is the process-global
  // compositor that we use to forward transactions directly to the
  // compositor context in another process.
  static CompositorChild* sCompositor;

  // Weakly hold the TabChild that made a request to be alerted when
  // the transaction has been received.
  nsWeakPtr mWeakTabChild;      // type is TabChild

  DISALLOW_EVIL_CONSTRUCTORS(CompositorChild);

  // When we receive overfill numbers, notify these client layer managers
  nsAutoTArray<ClientLayerManager*,0> mOverfillObservers;

  // True until the beginning of the two-step shutdown sequence of this actor.
  bool mCanSend;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_CompositorChild_h
