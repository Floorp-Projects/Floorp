/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/SharedBufferManagerParent.h"
#include "base/message_loop.h"          // for MessageLoop
#include "base/process.h"               // for ProcessHandle
#include "base/process_util.h"          // for OpenProcessHandle
#include "base/task.h"                  // for CancelableTask, DeleteTask, etc
#include "base/tracked.h"               // for FROM_HERE
#include "base/thread.h"
#include "mozilla/ipc/MessageChannel.h" // for MessageChannel, etc
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Transport.h"      // for Transport
#include "nsIMemoryReporter.h"
#ifdef MOZ_WIDGET_GONK
#include "ui/PixelFormat.h"
#endif
#include "nsThreadUtils.h"

using namespace mozilla::ipc;
#ifdef MOZ_WIDGET_GONK
using namespace android;
#endif
using std::map;

namespace mozilla {
namespace layers {

class GrallocReporter MOZ_FINAL : public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS

  GrallocReporter()
  {
#ifdef DEBUG
    // There must be only one instance of this class, due to |sAmount|
    // being static.  Assert this.
    static bool hasRun = false;
    MOZ_ASSERT(!hasRun);
    hasRun = true;
#endif
  }

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData)
  {
    return MOZ_COLLECT_REPORT(
      "gralloc", KIND_OTHER, UNITS_BYTES, sAmount,
"Special RAM that can be shared between processes and directly accessed by "
"both the CPU and GPU. Gralloc memory is usually a relatively precious "
"resource, with much less available than generic RAM. When it's exhausted, "
"graphics performance can suffer. This value can be incorrect because of race "
"conditions.");
  }

  static int64_t sAmount;
};

NS_IMPL_ISUPPORTS(GrallocReporter, nsIMemoryReporter)

int64_t GrallocReporter::sAmount = 0;

void InitGralloc() {
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  RegisterStrongMemoryReporter(new GrallocReporter());
}

map<base::ProcessId, SharedBufferManagerParent* > SharedBufferManagerParent::sManagers;
StaticAutoPtr<Monitor> SharedBufferManagerParent::sManagerMonitor;
uint64_t SharedBufferManagerParent::sBufferKey = 0;

/**
 * Task that deletes SharedBufferManagerParent on a specified thread.
 */
class DeleteSharedBufferManagerParentTask : public Task
{
public:
    DeleteSharedBufferManagerParentTask(SharedBufferManagerParent* aSharedBufferManager)
        : mSharedBufferManager(aSharedBufferManager) {
    }
    virtual void Run() MOZ_OVERRIDE {}
private:
    nsAutoPtr<SharedBufferManagerParent> mSharedBufferManager;
};

SharedBufferManagerParent::SharedBufferManagerParent(Transport* aTransport, base::ProcessId aOwner, base::Thread* aThread)
  : mTransport(aTransport)
  , mThread(aThread)
  , mMainMessageLoop(MessageLoop::current())
  , mDestroyed(false)
  , mLock("SharedBufferManagerParent.mLock")
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sManagerMonitor) {
    sManagerMonitor = new Monitor("Manager Monitor");
  }

  MonitorAutoLock lock(*sManagerMonitor.get());
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread");
  if (!aThread->IsRunning()) {
    aThread->Start();
  }

  if (sManagers.count(aOwner) != 0) {
    printf_stderr("SharedBufferManagerParent already exists.");
  }
  mOwner = aOwner;
  sManagers[aOwner] = this;
}

SharedBufferManagerParent::~SharedBufferManagerParent()
{
  MonitorAutoLock lock(*sManagerMonitor.get());
  if (mTransport) {
    XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                     new DeleteTask<Transport>(mTransport));
  }
  sManagers.erase(mOwner);
  delete mThread;
}

void
SharedBufferManagerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MutexAutoLock lock(mLock);
  mDestroyed = true;
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  mBuffers.clear();
#endif
  DeleteSharedBufferManagerParentTask* task = new DeleteSharedBufferManagerParentTask(this);
  mMainMessageLoop->PostTask(FROM_HERE, task);
}

static void
ConnectSharedBufferManagerInParentProcess(SharedBufferManagerParent* aManager,
                                          Transport* aTransport,
                                          base::ProcessHandle aOtherProcess)
{
  aManager->Open(aTransport, aOtherProcess, XRE_GetIOMessageLoop(), ipc::ParentSide);
}

PSharedBufferManagerParent* SharedBufferManagerParent::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  ProcessHandle processHandle;
  if (!base::OpenProcessHandle(aOtherProcess, &processHandle)) {
    return nullptr;
  }
  base::Thread* thread = nullptr;
  char thrname[128];
  base::snprintf(thrname, 128, "BufMgrParent#%d", aOtherProcess);
  thread = new base::Thread(thrname);

  SharedBufferManagerParent* manager = new SharedBufferManagerParent(aTransport, aOtherProcess, thread);
  if (!thread->IsRunning()) {
    thread->Start();
  }
  thread->message_loop()->PostTask(FROM_HERE,
                                   NewRunnableFunction(ConnectSharedBufferManagerInParentProcess,
                                                       manager, aTransport, processHandle));
  return manager;
}

bool SharedBufferManagerParent::RecvAllocateGrallocBuffer(const IntSize& aSize, const uint32_t& aFormat, const uint32_t& aUsage, mozilla::layers::MaybeMagicGrallocBufferHandle* aHandle)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC

  *aHandle = null_t();

  if (aFormat == 0 || aUsage == 0) {
    printf_stderr("SharedBufferManagerParent::RecvAllocateGrallocBuffer -- format and usage must be non-zero");
    return true;
  }

  // If the requested size is too big (i.e. exceeds the commonly used max GL texture size)
  // then we risk OOMing the parent process. It's better to just deny the allocation and
  // kill the child process, which is what the following code does.
  // TODO: actually use GL_MAX_TEXTURE_SIZE instead of hardcoding 4096
  if (aSize.width > 4096 || aSize.height > 4096) {
    printf_stderr("SharedBufferManagerParent::RecvAllocateGrallocBuffer -- requested gralloc buffer is too big.");
    return false;
  }

  sp<GraphicBuffer> outgoingBuffer = new GraphicBuffer(aSize.width, aSize.height, aFormat, aUsage);
  if (!outgoingBuffer.get() || outgoingBuffer->initCheck() != NO_ERROR) {
    printf_stderr("SharedBufferManagerParent::RecvAllocateGrallocBuffer -- gralloc buffer allocation failed");
    return true;
  }

  int64_t bufferKey;
  {
    MonitorAutoLock lock(*sManagerMonitor.get());
    bufferKey = ++sBufferKey; 
  }
  GrallocBufferRef ref;
  ref.mOwner = mOwner;
  ref.mKey = bufferKey;
  *aHandle = MagicGrallocBufferHandle(outgoingBuffer, ref);

  int bpp = 0;
  bpp = android::bytesPerPixel(outgoingBuffer->getPixelFormat());
  if (bpp > 0)
    GrallocReporter::sAmount += outgoingBuffer->getStride() * outgoingBuffer->getHeight() * bpp;
  else // Specical case for BSP specific formats(mainly YUV formats, count it as normal YUV buffer)
    GrallocReporter::sAmount += outgoingBuffer->getStride() * outgoingBuffer->getHeight() * 3 / 2;

  {
    MutexAutoLock lock(mLock);
    mBuffers[bufferKey] = outgoingBuffer;
  }
#endif
  return true;
}

bool SharedBufferManagerParent::RecvDropGrallocBuffer(const mozilla::layers::MaybeMagicGrallocBufferHandle& handle)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  NS_ASSERTION(handle.type() == MaybeMagicGrallocBufferHandle::TGrallocBufferRef, "We shouldn't interact with the real buffer!");
  int64_t bufferKey = handle.get_GrallocBufferRef().mKey;
  sp<GraphicBuffer> buf = GetGraphicBuffer(bufferKey);
  MOZ_ASSERT(buf.get());
  MutexAutoLock lock(mLock);
  NS_ASSERTION(mBuffers.count(bufferKey) == 1, "How can you drop others buffer");
  mBuffers.erase(bufferKey);

  if(!buf.get()) {
    printf_stderr("SharedBufferManagerParent::RecvDropGrallocBuffer -- invalid buffer key.");
    return true;
  }

  int bpp = 0;
  bpp = android::bytesPerPixel(buf->getPixelFormat());
  if (bpp > 0)
    GrallocReporter::sAmount -= buf->getStride() * buf->getHeight() * bpp;
  else // Specical case for BSP specific formats(mainly YUV formats, count it as normal YUV buffer)
    GrallocReporter::sAmount -= buf->getStride() * buf->getHeight() * 3 / 2;

#endif
  return true;
}

/*static*/
void SharedBufferManagerParent::DropGrallocBufferSync(SharedBufferManagerParent* mgr, mozilla::layers::SurfaceDescriptor aDesc)
{
  mgr->DropGrallocBufferImpl(aDesc);
}

/*static*/
void SharedBufferManagerParent::DropGrallocBuffer(ProcessId id, mozilla::layers::SurfaceDescriptor aDesc)
{
  if (aDesc.type() != SurfaceDescriptor::TNewSurfaceDescriptorGralloc) {
    return;
  }

  MonitorAutoLock lock(*sManagerMonitor.get());
  SharedBufferManagerParent* mgr = SharedBufferManagerParent::GetInstance(id);
  if (!mgr) {
    return;
  }

  MutexAutoLock mgrlock(mgr->mLock);
  if (mgr->mDestroyed) {
    return;
  }

  if (PlatformThread::CurrentId() == mgr->mThread->thread_id()) {
    MOZ_CRASH("SharedBufferManagerParent::DropGrallocBuffer should not be called on SharedBufferManagerParent thread");
  } else {
    mgr->mThread->message_loop()->PostTask(FROM_HERE,
                                      NewRunnableFunction(&DropGrallocBufferSync, mgr, aDesc));
  }
  return;
}

void SharedBufferManagerParent::DropGrallocBufferImpl(mozilla::layers::SurfaceDescriptor aDesc)
{
  MutexAutoLock lock(mLock);
  if (mDestroyed) {
    return;
  }
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  int64_t key = -1;
  MaybeMagicGrallocBufferHandle handle;
  if (aDesc.type() == SurfaceDescriptor::TNewSurfaceDescriptorGralloc) {
    handle = aDesc.get_NewSurfaceDescriptorGralloc().buffer();
  } else {
    return;
  }

  if (handle.type() == MaybeMagicGrallocBufferHandle::TGrallocBufferRef) {
    key = handle.get_GrallocBufferRef().mKey;
  } else if (handle.type() == MaybeMagicGrallocBufferHandle::TMagicGrallocBufferHandle) {
    key = handle.get_MagicGrallocBufferHandle().mRef.mKey;
  }

  NS_ASSERTION(key != -1, "Invalid buffer key");
  NS_ASSERTION(mBuffers.count(key) == 1, "How can you drop others buffer");
  mBuffers.erase(key);
  SendDropGrallocBuffer(handle);
#endif
}

MessageLoop* SharedBufferManagerParent::GetMessageLoop()
{
  return mThread->message_loop();
}

SharedBufferManagerParent* SharedBufferManagerParent::GetInstance(ProcessId id)
{
  NS_ASSERTION(sManagers.count(id) == 1, "No BufferManager for the process");
  return sManagers[id];
}

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
android::sp<android::GraphicBuffer>
SharedBufferManagerParent::GetGraphicBuffer(int64_t key)
{
  MutexAutoLock lock(mLock);
  if (mBuffers.count(key) == 1) {
    return mBuffers[key];
  } else {
    // The buffer can be dropped, or invalid
    printf_stderr("SharedBufferManagerParent::GetGraphicBuffer -- invalid key");
    return nullptr;
  }
}

android::sp<android::GraphicBuffer>
SharedBufferManagerParent::GetGraphicBuffer(GrallocBufferRef aRef)
{
  MonitorAutoLock lock(*sManagerMonitor.get());
  return GetInstance(aRef.mOwner)->GetGraphicBuffer(aRef.mKey);
}
#endif

IToplevelProtocol*
SharedBufferManagerParent::CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                                 base::ProcessHandle aPeerProcess,
                                 mozilla::ipc::ProtocolCloneContext* aCtx)
{
  for (unsigned int i = 0; i < aFds.Length(); i++) {
    if (aFds[i].protocolId() == unsigned(GetProtocolId())) {
      Transport* transport = OpenDescriptor(aFds[i].fd(),
                                            Transport::MODE_SERVER);
      PSharedBufferManagerParent* bufferManager = Create(transport, base::GetProcId(aPeerProcess));
      bufferManager->CloneManagees(this, aCtx);
      bufferManager->IToplevelProtocol::SetTransport(transport);
      return bufferManager;
    }
  }
  return nullptr;
}

} /* namespace layers */
} /* namespace mozilla */
