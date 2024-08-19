
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLQUEUEPARAMTRAITS_H_
#define WEBGLQUEUEPARAMTRAITS_H_

#include <type_traits>

#include "ipc/EnumSerializer.h"
#include "TexUnpackBlob.h"
#include "WebGLTypes.h"

namespace mozilla {
namespace webgl {
template <typename T>
struct QueueParamTraits;

// -

#define USE_TIED_FIELDS(T) \
  template <>              \
  struct QueueParamTraits<T> : QueueParamTraits_TiedFields<T> {};

// -

USE_TIED_FIELDS(layers::RemoteTextureId)
USE_TIED_FIELDS(layers::RemoteTextureOwnerId)
USE_TIED_FIELDS(WebGLContextOptions)
USE_TIED_FIELDS(webgl::PixelUnpackStateWebgl)
USE_TIED_FIELDS(webgl::SwapChainOptions)
USE_TIED_FIELDS(webgl::ReadPixelsDesc)
USE_TIED_FIELDS(webgl::VertAttribPointerDesc)
USE_TIED_FIELDS(webgl::PackingInfo)
USE_TIED_FIELDS(webgl::TypedQuad)
USE_TIED_FIELDS(webgl::PixelPackingState)
USE_TIED_FIELDS(FloatOrInt)

}  // namespace webgl
template <>
inline auto TiedFields<gfx::IntSize>(gfx::IntSize& a) {
  return std::tie(a.width, a.height);
}
namespace webgl {
USE_TIED_FIELDS(gfx::IntSize)

// -

#undef USE_TIED_FIELDS

// -

template <class T>
struct QueueParamTraits<avec2<T>> : QueueParamTraits_TiedFields<avec2<T>> {};

template <class T>
struct QueueParamTraits<avec3<T>> : QueueParamTraits_TiedFields<avec3<T>> {};

// ---------------------------------------------------------------------
// Enums!

}  // namespace webgl

inline constexpr bool IsEnumCase(const webgl::AttribBaseType raw) {
  switch (raw) {
    case webgl::AttribBaseType::Boolean:
    case webgl::AttribBaseType::Float:
    case webgl::AttribBaseType::Int:
    case webgl::AttribBaseType::Uint:
      return true;
  }
  return false;
}
static_assert(IsEnumCase(webgl::AttribBaseType(3)));
static_assert(!IsEnumCase(webgl::AttribBaseType(4)));
static_assert(!IsEnumCase(webgl::AttribBaseType(5)));

namespace webgl {

#define USE_IS_ENUM_CASE(T) \
  template <>               \
  struct QueueParamTraits<T> : QueueParamTraits_IsEnumCase<T> {};

USE_IS_ENUM_CASE(webgl::AttribBaseType)
USE_IS_ENUM_CASE(webgl::ProvokingVertex)

#undef USE_IS_ENUM_CASE

// ---------------------------------------------------------------------
// Custom QueueParamTraits

template <typename T>
struct QueueParamTraits<Span<T>> {
  template <typename U>
  static bool Write(ProducerView<U>& view, const Span<T>& in) {
    const auto& elemCount = in.size();
    auto status = view.WriteParam(elemCount);
    if (!status) return status;

    if (!elemCount) return status;
    status = view.WriteFromRange(Range<const T>{in});

    return status;
  }

  template <typename U>
  static bool Read(ConsumerView<U>& view, Span<const T>* const out) {
    size_t elemCount = 0;
    auto status = view.ReadParam(&elemCount);
    if (!status) return status;

    if (!elemCount) {
      *out = {};
      return true;
    }

    auto data = view.template ReadRange<const T>(elemCount);
    if (!data) return false;
    *out = Span{*data};
    return true;
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
  static bool Write(ProducerView<U>& aProducerView, const T& aArg) {
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
  static bool Read(ConsumerView<U>& aConsumerView, T* aArg) {
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
  static bool Write(ProducerView<U>& aProducerView, const T& aArg) {
    const auto size = aArg.size();
    auto status = aProducerView.WriteParam(size);
    if (!status) return status;
    status = aProducerView.WriteFromRange(Range<const char>{aArg.data(), size});
    return status;
  }

  template <typename U>
  static bool Read(ConsumerView<U>& aConsumerView, T* aArg) {
    size_t size;
    auto status = aConsumerView.ReadParam(&size);
    if (!status) return status;

    const auto view = aConsumerView.template ReadRange<char>(size);
    if (!view) return false;
    aArg->assign(view->begin().get(), size);
    return status;
  }
};

template <typename U>
struct QueueParamTraits<std::vector<U>> {
  using T = std::vector<U>;

  template <typename V>
  static bool Write(ProducerView<V>& aProducerView, const T& aArg) {
    auto status = aProducerView.WriteParam(aArg.size());
    if (!status) return status;

    for (const auto& cur : aArg) {
      status = aProducerView.WriteParam(cur);
      if (!status) return status;
    }
    return status;
  }

  template <typename V>
  static bool Read(ConsumerView<V>& aConsumerView, T* aArg) {
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
  static bool Write(ProducerView<U>& aProducerView, const T& aArg) {
    aProducerView.WriteParam(aArg.pending);
    aProducerView.WriteParam(aArg.log);
    aProducerView.WriteParam(aArg.translatedSource);
    return aProducerView.WriteParam(aArg.success);
  }

  template <typename U>
  static bool Read(ConsumerView<U>& aConsumerView, T* aArg) {
    aConsumerView.ReadParam(&aArg->pending);
    aConsumerView.ReadParam(&aArg->log);
    aConsumerView.ReadParam(&aArg->translatedSource);
    return aConsumerView.ReadParam(&aArg->success);
  }
};

template <>
struct QueueParamTraits<mozilla::layers::TextureType>
    : public ContiguousEnumSerializer<mozilla::layers::TextureType,
                                      mozilla::layers::TextureType::Unknown,
                                      mozilla::layers::TextureType::Last> {};

template <>
struct QueueParamTraits<mozilla::gfx::SurfaceFormat>
    : public ContiguousEnumSerializerInclusive<
          mozilla::gfx::SurfaceFormat, mozilla::gfx::SurfaceFormat::B8G8R8A8,
          mozilla::gfx::SurfaceFormat::UNKNOWN> {};

template <>
struct QueueParamTraits<gfxAlphaType>
    : public ContiguousEnumSerializerInclusive<
          gfxAlphaType, gfxAlphaType::Opaque, gfxAlphaType::NonPremult> {};

// -

template <class Enum>
using WebIDLEnumQueueSerializer =
    ContiguousEnumSerializerInclusive<Enum, ContiguousEnumValues<Enum>::min,
                                      ContiguousEnumValues<Enum>::max>;

template <>
struct QueueParamTraits<dom::WebGLPowerPreference>
    : public WebIDLEnumQueueSerializer<dom::WebGLPowerPreference> {};
template <>
struct QueueParamTraits<dom::PredefinedColorSpace>
    : public WebIDLEnumQueueSerializer<dom::PredefinedColorSpace> {};

}  // namespace webgl
}  // namespace mozilla

#endif  // WEBGLQUEUEPARAMTRAITS_H_
