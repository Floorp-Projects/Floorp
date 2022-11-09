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
  tmp->Drop();
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
               uint32_t aUsage, ipc::Shmem&& aShmem)
    : ChildOf(aParent), mId(aId), mSize(aSize), mUsage(aUsage), mShmem(aShmem) {
  mozilla::HoldJSObjects(this);
  MOZ_ASSERT(mParent);
}

Buffer::~Buffer() {
  Drop();
  mozilla::DropJSObjects(this);
}

already_AddRefed<Buffer> Buffer::Create(Device* aDevice, RawId aDeviceId,
                                        const dom::GPUBufferDescriptor& aDesc,
                                        ErrorResult& aRv) {
  if (aDevice->IsLost()) {
    RefPtr<Buffer> buffer =
        new Buffer(aDevice, 0, aDesc.mSize, 0, ipc::Shmem());
    return buffer.forget();
  }

  RefPtr<WebGPUChild> actor = aDevice->GetBridge();

  ipc::Shmem shmem;
  bool hasMapFlags = aDesc.mUsage & (dom::GPUBufferUsage_Binding::MAP_WRITE |
                                     dom::GPUBufferUsage_Binding::MAP_READ);
  if (hasMapFlags || aDesc.mMappedAtCreation) {
    const auto checked = CheckedInt<size_t>(aDesc.mSize);
    if (!checked.isValid()) {
      aRv.ThrowRangeError("Mappable size is too large");
      return nullptr;
    }
    size_t size = checked.value();
    if (size == 0) {
      // Can't send zero-sized shmems.
      size = 1;
    }

    if (!actor->AllocUnsafeShmem(size, &shmem)) {
      aRv.ThrowAbortError(
          nsPrintfCString("Unable to allocate shmem of size %" PRIuPTR, size));
      return nullptr;
    }

    // zero out memory
    memset(shmem.get<uint8_t>(), 0, size);
  }

  MaybeShmem maybeShmem = mozilla::null_t();
  if (shmem.IsReadable()) {
    maybeShmem = shmem;
  }
  RawId id = actor->DeviceCreateBuffer(aDeviceId, aDesc, std::move(maybeShmem));

  RefPtr<Buffer> buffer =
      new Buffer(aDevice, id, aDesc.mSize, aDesc.mUsage, std::move(shmem));
  if (aDesc.mMappedAtCreation) {
    // Mapped at creation's raison d'être is write access, since the buffer is
    // being created and there isn't anything interesting to read in it yet.
    bool writable = true;
    buffer->SetMapped(0, aDesc.mSize, writable);
  }

  return buffer.forget();
}

void Buffer::Drop() {
  AbortMapRequest();

  if (mMapped && !mMapped->mArrayBuffers.IsEmpty()) {
    // The array buffers could live longer than us and our shmem, so make sure
    // we clear the external buffer bindings.
    dom::AutoJSAPI jsapi;
    if (jsapi.Init(GetDevice().GetOwnerGlobal())) {
      IgnoredErrorResult rv;
      UnmapArrayBuffers(jsapi.cx(), rv);
    }
  }
  mMapped.reset();

  if (mValid && !GetDevice().IsLost()) {
    GetDevice().GetBridge()->SendBufferDrop(mId);
  }
  mValid = false;
}

void Buffer::SetMapped(BufferAddress aOffset, BufferAddress aSize,
                       bool aWritable) {
  MOZ_ASSERT(!mMapped);
  MOZ_RELEASE_ASSERT(aOffset <= mSize);
  MOZ_RELEASE_ASSERT(aSize <= mSize - aOffset);

  mMapped.emplace();
  mMapped->mWritable = aWritable;
  mMapped->mOffset = aOffset;
  mMapped->mSize = aSize;
}

already_AddRefed<dom::Promise> Buffer::MapAsync(
    uint32_t aMode, uint64_t aOffset, const dom::Optional<uint64_t>& aSize,
    ErrorResult& aRv) {
  RefPtr<dom::Promise> promise = dom::Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (GetDevice().IsLost()) {
    promise->MaybeRejectWithOperationError("Device Lost");
    return promise.forget();
  }

  if (mMapRequest) {
    promise->MaybeRejectWithOperationError("Buffer mapping is already pending");
    return promise.forget();
  }

  BufferAddress size = 0;
  if (aSize.WasPassed()) {
    size = aSize.Value();
  } else if (aOffset <= mSize) {
    // Default to passing the reminder of the buffer after the provided offset.
    size = mSize - aOffset;
  } else {
    // The provided offset is larger than the buffer size.
    // The parent side will handle the error, we can let the requested size be
    // zero.
  }

  RefPtr<Buffer> self(this);

  auto mappingPromise =
      GetDevice().GetBridge()->SendBufferMap(mId, aMode, aOffset, size);
  MOZ_ASSERT(mappingPromise);

  mMapRequest = promise;

  mappingPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise, self](BufferMapResult&& aResult) {
        // Unmap might have been called while the result was on the way back.
        if (promise->State() != dom::Promise::PromiseState::Pending) {
          return;
        }

        switch (aResult.type()) {
          case BufferMapResult::TBufferMapSuccess: {
            auto& success = aResult.get_BufferMapSuccess();
            self->mMapRequest = nullptr;
            self->SetMapped(success.offset(), success.size(),
                            success.writable());
            promise->MaybeResolve(0);
            break;
          }
          case BufferMapResult::TBufferMapError: {
            auto& error = aResult.get_BufferMapError();
            self->RejectMapRequest(promise, error.message());
            break;
          }
          default: {
            MOZ_CRASH("unreachable");
          }
        }
      },
      [promise](const ipc::ResponseRejectReason&) {
        promise->MaybeRejectWithAbortError("Internal communication error!");
      });

  return promise.forget();
}

void Buffer::GetMappedRange(JSContext* aCx, uint64_t aOffset,
                            const dom::Optional<uint64_t>& aSize,
                            JS::Rooted<JSObject*>* aObject, ErrorResult& aRv) {
  if (!mMapped || !mShmem.IsReadable()) {
    aRv.ThrowInvalidStateError("Buffer is not mapped");
    return;
  }

  const auto checkedOffset = CheckedInt<size_t>(aOffset);
  const auto checkedSize = aSize.WasPassed()
                               ? CheckedInt<size_t>(aSize.Value())
                               : CheckedInt<size_t>(mSize) - aOffset;
  const auto checkedMinBufferSize = checkedOffset + checkedSize;

  if (!checkedOffset.isValid() || !checkedSize.isValid() ||
      !checkedMinBufferSize.isValid() || aOffset < mMapped->mOffset ||
      checkedMinBufferSize.value() > mMapped->mOffset + mMapped->mSize) {
    aRv.ThrowRangeError("Invalid range");
    return;
  }

  auto* const arrayBuffer = GetDevice().CreateExternalArrayBuffer(
      aCx, checkedOffset.value(), checkedSize.value(), mShmem);
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

  AbortMapRequest();

  if (NS_WARN_IF(!detachedArrayBuffers)) {
    aRv.NoteJSContextException(aCx);
    return;
  }
}

void Buffer::RejectMapRequest(dom::Promise* aPromise, nsACString& message) {
  if (mMapRequest == aPromise) {
    mMapRequest = nullptr;
  }

  aPromise->MaybeRejectWithOperationError(message);
}

void Buffer::AbortMapRequest() {
  if (mMapRequest) {
    mMapRequest->MaybeRejectWithAbortError("Buffer unmapped");
  }
  mMapRequest = nullptr;
}

void Buffer::Unmap(JSContext* aCx, ErrorResult& aRv) {
  if (!mMapped) {
    return;
  }

  UnmapArrayBuffers(aCx, aRv);

  bool hasMapFlags = mUsage & (dom::GPUBufferUsage_Binding::MAP_WRITE |
                               dom::GPUBufferUsage_Binding::MAP_READ);

  if (!hasMapFlags) {
    // We get here if the buffer was mapped at creation without map flags.
    // It won't be possible to map the buffer again so we can get rid of
    // our shmem handle on this side. The parent side will deallocate it.
    mShmem = ipc::Shmem();
  }

  if (!GetDevice().IsLost()) {
    GetDevice().GetBridge()->SendBufferUnmap(GetDevice().mId, mId,
                                             mMapped->mWritable);
  }

  mMapped.reset();
}

void Buffer::Destroy(JSContext* aCx, ErrorResult& aRv) {
  if (mMapped) {
    Unmap(aCx, aRv);
  }

  if (!GetDevice().IsLost()) {
    GetDevice().GetBridge()->SendBufferDestroy(mId);
  }
  // TODO: we don't have to implement it right now, but it's used by the
  // examples
}

}  // namespace mozilla::webgpu
