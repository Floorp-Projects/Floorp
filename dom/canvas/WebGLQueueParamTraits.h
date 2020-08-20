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
struct IsTriviallySerializable<mozilla::webgl::PackingInfo> : std::true_type {};

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
  static QueueStatus Write(ProducerView<U>& view, const ParamType& in) {
    const auto& elemCount = in.size();
    auto status = view.WriteParam(elemCount);
    if (!status) return status;
    if (!elemCount) return status;

    const auto& begin = in.begin();
    const bool hasData = static_cast<bool>(begin);
    status = view.WriteParam(hasData);
    if (!status) return status;
    if (!hasData) return status;

    status = view.Write(begin, begin + elemCount);
    return status;
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& view, ParamType* const out) {
    size_t elemCount = 0;
    auto status = view.ReadParam(&elemCount);
    if (!status) return status;
    if (!elemCount) {
      *out = {};
      return QueueStatus::kSuccess;
    }

    uint8_t hasData = 0;
    status = view.ReadParam(&hasData);
    if (!status) return status;
    if (!hasData) {
      auto temp = RawBuffer<T>{elemCount};
      *out = std::move(temp);
      return QueueStatus::kSuccess;
    }

    auto buffer = UniqueBuffer::Alloc(elemCount * sizeof(T));
    if (!buffer) return QueueStatus::kOOMError;

    using MutT = std::remove_cv_t<T>;
    const auto begin = reinterpret_cast<MutT*>(buffer.get());
    const auto range = Range<MutT>{begin, elemCount};

    auto temp = RawBuffer<T>{range, std::move(buffer)};
    *out = std::move(temp);
    return view.Read(range.begin().get(), range.end().get());
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
  static QueueStatus Read(ConsumerView<U>& aConsumerView, T* aArg) {
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
};

template <>
struct QueueParamTraits<std::string> {
  using T = std::string;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView, const T& aArg) {
    auto status = aProducerView.WriteParam(aArg.size());
    if (!status) return status;
    status = aProducerView.Write(aArg.data(), aArg.data() + aArg.size());
    return status;
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, T* aArg) {
    size_t size;
    auto status = aConsumerView.ReadParam(&size);
    if (!status) return status;

    const UniqueBuffer temp = malloc(size);
    const auto dest = static_cast<char*>(temp.get());
    if (!dest) return QueueStatus::kFatalError;

    status = aConsumerView.Read(dest, dest + size);
    aArg->assign(dest, size);
    return status;
  }
};

template <typename U>
struct QueueParamTraits<std::vector<U>> {
  using T = std::vector<U>;

  template <typename V>
  static QueueStatus Write(ProducerView<V>& aProducerView, const T& aArg) {
    auto status = aProducerView.WriteParam(aArg.size());
    if (!status) return status;

    for (const auto& cur : aArg) {
      status = aProducerView.WriteParam(cur);
      if (!status) return status;
    }
    return status;
  }

  template <typename V>
  static QueueStatus Read(ConsumerView<V>& aConsumerView, T* aArg) {
    size_t size;
    auto status = aConsumerView.ReadParam(&size);
    if (!status) return status;
    aArg->resize(size);

    for (auto& cur : *aArg) {
      status = aConsumerView.ReadParam(&cur);
      if (!status) return status;
    }
    return status;
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
  static QueueStatus Read(ConsumerView<U>& aConsumerView, T* aArg) {
    aConsumerView.ReadParam(&aArg->pending);
    aConsumerView.ReadParam(&aArg->log);
    aConsumerView.ReadParam(&aArg->translatedSource);
    return aConsumerView.ReadParam(&aArg->success);
  }
};

}  // namespace webgl
}  // namespace mozilla

#endif  // WEBGLQUEUEPARAMTRAITS_H_
