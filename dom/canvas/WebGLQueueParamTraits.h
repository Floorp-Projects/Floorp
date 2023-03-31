
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLQUEUEPARAMTRAITS_H_
#define WEBGLQUEUEPARAMTRAITS_H_

#include <type_traits>

#include "ipc/EnumSerializer.h"
#include "TexUnpackBlob.h"
#include "WebGLContext.h"
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

inline constexpr bool IsEnumCase(const dom::WebGLPowerPreference raw) {
  switch (raw) {
    case dom::WebGLPowerPreference::Default:
    case dom::WebGLPowerPreference::Low_power:
    case dom::WebGLPowerPreference::High_performance:
      return true;
    case dom::WebGLPowerPreference::EndGuard_:
      break;
  }
  return false;
}

inline constexpr bool IsEnumCase(const dom::PredefinedColorSpace raw) {
  switch (raw) {
    case dom::PredefinedColorSpace::Srgb:
    case dom::PredefinedColorSpace::Display_p3:
      return true;
    case dom::PredefinedColorSpace::EndGuard_:
      break;
  }
  return false;
}

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

static_assert(IsEnumCase(dom::WebGLPowerPreference(2)));
static_assert(!IsEnumCase(dom::WebGLPowerPreference(3)));
static_assert(!IsEnumCase(dom::WebGLPowerPreference(5)));

#define USE_IS_ENUM_CASE(T) \
  template <>               \
  struct QueueParamTraits<T> : QueueParamTraits_IsEnumCase<T> {};

USE_IS_ENUM_CASE(dom::WebGLPowerPreference)
USE_IS_ENUM_CASE(dom::PredefinedColorSpace)
USE_IS_ENUM_CASE(webgl::AttribBaseType)

#undef USE_IS_ENUM_CASE

// ---------------------------------------------------------------------
// Custom QueueParamTraits

template <typename T>
struct QueueParamTraits<RawBuffer<T>> {
  using ParamType = RawBuffer<T>;

  template <typename U>
  static bool Write(ProducerView<U>& view, const ParamType& in) {
    const auto& elemCount = in.size();
    auto status = view.WriteParam(elemCount);
    if (!status) return status;
    if (!elemCount) return status;

    const auto& begin = in.begin();
    const bool hasData = static_cast<bool>(begin);
    status = view.WriteParam(hasData);
    if (!status) return status;
    if (!hasData) return status;

    status = view.WriteFromRange(in.Data());
    return status;
  }

  template <typename U>
  static bool Read(ConsumerView<U>& view, ParamType* const out) {
    size_t elemCount = 0;
    auto status = view.ReadParam(&elemCount);
    if (!status) return status;
    if (!elemCount) {
      *out = {};
      return true;
    }

    uint8_t hasData = 0;
    status = view.ReadParam(&hasData);
    if (!status) return status;
    if (!hasData) {
      auto temp = RawBuffer<T>{elemCount};
      *out = std::move(temp);
      return true;
    }

    auto data = view.template ReadRange<T>(elemCount);
    if (!data) return false;
    *out = std::move(RawBuffer<T>{*data});
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

}  // namespace webgl
}  // namespace mozilla

#endif  // WEBGLQUEUEPARAMTRAITS_H_
