/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OffscreenCanvasRenderingContext2D.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/dom/OffscreenCanvasRenderingContext2DBinding.h"
#include "mozilla/dom/OffscreenCanvas.h"

using namespace mozilla;

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(OffscreenCanvasRenderingContext2D,
                                   CanvasRenderingContext2D)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(OffscreenCanvasRenderingContext2D)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(CanvasRenderingContext2D)

// Need to use NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED
// and dummy trace since we're missing some _SKIPPABLE_ macros without
// SCRIPT_HOLDER.
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(
    OffscreenCanvasRenderingContext2D, CanvasRenderingContext2D)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(OffscreenCanvasRenderingContext2D)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(OffscreenCanvasRenderingContext2D)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(OffscreenCanvasRenderingContext2D)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_ADDREF_INHERITED(OffscreenCanvasRenderingContext2D,
                         CanvasRenderingContext2D)
NS_IMPL_RELEASE_INHERITED(OffscreenCanvasRenderingContext2D,
                          CanvasRenderingContext2D)

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(
    layers::LayersBackend aCompositorBackend)
    : CanvasRenderingContext2D(aCompositorBackend) {}

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D() =
    default;

JSObject* OffscreenCanvasRenderingContext2D::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return OffscreenCanvasRenderingContext2D_Binding::Wrap(aCx, this,
                                                         aGivenProto);
}

nsIGlobalObject* OffscreenCanvasRenderingContext2D::GetParentObject() const {
  return mOffscreenCanvas->GetOwnerGlobal();
}

NS_IMETHODIMP OffscreenCanvasRenderingContext2D::InitializeWithDrawTarget(
    nsIDocShell* aShell, NotNull<gfx::DrawTarget*> aTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void OffscreenCanvasRenderingContext2D::Commit(ErrorResult& aRv) {
  if (!mOffscreenCanvas->IsTransferredFromElement()) {
    return;
  }

  mOffscreenCanvas->CommitFrameToCompositor();
}

void OffscreenCanvasRenderingContext2D::AddZoneWaitingForGC() {
  JSObject* wrapper = GetWrapperPreserveColor();
  if (wrapper) {
    CycleCollectedJSRuntime::Get()->AddZoneWaitingForGC(
        JS::GetObjectZone(wrapper));
  }
}

void OffscreenCanvasRenderingContext2D::AddAssociatedMemory() {
  JSObject* wrapper = GetWrapperMaybeDead();
  if (wrapper) {
    JS::AddAssociatedMemory(wrapper, BindingJSObjectMallocBytes(this),
                            JS::MemoryUse::DOMBinding);
  }
}

void OffscreenCanvasRenderingContext2D::RemoveAssociatedMemory() {
  JSObject* wrapper = GetWrapperMaybeDead();
  if (wrapper) {
    JS::RemoveAssociatedMemory(wrapper, BindingJSObjectMallocBytes(this),
                               JS::MemoryUse::DOMBinding);
  }
}

size_t BindingJSObjectMallocBytes(OffscreenCanvasRenderingContext2D* aContext) {
  gfx::IntSize size = aContext->GetSize();

  // TODO: Bug 1552137: No memory will be allocated if either dimension is
  // greater than gfxPrefs::gfx_canvas_max_size(). We should check this here
  // too.

  CheckedInt<uint32_t> bytes =
      CheckedInt<uint32_t>(size.width) * size.height * 4;
  if (!bytes.isValid()) {
    return 0;
  }

  return bytes.value();
}

}  // namespace mozilla::dom
