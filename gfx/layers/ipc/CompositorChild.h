/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorChild_h
#define mozilla_layers_CompositorChild_h

#include "base/basictypes.h"            // for DISALLOW_EVIL_CONSTRUCTORS
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/PCompositorChild.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsClassHashtable.h"           // for nsClassHashtable
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsHashKeys.h"                 // for nsUint64HashKey
#include "nsISupportsImpl.h"            // for NS_INLINE_DECL_REFCOUNTING

class nsIObserver;

namespace mozilla {
namespace layers {

class ClientLayerManager;
class CompositorParent;
struct FrameMetrics;

class CompositorChild MOZ_FINAL : public PCompositorChild
{
  NS_INLINE_DECL_REFCOUNTING(CompositorChild)
public:
  CompositorChild(ClientLayerManager *aLayerManager);

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

  static CompositorChild* Get();

  static bool ChildProcessHasCompositor() { return sCompositor != nullptr; }

  virtual bool RecvInvalidateAll() MOZ_OVERRIDE;
  virtual bool RecvOverfill(const uint32_t &aOverfill) MOZ_OVERRIDE;
  void AddOverfillObserver(ClientLayerManager* aLayerManager);

  virtual bool RecvDidComposite(const uint64_t& aId, const uint64_t& aTransactionId) MOZ_OVERRIDE;

private:
  // Private destructor, to discourage deletion outside of Release():
  virtual ~CompositorChild();

  virtual PLayerTransactionChild*
    AllocPLayerTransactionChild(const nsTArray<LayersBackend>& aBackendHints,
                                const uint64_t& aId,
                                TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                bool* aSuccess) MOZ_OVERRIDE;

  virtual bool DeallocPLayerTransactionChild(PLayerTransactionChild *aChild) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool RecvSharedCompositorFrameMetrics(const mozilla::ipc::SharedMemoryBasic::Handle& metrics,
                                                const CrossProcessMutexHandle& handle,
                                                const uint32_t& aAPZCId) MOZ_OVERRIDE;

  virtual bool RecvReleaseSharedCompositorFrameMetrics(const ViewID& aId,
                                                       const uint32_t& aAPZCId) MOZ_OVERRIDE;

  // Class used to store the shared FrameMetrics, mutex, and APZCId  in a hash table
  class SharedFrameMetricsData {
  public:
    SharedFrameMetricsData(
        const mozilla::ipc::SharedMemoryBasic::Handle& metrics,
        const CrossProcessMutexHandle& handle,
        const uint32_t& aAPZCId);

    ~SharedFrameMetricsData();

    void CopyFrameMetrics(FrameMetrics* aFrame);
    FrameMetrics::ViewID GetViewID();
    uint32_t GetAPZCId();

  private:
    // Pointer to the class that allows access to the shared memory that contains
    // the shared FrameMetrics
    mozilla::ipc::SharedMemoryBasic* mBuffer;
    CrossProcessMutex* mMutex;
    // Unique ID of the APZC that is sharing the FrameMetrics
    uint32_t mAPZCId;
  };

  nsRefPtr<ClientLayerManager> mLayerManager;

  // The ViewID of the FrameMetrics is used as the key for this hash table.
  // While this should be safe to use since the ViewID is unique
  nsClassHashtable<nsUint64HashKey, SharedFrameMetricsData> mFrameMetricsTable;

  // When we're in a child process, this is the process-global
  // compositor that we use to forward transactions directly to the
  // compositor context in another process.
  static CompositorChild* sCompositor;

  DISALLOW_EVIL_CONSTRUCTORS(CompositorChild);

  // When we receive overfill numbers, notify these client layer managers
  nsAutoTArray<ClientLayerManager*,0> mOverfillObservers;
};

} // layers
} // mozilla

#endif // mozilla_layers_CompositorChild_h
