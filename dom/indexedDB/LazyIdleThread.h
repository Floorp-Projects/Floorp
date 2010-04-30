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

BEGIN_INDEXEDDB_NAMESPACE

/**
 * This class provides a basic event target that creates its thread lazily and
 * destroys its thread after a period of inactivity. It may be created on any
 * thread but it may only be used from the thread on which it is created. If it
 * is created on the main thread then it will automatically join its thread on
 * XPCOM shutdown using the Observer Service.
 */
class LazyIdleThread : public nsITimerCallback,
                       public nsIThreadObserver,
                       public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSITHREADOBSERVER
  NS_DECL_NSIOBSERVER

  /**
   * Create a new LazyIdleThread that will destroy its thread after the given
   * number of milliseconds.
   */
  LazyIdleThread(PRUint32 aIdleTimeoutMS);

  /**
   * Dispatch an event to the thread, creating it if necessary.
   */
  nsresult Dispatch(nsIRunnable* aEvent);

  /**
   * Join the thread and prevent further event dispatch. It is not possible to
   * reuse the LazyIdleThread after calling this. Shutdown() will also be called
   * automatically when the reference count drops to 0.
   */
  void Shutdown();

private:
  /**
   * Calls Shutdown().
   */
  ~LazyIdleThread();

  /**
   * Makes sure a valid thread lives in mThread.
   */
  nsresult EnsureThread();

  /**
   * Called on mThread to set up the thread observer.
   */
  void InitThread();

  /**
   * Called when we are shutting down mThread.
   */
  void ShutdownThread();

  /**
   * Protects mIdleTimer and mThreadHasTimedOut.
   */
  mozilla::Mutex mMutex;

  /**
   * Touched on both threads but set before mThread is created. Used to direct
   * timer events to the owning thread.
   */
  nsCOMPtr<nsIThread> mOwningThread;

  /**
   * The number of milliseconds a thread should be idle before dying.
   */
  const PRUint32 mIdleTimeoutMS;

  /**
   * Only accessed on the owning thread. Set by EnsureThread().
   */
  nsCOMPtr<nsIThread> mThread;

  /**
   * Only accessed on the owning thread. Set to true when Shutdown() has been
   * called and prevents EnsureThread() from recreating mThread.
   */
  PRBool mShutdown;

  /**
   * Protected by mMutex. Created when mThread has no pending events and fired
   * at mOwningThread. Any thread that dispatches to mThread will take ownership
   * of the timer and fire a separate cancel event to the owning thread.
   */
  nsCOMPtr<nsITimer> mIdleTimer;

  /**
   * Protected by mMutex. Effectively makes the thread observer methods no-ops
   * when we're in the process of shutting down mThread.
   */
  PRBool mThreadHasTimedOut;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_lazyidlethread_h__
