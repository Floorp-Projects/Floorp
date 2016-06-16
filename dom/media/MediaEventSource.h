/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEventSource_h_
#define MediaEventSource_h_

#include "mozilla/AbstractThread.h"
#include "mozilla/Atomics.h"
#include "mozilla/IndexSequence.h"
#include "mozilla/Mutex.h"
#include "mozilla/Tuple.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"

#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

namespace mozilla {

/**
 * A thread-safe tool to communicate "revocation" across threads. It is used to
 * disconnect a listener from the event source to prevent future notifications
 * from coming. Revoke() can be called on any thread. However, it is recommended
 * to be called on the target thread to avoid race condition.
 *
 * RevocableToken is not exposed to the client code directly.
 * Use MediaEventListener below to do the job.
 */
class RevocableToken {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RevocableToken);

public:
  RevocableToken() : mRevoked(false) {}

  void Revoke() {
    mRevoked = true;
  }

  bool IsRevoked() const {
    return mRevoked;
  }

private:
  ~RevocableToken() {}
  Atomic<bool> mRevoked;
};

enum class ListenerMode : int8_t {
  // Allow at most one listener. Move will be used when possible
  // to pass the event data to save copy.
  Exclusive,
  // This is the default. Event data will always be copied when passed
  // to the listeners.
  NonExclusive
};

namespace detail {

/**
 * Define how an event type is passed internally in MediaEventSource and to the
 * listeners. Specialized for the void type to pass a dummy bool instead.
 */
template <typename T>
struct EventTypeTraits {
  typedef T ArgType;
};

template <>
struct EventTypeTraits<void> {
  typedef bool ArgType;
};

/**
 * Test if a method function or lambda accepts one or more arguments.
 */
template <typename T>
class TakeArgsHelper {
  template <typename C> static FalseType test(void(C::*)(), int);
  template <typename C> static FalseType test(void(C::*)() const, int);
  template <typename C> static FalseType test(void(C::*)() volatile, int);
  template <typename C> static FalseType test(void(C::*)() const volatile, int);
  template <typename F> static FalseType test(F&&, decltype(DeclVal<F>()(), 0));
  static TrueType test(...);
public:
  typedef decltype(test(DeclVal<T>(), 0)) Type;
};

template <typename T>
struct TakeArgs : public TakeArgsHelper<T>::Type {};

template <typename T> struct EventTarget;

template <>
struct EventTarget<nsIEventTarget> {
  static void
  Dispatch(nsIEventTarget* aTarget, already_AddRefed<nsIRunnable> aTask) {
    aTarget->Dispatch(Move(aTask), NS_DISPATCH_NORMAL);
  }
};

template <>
struct EventTarget<AbstractThread> {
  static void
  Dispatch(AbstractThread* aTarget, already_AddRefed<nsIRunnable> aTask) {
    aTarget->Dispatch(Move(aTask));
  }
};

/**
 * Encapsulate a raw pointer to be captured by a lambda without causing
 * static-analysis errors.
 */
template <typename T>
class RawPtr {
public:
  explicit RawPtr(T* aPtr) : mPtr(aPtr) {}
  T* get() const { return mPtr; }
private:
  T* const mPtr;
};

/**
 * A helper class to pass event data to the listeners. Optimized to save
 * copy when Move is possible or |Function| takes no arguments.
 */
template<typename Target, typename Function>
class ListenerHelper {
  // Define our custom runnable to minimize copy of the event data.
  // NS_NewRunnableFunction will result in 2 copies of the event data.
  // One is captured by the lambda and the other is the copy of the lambda.
  template <typename... Ts>
  class R : public Runnable {
  public:
    template <typename... Us>
    R(RevocableToken* aToken, const Function& aFunction, Us&&... aEvents)
      : mToken(aToken)
      , mFunction(aFunction)
      , mEvents(Forward<Us>(aEvents)...) {}

    template <typename... Vs, size_t... Is>
    void Invoke(Tuple<Vs...>& aEvents, IndexSequence<Is...>) {
      // Enable move whenever possible since mEvent won't be used anymore.
      mFunction(Move(Get<Is>(aEvents))...);
    }

    NS_IMETHOD Run() override {
      // Don't call the listener if it is disconnected.
      if (!mToken->IsRevoked()) {
        Invoke(mEvents, typename IndexSequenceFor<Ts...>::Type());
      }
      return NS_OK;
    }

  private:
    RefPtr<RevocableToken> mToken;
    Function mFunction;

    template <typename T>
    using ArgType = typename RemoveCV<typename RemoveReference<T>::Type>::Type;
    Tuple<ArgType<Ts>...> mEvents;
  };

public:
  ListenerHelper(RevocableToken* aToken, Target* aTarget, const Function& aFunc)
    : mToken(aToken), mTarget(aTarget), mFunction(aFunc) {}

  // |F| takes one or more arguments.
  template <typename F, typename... Ts>
  typename EnableIf<TakeArgs<F>::value, void>::Type
  DispatchHelper(const F& aFunc, Ts&&... aEvents) {
    nsCOMPtr<nsIRunnable> r =
      new R<Ts...>(mToken, aFunc, Forward<Ts>(aEvents)...);
    EventTarget<Target>::Dispatch(mTarget.get(), r.forget());
  }

  // |F| takes no arguments. Don't bother passing aEvent.
  template <typename F, typename... Ts>
  typename EnableIf<!TakeArgs<F>::value, void>::Type
  DispatchHelper(const F& aFunc, Ts&&...) {
    const RefPtr<RevocableToken>& token = mToken;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
      // Don't call the listener if it is disconnected.
      if (!token->IsRevoked()) {
        aFunc();
      }
    });
    EventTarget<Target>::Dispatch(mTarget.get(), r.forget());
  }

  template <typename... Ts>
  void Dispatch(Ts&&... aEvents) {
    DispatchHelper(mFunction, Forward<Ts>(aEvents)...);
  }

private:
  RefPtr<RevocableToken> mToken;
  const RefPtr<Target> mTarget;
  Function mFunction;
};

/**
 * Define whether an event data should be copied or moved to the listeners.
 *
 * @Copy Data will always be copied. Each listener gets a copy.
 * @Move Data will always be moved.
 */
enum class EventPassMode : int8_t {
  Copy,
  Move
};

class ListenerBase {
public:
  ListenerBase() : mToken(new RevocableToken()) {}
  ~ListenerBase() {
    MOZ_ASSERT(Token()->IsRevoked(), "Must disconnect the listener.");
  }
  RevocableToken* Token() const {
    return mToken;
  }
private:
  const RefPtr<RevocableToken> mToken;
};

/**
 * Stored by MediaEventSource to send notifications to the listener.
 * Since virtual methods can not be templated, this class is specialized
 * to provide different Dispatch() overloads depending on EventPassMode.
 */
template <EventPassMode Mode, typename... As>
class Listener : public ListenerBase {
public:
  virtual ~Listener() {}
  virtual void Dispatch(const As&... aEvents) = 0;
};

template <typename... As>
class Listener<EventPassMode::Move, As...> : public ListenerBase {
public:
  virtual ~Listener() {}
  virtual void Dispatch(As... aEvents) = 0;
};

/**
 * Store the registered target thread and function so it knows where and to
 * whom to send the event data.
 */
template <typename Target, typename Function, EventPassMode, typename... As>
class ListenerImpl : public Listener<EventPassMode::Copy, As...> {
public:
  ListenerImpl(Target* aTarget, const Function& aFunction)
    : mHelper(ListenerBase::Token(), aTarget, aFunction) {}
  void Dispatch(const As&... aEvents) override {
    mHelper.Dispatch(aEvents...);
  }
private:
  ListenerHelper<Target, Function> mHelper;
};

template <typename Target, typename Function, typename... As>
class ListenerImpl<Target, Function, EventPassMode::Move, As...>
  : public Listener<EventPassMode::Move, As...> {
public:
  ListenerImpl(Target* aTarget, const Function& aFunction)
    : mHelper(ListenerBase::Token(), aTarget, aFunction) {}
  void Dispatch(As... aEvents) override {
    mHelper.Dispatch(Move(aEvents)...);
  }
private:
  ListenerHelper<Target, Function> mHelper;
};

/**
 * Select EventPassMode based on ListenerMode.
 *
 * @Copy Selected when ListenerMode is NonExclusive because each listener
 * must get a copy.
 *
 * @Move Selected when ListenerMode is Exclusive. All types passed to
 * MediaEventProducer::Notify() must be movable.
 */
template <ListenerMode Mode>
struct PassModePicker {
  static const EventPassMode Value =
    Mode == ListenerMode::NonExclusive ?
    EventPassMode::Copy : EventPassMode::Move;
};

/**
 * Return true if any type is a reference type.
 */
template <typename Head, typename... Tails>
struct IsAnyReference {
  static const bool value = IsReference<Head>::value ||
                            IsAnyReference<Tails...>::value;
};

template <typename T>
struct IsAnyReference<T> {
  static const bool value = IsReference<T>::value;
};

} // namespace detail

template <ListenerMode, typename... Ts> class MediaEventSourceImpl;

/**
 * Not thread-safe since this is not meant to be shared and therefore only
 * move constructor is provided. Used to hold the result of
 * MediaEventSource<T>::Connect() and call Disconnect() to disconnect the
 * listener from an event source.
 */
class MediaEventListener {
  template <ListenerMode, typename... Ts>
  friend class MediaEventSourceImpl;

public:
  MediaEventListener() {}

  MediaEventListener(MediaEventListener&& aOther)
    : mToken(Move(aOther.mToken)) {}

  MediaEventListener& operator=(MediaEventListener&& aOther) {
    MOZ_ASSERT(!mToken, "Must disconnect the listener.");
    mToken = Move(aOther.mToken);
    return *this;
  }

  ~MediaEventListener() {
    MOZ_ASSERT(!mToken, "Must disconnect the listener.");
  }

  void Disconnect() {
    mToken->Revoke();
    mToken = nullptr;
  }

  void DisconnectIfExists() {
    if (mToken) {
      Disconnect();
    }
  }

private:
  // Avoid exposing RevocableToken directly to the client code so that
  // listeners can be disconnected in a controlled manner.
  explicit MediaEventListener(RevocableToken* aToken) : mToken(aToken) {}
  RefPtr<RevocableToken> mToken;
};

/**
 * A generic and thread-safe class to implement the observer pattern.
 */
template <ListenerMode Mode, typename... Es>
class MediaEventSourceImpl {
  static_assert(!detail::IsAnyReference<Es...>::value,
                "Ref-type not supported!");

  template <typename T>
  using ArgType = typename detail::EventTypeTraits<T>::ArgType;

  static const detail::EventPassMode PassMode =
    detail::PassModePicker<Mode>::Value;

  typedef detail::Listener<PassMode, ArgType<Es>...> Listener;

  template<typename Target, typename Func>
  using ListenerImpl =
    detail::ListenerImpl<Target, Func, PassMode, ArgType<Es>...>;

  template <typename Method>
  using TakeArgs = detail::TakeArgs<Method>;

  template<typename Target, typename Function>
  MediaEventListener
  ConnectInternal(Target* aTarget, const Function& aFunction) {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(Mode == ListenerMode::NonExclusive || mListeners.IsEmpty());
    auto l = mListeners.AppendElement();
    l->reset(new ListenerImpl<Target, Function>(aTarget, aFunction));
    return MediaEventListener((*l)->Token());
  }

  // |Method| takes one or more arguments.
  template <typename Target, typename This, typename Method>
  typename EnableIf<TakeArgs<Method>::value, MediaEventListener>::Type
  ConnectInternal(Target* aTarget, This* aThis, Method aMethod) {
    detail::RawPtr<This> thiz(aThis);
    auto f = [=] (ArgType<Es>&&... aEvents) {
      (thiz.get()->*aMethod)(Move(aEvents)...);
    };
    return ConnectInternal(aTarget, f);
  }

  // |Method| takes no arguments. Don't bother passing the event data.
  template <typename Target, typename This, typename Method>
  typename EnableIf<!TakeArgs<Method>::value, MediaEventListener>::Type
  ConnectInternal(Target* aTarget, This* aThis, Method aMethod) {
    detail::RawPtr<This> thiz(aThis);
    auto f = [=] () {
      (thiz.get()->*aMethod)();
    };
    return ConnectInternal(aTarget, f);
  }

public:
  /**
   * Register a function to receive notifications from the event source.
   *
   * @param aTarget The target thread on which the function will run.
   * @param aFunction A function to be called on the target thread. The function
   *                  parameter must be convertible from |EventType|.
   * @return An object used to disconnect from the event source.
   */
  template<typename Function>
  MediaEventListener
  Connect(AbstractThread* aTarget, const Function& aFunction) {
    return ConnectInternal(aTarget, aFunction);
  }

  template<typename Function>
  MediaEventListener
  Connect(nsIEventTarget* aTarget, const Function& aFunction) {
    return ConnectInternal(aTarget, aFunction);
  }

  /**
   * As above.
   *
   * Note we deliberately keep a weak reference to |aThis| in order not to
   * change its lifetime. This is because notifications are dispatched
   * asynchronously and removing a listener doesn't always break the reference
   * cycle for the pending event could still hold a reference to |aThis|.
   *
   * The caller must call MediaEventListener::Disconnect() to avoid dangling
   * pointers.
   */
  template <typename This, typename Method>
  MediaEventListener
  Connect(AbstractThread* aTarget, This* aThis, Method aMethod) {
    return ConnectInternal(aTarget, aThis, aMethod);
  }

  template <typename This, typename Method>
  MediaEventListener
  Connect(nsIEventTarget* aTarget, This* aThis, Method aMethod) {
    return ConnectInternal(aTarget, aThis, aMethod);
  }

protected:
  MediaEventSourceImpl() : mMutex("MediaEventSourceImpl::mMutex") {}

  template <typename... Ts>
  void NotifyInternal(Ts&&... aEvents) {
    MutexAutoLock lock(mMutex);
    for (int32_t i = mListeners.Length() - 1; i >= 0; --i) {
      auto&& l = mListeners[i];
      // Remove disconnected listeners.
      // It is not optimal but is simple and works well.
      if (l->Token()->IsRevoked()) {
        mListeners.RemoveElementAt(i);
        continue;
      }
      l->Dispatch(Forward<Ts>(aEvents)...);
    }
  }

private:
  Mutex mMutex;
  nsTArray<UniquePtr<Listener>> mListeners;
};

template <typename... Es>
using MediaEventSource =
  MediaEventSourceImpl<ListenerMode::NonExclusive, Es...>;

template <typename... Es>
using MediaEventSourceExc =
  MediaEventSourceImpl<ListenerMode::Exclusive, Es...>;

/**
 * A class to separate the interface of event subject (MediaEventSource)
 * and event publisher. Mostly used as a member variable to publish events
 * to the listeners.
 */
template <typename... Es>
class MediaEventProducer : public MediaEventSource<Es...> {
public:
  template <typename... Ts>
  void Notify(Ts&&... aEvents) {
    this->NotifyInternal(Forward<Ts>(aEvents)...);
  }
};

/**
 * Specialization for void type. A dummy bool is passed to NotifyInternal
 * since there is no way to pass a void value.
 */
template <>
class MediaEventProducer<void> : public MediaEventSource<void> {
public:
  void Notify() {
    this->NotifyInternal(true /* dummy */);
  }
};

/**
 * A producer with Exclusive mode.
 */
template <typename... Es>
class MediaEventProducerExc : public MediaEventSourceExc<Es...> {
public:
  template <typename... Ts>
  void Notify(Ts&&... aEvents) {
    this->NotifyInternal(Forward<Ts>(aEvents)...);
  }
};

} // namespace mozilla

#endif //MediaEventSource_h_
