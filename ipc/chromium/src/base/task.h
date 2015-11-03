/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_H_
#define BASE_TASK_H_

#include "base/non_thread_safe.h"
#include "base/revocable_store.h"
#include "base/tracked.h"
#include "base/tuple.h"
#include "mozilla/IndexSequence.h"
#include "mozilla/Tuple.h"

// Helper functions so that we can call a function a pass it arguments that come
// from a Tuple.

namespace details {

// Call the given method on the given object. Arguments are passed by move
// semantics from the given tuple. If the tuple has length N, the sequence must
// be IndexSequence<0, 1, ..., N-1>.
template<size_t... Indices, class ObjT, class Method, typename... Args>
void CallMethod(mozilla::IndexSequence<Indices...>, ObjT* obj, Method method,
                mozilla::Tuple<Args...>& arg)
{
  (obj->*method)(mozilla::Move(mozilla::Get<Indices>(arg))...);
}

// Same as above, but call a function.
template<size_t... Indices, typename Function, typename... Args>
void CallFunction(mozilla::IndexSequence<Indices...>, Function function,
                  mozilla::Tuple<Args...>& arg)
{
  (*function)(mozilla::Move(mozilla::Get<Indices>(arg))...);
}

} // namespace details

// Call a method on the given object. Arguments are passed by move semantics
// from the given tuple.
template<class ObjT, class Method, typename... Args>
void DispatchTupleToMethod(ObjT* obj, Method method, mozilla::Tuple<Args...>& arg)
{
  details::CallMethod(typename mozilla::IndexSequenceFor<Args...>::Type(),
                      obj, method, arg);
}

// Same as above, but call a function.
template<typename Function, typename... Args>
void DispatchTupleToFunction(Function function, mozilla::Tuple<Args...>& arg)
{
  details::CallFunction(typename mozilla::IndexSequenceFor<Args...>::Type(),
                        function, arg);
}

// Task ------------------------------------------------------------------------
//
// A task is a generic runnable thingy, usually used for running code on a
// different thread or for scheduling future tasks off of the message loop.

class Task : public tracked_objects::Tracked {
 public:
  Task() {}
  virtual  B2G_ACL_EXPORT ~Task() {}

  // Tasks are automatically deleted after Run is called.
  virtual void Run() = 0;
};

class CancelableTask : public Task {
 public:
  // Not all tasks support cancellation.
  virtual void Cancel() = 0;
};

// Scoped Factories ------------------------------------------------------------
//
// These scoped factory objects can be used by non-refcounted objects to safely
// place tasks in a message loop.  Each factory guarantees that the tasks it
// produces will not run after the factory is destroyed.  Commonly, factories
// are declared as class members, so the class' tasks will automatically cancel
// when the class instance is destroyed.
//
// Exampe Usage:
//
// class MyClass {
//  private:
//   // This factory will be used to schedule invocations of SomeMethod.
//   ScopedRunnableMethodFactory<MyClass> some_method_factory_;
//
//  public:
//   // It is safe to suppress warning 4355 here.
//   MyClass() : some_method_factory_(this) { }
//
//   void SomeMethod() {
//     // If this function might be called directly, you might want to revoke
//     // any outstanding runnable methods scheduled to call it.  If it's not
//     // referenced other than by the factory, this is unnecessary.
//     some_method_factory_.RevokeAll();
//     ...
//   }
//
//   void ScheduleSomeMethod() {
//     // If you'd like to only only have one pending task at a time, test for
//     // |empty| before manufacturing another task.
//     if (!some_method_factory_.empty())
//       return;
//
//     // The factories are not thread safe, so always invoke on
//     // |MessageLoop::current()|.
//     MessageLoop::current()->PostDelayedTask(FROM_HERE,
//         some_method_factory_.NewRunnableMethod(&MyClass::SomeMethod),
//         kSomeMethodDelayMS);
//   }
// };

// A ScopedTaskFactory produces tasks of type |TaskType| and prevents them from
// running after it is destroyed.
template<class TaskType>
class ScopedTaskFactory : public RevocableStore {
 public:
  ScopedTaskFactory() { }

  // Create a new task.
  inline TaskType* NewTask() {
    return new TaskWrapper(this);
  }

  class TaskWrapper : public TaskType, public NonThreadSafe {
   public:
    explicit TaskWrapper(RevocableStore* store) : revocable_(store) { }

    virtual void Run() {
      if (!revocable_.revoked())
        TaskType::Run();
    }

   private:
    Revocable revocable_;

    DISALLOW_EVIL_CONSTRUCTORS(TaskWrapper);
  };

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ScopedTaskFactory);
};

// A ScopedRunnableMethodFactory creates runnable methods for a specified
// object.  This is particularly useful for generating callbacks for
// non-reference counted objects when the factory is a member of the object.
template<class T>
class ScopedRunnableMethodFactory : public RevocableStore {
 public:
  explicit ScopedRunnableMethodFactory(T* object) : object_(object) { }

  template <class Method, typename... Elements>
  inline Task* NewRunnableMethod(Method method, Elements&&... elements) {
    typedef mozilla::Tuple<typename mozilla::Decay<Elements>::Type...> ArgsTuple;
    typedef RunnableMethod<Method, ArgsTuple> Runnable;
    typedef typename ScopedTaskFactory<Runnable>::TaskWrapper TaskWrapper;

    TaskWrapper* task = new TaskWrapper(this);
    task->Init(object_, method, mozilla::MakeTuple(mozilla::Forward<Elements>(elements)...));
    return task;
  }

 protected:
  template <class Method, class Params>
  class RunnableMethod : public Task {
   public:
    RunnableMethod() { }

    void Init(T* obj, Method meth, Params&& params) {
      obj_ = obj;
      meth_ = meth;
      params_ = mozilla::Forward<Params>(params);
    }

    virtual void Run() { DispatchTupleToMethod(obj_, meth_, params_); }

   private:
    T* MOZ_UNSAFE_REF("The validity of this pointer must be enforced by "
                      "external factors.") obj_;
    Method meth_;
    Params params_;

    DISALLOW_EVIL_CONSTRUCTORS(RunnableMethod);
  };

 private:
  T* object_;

  DISALLOW_EVIL_CONSTRUCTORS(ScopedRunnableMethodFactory);
};

// General task implementations ------------------------------------------------

// Task to delete an object
template<class T>
class DeleteTask : public CancelableTask {
 public:
  explicit DeleteTask(T* obj) : obj_(obj) {
  }
  virtual void Run() {
    delete obj_;
  }
  virtual void Cancel() {
    obj_ = NULL;
  }
 private:
  T* MOZ_UNSAFE_REF("The validity of this pointer must be enforced by "
                    "external factors.") obj_;
};

// Task to Release() an object
template<class T>
class ReleaseTask : public CancelableTask {
 public:
  explicit ReleaseTask(T* obj) : obj_(obj) {
  }
  virtual void Run() {
    if (obj_)
      obj_->Release();
  }
  virtual void Cancel() {
    obj_ = NULL;
  }
 private:
  T* MOZ_UNSAFE_REF("The validity of this pointer must be enforced by "
                    "external factors.") obj_;
};

// RunnableMethodTraits --------------------------------------------------------
//
// This traits-class is used by RunnableMethod to manage the lifetime of the
// callee object.  By default, it is assumed that the callee supports AddRef
// and Release methods.  A particular class can specialize this template to
// define other lifetime management.  For example, if the callee is known to
// live longer than the RunnableMethod object, then a RunnableMethodTraits
// struct could be defined with empty RetainCallee and ReleaseCallee methods.

template <class T>
struct RunnableMethodTraits {
  static void RetainCallee(T* obj) {
    obj->AddRef();
  }
  static void ReleaseCallee(T* obj) {
    obj->Release();
  }
};

// This allows using the NewRunnableMethod() functions with a const pointer
// to the callee object. See the similar support in nsRefPtr for a rationale
// of why this is reasonable.
template <class T>
struct RunnableMethodTraits<const T> {
  static void RetainCallee(const T* obj) {
    const_cast<T*>(obj)->AddRef();
  }
  static void ReleaseCallee(const  T* obj) {
    const_cast<T*>(obj)->Release();
  }
};

// RunnableMethod and RunnableFunction -----------------------------------------
//
// Runnable methods are a type of task that call a function on an object when
// they are run. We implement both an object and a set of NewRunnableMethod and
// NewRunnableFunction functions for convenience. These functions are
// overloaded and will infer the template types, simplifying calling code.
//
// The template definitions all use the following names:
// T                - the class type of the object you're supplying
//                    this is not needed for the Static version of the call
// Method/Function  - the signature of a pointer to the method or function you
//                    want to call
// Param            - the parameter(s) to the method, possibly packed as a Tuple
// A                - the first parameter (if any) to the method
// B                - the second parameter (if any) to the mathod
//
// Put these all together and you get an object that can call a method whose
// signature is:
//   R T::MyFunction([A[, B]])
//
// Usage:
// PostTask(FROM_HERE, NewRunnableMethod(object, &Object::method[, a[, b]])
// PostTask(FROM_HERE, NewRunnableFunction(&function[, a[, b]])

// RunnableMethod and NewRunnableMethod implementation -------------------------

template <class T, class Method, class Params>
class RunnableMethod : public CancelableTask,
                       public RunnableMethodTraits<T> {
 public:
  RunnableMethod(T* obj, Method meth, Params&& params)
      : obj_(obj), meth_(meth), params_(mozilla::Forward<Params>(params)) {
    this->RetainCallee(obj_);
  }
  ~RunnableMethod() {
    ReleaseCallee();
  }

  virtual void Run() {
    if (obj_)
      DispatchTupleToMethod(obj_, meth_, params_);
  }

  virtual void Cancel() {
    ReleaseCallee();
  }

 private:
  void ReleaseCallee() {
    if (obj_) {
      RunnableMethodTraits<T>::ReleaseCallee(obj_);
      obj_ = nullptr;
    }
  }

  // This is owning because of the RetainCallee and ReleaseCallee calls in the
  // constructor and destructor.
  T* MOZ_OWNING_REF obj_;
  Method meth_;
  Params params_;
};

template <class T, class Method, typename... Args>
inline CancelableTask* NewRunnableMethod(T* object, Method method, Args&&... args) {
  typedef mozilla::Tuple<typename mozilla::Decay<Args>::Type...> ArgsTuple;
  return new RunnableMethod<T, Method, ArgsTuple>(
      object, method, mozilla::MakeTuple(mozilla::Forward<Args>(args)...));
}

// RunnableFunction and NewRunnableFunction implementation ---------------------

template <class Function, class Params>
class RunnableFunction : public CancelableTask {
 public:
  RunnableFunction(Function function, Params&& params)
      : function_(function), params_(mozilla::Forward<Params>(params)) {
  }

  ~RunnableFunction() {
  }

  virtual void Run() {
    if (function_)
      DispatchTupleToFunction(function_, params_);
  }

  virtual void Cancel() {
    function_ = nullptr;
  }

  Function function_;
  Params params_;
};

template <class Function, typename... Args>
inline CancelableTask* NewRunnableFunction(Function function, Args&&... args) {
  typedef mozilla::Tuple<typename mozilla::Decay<Args>::Type...> ArgsTuple;
  return new RunnableFunction<Function, ArgsTuple>(
      function, mozilla::MakeTuple(mozilla::Forward<Args>(args)...));
}

// Callback --------------------------------------------------------------------
//
// A Callback is like a Task but with unbound parameters. It is basically an
// object-oriented function pointer.
//
// Callbacks are designed to work with Tuples.  A set of helper functions and
// classes is provided to hide the Tuple details from the consumer.  Client
// code will generally work with the CallbackRunner base class, which merely
// provides a Run method and is returned by the New* functions. This allows
// users to not care which type of class implements the callback, only that it
// has a certain number and type of arguments.
//
// The implementation of this is done by CallbackImpl, which inherits
// CallbackStorage to store the data. This allows the storage of the data
// (requiring the class type T) to be hidden from users, who will want to call
// this regardless of the implementor's type T.
//
// Note that callbacks currently have no facility for cancelling or abandoning
// them. We currently handle this at a higher level for cases where this is
// necessary. The pointer in a callback must remain valid until the callback
// is made.
//
// Like Task, the callback executor is responsible for deleting the callback
// pointer once the callback has executed.
//
// Example client usage:
//   void Object::DoStuff(int, string);
//   Callback2<int, string>::Type* callback =
//       NewCallback(obj, &Object::DoStuff);
//   callback->Run(5, string("hello"));
//   delete callback;
// or, equivalently, using tuples directly:
//   CallbackRunner<Tuple2<int, string> >* callback =
//       NewCallback(obj, &Object::DoStuff);
//   callback->RunWithParams(base::MakeTuple(5, string("hello")));

// Base for all Callbacks that handles storage of the pointers.
template <class T, typename Method>
class CallbackStorage {
 public:
  CallbackStorage(T* obj, Method meth) : obj_(obj), meth_(meth) {
  }

 protected:
  T* MOZ_UNSAFE_REF("The validity of this pointer must be enforced by "
                    "external factors.") obj_;
  Method meth_;
};

// Interface that is exposed to the consumer, that does the actual calling
// of the method.
template <typename Params>
class CallbackRunner {
 public:
  typedef Params TupleType;

  virtual ~CallbackRunner() {}
  virtual void RunWithParams(const Params& params) = 0;

  // Convenience functions so callers don't have to deal with Tuples.
  inline void Run() {
    RunWithParams(Tuple0());
  }

  template <typename Arg1>
  inline void Run(const Arg1& a) {
    RunWithParams(Params(a));
  }

  template <typename Arg1, typename Arg2>
  inline void Run(const Arg1& a, const Arg2& b) {
    RunWithParams(Params(a, b));
  }

  template <typename Arg1, typename Arg2, typename Arg3>
  inline void Run(const Arg1& a, const Arg2& b, const Arg3& c) {
    RunWithParams(Params(a, b, c));
  }

  template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
  inline void Run(const Arg1& a, const Arg2& b, const Arg3& c, const Arg4& d) {
    RunWithParams(Params(a, b, c, d));
  }

  template <typename Arg1, typename Arg2, typename Arg3,
            typename Arg4, typename Arg5>
  inline void Run(const Arg1& a, const Arg2& b, const Arg3& c,
                  const Arg4& d, const Arg5& e) {
    RunWithParams(Params(a, b, c, d, e));
  }
};

template <class T, typename Method, typename Params>
class CallbackImpl : public CallbackStorage<T, Method>,
                     public CallbackRunner<Params> {
 public:
  CallbackImpl(T* obj, Method meth) : CallbackStorage<T, Method>(obj, meth) {
  }
  virtual void RunWithParams(const Params& params) {
    // use "this->" to force C++ to look inside our templatized base class; see
    // Effective C++, 3rd Ed, item 43, p210 for details.
    DispatchToMethod(this->obj_, this->meth_, params);
  }
};

// 0-arg implementation
struct Callback0 {
  typedef CallbackRunner<Tuple0> Type;
};

template <class T>
typename Callback0::Type* NewCallback(T* object, void (T::*method)()) {
  return new CallbackImpl<T, void (T::*)(), Tuple0 >(object, method);
}

// 1-arg implementation
template <typename Arg1>
struct Callback1 {
  typedef CallbackRunner<Tuple1<Arg1> > Type;
};

template <class T, typename Arg1>
typename Callback1<Arg1>::Type* NewCallback(T* object,
                                            void (T::*method)(Arg1)) {
  return new CallbackImpl<T, void (T::*)(Arg1), Tuple1<Arg1> >(object, method);
}

// 2-arg implementation
template <typename Arg1, typename Arg2>
struct Callback2 {
  typedef CallbackRunner<Tuple2<Arg1, Arg2> > Type;
};

template <class T, typename Arg1, typename Arg2>
typename Callback2<Arg1, Arg2>::Type* NewCallback(
    T* object,
    void (T::*method)(Arg1, Arg2)) {
  return new CallbackImpl<T, void (T::*)(Arg1, Arg2),
      Tuple2<Arg1, Arg2> >(object, method);
}

// 3-arg implementation
template <typename Arg1, typename Arg2, typename Arg3>
struct Callback3 {
  typedef CallbackRunner<Tuple3<Arg1, Arg2, Arg3> > Type;
};

template <class T, typename Arg1, typename Arg2, typename Arg3>
typename Callback3<Arg1, Arg2, Arg3>::Type* NewCallback(
    T* object,
    void (T::*method)(Arg1, Arg2, Arg3)) {
  return new CallbackImpl<T,  void (T::*)(Arg1, Arg2, Arg3),
      Tuple3<Arg1, Arg2, Arg3> >(object, method);
}

// 4-arg implementation
template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
struct Callback4 {
  typedef CallbackRunner<Tuple4<Arg1, Arg2, Arg3, Arg4> > Type;
};

template <class T, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
typename Callback4<Arg1, Arg2, Arg3, Arg4>::Type* NewCallback(
    T* object,
    void (T::*method)(Arg1, Arg2, Arg3, Arg4)) {
  return new CallbackImpl<T, void (T::*)(Arg1, Arg2, Arg3, Arg4),
      Tuple4<Arg1, Arg2, Arg3, Arg4> >(object, method);
}

// 5-arg implementation
template <typename Arg1, typename Arg2, typename Arg3,
          typename Arg4, typename Arg5>
struct Callback5 {
  typedef CallbackRunner<Tuple5<Arg1, Arg2, Arg3, Arg4, Arg5> > Type;
};

template <class T, typename Arg1, typename Arg2,
          typename Arg3, typename Arg4, typename Arg5>
typename Callback5<Arg1, Arg2, Arg3, Arg4, Arg5>::Type* NewCallback(
    T* object,
    void (T::*method)(Arg1, Arg2, Arg3, Arg4, Arg5)) {
  return new CallbackImpl<T, void (T::*)(Arg1, Arg2, Arg3, Arg4, Arg5),
      Tuple5<Arg1, Arg2, Arg3, Arg4, Arg5> >(object, method);
}

// An UnboundMethod is a wrapper for a method where the actual object is
// provided at Run dispatch time.
template <class T, class Method, class Params>
class UnboundMethod {
 public:
  UnboundMethod(Method m, Params p) : m_(m), p_(p) {}
  void Run(T* obj) const {
    DispatchToMethod(obj, m_, p_);
  }
 private:
  Method m_;
  Params p_;
};

#endif  // BASE_TASK_H_
