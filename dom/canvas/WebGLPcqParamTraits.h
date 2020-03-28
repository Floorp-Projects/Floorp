/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLPCQPARAMTRAITS_H_
#define WEBGLPCQPARAMTRAITS_H_

#include <type_traits>

#include "mozilla/dom/ProducerConsumerQueue.h"
#include "TexUnpackBlob.h"
#include "WebGLContext.h"
#include "WebGLTypes.h"

namespace mozilla {

namespace webgl {
template <typename T>
struct PcqParamTraits;

template <>
struct IsTriviallySerializable<FloatOrInt> : TrueType {};

template <>
struct IsTriviallySerializable<webgl::ShaderPrecisionFormat> : TrueType {};

template <>
struct IsTriviallySerializable<WebGLContextOptions> : TrueType {};

template <>
struct IsTriviallySerializable<WebGLPixelStore> : TrueType {};

template <>
struct IsTriviallySerializable<WebGLTexImageData> : TrueType {};

template <>
struct IsTriviallySerializable<WebGLTexPboOffset> : TrueType {};

template <>
struct IsTriviallySerializable<webgl::ExtensionBits> : TrueType {};
template <>
struct IsTriviallySerializable<webgl::GetUniformData> : TrueType {};

template <>
struct IsTriviallySerializable<ICRData> : TrueType {};

template <>
struct IsTriviallySerializable<gfx::IntSize> : TrueType {};

template <typename T>
struct IsTriviallySerializable<avec2<T>> : TrueType {};
template <typename T>
struct IsTriviallySerializable<avec3<T>> : TrueType {};

template <>
struct IsTriviallySerializable<webgl::TexUnpackBlob> : TrueType {};
/*
template <>
struct PcqParamTraits<WebGLActiveInfo> {
  using ParamType = WebGLActiveInfo;

  static PcqStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    aProducerView.WriteParam(aArg.mElemCount);
    aProducerView.WriteParam(aArg.mElemType);
    aProducerView.WriteParam(aArg.mBaseUserName);
    aProducerView.WriteParam(aArg.mIsArray);
    aProducerView.WriteParam(aArg.mElemSize);
    aProducerView.WriteParam(aArg.mBaseMappedName);
    return aProducerView.WriteParam(aArg.mBaseType);
  }

  static PcqStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
    aConsumerView.ReadParam(aArg ? &aArg->mElemCount : nullptr);
    aConsumerView.ReadParam(aArg ? &aArg->mElemType : nullptr);
    aConsumerView.ReadParam(aArg ? &aArg->mBaseUserName : nullptr);
    aConsumerView.ReadParam(aArg ? &aArg->mIsArray : nullptr);
    aConsumerView.ReadParam(aArg ? &aArg->mElemSize : nullptr);
    aConsumerView.ReadParam(aArg ? &aArg->mBaseMappedName : nullptr);
    return aConsumerView.ReadParam(aArg ? &aArg->mBaseType : nullptr);
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    return aView.MinSizeParam(aArg ? &aArg->mElemCount : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->mElemType : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->mBaseUserName : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->mIsArray : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->mElemSize : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->mBaseMappedName : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->mBaseType : nullptr);
  }
};
*/
template <typename T>
struct PcqParamTraits<RawBuffer<T>> {
  using ParamType = RawBuffer<T>;

  static PcqStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    aProducerView.WriteParam(aArg.mLength);
    return (aArg.mLength > 0)
               ? aProducerView.Write(aArg.mData, aArg.mLength * sizeof(T))
               : aProducerView.Status();
  }

  template <
      typename ElementType = std::remove_cv_t<typename ParamType::ElementType>>
  static PcqStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
    size_t len;
    PcqStatus status = aConsumerView.ReadParam(&len);
    if (!status) {
      return status;
    }

    if (len == 0) {
      if (aArg) {
        aArg->mLength = 0;
        aArg->mData = nullptr;
      }
      return PcqStatus::Success;
    }

    if (!aArg) {
      return aConsumerView.Read(nullptr, len * sizeof(T));
    }

    struct RawBufferReadMatcher {
      PcqStatus operator()(RefPtr<mozilla::ipc::SharedMemoryBasic>& smem) {
        if (!smem) {
          return PcqStatus::PcqFatalError;
        }
        mArg->mSmem = smem;
        mArg->mData = static_cast<ElementType*>(smem->memory());
        mArg->mLength = mLength;
        mArg->mOwnsData = false;
        return PcqStatus::Success;
      }
      PcqStatus operator()() {
        mArg->mSmem = nullptr;
        ElementType* buf = new ElementType[mLength];
        mArg->mData = buf;
        mArg->mLength = mLength;
        mArg->mOwnsData = true;
        return mConsumerView.Read(buf, mLength * sizeof(T));
      }

      ConsumerView& mConsumerView;
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

/*
enum TexUnpackTypes : uint8_t { Bytes, Surface, Image, Pbo };

template <>
struct PcqParamTraits<webgl::TexUnpackBytes> {
  using ParamType = webgl::TexUnpackBytes;

  static PcqStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    // Write TexUnpackBlob base class, then the RawBuffer.
    aProducerView.WriteParam(static_cast<const webgl::TexUnpackBlob&>(aArg));
    return aProducerView.WriteParam(aArg.mPtr);
  }

  static PcqStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
    // Read TexUnpackBlob base class, then the RawBuffer.
    aConsumerView.ReadParam(static_cast<webgl::TexUnpackBlob*>(aArg));
    return aConsumerView.ReadParam(aArg ? &aArg->mPtr : nullptr);
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    return aView.MinSizeParam(static_cast<const webgl::TexUnpackBlob*>(aArg)) +
           aView.MinSizeParam(aArg ? &aArg->mPtr : nullptr);
  }
};

template <>
struct PcqParamTraits<webgl::TexUnpackSurface> {
  using ParamType = webgl::TexUnpackSurface;

  static PcqStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    aProducerView.WriteParam(static_cast<const webgl::TexUnpackBlob&>(aArg));
    aProducerView.WriteParam(aArg.mSize);
    aProducerView.WriteParam(aArg.mFormat);
    aProducerView.WriteParam(aArg.mData);
    return aProducerView.WriteParam(aArg.mStride);
  }

  static PcqStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
    aConsumerView.ReadParam(static_cast<webgl::TexUnpackBlob*>(aArg));
    aConsumerView.ReadParam(aArg ? &aArg->mSize : nullptr);
    aConsumerView.ReadParam(aArg ? &aArg->mFormat : nullptr);
    aConsumerView.ReadParam(aArg ? &aArg->mData : nullptr);
    return aConsumerView.ReadParam(aArg ? &aArg->mStride : nullptr);
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    return aView.MinSizeParam(static_cast<const webgl::TexUnpackBlob*>(aArg)) +
           aView.MinSizeParam(aArg ? &aArg->mSize : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->mFormat : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->mData : nullptr) +
           aView.MinSizeParam(aArg ? &aArg->mStride : nullptr);
  }
};

// Specialization of PcqParamTraits that adapts the TexUnpack type in order to
// efficiently convert types.  For example, a TexUnpackSurface may deserialize
// as a TexUnpackBytes.
template <>
struct PcqParamTraits<WebGLTexUnpackVariant> {
  using ParamType = WebGLTexUnpackVariant;

  static PcqStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    struct TexUnpackWriteMatcher {
      PcqStatus operator()(const UniquePtr<webgl::TexUnpackBytes>& x) {
        mProducerView.WriteParam(TexUnpackTypes::Bytes);
        return mProducerView.WriteParam(x);
      }
      PcqStatus operator()(const UniquePtr<webgl::TexUnpackSurface>& x) {
        mProducerView.WriteParam(TexUnpackTypes::Surface);
        return mProducerView.WriteParam(x);
      }
      PcqStatus operator()(const UniquePtr<webgl::TexUnpackImage>& x) {
        MOZ_ASSERT_UNREACHABLE("TODO:");
        return PcqStatus::PcqFatalError;
      }
      PcqStatus operator()(const WebGLTexPboOffset& x) {
        mProducerView.WriteParam(TexUnpackTypes::Pbo);
        return mProducerView.WriteParam(x);
      }
      ProducerView& mProducerView;
    };
    return aArg.match(TexUnpackWriteMatcher{aProducerView});
  }

  static PcqStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
    if (!aArg) {
      // Not a great estimate but we can't do much better.
      return aConsumerView.template ReadParam<TexUnpackTypes>();
    }
    TexUnpackTypes unpackType;
    if (!aConsumerView.ReadParam(&unpackType)) {
      return aConsumerView.Status();
    }
    switch (unpackType) {
      case TexUnpackTypes::Bytes:
        *aArg = AsVariant(UniquePtr<webgl::TexUnpackBytes>());
        return aConsumerView.ReadParam(
            &aArg->as<UniquePtr<webgl::TexUnpackBytes>>());
      case TexUnpackTypes::Surface:
        *aArg = AsVariant(UniquePtr<webgl::TexUnpackSurface>());
        return aConsumerView.ReadParam(
            &aArg->as<UniquePtr<webgl::TexUnpackSurface>>());
      case TexUnpackTypes::Image:
        MOZ_ASSERT_UNREACHABLE("TODO:");
        return PcqStatus::PcqFatalError;
      case TexUnpackTypes::Pbo:
        *aArg = AsVariant(WebGLTexPboOffset());
        return aConsumerView.ReadParam(&aArg->as<WebGLTexPboOffset>());
    }
    MOZ_ASSERT_UNREACHABLE("Illegal texture unpack type");
    return PcqStatus::PcqFatalError;
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    size_t ret = aView.template MinSizeParam<TexUnpackTypes>();
    if (!aArg) {
      return ret;
    }

    struct TexUnpackMinSizeMatcher {
      size_t operator()(const UniquePtr<webgl::TexUnpackBytes>& x) {
        return mView.MinSizeParam(&x);
      }
      size_t operator()(const UniquePtr<webgl::TexUnpackSurface>& x) {
        return mView.MinSizeParam(&x);
      }
      size_t operator()(const UniquePtr<webgl::TexUnpackImage>& x) {
        MOZ_ASSERT_UNREACHABLE("TODO:");
        return 0;
      }
      size_t operator()(const WebGLTexPboOffset& x) {
        return mView.MinSizeParam(&x);
      }
      View& mView;
    };
    return ret + aArg->match(TexUnpackMinSizeMatcher{aView});
  }
};
*/
template <>
struct PcqParamTraits<webgl::ContextLossReason> {
  using ParamType = webgl::ContextLossReason;

  static PcqStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    return aProducerView.WriteParam(static_cast<uint8_t>(aArg));
  }

  static PcqStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
    uint8_t val;
    auto status = aConsumerView.ReadParam(&val);
    if (!status) return status;
    if (!ReadContextLossReason(val, aArg)) {
      MOZ_ASSERT_UNREACHABLE("Invalid ContextLossReason");
      return PcqStatus::PcqFatalError;
    }
    return status;
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    return aView.template MinSizeParam<uint8_t>();
  }
};

template <typename V, typename E>
struct PcqParamTraits<Result<V, E>> {
  using T = Result<V, E>;

  static PcqStatus Write(ProducerView& aProducerView, const T& aArg) {
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

  static PcqStatus Read(ConsumerView& aConsumerView, T* const aArg) {
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
struct PcqParamTraits<std::string> {
  using T = std::string;

  static PcqStatus Write(ProducerView& aProducerView, const T& aArg) {
    auto status = aProducerView.WriteParam(aArg.size());
    if (!status) return status;
    status = aProducerView.Write(aArg.data(), aArg.size());
    return status;
  }

  static PcqStatus Read(ConsumerView& aConsumerView, T* const aArg) {
    size_t size;
    auto status = aConsumerView.ReadParam(&size);
    if (!status) return status;

    const UniqueBuffer temp = malloc(size);
    const auto dest = static_cast<char*>(temp.get());
    if (!dest) return PcqStatus::PcqFatalError;

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
struct PcqParamTraits<std::vector<U>> {
  using T = std::string;

  static PcqStatus Write(ProducerView& aProducerView, const T& aArg) {
    auto status = aProducerView.WriteParam(aArg.size());
    if (!status) return status;
    status = aProducerView.Write(aArg.data(), aArg.size());
    return status;
  }

  static PcqStatus Read(ConsumerView& aConsumerView, T* const aArg) {
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
struct PcqParamTraits<WebGLExtensionID> {
  using T = WebGLExtensionID;

  static PcqStatus Write(ProducerView& aProducerView, const T& aArg) {
    return aProducerView.WriteParam(mozilla::EnumValue(aArg));
  }

  static PcqStatus Read(ConsumerView& aConsumerView, T* const aArg) {
    PcqStatus status = PcqStatus::Success;
    if (aArg) {
      status = aConsumerView.ReadParam(
          reinterpret_cast<typename std::underlying_type<T>::type*>(aArg));
      if (*aArg >= WebGLExtensionID::Max) {
        MOZ_ASSERT(false);
        return PcqStatus::PcqFatalError;
      }
    }
    return status;
  }

  template <typename View>
  static size_t MinSize(View& aView, const T* const aArg) {
    return sizeof(T);
  }
};

}  // namespace webgl
}  // namespace mozilla

#endif  // WEBGLPCQPARAMTRAITS_H_
