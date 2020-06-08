/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLQUEUEPARAMTRAITS_H_
#define WEBGLQUEUEPARAMTRAITS_H_

#include <type_traits>

#include "mozilla/dom/ProducerConsumerQueue.h"
#include "TexUnpackBlob.h"
#include "WebGLContext.h"
#include "WebGLTypes.h"

namespace mozilla {

namespace webgl {
template <typename T>
struct QueueParamTraits;

template <>
struct IsTriviallySerializable<FloatOrInt> : std::true_type {};

template <>
struct IsTriviallySerializable<webgl::ShaderPrecisionFormat> : std::true_type {
};

template <>
struct IsTriviallySerializable<WebGLContextOptions> : std::true_type {};

template <>
struct IsTriviallySerializable<WebGLPixelStore> : std::true_type {};

template <>
struct IsTriviallySerializable<WebGLTexImageData> : std::true_type {};

template <>
struct IsTriviallySerializable<WebGLTexPboOffset> : std::true_type {};

template <>
struct IsTriviallySerializable<webgl::ExtensionBits> : std::true_type {};
template <>
struct IsTriviallySerializable<webgl::GetUniformData> : std::true_type {};

template <>
struct IsTriviallySerializable<ICRData> : std::true_type {};

template <>
struct IsTriviallySerializable<gfx::IntSize> : std::true_type {};

template <typename T>
struct IsTriviallySerializable<avec2<T>> : std::true_type {};
template <typename T>
struct IsTriviallySerializable<avec3<T>> : std::true_type {};

template <>
struct IsTriviallySerializable<webgl::TexUnpackBlob> : std::true_type {};

template <>
struct IsTriviallySerializable<webgl::TypedQuad> : std::true_type {};
template <>
struct IsTriviallySerializable<webgl::VertAttribPointerDesc> : std::true_type {
};
template <>
struct IsTriviallySerializable<webgl::ReadPixelsDesc> : std::true_type {};

template <typename T>
struct QueueParamTraits<RawBuffer<T>> {
  using ParamType = RawBuffer<T>;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    aProducerView.WriteParam(aArg.mLength);
    return (aArg.mLength > 0)
               ? aProducerView.Write(aArg.mData, aArg.mLength * sizeof(T))
               : aProducerView.GetStatus();
  }

  template <typename U, typename ElementType = typename std::remove_cv_t<
                            typename ParamType::ElementType>>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    size_t len;
    QueueStatus status = aConsumerView.ReadParam(&len);
    if (!status) {
      return status;
    }

    if (len == 0) {
      if (aArg) {
        aArg->mLength = 0;
        aArg->mData = nullptr;
      }
      return QueueStatus::kSuccess;
    }

    if (!aArg) {
      return aConsumerView.Read(nullptr, len * sizeof(T));
    }

    struct RawBufferReadMatcher {
      QueueStatus operator()(RefPtr<mozilla::ipc::SharedMemoryBasic>& smem) {
        if (!smem) {
          return QueueStatus::kFatalError;
        }
        mArg->mSmem = smem;
        mArg->mData = static_cast<ElementType*>(smem->memory());
        mArg->mLength = mLength;
        mArg->mOwnsData = false;
        return QueueStatus::kSuccess;
      }
      QueueStatus operator()() {
        mArg->mSmem = nullptr;
        ElementType* buf = new ElementType[mLength];
        mArg->mData = buf;
        mArg->mLength = mLength;
        mArg->mOwnsData = true;
        return mConsumerView.Read(buf, mLength * sizeof(T));
      }

      ConsumerView<U>& mConsumerView;
      ParamType* mArg;
      size_t mLength;
    };

    return aConsumerView.ReadVariant(
        len * sizeof(T), RawBufferReadMatcher{aConsumerView, aArg, len});
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    return aView.template MinSizeParam<size_t>() +
           aView.MinSizeBytes(aArg ? aArg->mLength * sizeof(T) : 0);
  }
};

template <>
struct QueueParamTraits<webgl::ContextLossReason>
    : public ContiguousEnumSerializerInclusive<
          webgl::ContextLossReason, webgl::ContextLossReason::None,
          webgl::ContextLossReason::Guilty> {};

template <typename V, typename E>
struct QueueParamTraits<Result<V, E>> {
  using T = Result<V, E>;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView, const T& aArg) {
    const auto ok = aArg.isOk();
    auto status = aProducerView.WriteParam(ok);
    if (!status) return status;
    if (ok) {
      status = aProducerView.WriteParam(aArg.unwrap());
    } else {
      status = aProducerView.WriteParam(aArg.unwrapErr());
    }
    return status;
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, T* const aArg) {
    bool ok;
    auto status = aConsumerView.ReadParam(&ok);
    if (!status) return status;
    if (ok) {
      V val;
      status = aConsumerView.ReadParam(&val);
      *aArg = val;
    } else {
      E val;
      status = aConsumerView.ReadParam(&val);
      *aArg = Err(val);
    }
    return status;
  }

  template <typename View>
  static size_t MinSize(View& aView, const T* const aArg) {
    auto size = aView.template MinSizeParam<bool>();
    if (aArg) {
      if (aArg->isOk()) {
        const auto& val = aArg->unwrap();
        size += aView.MinSizeParam(&val);
      } else {
        const auto& val = aArg->unwrapErr();
        size += aView.MinSizeParam(&val);
      }
    }
    return size;
  }
};

template <>
struct QueueParamTraits<std::string> {
  using T = std::string;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView, const T& aArg) {
    auto status = aProducerView.WriteParam(aArg.size());
    if (!status) return status;
    status = aProducerView.Write(aArg.data(), aArg.size());
    return status;
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, T* const aArg) {
    size_t size;
    auto status = aConsumerView.ReadParam(&size);
    if (!status) return status;

    const UniqueBuffer temp = malloc(size);
    const auto dest = static_cast<char*>(temp.get());
    if (!dest) return QueueStatus::kFatalError;

    status = aConsumerView.Read(dest, size);
    if (aArg) {
      aArg->assign(dest, size);
    }
    return status;
  }

  template <typename View>
  static size_t MinSize(View& aView, const T* const aArg) {
    auto size = aView.template MinSizeParam<size_t>();
    if (aArg) {
      size += aArg->size();
    }
    return size;
  }
};

template <typename U>
struct QueueParamTraits<std::vector<U>> {
  using T = std::vector<U>;

  template <typename V>
  static QueueStatus Write(ProducerView<V>& aProducerView, const T& aArg) {
    auto status = aProducerView.WriteParam(aArg.size());
    if (!status) return status;
    status = aProducerView.Write(aArg.data(), aArg.size());
    return status;
  }

  template <typename V>
  static QueueStatus Read(ConsumerView<V>& aConsumerView, T* const aArg) {
    MOZ_CRASH("no way to fallibly resize vectors without exceptions");
    size_t size;
    auto status = aConsumerView.ReadParam(&size);
    if (!status) return status;

    aArg->resize(size);
    status = aConsumerView.Read(aArg->data(), aArg->size());
    return status;
  }

  template <typename View>
  static size_t MinSize(View& aView, const T* const aArg) {
    auto size = aView.template MinSizeParam<size_t>();
    if (aArg) {
      size += aArg->size() * sizeof(U);
    }
    return size;
  }
};

template <>
struct QueueParamTraits<WebGLExtensionID>
    : public ContiguousEnumSerializer<WebGLExtensionID,
                                      WebGLExtensionID::ANGLE_instanced_arrays,
                                      WebGLExtensionID::Max> {};

template <>
struct QueueParamTraits<CompileResult> {
  using T = CompileResult;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView, const T& aArg) {
    aProducerView.WriteParam(aArg.pending);
    aProducerView.WriteParam(aArg.log);
    aProducerView.WriteParam(aArg.translatedSource);
    return aProducerView.WriteParam(aArg.success);
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, T* const aArg) {
    aConsumerView.ReadParam(aArg ? &aArg->pending : nullptr);
    aConsumerView.ReadParam(aArg ? &aArg->log : nullptr);
    aConsumerView.ReadParam(aArg ? &aArg->translatedSource : nullptr);
    return aConsumerView.ReadParam(aArg ? &aArg->success : nullptr);
  }

  template <typename View>
  static size_t MinSize(View& aView, const T* const aArg) {
    return aView.MinSizeParam(aArg ? &aArg->pending : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->log : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->translatedSource : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->success : nullptr);
  }
};

}  // namespace webgl
}  // namespace mozilla

#endif  // WEBGLQUEUEPARAMTRAITS_H_
