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
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla::webgpu {

GPU_IMPL_JS_WRAP(Buffer)

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
               uint32_t aUsage, ipc::WritableSharedMemoryMapping&& aShmem)
    : ChildOf(aParent), mId(aId), mSize(aSize), mUsage(aUsage) {
  mozilla::HoldJSObjects(this);
  mShmem =
      std::make_shared<ipc::WritableSharedMemoryMapping>(std::move(aShmem));
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
    // Create and return an invalid Buffer. This Buffer will have id 0 and
    // won't be sent in any messages to the parent.
    RefPtr<Buffer> buffer = new Buffer(aDevice, 0, aDesc.mSize, 0,
                                       ipc::WritableSharedMemoryMapping());

    // Track the invalid Buffer to ensure that ::Drop can untrack it later.
    aDevice->TrackBuffer(buffer.get());
    return buffer.forget();
  }

  RefPtr<WebGPUChild> actor = aDevice->GetBridge();

  auto handle = ipc::UnsafeSharedMemoryHandle();
  auto mapping = ipc::WritableSharedMemoryMapping();

  bool hasMapFlags = aDesc.mUsage & (dom::GPUBufferUsage_Binding::MAP_WRITE |
                                     dom::GPUBufferUsage_Binding::MAP_READ);

  bool allocSucceeded = false;
  if (hasMapFlags || aDesc.mMappedAtCreation) {
    // If shmem allocation fails, we continue and provide the parent side with
    // an empty shmem which it will interpret as an OOM situtation.
    const auto checked = CheckedInt<size_t>(aDesc.mSize);
    const size_t maxSize = WGPUMAX_BUFFER_SIZE;
    if (checked.isValid()) {
      size_t size = checked.value();

      if (size > 0 && size < maxSize) {
        auto maybeShmem = ipc::UnsafeSharedMemoryHandle::CreateAndMap(size);

        if (maybeShmem.isSome()) {
          allocSucceeded = true;
          handle = std::move(maybeShmem.ref().first);
          mapping = std::move(maybeShmem.ref().second);

          MOZ_RELEASE_ASSERT(mapping.Size() >= size);

          // zero out memory
          memset(mapping.Bytes().data(), 0, size);
        }
      }

      if (size == 0) {
        // Zero-sized buffers is a special case. We don't create a shmem since
        // allocating the memory would not make sense, however mappable null
        // buffers are allowed by the spec so we just pass the null handle which
        // in practice deserializes into a null handle on the parent side and
        // behaves like a zero-sized allocation.
        allocSucceeded = true;
      }
    }
  }

  // If mapped at creation and the shmem allocation failed, immediately throw
  // a range error and don't attempt to create the buffer.
  if (aDesc.mMappedAtCreation && !allocSucceeded) {
    aRv.ThrowRangeError("Allocation failed");
    return nullptr;
  }

  RawId id = actor->DeviceCreateBuffer(aDeviceId, aDesc, std::move(handle));

  RefPtr<Buffer> buffer =
      new Buffer(aDevice, id, aDesc.mSize, aDesc.mUsage, std::move(mapping));
  buffer->SetLabel(aDesc.mLabel);

  if (aDesc.mMappedAtCreation) {
    // Mapped at creation's raison d'être is write access, since the buffer is
    // being created and there isn't anything interesting to read in it yet.
    bool writable = true;
    buffer->SetMapped(0, aDesc.mSize, writable);
  }

  aDevice->TrackBuffer(buffer.get());

  return buffer.forget();
}

void Buffer::Drop() {
  if (!mValid) {
    return;
  }

  mValid = false;

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

  GetDevice().UntrackBuffer(this);

  if (GetDevice().IsBridgeAlive() && mId) {
    GetDevice().GetBridge()->SendBufferDrop(mId);
  }
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

        // mValid should be true or we should have called unmap while marking
        // the buffer invalid, causing the promise to be rejected and the branch
        // above to have early-returned.
        MOZ_RELEASE_ASSERT(self->mValid);

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

static void ExternalBufferFreeCallback(void* aContents, void* aUserData) {
  Unused << aContents;
  auto shm = static_cast<std::shared_ptr<ipc::WritableSharedMemoryMapping>*>(
      aUserData);
  delete shm;
}

void Buffer::GetMappedRange(JSContext* aCx, uint64_t aOffset,
                            const dom::Optional<uint64_t>& aSize,
                            JS::Rooted<JSObject*>* aObject, ErrorResult& aRv) {
  if (!mMapped) {
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

  auto offset = checkedOffset.value();
  auto size = checkedSize.value();
  auto span = mShmem->Bytes().Subspan(offset, size);

  std::shared_ptr<ipc::WritableSharedMemoryMapping>* userData =
      new std::shared_ptr<ipc::WritableSharedMemoryMapping>(mShmem);
  UniquePtr<void, JS::BufferContentsDeleter> dataPtr{
      span.data(), {&ExternalBufferFreeCallback, userData}};
  JS::Rooted<JSObject*> arrayBuffer(
      aCx, JS::NewExternalArrayBuffer(aCx, size, std::move(dataPtr)));
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
    // our shmem on this side.
    mShmem = std::make_shared<ipc::WritableSharedMemoryMapping>();
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

dom::GPUBufferMapState Buffer::MapState() const {
  // Implementation reference:
  // <https://gpuweb.github.io/gpuweb/#dom-gpubuffer-mapstate>.

  if (mMapped) {
    return dom::GPUBufferMapState::Mapped;
  }
  if (mMapRequest) {
    return dom::GPUBufferMapState::Pending;
  }
  return dom::GPUBufferMapState::Unmapped;
}

}  // namespace mozilla::webgpu
