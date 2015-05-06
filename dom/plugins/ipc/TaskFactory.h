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

    virtual void Run() {
      if (!revocable_.revoked())
        TaskType::Run();
    }

  private:
    Revocable revocable_;
  };

public:
  explicit TaskFactory(T* object) : object_(object) { }

  template <typename TaskParamType, typename... Args>
  inline TaskParamType* NewTask(Args&&... args)
  {
    typedef TaskWrapper<TaskParamType> TaskWrapper;
    TaskWrapper* task = new TaskWrapper(this, mozilla::Forward<Args>(args)...);
    return task;
  }

  template <class Method>
  inline Task* NewRunnableMethod(Method method) {
    typedef TaskWrapper<RunnableMethod<Method, Tuple0> > TaskWrapper;

    TaskWrapper* task = new TaskWrapper(this);
    task->Init(object_, method, MakeTuple());
    return task;
  }

  template <class Method, class A>
  inline Task* NewRunnableMethod(Method method, const A& a) {
    typedef TaskWrapper<RunnableMethod<Method, Tuple1<A> > > TaskWrapper;

    TaskWrapper* task = new TaskWrapper(this);
    task->Init(object_, method, MakeTuple(a));
    return task;
  }

protected:
  template <class Method, class Params>
  class RunnableMethod : public Task {
   public:
    RunnableMethod() { }

    void Init(T* obj, Method meth, const Params& params) {
      obj_ = obj;
      meth_ = meth;
      params_ = params;
    }

    virtual void Run() { DispatchToMethod(obj_, meth_, params_); }

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
