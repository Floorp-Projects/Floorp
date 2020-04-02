/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "Buffer.h"

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ipc/Shmem.h"
#include "nsContentUtils.h"
#include "nsWrapperCache.h"
#include "Device.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_JS_WRAP(Buffer)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(Buffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(Buffer, Release)
NS_IMPL_CYCLE_COLLECTION_CLASS(Buffer)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Buffer)
  tmp->Cleanup();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Buffer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Buffer)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  if (tmp->mMapping) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mMapping->mArrayBuffer)
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

Buffer::Mapping::Mapping(ipc::Shmem&& aShmem, JSObject* aArrayBuffer)
    : mShmem(MakeUnique<ipc::Shmem>(std::move(aShmem))),
      mArrayBuffer(aArrayBuffer) {}

Buffer::Buffer(Device* const aParent, RawId aId, BufferAddress aSize)
    : ChildOf(aParent), mId(aId), mSize(aSize) {
  mozilla::HoldJSObjects(this);
}

Buffer::~Buffer() {
  Cleanup();
  mozilla::DropJSObjects(this);
}

void Buffer::Cleanup() {
  if (mParent) {
    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      bridge->SendBufferDestroy(mId);
    }
  }
  mMapping.reset();
}

void Buffer::InitMapping(ipc::Shmem&& aShmem, JSObject* aArrayBuffer) {
  mMapping.emplace(std::move(aShmem), aArrayBuffer);
}

already_AddRefed<dom::Promise> Buffer::MapReadAsync(ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  if (mMapping) {
    aRv.ThrowInvalidStateError("Unable to map a buffer that is already mapped");
    return nullptr;
  }
  const auto checked = CheckedInt<size_t>(mSize);
  if (!checked.isValid()) {
    aRv.ThrowRangeError("Mapped size is too large");
    return nullptr;
  }

  const auto& size = checked.value();
  RefPtr<Buffer> self(this);

  auto mappingPromise = mParent->MapBufferForReadAsync(mId, size, aRv);
  if (!mappingPromise) {
    return nullptr;
  }

  mappingPromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise, size, self](ipc::Shmem&& aShmem) {
        MOZ_ASSERT(aShmem.Size<uint8_t>() == size);
        dom::AutoJSAPI jsapi;
        if (!jsapi.Init(self->GetParentObject())) {
          promise->MaybeRejectWithAbortError("Owning page was unloaded!");
          return;
        }
        JS::Rooted<JSObject*> arrayBuffer(
            jsapi.cx(),
            Device::CreateExternalArrayBuffer(jsapi.cx(), size, aShmem));
        if (!arrayBuffer) {
          ErrorResult rv;
          rv.StealExceptionFromJSContext(jsapi.cx());
          promise->MaybeReject(std::move(rv));
          return;
        }
        JS::Rooted<JS::Value> val(jsapi.cx(), JS::ObjectValue(*arrayBuffer));
        self->mMapping.emplace(std::move(aShmem), arrayBuffer);
        promise->MaybeResolve(val);
      },
      [promise](const ipc::ResponseRejectReason&) {
        promise->MaybeRejectWithAbortError("Internal communication error!");
      });

  return promise.forget();
}

void Buffer::Unmap(JSContext* aCx, ErrorResult& aRv) {
  if (!mMapping) {
    return;
  }
  JS::Rooted<JSObject*> rooted(aCx, mMapping->mArrayBuffer);
  bool ok = JS::DetachArrayBuffer(aCx, rooted);
  if (!ok) {
    aRv.NoteJSContextException(aCx);
    return;
  }
  mParent->UnmapBuffer(mId, std::move(mMapping->mShmem));
  mMapping.reset();
}

}  // namespace webgpu
}  // namespace mozilla
