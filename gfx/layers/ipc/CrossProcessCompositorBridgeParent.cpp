/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/CrossProcessCompositorBridgeParent.h"
#include <stdint.h>                     // for uint64_t
#include "LayerTransactionParent.h"     // for LayerTransactionParent
#include "base/message_loop.h"          // for MessageLoop
#include "base/task.h"                  // for CancelableTask, etc
#include "base/thread.h"                // for Thread
#include "mozilla/ipc/Transport.h"      // for Transport
#include "mozilla/layers/APZCTreeManager.h"  // for APZCTreeManager
#include "mozilla/layers/APZCTreeManagerParent.h"  // for APZCTreeManagerParent
#include "mozilla/layers/APZThreadUtils.h"  // for APZCTreeManager
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layers/PLayerTransactionParent.h"
#include "mozilla/layers/RemoteContentController.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsTArray.h"                   // for nsTArray
#include "nsXULAppAPI.h"                // for XRE_GetIOMessageLoop
#include "mozilla/Unused.h"
#include "mozilla/StaticPtr.h"

using namespace std;

namespace mozilla {

namespace layers {

// defined in CompositorBridgeParent.cpp
typedef map<uint64_t, CompositorBridgeParent::LayerTreeState> LayerTreeMap;
extern LayerTreeMap sIndirectLayerTrees;
extern StaticAutoPtr<mozilla::Monitor> sIndirectLayerTreesLock;
void UpdateIndirectTree(uint64_t aId, Layer* aRoot, const TargetConfig& aTargetConfig);
void EraseLayerState(uint64_t aId);

mozilla::ipc::IPCResult
CrossProcessCompositorBridgeParent::RecvRequestNotifyAfterRemotePaint()
{
  mNotifyAfterRemotePaint = true;
  return IPC_OK();
}

void
CrossProcessCompositorBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // We must keep this object alive untill the code handling message
  // reception is finished on this thread.
  MessageLoop::current()->PostTask(NewRunnableMethod(this, &CrossProcessCompositorBridgeParent::DeferredDestroy));
}

PLayerTransactionParent*
CrossProcessCompositorBridgeParent::AllocPLayerTransactionParent(
  const nsTArray<LayersBackend>&,
  const uint64_t& aId,
  TextureFactoryIdentifier* aTextureFactoryIdentifier,
  bool *aSuccess)
{
  MOZ_ASSERT(aId != 0);

  // Check to see if this child process has access to this layer tree.
  if (!LayerTreeOwnerTracker::Get()->IsMapped(aId, OtherPid())) {
    NS_ERROR("Unexpected layers id in AllocPLayerTransactionParent; dropping message...");
    return nullptr;
  }

  MonitorAutoLock lock(*sIndirectLayerTreesLock);

  CompositorBridgeParent::LayerTreeState* state = nullptr;
  LayerTreeMap::iterator itr = sIndirectLayerTrees.find(aId);
  if (sIndirectLayerTrees.end() != itr) {
    state = &itr->second;
  }

  if (state && state->mLayerManager) {
    state->mCrossProcessParent = this;
    HostLayerManager* lm = state->mLayerManager;
    *aTextureFactoryIdentifier = lm->GetCompositor()->GetTextureFactoryIdentifier();
    *aSuccess = true;
    LayerTransactionParent* p = new LayerTransactionParent(lm, this, aId);
    p->AddIPDLReference();
    sIndirectLayerTrees[aId].mLayerTree = p;
    p->SetPendingCompositorUpdates(state->mPendingCompositorUpdates);
    return p;
  }

  NS_WARNING("Created child without a matching parent?");
  // XXX: should be false, but that causes us to fail some tests on Mac w/ OMTC.
  // Bug 900745. change *aSuccess to false to see test failures.
  *aSuccess = true;
  LayerTransactionParent* p = new LayerTransactionParent(nullptr, this, aId);
  p->AddIPDLReference();
  return p;
}

bool
CrossProcessCompositorBridgeParent::DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers)
{
  LayerTransactionParent* slp = static_cast<LayerTransactionParent*>(aLayers);
  EraseLayerState(slp->GetId());
  static_cast<LayerTransactionParent*>(aLayers)->ReleaseIPDLReference();
  return true;
}

mozilla::ipc::IPCResult
CrossProcessCompositorBridgeParent::RecvAsyncPanZoomEnabled(const uint64_t& aLayersId, bool* aHasAPZ)
{
  // Check to see if this child process has access to this layer tree.
  if (!LayerTreeOwnerTracker::Get()->IsMapped(aLayersId, OtherPid())) {
    NS_ERROR("Unexpected layers id in RecvAsyncPanZoomEnabled; dropping message...");
    return IPC_FAIL_NO_REASON(this);
  }

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& state = sIndirectLayerTrees[aLayersId];

  *aHasAPZ = state.mParent ? state.mParent->AsyncPanZoomEnabled() : false;
  return IPC_OK();
}

PAPZCTreeManagerParent*
CrossProcessCompositorBridgeParent::AllocPAPZCTreeManagerParent(const uint64_t& aLayersId)
{
  // Check to see if this child process has access to this layer tree.
  if (!LayerTreeOwnerTracker::Get()->IsMapped(aLayersId, OtherPid())) {
    NS_ERROR("Unexpected layers id in AllocPAPZCTreeManagerParent; dropping message...");
    return nullptr;
  }

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& state = sIndirectLayerTrees[aLayersId];
  MOZ_ASSERT(state.mParent);
  MOZ_ASSERT(!state.mApzcTreeManagerParent);
  state.mApzcTreeManagerParent = new APZCTreeManagerParent(aLayersId, state.mParent->GetAPZCTreeManager());

  return state.mApzcTreeManagerParent;
}
bool
CrossProcessCompositorBridgeParent::DeallocPAPZCTreeManagerParent(PAPZCTreeManagerParent* aActor)
{
  APZCTreeManagerParent* parent = static_cast<APZCTreeManagerParent*>(aActor);

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  auto iter = sIndirectLayerTrees.find(parent->LayersId());
  if (iter != sIndirectLayerTrees.end()) {
    CompositorBridgeParent::LayerTreeState& state = iter->second;
    MOZ_ASSERT(state.mApzcTreeManagerParent == parent);
    state.mApzcTreeManagerParent = nullptr;
  }

  delete parent;

  return true;
}

PAPZParent*
CrossProcessCompositorBridgeParent::AllocPAPZParent(const uint64_t& aLayersId)
{
  // Check to see if this child process has access to this layer tree.
  if (!LayerTreeOwnerTracker::Get()->IsMapped(aLayersId, OtherPid())) {
    NS_ERROR("Unexpected layers id in AllocPAPZParent; dropping message...");
    return nullptr;
  }

  RemoteContentController* controller = new RemoteContentController();

  // Increment the controller's refcount before we return it. This will keep the
  // controller alive until it is released by IPDL in DeallocPAPZParent.
  controller->AddRef();

  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& state = sIndirectLayerTrees[aLayersId];
  MOZ_ASSERT(!state.mController);
  state.mController = controller;

  return controller;
}

bool
CrossProcessCompositorBridgeParent::DeallocPAPZParent(PAPZParent* aActor)
{
  RemoteContentController* controller = static_cast<RemoteContentController*>(aActor);
  controller->Release();
  return true;
}

mozilla::ipc::IPCResult
CrossProcessCompositorBridgeParent::RecvNotifyChildCreated(const uint64_t& child)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  for (LayerTreeMap::iterator it = sIndirectLayerTrees.begin();
       it != sIndirectLayerTrees.end(); it++) {
    CompositorBridgeParent::LayerTreeState* lts = &it->second;
    if (lts->mParent && lts->mCrossProcessParent == this) {
      lts->mParent->NotifyChildCreated(child);
      return IPC_OK();
    }
  }
  return IPC_FAIL_NO_REASON(this);
}

void
CrossProcessCompositorBridgeParent::ShadowLayersUpdated(
  LayerTransactionParent* aLayerTree,
  const TransactionInfo& aInfo,
  bool aHitTestUpdate)
{
  uint64_t id = aLayerTree->GetId();

  MOZ_ASSERT(id != 0);

  CompositorBridgeParent::LayerTreeState* state =
    CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state) {
    return;
  }
  MOZ_ASSERT(state->mParent);
  state->mParent->ScheduleRotationOnCompositorThread(
    aInfo.targetConfig(),
    aInfo.isFirstPaint());

  Layer* shadowRoot = aLayerTree->GetRoot();
  if (shadowRoot) {
    CompositorBridgeParent::SetShadowProperties(shadowRoot);
  }
  UpdateIndirectTree(id, shadowRoot, aInfo.targetConfig());

  // Cache the plugin data for this remote layer tree
  state->mPluginData = aInfo.plugins();
  state->mUpdatedPluginDataAvailable = true;

  state->mParent->NotifyShadowTreeTransaction(
    id,
    aInfo.isFirstPaint(),
    aInfo.scheduleComposite(),
    aInfo.paintSequenceNumber(),
    aInfo.isRepeatTransaction(),
    aHitTestUpdate);

  // Send the 'remote paint ready' message to the content thread if it has already asked.
  if(mNotifyAfterRemotePaint)  {
    Unused << SendRemotePaintIsReady();
    mNotifyAfterRemotePaint = false;
  }

  if (aLayerTree->ShouldParentObserveEpoch()) {
    // Note that we send this through the window compositor, since this needs
    // to reach the widget owning the tab.
    Unused << state->mParent->SendObserveLayerUpdate(id, aLayerTree->GetChildEpoch(), true);
  }

  aLayerTree->SetPendingTransactionId(aInfo.id());
}

void
CrossProcessCompositorBridgeParent::DidComposite(
  uint64_t aId,
  TimeStamp& aCompositeStart,
  TimeStamp& aCompositeEnd)
{
  sIndirectLayerTreesLock->AssertCurrentThreadOwns();
  if (LayerTransactionParent *layerTree = sIndirectLayerTrees[aId].mLayerTree) {
    Unused << SendDidComposite(aId, layerTree->GetPendingTransactionId(), aCompositeStart, aCompositeEnd);
    layerTree->SetPendingTransactionId(0);
  }
}

void
CrossProcessCompositorBridgeParent::ForceComposite(LayerTransactionParent* aLayerTree)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  CompositorBridgeParent* parent;
  { // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    parent = sIndirectLayerTrees[id].mParent;
  }
  if (parent) {
    parent->ForceComposite(aLayerTree);
  }
}

void
CrossProcessCompositorBridgeParent::NotifyClearCachedResources(LayerTransactionParent* aLayerTree)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);

  const CompositorBridgeParent::LayerTreeState* state =
    CompositorBridgeParent::GetIndirectShadowTree(id);
  if (state && state->mParent) {
    // Note that we send this through the window compositor, since this needs
    // to reach the widget owning the tab.
    Unused << state->mParent->SendObserveLayerUpdate(id, aLayerTree->GetChildEpoch(), false);
  }
}

bool
CrossProcessCompositorBridgeParent::SetTestSampleTime(
  LayerTransactionParent* aLayerTree, const TimeStamp& aTime)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  const CompositorBridgeParent::LayerTreeState* state =
    CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state) {
    return false;
  }

  MOZ_ASSERT(state->mParent);
  return state->mParent->SetTestSampleTime(aLayerTree, aTime);
}

void
CrossProcessCompositorBridgeParent::LeaveTestMode(LayerTransactionParent* aLayerTree)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  const CompositorBridgeParent::LayerTreeState* state =
    CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state) {
    return;
  }

  MOZ_ASSERT(state->mParent);
  state->mParent->LeaveTestMode(aLayerTree);
}

void
CrossProcessCompositorBridgeParent::ApplyAsyncProperties(
    LayerTransactionParent* aLayerTree)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  const CompositorBridgeParent::LayerTreeState* state =
    CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state) {
    return;
  }

  MOZ_ASSERT(state->mParent);
  state->mParent->ApplyAsyncProperties(aLayerTree);
}

void
CrossProcessCompositorBridgeParent::FlushApzRepaints(const LayerTransactionParent* aLayerTree)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  const CompositorBridgeParent::LayerTreeState* state =
    CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state) {
    return;
  }

  MOZ_ASSERT(state->mParent);
  state->mParent->FlushApzRepaints(aLayerTree);
}

void
CrossProcessCompositorBridgeParent::GetAPZTestData(
  const LayerTransactionParent* aLayerTree,
  APZTestData* aOutData)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  *aOutData = sIndirectLayerTrees[id].mApzTestData;
}

void
CrossProcessCompositorBridgeParent::SetConfirmedTargetAPZC(
  const LayerTransactionParent* aLayerTree,
  const uint64_t& aInputBlockId,
  const nsTArray<ScrollableLayerGuid>& aTargets)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  const CompositorBridgeParent::LayerTreeState* state =
    CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state || !state->mParent) {
    return;
  }

  state->mParent->SetConfirmedTargetAPZC(aLayerTree, aInputBlockId, aTargets);
}

AsyncCompositionManager*
CrossProcessCompositorBridgeParent::GetCompositionManager(LayerTransactionParent* aLayerTree)
{
  uint64_t id = aLayerTree->GetId();
  const CompositorBridgeParent::LayerTreeState* state =
    CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state) {
    return nullptr;
  }

  MOZ_ASSERT(state->mParent);
  return state->mParent->GetCompositionManager(aLayerTree);
}

mozilla::ipc::IPCResult
CrossProcessCompositorBridgeParent::RecvAcknowledgeCompositorUpdate(const uint64_t& aLayersId)
{
  MonitorAutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& state = sIndirectLayerTrees[aLayersId];

  if (LayerTransactionParent* ltp = state.mLayerTree) {
    ltp->AcknowledgeCompositorUpdate();
  }
  MOZ_ASSERT(state.mPendingCompositorUpdates > 0);
  state.mPendingCompositorUpdates--;
  return IPC_OK();
}

void
CrossProcessCompositorBridgeParent::DeferredDestroy()
{
  mCompositorThreadHolder = nullptr;
  mSelfRef = nullptr;
}

CrossProcessCompositorBridgeParent::~CrossProcessCompositorBridgeParent()
{
  MOZ_ASSERT(XRE_GetIOMessageLoop());
  MOZ_ASSERT(IToplevelProtocol::GetTransport());
}

PTextureParent*
CrossProcessCompositorBridgeParent::AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                                        const LayersBackend& aLayersBackend,
                                                        const TextureFlags& aFlags,
                                                        const uint64_t& aId,
                                                        const uint64_t& aSerial)
{
  CompositorBridgeParent::LayerTreeState* state = nullptr;

  LayerTreeMap::iterator itr = sIndirectLayerTrees.find(aId);
  if (sIndirectLayerTrees.end() != itr) {
    state = &itr->second;
  }

  TextureFlags flags = aFlags;

  if (!state || state->mPendingCompositorUpdates) {
    // The compositor was recreated, and we're receiving layers updates for a
    // a layer manager that will soon be discarded or invalidated. We can't
    // return null because this will mess up deserialization later and we'll
    // kill the content process. Instead, we signal that the underlying
    // TextureHost should not attempt to access the compositor.
    flags |= TextureFlags::INVALID_COMPOSITOR;
  } else if (state->mLayerManager && state->mLayerManager->GetCompositor() &&
             aLayersBackend != state->mLayerManager->GetCompositor()->GetBackendType()) {
    gfxDevCrash(gfx::LogReason::PAllocTextureBackendMismatch) << "Texture backend is wrong";
  }

  return TextureHost::CreateIPDLActor(this, aSharedData, aLayersBackend, aFlags, aSerial);
}

bool
CrossProcessCompositorBridgeParent::DeallocPTextureParent(PTextureParent* actor)
{
  return TextureHost::DestroyIPDLActor(actor);
}

bool
CrossProcessCompositorBridgeParent::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

mozilla::ipc::IPCResult
CrossProcessCompositorBridgeParent::RecvClearApproximatelyVisibleRegions(const uint64_t& aLayersId,
                                                                         const uint32_t& aPresShellId)
{
  CompositorBridgeParent* parent;
  { // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    parent = sIndirectLayerTrees[aLayersId].mParent;
  }
  if (parent) {
    parent->ClearApproximatelyVisibleRegions(aLayersId, Some(aPresShellId));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
CrossProcessCompositorBridgeParent::RecvNotifyApproximatelyVisibleRegion(const ScrollableLayerGuid& aGuid,
                                                                         const CSSIntRegion& aRegion)
{
  CompositorBridgeParent* parent;
  { // scope lock
    MonitorAutoLock lock(*sIndirectLayerTreesLock);
    parent = sIndirectLayerTrees[aGuid.mLayersId].mParent;
  }
  if (parent) {
    if (!parent->RecvNotifyApproximatelyVisibleRegion(aGuid, aRegion)) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();;
  }
  return IPC_OK();
}

void
CrossProcessCompositorBridgeParent::UpdatePaintTime(LayerTransactionParent* aLayerTree, const TimeDuration& aPaintTime)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);

  CompositorBridgeParent::LayerTreeState* state =
    CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state || !state->mParent) {
    return;
  }

  state->mParent->UpdatePaintTime(aLayerTree, aPaintTime);
}

} // namespace layers
} // namespace mozilla
