/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCOMMANDQUEUE_H_
#define WEBGLCOMMANDQUEUE_H_

#include "mozilla/FunctionTypeTraits.h"
#include "mozilla/dom/ProducerConsumerQueue.h"
#include "mozilla/ipc/IPDLParamTraits.h"
#include <type_traits>

// Get around a bug in Clang related to __thiscall method pointers
#if defined(_M_IX86)
#  define SINK_FCN_CC __thiscall
#else
#  define SINK_FCN_CC
#endif

namespace mozilla {

using mozilla::ipc::IPDLParamTraits;
using mozilla::webgl::Consumer;
using mozilla::webgl::PcqStatus;
using mozilla::webgl::Producer;
using mozilla::webgl::ProducerConsumerQueue;

template <typename Derived, typename SinkType>
struct MethodDispatcher;

enum CommandResult { Success, TimeExpired, QueueEmpty, Error };

enum CommandSyncType { ASYNC, SYNC };

class BasicSource {
 public:
  explicit BasicSource(UniquePtr<Producer>&& aProducer)
      : mProducer(std::move(aProducer)) {
    MOZ_ASSERT(mProducer);
  }
  virtual ~BasicSource() = default;

  // For IPDL:
  BasicSource() = default;
  friend struct mozilla::ipc::IPDLParamTraits<BasicSource>;

 protected:
  UniquePtr<Producer> mProducer;
};

class BasicSink {
 public:
  explicit BasicSink(UniquePtr<Consumer>&& aConsumer)
      : mConsumer(std::move(aConsumer)) {
    MOZ_ASSERT(mConsumer);
  }
  virtual ~BasicSink() = default;

  // For IPDL:
  BasicSink() = default;
  friend struct mozilla::ipc::IPDLParamTraits<BasicSink>;

 protected:
  UniquePtr<Consumer> mConsumer;
};

/**
 * A CommandSource is obtained from a CommandQueue.  Use it by inserting a
 * command (represented by type Command) using InsertCommand, which also
 * needs all parameters to the command.  They are then serialized and sent
 * to the CommandSink, which must understand the Command+Args combination
 * to execute it.
 */
template <typename Command>
class CommandSource : public BasicSource {
 public:
  explicit CommandSource(UniquePtr<Producer>&& aProducer)
      : BasicSource(std::move(aProducer)) {}

  template <typename... Args>
  PcqStatus InsertCommand(Command aCommand, Args&&... aArgs) {
    return this->mProducer->TryWaitInsert(Nothing() /* wait forever */,
                                          aCommand, aArgs...);
  }

  PcqStatus InsertCommand(Command aCommand) {
    return this->mProducer->TryWaitInsert(Nothing() /* wait forever */,
                                          aCommand);
  }

  template <typename... Args>
  PcqStatus RunCommand(Command aCommand, Args&&... aArgs) {
    return InsertCommand(aCommand, std::forward<Args>(aArgs)...);
  }

  // For IPDL:
  CommandSource() = default;
};

/**
 * A CommandSink is obtained from a CommandQueue.  It executes commands that
 * originated in its CommandSource.  Use this class by calling one of the
 * Process methods, which will autonomously deserialize, dispatch and
 * post-process the execution.  This class handles deserialization -- dispatch
 * and processing are to be provided by a subclass in its implementation of the
 * pure-virtual DispatchCommand method.  DispatchCommand implementations can
 * easily run functions and methods using arguments taken from the command
 * queue by calling the Dispatch methods in this class.
 */
template <typename Command>
class CommandSink : public BasicSink {
 public:
  explicit CommandSink(UniquePtr<Consumer>&& aConsumer)
      : BasicSink(std::move(aConsumer)) {}

  /**
   * Attempts to process the next command in the queue, if one is available.
   */
  CommandResult ProcessOne(const Maybe<TimeDuration>& aTimeout) {
    Command command;
    PcqStatus status = (aTimeout.isNothing() || aTimeout.value())
                           ? this->mConsumer->TryWaitRemove(aTimeout, command)
                           : this->mConsumer->TryRemove(command);

    if (status == PcqStatus::Success) {
      if (DispatchCommand(command)) {
        return CommandResult::Success;
      }
      return CommandResult::Error;
    }

    if (status == PcqStatus::PcqNotReady) {
      return CommandResult::QueueEmpty;
    }

    if (status == PcqStatus::PcqOOMError) {
      ReportOOM();
    }
    return CommandResult::Error;
  }

  CommandResult ProcessOneNow() { return ProcessOne(Some(TimeDuration(0))); }

  /**
   * Drains the queue until the queue is empty or an error occurs, whichever
   * comes first.
   * Returns the result of the last attempt to process a command, which will
   * be either QueueEmpty or Error.
   */
  CommandResult ProcessAll() {
    CommandResult result;
    do {
      result = ProcessOneNow();
    } while (result == CommandResult::Success);
    return result;
  }

  /**
   * Drains the queue until aDuration expires, the queue is empty, or an error
   * occurs, whichever comes first.
   * Returns the result of the last attempt to process a command.
   */
  CommandResult ProcessUpToDuration(TimeDuration aDuration) {
    TimeStamp start = TimeStamp::Now();
    TimeStamp now = start;
    CommandResult result;

    do {
      result = ProcessOne(Some(aDuration - (now - start)));
      now = TimeStamp::Now();
    } while ((result == CommandResult::Success) && ((now - start) < aDuration));
    return result;
  }

  // For IPDL:
  CommandSink() = default;

  // non-void return value, non-const method variant
  template <typename T, typename ReturnType, typename... Args>
  bool DispatchAsyncMethod(T& aObj, ReturnType (T::*aMethod)(Args...)) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!ReadArgs(args)) {
      return false;
    }
    CallMethod(aObj, aMethod, args, std::index_sequence_for<Args...>{});
    return true;
  }

  // non-void return value, const method variant
  template <typename T, typename ReturnType, typename... Args>
  bool DispatchAsyncMethod(const T& aObj,
                           ReturnType (T::*aMethod)(Args...) const) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!ReadArgs(args)) {
      return false;
    }
    CallMethod(aObj, aMethod, args, std::index_sequence_for<Args...>{});
    return true;
  }

  // void return value, non-const method variant
  template <typename T, typename... Args>
  bool DispatchAsyncMethod(T* aObj, void (T::*aMethod)(Args...)) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!ReadArgs(args)) {
      return false;
    }

    CallVoidMethod(aObj, aMethod, args, std::index_sequence_for<Args...>{});
    return true;
  }

  // void return value, const method variant
  template <typename T, typename... Args>
  bool DispatchAsyncMethod(const T* aObj, void (T::*aMethod)(Args...) const) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!ReadArgs(args)) {
      return false;
    }

    CallVoidMethod(aObj, aMethod, args, std::index_sequence_for<Args...>{});
    return true;
  }

 protected:
  /**
   * Implementations will usually be something like a big switch statement
   * that calls one of the Dispatch methods in this class.
   */
  virtual bool DispatchCommand(Command command) = 0;

  /**
   * Implementations can override this to detect out-of-memory during
   * deserialization.
   */
  virtual void ReportOOM() {}

  template <typename... Args, size_t... Indices>
  PcqStatus CallTryRemove(std::tuple<Args...>& aArgs,
                          std::index_sequence<Indices...>) {
    PcqStatus status = mConsumer->TryRemove(std::get<Indices>(aArgs)...);
    // The CommandQueue inserts the command and the args together as an atomic
    // operation.  We already read the command so the args must also be
    // available.
    MOZ_ASSERT(status != PcqStatus::PcqNotReady);
    return status;
  }

  PcqStatus CallTryRemove(std::tuple<>& aArgs,
                          std::make_integer_sequence<size_t, 0>) {
    return PcqStatus::Success;
  }

  template <typename T, typename MethodType, typename... Args,
            size_t... Indices, typename ReturnType>
  std::result_of<MethodType> CallMethod(T& aObj, MethodType aMethod,
                                        std::tuple<Args...>& aArgs,
                                        std::index_sequence<Indices...>) {
    return (aObj.*aMethod)(std::forward<Args>(std::get<Indices>(aArgs))...);
  }

  template <typename T, typename MethodType, typename... Args,
            size_t... Indices>
  void CallVoidMethod(T& aObj, MethodType aMethod, std::tuple<Args...>& aArgs,
                      std::index_sequence<Indices...>) {
    (aObj.*aMethod)(std::forward<Args>(std::get<Indices>(aArgs))...);
  }

  template <typename... Args>
  bool ReadArgs(std::tuple<Args...>& aArgs) {
    PcqStatus status = CallTryRemove(aArgs, std::index_sequence_for<Args...>{});
    return IsSuccess(status);
  }
};

enum SyncResponse : uint8_t { RESPONSE_NAK, RESPONSE_ACK };

/**
 * This is the Source for a SyncCommandSink.  It takes an extra PCQ,
 * the ResponsePcq, and uses it to receive synchronous responses from
 * the sink.  The ResponsePcq is a regular ProducerConsumerQueue,
 * not a CommandQueue.
 */
template <typename Command>
class SyncCommandSource : public CommandSource<Command> {
 public:
  using BaseType = CommandSource<Command>;
  SyncCommandSource(UniquePtr<Producer>&& aProducer,
                    UniquePtr<Consumer>&& aResponseConsumer)
      : CommandSource<Command>(std::move(aProducer)),
        mConsumer(std::move(aResponseConsumer)) {}

  template <typename... Args>
  PcqStatus RunAsyncCommand(Command aCommand, Args&&... aArgs) {
    return this->RunCommand(aCommand, std::forward<Args>(aArgs)...);
  }

  template <typename... Args>
  PcqStatus RunVoidSyncCommand(Command aCommand, Args&&... aArgs) {
    PcqStatus status = RunAsyncCommand(aCommand, std::forward<Args>(aArgs)...);
    return IsSuccess(status) ? this->ReadSyncResponse() : status;
  }

  template <typename ResultType, typename... Args>
  PcqStatus RunSyncCommand(Command aCommand, ResultType& aReturn,
                           Args&&... aArgs) {
    PcqStatus status =
        RunVoidSyncCommand(aCommand, std::forward<Args>(aArgs)...);
    return IsSuccess(status) ? this->ReadResult(aReturn) : status;
  }

  // for IPDL:
  SyncCommandSource() = default;
  friend struct mozilla::ipc::IPDLParamTraits<SyncCommandSource<Command>>;

 protected:
  PcqStatus ReadSyncResponse() {
    SyncResponse response;
    PcqStatus status =
        mConsumer->TryWaitRemove(Nothing() /* wait forever */, response);
    MOZ_ASSERT(status != PcqStatus::PcqNotReady);

    if (IsSuccess(status) && response != RESPONSE_ACK) {
      return PcqStatus::PcqFatalError;
    }
    return status;
  }

  template <typename T>
  PcqStatus ReadResult(T& aResult) {
    PcqStatus status = mConsumer->TryRemove(aResult);
    // The Sink posts the response code and result as an atomic transaction.  We
    // already read the response code so the result must be available.
    MOZ_ASSERT(status != PcqStatus::PcqNotReady);
    return status;
  }

  UniquePtr<Consumer> mConsumer;
};

/**
 * This is the Sink for a SyncCommandSource.  It takes an extra PCQ, the
 * ResponsePcq, and uses it to issue synchronous responses to the client.
 * Subclasses can use the DispatchSync methods in this class in their
 * DispatchCommand implementations.
 * The ResponsePcq is not a CommandQueue.
 */
template <typename Command>
class SyncCommandSink : public CommandSink<Command> {
  using BaseType = CommandSink<Command>;

 public:
  SyncCommandSink(UniquePtr<Consumer>&& aConsumer,
                  UniquePtr<Producer>&& aResponseProducer)
      : CommandSink<Command>(std::move(aConsumer)),
        mProducer(std::move(aResponseProducer)) {}

  // for IPDL:
  SyncCommandSink() = default;
  friend struct mozilla::ipc::IPDLParamTraits<SyncCommandSink<Command>>;

  // Places RESPONSE_ACK and the typed return value, or RESPONSE_NAK, in
  // the response queue,
  // __cdecl/__thiscall non-const method variant.
  template <typename T, typename ReturnType, typename... Args>
  bool DispatchSyncMethod(T& aObj,
                          ReturnType SINK_FCN_CC (T::*aMethod)(Args...)) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!BaseType::ReadArgs(args)) {
      WriteNAK();
      return false;
    }

    ReturnType response = BaseType::CallMethod(
        aObj, aMethod, args, std::index_sequence_for<Args...>{});

    return WriteACK(response);
  }

  // __cdecl/__thiscall const method variant.
  template <typename T, typename ReturnType, typename... Args>
  bool DispatchSyncMethod(const T& aObj,
                          ReturnType SINK_FCN_CC (T::*aMethod)(Args...) const) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!BaseType::ReadArgs(args)) {
      WriteNAK();
      return false;
    }

    ReturnType response = BaseType::CallMethod(
        aObj, aMethod, args, std::index_sequence_for<Args...>{});

    return WriteACK(response);
  }

#if defined(_M_IX86)
  // __stdcall non-const method variant.
  template <typename T, typename ReturnType, typename... Args>
  bool DispatchSyncMethod(T& aObj,
                          ReturnType __stdcall (T::*aMethod)(Args...)) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!BaseType::ReadArgs(args)) {
      WriteNAK();
      return false;
    }

    ReturnType response = BaseType::CallMethod(
        aObj, aMethod, args, std::index_sequence_for<Args...>{});

    return WriteACK(response);
  }

  // __stdcall const method variant.
  template <typename T, typename ReturnType, typename... Args>
  bool DispatchSyncMethod(const T& aObj,
                          ReturnType __stdcall (T::*aMethod)(Args...) const) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!BaseType::ReadArgs(args)) {
      WriteNAK();
      return false;
    }

    ReturnType response = BaseType::CallMethod(
        aObj, aMethod, args, std::index_sequence_for<Args...>{});

    return WriteACK(response);
  }
#endif

  // __cdecl/__thiscall non-const void method variant
  template <typename T, typename... Args>
  bool DispatchSyncMethod(T& aObj, void SINK_FCN_CC (T::*aMethod)(Args...)) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!BaseType::ReadArgs(args)) {
      WriteNAK();
      return false;
    }

    BaseType::CallVoidMethod(aObj, aMethod, args,
                             std::index_sequence_for<Args...>{});
    return WriteACK();
  }

  // __cdecl/__thiscall const void method variant
  template <typename T, typename... Args>
  bool DispatchSyncMethod(const T& aObj,
                          void SINK_FCN_CC (T::*aMethod)(Args...) const) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!BaseType::ReadArgs(args)) {
      WriteNAK();
      return false;
    }

    BaseType::CallVoidMethod(aObj, aMethod, args,
                             std::index_sequence_for<Args...>{});
    return WriteACK();
  }

#if defined(_M_IX86)
  // __stdcall non-const void method variant
  template <typename T, typename... Args>
  bool DispatchSyncMethod(T& aObj, void __stdcall (T::*aMethod)(Args...)) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!BaseType::ReadArgs(args)) {
      WriteNAK();
      return false;
    }

    BaseType::CallVoidMethod(aObj, aMethod, args,
                             std::index_sequence_for<Args...>{});
    return WriteACK();
  }

  // __stdcall const void method variant
  template <typename T, typename... Args>
  bool DispatchSyncMethod(const T& aObj,
                          void __stdcall (T::*aMethod)(Args...) const) {
    std::tuple<typename RemoveCV<std::remove_reference_t<Args>>::Type...> args;
    if (!BaseType::ReadArgs(args)) {
      WriteNAK();
      return false;
    }

    BaseType::CallVoidMethod(aObj, aMethod, args,
                             std::index_sequence_for<Args...>{});
    return WriteACK();
  }
#endif

 protected:
  template <typename... Args>
  bool WriteArgs(const Args&... aArgs) {
    return IsSuccess(mProducer->TryInsert(aArgs...));
  }

  template <typename... Args>
  bool WriteACK(const Args&... aArgs) {
    SyncResponse ack = RESPONSE_ACK;
    return WriteArgs(ack, aArgs...);
  }

  bool WriteNAK() {
    SyncResponse nak = RESPONSE_NAK;
    return WriteArgs(nak);
  }

  UniquePtr<Producer> mProducer;
};

/**
 * Can be used by a sink to find and execute the handler for a given commandId.
 */
template <typename Derived>
struct CommandDispatchDriver {
  /**
   * Find and run the command.
   */
  template <size_t commandId, typename... Args>
  static MOZ_ALWAYS_INLINE bool DispatchCommandHelper(size_t aId,
                                                      Args&... aArgs) {
    if (commandId == aId) {
      return Derived::template Dispatch<commandId>(aArgs...);
    }
    return Derived::template DispatchCommand<commandId + 1>(aId, aArgs...);
  }
};

/**
 * This CommandDispatcher provides helper methods that subclasses can
 * use to dispatch sync/async commands to a method via a CommandSink.
 * See DECLARE_METHOD_DISPATCHER and DEFINE_METHOD_DISPATCHER.
 */
template <typename Derived, typename _SinkType>
struct MethodDispatcher {
  using SinkType = _SinkType;
  template <CommandSyncType syncType>
  struct DispatchMethod;
  /*
    // Specialization for dispatching asynchronous methods
    template <CommandSyncType SyncType>
    struct DispatchMethod {
      template <typename MethodType, typename ObjectType>
      static MOZ_ALWAYS_INLINE bool Run(SinkType& aSink, MethodType mMethod,
                                        ObjectType& aObj) {
        return aSink.DispatchMethod<SyncType>(aObj, mMethod);
      }
    };

    // Specialization for dispatching synchronous methods
    template <>
    struct DispatchMethod<CommandSyncType::SYNC> {
      template <typename MethodType, typename ObjectType>
      static MOZ_ALWAYS_INLINE bool Run(SinkType& aSink, MethodType aMethod,
                                        ObjectType& aObj) {
        return aSink.DispatchSyncMethod(aObj, aMethod);
      }
    };
    */
};

// Declares a MethodDispatcher with the given name and CommandSink type.
// The ObjectType is the type of the object this class will dispatch methods to.
#define DECLARE_METHOD_DISPATCHER(_DISPATCHER, _SINKTYPE, _OBJECTTYPE)         \
  struct _DISPATCHER : public MethodDispatcher<_DISPATCHER, _SINKTYPE> {       \
    using ObjectType = _OBJECTTYPE;                                            \
    template <size_t commandId = 0>                                            \
    static MOZ_ALWAYS_INLINE bool DispatchCommand(size_t aId, SinkType& aSink, \
                                                  ObjectType& aObj) {          \
      MOZ_ASSERT_UNREACHABLE("Unhandled command ID");                          \
      return false;                                                            \
    }                                                                          \
    template <size_t commandId>                                                \
    static MOZ_ALWAYS_INLINE bool Dispatch(SinkType& aSink, ObjectType& aObj); \
    template <size_t commandId>                                                \
    struct MethodInfo;                                                         \
    template <size_t commandId>                                                \
    static constexpr CommandSyncType SyncType();                               \
    template <typename MethodType, MethodType method>                          \
    static constexpr size_t Id();                                              \
  };

// Defines a handler in the given dispatcher for the command with the given
// id.  The handler uses a CommandSink to read parameters, call the
// given method using the given synchronization protocol, and provide
// compile-time lookup of the ID by class method.
#define DEFINE_METHOD_DISPATCHER(_DISPATCHER, _ID, _METHOD, _SYNC)           \
  /*  template <>                                                            \
    bool _DISPATCHER::DispatchCommand<_ID>(size_t aId, SinkType & aSink,     \
                                           ObjectType & aObj) {              \
      return CommandDispatchDriver<_DISPATCHER>::DispatchCommandHelper<_ID>( \
          aId, aSink, aObj);                                                 \
    }                                                                        \
    template <>                                                              \
    bool _DISPATCHER::Dispatch<_ID>(SinkType & aSink, ObjectType & aObj) {   \
      return DispatchMethod<_SYNC>::Run(aSink, &_METHOD, aObj);              \
    } */                                                                     \
  template <>                                                                \
  struct _DISPATCHER::MethodInfo<_ID> {                                      \
    using MethodType = decltype(&_METHOD);                                   \
  };                                                                         \
  template <>                                                                \
  constexpr CommandSyncType _DISPATCHER::SyncType<_ID>() {                   \
    return _SYNC;                                                            \
  }                                                                          \
  template <>                                                                \
  constexpr size_t _DISPATCHER::Id<decltype(&_METHOD), &_METHOD>() {         \
    return _ID;                                                              \
  }

namespace ipc {
template <typename T>
struct IPDLParamTraits;

template <>
struct IPDLParamTraits<mozilla::BasicSource> {
 public:
  typedef mozilla::BasicSource paramType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    MOZ_ASSERT(aParam.mProducer);
    WriteIPDLParam(aMsg, aActor, *aParam.mProducer.get());
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    Producer* producer = new Producer;
    bool ret = ReadIPDLParam(aMsg, aIter, aActor, producer);
    aResult->mProducer.reset(producer);
    return ret;
  }
};

template <>
struct IPDLParamTraits<mozilla::BasicSink> {
 public:
  typedef mozilla::BasicSink paramType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    MOZ_ASSERT(aParam.mConsumer);
    WriteIPDLParam(aMsg, aActor, *aParam.mConsumer.get());
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    Consumer* consumer = new Consumer;
    bool ret = ReadIPDLParam(aMsg, aIter, aActor, consumer);
    aResult->mConsumer.reset(consumer);
    return ret;
  }
};

template <typename Command>
struct IPDLParamTraits<mozilla::CommandSource<Command>>
    : public IPDLParamTraits<mozilla::BasicSource> {
 public:
  typedef mozilla::CommandSource<Command> paramType;
};

template <typename Command>
struct IPDLParamTraits<mozilla::CommandSink<Command>>
    : public IPDLParamTraits<mozilla::BasicSink> {
 public:
  typedef mozilla::CommandSink<Command> paramType;
};

template <typename Command>
struct IPDLParamTraits<mozilla::SyncCommandSource<Command>>
    : public IPDLParamTraits<mozilla::CommandSource<Command>> {
 public:
  typedef mozilla::SyncCommandSource<Command> paramType;
  typedef typename paramType::BaseType paramBaseType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, static_cast<const paramBaseType&>(aParam));
    WriteIPDLParam(aMsg, aActor, aParam.mConsumer);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aParam) {
    bool result =
        ReadIPDLParam(aMsg, aIter, aActor, static_cast<paramBaseType*>(aParam));
    return result && ReadIPDLParam(aMsg, aIter, aActor, &aParam->mConsumer);
  }
};

template <typename Command>
struct IPDLParamTraits<mozilla::SyncCommandSink<Command>>
    : public IPDLParamTraits<mozilla::CommandSink<Command>> {
 public:
  typedef mozilla::SyncCommandSink<Command> paramType;
  typedef typename paramType::BaseType paramBaseType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, static_cast<const paramBaseType&>(aParam));
    WriteIPDLParam(aMsg, aActor, aParam.mProducer);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aParam) {
    bool result =
        ReadIPDLParam(aMsg, aIter, aActor, static_cast<paramBaseType*>(aParam));
    return result && ReadIPDLParam(aMsg, aIter, aActor, &aParam->mProducer);
  }
};

}  // namespace ipc

}  // namespace mozilla

#endif  // WEBGLCOMMANDQUEUE_H_
