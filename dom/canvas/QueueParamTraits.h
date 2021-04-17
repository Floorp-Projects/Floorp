/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _QUEUEPARAMTRAITS_H_
#define _QUEUEPARAMTRAITS_H_ 1

#include "ipc/EnumSerializer.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Assertions.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/SharedMemoryBasic.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/Logging.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"
#include "nsExceptionHandler.h"
#include "nsString.h"
#include "WebGLTypes.h"

namespace mozilla {
namespace webgl {

enum class QueueStatus {
  // Operation was successful
  kSuccess,
  // The operation failed because the queue isn't ready for it.
  // Either the queue is too full for an insert or too empty for a remove.
  // The operation may succeed if retried.
  kNotReady,
  // The operation required more room than the queue supports.
  // It should not be retried -- it will always fail.
  kTooSmall,
  // The operation failed for some reason that is unrecoverable.
  // All values below this value indicate a fata error.
  kFatalError,
  // Fatal error: Internal processing ran out of memory.  This is likely e.g.
  // during de-serialization.
  kOOMError,
};

inline bool IsSuccess(QueueStatus status) {
  return status == QueueStatus::kSuccess;
}

inline bool operator!(const QueueStatus status) { return !IsSuccess(status); }

template <typename T>
struct RemoveCVR {
  typedef typename std::remove_reference<typename std::remove_cv<T>::type>::type
      Type;
};

inline size_t UsedBytes(size_t aQueueBufferSize, size_t aRead, size_t aWrite) {
  return (aRead <= aWrite) ? aWrite - aRead
                           : (aQueueBufferSize - aRead) + aWrite;
}

inline size_t FreeBytes(size_t aQueueBufferSize, size_t aRead, size_t aWrite) {
  // Remember, queueSize is queueBufferSize-1
  return (aQueueBufferSize - 1) - UsedBytes(aQueueBufferSize, aRead, aWrite);
}

template <typename T>
struct IsTriviallySerializable
    : public std::integral_constant<bool, std::is_arithmetic<T>::value &&
                                              !std::is_same<T, bool>::value> {};

/**
 * QueueParamTraits provide the user with a way to implement PCQ argument
 * (de)serialization.  It uses a PcqView, which permits the system to
 * abandon all changes to the underlying PCQ if any operation fails.
 *
 * The transactional nature of PCQ operations make the ideal behavior a bit
 * complex.  Since the PCQ has a fixed amount of memory available to it,
 * TryInsert operations operations are expected to sometimes fail and be
 * re-issued later.  We want these failures to be inexpensive.  The same
 * goes for TryRemove, which fails when there isn't enough data in
 * the queue yet for them to complete.
 *
 * Their expected interface is:
 *
 * template<> struct QueueParamTraits<typename RemoveCVR<Arg>::Type> {
 *   // Write data from aArg into the PCQ.
 *   static QueueStatus Write(ProducerView& aProducerView, const Arg& aArg)
 *   {...};
 *
 *   // Read data from the PCQ into aArg, or just skip the data if aArg is null.
 *   static QueueStatus Read(ConsumerView& aConsumerView, Arg* aArg) {...}
 * };
 */
template <typename Arg>
struct QueueParamTraits;  // Todo: s/QueueParamTraits/SizedParamTraits/

/**
 * The marshaller handles all data insertion into the queue.
 */
class Marshaller {
 public:
  static QueueStatus WriteObject(uint8_t* aQueue, size_t aQueueBufferSize,
                                 size_t aRead, size_t* aWrite, const void* aArg,
                                 size_t aArgLength) {
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(aArg);
    if (FreeBytes(aQueueBufferSize, aRead, *aWrite) < aArgLength) {
      return QueueStatus::kNotReady;
    }

    if (*aWrite + aArgLength <= aQueueBufferSize) {
      memcpy(aQueue + *aWrite, buf, aArgLength);
    } else {
      size_t firstLen = aQueueBufferSize - *aWrite;
      memcpy(aQueue + *aWrite, buf, firstLen);
      memcpy(aQueue, &buf[firstLen], aArgLength - firstLen);
    }
    *aWrite = (*aWrite + aArgLength) % aQueueBufferSize;
    return QueueStatus::kSuccess;
  }

  static QueueStatus ReadObject(const uint8_t* aQueue, size_t aQueueBufferSize,
                                size_t* aRead, size_t aWrite, void* aArg,
                                size_t aArgLength) {
    if (UsedBytes(aQueueBufferSize, *aRead, aWrite) < aArgLength) {
      return QueueStatus::kNotReady;
    }

    if (aArg) {
      uint8_t* buf = reinterpret_cast<uint8_t*>(aArg);
      if (*aRead + aArgLength <= aQueueBufferSize) {
        memcpy(buf, aQueue + *aRead, aArgLength);
      } else {
        size_t firstLen = aQueueBufferSize - *aRead;
        memcpy(buf, aQueue + *aRead, firstLen);
        memcpy(&buf[firstLen], aQueue, aArgLength - firstLen);
      }
    }

    *aRead = (*aRead + aArgLength) % aQueueBufferSize;
    return QueueStatus::kSuccess;
  }
};

template <typename T>
inline Range<T> AsRange(T* const begin, T* const end) {
  const auto size = MaybeAs<size_t>(end - begin);
  MOZ_RELEASE_ASSERT(size);
  return {begin, *size};
}

/**
 * Used to give QueueParamTraits a way to write to the Producer without
 * actually altering it, in case the transaction fails.
 * THis object maintains the error state of the transaction and
 * discards commands issued after an error is encountered.
 */
template <typename _Producer>
class ProducerView {
 public:
  using Producer = _Producer;

  ProducerView(Producer* aProducer, size_t aRead, size_t* aWrite)
      : mProducer(aProducer),
        mRead(aRead),
        mWrite(aWrite),
        mStatus(QueueStatus::kSuccess) {}

  template <typename T>
  QueueStatus WriteFromRange(const Range<const T>& src) {
    if (!mStatus) return mStatus;
    mProducer->WriteFromRange(src);
    return mStatus;
  }
  /**
   * Copy bytes from aBuffer to the producer if there is enough room.
   * aBufferSize must not be 0.
   */
  template <typename T>
  inline QueueStatus Write(const T* begin, const T* end) {
    MOZ_RELEASE_ASSERT(begin <= end);
    if (!mStatus) return mStatus;
    WriteFromRange(AsRange(begin, end));
    return mStatus;
  }

  template <typename T>
  inline QueueStatus WritePod(const T& in) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto begin = reinterpret_cast<const uint8_t*>(&in);
    return Write(begin, begin + sizeof(T));
  }

  /**
   * Serialize aArg using Arg's QueueParamTraits.
   */
  template <typename Arg>
  QueueStatus WriteParam(const Arg& aArg) {
    return mozilla::webgl::QueueParamTraits<
        typename RemoveCVR<Arg>::Type>::Write(*this, aArg);
  }

  QueueStatus GetStatus() { return mStatus; }

 private:
  Producer* mProducer;
  size_t mRead;
  size_t* mWrite;
  QueueStatus mStatus;
};

/**
 * Used to give QueueParamTraits a way to read from the Consumer without
 * actually altering it, in case the transaction fails.
 */
template <typename _Consumer>
class ConsumerView {
 public:
  using Consumer = _Consumer;

  ConsumerView(Consumer* aConsumer, size_t* aRead, size_t aWrite)
      : mConsumer(aConsumer),
        mRead(aRead),
        mWrite(aWrite),
        mStatus(QueueStatus::kSuccess) {}

  /**
   * Read bytes from the consumer if there is enough data.  aBuffer may
   * be null (in which case the data is skipped)
   */
  template <typename T>
  inline QueueStatus Read(T* const destBegin, T* const destEnd) {
    MOZ_ASSERT(destBegin);
    MOZ_RELEASE_ASSERT(destBegin <= destEnd);
    if (!mStatus) return mStatus;

    const auto dest = AsRange(destBegin, destEnd);
    const auto view = ReadRange<T>(dest.length());
    if (!view) return mStatus;
    const auto byteSize = ByteSize(dest);
    if (byteSize) {
      memcpy(dest.begin().get(), view->begin().get(), byteSize);
    }
    return mStatus;
  }

  /// Return a view wrapping the shmem.
  template <typename T>
  inline Maybe<Range<const T>> ReadRange(const size_t elemCount) {
    if (!mStatus) return {};
    const auto view = mConsumer->template ReadRange<T>(elemCount);
    if (!view) {
      mStatus = QueueStatus::kTooSmall;
    }
    return view;
  }

  template <typename T>
  inline QueueStatus ReadPod(T* out) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto begin = reinterpret_cast<uint8_t*>(out);
    return Read(begin, begin + sizeof(T));
  }

  /**
   * Deserialize aArg using Arg's QueueParamTraits.
   * If the return value is not Success then aArg is not changed.
   */
  template <typename Arg>
  QueueStatus ReadParam(Arg* aArg) {
    MOZ_ASSERT(aArg);
    return mozilla::webgl::QueueParamTraits<std::remove_cv_t<Arg>>::Read(*this,
                                                                         aArg);
  }

  QueueStatus GetStatus() { return mStatus; }

 private:
  Consumer* mConsumer;
  size_t* mRead;
  size_t mWrite;
  QueueStatus mStatus;
};

// ---------------------------------------------------------------

/**
 * True for types that can be (de)serialized by memcpy.
 */
template <typename Arg>
struct QueueParamTraits {
  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView, const Arg& aArg) {
    static_assert(mozilla::webgl::template IsTriviallySerializable<Arg>::value,
                  "No QueueParamTraits specialization was found for this type "
                  "and it does not satisfy IsTriviallySerializable.");
    // Write self as binary
    const auto begin = &aArg;
    return aProducerView.Write(begin, begin + 1);
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, Arg* aArg) {
    static_assert(mozilla::webgl::template IsTriviallySerializable<Arg>::value,
                  "No QueueParamTraits specialization was found for this type "
                  "and it does not satisfy IsTriviallySerializable.");
    // Read self as binary
    return aConsumerView.Read(aArg, aArg + 1);
  }
};

// ---------------------------------------------------------------

template <>
struct QueueParamTraits<bool> {
  using ParamType = bool;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    uint8_t temp = aArg ? 1 : 0;
    return aProducerView.WriteParam(temp);
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    uint8_t temp;
    if (IsSuccess(aConsumerView.ReadParam(&temp))) {
      MOZ_ASSERT(temp == 1 || temp == 0);
      *aArg = temp ? true : false;
    }
    return aConsumerView.GetStatus();
  }
};

// ---------------------------------------------------------------

// Adapted from IPC::EnumSerializer, this class safely handles enum values,
// validating that they are in range using the same EnumValidators as IPDL
// (namely ContiguousEnumValidator and ContiguousEnumValidatorInclusive).
template <typename E, typename EnumValidator>
struct EnumSerializer {
  typedef E ParamType;
  typedef typename std::underlying_type<E>::type DataType;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aValue) {
    MOZ_RELEASE_ASSERT(
        EnumValidator::IsLegalValue(static_cast<DataType>(aValue)));
    return aProducerView.WriteParam(DataType(aValue));
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aResult) {
    DataType value;
    if (!aConsumerView.ReadParam(&value)) {
      CrashReporter::AnnotateCrashReport(
          CrashReporter::Annotation::IPCReadErrorReason, "Bad iter"_ns);
      return aConsumerView.GetStatus();
    }
    if (!EnumValidator::IsLegalValue(static_cast<DataType>(value))) {
      CrashReporter::AnnotateCrashReport(
          CrashReporter::Annotation::IPCReadErrorReason, "Illegal value"_ns);
      return QueueStatus::kFatalError;
    }

    *aResult = ParamType(value);
    return QueueStatus::kSuccess;
  }
};

using IPC::ContiguousEnumValidator;
using IPC::ContiguousEnumValidatorInclusive;

template <typename E, E MinLegal, E HighBound>
struct ContiguousEnumSerializer
    : EnumSerializer<E, ContiguousEnumValidator<E, MinLegal, HighBound>> {};

template <typename E, E MinLegal, E MaxLegal>
struct ContiguousEnumSerializerInclusive
    : EnumSerializer<E,
                     ContiguousEnumValidatorInclusive<E, MinLegal, MaxLegal>> {
};

// ---------------------------------------------------------------

template <>
struct QueueParamTraits<QueueStatus>
    : public ContiguousEnumSerializerInclusive<
          QueueStatus, QueueStatus::kSuccess, QueueStatus::kOOMError> {};

// ---------------------------------------------------------------

template <>
struct QueueParamTraits<webgl::TexUnpackBlobDesc> {
  using ParamType = webgl::TexUnpackBlobDesc;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& view, const ParamType& in) {
    MOZ_RELEASE_ASSERT(!in.image);
    const bool isDataSurf = bool(in.dataSurf);
    if (!view.WriteParam(in.imageTarget) || !view.WriteParam(in.size) ||
        !view.WriteParam(in.srcAlphaType) || !view.WriteParam(in.unpacking) ||
        !view.WriteParam(in.cpuData) || !view.WriteParam(in.pboOffset) ||
        !view.WriteParam(in.imageSize) || !view.WriteParam(in.sd) ||
        !view.WriteParam(isDataSurf)) {
      return view.GetStatus();
    }
    if (isDataSurf) {
      const auto& surf = in.dataSurf;
      gfx::DataSourceSurface::ScopedMap map(surf, gfx::DataSourceSurface::READ);
      if (!map.IsMapped()) {
        return QueueStatus::kOOMError;
      }
      const auto& surfSize = surf->GetSize();
      const auto stride = *MaybeAs<size_t>(map.GetStride());
      if (!view.WriteParam(surfSize) || !view.WriteParam(surf->GetFormat()) ||
          !view.WriteParam(stride)) {
        return view.GetStatus();
      }

      const size_t dataSize = stride * surfSize.height;
      const auto& begin = map.GetData();
      const auto range = Range<const uint8_t>{begin, dataSize};
      if (!view.WriteFromRange(range)) {
        return view.GetStatus();
      }
    }
    return QueueStatus::kSuccess;
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& view, ParamType* const out) {
    bool isDataSurf;
    if (!view.ReadParam(&out->imageTarget) || !view.ReadParam(&out->size) ||
        !view.ReadParam(&out->srcAlphaType) ||
        !view.ReadParam(&out->unpacking) || !view.ReadParam(&out->cpuData) ||
        !view.ReadParam(&out->pboOffset) || !view.ReadParam(&out->imageSize) ||
        !view.ReadParam(&out->sd) || !view.ReadParam(&isDataSurf)) {
      return view.GetStatus();
    }
    if (isDataSurf) {
      gfx::IntSize surfSize;
      gfx::SurfaceFormat format;
      size_t stride;
      if (!view.ReadParam(&surfSize) || !view.ReadParam(&format) ||
          !view.ReadParam(&stride)) {
        return view.GetStatus();
      }
      const size_t dataSize = stride * surfSize.height;
      const auto range = view.template ReadRange<uint8_t>(dataSize);
      if (!range) return view.GetStatus();

      // DataSourceSurface demands pointer-to-mutable.
      const auto bytes = const_cast<uint8_t*>(range->begin().get());
      out->dataSurf = gfx::Factory::CreateWrappingDataSourceSurface(
          bytes, stride, surfSize, format);
      MOZ_ASSERT(out->dataSurf);
    }
    return QueueStatus::kSuccess;
  }
};

// ---------------------------------------------------------------

template <>
struct QueueParamTraits<nsACString> {
  using ParamType = nsACString;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    if ((!aProducerView.WriteParam(aArg.IsVoid())) || aArg.IsVoid()) {
      return aProducerView.GetStatus();
    }

    uint32_t len = aArg.Length();
    if ((!aProducerView.WriteParam(len)) || (len == 0)) {
      return aProducerView.GetStatus();
    }

    return aProducerView.Write(aArg.BeginReading(), len);
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    bool isVoid = false;
    if (!IsSuccess(aConsumerView.ReadParam(&isVoid))) {
      return aConsumerView.GetStatus();
    }
    aArg->SetIsVoid(isVoid);
    if (isVoid) {
      return QueueStatus::kSuccess;
    }

    uint32_t len = 0;
    if (!IsSuccess(aConsumerView.ReadParam(&len))) {
      return aConsumerView.GetStatus();
    }

    if (len == 0) {
      *aArg = "";
      return QueueStatus::kSuccess;
    }

    char* buf = new char[len + 1];
    if (!buf) {
      return QueueStatus::kOOMError;
    }
    if (!IsSuccess(aConsumerView.Read(buf, len))) {
      return aConsumerView.GetStatus();
    }
    buf[len] = '\0';
    aArg->Adopt(buf, len);
    return QueueStatus::kSuccess;
  }
};

template <>
struct QueueParamTraits<nsAString> {
  using ParamType = nsAString;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    if ((!aProducerView.WriteParam(aArg.IsVoid())) || (aArg.IsVoid())) {
      return aProducerView.GetStatus();
    }
    // DLP: No idea if this includes null terminator
    uint32_t len = aArg.Length();
    if ((!aProducerView.WriteParam(len)) || (len == 0)) {
      return aProducerView.GetStatus();
    }
    constexpr const uint32_t sizeofchar = sizeof(typename ParamType::char_type);
    return aProducerView.Write(aArg.BeginReading(), len * sizeofchar);
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    bool isVoid = false;
    if (!aConsumerView.ReadParam(&isVoid)) {
      return aConsumerView.GetStatus();
    }
    aArg->SetIsVoid(isVoid);
    if (isVoid) {
      return QueueStatus::kSuccess;
    }

    // DLP: No idea if this includes null terminator
    uint32_t len = 0;
    if (!aConsumerView.ReadParam(&len)) {
      return aConsumerView.GetStatus();
    }

    if (len == 0) {
      *aArg = nsString();
      return QueueStatus::kSuccess;
    }

    uint32_t sizeofchar = sizeof(typename ParamType::char_type);
    typename ParamType::char_type* buf = nullptr;
    buf = static_cast<typename ParamType::char_type*>(
        malloc((len + 1) * sizeofchar));
    if (!buf) {
      return QueueStatus::kOOMError;
    }

    if (!aConsumerView.Read(buf, len * sizeofchar)) {
      return aConsumerView.GetStatus();
    }

    buf[len] = L'\0';
    aArg->Adopt(buf, len);
    return QueueStatus::kSuccess;
  }
};

template <>
struct QueueParamTraits<nsCString> : public QueueParamTraits<nsACString> {
  using ParamType = nsCString;
};

template <>
struct QueueParamTraits<nsString> : public QueueParamTraits<nsAString> {
  using ParamType = nsString;
};

// ---------------------------------------------------------------

template <typename NSTArrayType,
          bool =
              IsTriviallySerializable<typename NSTArrayType::elem_type>::value>
struct NSArrayQueueParamTraits;

// For ElementTypes that are !IsTriviallySerializable
template <typename _ElementType>
struct NSArrayQueueParamTraits<nsTArray<_ElementType>, false> {
  using ElementType = _ElementType;
  using ParamType = nsTArray<ElementType>;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    aProducerView.WriteParam(aArg.Length());
    for (auto& elt : aArg) {
      aProducerView.WriteParam(elt);
    }
    return aProducerView.GetStatus();
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    size_t arrayLen;
    if (!aConsumerView.ReadParam(&arrayLen)) {
      return aConsumerView.GetStatus();
    }

    if (!aArg->AppendElements(arrayLen, fallible)) {
      return QueueStatus::kOOMError;
    }

    for (auto i : IntegerRange(arrayLen)) {
      ElementType& elt = aArg->ElementAt(i);
      aConsumerView.ReadParam(elt);
    }
    return aConsumerView.GetStatus();
  }
};

// For ElementTypes that are IsTriviallySerializable
template <typename _ElementType>
struct NSArrayQueueParamTraits<nsTArray<_ElementType>, true> {
  using ElementType = _ElementType;
  using ParamType = nsTArray<ElementType>;

  // TODO: Are there alignment issues?
  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    size_t arrayLen = aArg.Length();
    aProducerView.WriteParam(arrayLen);
    return aProducerView.Write(&aArg[0], aArg.Length() * sizeof(ElementType));
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    size_t arrayLen;
    if (!aConsumerView.ReadParam(&arrayLen)) {
      return aConsumerView.GetStatus();
    }

    if (!aArg->AppendElements(arrayLen, fallible)) {
      return QueueStatus::kOOMError;
    }

    return aConsumerView.Read(aArg->Elements(), arrayLen * sizeof(ElementType));
  }
};

template <typename ElementType>
struct QueueParamTraits<nsTArray<ElementType>>
    : public NSArrayQueueParamTraits<nsTArray<ElementType>> {
  using ParamType = nsTArray<ElementType>;
};

// ---------------------------------------------------------------

template <typename ArrayType,
          bool =
              IsTriviallySerializable<typename ArrayType::ElementType>::value>
struct ArrayQueueParamTraits;

// For ElementTypes that are !IsTriviallySerializable
template <typename _ElementType, size_t Length>
struct ArrayQueueParamTraits<Array<_ElementType, Length>, false> {
  using ElementType = _ElementType;
  using ParamType = Array<ElementType, Length>;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    for (const auto& elt : aArg) {
      aProducerView.WriteParam(elt);
    }
    return aProducerView.GetStatus();
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    for (auto& elt : *aArg) {
      aConsumerView.ReadParam(elt);
    }
    return aConsumerView.GetStatus();
  }
};

// For ElementTypes that are IsTriviallySerializable
template <typename _ElementType, size_t Length>
struct ArrayQueueParamTraits<Array<_ElementType, Length>, true> {
  using ElementType = _ElementType;
  using ParamType = Array<ElementType, Length>;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    return aProducerView.Write(aArg.begin(), sizeof(ElementType[Length]));
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    return aConsumerView.Read(aArg->begin(), sizeof(ElementType[Length]));
  }
};

template <typename ElementType, size_t Length>
struct QueueParamTraits<Array<ElementType, Length>>
    : public ArrayQueueParamTraits<Array<ElementType, Length>> {
  using ParamType = Array<ElementType, Length>;
};

// ---------------------------------------------------------------

template <typename ElementType>
struct QueueParamTraits<Maybe<ElementType>> {
  using ParamType = Maybe<ElementType>;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    aProducerView.WriteParam(static_cast<bool>(aArg));
    return aArg ? aProducerView.WriteParam(aArg.ref())
                : aProducerView.GetStatus();
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    bool isSome;
    if (!aConsumerView.ReadParam(&isSome)) {
      return aConsumerView.GetStatus();
    }

    if (!isSome) {
      aArg->reset();
      return QueueStatus::kSuccess;
    }

    aArg->emplace();
    return aConsumerView.ReadParam(aArg->ptr());
  }
};

// ---------------------------------------------------------------

// Maybe<Variant> needs special behavior since Variant is not default
// constructable.  The Variant's first type must be default constructible.
template <typename T, typename... Ts>
struct QueueParamTraits<Maybe<Variant<T, Ts...>>> {
  using ParamType = Maybe<Variant<T, Ts...>>;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    aProducerView.WriteParam(aArg.mIsSome);
    return (aArg.mIsSome) ? aProducerView.WriteParam(aArg.ref())
                          : aProducerView.GetStatus();
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    bool isSome;
    if (!aConsumerView.ReadParam(&isSome)) {
      return aConsumerView.GetStatus();
    }

    if (!isSome) {
      aArg->reset();
      return QueueStatus::kSuccess;
    }

    aArg->emplace(VariantType<T>());
    return aConsumerView.ReadParam(aArg->ptr());
  }
};

// ---------------------------------------------------------------

template <typename TypeA, typename TypeB>
struct QueueParamTraits<std::pair<TypeA, TypeB>> {
  using ParamType = std::pair<TypeA, TypeB>;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    aProducerView.WriteParam(aArg.first());
    return aProducerView.WriteParam(aArg.second());
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    aConsumerView.ReadParam(aArg->first());
    return aConsumerView.ReadParam(aArg->second());
  }
};

// ---------------------------------------------------------------

template <typename T>
struct QueueParamTraits<UniquePtr<T>> {
  using ParamType = UniquePtr<T>;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    // TODO: Clean up move with PCQ
    aProducerView.WriteParam(!static_cast<bool>(aArg));
    if (aArg && aProducerView.WriteParam(*aArg.get())) {
      const_cast<ParamType&>(aArg).reset();
    }
    return aProducerView.GetStatus();
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    bool isNull;
    if (!aConsumerView.ReadParam(&isNull)) {
      return aConsumerView.GetStatus();
    }
    if (isNull) {
      aArg->reset(nullptr);
      return QueueStatus::kSuccess;
    }

    T* obj = new T();
    aArg->reset(obj);
    return aConsumerView.ReadParam(obj);
  }
};

// ---------------------------------------------------------------

template <>
struct QueueParamTraits<mozilla::ipc::Shmem> {
  using ParamType = mozilla::ipc::Shmem;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView, ParamType&& aParam) {
    if (!aProducerView.WriteParam(
            aParam.Id(mozilla::ipc::Shmem::PrivateIPDLCaller()))) {
      return aProducerView.GetStatus();
    }

    aParam.RevokeRights(mozilla::ipc::Shmem::PrivateIPDLCaller());
    aParam.forget(mozilla::ipc::Shmem::PrivateIPDLCaller());
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aResult) {
    ParamType::id_t id;
    if (!aConsumerView.ReadParam(&id)) {
      return aConsumerView.GetStatus();
    }

    mozilla::ipc::Shmem::SharedMemory* rawmem =
        aConsumerView.LookupSharedMemory(id);
    if (!rawmem) {
      return QueueStatus::kFatalError;
    }

    *aResult = mozilla::ipc::Shmem(mozilla::ipc::Shmem::PrivateIPDLCaller(),
                                   rawmem, id);
    return QueueStatus::kSuccess;
  }
};

}  // namespace webgl
}  // namespace mozilla

#endif  // _QUEUEPARAMTRAITS_H_
