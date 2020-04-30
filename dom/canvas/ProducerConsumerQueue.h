/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProducerConsumerQueue_h
#define mozilla_ipc_ProducerConsumerQueue_h 1

#include <atomic>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include "mozilla/ipc/SharedMemoryBasic.h"
#include "mozilla/Assertions.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/TimeStamp.h"
#include "nsString.h"
#include "CrossProcessSemaphore.h"

namespace IPC {

template <typename T>
struct ParamTraits;

typedef uint32_t PcqTypeInfoID;
template <typename T>
struct PcqTypeInfo;

/**
 * User-defined types can be added to TypeInfo 'locally', meaning in any
 * context where they are defined.  This is done with the MAKE_TYPEINFO
 * macro, which associates a type with an integer ID.  User-defined types
 * should never use IDs below USER_TYPEINFO_START.  They are reserved
 * for the system.
 *
 * MAKE_TYPEINFO should only be used in the IPC namespace.
 */
#define MAKE_PCQTYPEINFO(__TYPENAME, __TYPEID) \
  template <>                                  \
  struct PcqTypeInfo<__TYPENAME> {             \
    static const PcqTypeInfoID ID = __TYPEID;  \
  };

MAKE_PCQTYPEINFO(bool, 1)
MAKE_PCQTYPEINFO(int8_t, 2)
MAKE_PCQTYPEINFO(uint8_t, 3)
MAKE_PCQTYPEINFO(int16_t, 4)
MAKE_PCQTYPEINFO(uint16_t, 5)
MAKE_PCQTYPEINFO(int32_t, 6)
MAKE_PCQTYPEINFO(uint32_t, 7)
MAKE_PCQTYPEINFO(int64_t, 8)
MAKE_PCQTYPEINFO(uint64_t, 9)
MAKE_PCQTYPEINFO(float, 10)
MAKE_PCQTYPEINFO(double, 11)
MAKE_PCQTYPEINFO(nsresult, 20)
MAKE_PCQTYPEINFO(nsString, 21)
MAKE_PCQTYPEINFO(nsCString, 22)

// TypeInfoIDs below this value are reserved for the system.
static const PcqTypeInfoID PcqTypeInfo_UserStart = 10000;

}  // namespace IPC

namespace mozilla {
namespace webgl {

using IPC::PcqTypeInfo;
using IPC::PcqTypeInfoID;

extern LazyLogModule gPCQLog;
#define PCQ_LOG_(lvl, ...) MOZ_LOG(mozilla::webgl::gPCQLog, lvl, (__VA_ARGS__))
#define PCQ_LOGD(...) PCQ_LOG_(LogLevel::Debug, __VA_ARGS__)
#define PCQ_LOGE(...) PCQ_LOG_(LogLevel::Error, __VA_ARGS__)

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
  using Type = std::remove_reference_t<std::remove_cv_t<T>>;
};

template <typename T>
struct IsTriviallySerializable
    : public std::integral_constant<bool, std::is_enum<T>::value ||
                                              std::is_arithmetic<T>::value> {};

class ProducerConsumerQueue;
class Producer;
class Consumer;

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

// Provides type-checking for PCQ parameters.
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
 * Used to give QueueParamTraits a way to write to the Producer without
 * actually altering it, in case the transaction fails.
 * THis object maintains the error state of the transaction and
 * discards commands issued after an error is encountered.
 */
class ProducerView {
 public:
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
class ConsumerView {
 public:
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

}  // namespace webgl

// NB: detail is in mozilla instead of mozilla::webgl because many points in
// existing code get confused if mozilla::detail and mozilla::webgl::detail
// exist.
namespace detail {
using IPC::PcqTypeInfo;
using IPC::PcqTypeInfoID;

using mozilla::ipc::Shmem;
using mozilla::webgl::IsSuccess;
using mozilla::webgl::ProducerConsumerQueue;
using mozilla::webgl::QueueStatus;

constexpr size_t GetCacheLineSize() { return 64; }

// NB: The header may end up consuming fewer bytes than this.  This value
// guarantees that we can always byte-align the header contents.
constexpr size_t GetMaxHeaderSize() {
  // Recall that the Shmem contents are laid out like this:
  // -----------------------------------------------------------------------
  // queue contents | align1 | mRead | align2 | mWrite | align3 | User Data
  // -----------------------------------------------------------------------

  constexpr size_t alignment =
      std::max(std::alignment_of<size_t>::value, GetCacheLineSize());
  static_assert(alignment >= sizeof(size_t),
                "alignment expected to be large enough to hold a size_t");

  // We may need up to this many bytes to properly align mRead
  constexpr size_t maxAlign1 = alignment - 1;
  constexpr size_t readAndAlign2 = alignment;
  constexpr size_t writeAndAlign3 = alignment;
  return maxAlign1 + readAndAlign2 + writeAndAlign3;
}

inline size_t UsedBytes(size_t aQueueBufferSize, size_t aRead, size_t aWrite) {
  return (aRead <= aWrite) ? aWrite - aRead
                           : (aQueueBufferSize - aRead) + aWrite;
}

inline size_t FreeBytes(size_t aQueueBufferSize, size_t aRead, size_t aWrite) {
  // Remember, queueSize is queueBufferSize-1
  return (aQueueBufferSize - 1) - UsedBytes(aQueueBufferSize, aRead, aWrite);
}

template <typename View, typename Arg, typename... Args>
size_t MinSizeofArgs(View& aView, const Arg* aArg, const Args*... aArgs) {
  return aView.MinSizeParam(aArg) + MinSizeofArgs(aView, aArgs...);
}

template <typename View>
size_t MinSizeofArgs(View&) {
  return 0;
}

template <typename View, typename Arg1, typename Arg2, typename... Args>
size_t MinSizeofArgs(View& aView) {
  return aView.template MinSizeParam<Arg1>(nullptr) +
         MinSizeofArgs<Arg2, Args...>(aView);
}

template <typename View, typename Arg>
size_t MinSizeofArgs(View& aView) {
  return aView.MinSizeParam(nullptr);
}

// Currently, require any parameters expected to need more than 1/16 the total
// number of bytes in the command queue to use their own SharedMemory.
// TODO: Base this heuristic on something.  Also, if I made the PCQ size
// (aTotal) a template parameter then this could be a compile-time check
// in nearly all cases.
inline bool NeedsSharedMemory(size_t aRequested, size_t aTotal) {
  return (aTotal / 16) < aRequested;
}

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

class PcqRCSemaphore {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PcqRCSemaphore)
  explicit PcqRCSemaphore(CrossProcessSemaphore* aSem) : mSem(aSem) {
    MOZ_ASSERT(mSem);
  }

  bool Wait(const Maybe<TimeDuration>& aTime) { return mSem->Wait(aTime); }
  void Signal() { mSem->Signal(); }
  bool IsAvailable() {
    MOZ_ASSERT_UNREACHABLE("Unimplemented");
    return false;
  }
  CrossProcessSemaphoreHandle ShareToProcess(base::ProcessId aTargetPid) {
    return mSem->ShareToProcess(aTargetPid);
  }
  void CloseHandle() { mSem->CloseHandle(); }

 private:
  ~PcqRCSemaphore() { delete mSem; }
  CrossProcessSemaphore* mSem;
};

/**
 * Common base class for Producer and Consumer.
 */
class PcqBase {
 public:
  /**
   * Bytes used in the queue if the parameters are the read/write heads.
   */
  size_t UsedBytes(size_t aRead, size_t aWrite) {
    MOZ_ASSERT(ValidState(aRead, aWrite));
    return detail::UsedBytes(QueueBufferSize(), aRead, aWrite);
  }

  /**
   * Bytes free in the queue if the parameters are the read/write heads.
   */
  size_t FreeBytes(size_t aRead, size_t aWrite) {
    MOZ_ASSERT(ValidState(aRead, aWrite));
    return detail::FreeBytes(QueueBufferSize(), aRead, aWrite);
  }

  /**
   * True when this queue is valid with the parameters as the read/write heads.
   */
  bool ValidState(size_t aRead, size_t aWrite) {
    return (aRead < QueueBufferSize()) && (aWrite < QueueBufferSize());
  }

  /**
   * True when this queue is empty with the parameters as the read/write heads.
   */
  bool IsEmpty(size_t aRead, size_t aWrite) {
    MOZ_ASSERT(ValidState(aRead, aWrite));
    return UsedBytes(aRead, aWrite) == 0;
  }

  /**
   * True when this queue is full with the parameters as the read/write heads.
   */
  bool IsFull(size_t aRead, size_t aWrite) {
    MOZ_ASSERT(ValidState(aRead, aWrite));
    return FreeBytes(aRead, aWrite) == 0;
  }

  // Cheaply get the used size of the current queue.  This does no
  // synchronization so the information may be stale.  On the Producer
  // side, it will never underestimate the number of bytes used and,
  // on the Consumer side, it will never overestimate them.
  // (The reciprocal is true of FreeBytes.)
  size_t UsedBytes() {
    size_t write = mWrite->load(std::memory_order_relaxed);
    size_t read = mRead->load(std::memory_order_relaxed);
    return UsedBytes(read, write);
  }

  // This does no synchronization so the information may be stale.
  size_t FreeBytes() { return QueueSize() - UsedBytes(); }

  // This does no synchronization so the information may be stale.
  bool IsEmpty() { return IsEmpty(GetReadRelaxed(), GetWriteRelaxed()); }

  // This does no synchronization so the information may be stale.
  bool IsFull() { return IsFull(GetReadRelaxed(), GetWriteRelaxed()); }

 protected:
  friend struct mozilla::ipc::IPDLParamTraits<PcqBase>;
  friend ProducerConsumerQueue;

  PcqBase()
      : mOtherPid(0),
        mQueue(nullptr),
        mQueueBufferSize(0),
        mUserReservedMemory(nullptr),
        mUserReservedSize(0),
        mRead(nullptr),
        mWrite(nullptr) {}

  PcqBase(Shmem& aShmem, base::ProcessId aOtherPid, size_t aQueueSize,
          RefPtr<PcqRCSemaphore> aMaybeNotEmptySem,
          RefPtr<PcqRCSemaphore> aMaybeNotFullSem) {
    Set(aShmem, aOtherPid, aQueueSize, aMaybeNotEmptySem, aMaybeNotFullSem);
  }

  PcqBase(const PcqBase&) = delete;
  PcqBase(PcqBase&&) = default;
  PcqBase& operator=(const PcqBase&) = delete;
  PcqBase& operator=(PcqBase&&) = default;

  void Set(Shmem& aShmem, base::ProcessId aOtherPid, size_t aQueueSize,
           RefPtr<PcqRCSemaphore> aMaybeNotEmptySem,
           RefPtr<PcqRCSemaphore> aMaybeNotFullSem) {
    mOtherPid = aOtherPid;
    mShmem = aShmem;
    mQueue = aShmem.get<uint8_t>();

    // NB: The buffer needs one extra byte for the queue contents
    mQueueBufferSize = aQueueSize + 1;

    // Recall that the Shmem contents are laid out like this:
    // -----------------------------------------------------------------------
    // queue contents | align1 | mRead | align2 | mWrite | align3 | User Data
    // -----------------------------------------------------------------------

    size_t shmemSize = aShmem.Size<uint8_t>();
    uint8_t* header = mQueue + mQueueBufferSize;

    constexpr size_t alignment =
        std::max(std::alignment_of<size_t>::value, GetCacheLineSize());
    static_assert(alignment >= sizeof(size_t),
                  "alignment expected to be large enough to hold a size_t");

    static_assert((alignment & (alignment - 1)) == 0,
                  "alignment must be a power of 2");

    // We may need up to this many bytes to properly align mRead
    constexpr size_t maxAlign1 = alignment - 1;

    // Find the lowest value of align1 that assures proper byte-alignment.
    uintptr_t alignValue = reinterpret_cast<uintptr_t>(header + maxAlign1);
    alignValue &= ~(alignment - 1);
    uint8_t* metadata = reinterpret_cast<uint8_t*>(alignValue);

    // NB: We do not call the nontrivial constructor here (we do not write
    // `new std::atomic_size_t()`) because it would zero the read/write values
    // in the shared memory, which may already represent data in the queue.
    mRead = new (metadata) std::atomic_size_t;
    mWrite = new (metadata + alignment) std::atomic_size_t;

    // The actual number of bytes we needed to properly align mRead
    size_t align1 = metadata - header;
    MOZ_ASSERT(align1 <= maxAlign1);

    // The rest of the memory is the user reserved memory
    size_t headerSize = align1 + 2 * alignment;
    size_t userSize = shmemSize - mQueueBufferSize - headerSize;
    if (userSize > 0) {
      mUserReservedMemory = mQueue + mQueueBufferSize + headerSize;
      mUserReservedSize = userSize;
    } else {
      mUserReservedMemory = nullptr;
      mUserReservedSize = 0;
    }

    // We use Monitors to wait for data when reading from an empty queue
    // and to wait for free space when writing to a full one.
    MOZ_ASSERT(aMaybeNotEmptySem && aMaybeNotFullSem);
    mMaybeNotEmptySem = aMaybeNotEmptySem;
    mMaybeNotFullSem = aMaybeNotFullSem;

    PCQ_LOGD("Created queue (%p) with size: %zu, alignment: %zu, align1: %zu",
             this, aQueueSize, alignment, align1);
  }

  ~PcqBase() {
    PCQ_LOGD("Destroying queue (%p).", this);
    // NB: We would call the destructors for mRead and mWrite here (but not
    // delete since their memory belongs to the shmem) but the std library's
    // type aliases make this tricky and, by the spec for std::atomic, their
    // destructors are trivial (i.e. no-ops) anyway.
  }

  size_t GetReadRelaxed() { return mRead->load(std::memory_order_relaxed); }

  size_t GetWriteRelaxed() { return mWrite->load(std::memory_order_relaxed); }

  /**
   * The QueueSize is the number of bytes the queue can hold.  The queue is
   * backed by a buffer that is one byte larger than this, meaning that one
   * byte of the buffer is always wasted.
   * This is usually the right method to use when testing queue capacity.
   */
  size_t QueueSize() { return QueueBufferSize() - 1; }

  /**
   * The QueueBufferSize is the number of bytes in the buffer that the queue
   * uses for storage.
   * This is usually the right method to use when calculating read/write head
   * positions.
   */
  size_t QueueBufferSize() { return mQueueBufferSize; }

  // PID of process on the other end.  Both ends may run on the same process.
  base::ProcessId mOtherPid;

  uint8_t* mQueue;
  size_t mQueueBufferSize;

  // Pointer to memory reserved for use by the user, or null if none
  uint8_t* mUserReservedMemory;
  size_t mUserReservedSize;

  // These std::atomics are in shared memory so DO NOT DELETE THEM!  We should,
  // however, call their destructors.
  std::atomic_size_t* mRead;
  std::atomic_size_t* mWrite;

  // The Shmem contents are laid out like this:
  // -----------------------------------------------------------------------
  // queue contents | align1 | mRead | align2 | mWrite | align3 | User Data
  // -----------------------------------------------------------------------
  // where align1 is chosen so that mRead is properly aligned for a
  // std_atomic_size_t and is on a cache line separate from the queue contents
  // align2 and align3 is chosen to separate mRead/mWrite and mWrite/User Data
  // similarly.
  Shmem mShmem;

  // Two semaphores that are signaled when the queue goes from a state
  // where it definitely is empty/full to a state where it "may not be".
  // Therefore, we can wait on them and know that we will be awakened if
  // there may be work to do.
  // Our use of these semaphores leans heavily on the assumption that
  // the queue is used by one producer and one consumer.
  RefPtr<PcqRCSemaphore> mMaybeNotEmptySem;
  RefPtr<PcqRCSemaphore> mMaybeNotFullSem;
};

}  // namespace detail

namespace webgl {

using mozilla::ipc::Shmem;

/**
 * The Producer is the endpoint that inserts elements into the queue.  It
 * should only be used from one thread at a time.
 */
class Producer : public detail::PcqBase {
 public:
  Producer(Producer&& aOther) = default;
  Producer& operator=(Producer&&) = default;
  Producer() = default;  // for IPDL

  /**
   * The number of bytes that the queue can hold.
   */
  size_t Size() { return QueueSize(); }

  /**
   * Attempts to insert aArgs into the queue.  If the operation does not
   * succeed then the queue is unchanged.
   */
  template <typename... Args>
  QueueStatus TryInsert(Args&&... aArgs) {
    size_t write = mWrite->load(std::memory_order_relaxed);
    const size_t initWrite = write;
    size_t read = mRead->load(std::memory_order_acquire);

    if (!ValidState(read, write)) {
      PCQ_LOGE(
          "Queue was found in an invalid state.  Queue Size: %zu.  "
          "Read: %zu.  Write: %zu",
          Size(), read, write);
      return QueueStatus::kFatalError;
    }

    ProducerView view(this, read, &write);

    // Check that the queue has enough unoccupied room for all Args types.
    // This is based on the user's size estimate for args from QueueParamTraits.
    size_t bytesNeeded = detail::MinSizeofArgs(view, &aArgs...);

    if (Size() < bytesNeeded) {
      PCQ_LOGE(
          "Queue is too small for objects.  Queue Size: %zu.  "
          "Needed: %zu",
          Size(), bytesNeeded);
      return QueueStatus::kTooSmall;
    }

    if (FreeBytes(read, write) < bytesNeeded) {
      PCQ_LOGD(
          "Not enough room to insert.  Has: %zu (%zu,%zu).  "
          "Needed: %zu",
          FreeBytes(read, write), read, write, bytesNeeded);
      return QueueStatus::kNotReady;
    }

    // Try to insert args in sequence.  Only update the queue if the
    // operation was successful.  We already checked all normal means of
    // failure but we can expect occasional failure here if the user's
    // QueueParamTraits::MinSize method was inexact.
    QueueStatus status = TryInsertHelper(view, aArgs...);
    if (!status) {
      PCQ_LOGD(
          "Failed to insert with error (%d).  Has: %zu (%zu,%zu).  "
          "Estimate of bytes needed: %zu",
          (int)status, FreeBytes(read, write), read, write, bytesNeeded);
      return status;
    }

    MOZ_ASSERT(ValidState(read, write));

    // Check that at least bytesNeeded were produced.  Failing this means
    // that some QueueParamTraits::MinSize estimated too many bytes.
    bool enoughBytes =
        UsedBytes(read, write) >=
        UsedBytes(read, (initWrite + bytesNeeded) % QueueBufferSize());
    MOZ_ASSERT(enoughBytes);
    if (!enoughBytes) {
      return QueueStatus::kFatalError;
    }

    // Commit the transaction.
    PCQ_LOGD(
        "Successfully inserted.  Producer used %zu bytes total.  "
        "Write index: %zu -> %zu",
        bytesNeeded, initWrite, write);
    mWrite->store(write, std::memory_order_release);

    // Set the semaphore (unless it is already set) to let the consumer know
    // that the queue may not be empty.  We just need to guarantee that it
    // was set (i.e. non-zero) at some time after mWrite was updated.
    if (!mMaybeNotEmptySem->IsAvailable()) {
      mMaybeNotEmptySem->Signal();
    }
    return status;
  }

  /**
   * Attempts to insert aArgs into the queue.  If the operation does not
   * succeed in the time allotted then the queue is unchanged.
   */
  template <typename... Args>
  QueueStatus TryWaitInsert(const Maybe<TimeDuration>& aDuration,
                            Args&&... aArgs) {
    return TryWaitInsertImpl(false, aDuration, std::forward<Args>(aArgs)...);
  }

  template <typename... Args>
  QueueStatus TryTypedInsert(Args&&... aArgs) {
    return TryInsert(PcqTypedArg<Args>(aArgs)...);
  }

 protected:
  friend ProducerConsumerQueue;
  friend ProducerView;

  template <typename Arg, typename... Args>
  QueueStatus TryInsertHelper(ProducerView& aView, const Arg& aArg,
                              const Args&... aArgs) {
    QueueStatus status = TryInsertItem(aView, aArg);
    return IsSuccess(status) ? TryInsertHelper(aView, aArgs...) : status;
  }

  QueueStatus TryInsertHelper(ProducerView&) { return QueueStatus::kSuccess; }

  template <typename Arg>
  QueueStatus TryInsertItem(ProducerView& aView, const Arg& aArg) {
    return QueueParamTraits<typename RemoveCVR<Arg>::Type>::Write(aView, aArg);
  }

  template <typename... Args>
  QueueStatus TryWaitInsertImpl(bool aRecursed,
                                const Maybe<TimeDuration>& aDuration,
                                Args&&... aArgs) {
    // Wait up to aDuration for the not-full semaphore to be signaled.
    // If we run out of time then quit.
    TimeStamp start(TimeStamp::Now());
    if (aRecursed && (!mMaybeNotFullSem->Wait(aDuration))) {
      return QueueStatus::kNotReady;
    }

    // Attempt to insert all args.  No waiting is done here.
    QueueStatus status = TryInsert(std::forward<Args>(aArgs)...);

    TimeStamp now;
    if (aRecursed && IsSuccess(status)) {
      // If our local view of the queue is that it is still not full then
      // we know it won't get full without us (we are the only producer).
      // So re-set the not-full semaphore unless it's already set.
      // (We are also the only not-full semaphore decrementer so it can't
      // become 0.)
      if ((!IsFull()) && (!mMaybeNotFullSem->IsAvailable())) {
        mMaybeNotFullSem->Signal();
      }
    } else if ((status == QueueStatus::kNotReady) &&
               (aDuration.isNothing() ||
                ((now = TimeStamp::Now()) - start) < aDuration.value())) {
      // We don't have enough room but still have time, e.g. because
      // the consumer read some data but not enough or because the
      // not-full semaphore gave a false positive.  Either way, retry.
      status =
          aDuration.isNothing()
              ? TryWaitInsertImpl(true, aDuration, std::forward<Args>(aArgs)...)
              : TryWaitInsertImpl(true, Some(aDuration.value() - (now - start)),
                                  std::forward<Args>(aArgs)...);
    }

    return status;
  }

  template <typename Arg>
  QueueStatus WriteObject(size_t aRead, size_t* aWrite, const Arg& arg,
                          size_t aArgSize) {
    return mozilla::detail::Marshaller::WriteObject(
        mQueue, QueueBufferSize(), aRead, aWrite, arg, aArgSize);
  }

  Producer(Shmem& aShmem, base::ProcessId aOtherPid, size_t aQueueSize,
           RefPtr<detail::PcqRCSemaphore> aMaybeNotEmptySem,
           RefPtr<detail::PcqRCSemaphore> aMaybeNotFullSem)
      : PcqBase(aShmem, aOtherPid, aQueueSize, aMaybeNotEmptySem,
                aMaybeNotFullSem) {
    // Since they are shared, this initializes mRead/mWrite in the Consumer
    // as well.
    *mRead = 0;
    *mWrite = 0;
  }

  Producer(const Producer&) = delete;
  Producer& operator=(const Producer&) = delete;
};

class Consumer : public detail::PcqBase {
 public:
  Consumer(Consumer&& aOther) = default;
  Consumer& operator=(Consumer&&) = default;
  Consumer() = default;  // for IPDL

  /**
   * The number of bytes that the queue can hold.
   */
  size_t Size() { return QueueSize(); }

  /**
   * Attempts to copy aArgs in the queue.  The queue remains unchanged.
   */
  template <typename... Args>
  QueueStatus TryPeek(Args&... aArgs) {
    return TryPeekOrRemove<false, Args...>(
        [&](ConsumerView& aView) -> QueueStatus {
          return TryPeekRemoveHelper(aView, &aArgs...);
        });
  }

  template <typename... Args>
  QueueStatus TryTypedPeek(Args&... aArgs) {
    return TryPeek(PcqTypedArg<Args>(aArgs)...);
  }

  /**
   * Attempts to copy and remove aArgs from the queue.  If the operation does
   * not succeed then the queue is unchanged.
   */
  template <typename... Args>
  QueueStatus TryRemove(Args&... aArgs) {
    return TryPeekOrRemove<true, Args...>(
        [&](ConsumerView& aView) -> QueueStatus {
          return TryPeekRemoveHelper(aView, &aArgs...);
        });
  }

  template <typename... Args>
  QueueStatus TryTypedRemove(Args&... aArgs) {
    return TryRemove(PcqTypedArg<Args>(&aArgs)...);
  }

  /**
   * Attempts to remove Args from the queue without copying them.  If the
   * operation does not succeed then the queue is unchanged.
   */
  template <typename... Args>
  QueueStatus TryRemove() {
    using seq = std::index_sequence_for<Args...>;
    return TryRemove<Args...>(seq{});
  }

  template <typename... Args>
  QueueStatus TryTypedRemove() {
    return TryRemove<PcqTypedArg<Args>...>();
  }

  /**
   * Wait for up to aDuration to get a copy of the requested data.  If the
   * operation does not succeed in the time allotted then the queue is
   * unchanged. Pass Nothing to wait until peek succeeds.
   */
  template <typename... Args>
  QueueStatus TryWaitPeek(const Maybe<TimeDuration>& aDuration,
                          Args&... aArgs) {
    return TryWaitPeekOrRemove<false>(aDuration, aArgs...);
  }

  /**
   * Wait for up to aDuration to remove the requested data from the queue.
   * Pass Nothing to wait until removal succeeds.
   */
  template <typename... Args>
  QueueStatus TryWaitRemove(const Maybe<TimeDuration>& aDuration,
                            Args&... aArgs) {
    return TryWaitPeekOrRemove<true>(aDuration, aArgs...);
  }

  /**
   * Wait for up to aDuration to remove the requested data.  No copy
   * of the data is returned.  If the operation does not succeed in the
   * time allotted then the queue is unchanged.
   * Pass Nothing to wait until removal succeeds.
   */
  template <typename... Args>
  QueueStatus TryWaitRemove(const Maybe<TimeDuration>& aDuration) {
    // Wait up to aDuration for the not-empty semaphore to be signaled.
    // If we run out of time then quit.
    TimeStamp start(TimeStamp::Now());
    if (!mMaybeNotEmptySem->Wait(aDuration)) {
      return QueueStatus::kNotReady;
    }

    // Attempt to remove all args.  No waiting is done here.
    QueueStatus status = TryRemove<Args...>();

    TimeStamp now;
    if (IsSuccess(status)) {
      // If our local view of the queue is that it is still not empty then
      // we know it won't get empty without us (we are the only consumer).
      // So re-set the not-empty semaphore unless it's already set.
      // (We are also the only not-empty semaphore decrementer so it can't
      // become 0.)
      if ((!IsEmpty()) && (!mMaybeNotEmptySem->IsAvailable())) {
        mMaybeNotEmptySem->Signal();
      }
    } else if ((status == QueueStatus::kNotReady) &&
               (aDuration.isNothing() ||
                ((now = TimeStamp::Now()) - start) < aDuration.value())) {
      // We don't have enough data but still have time, e.g. because
      // the producer wrote some data but not enough or because the
      // not-empty semaphore gave a false positive.  Either way, retry.
      status =
          aDuration.isNothing()
              ? TryWaitRemove<Args...>(aDuration)
              : TryWaitRemove<Args...>(Some(aDuration.value() - (now - start)));
    }

    return status;
  }

 protected:
  friend ProducerConsumerQueue;
  friend ConsumerView;

  // PeekOrRemoveOperation takes a read pointer and a write index.
  using PeekOrRemoveOperation = std::function<QueueStatus(ConsumerView&)>;

  template <bool isRemove, typename... Args>
  QueueStatus TryPeekOrRemove(const PeekOrRemoveOperation& aOperation) {
    size_t write = mWrite->load(std::memory_order_acquire);
    size_t read = mRead->load(std::memory_order_relaxed);
    const size_t initRead = read;

    if (!ValidState(read, write)) {
      PCQ_LOGE(
          "Queue was found in an invalid state.  Queue Size: %zu.  "
          "Read: %zu.  Write: %zu",
          Size(), read, write);
      return QueueStatus::kFatalError;
    }

    ConsumerView view(this, &read, write);

    // Check that the queue has enough unoccupied room for all Args types.
    // This is based on the user's size estimate for Args from QueueParamTraits.
    size_t bytesNeeded = detail::MinSizeofArgs(view);

    if (Size() < bytesNeeded) {
      PCQ_LOGE(
          "Queue is too small for objects.  Queue Size: %zu.  "
          "Bytes needed: %zu.",
          Size(), bytesNeeded);
      return QueueStatus::kTooSmall;
    }

    if (UsedBytes(read, write) < bytesNeeded) {
      PCQ_LOGD(
          "Not enough data in queue.  Has: %zu (%zu,%zu).  "
          "Bytes needed: %zu",
          UsedBytes(read, write), read, write, bytesNeeded);
      return QueueStatus::kNotReady;
    }

    // Only update the queue if the operation was successful and we aren't
    // peeking.
    QueueStatus status = aOperation(view);
    if (!status) {
      return status;
    }

    // Check that at least bytesNeeded were consumed.  Failing this means
    // that some QueueParamTraits::MinSize estimated too many bytes.
    bool enoughBytes =
        FreeBytes(read, write) >=
        FreeBytes((initRead + bytesNeeded) % QueueBufferSize(), write);
    MOZ_ASSERT(enoughBytes);
    if (!enoughBytes) {
      return QueueStatus::kFatalError;
    }

    MOZ_ASSERT(ValidState(read, write));

    PCQ_LOGD(
        "Successfully %s.  Consumer used %zu bytes total.  "
        "Read index: %zu -> %zu",
        isRemove ? "removed" : "peeked", bytesNeeded, initRead, read);

    // Commit the transaction... unless we were just peeking.
    if (isRemove) {
      mRead->store(read, std::memory_order_release);
      // Set the semaphore (unless it is already set) to let the producer know
      // that the queue may not be full.  We just need to guarantee that it
      // was set (i.e. non-zero) at some time after mRead was updated.
      if (!mMaybeNotFullSem->IsAvailable()) {
        mMaybeNotFullSem->Signal();
      }
    }
    return status;
  }

  // Helper that passes nulls for all Args*
  template <typename... Args, size_t... Is>
  QueueStatus TryRemove(std::index_sequence<Is...>) {
    std::tuple<Args*...> nullArgs;
    return TryPeekOrRemove<true, Args...>([&](ConsumerView& aView) {
      return TryPeekRemoveHelper(aView, std::get<Is>(nullArgs)...);
    });
  }

  template <bool isRemove, typename... Args>
  QueueStatus TryWaitPeekOrRemove(const Maybe<TimeDuration>& aDuration,
                                  Args&&... aArgs) {
    return TryWaitPeekOrRemoveImpl<isRemove>(false, aDuration,
                                             std::forward<Args>(aArgs)...);
  }

  template <bool isRemove, typename... Args>
  QueueStatus TryWaitPeekOrRemoveImpl(bool aRecursed,
                                      const Maybe<TimeDuration>& aDuration,
                                      Args&... aArgs) {
    // Wait up to aDuration for the not-empty semaphore to be signaled.
    // If we run out of time then quit.
    TimeStamp start(TimeStamp::Now());
    if (aRecursed && (!mMaybeNotEmptySem->Wait(aDuration))) {
      return QueueStatus::kNotReady;
    }

    // Attempt to read all args.  No waiting is done here.
    QueueStatus status = isRemove ? TryRemove(aArgs...) : TryPeek(aArgs...);

    TimeStamp now;
    if (aRecursed && IsSuccess(status)) {
      // If our local view of the queue is that it is still not empty then
      // we know it won't get empty without us (we are the only consumer).
      // So re-set the not-empty semaphore unless it's already set.
      // (We are also the only not-empty semaphore decrementer so it can't
      // become 0.)
      if ((!IsEmpty()) && (!mMaybeNotEmptySem->IsAvailable())) {
        mMaybeNotEmptySem->Signal();
      }
    } else if ((status == QueueStatus::kNotReady) &&
               (aDuration.isNothing() ||
                ((now = TimeStamp::Now()) - start) < aDuration.value())) {
      // We don't have enough data but still have time, e.g. because
      // the producer wrote some data but not enough or because the
      // not-empty semaphore gave a false positive.  Either way, retry.
      status =
          aDuration.isNothing()
              ? TryWaitPeekOrRemoveImpl<isRemove>(true, aDuration, aArgs...)
              : TryWaitPeekOrRemoveImpl<isRemove>(
                    true, Some(aDuration.value() - (now - start)), aArgs...);
    }

    return status;
  }

  // Version of the helper for copying values out of the queue.
  template <typename... Args>
  QueueStatus TryPeekRemoveHelper(ConsumerView& aView, Args*... aArgs);

  template <typename Arg, typename... Args>
  QueueStatus TryPeekRemoveHelper(ConsumerView& aView, Arg* aArg,
                                  Args*... aArgs) {
    QueueStatus status = TryCopyOrSkipItem<Arg>(aView, aArg);
    return IsSuccess(status) ? TryPeekRemoveHelper<Args...>(aView, aArgs...)
                             : status;
  }

  QueueStatus TryPeekRemoveHelper(ConsumerView&) {
    return QueueStatus::kSuccess;
  }

  // If an item is available then it is copied into aArg.  The item is skipped
  // over if aArg is null.
  template <typename Arg>
  QueueStatus TryCopyOrSkipItem(ConsumerView& aView, Arg* aArg) {
    return QueueParamTraits<typename RemoveCVR<Arg>::Type>::Read(
        aView, const_cast<std::remove_cv_t<Arg>*>(aArg));
  }

  template <typename Arg>
  QueueStatus ReadObject(size_t* aRead, size_t aWrite, Arg* arg,
                         size_t aArgSize) {
    return mozilla::detail::Marshaller::ReadObject(
        mQueue, QueueBufferSize(), aRead, aWrite, arg, aArgSize);
  }

  Consumer(Shmem& aShmem, base::ProcessId aOtherPid, size_t aQueueSize,
           RefPtr<detail::PcqRCSemaphore> aMaybeNotEmptySem,
           RefPtr<detail::PcqRCSemaphore> aMaybeNotFullSem)
      : PcqBase(aShmem, aOtherPid, aQueueSize, aMaybeNotEmptySem,
                aMaybeNotFullSem) {}

  Consumer(const Consumer&) = delete;
  Consumer& operator=(const Consumer&) = delete;
};

QueueStatus ProducerView::Write(const void* aBuffer, size_t aBufferSize) {
  MOZ_ASSERT(aBuffer && (aBufferSize > 0));
  if (!mStatus) {
    return mStatus;
  }

  if (detail::NeedsSharedMemory(aBufferSize, mProducer->Size())) {
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

size_t ProducerView::MinSizeBytes(size_t aNBytes) {
  return detail::NeedsSharedMemory(aNBytes, mProducer->Size())
             ? MinSizeParam((mozilla::ipc::SharedMemoryBasic::Handle*)nullptr)
             : aNBytes;
}

QueueStatus ConsumerView::Read(void* aBuffer, size_t aBufferSize) {
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

template <typename Matcher>
QueueStatus ConsumerView::ReadVariant(size_t aBufferSize, Matcher&& aMatcher) {
  if (!mStatus) {
    return mStatus;
  }

  if (detail::NeedsSharedMemory(aBufferSize, mConsumer->Size())) {
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

size_t ConsumerView::MinSizeBytes(size_t aNBytes) {
  return detail::NeedsSharedMemory(aNBytes, mConsumer->Size())
             ? MinSizeParam((mozilla::ipc::SharedMemoryBasic::Handle*)nullptr)
             : aNBytes;
}

using mozilla::detail::GetCacheLineSize;
using mozilla::detail::GetMaxHeaderSize;

/**
 * A single producer + single consumer queue, implemented as a
 * circular queue.  The object is backed with a Shmem, which allows
 * it to be used across processes.
 *
 * To work with this queue:
 * 1. In some process (typically either the producer or consumer process),
 *    create a ProducerConsumerQueue:
 *    `UniquePtr<ProducerConsumerQueue> pcq(ProducerConsumerQueue::Create())`
 * 2. Grab either the Producer or the Consumer from the ProducerConsumerQueue
 *    with e.g.:
 *      UniquePtr<Consumer> my_consumer(std::move(pcq.mConsumer))
 * 3. (If using cross-process:) Create an IPDL message in an actor that runs
 *    in both processes.  This message will send the other endpoint from your
 *    ProducerConsumerQueue.  It needs to provide a `using` statement for the
 *    endpoint that specifies it as a shmemholder, as well as a message to send
 *    it:
 *      using shmemholder Producer from "mozilla/dom/ProducerConsumerQueue.h";
 *      // ...
 *      async MyIPDLMessage(UniquePtr<Producer> aProducer);
 *    If you don't label the type as a shmemholder then you will get a runtime
 *    error whenever you attempt to send it.
 * 4. Either send the other endpoint (producer or consumer) to the remote
 *    process and Recv it there:
 *      SendMyIPDLMessage(std::move(pcq.mProducer));
 *      ---
 *      IPCResult RecvMyIPDLMessage(UniquePtr<Producer>&& aProducer) {
 *        // ...
 *      }
 *    or grab the other endpoint for use in the same process without an IPDL
 *    message, as in step 2.
 *
 * The ProducerConsumerQueue object is then empty and can be freed.
 *
 * With endpoints in their proper processes, the producer can begin producing
 * entries and the consumer consuming them, with synchronization being handled
 * by this class.
 *
 * This is a single-producer/single-consumer queue.  Another way of saying that
 * is to say that the Producer and Consumer objects are not thread-safe.
 */
class ProducerConsumerQueue {
 private:
  UniquePtr<Producer> mProducer;
  UniquePtr<Consumer> mConsumer;

 public:
  /**
   * Create a queue whose endpoints are the same as those of aProtocol.
   * In choosing a queueSize, be aware that both the queue and the Shmem will
   * allocate additional shared memory for internal accounting (see
   * GetMaxHeaderSize) and that Shmem sizes are a multiple of the operating
   * system's page sizes.
   *
   * aAdditionalBytes of shared memory will also be allocated.
   * Clients may use this shared memory for their own purposes.
   * See GetUserReservedMemory() and GetUserReservedMemorySize()
   */
  static UniquePtr<ProducerConsumerQueue> Create(
      mozilla::ipc::IProtocol* aProtocol, size_t aQueueSize,
      size_t aAdditionalBytes = 0) {
    MOZ_ASSERT(aProtocol);
    Shmem shmem;

    // NB: We need one extra byte for the queue contents (hence the "+1").
    uint32_t totalShmemSize =
        aQueueSize + 1 + GetMaxHeaderSize() + aAdditionalBytes;

    if (!aProtocol->AllocUnsafeShmem(
            totalShmemSize, mozilla::ipc::SharedMemory::TYPE_BASIC, &shmem)) {
      return nullptr;
    }

    UniquePtr<ProducerConsumerQueue> ret =
        Create(shmem, aProtocol->OtherPid(), aQueueSize);
    if (!ret) {
      return ret;
    }

    // The system may have reserved more bytes than the user asked for.
    // Make sure they aren't given access to the extra.
    MOZ_ASSERT(ret->mProducer->mUserReservedSize >= aAdditionalBytes);
    ret->mProducer->mUserReservedSize = aAdditionalBytes;
    ret->mConsumer->mUserReservedSize = aAdditionalBytes;
    if (aAdditionalBytes == 0) {
      ret->mProducer->mUserReservedMemory = nullptr;
      ret->mConsumer->mUserReservedMemory = nullptr;
    }
    return ret;
  }

  /**
   * Create a queue that is backed by aShmem, which must be:
   * (1) unsafe
   * (2) made for use with aOtherPid (may be this process' PID)
   * (3) large enough to hold the queue contents and the shared meta-data of
   *     the queue (see GetMaxHeaderSize).  Any room left over will be available
   *     as user reserved memory.
   *     See GetUserReservedMemory() and GetUserReservedMemorySize()
   */
  static UniquePtr<ProducerConsumerQueue> Create(Shmem& aShmem,
                                                 base::ProcessId aOtherPid,
                                                 size_t aQueueSize) {
    uint32_t totalShmemSize = aShmem.Size<uint8_t>();

    // NB: We need one extra byte for the queue contents (hence the "+1").
    if ((!aShmem.IsWritable()) || (!aShmem.IsReadable()) ||
        ((GetMaxHeaderSize() + aQueueSize + 1) > totalShmemSize)) {
      return nullptr;
    }

    auto notempty = MakeRefPtr<detail::PcqRCSemaphore>(
        CrossProcessSemaphore::Create("webgl-notempty", 0));
    auto notfull = MakeRefPtr<detail::PcqRCSemaphore>(
        CrossProcessSemaphore::Create("webgl-notfull", 1));
    return WrapUnique(new ProducerConsumerQueue(aShmem, aOtherPid, aQueueSize,
                                                notempty, notfull));
  }

  /**
   * The queue needs a few bytes for 2 shared counters.  It takes these from the
   * underlying Shmem.  This will still work if the cache line size is incorrect
   * for some architecture but operations may be less efficient.
   */
  static constexpr size_t GetMaxHeaderSize() {
    return mozilla::detail::GetMaxHeaderSize();
  }

  /**
   * Cache line size for the machine.  We assume a 64-byte cache line size.
   */
  static constexpr size_t GetCacheLineSize() {
    return mozilla::detail::GetCacheLineSize();
  }

  UniquePtr<Producer> TakeProducer() { return std::move(mProducer); }
  UniquePtr<Consumer> TakeConsumer() { return std::move(mConsumer); }

 private:
  ProducerConsumerQueue(Shmem& aShmem, base::ProcessId aOtherPid,
                        size_t aQueueSize,
                        RefPtr<detail::PcqRCSemaphore>& aMaybeNotEmptySem,
                        RefPtr<detail::PcqRCSemaphore>& aMaybeNotFullSem)
      : mProducer(
            WrapUnique(new Producer(aShmem, aOtherPid, aQueueSize,
                                    aMaybeNotEmptySem, aMaybeNotFullSem))),
        mConsumer(
            WrapUnique(new Consumer(aShmem, aOtherPid, aQueueSize,
                                    aMaybeNotEmptySem, aMaybeNotFullSem))) {
    PCQ_LOGD(
        "Constructed PCQ (%p).  Shmem Size = %zu. Queue Size = %zu.  "
        "Other process ID: %08x.",
        this, aShmem.Size<uint8_t>(), aQueueSize, (uint32_t)aOtherPid);
  }
};

}  // namespace webgl

namespace ipc {

template <>
struct IPDLParamTraits<mozilla::detail::PcqBase> {
  typedef mozilla::detail::PcqBase paramType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor, paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.QueueSize());
    WriteIPDLParam(aMsg, aActor, std::move(aParam.mShmem));

    // May not currently share a Producer or Consumer with a process that it's
    // Shmem is not related to.
    MOZ_ASSERT(aActor->OtherPid() == aParam.mOtherPid);
    WriteIPDLParam(
        aMsg, aActor,
        aParam.mMaybeNotEmptySem->ShareToProcess(aActor->OtherPid()));

    WriteIPDLParam(aMsg, aActor,
                   aParam.mMaybeNotFullSem->ShareToProcess(aActor->OtherPid()));
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    size_t queueSize;
    Shmem shmem;
    CrossProcessSemaphoreHandle notEmptyHandle;
    CrossProcessSemaphoreHandle notFullHandle;

    if (!ReadIPDLParam(aMsg, aIter, aActor, &queueSize) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &shmem) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &notEmptyHandle) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &notFullHandle)) {
      return false;
    }

    MOZ_ASSERT(IsHandleValid(notEmptyHandle) && IsHandleValid(notFullHandle));
    aResult->Set(shmem, aActor->OtherPid(), queueSize,
                 MakeRefPtr<detail::PcqRCSemaphore>(
                     CrossProcessSemaphore::Create(notEmptyHandle)),
                 MakeRefPtr<detail::PcqRCSemaphore>(
                     CrossProcessSemaphore::Create(notFullHandle)));
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    IPDLParamTraits<Shmem>::Log(aParam.mShmem, aLog);
  }
};

template <>
struct IPDLParamTraits<mozilla::webgl::Producer>
    : public IPDLParamTraits<mozilla::detail::PcqBase> {
  typedef mozilla::webgl::Producer paramType;
};

template <>
struct IPDLParamTraits<mozilla::webgl::Consumer>
    : public IPDLParamTraits<mozilla::detail::PcqBase> {
  typedef mozilla::webgl::Consumer paramType;
};

}  // namespace ipc

// ---------------------------------------------------------------

namespace webgl {

template <typename Arg>
struct QueueParamTraits<PcqTypedArg<Arg>> {
  using ParamType = PcqTypedArg<Arg>;

  template <PcqTypeInfoID ArgTypeId = PcqTypeInfo<Arg>::ID>
  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    MOZ_ASSERT(aArg.mWrite);
    aProducerView.WriteParam(ArgTypeId);
    return aProducerView.WriteParam(*aArg.mWrite);
  }

  template <PcqTypeInfoID ArgTypeId = PcqTypeInfo<Arg>::ID>
  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
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
  static QueueStatus Write(ProducerView& aProducerView, const Arg& aArg) {
    static_assert(mozilla::webgl::template IsTriviallySerializable<Arg>::value,
                  "No QueueParamTraits specialization was found for this type "
                  "and it does not satisfy IsTriviallySerializable.");
    // Write self as binary
    return aProducerView.Write(&aArg, sizeof(Arg));
  }

  static QueueStatus Read(ConsumerView& aConsumerView, Arg* aArg) {
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
struct IsTriviallySerializable<QueueStatus> : TrueType {};

// ---------------------------------------------------------------

template <>
struct QueueParamTraits<nsACString> {
  using ParamType = nsACString;

  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    if ((!aProducerView.WriteParam(aArg.IsVoid())) || aArg.IsVoid()) {
      return aProducerView.GetStatus();
    }

    uint32_t len = aArg.Length();
    if ((!aProducerView.WriteParam(len)) || (len == 0)) {
      return aProducerView.GetStatus();
    }

    return aProducerView.Write(aArg.BeginReading(), len);
  }

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
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

  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
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

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
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

  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    size_t arrayLen = aArg.Length();
    aProducerView.WriteParam(arrayLen);
    for (size_t i = 0; i < aArg.Length(); ++i) {
      aProducerView.WriteParam(aArg[i]);
    }
    return aProducerView.GetStatus();
  }

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
    size_t arrayLen;
    if (!aConsumerView.ReadParam(&arrayLen)) {
      return aConsumerView.GetStatus();
    }

    if (aArg && !aArg->AppendElements(arrayLen, fallible)) {
      return QueueStatus::kOOMError;
    }

    for (size_t i = 0; i < arrayLen; ++i) {
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

    size_t arrayLen = aArg->Length();
    for (size_t i = 0; i < arrayLen; ++i) {
      ret += aView.MinSizeParam(&aArg[i]);
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

  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    size_t arrayLen = aArg.Length();
    aProducerView.WriteParam(arrayLen);
    return aProducerView.Write(&aArg[0], aArg.Length() * sizeof(ElementType));
  }

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
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

  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    for (size_t i = 0; i < Length; ++i) {
      aProducerView.WriteParam(aArg[i]);
    }
    return aProducerView.GetStatus();
  }

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
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

  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    return aProducerView.Write(aArg.begin(), sizeof(ElementType[Length]));
  }

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
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

  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    aProducerView.WriteParam(static_cast<bool>(aArg));
    return aArg ? aProducerView.WriteParam(aArg.ref())
                : aProducerView.GetStatus();
  }

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
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

  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    aProducerView.WriteParam(aArg.mIsSome);
    return (aArg.mIsSome) ? aProducerView.WriteParam(aArg.ref())
                          : aProducerView.GetStatus();
  }

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
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

  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    aProducerView.WriteParam(aArg.first);
    return aProducerView.WriteParam(aArg.second);
  }

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
    aConsumerView.ReadParam(aArg ? (&aArg->first) : nullptr);
    return aConsumerView.ReadParam(aArg ? (&aArg->second) : nullptr);
  }

  template <typename View>
  static size_t MinSize(View& aView, const ParamType* aArg) {
    return aView.MinSizeParam(aArg ? aArg->first : nullptr) +
           aView.MinSizeParam(aArg ? aArg->second : nullptr);
  }
};

// ---------------------------------------------------------------

template <typename T>
struct QueueParamTraits<UniquePtr<T>> {
  using ParamType = UniquePtr<T>;

  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    // TODO: Clean up move with PCQ
    aProducerView.WriteParam(!static_cast<bool>(aArg));
    if (aArg && aProducerView.WriteParam(*aArg.get())) {
      const_cast<ParamType&>(aArg).reset();
    }
    return aProducerView.GetStatus();
  }

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
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
  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    // PCQs don't use auto-close.
    // Convert any negative (i.e. invalid) fds to -1, as done with ParamTraits
    // (TODO: why?)
    return aProducerView.WriteParam(aArg.fd > 0 ? aArg.fd : -1);
  }

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
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
struct PcqVariantWriter {
  ProducerView& mView;
  template <typename T>
  QueueStatus match(const T& x) {
    return mView.WriteParam(x);
  }
};

template <typename... Types>
struct QueueParamTraits<Variant<Types...>> {
  using ParamType = Variant<Types...>;
  using Tag = typename mozilla::detail::VariantTag<Types...>::Type;

  static QueueStatus Write(ProducerView& aProducerView, const ParamType& aArg) {
    aProducerView.WriteParam(aArg.tag);
    return aArg.match(PcqVariantWriter{aProducerView});
  }

  // Check the N-1th tag.  See ParamTraits<mozilla::Variant> for details.
  template <size_t N, typename dummy = void>
  struct VariantReader {
    using Next = VariantReader<N - 1>;
    static QueueStatus Read(ConsumerView& aView, Tag aTag, ParamType* aArg) {
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
    static QueueStatus Read(ConsumerView& aView, Tag aTag, ParamType* aArg) {
      MOZ_ASSERT_UNREACHABLE("Tag wasn't for an entry in this Variant");
      return QueueStatus::kFatalError;
    }
  };

  static QueueStatus Read(ConsumerView& aConsumerView, ParamType* aArg) {
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

#endif  // mozilla_ipc_ProducerConsumerQueue_h
