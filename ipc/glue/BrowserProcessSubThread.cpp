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

#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "chrome/common/notification_service.h"

#if defined(OS_WIN)
#include <objbase.h>
#endif

namespace mozilla {
namespace ipc {

//
// BrowserProcessSubThread
//

// Friendly names for the well-known threads.
static const char* kBrowserThreadNames[BrowserProcessSubThread::ID_COUNT] = {
  "Gecko_IOThread",  // IO
//  "Chrome_FileThread",  // FILE
//  "Chrome_DBThread",  // DB
//  "Chrome_HistoryThread",  // HISTORY
#if defined(OS_LINUX)
  "Gecko_Background_X11Thread",  // BACKGROUND_X11
#endif
};

Lock BrowserProcessSubThread::sLock;
BrowserProcessSubThread* BrowserProcessSubThread::sBrowserThreads[ID_COUNT] = {
  NULL,  // IO
//  NULL,  // FILE
//  NULL,  // DB
//  NULL,  // HISTORY
#if defined(OS_LINUX)
  NULL,  // BACKGROUND_X11
#endif
};

BrowserProcessSubThread::BrowserProcessSubThread(ID aId) :
  base::Thread(kBrowserThreadNames[aId]),
  mIdentifier(aId),
  mNotificationService(NULL)
{
  AutoLock lock(sLock);
  DCHECK(aId >= 0 && aId < ID_COUNT);
  DCHECK(sBrowserThreads[aId] == NULL);
  sBrowserThreads[aId] = this;
}

BrowserProcessSubThread::~BrowserProcessSubThread()
{
  Stop();
  {AutoLock lock(sLock);
    sBrowserThreads[mIdentifier] = NULL;
  }

}

void
BrowserProcessSubThread::Init()
{
#if defined(OS_WIN)
  // Initializes the COM library on the current thread.
  CoInitialize(NULL);
#endif
  mNotificationService = new NotificationService();
}

void
BrowserProcessSubThread::CleanUp()
{
  delete mNotificationService;
  mNotificationService = NULL;

#if defined(OS_WIN)
  // Closes the COM library on the current thread. CoInitialize must
  // be balanced by a corresponding call to CoUninitialize.
  CoUninitialize();
#endif
}

// static
MessageLoop*
BrowserProcessSubThread::GetMessageLoop(ID aId)
{
  AutoLock lock(sLock);
  DCHECK(aId >= 0 && aId < ID_COUNT);

  if (sBrowserThreads[aId])
    return sBrowserThreads[aId]->message_loop();

  return NULL;
}

} // namespace ipc
} // namespace mozilla
