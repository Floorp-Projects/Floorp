/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_ScopedMethodFactory_h
#define mozilla_plugins_ScopedMethodFactory_h

#include <base/task.h>

/*
 * This is based on the ScopedRunnableMethodFactory from ipc/chromium/src/base/task.h
 * Chromiums factories assert if tasks are created and run on different threads,
 * which is something we need to do in PluginModuleParent (hang UI vs. main thread).
 * ScopedMethodFactory just provides cancellable tasks that don't assert this.
 */

namespace mozilla {
namespace plugins {

template<class T>
class ScopedMethodFactory : public RevocableStore
{
private:
  template<class TaskType>
  class TaskWrapper : public TaskType
  {
  public:
    explicit TaskWrapper(RevocableStore* store) : revocable_(store) { }

    virtual void Run() {
      if (!revocable_.revoked())
        TaskType::Run();
    }

  private:
    Revocable revocable_;
  };

public:
  explicit ScopedMethodFactory(T* object) : object_(object) { }

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

#endif // mozilla_plugins_ScopedMethodFactory_h
