/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedBufferManagerPARENT_H_
#define SharedBufferManagerPARENT_H_

#include "mozilla/layers/PSharedBufferManagerParent.h"

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/Mutex.h"            // for Mutex

namespace android {
class GraphicBuffer;
}
#endif

namespace base {
class Thread;
}

namespace mozilla {
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
class Mutex;
#endif

namespace layers {

class SharedBufferManagerParent : public PSharedBufferManagerParent
{
public:
  /**
   * Create a SharedBufferManagerParent for child process, and link to the child side before leaving
   */
  static PSharedBufferManagerParent* Create(Transport* aTransport, ProcessId aOtherProcess);

  /**
   * Function for find the buffer owner, most buffer passing on IPC contains only owner/key pair.
   * Use these function to access the real buffer.
   */
  static SharedBufferManagerParent* GetInstance(ProcessId id);
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  android::sp<android::GraphicBuffer> GetGraphicBuffer(int key);
  static android::sp<android::GraphicBuffer> GetGraphicBuffer(GrallocBufferRef aRef);
#endif
  /**
   * Create a SharedBufferManagerParent but do not open the link
   */
  SharedBufferManagerParent(Transport* aTransport, ProcessId aOwner, base::Thread* aThread);
  virtual ~SharedBufferManagerParent();

  /**
   * When the IPC channel down or something bad make this Manager die, clear all the buffer reference!
   */
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  virtual bool RecvAllocateGrallocBuffer(const IntSize&, const uint32_t&, const uint32_t&, mozilla::layers::MaybeMagicGrallocBufferHandle*);
  virtual bool RecvDropGrallocBuffer(const mozilla::layers::MaybeMagicGrallocBufferHandle& handle);

  /**
   * Break the buffer's sharing state, decrease buffer reference for both side
   */
  void DropGrallocBuffer(mozilla::layers::SurfaceDescriptor aDesc);

  // Overriden from IToplevelProtocol
  IToplevelProtocol*
  CloneToplevel(const InfallibleTArray<ProtocolFdMapping>& aFds,
                base::ProcessHandle aPeerProcess,
                mozilla::ipc::ProtocolCloneContext* aCtx) MOZ_OVERRIDE;
  MessageLoop* GetMessageLoop();

protected:
  /**
   * All living SharedBufferManager instances used to find the buffer owner, and parent->child IPCs
   */
  static std::map<base::ProcessId, SharedBufferManagerParent*> sManagers;

  /**
   * Break the buffer's sharing state, decrease buffer reference for both side
   *
   * Must be called from SharedBufferManagerParent's thread
   */
  void DropGrallocBufferImpl(mozilla::layers::SurfaceDescriptor aDesc);

  // dispatched function
  static void DropGrallocBufferSync(SharedBufferManagerParent* mgr, mozilla::layers::SurfaceDescriptor aDesc);

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  /**
   * Buffers owned by this SharedBufferManager pair
   */
  std::map<int, android::sp<android::GraphicBuffer> > mBuffers;
  Mutex mBuffersMutex;
#endif
  
  Transport* mTransport;
  base::ProcessId mOwner;
  base::Thread* mThread;
  static int sBufferKey;
  static Monitor sManagerMonitor;
};

} /* namespace layers */
} /* namespace mozilla */
#endif /* SharedBufferManagerPARENT_H_ */
