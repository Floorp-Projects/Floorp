/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_TaskFactory_h
#define mozilla_plugins_TaskFactory_h

#include <base/task.h>

#include "mozilla/Move.h"

/*
 * This is based on the ScopedRunnableMethodFactory from ipc/chromium/src/base/task.h
 * Chromium's factories assert if tasks are created and run on different threads,
 * which is something we need to do in PluginModuleParent (hang UI vs. main thread).
 * TaskFactory just provides cancellable tasks that don't assert this.
 * This version also allows both ScopedMethod and regular Tasks to be generated
 * by the same Factory object.
 */

namespace mozilla {
namespace plugins {

template<class T>
class TaskFactory : public RevocableStore
{
private:
  template<class TaskType>
  class TaskWrapper : public TaskType
  {
  public:
    template<typename... Args>
    explicit TaskWrapper(RevocableStore* store, Args&&... args)
      : TaskType(mozilla::Forward<Args>(args)...)
      , revocable_(store)
    {
    }

    NS_IMETHOD Run() override {
      if (!revocable_.revoked())
        TaskType::Run();
      return NS_OK;
    }

  private:
    Revocable revocable_;
  };

public:
  explicit TaskFactory(T* object) : object_(object) { }

  template <typename TaskParamType, typename... Args>
  inline already_AddRefed<TaskParamType> NewTask(Args&&... args)
  {
    typedef TaskWrapper<TaskParamType> TaskWrapper;
    RefPtr<TaskWrapper> task =
      new TaskWrapper(this, mozilla::Forward<Args>(args)...);
    return task.forget();
  }

  template <class Method>
  inline already_AddRefed<Runnable> NewRunnableMethod(Method method) {
    typedef TaskWrapper<RunnableMethod<Method, Tuple0> > TaskWrapper;

    RefPtr<TaskWrapper> task = new TaskWrapper(this);
    task->Init(object_, method, base::MakeTuple());
    return task.forget();
  }

  template <class Method, class A>
  inline already_AddRefed<Runnable> NewRunnableMethod(Method method, const A& a) {
    typedef TaskWrapper<RunnableMethod<Method, Tuple1<A> > > TaskWrapper;

    RefPtr<TaskWrapper> task = new TaskWrapper(this);
    task->Init(object_, method, base::MakeTuple(a));
    return task.forget();
  }

protected:
  template <class Method, class Params>
  class RunnableMethod : public Runnable {
   public:
    RunnableMethod() { }

    void Init(T* obj, Method meth, const Params& params) {
      obj_ = obj;
      meth_ = meth;
      params_ = params;
    }

    NS_IMETHOD Run() override {
      DispatchToMethod(obj_, meth_, params_);
      return NS_OK;
    }

   private:
    T* obj_;
    Method meth_;
    Params params_;
  };

private:
  T* object_;
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_TaskFactory_h
