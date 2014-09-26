/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

class ReadbackManagerD3D10 MOZ_FINAL : public IUnknown
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
   * @param aOrigin  Origin of the aTexture surface in the PaintedLayer
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
