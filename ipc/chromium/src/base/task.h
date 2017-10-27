/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_H_
#define BASE_TASK_H_

#include "base/revocable_store.h"
#include "base/tuple.h"
#include "mozilla/IndexSequence.h"
#include "mozilla/Tuple.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"

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
//     MessageLoop::current()->PostDelayedTask(
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

  class TaskWrapper : public TaskType {
   public:
    explicit TaskWrapper(RevocableStore* store) : revocable_(store) { }

    NS_IMETHOD Run() override {
      if (!revocable_.revoked())
        TaskType::Run();
      return NS_OK;
    }

    ~TaskWrapper() {
      NS_ASSERT_OWNINGTHREAD(TaskWrapper);
    }

   private:
    Revocable revocable_;

    NS_DECL_OWNINGTHREAD

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
  inline already_AddRefed<mozilla::Runnable>
  NewRunnableMethod(Method method, Elements&&... elements) {
    typedef mozilla::Tuple<typename mozilla::Decay<Elements>::Type...> ArgsTuple;
    typedef RunnableMethod<Method, ArgsTuple> Runnable;
    typedef typename ScopedTaskFactory<Runnable>::TaskWrapper TaskWrapper;

    RefPtr<TaskWrapper> task = new TaskWrapper(this);
    task->Init(object_, method, mozilla::MakeTuple(mozilla::Forward<Elements>(elements)...));
    return task.forget();
  }

 protected:
  template <class Method, class Params>
  class RunnableMethod : public mozilla::Runnable {
   public:
     RunnableMethod()
       : mozilla::Runnable("ScopedRunnableMethodFactory::RunnableMethod")
     {
     }

     void Init(T* obj, Method meth, Params&& params)
     {
       obj_ = obj;
       meth_ = meth;
       params_ = mozilla::Forward<Params>(params);
    }

    NS_IMETHOD Run() override {
      DispatchTupleToMethod(obj_, meth_, params_);
      return NS_OK;
    }

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
class DeleteTask : public mozilla::CancelableRunnable {
 public:
   explicit DeleteTask(T* obj)
     : mozilla::CancelableRunnable("DeleteTask")
     , obj_(obj)
   {
  }
  NS_IMETHOD Run() override {
    delete obj_;
    return NS_OK;
  }
  virtual nsresult Cancel() override {
    obj_ = NULL;
    return NS_OK;
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
// PostTask(NewRunnableMethod(object, &Object::method[, a[, b]])
// PostTask(NewRunnableFunction(&function[, a[, b]])

// RunnableMethod and NewRunnableMethod implementation -------------------------

template <class T, class Method, class Params>
class RunnableMethod : public mozilla::CancelableRunnable,
                       public RunnableMethodTraits<T> {
 public:
   RunnableMethod(T* obj, Method meth, Params&& params)
     : mozilla::CancelableRunnable("RunnableMethod")
     , obj_(obj)
     , meth_(meth)
     , params_(mozilla::Forward<Params>(params))
   {
     this->RetainCallee(obj_);
  }
  ~RunnableMethod() {
    ReleaseCallee();
  }

  NS_IMETHOD Run() override {
    if (obj_)
      DispatchTupleToMethod(obj_, meth_, params_);
    return NS_OK;
  }

  virtual nsresult Cancel() override {
    ReleaseCallee();
    return NS_OK;
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

namespace dont_add_new_uses_of_this {

// Don't add new uses of this!!!!
template <class T, class Method, typename... Args>
inline already_AddRefed<mozilla::Runnable>
NewRunnableMethod(T* object, Method method, Args&&... args) {
  typedef mozilla::Tuple<typename mozilla::Decay<Args>::Type...> ArgsTuple;
  RefPtr<mozilla::Runnable> t =
    new RunnableMethod<T, Method, ArgsTuple>(object, method,
                                             mozilla::MakeTuple(mozilla::Forward<Args>(args)...));
  return t.forget();
}

} // namespace dont_add_new_uses_of_this

// RunnableFunction and NewRunnableFunction implementation ---------------------

template <class Function, class Params>
class RunnableFunction : public mozilla::CancelableRunnable {
 public:
   RunnableFunction(const char* name, Function function, Params&& params)
     : mozilla::CancelableRunnable(name)
     , function_(function)
     , params_(mozilla::Forward<Params>(params))
   {
  }

  ~RunnableFunction() {
  }

  NS_IMETHOD Run() override {
    if (function_)
      DispatchTupleToFunction(function_, params_);
    return NS_OK;
  }

  virtual nsresult Cancel() override {
    function_ = nullptr;
    return NS_OK;
  }

  Function function_;
  Params params_;
};

template <class Function, typename... Args>
inline already_AddRefed<mozilla::CancelableRunnable>
NewCancelableRunnableFunction(const char* name, Function function, Args&&... args) {
  typedef mozilla::Tuple<typename mozilla::Decay<Args>::Type...> ArgsTuple;
  RefPtr<mozilla::CancelableRunnable> t =
    new RunnableFunction<Function, ArgsTuple>(name, function,
                                              mozilla::MakeTuple(mozilla::Forward<Args>(args)...));
  return t.forget();
}

template <class Function, typename... Args>
inline already_AddRefed<mozilla::Runnable>
NewRunnableFunction(const char* name, Function function, Args&&... args) {
  typedef mozilla::Tuple<typename mozilla::Decay<Args>::Type...> ArgsTuple;
  RefPtr<mozilla::Runnable> t =
    new RunnableFunction<Function, ArgsTuple>(name, function,
                                              mozilla::MakeTuple(mozilla::Forward<Args>(args)...));
  return t.forget();
}

#endif  // BASE_TASK_H_
