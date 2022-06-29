/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "Buffer.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/ipc/Shmem.h"
#include "ipc/WebGPUChild.h"
#include "js/ArrayBuffer.h"
#include "js/RootingAPI.h"
#include "nsContentUtils.h"
#include "nsWrapperCache.h"
#include "Device.h"

namespace mozilla::webgpu {

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
  if (tmp->mMapped) {
    for (uint32_t i = 0; i < tmp->mMapped->mArrayBuffers.Length(); ++i) {
      NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(
          mMapped->mArrayBuffers[i])
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

Buffer::Buffer(Device* const aParent, RawId aId, BufferAddress aSize,
               uint32_t aUsage)
    : ChildOf(aParent), mId(aId), mSize(aSize), mUsage(aUsage) {
  mozilla::HoldJSObjects(this);
}

Buffer::~Buffer() {
  Cleanup();
  mozilla::DropJSObjects(this);
}

bool Buffer::Mappable() const {
  return (mUsage & (dom::GPUBufferUsage_Binding::MAP_WRITE |
                    dom::GPUBufferUsage_Binding::MAP_READ)) != 0;
}

void Buffer::Cleanup() {
  if (mValid && mParent) {
    mValid = false;

    if (mMapped && !mMapped->mArrayBuffers.IsEmpty()) {
      // The array buffers could live longer than us and our shmem, so make sure
      // we clear the external buffer bindings.
      dom::AutoJSAPI jsapi;
      if (jsapi.Init(mParent->GetOwnerGlobal())) {
        IgnoredErrorResult rv;
        UnmapArrayBuffers(jsapi.cx(), rv);
      }
    }

    auto bridge = mParent->GetBridge();
    if (bridge && bridge->IsOpen()) {
      // Note: even if the buffer is considered mapped,
      // the shmem may be empty before the mapAsync callback
      // is resolved.
      if (mMapped && mMapped->mShmem.IsReadable()) {
        // Note: if the bridge is closed, all associated shmems are already
        // deleted.
        bridge->DeallocShmem(mMapped->mShmem);
      }
      bridge->SendBufferDestroy(mId);
    }
  }
}

void Buffer::SetMapped(ipc::Shmem&& aShmem, bool aWritable) {
  MOZ_ASSERT(!mMapped);
  mMapped.emplace();
  mMapped->mShmem = std::move(aShmem);
  mMapped->mWritable = aWritable;
}

already_AddRefed<dom::Promise> Buffer::MapAsync(
    uint32_t aMode, uint64_t aOffset, const dom::Optional<uint64_t>& aSize,
    ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  if (mMapped) {
    aRv.ThrowInvalidStateError("Unable to map a buffer that is already mapped");
    return nullptr;
  }

  switch (aMode) {
    case dom::GPUMapMode_Binding::READ:
      if ((mUsage & dom::GPUBufferUsage_Binding::MAP_READ) == 0) {
        promise->MaybeRejectWithOperationError(
            "mapAsync: 'mode' is GPUMapMode.READ, \
but GPUBuffer was not created with GPUBufferUsage.MAP_READ");
        return promise.forget();
      }
      break;
    case dom::GPUMapMode_Binding::WRITE:
      if ((mUsage & dom::GPUBufferUsage_Binding::MAP_WRITE) == 0) {
        promise->MaybeRejectWithOperationError(
            "mapAsync: 'mode' is GPUMapMode.WRITE, \
but GPUBuffer was not created with GPUBufferUsage.MAP_WRITE");
        return promise.forget();
      }
      break;
    default:
      promise->MaybeRejectWithOperationError(
          "GPUBuffer.mapAsync 'mode' argument \
must be either GPUMapMode.READ or GPUMapMode.WRITE");
      return promise.forget();
  }

  // Initialize with a dummy shmem, it will become real after the promise is
  // resolved.
  SetMapped(ipc::Shmem(), aMode == dom::GPUMapMode_Binding::WRITE);

  const auto checked = aSize.WasPassed() ? CheckedInt<size_t>(aSize.Value())
                                         : CheckedInt<size_t>(mSize) - aOffset;
  if (!checked.isValid()) {
    aRv.ThrowRangeError("Mapped size is too large");
    return nullptr;
  }

  const auto& size = checked.value();
  RefPtr<Buffer> self(this);

  auto mappingPromise = mParent->MapBufferAsync(mId, aMode, aOffset, size, aRv);
  if (!mappingPromise) {
    return nullptr;
  }

  mappingPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise, self](ipc::Shmem&& aShmem) {
        self->mMapped->mShmem = std::move(aShmem);
        promise->MaybeResolve(0);
      },
      [promise](const ipc::ResponseRejectReason&) {
        promise->MaybeRejectWithAbortError("Internal communication error!");
      });

  return promise.forget();
}

void Buffer::GetMappedRange(JSContext* aCx, uint64_t aOffset,
                            const dom::Optional<uint64_t>& aSize,
                            JS::Rooted<JSObject*>* aObject, ErrorResult& aRv) {
  const auto checkedOffset = CheckedInt<size_t>(aOffset);
  const auto checkedSize = aSize.WasPassed()
                               ? CheckedInt<size_t>(aSize.Value())
                               : CheckedInt<size_t>(mSize) - aOffset;
  const auto checkedMinBufferSize = checkedOffset + checkedSize;
  if (!checkedOffset.isValid() || !checkedSize.isValid() ||
      !checkedMinBufferSize.isValid()) {
    aRv.ThrowRangeError("Invalid mapped range");
    return;
  }
  if (!mMapped || !mMapped->IsReady()) {
    aRv.ThrowInvalidStateError("Buffer is not mapped");
    return;
  }
  if (checkedMinBufferSize.value() > mMapped->mShmem.Size<uint8_t>()) {
    aRv.ThrowOperationError("Mapped range exceeds buffer size");
    return;
  }

  auto* const arrayBuffer = mParent->CreateExternalArrayBuffer(
      aCx, checkedOffset.value(), checkedSize.value(), mMapped->mShmem);
  if (!arrayBuffer) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  aObject->set(arrayBuffer);
  mMapped->mArrayBuffers.AppendElement(*aObject);
}

void Buffer::UnmapArrayBuffers(JSContext* aCx, ErrorResult& aRv) {
  MOZ_ASSERT(mMapped);

  bool detachedArrayBuffers = true;
  for (const auto& arrayBuffer : mMapped->mArrayBuffers) {
    JS::Rooted<JSObject*> rooted(aCx, arrayBuffer);
    if (!JS::DetachArrayBuffer(aCx, rooted)) {
      detachedArrayBuffers = false;
    }
  };

  mMapped->mArrayBuffers.Clear();

  if (NS_WARN_IF(!detachedArrayBuffers)) {
    aRv.NoteJSContextException(aCx);
    return;
  }
}

void Buffer::Unmap(JSContext* aCx, ErrorResult& aRv) {
  if (!mMapped) {
    return;
  }

  UnmapArrayBuffers(aCx, aRv);
  mParent->UnmapBuffer(mId, std::move(mMapped->mShmem), mMapped->mWritable,
                       Mappable());
  mMapped.reset();
}

void Buffer::Destroy() {
  // TODO: we don't have to implement it right now, but it's used by the
  // examples
}

}  // namespace mozilla::webgpu
