/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_BrowserProcessSubThread_h
#define mozilla_ipc_BrowserProcessSubThread_h

#include "base/thread.h"
#include "mozilla/StaticMutex.h"

#include "nsDebug.h"

namespace mozilla {
namespace ipc {

// Copied from browser_process_impl.cc, modified slightly.
class BrowserProcessSubThread : public base::Thread
{
public:
  // An enumeration of the well-known threads.
  enum ID {
      IO,
      //FILE,
      //DB,
      //HISTORY,
#if defined(OS_LINUX) || defined(OS_SOLARIS)
      // This thread has a second connection to the X server and is used
      // to process UI requests when routing the request to the UI
      // thread would risk deadlock.
      BACKGROUND_X11,
#endif

      // This identifier does not represent a thread.  Instead it counts
      // the number of well-known threads.  Insert new well-known
      // threads before this identifier.
      ID_COUNT
  };

  explicit BrowserProcessSubThread(ID aId);
  ~BrowserProcessSubThread();

  static MessageLoop* GetMessageLoop(ID identifier);

protected:
  virtual void Init();
  virtual void CleanUp();

private:
  // The identifier of this thread.  Only one thread can exist with a given
  // identifier at a given time.
  ID mIdentifier;

  // This lock protects |browser_threads_|.  Do not read or modify that array
  // without holding this lock.  Do not block while holding this lock.

  static StaticMutex sLock;

  // An array of the ChromeThread objects.  This array is protected by |lock_|.
  // The threads are not owned by this array.  Typically, the threads are owned
  // on the UI thread by the g_browser_process object.  ChromeThreads remove
  // themselves from this array upon destruction.
  static BrowserProcessSubThread* sBrowserThreads[ID_COUNT];
};

inline void AssertIOThread()
{
  NS_ASSERTION(MessageLoop::TYPE_IO == MessageLoop::current()->type(),
               "should be on the IO thread!");
}

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_BrowserProcessSubThread_h
