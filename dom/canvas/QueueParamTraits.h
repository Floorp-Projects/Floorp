/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _QUEUEPARAMTRAITS_H_
#define _QUEUEPARAMTRAITS_H_ 1

#include "mozilla/ipc/SharedMemoryBasic.h"
#include "mozilla/Assertions.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"
#include "nsString.h"

namespace mozilla {
namespace webgl {

struct QueueStatus {
  enum EStatus {
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
  } mValue;

  MOZ_IMPLICIT QueueStatus(const EStatus status = kSuccess) : mValue(status) {}
  explicit operator bool() const { return mValue == kSuccess; }
  explicit operator int() const { return static_cast<int>(mValue); }
  bool operator==(const EStatus& o) const { return mValue == o; }
  bool operator!=(const EStatus& o) const { return !(*this == o); }
};

inline bool IsSuccess(QueueStatus status) {
  return status == QueueStatus::kSuccess;
}

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
    : public std::integral_constant<bool, std::is_enum<T>::value ||
                                              std::is_arithmetic<T>::value> {};

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
 * QueueParamTraits resolve this problem by allowing the Try... operations to
 * use QueueParamTraits<typename RemoveCVR<Arg>::Type>::MinSize() to get a
 * lower-bound on the amount of room in the queue required for Arg.  If the
 * operation needs more than is available then the operation quickly fails.
 * Otherwise, (de)serialization will commence, although it may still fail if
 * MinSize() was too low.
 *
 * Their expected interface is:
 *
 * template<> struct QueueParamTraits<typename RemoveCVR<Arg>::Type> {
 *   // Write data from aArg into the PCQ.  It is an error to write less than
 *   // is reported by MinSize(aArg).
 *   static QueueStatus Write(ProducerView& aProducerView, const Arg& aArg)
 *   {...};
 *
 *   // Read data from the PCQ into aArg, or just skip the data if aArg is null.
 *   // It is an error to read less than is reported by MinSize(aArg).
 *   static QueueStatus Read(ConsumerView& aConsumerView, Arg* aArg) {...}
 *
 *   // The minimum number of bytes needed to represent this object in the
 *   // queue.  It is intended to be a very fast estimate but most cases can
 *   // easily compute the exact value.
 *   // It is an error for the queue to require less room than MinSize()
 *   // reports.  A MinSize of 0 is always valid (albeit wasteful).
 *   static size_t MinSize(const Arg& aArg) {...}
 * };
 */
template <typename Arg>
struct QueueParamTraits;

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

  /**
   * Copy bytes from aBuffer to the producer if there is enough room.
   * aBufferSize must not be 0.
   */
  inline QueueStatus Write(const void* aBuffer, size_t aBufferSize);

  /**
   * Copy bytes from src (an array of Ts), to the producer if there is
   * enough room.  count is the number of elements in the array.
   */
  template <typename T>
  inline QueueStatus Write(const T* src, size_t count) {
    return Write(reinterpret_cast<const void*>(src), count * sizeof(T));
  }

  /**
   * Serialize aArg using Arg's QueueParamTraits.
   */
  template <typename Arg>
  QueueStatus WriteParam(const Arg& aArg) {
    return mozilla::webgl::QueueParamTraits<
        typename RemoveCVR<Arg>::Type>::Write(*this, aArg);
  }

  /**
   * MinSize of Arg using QueueParamTraits.
   */
  template <typename Arg>
  size_t MinSizeParam(const Arg& aArg = nullptr) {
    return mozilla::webgl::QueueParamTraits<
        typename RemoveCVR<Arg>::Type>::MinSize(*this, aArg);
  }

  inline size_t MinSizeBytes(size_t aNBytes);

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
  inline QueueStatus Read(void* aBuffer, size_t aBufferSize);

  /**
   * Read bytes from the consumer into an array of Ts, if there is enough data.
   * aBuffer may be null (in which case the data is skipped).  count is the
   * number of array elements to read.
   */
  template <typename T>
  inline QueueStatus Read(T* dest, size_t count) {
    return Read(reinterpret_cast<void*>(dest), count * sizeof(T));
  }

  /**
   * Deserialize aArg using Arg's QueueParamTraits.
   * If the return value is not Success then aArg is not changed.
   */
  template <typename Arg>
  QueueStatus ReadParam(Arg* aArg) {
    MOZ_ASSERT(aArg);
    return mozilla::webgl::QueueParamTraits<std::remove_cv_t<Arg>*>::Read(*this,
                                                                          aArg);
  }

  /**
   * MinSize of Arg using QueueParamTraits.  aArg may be null.
   */
  template <typename Arg>
  size_t MinSizeParam(Arg& aArg) {
    MOZ_ASSERT(aArg);
    return mozilla::webgl::QueueParamTraits<std::remove_cv_t<Arg>&>::MinSize(
        *this, aArg);
  }

  inline size_t MinSizeBytes(size_t aNBytes);

  QueueStatus GetStatus() { return mStatus; }

 private:
  Consumer* mConsumer;
  size_t* mRead;
  size_t mWrite;
  QueueStatus mStatus;
};

template <typename T>
QueueStatus ProducerView<T>::Write(const void* aBuffer, size_t aBufferSize) {
  MOZ_ASSERT(aBuffer && (aBufferSize > 0));
  if (!mStatus) {
    return mStatus;
  }

  if (mProducer->NeedsSharedMemory(aBufferSize)) {
    mozilla::ipc::Shmem shmem;
    QueueStatus status = mProducer->AllocShmem(&shmem, aBufferSize, aBuffer);
    if (!IsSuccess(status)) {
      return status;
    }
    return WriteParam(std::move(shmem));
  }

  return mProducer->WriteObject(mRead, mWrite, aBuffer, aBufferSize);
}

template <typename T>
size_t ProducerView<T>::MinSizeBytes(size_t aNBytes) {
  mozilla::ipc::Shmem shmem;
  return mProducer->NeedsSharedMemory(aNBytes) ? MinSizeParam(shmem) : aNBytes;
}

template <typename T>
QueueStatus ConsumerView<T>::Read(void* aBuffer, size_t aBufferSize) {
  MOZ_ASSERT(aBufferSize > 0);
  if (!mStatus) {
    return mStatus;
  }

  if (mConsumer->NeedsSharedMemory(aBufferSize)) {
    mozilla::ipc::Shmem shmem;
    QueueStatus status = ReadParam(&shmem);
    if (!IsSuccess(status)) {
      return status;
    }

    if ((shmem.Size<uint8_t>() != aBufferSize) || (!shmem.get<uint8_t>())) {
      return QueueStatus::kFatalError;
    }

    if (aBuffer) {
      memcpy(aBuffer, shmem.get<uint8_t>(), aBufferSize);
    }
    return QueueStatus::kSuccess;
  }
  return mConsumer->ReadObject(mRead, mWrite, aBuffer, aBufferSize);
}

template <typename T>
size_t ConsumerView<T>::MinSizeBytes(size_t aNBytes) {
  mozilla::ipc::Shmem shmem;
  return mConsumer->NeedsSharedMemory(aNBytes) ? MinSizeParam(shmem) : aNBytes;
}

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
    return aProducerView.Write(&aArg, sizeof(Arg));
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, Arg* aArg) {
    static_assert(mozilla::webgl::template IsTriviallySerializable<Arg>::value,
                  "No QueueParamTraits specialization was found for this type "
                  "and it does not satisfy IsTriviallySerializable.");
    // Read self as binary
    return aConsumerView.Read(aArg, sizeof(Arg));
  }

  template <typename View>
  static constexpr size_t MinSize(View& aView, const Arg& aArg) {
    static_assert(mozilla::webgl::template IsTriviallySerializable<Arg>::value,
                  "No QueueParamTraits specialization was found for this type "
                  "and it does not satisfy IsTriviallySerializable.");
    return sizeof(Arg);
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
  static void Write(ProducerView<U>& aProducerView, const ParamType& aValue) {
    MOZ_RELEASE_ASSERT(EnumValidator::IsLegalValue(aValue));
    aProducerView.WriteParam(DataType(aValue));
  }

  template <typename U>
  static bool Read(ConsumerView<U>& aConsumerView, ParamType* aResult) {
    DataType value;
    if (!aConsumerView.ReadParam(&value)) {
      CrashReporter::AnnotateCrashReport(
          CrashReporter::Annotation::IPCReadErrorReason, "Bad iter"_ns);
      return aConsumerView.GetStatus();
    }
    if (!EnumValidator::IsLegalValue(ParamType(value))) {
      CrashReporter::AnnotateCrashReport(
          CrashReporter::Annotation::IPCReadErrorReason, "Illegal value"_ns);
      return aConsumerView.GetStatus();
    }

    *aResult = ParamType(value);
    return QueueStatus::kSuccess;
  }

  template <typename View>
  static constexpr size_t MinSize(View& aView, const ParamType& aArg) {
    return aView.MinSizeParam(DataType(aArg));
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
struct QueueParamTraits<QueueStatus::EStatus>
    : public ContiguousEnumSerializerInclusive<
          QueueStatus::EStatus, QueueStatus::EStatus::kSuccess,
          QueueStatus::EStatus::kOOMError> {};

template <>
struct QueueParamTraits<QueueStatus> {
  using ParamType = QueueStatus;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    return aProducerView.WriteParam(aArg.mValue);
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    return aConsumerView.ReadParam(&aArg->mValue);
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    return aView.MinSize(aArg.mValue);
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
    if (!aConsumerView.ReadParam(&isVoid)) {
      return aConsumerView.GetStatus();
    }
    aArg->SetIsVoid(isVoid);
    if (isVoid) {
      return QueueStatus::kSuccess;
    }

    uint32_t len = 0;
    if (!aConsumerView.ReadParam(&len)) {
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
    if (!aConsumerView.Read(buf, len)) {
      return aConsumerView.GetStatus();
    }
    buf[len] = '\0';
    aArg->Adopt(buf, len);
    return QueueStatus::kSuccess;
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    size_t minSize = aView.template MinSizeParam<bool>(aArg.IsVoid());
    if (aArg.IsVoid()) {
      return minSize;
    }
    minSize += aView.template MinSizeParam<uint32_t>(aArg.Length()) +
               aView.MinSizeBytes(aArg.Length());
    return minSize;
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

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    size_t minSize = aView.template MinSizeParam<bool>(aArg.IsVoid());
    if (aArg.IsVoid()) {
      return minSize;
    }
    uint32_t sizeofchar = sizeof(typename ParamType::char_type);
    minSize += aView.template MinSizeParam<uint32_t>(aArg.Length()) +
               aView.MinSizeBytes(aArg.Length() * sizeofchar);
    return minSize;
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

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    size_t ret = aView.MinSizeParam(aArg.Length());
    for (auto& elt : aArg) {
      ret += aView.MinSizeParam(elt);
    }
    return ret;
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

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    size_t ret = aView.template MinSizeParam<size_t>(aArg.Length());
    ret += aView.MinSizeBytes(aArg.Length() * sizeof(ElementType));
    return ret;
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

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    size_t ret = 0;
    for (const auto& elt : aArg) {
      ret += aView.MinSizeParam(elt);
    }
    return ret;
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

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    return aView.MinSizeBytes(sizeof(ElementType[Length]));
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

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    return aView.template MinSizeParam<bool>(aArg) +
           (aArg.isSome() ? aView.MinSizeParam(aArg.ref()) : 0);
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

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    return aView.MinSizeParam(aArg.mIsSome) +
           (aArg.isSome() ? aView.MinSizeParam(aArg.ref()) : 0);
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

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    return aView.MinSizeParam(aArg.first()) + aView.MinSizeParam(aArg.second());
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

    T* obj = nullptr;
    obj = new T();
    if (!obj) {
      return QueueStatus::kOOMError;
    }
    aArg->reset(obj);
    return aConsumerView.ReadParam(obj);
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    size_t ret = aView.template MinSizeParam<bool>(aArg);
    return ret + aView.MinSizeParam(*aArg);
  }
};

// ---------------------------------------------------------------

// C++ does not allow this struct with a templated method to be local to
// another struct (QueueParamTraits<Variant<...>>) so we put it here.
template <typename U>
struct PcqVariantWriter {
  ProducerView<U>& mView;
  template <typename T>
  QueueStatus match(const T& x) {
    return mView.WriteParam(x);
  }
};

template <typename... Types>
struct QueueParamTraits<Variant<Types...>> {
  using ParamType = Variant<Types...>;
  using Tag = typename mozilla::detail::VariantTag<Types...>::Type;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    aProducerView.WriteParam(aArg.tag);
    return aArg.match(PcqVariantWriter{aProducerView});
  }

  // Check the N-1th tag.  See ParamTraits<mozilla::Variant> for details.
  template <size_t N, typename dummy = void>
  struct VariantReader {
    using Next = VariantReader<N - 1>;
    template <typename U>
    static QueueStatus Read(ConsumerView<U>& aView, Tag aTag, ParamType* aArg) {
      if (aTag == N - 1) {
        using EntryType = typename mozilla::detail::Nth<N - 1, Types...>::Type;
        return aView.ReadParam(*static_cast<EntryType*>(aArg->ptr()));
      }
      return Next::Read(aView, aTag, aArg);
    }
  };

  template <typename dummy>
  struct VariantReader<0, dummy> {
    template <typename U>
    static QueueStatus Read(ConsumerView<U>& aView, Tag aTag, ParamType* aArg) {
      MOZ_ASSERT_UNREACHABLE("Tag wasn't for an entry in this Variant");
      return QueueStatus::kFatalError;
    }
  };

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    Tag tag;
    if (!aConsumerView.ReadParam(&tag)) {
      return aConsumerView.GetStatus();
    }
    aArg->tag = tag;
    return VariantReader<sizeof...(Types)>::Read(aConsumerView, tag, aArg);
  }

  // Get the min size of the given variant or get the min size of all of the
  // variant's types.
  template <size_t N, typename View>
  struct MinSizeVariant {
    using Next = MinSizeVariant<N - 1, View>;
    static size_t MinSize(View& aView, const Tag* aTag, const ParamType& aArg) {
      using EntryType = typename mozilla::detail::Nth<N - 1, Types...>::Type;
      MOZ_ASSERT(aTag);
      if (*aTag == N - 1) {
        return aView.MinSizeParam(aArg.template as<EntryType>());
      }
      return Next::MinSize(aView, aTag, aArg);
    }
  };

  template <typename View>
  struct MinSizeVariant<0, View> {
    // We've reached the end of the type list.  We will legitimately get here
    // when calculating MinSize for a null Variant.
    static size_t MinSize(View& aView, const Tag* aTag, const ParamType& aArg) {
      MOZ_ASSERT_UNREACHABLE("Tag wasn't for an entry in this Variant");
      return 0;
    }
  };

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    return aView.MinSizeParam(aArg.tag) +
           MinSizeVariant<sizeof...(Types), View>::MinSize(aView, aArg.tag,
                                                           aArg);
  }
};

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

  template <typename View>
  static size_t MinSize(View& aView, const ParamType& aArg) {
    return aView.MinSizeParam(
        aArg.Id(mozilla::ipc::Shmem::PrivateIPDLCaller()));
  }
};

}  // namespace webgl
}  // namespace mozilla

#endif  // _QUEUEPARAMTRAITS_H_
