/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ReadbackManagerD3D11.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/gfx/2D.h"

#include "nsIThread.h"
#include "nsThreadUtils.h"

namespace mozilla {

using namespace gfx;

namespace layers {

// Structure that contains the information required to execute a readback task,
// the only member accessed off the main thread here is mReadbackTexture. Since
// mSink may be released only on the main thread this object should always be
// destroyed on the main thread!
struct ReadbackTask {
  // The texture that we copied the contents of the paintedlayer to.
  RefPtr<ID3D10Texture2D> mReadbackTexture;
  // The sink that we're trying to read back to.
  RefPtr<TextureReadbackSink> mSink;
};

// This class is created and dispatched from the Readback thread but it must be
// destroyed by the main thread.
class ReadbackResultWriterD3D11 final : public nsIRunnable {
  ~ReadbackResultWriterD3D11() {}
  NS_DECL_THREADSAFE_ISUPPORTS
 public:
  explicit ReadbackResultWriterD3D11(UniquePtr<ReadbackTask>&& aTask)
      : mTask(std::move(aTask)) {}

  NS_IMETHOD Run() override {
    D3D10_TEXTURE2D_DESC desc;
    mTask->mReadbackTexture->GetDesc(&desc);

    D3D10_MAPPED_TEXTURE2D mappedTex;
    // Unless there is an error this map should succeed immediately, as we've
    // recently mapped (and unmapped) this copied data on our task thread.
    HRESULT hr = mTask->mReadbackTexture->Map(0, D3D10_MAP_READ, 0, &mappedTex);

    if (FAILED(hr)) {
      mTask->mSink->ProcessReadback(nullptr);
      return NS_OK;
    }

    {
      RefPtr<DataSourceSurface> surf = Factory::CreateWrappingDataSourceSurface(
          (uint8_t*)mappedTex.pData, mappedTex.RowPitch,
          IntSize(desc.Width, desc.Height), SurfaceFormat::B8G8R8A8);

      mTask->mSink->ProcessReadback(surf);

      MOZ_ASSERT(surf->hasOneRef());
    }

    mTask->mReadbackTexture->Unmap(0);

    return NS_OK;
  }

 private:
  UniquePtr<ReadbackTask> mTask;
};

NS_IMPL_ISUPPORTS(ReadbackResultWriterD3D11, nsIRunnable)

DWORD WINAPI ReadbackManagerD3D11::StartTaskThread(void* aManager) {
  static_cast<ReadbackManagerD3D11*>(aManager)->ProcessTasks();

  return 0;
}

ReadbackManagerD3D11::ReadbackManagerD3D11() : mRefCnt(0) {
  ::InitializeCriticalSection(&mTaskMutex);
  mShutdownEvent = ::CreateEventA(nullptr, FALSE, FALSE, nullptr);
  mTaskSemaphore = ::CreateSemaphoreA(nullptr, 0, 1000000, nullptr);
  mTaskThread = ::CreateThread(nullptr, 0, StartTaskThread, this, 0, 0);
}

ReadbackManagerD3D11::~ReadbackManagerD3D11() {
  ::SetEvent(mShutdownEvent);

  // This shouldn't take longer than 5 seconds, if it does we're going to choose
  // to leak the thread and its synchronisation in favor of crashing or freezing
  DWORD result = ::WaitForSingleObject(mTaskThread, 5000);
  if (result != WAIT_TIMEOUT) {
    ::DeleteCriticalSection(&mTaskMutex);
    ::CloseHandle(mShutdownEvent);
    ::CloseHandle(mTaskSemaphore);
    ::CloseHandle(mTaskThread);
  } else {
    MOZ_CRASH("ReadbackManager: Task thread did not shutdown in 5 seconds.");
  }
}

void ReadbackManagerD3D11::PostTask(ID3D10Texture2D* aTexture,
                                    TextureReadbackSink* aSink) {
  auto task = MakeUnique<ReadbackTask>();
  task->mReadbackTexture = aTexture;
  task->mSink = aSink;

  ::EnterCriticalSection(&mTaskMutex);
  mPendingReadbackTasks.AppendElement(std::move(task));
  ::LeaveCriticalSection(&mTaskMutex);

  ::ReleaseSemaphore(mTaskSemaphore, 1, nullptr);
}

void ReadbackManagerD3D11::ProcessTasks() {
  HANDLE handles[] = {mTaskSemaphore, mShutdownEvent};

  while (true) {
    DWORD result = ::WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    if (result != WAIT_OBJECT_0) {
      return;
    }

    ::EnterCriticalSection(&mTaskMutex);
    if (mPendingReadbackTasks.Length() == 0) {
      MOZ_CRASH("Trying to read from an empty array, bad bad bad");
    }
    UniquePtr<ReadbackTask> nextReadbackTask =
        std::move(mPendingReadbackTasks[0]);
    mPendingReadbackTasks.RemoveElementAt(0);
    ::LeaveCriticalSection(&mTaskMutex);

    // We want to block here until the texture contents are available, the
    // easiest thing is to simply map and unmap.
    D3D10_MAPPED_TEXTURE2D mappedTex;
    nextReadbackTask->mReadbackTexture->Map(0, D3D10_MAP_READ, 0, &mappedTex);
    nextReadbackTask->mReadbackTexture->Unmap(0);

    // We can only send the update to the sink on the main thread, so post an
    // event there to do so. Ownership of the task is passed from
    // mPendingReadbackTasks to ReadbackResultWriter here.
    nsCOMPtr<nsIThread> thread = do_GetMainThread();
    thread->Dispatch(new ReadbackResultWriterD3D11(std::move(nextReadbackTask)),
                     nsIEventTarget::DISPATCH_NORMAL);
  }
}

}  // namespace layers
}  // namespace mozilla
