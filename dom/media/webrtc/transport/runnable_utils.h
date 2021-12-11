/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef runnable_utils_h__
#define runnable_utils_h__

#include <utility>

#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"
#include <functional>
#include <tuple>
#include <type_traits>

// Abstract base class for all of our templates
namespace mozilla {

namespace detail {

enum RunnableResult { NoResult, ReturnsResult };

static inline nsresult RunOnThreadInternal(nsIEventTarget* thread,
                                           nsIRunnable* runnable,
                                           uint32_t flags) {
  return thread->Dispatch(runnable, flags);
}

template <RunnableResult result>
class runnable_args_base : public Runnable {
 public:
  runnable_args_base() : Runnable("media-runnable_args_base") {}

  NS_IMETHOD Run() final {
    MOZ_ASSERT(!mHasRun, "Can only be run once");

    RunInternal();
#ifdef DEBUG
    mHasRun = true;
#endif

    return NS_OK;
  }

 protected:
  virtual void RunInternal() = 0;
#ifdef DEBUG
  bool mHasRun = false;
#endif
};

}  // namespace detail

template <typename FunType, typename... Args>
class runnable_args_func : public detail::runnable_args_base<detail::NoResult> {
 public:
  // |explicit| to pacify static analysis when there are no |args|.
  template <typename... Arguments>
  explicit runnable_args_func(FunType f, Arguments&&... args)
      : mFunc(f), mArgs(std::forward<Arguments>(args)...) {}

 protected:
  void RunInternal() override {
    std::apply(std::move(mFunc), std::move(mArgs));
  }

 private:
  FunType mFunc;
  std::tuple<Args...> mArgs;
};

template <typename FunType, typename... Args>
runnable_args_func<FunType, std::decay_t<Args>...>* WrapRunnableNM(
    FunType f, Args&&... args) {
  return new runnable_args_func<FunType, std::decay_t<Args>...>(
      f, std::forward<Args>(args)...);
}

template <typename Ret, typename FunType, typename... Args>
class runnable_args_func_ret
    : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  template <typename... Arguments>
  runnable_args_func_ret(Ret* ret, FunType f, Arguments&&... args)
      : mReturn(ret), mFunc(f), mArgs(std::forward<Arguments>(args)...) {}

 protected:
  void RunInternal() override {
    *mReturn = std::apply(std::move(mFunc), std::move(mArgs));
  }

 private:
  Ret* mReturn;
  FunType mFunc;
  std::tuple<Args...> mArgs;
};

template <typename R, typename FunType, typename... Args>
runnable_args_func_ret<R, FunType, std::decay_t<Args>...>* WrapRunnableNMRet(
    R* ret, FunType f, Args&&... args) {
  return new runnable_args_func_ret<R, FunType, std::decay_t<Args>...>(
      ret, f, std::forward<Args>(args)...);
}

template <typename Class, typename M, typename... Args>
class runnable_args_memfn
    : public detail::runnable_args_base<detail::NoResult> {
 public:
  template <typename... Arguments>
  runnable_args_memfn(Class&& obj, M method, Arguments&&... args)
      : mObj(std::forward<Class>(obj)),
        mMethod(method),
        mArgs(std::forward<Arguments>(args)...) {}

 protected:
  void RunInternal() override {
    std::apply(std::mem_fn(mMethod),
               std::tuple_cat(std::tie(mObj), std::move(mArgs)));
  }

 private:
  // For holders such as RefPtr and UniquePtr make sure concrete copy is held
  // rather than a potential dangling reference.
  std::decay_t<Class> mObj;
  M mMethod;
  std::tuple<Args...> mArgs;
};

template <typename Class, typename M, typename... Args>
runnable_args_memfn<Class, M, std::decay_t<Args>...>* WrapRunnable(
    Class&& obj, M method, Args&&... args) {
  return new runnable_args_memfn<Class, M, std::decay_t<Args>...>(
      std::forward<Class>(obj), method, std::forward<Args>(args)...);
}

template <typename Ret, typename Class, typename M, typename... Args>
class runnable_args_memfn_ret
    : public detail::runnable_args_base<detail::ReturnsResult> {
 public:
  template <typename... Arguments>
  runnable_args_memfn_ret(Ret* ret, Class&& obj, M method, Arguments... args)
      : mReturn(ret),
        mObj(std::forward<Class>(obj)),
        mMethod(method),
        mArgs(std::forward<Arguments>(args)...) {}

 protected:
  void RunInternal() override {
    *mReturn = std::apply(std::mem_fn(mMethod),
                          std::tuple_cat(std::tie(mObj), std::move(mArgs)));
  }

 private:
  Ret* mReturn;
  // For holders such as RefPtr and UniquePtr make sure concrete copy is held
  // rather than a potential dangling reference.
  std::decay_t<Class> mObj;
  M mMethod;
  std::tuple<Args...> mArgs;
};

template <typename R, typename Class, typename M, typename... Args>
runnable_args_memfn_ret<R, Class, M, std::decay_t<Args>...>* WrapRunnableRet(
    R* ret, Class&& obj, M method, Args&&... args) {
  return new runnable_args_memfn_ret<R, Class, M, std::decay_t<Args>...>(
      ret, std::forward<Class>(obj), method, std::forward<Args>(args)...);
}

static inline nsresult RUN_ON_THREAD(
    nsIEventTarget* thread,
    detail::runnable_args_base<detail::NoResult>* runnable, uint32_t flags) {
  return detail::RunOnThreadInternal(
      thread, static_cast<nsIRunnable*>(runnable), flags);
}

static inline nsresult RUN_ON_THREAD(
    nsIEventTarget* thread,
    detail::runnable_args_base<detail::ReturnsResult>* runnable) {
  return detail::RunOnThreadInternal(
      thread, static_cast<nsIRunnable*>(runnable), NS_DISPATCH_SYNC);
}

#ifdef DEBUG
#  define ASSERT_ON_THREAD(t)           \
    do {                                \
      if (t) {                          \
        bool on;                        \
        nsresult rv;                    \
        rv = t->IsOnCurrentThread(&on); \
        MOZ_ASSERT(NS_SUCCEEDED(rv));   \
        MOZ_ASSERT(on);                 \
      }                                 \
    } while (0)
#else
#  define ASSERT_ON_THREAD(t)
#endif

template <class T>
class DispatchedRelease : public detail::runnable_args_base<detail::NoResult> {
 public:
  explicit DispatchedRelease(already_AddRefed<T>& ref) : ref_(ref) {}

 protected:
  void RunInternal() override { ref_ = nullptr; }

 private:
  RefPtr<T> ref_;
};

template <typename T>
DispatchedRelease<T>* WrapRelease(already_AddRefed<T>&& ref) {
  return new DispatchedRelease<T>(ref);
}

} /* namespace mozilla */

#endif
