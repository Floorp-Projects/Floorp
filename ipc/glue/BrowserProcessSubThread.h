/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef mozilla_ipc_BrowserProcessSubThread_h
#define mozilla_ipc_BrowserProcessSubThread_h

#include "base/thread.h"
#include "base/lock.h"

#include "nsDebug.h"

class NotificationService;

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
#if defined(OS_LINUX)
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

  NotificationService* mNotificationService;

  // This lock protects |browser_threads_|.  Do not read or modify that array
  // without holding this lock.  Do not block while holding this lock.

  // FIXME/cjones: XPCOM doesn't like static vars, so can't use 
  // mozilla::Mutex
  static Lock sLock;

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
