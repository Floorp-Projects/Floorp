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

#ifndef mozilla_ipc_MozillaChildThread_h
#define mozilla_ipc_MozillaChildThread_h

#include "base/thread.h"
#include "base/process.h"

#include "chrome/common/child_thread.h"

namespace mozilla {
namespace ipc {

/**
 * A MozillaChildThread is the main thread in a child process. It will
 * initialize XPCOM leak detectors (NS_LogInit) but it will not initialize
 * XPCOM (see ContentProcessThread for a child which uses XPCOM).
 */
class MozillaChildThread : public ChildThread
{
public:
  typedef base::ProcessHandle ProcessHandle;

  MozillaChildThread(ProcessHandle aParentProcessHandle,
		     MessageLoop::Type type=MessageLoop::TYPE_UI)
  : ChildThread(base::Thread::Options(type,    // message loop type
				      0,       // stack size
				      false)), // wait for Init()?
    mParentProcessHandle(aParentProcessHandle)
  { }

protected:
  virtual void OnControlMessageReceived(const IPC::Message& aMessage);

  ProcessHandle GetParentProcessHandle() {
    return mParentProcessHandle;
  }

  // Thread implementation:
  virtual void Init();
  virtual void CleanUp();

private:
  ProcessHandle mParentProcessHandle;

  DISALLOW_EVIL_CONSTRUCTORS(MozillaChildThread);
};

} /* namespace ipc */
} /* namespace mozilla */

#endif /* mozilla_ipc_MozillaChildThread_h */
