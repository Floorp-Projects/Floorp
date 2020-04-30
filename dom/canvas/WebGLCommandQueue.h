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
using mozilla::webgl::QueueStatus;

enum CommandResult { kSuccess, kTimeExpired, kQueueEmpty, kError };

enum CommandSyncType { ASYNC, SYNC };

/**
 * A CommandSource is obtained from a CommandQueue.  Use it by inserting a
 * command (represented by type Command) using InsertCommand, which also
 * needs all parameters to the command.  They are then serialized and sent
 * to the CommandSink, which must understand the Command+Args combination
 * to execute it.
 */
template <typename Command, typename _Source>
class CommandSource {
  using Source = _Source;

 public:
  explicit CommandSource(UniquePtr<Source>&& aSource)
      : mSource(std::move(aSource)) {
    MOZ_ASSERT(mSource);
  }

  template <typename... Args>
  QueueStatus InsertCommand(Command aCommand, Args&&... aArgs) {
    return this->mSource->TryWaitInsert(Nothing() /* wait forever */, aCommand,
                                        aArgs...);
  }

  QueueStatus InsertCommand(Command aCommand) {
    return this->mSource->TryWaitInsert(Nothing() /* wait forever */, aCommand);
  }

  template <typename... Args>
  QueueStatus RunCommand(Command aCommand, Args&&... aArgs) {
    return InsertCommand(aCommand, std::forward<Args>(aArgs)...);
  }

  // For IPDL:
  CommandSource() = default;

 protected:
  friend struct IPDLParamTraits<mozilla::CommandSource<Command, Source>>;

  UniquePtr<Source> mSource;
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
template <typename Command, typename _Sink>
class CommandSink {
  using Sink = _Sink;

 public:
  explicit CommandSink(UniquePtr<Sink>&& aSink) : mSink(std::move(aSink)) {
    MOZ_ASSERT(mSink);
  }

  /**
   * Attempts to process the next command in the queue, if one is available.
   */
  CommandResult ProcessOne(const Maybe<TimeDuration>& aTimeout) {
    Command command;
    QueueStatus status = (aTimeout.isNothing() || aTimeout.value())
                             ? this->mSink->TryWaitRemove(aTimeout, command)
                             : this->mSink->TryRemove(command);

    if (status == QueueStatus::kSuccess) {
      if (DispatchCommand(command)) {
        return CommandResult::kSuccess;
      }
      return CommandResult::kError;
    }

    if (status == QueueStatus::kNotReady) {
      return CommandResult::kQueueEmpty;
    }

    if (status == QueueStatus::kOOMError) {
      ReportOOM();
    }
    return CommandResult::kError;
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
    } while (result == CommandResult::kSuccess);
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
    } while ((result == CommandResult::kSuccess) &&
             ((now - start) < aDuration));
    return result;
  }

  // For IPDL:
  CommandSink() = default;

  // non-void return value, non-const method variant
  template <typename T, typename ReturnType, typename... Args>
  bool DispatchAsyncMethod(T& aObj, ReturnType (T::*aMethod)(Args...)) {
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
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
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
    if (!ReadArgs(args)) {
      return false;
    }
    CallMethod(aObj, aMethod, args, std::index_sequence_for<Args...>{});
    return true;
  }

  // void return value, non-const method variant
  template <typename T, typename... Args>
  bool DispatchAsyncMethod(T* aObj, void (T::*aMethod)(Args...)) {
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
    if (!ReadArgs(args)) {
      return false;
    }

    CallVoidMethod(aObj, aMethod, args, std::index_sequence_for<Args...>{});
    return true;
  }

  // void return value, const method variant
  template <typename T, typename... Args>
  bool DispatchAsyncMethod(const T* aObj, void (T::*aMethod)(Args...) const) {
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
    if (!ReadArgs(args)) {
      return false;
    }

    CallVoidMethod(aObj, aMethod, args, std::index_sequence_for<Args...>{});
    return true;
  }

 protected:
  friend struct IPDLParamTraits<mozilla::CommandSink<Command, Sink>>;

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
  QueueStatus CallTryRemove(std::tuple<Args...>& aArgs,
                            std::index_sequence<Indices...>) {
    QueueStatus status = mSink->TryRemove(std::get<Indices>(aArgs)...);
    // The CommandQueue inserts the command and the args together as an atomic
    // operation.  We already read the command so the args must also be
    // available.
    MOZ_ASSERT(status != QueueStatus::kNotReady);
    return status;
  }

  QueueStatus CallTryRemove(std::tuple<>& aArgs,
                            std::make_integer_sequence<size_t, 0>) {
    return QueueStatus::kSuccess;
  }

  template <typename T, typename MethodType, typename... Args,
            size_t... Indices,
            typename ReturnType =
                typename mozilla::FunctionTypeTraits<MethodType>::ReturnType>
  ReturnType CallMethod(T& aObj, MethodType aMethod, std::tuple<Args...>& aArgs,
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
    QueueStatus status =
        CallTryRemove(aArgs, std::index_sequence_for<Args...>{});
    return IsSuccess(status);
  }

  UniquePtr<Sink> mSink;
};

enum SyncResponse : uint8_t { RESPONSE_NAK, RESPONSE_ACK };

/**
 * This is the Source for a SyncCommandSink.  It takes an extra queue,
 * the ResponseQueue, and uses it to receive synchronous responses from
 * the sink.  The ResponseQueue is a regular queue, not a CommandQueue.
 */
template <typename Command, typename _Source, typename _ResponseQueue>
class SyncCommandSource : public CommandSource<Command, _Source> {
 public:
  using BaseType = CommandSource<Command, _Source>;
  using Source = _Source;
  using ResponseQueue = _ResponseQueue;
  using ResponseSink = typename ResponseQueue::Consumer;

  SyncCommandSource(UniquePtr<Source>&& aSource,
                    UniquePtr<ResponseSink>&& aResponseSink)
      : CommandSource<Command, Source>(std::move(aSource)),
        mResponseSink(std::move(aResponseSink)) {}

  template <typename... Args>
  QueueStatus RunAsyncCommand(Command aCommand, Args&&... aArgs) {
    return this->RunCommand(aCommand, std::forward<Args>(aArgs)...);
  }

  template <typename... Args>
  QueueStatus RunVoidSyncCommand(Command aCommand, Args&&... aArgs) {
    QueueStatus status =
        RunAsyncCommand(aCommand, std::forward<Args>(aArgs)...);
    return IsSuccess(status) ? this->ReadSyncResponse() : status;
  }

  template <typename ResultType, typename... Args>
  QueueStatus RunSyncCommand(Command aCommand, ResultType& aReturn,
                             Args&&... aArgs) {
    QueueStatus status =
        RunVoidSyncCommand(aCommand, std::forward<Args>(aArgs)...);
    return IsSuccess(status) ? this->ReadResult(aReturn) : status;
  }

  // for IPDL:
  SyncCommandSource() = default;
  friend struct mozilla::ipc::IPDLParamTraits<
      SyncCommandSource<Command, Source, ResponseQueue>>;

 protected:
  QueueStatus ReadSyncResponse() {
    SyncResponse response;
    QueueStatus status =
        mResponseSink->TryWaitRemove(Nothing() /* wait forever */, response);
    MOZ_ASSERT(status != QueueStatus::kNotReady);

    if (IsSuccess(status) && response != RESPONSE_ACK) {
      return QueueStatus::kFatalError;
    }
    return status;
  }

  template <typename T>
  QueueStatus ReadResult(T& aResult) {
    QueueStatus status = mResponseSink->TryRemove(aResult);
    // The Sink posts the response code and result as an atomic transaction.  We
    // already read the response code so the result must be available.
    MOZ_ASSERT(status != QueueStatus::kNotReady);
    return status;
  }

  UniquePtr<ResponseSink> mResponseSink;
};

/**
 * This is the Sink for a SyncCommandSource.  It takes an extra queue, the
 * ResponseQueue, and uses it to issue synchronous responses to the client.
 * Subclasses can use the DispatchSync methods in this class in their
 * DispatchCommand implementations.
 * The ResponseQueue is not a CommandQueue.
 */
template <typename Command, typename _Sink, typename _ResponseQueue>
class SyncCommandSink : public CommandSink<Command, _Sink> {
  using BaseType = CommandSink<Command, _Sink>;
  using ResponseQueue = _ResponseQueue;
  using Sink = _Sink;
  using ResponseSource = typename ResponseQueue::Producer;

 public:
  SyncCommandSink(UniquePtr<Sink>&& aSink,
                  UniquePtr<ResponseSource>&& aResponseSource)
      : CommandSink<Command, Sink>(std::move(aSink)),
        mResponseSource(std::move(aResponseSource)) {
    MOZ_ASSERT(mResponseSource);
  }

  // for IPDL:
  SyncCommandSink() = default;
  friend struct mozilla::ipc::IPDLParamTraits<
      SyncCommandSink<Command, Sink, ResponseQueue>>;

  // Places RESPONSE_ACK and the typed return value, or RESPONSE_NAK, in
  // the response queue,
  // __cdecl/__thiscall non-const method variant.
  template <typename T, typename ReturnType, typename... Args>
  bool DispatchSyncMethod(T& aObj,
                          ReturnType SINK_FCN_CC (T::*aMethod)(Args...)) {
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
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
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
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
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
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
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
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
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
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
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
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
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
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
    std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...> args;
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
    return IsSuccess(mResponseSource->TryInsert(aArgs...));
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

  UniquePtr<ResponseSource> mResponseSource;
};

// The MethodDispatcher setup uses a CommandSink to read parameters, call the
// given method using the given synchronization protocol, and provide
// compile-time lookup of the ID by class method.
// To use this system, first define a dispatcher subclass of
// EmptyMethodDispatcher.  This class must be parameterized by command ID.
//
// Example:
// template <size_t id=0> class MyDispatcher
//    : public EmptyMethodDispatcher<MyDispatcher> {};
//
// Then, for each command handled, specialize this to subclass MethodDispatcher.
// The subclass must define the Method.  It may optionally define isSync for
// synchronous methods.
//
// Example:
// template <>
// class MyDispatcher<0>
//    : public MethodDispatcher<MyDispatcher, 0,
//        decltype(&MyClass::MyMethod), MyClass::MyMethod,
//        CommandSyncType::ASYNC> {};
//
// The method may then be called from the source and run on the sink.
//
// Example:
// int result = Run<MyClass::MyMethod>(param1, std::move(param2));

template <template <size_t> typename Derived>
class EmptyMethodDispatcher {
 public:
  template <typename SinkType, typename ObjectType>
  static MOZ_ALWAYS_INLINE bool DispatchCommand(size_t aId, SinkType& aSink,
                                                ObjectType& aObj) {
    MOZ_CRASH("Illegal ID in DispatchCommand");
  }
  static MOZ_ALWAYS_INLINE CommandSyncType SyncType(size_t aId) {
    MOZ_CRASH("Illegal ID in SyncType");
  }
};

// Derived type must be parameterized by the ID.
template <template <size_t> typename Derived, size_t id, typename MethodType,
          MethodType method, CommandSyncType syncType>
class MethodDispatcher {
  using DerivedType = Derived<id>;
  using NextDispatcher = Derived<id + 1>;

 public:
  template <typename SinkType, typename ObjectType>
  static MOZ_ALWAYS_INLINE bool DispatchCommand(size_t aId, SinkType& aSink,
                                                ObjectType& aObj) {
    if (aId == id) {
      return (syncType == CommandSyncType::ASYNC)
                 ? aSink.DispatchAsyncMethod(aObj, Method())
                 : aSink.DispatchSyncMethod(aObj, Method());
    }
    return NextDispatcher::DispatchCommand(aId, aSink, aObj);
  }

  static MOZ_ALWAYS_INLINE CommandSyncType SyncType(size_t aId) {
    return (aId == id) ? syncType : NextDispatcher::SyncType(aId);
  }

  static constexpr CommandSyncType SyncType() { return syncType; }
  static constexpr size_t Id() { return id; }
  static constexpr MethodType Method() { return method; }
};

namespace ipc {
template <typename T>
struct IPDLParamTraits;

template <typename Command, typename Source>
struct IPDLParamTraits<mozilla::CommandSource<Command, Source>> {
 public:
  typedef mozilla::CommandSource<Command, Source> paramType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mSource);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    return ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSource);
  }
};

template <typename Command, typename Sink>
struct IPDLParamTraits<mozilla::CommandSink<Command, Sink>> {
 public:
  typedef mozilla::CommandSink<Command, Sink> paramType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mSink);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    return ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSink);
  }
};

template <typename Command, typename Source, typename ResponseQueue>
struct IPDLParamTraits<
    mozilla::SyncCommandSource<Command, Source, ResponseQueue>>
    : public IPDLParamTraits<mozilla::CommandSource<Command, Source>> {
 public:
  typedef mozilla::SyncCommandSource<Command, Source, ResponseQueue> paramType;
  typedef typename paramType::BaseType paramBaseType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, static_cast<const paramBaseType&>(aParam));
    WriteIPDLParam(aMsg, aActor, aParam.mResponseSink);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aParam) {
    bool result =
        ReadIPDLParam(aMsg, aIter, aActor, static_cast<paramBaseType*>(aParam));
    return result && ReadIPDLParam(aMsg, aIter, aActor, &aParam->mResponseSink);
  }
};

template <typename Command, typename Sink, typename ResponseQueue>
struct IPDLParamTraits<mozilla::SyncCommandSink<Command, Sink, ResponseQueue>>
    : public IPDLParamTraits<mozilla::CommandSink<Command, Sink>> {
 public:
  typedef mozilla::SyncCommandSink<Command, Sink, ResponseQueue> paramType;
  typedef typename paramType::BaseType paramBaseType;

  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, static_cast<const paramBaseType&>(aParam));
    WriteIPDLParam(aMsg, aActor, aParam.mResponseSource);
  }

  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aParam) {
    bool result =
        ReadIPDLParam(aMsg, aIter, aActor, static_cast<paramBaseType*>(aParam));
    return result &&
           ReadIPDLParam(aMsg, aIter, aActor, &aParam->mResponseSource);
  }
};

}  // namespace ipc

}  // namespace mozilla

#endif  // WEBGLCOMMANDQUEUE_H_
