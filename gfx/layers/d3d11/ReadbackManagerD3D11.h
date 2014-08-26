/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_READBACKMANAGERD3D11_H
#define GFX_READBACKMANAGERD3D11_H

#include <windows.h>
#include <d3d10_1.h>

#include "nsTArray.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace layers {

class TextureReadbackSink;
struct ReadbackTask;

class ReadbackManagerD3D11 MOZ_FINAL
{
  NS_INLINE_DECL_REFCOUNTING(ReadbackManagerD3D11)
public:
  ReadbackManagerD3D11();

  /**
   * Tell the readback manager to post a readback task.
   *
   * @param aTexture D3D10_USAGE_STAGING texture that will contain the data that
   *                 was readback.
   * @param aSink    TextureReadbackSink that the resulting DataSourceSurface
   *                 should be dispatched to.
   */
  void PostTask(ID3D10Texture2D* aTexture, TextureReadbackSink* aSink);

private:
  ~ReadbackManagerD3D11();

  static DWORD WINAPI StartTaskThread(void *aManager);

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
};

}
}

#endif /* GFX_READBACKMANAGERD3D11_H */
