/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#ifndef GFX_READBACKMANAGERD3D10_H
#define GFX_READBACKMANAGERD3D10_H

#include <windows.h>
#include <d3d10_1.h>

#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "gfxPoint.h"

namespace mozilla {
namespace layers {

DWORD WINAPI StartTaskThread(void *aManager);

struct ReadbackTask;

class ReadbackManagerD3D10 : public IUnknown
{
public:
  ReadbackManagerD3D10();
  ~ReadbackManagerD3D10();

  /**
   * Tell the readback manager to post a readback task.
   *
   * @param aTexture D3D10_USAGE_STAGING texture that will contain the data that
   *                 was readback.
   * @param aUpdate  ReadbackProcessor::Update object. This is a void pointer
   *                 since we cannot forward declare a nested class, and do not
   *                 export ReadbackProcessor.h
   * @param aOrigin  Origin of the aTexture surface in the ThebesLayer
   *                 coordinate system.
   */
  void PostTask(ID3D10Texture2D *aTexture, void *aUpdate, const gfxPoint &aOrigin);

  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                                   void **ppvObject);
  virtual ULONG STDMETHODCALLTYPE AddRef(void);
  virtual ULONG STDMETHODCALLTYPE Release(void);

private:
  friend DWORD WINAPI StartTaskThread(void *aManager);

  void ProcessTasks();

  // The invariant maintained by |mTaskSemaphore| is that the readback thread
  // will awaken from WaitForMultipleObjects() at least once per readback 
  // task enqueued by the main thread.  Since the readback thread processes
  // exactly one task per wakeup (with one exception), no tasks are lost.  The
  // exception is when the readback thread is shut down, which orphans the
  // remaining tasks, on purpose.
  HANDLE mTaskSemaphore;
  // Event signaled when the task thread should shutdown
  HANDLE mShutdownEvent;
  // Handle to the task thread
  HANDLE mTaskThread;

  // FiFo list of readback tasks that are to be executed. Access is synchronized
  // by mTaskMutex.
  CRITICAL_SECTION mTaskMutex;
  nsTArray<nsAutoPtr<ReadbackTask>> mPendingReadbackTasks;

  ULONG mRefCnt;
};

}
}

#endif /* GFX_READBACKMANAGERD3D10_H */