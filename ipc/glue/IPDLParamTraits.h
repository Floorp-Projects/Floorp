#ifndef mozilla_ipc_IPDLParamTraits_h
#define mozilla_ipc_IPDLParamTraits_h

#include "chrome/common/ipc_message_utils.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace ipc {

class IProtocol;

//
// IPDLParamTraits are an extended version of ParamTraits. Unlike ParamTraits,
// IPDLParamTraits supports passing an additional IProtocol* argument to the
// write and read methods.
//
// This is important for serializing and deserializing types which require
// knowledge of which protocol they're being sent over, such as actors and
// nsIInputStreams.
//
// All types which already implement ParamTraits also support IPDLParamTraits.
//
template <typename P>
struct IPDLParamTraits {
  // This is the default impl which discards the actor parameter and calls into
  // ParamTraits. Types which want to use the actor parameter must specialize
  // IPDLParamTraits.
  template <typename R>
  static inline void Write(IPC::Message* aMsg, IProtocol*, R&& aParam) {
    static_assert(
        IsSame<P, typename IPC::ParamTraitsSelector<R>::Type>::value,
        "IPDLParamTraits::Write only forwards calls which work via WriteParam");

    IPC::ParamTraits<P>::Write(aMsg, std::forward<R>(aParam));
  }

  template <typename R>
  static inline bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                          IProtocol*, R* aResult) {
    static_assert(
        IsSame<P, typename IPC::ParamTraitsSelector<R>::Type>::value,
        "IPDLParamTraits::Read only forwards calls which work via ReadParam");

    return IPC::ParamTraits<P>::Read(aMsg, aIter, aResult);
  }
};

//
// WriteIPDLParam and ReadIPDLParam are like IPC::WriteParam and IPC::ReadParam,
// however, they also accept an extra actor argument, and use IPDLParamTraits
// rather than ParamTraits.
//
// NOTE: WriteIPDLParam takes a universal reference, so that it can support
// whatever reference type is supported by the underlying IPDLParamTraits::Write
// implementation. See the comment on IPDLParamTraits<nsTArray<T>>::Write for
// more information.
//
template <typename P>
static MOZ_NEVER_INLINE void WriteIPDLParam(IPC::Message* aMsg,
                                            IProtocol* aActor, P&& aParam) {
  IPDLParamTraits<typename IPC::ParamTraitsSelector<P>::Type>::Write(
      aMsg, aActor, std::forward<P>(aParam));
}

template <typename P>
static MOZ_NEVER_INLINE bool ReadIPDLParam(const IPC::Message* aMsg,
                                           PickleIterator* aIter,
                                           IProtocol* aActor, P* aResult) {
  return IPDLParamTraits<typename IPC::ParamTraitsSelector<P>::Type>::Read(
      aMsg, aIter, aActor, aResult);
}

constexpr void WriteIPDLParamList(IPC::Message*, IProtocol*) {}

template <typename P, typename... Ps>
static void WriteIPDLParamList(IPC::Message* aMsg, IProtocol* aActor,
                               const P& aParam, const Ps&... aParams) {
  WriteIPDLParam(aMsg, aActor, aParam);
  WriteIPDLParamList(aMsg, aActor, aParams...);
}

constexpr bool ReadIPDLParamList(const IPC::Message*, PickleIterator*,
                                 IProtocol*) {
  return true;
}

template <typename P, typename... Ps>
static bool ReadIPDLParamList(const IPC::Message* aMsg, PickleIterator* aIter,
                              IProtocol* aActor, P* aResult, Ps*... aResults) {
  return ReadIPDLParam(aMsg, aIter, aActor, aResult) &&
         ReadIPDLParamList(aMsg, aIter, aActor, aResults...);
}

// nsTArray support for IPDLParamTraits
template <typename T>
struct IPDLParamTraits<nsTArray<T>> {
  static inline void Write(IPC::Message* aMsg, IProtocol* aActor,
                           const nsTArray<T>& aParam) {
    WriteInternal(aMsg, aActor, aParam);
  }

  // Some serializers need to take a mutable reference to their backing object,
  // such as Shmem segments and Byte Buffers. These serializers take the
  // backing data and move it into the IPC layer for efficiency. This overload
  // of Write on nsTArray is needed, as occasionally these types appear inside
  // of IPDL arrays.
  static inline void Write(IPC::Message* aMsg, IProtocol* aActor,
                           nsTArray<T>&& aParam) {
    WriteInternal(aMsg, aActor, aParam);
  }

  // This method uses infallible allocation so that an OOM failure will
  // show up as an OOM crash rather than an IPC FatalError.
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, nsTArray<T>* aResult) {
    uint32_t length;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &length)) {
      return false;
    }

    if (sUseWriteBytes) {
      auto pickledLength = CheckedInt<int>(length) * sizeof(T);
      if (!pickledLength.isValid() ||
          !aMsg->HasBytesAvailable(aIter, pickledLength.value())) {
        return false;
      }

      T* elements = aResult->AppendElements(length);
      return aMsg->ReadBytesInto(aIter, elements, pickledLength.value());
    }

    // Each ReadIPDLParam<E> may read more than 1 byte each; this is an attempt
    // to minimally validate that the length isn't much larger than what's
    // actually available in aMsg. We cannot use |pickledLength|, like in the
    // codepath above, because ReadIPDLParam can read variable amounts of data
    // from aMsg.
    if (!aMsg->HasBytesAvailable(aIter, length)) {
      return false;
    }

    aResult->SetCapacity(length);

    for (uint32_t index = 0; index < length; index++) {
      T* element = aResult->AppendElement();
      if (!ReadIPDLParam(aMsg, aIter, aActor, element)) {
        return false;
      }
    }
    return true;
  }

 private:
  template <typename U>
  static inline void WriteInternal(IPC::Message* aMsg, IProtocol* aActor,
                                   U&& aParam) {
    uint32_t length = aParam.Length();
    WriteIPDLParam(aMsg, aActor, length);

    if (sUseWriteBytes) {
      auto pickledLength = CheckedInt<int>(length) * sizeof(T);
      MOZ_RELEASE_ASSERT(pickledLength.isValid());
      aMsg->WriteBytes(aParam.Elements(), pickledLength.value());
    } else {
      for (uint32_t index = 0; index < length; index++) {
        WriteIPDLParam(aMsg, aActor, std::move(aParam.Elements()[index]));
      }
    }
  }

  // We write arrays of integer or floating-point data using a single pickling
  // call, rather than writing each element individually.  We deliberately do
  // not use mozilla::IsPod here because it is perfectly reasonable to have
  // a data structure T for which IsPod<T>::value is true, yet also have a
  // {IPDL,}ParamTraits<T> specialization.
  static const bool sUseWriteBytes =
      (mozilla::IsIntegral<T>::value || mozilla::IsFloatingPoint<T>::value);
};

// Maybe support for IPDLParamTraits
template <typename T>
struct IPDLParamTraits<mozilla::Maybe<T>> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const mozilla::Maybe<T>& aParam) {
    bool isSome = aParam.isSome();
    WriteIPDLParam(aMsg, aActor, isSome);

    if (isSome) {
      WriteIPDLParam(aMsg, aActor, std::move(aParam.value()));
    }
  }

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    mozilla::Maybe<T>&& aParam) {
    bool isSome = aParam.isSome();
    WriteIPDLParam(aMsg, aActor, isSome);

    if (isSome) {
      WriteIPDLParam(aMsg, aActor, std::move(aParam.ref()));
    }
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, mozilla::Maybe<T>* aResult) {
    bool isSome;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &isSome)) {
      return false;
    }

    if (isSome) {
      aResult->emplace();
      if (!ReadIPDLParam(aMsg, aIter, aActor, aResult->ptr())) {
        return false;
      }
    } else {
      aResult->reset();
    }
    return true;
  }
};

template <typename T>
struct IPDLParamTraits<mozilla::UniquePtr<T>> {
  typedef mozilla::UniquePtr<T> paramType;

  // Allow UniquePtr<T>& and UniquePtr<T>&&
  template <typename ParamTypeRef>
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    ParamTypeRef&& aParam) {
    // write bool true if inner object is null
    WriteParam(aMsg, aParam == nullptr);
    if (aParam) {
      WriteIPDLParam(aMsg, aActor, *aParam.get());
      aParam = nullptr;
    }
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    MOZ_ASSERT(aResult);
    bool isNull;
    *aResult = nullptr;
    if (!ReadParam(aMsg, aIter, &isNull)) {
      return false;
    }
    if (isNull) {
      return true;
    }
    T* obj = new T();
    if (!ReadIPDLParam(aMsg, aIter, aActor, obj)) {
      delete obj;
      return false;
    }
    aResult->reset(obj);
    return true;
  }
};

template <typename... Ts>
struct IPDLParamTraits<Tuple<Ts...>> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const Tuple<Ts...>& aParam) {
    WriteInternal(aMsg, aActor, aParam, std::index_sequence_for<Ts...>{});
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, Tuple<Ts...>* aResult) {
    return ReadInternal(aMsg, aIter, aActor, *aResult,
                        std::index_sequence_for<Ts...>{});
  }

 private:
  template <size_t... Is>
  static void WriteInternal(IPC::Message* aMsg, IProtocol* aActor,
                            const Tuple<Ts...>& aParam,
                            std::index_sequence<Is...>) {
    WriteIPDLParamList(aMsg, aActor, Get<Is>(aParam)...);
  }

  template <size_t... Is>
  static bool ReadInternal(const IPC::Message* aMsg, PickleIterator* aIter,
                           IProtocol* aActor, Tuple<Ts...>& aResult,
                           std::index_sequence<Is...>) {
    return ReadIPDLParamList(aMsg, aIter, aActor, &Get<Is>(aResult)...);
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // defined(mozilla_ipc_IPDLParamTraits_h)
