/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEventSource_h_
#define MediaEventSource_h_

#include "mozilla/AbstractThread.h"
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"
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
  Dispatch(nsIEventTarget* aTarget, already_AddRefed<nsIRunnable>&& aTask) {
    aTarget->Dispatch(Move(aTask), NS_DISPATCH_NORMAL);
  }
};

template <>
struct EventTarget<AbstractThread> {
  static void
  Dispatch(AbstractThread* aTarget, already_AddRefed<nsIRunnable>&& aTask) {
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
  template <typename T>
  class R : public nsRunnable {
    typedef typename RemoveCV<typename RemoveReference<T>::Type>::Type ArgType;
  public:
    template <typename U>
    R(RevocableToken* aToken, const Function& aFunction, U&& aEvent)
      : mToken(aToken), mFunction(aFunction), mEvent(Forward<U>(aEvent)) {}

    NS_IMETHOD Run() override {
      // Don't call the listener if it is disconnected.
      if (!mToken->IsRevoked()) {
        // Enable move whenever possible since mEvent won't be used anymore.
        mFunction(Move(mEvent));
      }
      return NS_OK;
    }

  private:
    nsRefPtr<RevocableToken> mToken;
    Function mFunction;
    ArgType mEvent;
  };

public:
  ListenerHelper(RevocableToken* aToken, Target* aTarget, const Function& aFunc)
    : mToken(aToken), mTarget(aTarget), mFunction(aFunc) {}

  // |F| takes one argument.
  template <typename F, typename T>
  typename EnableIf<TakeArgs<F>::value, void>::Type
  Dispatch(const F& aFunc, T&& aEvent) {
    nsCOMPtr<nsIRunnable> r = new R<T>(mToken, aFunc, Forward<T>(aEvent));
    EventTarget<Target>::Dispatch(mTarget.get(), r.forget());
  }

  // |F| takes no arguments. Don't bother passing aEvent.
  template <typename F, typename T>
  typename EnableIf<!TakeArgs<F>::value, void>::Type
  Dispatch(const F& aFunc, T&&) {
    const nsRefPtr<RevocableToken>& token = mToken;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction([=] () {
      // Don't call the listener if it is disconnected.
      if (!token->IsRevoked()) {
        aFunc();
      }
    });
    EventTarget<Target>::Dispatch(mTarget.get(), r.forget());
  }

  template <typename T>
  void Dispatch(T&& aEvent) {
    Dispatch(mFunction, Forward<T>(aEvent));
  }

private:
  nsRefPtr<RevocableToken> mToken;
  const nsRefPtr<Target> mTarget;
  Function mFunction;
};

/**
 * Define whether an event data should be copied or moved to the listeners.
 *
 * @Copy Data will always be copied. Each listener gets a copy.
 * @Move Data will always be moved.
 * @Both Data will be moved when possible or copied when necessary.
 */
enum class EventPassMode : int8_t {
  Copy,
  Move,
  Both
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
  const nsRefPtr<RevocableToken> mToken;
};

/**
 * Stored by MediaEventSource to send notifications to the listener.
 * Since virtual methods can not be templated, this class is specialized
 * to provide different Dispatch() overloads depending on EventPassMode.
 */
template <typename ArgType, EventPassMode Mode = EventPassMode::Copy>
class Listener : public ListenerBase {
public:
  virtual ~Listener() {}
  virtual void Dispatch(const ArgType& aEvent) = 0;
};

template <typename ArgType>
class Listener<ArgType, EventPassMode::Both> : public ListenerBase {
public:
  virtual ~Listener() {}
  virtual void Dispatch(const ArgType& aEvent) = 0;
  virtual void Dispatch(ArgType&& aEvent) = 0;
};

template <typename ArgType>
class Listener<ArgType, EventPassMode::Move> : public ListenerBase {
public:
  virtual ~Listener() {}
  virtual void Dispatch(ArgType&& aEvent) = 0;
};

/**
 * Store the registered target thread and function so it knows where and to
 * whom to send the event data.
 */
template <typename Target, typename Function, typename ArgType, EventPassMode>
class ListenerImpl : public Listener<ArgType, EventPassMode::Copy> {
public:
  ListenerImpl(Target* aTarget, const Function& aFunction)
    : mHelper(ListenerBase::Token(), aTarget, aFunction) {}
  void Dispatch(const ArgType& aEvent) override {
    mHelper.Dispatch(aEvent);
  }
private:
  ListenerHelper<Target, Function> mHelper;
};

template <typename Target, typename Function, typename ArgType>
class ListenerImpl<Target, Function, ArgType, EventPassMode::Both>
  : public Listener<ArgType, EventPassMode::Both> {
public:
  ListenerImpl(Target* aTarget, const Function& aFunction)
    : mHelper(ListenerBase::Token(), aTarget, aFunction) {}
  void Dispatch(const ArgType& aEvent) override {
    mHelper.Dispatch(aEvent);
  }
  void Dispatch(ArgType&& aEvent) override {
    mHelper.Dispatch(Move(aEvent));
  }
private:
  ListenerHelper<Target, Function> mHelper;
};

template <typename Target, typename Function, typename ArgType>
class ListenerImpl<Target, Function, ArgType, EventPassMode::Move>
  : public Listener<ArgType, EventPassMode::Move> {
public:
  ListenerImpl(Target* aTarget, const Function& aFunction)
    : mHelper(ListenerBase::Token(), aTarget, aFunction) {}
  void Dispatch(ArgType&& aEvent) override {
    mHelper.Dispatch(Move(aEvent));
  }
private:
  ListenerHelper<Target, Function> mHelper;
};

/**
 * Select EventPassMode based on ListenerMode and if the type is copyable.
 *
 * @Copy Selected when ListenerMode is NonExclusive because each listener
 * must get a copy.
 *
 * @Move Selected when ListenerMode is Exclusive and the type is move-only.
 *
 * @Both Selected when ListenerMode is Exclusive and the type is copyable.
 * The data will be moved when possible and copied when necessary.
 */
template <typename ArgType, ListenerMode Mode>
struct PassModePicker {
  // TODO: pick EventPassMode::Both when we can detect if a type is
  // copy-constructible to allow copy-only types in Exclusive mode.
  static const EventPassMode Value =
    Mode == ListenerMode::NonExclusive ?
    EventPassMode::Copy : EventPassMode::Move;
};

} // namespace detail

template <typename T, ListenerMode> class MediaEventSource;

/**
 * Not thread-safe since this is not meant to be shared and therefore only
 * move constructor is provided. Used to hold the result of
 * MediaEventSource<T>::Connect() and call Disconnect() to disconnect the
 * listener from an event source.
 */
class MediaEventListener {
  template <typename T, ListenerMode>
  friend class MediaEventSource;

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

private:
  // Avoid exposing RevocableToken directly to the client code so that
  // listeners can be disconnected in a controlled manner.
  explicit MediaEventListener(RevocableToken* aToken) : mToken(aToken) {}
  nsRefPtr<RevocableToken> mToken;
};

/**
 * A generic and thread-safe class to implement the observer pattern.
 */
template <typename EventType, ListenerMode Mode = ListenerMode::NonExclusive>
class MediaEventSource {
  static_assert(!IsReference<EventType>::value, "Ref-type not supported!");
  typedef typename detail::EventTypeTraits<EventType>::ArgType ArgType;
  static const detail::EventPassMode PassMode
    = detail::PassModePicker<ArgType, Mode>::Value;
  typedef detail::Listener<ArgType, PassMode> Listener;

  template<typename Target, typename Func>
  using ListenerImpl = detail::ListenerImpl<Target, Func, ArgType, PassMode>;

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

  // |Method| takes one argument.
  template <typename Target, typename This, typename Method>
  typename EnableIf<TakeArgs<Method>::value, MediaEventListener>::Type
  ConnectInternal(Target* aTarget, This* aThis, Method aMethod) {
    detail::RawPtr<This> thiz(aThis);
    auto f = [=] (ArgType&& aEvent) {
      (thiz.get()->*aMethod)(Move(aEvent));
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
  MediaEventSource() : mMutex("MediaEventSource::mMutex") {}

  template <typename T>
  void NotifyInternal(T&& aEvent) {
    MutexAutoLock lock(mMutex);
    for (int32_t i = mListeners.Length() - 1; i >= 0; --i) {
      auto&& l = mListeners[i];
      // Remove disconnected listeners.
      // It is not optimal but is simple and works well.
      if (l->Token()->IsRevoked()) {
        mListeners.RemoveElementAt(i);
        continue;
      }
      l->Dispatch(Forward<T>(aEvent));
    }
  }

private:
  Mutex mMutex;
  nsTArray<UniquePtr<Listener>> mListeners;
};

/**
 * A class to separate the interface of event subject (MediaEventSource)
 * and event publisher. Mostly used as a member variable to publish events
 * to the listeners.
 */
template <typename EventType, ListenerMode Mode = ListenerMode::NonExclusive>
class MediaEventProducer : public MediaEventSource<EventType, Mode> {
public:
  template <typename T>
  void Notify(T&& aEvent) {
    this->NotifyInternal(Forward<T>(aEvent));
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

} // namespace mozilla

#endif //MediaEventSource_h_
