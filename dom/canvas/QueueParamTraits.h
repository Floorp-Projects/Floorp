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
#include "mozilla/ipc/Shmem.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"
#include "nsString.h"

namespace IPC {
typedef uint32_t PcqTypeInfoID;
template <typename T>
struct PcqTypeInfo;
}  // namespace IPC

namespace mozilla {
namespace webgl {

using IPC::PcqTypeInfo;
using IPC::PcqTypeInfoID;

struct QueueStatus {
  enum EStatus {
    // Operation was successful
    kSuccess,
    // The operation failed because the queue isn't ready for it.
    // Either the queue is too full for an insert or too empty for a remove.
    // The operation may succeed if retried.
    kNotReady,
    // The operation was typed and the type check failed.
    kTypeError,
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

class ProducerConsumerQueue;
class PcqProducer;
class PcqConsumer;

/**
 * QueueParamTraits provide the user with a way to implement PCQ argument
 * (de)serialization.  It uses a PcqView, which permits the system to
 * abandon all changes to the underlying PCQ if any operation fails.
 *
 * The transactional nature of PCQ operations make the ideal behavior a bit
 * complex.  Since the PCQ has a fixed amount of memory available to it,
 * TryInsert operations operations are expected to sometimes fail and be
 * re-issued later.  We want these failures to be inexpensive.  The same
 * goes for TryPeek/TryRemove, which fail when there isn't enough data in
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
 *  *   static QueueStatus Write(ProducerView& aProducerView, const Arg& aArg)
 * {...};
 *
 *   // Read data from the PCQ into aArg, or just skip the data if aArg is null.
 *   // It is an error to read less than is reported by MinSize(aArg).
 *  *   static QueueStatus Read(ConsumerView& aConsumerView, Arg* aArg) {...}
 *
 *   // The minimum number of bytes needed to represent this object in the
 * queue.
 *   // It is intended to be a very fast estimate but most cases can easily
 *   // compute the exact value.
 *   // If aArg is null then this should be the minimum ever required (it is
 * only
 *   // null when checking for deserialization, since the argument is obviously
 *   // not yet available).  It is an error for the queue to require less room
 *   // than MinSize() reports.  A MinSize of 0 is always valid (albeit
 * wasteful). static size_t MinSize(const Arg* aArg) {...}
 * };
 */
template <typename Arg>
struct QueueParamTraits;

// Provides type-checking for queue parameters.
template <typename Arg>
struct PcqTypedArg {
  explicit PcqTypedArg(const Arg& aArg) : mWrite(&aArg), mRead(nullptr) {}
  explicit PcqTypedArg(Arg* aArg) : mWrite(nullptr), mRead(aArg) {}

 private:
  friend struct QueueParamTraits<PcqTypedArg<Arg>>;
  const Arg* mWrite;
  Arg* mRead;
};

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

  // The PcqBase must belong to a Consumer.
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
   * Write bytes from aBuffer to the producer if there is enough room.
   * aBufferSize must not be 0.
   */
  inline QueueStatus Write(const void* aBuffer, size_t aBufferSize);

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
   * Serialize aArg using Arg's QueueParamTraits and PcqTypeInfo.
   */
  template <typename Arg>
  QueueStatus WriteTypedParam(const Arg& aArg) {
    return mozilla::webgl::QueueParamTraits<PcqTypedArg<Arg>>::Write(
        *this, PcqTypedArg<Arg>(aArg));
  }

  /**
   * MinSize of Arg using QueueParamTraits.
   */
  template <typename Arg>
  size_t MinSizeParam(const Arg* aArg = nullptr) {
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

  // When reading raw memory blocks, we may get an error, a shared memory
  // object that we take ownership of, or a pointer to a block of
  // memory that is only guaranteed to exist as long as the ReadVariant
  // call.
  using PcqReadBytesVariant =
      Variant<QueueStatus, RefPtr<mozilla::ipc::SharedMemoryBasic>, void*>;

  /**
   * Read bytes from the consumer if there is enough data.  aBuffer may
   * be null (in which case the data is skipped)
   */
  inline QueueStatus Read(void* aBuffer, size_t aBufferSize);

  template <typename T>
  inline QueueStatus Read(T* dest, size_t count) {
    return Read(reinterpret_cast<void*>(dest), count * sizeof(T));
  }

  /**
   * Calls a Matcher that returns a QueueStatus when told that the next bytes
   * are in the queue or are in shared memory.
   *
   * The matcher looks like this:
   * struct MyMatcher {
   *   QueueStatus operator()(RefPtr<mozilla::ipc::SharedMemoryBasic>& x) {
   *     Read or copy x; take responsibility for closing x.
   *   }
   *   QueueStatus operator()() { Data is in queue.  Use ConsumerView::Read. }
   * };
   *
   * The only reason to use this instead of Read is if it is important to
   * get the data without copying "large" items.  Few things are large
   * enough to bother.
   */
  template <typename Matcher>
  inline QueueStatus ReadVariant(size_t aBufferSize, Matcher&& aMatcher);

  /**
   * Deserialize aArg using Arg's QueueParamTraits.
   * If the return value is not Success then aArg is not changed.
   */
  template <typename Arg>
  QueueStatus ReadParam(Arg* aArg = nullptr) {
    return mozilla::webgl::QueueParamTraits<
        typename RemoveCVR<Arg>::Type>::Read(*this, aArg);
  }

  /**
   * Deserialize aArg using Arg's QueueParamTraits and PcqTypeInfo.
   * If the return value is not Success then aArg is not changed.
   */
  template <typename Arg>
  QueueStatus ReadTypedParam(Arg* aArg = nullptr) {
    return mozilla::webgl::QueueParamTraits<PcqTypedArg<Arg>>::Read(
        *this, PcqTypedArg(aArg));
  }

  /**
   * MinSize of Arg using QueueParamTraits.  aArg may be null.
   */
  template <typename Arg>
  size_t MinSizeParam(Arg* aArg = nullptr) {
    return mozilla::webgl::QueueParamTraits<
        typename RemoveCVR<Arg>::Type>::MinSize(*this, aArg);
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

  if (NeedsSharedMemory(aBufferSize, mProducer->Size())) {
    auto smem = MakeRefPtr<mozilla::ipc::SharedMemoryBasic>();
    if (!smem->Create(aBufferSize) || !smem->Map(aBufferSize)) {
      return QueueStatus::kFatalError;
    }
    mozilla::ipc::SharedMemoryBasic::Handle handle;
    if (!smem->ShareToProcess(mProducer->mOtherPid, &handle)) {
      return QueueStatus::kFatalError;
    }
    memcpy(smem->memory(), aBuffer, aBufferSize);
    smem->CloseHandle();
    return WriteParam(handle);
  }

  return mProducer->WriteObject(mRead, mWrite, aBuffer, aBufferSize);
}

template <typename T>
size_t ProducerView<T>::MinSizeBytes(size_t aNBytes) {
  return NeedsSharedMemory(aNBytes, mProducer->Size())
             ? MinSizeParam((mozilla::ipc::SharedMemoryBasic::Handle*)nullptr)
             : aNBytes;
}

template <typename T>
QueueStatus ConsumerView<T>::Read(void* aBuffer, size_t aBufferSize) {
  struct PcqReadBytesMatcher {
    QueueStatus operator()(RefPtr<mozilla::ipc::SharedMemoryBasic>& smem) {
      MOZ_ASSERT(smem);
      QueueStatus ret;
      if (smem->memory()) {
        if (mBuffer) {
          memcpy(mBuffer, smem->memory(), mBufferSize);
        }
        ret = QueueStatus::kSuccess;
      } else {
        ret = QueueStatus::kFatalError;
      }
      // TODO: Problem: CloseHandle should only be called on the remove/skip
      // call.  A peek should not CloseHandle!
      smem->CloseHandle();
      return ret;
    }
    QueueStatus operator()() {
      return mConsumer.ReadObject(mRead, mWrite, mBuffer, mBufferSize);
    }

    Consumer& mConsumer;
    size_t* mRead;
    size_t mWrite;
    void* mBuffer;
    size_t mBufferSize;
  };

  MOZ_ASSERT(aBufferSize > 0);
  if (!mStatus) {
    return mStatus;
  }

  return ReadVariant(aBufferSize,
                     PcqReadBytesMatcher{*(this->mConsumer), mRead, mWrite,
                                         aBuffer, aBufferSize});
}

template <typename T>
template <typename Matcher>
QueueStatus ConsumerView<T>::ReadVariant(size_t aBufferSize,
                                         Matcher&& aMatcher) {
  if (!mStatus) {
    return mStatus;
  }

  if (NeedsSharedMemory(aBufferSize, mConsumer->Size())) {
    // Always read shared-memory -- don't just skip.
    mozilla::ipc::SharedMemoryBasic::Handle handle;
    if (!ReadParam(&handle)) {
      return GetStatus();
    }

    // TODO: Find some way to MOZ_RELEASE_ASSERT that buffersize exactly matches
    // what was in queue.  This doesn't appear to be possible with the
    // information available.
    // TODO: This needs to return the same refptr even when peeking/during
    // transactions that get aborted/rewound.  So this is wrong.
    auto sharedMem = MakeRefPtr<mozilla::ipc::SharedMemoryBasic>();
    if (!sharedMem->IsHandleValid(handle) ||
        !sharedMem->SetHandle(handle,
                              mozilla::ipc::SharedMemory::RightsReadWrite) ||
        !sharedMem->Map(aBufferSize)) {
      return QueueStatus::kFatalError;
    }
    return aMatcher(sharedMem);
  }
  return aMatcher();
}

template <typename T>
size_t ConsumerView<T>::MinSizeBytes(size_t aNBytes) {
  return NeedsSharedMemory(aNBytes, mConsumer->Size())
             ? MinSizeParam((mozilla::ipc::SharedMemoryBasic::Handle*)nullptr)
             : aNBytes;
}

// ---------------------------------------------------------------

template <typename Arg>
struct QueueParamTraits<PcqTypedArg<Arg>> {
  using ParamType = PcqTypedArg<Arg>;

  template <typename U, PcqTypeInfoID ArgTypeId = PcqTypeInfo<Arg>::ID>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    MOZ_ASSERT(aArg.mWrite);
    aProducerView.WriteParam(ArgTypeId);
    return aProducerView.WriteParam(*aArg.mWrite);
  }

  template <typename U, PcqTypeInfoID ArgTypeId = PcqTypeInfo<Arg>::ID>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    MOZ_ASSERT(aArg->mRead);
    PcqTypeInfoID typeId;
    if (!aConsumerView.ReadParam(&typeId)) {
      return aConsumerView.GetStatus();
    }
    return (typeId == ArgTypeId) ? aConsumerView.ReadParam(aArg)
                                 : QueueStatus::kTypeError;
  }

  template <typename View>
  static constexpr size_t MinSize(View& aView, const ParamType* aArg) {
    return sizeof(PcqTypeInfoID) +
           aView.MinSize(aArg->mWrite ? aArg->mWrite : aArg->mRead);
  }
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
  static constexpr size_t MinSize(View& aView, const Arg* aArg) {
    static_assert(mozilla::webgl::template IsTriviallySerializable<Arg>::value,
                  "No QueueParamTraits specialization was found for this type "
                  "and it does not satisfy IsTriviallySerializable.");
    return sizeof(Arg);
  }
};

// ---------------------------------------------------------------

template <>
struct IsTriviallySerializable<QueueStatus> : std::true_type {};

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
    if (aArg) {
      aArg->SetIsVoid(isVoid);
    }
    if (isVoid) {
      return QueueStatus::kSuccess;
    }

    uint32_t len = 0;
    if (!aConsumerView.ReadParam(&len)) {
      return aConsumerView.GetStatus();
    }

    if (len == 0) {
      if (aArg) {
        *aArg = "";
      }
      return QueueStatus::kSuccess;
    }

    char* buf = aArg ? new char[len + 1] : nullptr;
    if (aArg && (!buf)) {
      return QueueStatus::kOOMError;
    }
    if (!aConsumerView.Read(buf, len)) {
      return aConsumerView.GetStatus();
    }
    buf[len] = '\0';
    if (aArg) {
      aArg->Adopt(buf, len);
    }
    return QueueStatus::kSuccess;
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    size_t minSize = aView.template MinSizeParam<bool>(nullptr);
    if ((!aArg) || aArg->IsVoid()) {
      return minSize;
    }
    minSize += aView.template MinSizeParam<uint32_t>(nullptr) +
               aView.MinSizeBytes(aArg->Length());
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
    if (aArg) {
      aArg->SetIsVoid(isVoid);
    }
    if (isVoid) {
      return QueueStatus::kSuccess;
    }

    // DLP: No idea if this includes null terminator
    uint32_t len = 0;
    if (!aConsumerView.ReadParam(&len)) {
      return aConsumerView.GetStatus();
    }

    if (len == 0) {
      if (aArg) {
        *aArg = nsString();
      }
      return QueueStatus::kSuccess;
    }

    uint32_t sizeofchar = sizeof(typename ParamType::char_type);
    typename ParamType::char_type* buf = nullptr;
    if (aArg) {
      buf = static_cast<typename ParamType::char_type*>(
          malloc((len + 1) * sizeofchar));
      if (!buf) {
        return QueueStatus::kOOMError;
      }
    }

    if (!aConsumerView.Read(buf, len * sizeofchar)) {
      return aConsumerView.GetStatus();
    }

    buf[len] = L'\0';
    if (aArg) {
      aArg->Adopt(buf, len);
    }

    return QueueStatus::kSuccess;
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    size_t minSize = aView.template MinSizeParam<bool>(nullptr);
    if ((!aArg) || aArg->IsVoid()) {
      return minSize;
    }
    uint32_t sizeofchar = sizeof(typename ParamType::char_type);
    minSize += aView.template MinSizeParam<uint32_t>(nullptr) +
               aView.MinSizeBytes(aArg->Length() * sizeofchar);
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

    if (aArg && !aArg->AppendElements(arrayLen, fallible)) {
      return QueueStatus::kOOMError;
    }

    for (auto i : IntegerRange(arrayLen)) {
      ElementType* elt = aArg ? (&aArg->ElementAt(i)) : nullptr;
      aConsumerView.ReadParam(elt);
    }
    return aConsumerView.GetStatus();
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    size_t ret = aView.template MinSizeParam<size_t>(nullptr);
    if (!aArg) {
      return ret;
    }

    for (auto& elt : aArg) {
      ret += aView.MinSizeParam(&elt);
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

    if (aArg && !aArg->AppendElements(arrayLen, fallible)) {
      return QueueStatus::kOOMError;
    }

    return aConsumerView.Read(aArg->Elements(), arrayLen * sizeof(ElementType));
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    size_t ret = aView.template MinSizeParam<size_t>(nullptr);
    if (!aArg) {
      return ret;
    }

    ret += aView.MinSizeBytes(aArg->Length() * sizeof(ElementType));
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
    for (size_t i = 0; i < Length; ++i) {
      aProducerView.WriteParam(aArg[i]);
    }
    return aProducerView.GetStatus();
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    for (size_t i = 0; i < Length; ++i) {
      ElementType* elt = aArg ? (&((*aArg)[i])) : nullptr;
      aConsumerView.ReadParam(elt);
    }
    return aConsumerView.GetStatus();
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    size_t ret = 0;
    for (size_t i = 0; i < Length; ++i) {
      ret += aView.MinSizeParam(&((*aArg)[i]));
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
  static size_t MinSize(View& aView, const ParamType* aArg) {
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
      if (aArg) {
        aArg->reset();
      }
      return QueueStatus::kSuccess;
    }

    if (!aArg) {
      return aConsumerView.template ReadParam<ElementType>(nullptr);
    }

    aArg->emplace();
    return aConsumerView.ReadParam(aArg->ptr());
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    return aView.template MinSizeParam<bool>(nullptr) +
           ((aArg && aArg->isSome()) ? aView.MinSizeParam(&aArg->ref()) : 0);
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
      if (aArg) {
        aArg->reset();
      }
      return QueueStatus::kSuccess;
    }

    if (!aArg) {
      return aConsumerView.template ReadParam<Variant<T, Ts...>>(nullptr);
    }

    aArg->emplace(VariantType<T>());
    return aConsumerView.ReadParam(aArg->ptr());
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    return aView.template MinSizeParam<bool>(nullptr) +
           ((aArg && aArg->isSome()) ? aView.MinSizeParam(&aArg->ref()) : 0);
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
    aConsumerView.ReadParam(aArg ? (&aArg->first()) : nullptr);
    return aConsumerView.ReadParam(aArg ? (&aArg->second()) : nullptr);
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    return aView.MinSizeParam(aArg ? aArg->first() : nullptr) +
           aView.MinSizeParam(aArg ? aArg->second() : nullptr);
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
      if (aArg) {
        aArg->reset(nullptr);
      }
      return QueueStatus::kSuccess;
    }

    T* obj = nullptr;
    if (aArg) {
      obj = new T();
      if (!obj) {
        return QueueStatus::kOOMError;
      }
      aArg->reset(obj);
    }
    return aConsumerView.ReadParam(obj);
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    if ((!aArg) || (!aArg->get())) {
      return aView.template MinSizeParam<bool>(nullptr);
    }
    return aView.template MinSizeParam<bool>(nullptr) +
           aView.MinSizeParam(aArg->get());
  }
};

// ---------------------------------------------------------------

// Both the Producer and the Consumer are required to maintain (i.e. close)
// the FileDescriptor themselves.  The PCQ does not do this for you, nor does
// it use FileDescriptor::auto_close.
#if defined(OS_WIN)
template <>
struct IsTriviallySerializable<base::SharedMemoryHandle> : std::true_type {};
#elif defined(OS_POSIX)
// SharedMemoryHandle is typedefed to base::FileDescriptor
template <>
struct QueueParamTraits<base::FileDescriptor> {
  using ParamType = base::FileDescriptor;

  template <typename U>
  static QueueStatus Write(ProducerView<U>& aProducerView,
                           const ParamType& aArg) {
    // PCQs don't use auto-close.
    // Convert any negative (i.e. invalid) fds to -1, as done with ParamTraits
    // (TODO: why?)
    return aProducerView.WriteParam(aArg.fd > 0 ? aArg.fd : -1);
  }

  template <typename U>
  static QueueStatus Read(ConsumerView<U>& aConsumerView, ParamType* aArg) {
    int fd;
    if (!aConsumerView.ReadParam(aArg ? &fd : nullptr)) {
      return aConsumerView.GetStatus();
    }

    if (aArg) {
      aArg->fd = fd;
      aArg->auto_close = false;  // PCQs don't use auto-close.
    }
    return QueueStatus::kSuccess;
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    return aView.MinSizeParam(aArg ? &aArg->fd : nullptr);
  }
};
#endif

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
        if (aArg) {
          return aView.ReadParam(static_cast<EntryType*>(aArg->ptr()));
        }
        return aView.template ReadParam<EntryType>();
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
    if (aArg) {
      aArg->tag = tag;
    }
    return VariantReader<sizeof...(Types)>::Read(aConsumerView, tag, aArg);
  }

  // Get the min size of the given variant or get the min size of all of the
  // variant's types.
  template <size_t N, typename View>
  struct MinSizeVariant {
    using Next = MinSizeVariant<N - 1, View>;
    static size_t MinSize(View& aView, const Tag* aTag, const ParamType* aArg) {
      using EntryType = typename mozilla::detail::Nth<N - 1, Types...>::Type;
      if (!aArg) {
        return std::min(aView.template MinSizeParam<EntryType>(),
                        Next::MinSize(aView, aTag, aArg));
      }
      MOZ_ASSERT(aTag);
      if (*aTag == N - 1) {
        return aView.MinSizeParam(&aArg->template as<EntryType>());
      }
      return Next::MinSize(aView, aTag, aArg);
    }
  };

  template <typename View>
  struct MinSizeVariant<0, View> {
    // We've reached the end of the type list.  We will legitimately get here
    // when calculating MinSize for a null Variant.
    static size_t MinSize(View& aView, const Tag* aTag, const ParamType* aArg) {
      if (!aArg) {
        return 0;
      }
      MOZ_ASSERT_UNREACHABLE("Tag wasn't for an entry in this Variant");
      return 0;
    }
  };

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    const Tag* tag = aArg ? &aArg->tag : nullptr;
    return aView.MinSizeParam(tag) +
           MinSizeVariant<sizeof...(Types), View>::MinSize(aView, tag, aArg);
  }
};

}  // namespace webgl
}  // namespace mozilla

#endif  // _QUEUEPARAMTRAITS_H_
