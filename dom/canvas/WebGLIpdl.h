/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLIPDL_H_
#define WEBGLIPDL_H_

#include "mozilla/layers/LayersSurfaces.h"
#include "WebGLTypes.h"

namespace mozilla {
namespace webgl {

// TODO: This should probably replace Shmem, or at least this should move to
// ipc/glue.

class RaiiShmem final {
  RefPtr<mozilla::ipc::ActorLifecycleProxy> mWeakRef;
  mozilla::ipc::Shmem mShmem = {};

 public:
  /// Returns zeroed data.
  static RaiiShmem Alloc(mozilla::ipc::IProtocol* const allocator,
                         const size_t size,
                         const Shmem::SharedMemory::SharedMemoryType type) {
    mozilla::ipc::Shmem shmem;
    if (!allocator->AllocShmem(size, type, &shmem)) return {};
    return {allocator, shmem};
  }

  // -

  RaiiShmem() = default;

  RaiiShmem(mozilla::ipc::IProtocol* const allocator,
            const mozilla::ipc::Shmem& shmem) {
    if (!allocator || !allocator->CanSend()) {
      return;
    }

    // Shmems are handled by the top-level, so use that or we might leak after
    // the actor dies.
    mWeakRef = allocator->ToplevelProtocol()->GetLifecycleProxy();
    mShmem = shmem;
    if (!mWeakRef || !mWeakRef->Get() || !IsShmem()) {
      reset();
    }
  }

  void reset() {
    if (IsShmem()) {
      const auto& allocator = mWeakRef->Get();
      if (allocator) {
        allocator->DeallocShmem(mShmem);
      }
    }
    mWeakRef = nullptr;
    mShmem = {};
  }

  ~RaiiShmem() { reset(); }

  // -

  RaiiShmem(RaiiShmem&& rhs) { *this = std::move(rhs); }
  RaiiShmem& operator=(RaiiShmem&& rhs) {
    reset();
    mWeakRef = rhs.mWeakRef;
    mShmem = rhs.Extract();
    return *this;
  }

  // -

  bool IsShmem() const { return mShmem.IsReadable(); }

  explicit operator bool() const { return IsShmem(); }

  // -

  const auto& Shmem() const {
    MOZ_ASSERT(IsShmem());
    return mShmem;
  }

  Range<uint8_t> ByteRange() const {
    if (!IsShmem()) {
      return {};
    }
    return {mShmem.get<uint8_t>(), mShmem.Size<uint8_t>()};
  }

  mozilla::ipc::Shmem Extract() {
    auto ret = mShmem;
    mShmem = {};
    reset();
    return ret;
  }
};

using Int32Vector = std::vector<int32_t>;

}  // namespace webgl

namespace ipc {

template <>
struct IPDLParamTraits<mozilla::webgl::FrontBufferSnapshotIpc> final {
  using T = mozilla::webgl::FrontBufferSnapshotIpc;

  static void Write(IPC::Message* const msg, IProtocol* actor, T& in) {
    WriteParam(msg, in.surfSize);
    WriteIPDLParam(msg, actor, std::move(in.shmem));
  }

  static bool Read(const IPC::Message* const msg, PickleIterator* const itr,
                   IProtocol* actor, T* const out) {
    return ReadParam(msg, itr, &out->surfSize) &&
           ReadIPDLParam(msg, itr, actor, &out->shmem);
  }
};

// -

template <>
struct IPDLParamTraits<mozilla::webgl::ReadPixelsResultIpc> final {
  using T = mozilla::webgl::ReadPixelsResultIpc;

  static void Write(IPC::Message* const msg, IProtocol* actor, T& in) {
    WriteParam(msg, in.subrect);
    WriteParam(msg, in.byteStride);
    WriteIPDLParam(msg, actor, std::move(in.shmem));
  }

  static bool Read(const IPC::Message* const msg, PickleIterator* const itr,
                   IProtocol* actor, T* const out) {
    return ReadParam(msg, itr, &out->subrect) &&
           ReadParam(msg, itr, &out->byteStride) &&
           ReadIPDLParam(msg, itr, actor, &out->shmem);
  }
};

}  // namespace ipc

namespace webgl {
using Int32Vector = std::vector<int32_t>;
}  // namespace webgl
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::webgl::ContextLossReason>
    : public ContiguousEnumSerializerInclusive<
          mozilla::webgl::ContextLossReason,
          mozilla::webgl::ContextLossReason::None,
          mozilla::webgl::ContextLossReason::Guilty> {};

template <>
struct ParamTraits<mozilla::webgl::AttribBaseType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::webgl::AttribBaseType,
          mozilla::webgl::AttribBaseType::Boolean,
          mozilla::webgl::AttribBaseType::Uint> {};

// -

template <typename T>
bool ValidateParam(const T& val) {
  return ParamTraits<T>::Validate(val);
}

template <typename T>
struct ValidatedPlainOldDataSerializer : public PlainOldDataSerializer<T> {
  static void Write(Message* const msg, const T& in) {
    MOZ_ASSERT(ValidateParam(in));
    PlainOldDataSerializer<T>::Write(msg, in);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    if (!PlainOldDataSerializer<T>::Read(msg, itr, out)) return false;
    return ValidateParam(*out);
  }

  // static bool Validate(const T&) = 0;
};

// -

template <>
struct ParamTraits<mozilla::webgl::InitContextDesc> final
    : public ValidatedPlainOldDataSerializer<mozilla::webgl::InitContextDesc> {
  using T = mozilla::webgl::InitContextDesc;

  static bool Validate(const T& val) {
    return ValidateParam(val.options) && (val.size.x && val.size.y);
  }
};

template <>
struct ParamTraits<mozilla::WebGLContextOptions> final
    : public ValidatedPlainOldDataSerializer<mozilla::WebGLContextOptions> {
  using T = mozilla::WebGLContextOptions;

  static bool Validate(const T& val) {
    return ValidateParam(val.powerPreference);
  }
};

template <>
struct ParamTraits<mozilla::dom::WebGLPowerPreference> final
    : public ValidatedPlainOldDataSerializer<
          mozilla::dom::WebGLPowerPreference> {
  using T = mozilla::dom::WebGLPowerPreference;

  static bool Validate(const T& val) { return val <= T::High_performance; }
};

template <>
struct ParamTraits<mozilla::webgl::OpaqueFramebufferOptions> final
    : public PlainOldDataSerializer<mozilla::webgl::OpaqueFramebufferOptions> {
};

// -

template <>
struct ParamTraits<mozilla::webgl::InitContextResult> final {
  using T = mozilla::webgl::InitContextResult;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.error);
    WriteParam(msg, in.options);
    WriteParam(msg, in.limits);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->error) &&
           ReadParam(msg, itr, &out->options) &&
           ReadParam(msg, itr, &out->limits);
  }
};

template <>
struct ParamTraits<mozilla::webgl::ExtensionBits> final
    : public PlainOldDataSerializer<mozilla::webgl::ExtensionBits> {};

template <>
struct ParamTraits<mozilla::webgl::Limits> final
    : public PlainOldDataSerializer<mozilla::webgl::Limits> {};

// -

template <>
struct ParamTraits<mozilla::webgl::ReadPixelsDesc> final {
  using T = mozilla::webgl::ReadPixelsDesc;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.srcOffset);
    WriteParam(msg, in.size);
    WriteParam(msg, in.pi);
    WriteParam(msg, in.packState);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->srcOffset) &&
           ReadParam(msg, itr, &out->size) && ReadParam(msg, itr, &out->pi) &&
           ReadParam(msg, itr, &out->packState);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::PixelPackState> final {
  using T = mozilla::webgl::PixelPackState;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.alignment);
    WriteParam(msg, in.rowLength);
    WriteParam(msg, in.skipRows);
    WriteParam(msg, in.skipPixels);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->alignment) &&
           ReadParam(msg, itr, &out->rowLength) &&
           ReadParam(msg, itr, &out->skipRows) &&
           ReadParam(msg, itr, &out->skipPixels);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::PackingInfo> final {
  using T = mozilla::webgl::PackingInfo;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.format);
    WriteParam(msg, in.type);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->format) && ReadParam(msg, itr, &out->type);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::CompileResult> final {
  using T = mozilla::webgl::CompileResult;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.pending);
    WriteParam(msg, in.log);
    WriteParam(msg, in.translatedSource);
    WriteParam(msg, in.success);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->pending) &&
           ReadParam(msg, itr, &out->log) &&
           ReadParam(msg, itr, &out->translatedSource) &&
           ReadParam(msg, itr, &out->success);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::LinkResult> final {
  using T = mozilla::webgl::LinkResult;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.pending);
    WriteParam(msg, in.log);
    WriteParam(msg, in.success);
    WriteParam(msg, in.active);
    WriteParam(msg, in.tfBufferMode);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->pending) &&
           ReadParam(msg, itr, &out->log) &&
           ReadParam(msg, itr, &out->success) &&
           ReadParam(msg, itr, &out->active) &&
           ReadParam(msg, itr, &out->tfBufferMode);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::LinkActiveInfo> final {
  using T = mozilla::webgl::LinkActiveInfo;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.activeAttribs);
    WriteParam(msg, in.activeUniforms);
    WriteParam(msg, in.activeUniformBlocks);
    WriteParam(msg, in.activeTfVaryings);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->activeAttribs) &&
           ReadParam(msg, itr, &out->activeUniforms) &&
           ReadParam(msg, itr, &out->activeUniformBlocks) &&
           ReadParam(msg, itr, &out->activeTfVaryings);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::ActiveInfo> final {
  using T = mozilla::webgl::ActiveInfo;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.elemType);
    WriteParam(msg, in.elemCount);
    WriteParam(msg, in.name);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->elemType) &&
           ReadParam(msg, itr, &out->elemCount) &&
           ReadParam(msg, itr, &out->name);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::ActiveAttribInfo> final {
  using T = mozilla::webgl::ActiveAttribInfo;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, static_cast<const mozilla::webgl::ActiveInfo&>(in));
    WriteParam(msg, in.location);
    WriteParam(msg, in.baseType);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, static_cast<mozilla::webgl::ActiveInfo*>(out)) &&
           ReadParam(msg, itr, &out->location) &&
           ReadParam(msg, itr, &out->baseType);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::ActiveUniformInfo> final {
  using T = mozilla::webgl::ActiveUniformInfo;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, static_cast<const mozilla::webgl::ActiveInfo&>(in));
    WriteParam(msg, in.locByIndex);
    WriteParam(msg, in.block_index);
    WriteParam(msg, in.block_offset);
    WriteParam(msg, in.block_arrayStride);
    WriteParam(msg, in.block_matrixStride);
    WriteParam(msg, in.block_isRowMajor);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, static_cast<mozilla::webgl::ActiveInfo*>(out)) &&
           ReadParam(msg, itr, &out->locByIndex) &&
           ReadParam(msg, itr, &out->block_index) &&
           ReadParam(msg, itr, &out->block_offset) &&
           ReadParam(msg, itr, &out->block_arrayStride) &&
           ReadParam(msg, itr, &out->block_matrixStride) &&
           ReadParam(msg, itr, &out->block_isRowMajor);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::ActiveUniformBlockInfo> final {
  using T = mozilla::webgl::ActiveUniformBlockInfo;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.name);
    WriteParam(msg, in.dataSize);
    WriteParam(msg, in.activeUniformIndices);
    WriteParam(msg, in.referencedByVertexShader);
    WriteParam(msg, in.referencedByFragmentShader);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->name) &&
           ReadParam(msg, itr, &out->dataSize) &&
           ReadParam(msg, itr, &out->activeUniformIndices) &&
           ReadParam(msg, itr, &out->referencedByVertexShader) &&
           ReadParam(msg, itr, &out->referencedByFragmentShader);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::ShaderPrecisionFormat> final {
  using T = mozilla::webgl::ShaderPrecisionFormat;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.rangeMin);
    WriteParam(msg, in.rangeMax);
    WriteParam(msg, in.precision);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->rangeMin) &&
           ReadParam(msg, itr, &out->rangeMax) &&
           ReadParam(msg, itr, &out->precision);
  }
};

// -

template <typename U, size_t N>
struct ParamTraits<U[N]> final {
  using T = U[N];
  static constexpr size_t kByteSize = sizeof(U) * N;

  static_assert(std::is_trivial<U>::value);

  static void Write(Message* const msg, const T& in) {
    msg->WriteBytes(in, kByteSize);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    if (!msg->HasBytesAvailable(itr, kByteSize)) {
      return false;
    }
    return msg->ReadBytesInto(itr, *out, kByteSize);
  }
};

// -

template <>
struct ParamTraits<mozilla::webgl::GetUniformData> final {
  using T = mozilla::webgl::GetUniformData;

  static void Write(Message* const msg, const T& in) {
    ParamTraits<decltype(in.data)>::Write(msg, in.data);
    WriteParam(msg, in.type);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ParamTraits<decltype(out->data)>::Read(msg, itr, &out->data) &&
           ReadParam(msg, itr, &out->type);
  }
};

// -

template <typename U>
struct ParamTraits<mozilla::avec2<U>> final {
  using T = mozilla::avec2<U>;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.x);
    WriteParam(msg, in.y);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->x) && ReadParam(msg, itr, &out->y);
  }
};

// -

template <typename U>
struct ParamTraits<mozilla::avec3<U>> final {
  using T = mozilla::avec3<U>;

  static void Write(Message* const msg, const T& in) {
    WriteParam(msg, in.x);
    WriteParam(msg, in.y);
    WriteParam(msg, in.z);
  }

  static bool Read(const Message* const msg, PickleIterator* const itr,
                   T* const out) {
    return ReadParam(msg, itr, &out->x) && ReadParam(msg, itr, &out->y) &&
           ReadParam(msg, itr, &out->z);
  }
};

}  // namespace IPC

#endif
