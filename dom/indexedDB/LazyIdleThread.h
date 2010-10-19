/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_dom_indexeddb_lazyidlethread_h__
#define mozilla_dom_indexeddb_lazyidlethread_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIObserver.h"
#include "nsIThreadInternal.h"
#include "nsITimer.h"

#include "mozilla/Mutex.h"

#define IDLE_THREAD_TOPIC "thread-shutting-down"

BEGIN_INDEXEDDB_NAMESPACE

/**
 * This class provides a basic event target that creates its thread lazily and
 * destroys its thread after a period of inactivity. It may be created on any
 * thread but it may only be used from the thread on which it is created. If it
 * is created on the main thread then it will automatically join its thread on
 * XPCOM shutdown using the Observer Service.
 */
class LazyIdleThread : public nsIThread,
                       public nsITimerCallback,
                       public nsIThreadObserver,
                       public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEVENTTARGET
  NS_DECL_NSITHREAD
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSITHREADOBSERVER
  NS_DECL_NSIOBSERVER

  enum ShutdownMethod {
    AutomaticShutdown = 0,
    ManualShutdown
  };

  /**
   * Create a new LazyIdleThread that will destroy its thread after the given
   * number of milliseconds.
   */
  LazyIdleThread(PRUint32 aIdleTimeoutMS,
                 ShutdownMethod aShutdownMethod = AutomaticShutdown,
                 nsIObserver* aIdleObserver = nsnull);

  /**
   * Add an observer that will be notified when the thread is idle and about to
   * be shut down. The aSubject argument can be QueryInterface'd to an nsIThread
   * that can be used to post cleanup events. The aTopic argument will be
   * IDLE_THREAD_TOPIC, and aData will be null. The LazyIdleThread does not add
   * a reference to the observer to avoid circular references as it is assumed
   * to be the owner. It is the caller's responsibility to clear this observer
   * if the pointer becomes invalid.
   */
  void SetWeakIdleObserver(nsIObserver* aObserver);

  /**
   * Disable the idle timeout for this thread. No effect if the timeout is
   * already disabled.
   */
  void DisableIdleTimeout();

  /**
   * Enable the idle timeout. No effect if the timeout is already enabled.
   */
  void EnableIdleTimeout();

private:
  /**
   * Calls Shutdown().
   */
  ~LazyIdleThread();

  /**
   * Called just before dispatching to mThread.
   */
  void PreDispatch();

  /**
   * Makes sure a valid thread lives in mThread.
   */
  nsresult EnsureThread();

  /**
   * Called on mThread to set up the thread observer.
   */
  void InitThread();

  /**
   * Called on mThread to clean up the thread observer.
   */
  void CleanupThread();

  /**
   * Called on the main thread when mThread believes itself to be idle. Sets up
   * the idle timer.
   */
  void ScheduleTimer();

  /**
   * Called when we are shutting down mThread.
   */
  nsresult ShutdownThread();

  /**
   * Deletes this object. Used to delay calling mThread->Shutdown() during the
   * final release (during a GC, for instance).
   */
  void SelfDestruct();

  /**
   * Protects data that is accessed on both threads.
   */
  mozilla::Mutex mMutex;

  /**
   * Touched on both threads but set before mThread is created. Used to direct
   * timer events to the owning thread.
   */
  nsCOMPtr<nsIThread> mOwningThread;

  /**
   * Only accessed on the owning thread. Set by EnsureThread().
   */
  nsCOMPtr<nsIThread> mThread;

  /**
   * Protected by mMutex. Created when mThread has no pending events and fired
   * at mOwningThread. Any thread that dispatches to mThread will take ownership
   * of the timer and fire a separate cancel event to the owning thread.
   */
  nsCOMPtr<nsITimer> mIdleTimer;

  /**
   * Idle observer. Called when the thread is about to be shut down. Released
   * only when Shutdown() is called.
   */
  nsIObserver* mIdleObserver;

  /**
   * The number of milliseconds a thread should be idle before dying.
   */
  const PRUint32 mIdleTimeoutMS;

  /**
   * The number of events that are pending on mThread. A nonzero value means
   * that the thread cannot be cleaned up.
   */
  PRUint32 mPendingEventCount;

  /**
   * The number of times that mThread has dispatched an idle notification. Any
   * timer that fires while this count is nonzero can safely be ignored as
   * another timer will be on the way.
   */
  PRUint32 mIdleNotificationCount;

  /**
   * Whether or not the thread should automatically shutdown. If the owner
   * specified ManualShutdown at construction time then the owner should take
   * care to call Shutdown() manually when appropriate.
   */
  ShutdownMethod mShutdownMethod;

  /**
   * Only accessed on the owning thread. Set to true when Shutdown() has been
   * called and prevents EnsureThread() from recreating mThread.
   */
  PRPackedBool mShutdown;

  /**
   * Set from CleanupThread and lasting until the thread has shut down. Prevents
   * further idle notifications during the shutdown process.
   */
  PRPackedBool mThreadIsShuttingDown;

  /**
   * Whether or not the idle timeout is enabled.
   */
  PRPackedBool mIdleTimeoutEnabled;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_lazyidlethread_h__
