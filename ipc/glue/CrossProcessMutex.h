/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
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

#ifndef mozilla_CrossProcessMutex_h
#define mozilla_CrossProcessMutex_h

#include "base/process.h"
#include "mozilla/Mutex.h"

namespace IPC {
template<typename T>
struct ParamTraits;
}

//
// Provides:
//
//  - CrossProcessMutex, a non-recursive mutex that can be shared across processes
//  - CrossProcessMutexAutoLock, an RAII class for ensuring that Mutexes are
//    properly locked and unlocked
//
// Using CrossProcessMutexAutoLock/CrossProcessMutexAutoUnlock is MUCH
// preferred to making bare calls to CrossProcessMutex.Lock and Unlock.
//
namespace mozilla {
#ifdef XP_WIN
typedef HANDLE CrossProcessMutexHandle;
#else
// Stub for other platforms. We can't use uintptr_t here since different
// processes could disagree on its size.
typedef uintptr_t CrossProcessMutexHandle;
#endif

class NS_COM_GLUE CrossProcessMutex
{
public:
  /**
   * CrossProcessMutex
   * @param name A name which can reference this lock (currently unused)
   **/
  CrossProcessMutex(const char* aName);
  /**
   * CrossProcessMutex
   * @param handle A handle of an existing cross process mutex that can be
   *               opened.
   */
  CrossProcessMutex(CrossProcessMutexHandle aHandle);

  /**
   * ~CrossProcessMutex
   **/
  ~CrossProcessMutex();

  /**
   * Lock
   * This will lock the mutex. Any other thread in any other process that
   * has access to this mutex calling lock will block execution until the
   * initial caller of lock has made a call to Unlock.
   *
   * If the owning process is terminated unexpectedly the mutex will be
   * released.
   **/
  void Lock();

  /**
   * Unlock
   * This will unlock the mutex. A single thread currently waiting on a lock
   * call will resume execution and aquire ownership of the lock. No
   * guarantees are made as to the order in which waiting threads will resume
   * execution.
   **/
  void Unlock();

  /**
   * ShareToProcess
   * This function is called to generate a serializable structure that can
   * be sent to the specified process and opened on the other side.
   *
   * @returns A handle that can be shared to another process
   */
  CrossProcessMutexHandle ShareToProcess(base::ProcessHandle aTarget);

private:
  friend struct IPC::ParamTraits<CrossProcessMutex>;

  CrossProcessMutex();
  CrossProcessMutex(const CrossProcessMutex&);
  CrossProcessMutex &operator=(const CrossProcessMutex&);

#ifdef XP_WIN
  HANDLE mMutex;
#endif
};

typedef BaseAutoLock<CrossProcessMutex> CrossProcessMutexAutoLock;
typedef BaseAutoUnlock<CrossProcessMutex> CrossProcessMutexAutoUnlock;

}
#endif
