// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OBSERVER_LIST_THREADSAFE_H_
#define BASE_OBSERVER_LIST_THREADSAFE_H_

#include <vector>
#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/task.h"

namespace base {

///////////////////////////////////////////////////////////////////////////////
//
// OVERVIEW:
//
//   A thread-safe container for a list of observers.
//   This is similar to the observer_list (see observer_list.h), but it
//   is more robust for multi-threaded situations.
//
//   The following use cases are supported:
//    * Observers can register for notifications from any thread.
//      Callbacks to the observer will occur on the same thread where
//      the observer initially called AddObserver() from.
//    * Any thread may trigger a notification via NOTIFY_OBSERVERS.
//    * Observers can remove themselves from the observer list inside
//      of a callback.
//    * If one thread is notifying observers concurrently with an observer
//      removing itself from the observer list, the notifications will
//      be silently dropped.
//
//   The drawback of the threadsafe observer list is that notifications
//   are not as real-time as the non-threadsafe version of this class.
//   Notifications will always be done via PostTask() to another thread,
//   whereas with the non-thread-safe observer_list, notifications happen
//   synchronously and immediately.
//
//   IMPLEMENTATION NOTES
//   The ObserverListThreadSafe maintains an ObserverList for each thread
//   which uses the ThreadSafeObserver.  When Notifying the observers,
//   we simply call PostTask to each registered thread, and then each thread
//   will notify its regular ObserverList.
//
///////////////////////////////////////////////////////////////////////////////
template <class ObserverType>
class ObserverListThreadSafe
    : public base::RefCountedThreadSafe<ObserverListThreadSafe<ObserverType> > {
 public:
  ObserverListThreadSafe() {}

  ~ObserverListThreadSafe() {
    typename ObserversListMap::const_iterator it;
    for (it = observer_lists_.begin(); it != observer_lists_.end(); ++it)
      delete (*it).second;
    observer_lists_.clear();
  }

  // Add an observer to the list.
  void AddObserver(ObserverType* obs) {
    ObserverList<ObserverType>* list = NULL;
    MessageLoop* loop = MessageLoop::current();
    // TODO(mbelshe): Get rid of this check.  Its needed right now because
    //                Time currently triggers usage of the ObserverList.
    //                And unittests use time without a MessageLoop.
    if (!loop)
      return;  // Some unittests may access this without a message loop.
    {
      AutoLock lock(list_lock_);
      if (observer_lists_.find(loop) == observer_lists_.end())
        observer_lists_[loop] = new ObserverList<ObserverType>();
      list = observer_lists_[loop];
    }
    list->AddObserver(obs);
  }

  // Remove an observer from the list.
  // If there are pending notifications in-transit to the observer, they will
  // be aborted.
  // RemoveObserver MUST be called from the same thread which called
  // AddObserver.
  void RemoveObserver(ObserverType* obs) {
    ObserverList<ObserverType>* list = NULL;
    MessageLoop* loop = MessageLoop::current();
    if (!loop)
      return;  // On shutdown, it is possible that current() is already null.
    {
      AutoLock lock(list_lock_);
      list = observer_lists_[loop];
      if (!list) {
        NOTREACHED() << "RemoveObserver called on for unknown thread";
        return;
      }

      // If we're about to remove the last observer from the list,
      // then we can remove this observer_list entirely.
      if (list->size() == 1)
        observer_lists_.erase(loop);
    }
    list->RemoveObserver(obs);

    // If RemoveObserver is called from a notification, the size will be
    // nonzero.  Instead of deleting here, the NotifyWrapper will delete
    // when it finishes iterating.
    if (list->size() == 0)
      delete list;
  }

  // Notify methods.
  // Make a thread-safe callback to each Observer in the list.
  // Note, these calls are effectively asynchronous.  You cannot assume
  // that at the completion of the Notify call that all Observers have
  // been Notified.  The notification may still be pending delivery.
  template <class Method>
  void Notify(Method m) {
    UnboundMethod<ObserverType, Method, Tuple0> method(m, MakeTuple());
    Notify<Method, Tuple0>(method);
  }

  template <class Method, class A>
  void Notify(Method m, const A &a) {
    UnboundMethod<ObserverType, Method, Tuple1<A> > method(m, MakeTuple(a));
    Notify<Method, Tuple1<A> >(method);
  }

  // TODO(mbelshe):  Add more wrappers for Notify() with more arguments.

 private:
  template <class Method, class Params>
  void Notify(const UnboundMethod<ObserverType, Method, Params>& method) {
    AutoLock lock(list_lock_);
    typename ObserversListMap::iterator it;
    for (it = observer_lists_.begin(); it != observer_lists_.end(); ++it) {
      MessageLoop* loop = (*it).first;
      ObserverList<ObserverType>* list = (*it).second;
      loop->PostTask(FROM_HERE,
          NewRunnableMethod(this,
              &ObserverListThreadSafe<ObserverType>::
                 template NotifyWrapper<Method, Params>, list, method));
    }
  }

  // Wrapper which is called to fire the notifications for each thread's
  // ObserverList.  This function MUST be called on the thread which owns
  // the unsafe ObserverList.
  template <class Method, class Params>
  void NotifyWrapper(ObserverList<ObserverType>* list,
      const UnboundMethod<ObserverType, Method, Params>& method) {

    // Check that this list still needs notifications.
    {
      AutoLock lock(list_lock_);
      typename ObserversListMap::iterator it =
          observer_lists_.find(MessageLoop::current());

      // The ObserverList could have been removed already.  In fact, it could
      // have been removed and then re-added!  If the master list's loop
      // does not match this one, then we do not need to finish this
      // notification.
      if (it == observer_lists_.end() || it->second != list)
        return;
    }

    {
      typename ObserverList<ObserverType>::Iterator it(*list);
      ObserverType* obs;
      while ((obs = it.GetNext()) != NULL)
        method.Run(obs);
    }

    // If there are no more observers on the list, we can now delete it.
    if (list->size() == 0) {
#ifndef NDEBUG
      {
        AutoLock lock(list_lock_);
        // Verify this list is no longer registered.
        typename ObserversListMap::iterator it =
            observer_lists_.find(MessageLoop::current());
        DCHECK(it == observer_lists_.end() || it->second != list);
      }
#endif
      delete list;
    }
  }

  typedef std::map<MessageLoop*, ObserverList<ObserverType>*> ObserversListMap;

  // These are marked mutable to facilitate having NotifyAll be const.
  Lock list_lock_;  // Protects the observer_lists_.
  ObserversListMap observer_lists_;

  DISALLOW_EVIL_CONSTRUCTORS(ObserverListThreadSafe);
};

} // namespace base

#endif  // BASE_OBSERVER_LIST_THREADSAFE_H_
