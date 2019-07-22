/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CanvasParent_h
#define mozilla_layers_CanvasParent_h

#include "mozilla/ipc/CrossProcessSemaphore.h"
#include "mozilla/layers/CanvasTranslator.h"
#include "mozilla/layers/PCanvasParent.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace layers {
class TextureData;

class CanvasParent final : public PCanvasParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CanvasParent)

  friend class PProtocolParent;

  /**
   * Create a CanvasParent and bind it to the given endpoint on the
   * CanvasPlaybackLoop.
   *
   * @params aEndpoint the endpoint to bind to
   * @returns the new CanvasParent
   */
  static already_AddRefed<CanvasParent> Create(
      Endpoint<PCanvasParent>&& aEndpoint);

  static bool IsInCanvasThread();

  /**
   * Shutdown the canvas thread.
   */
  static void Shutdown();

  /**
   * Create a canvas translator for a particular TextureType, which translates
   * events from a CanvasEventRingBuffer.
   *
   * @param aTextureType the TextureType the translator will create
   * @param aReadHandle handle to the shared memory for the
   * CanvasEventRingBuffer
   * @param aReaderSem reading blocked semaphore for the CanvasEventRingBuffer
   * @param aWriterSem writing blocked semaphore for the CanvasEventRingBuffer
   */
  ipc::IPCResult RecvCreateTranslator(
      const TextureType& aTextureType,
      const ipc::SharedMemoryBasic::Handle& aReadHandle,
      const CrossProcessSemaphoreHandle& aReaderSem,
      const CrossProcessSemaphoreHandle& aWriterSem);

  /**
   * Used to tell the CanvasTranslator to start translating again after it has
   * stopped due to a timeout waiting for events.
   */
  ipc::IPCResult RecvResumeTranslation();

  void ActorDestroy(ActorDestroyReason why) final;

  void ActorDealloc() final;

  /**
   * Used by the compositor thread to get the SurfaceDescriptor associated with
   * the DrawTarget from another process.
   *
   * @param aDrawTarget the key to find the TextureData
   * @returns the SurfaceDescriptor associated with the key
   */
  UniquePtr<SurfaceDescriptor> LookupSurfaceDescriptorForClientDrawTarget(
      const uintptr_t aDrawTarget);

 private:
  CanvasParent();
  ~CanvasParent() final;

  DISALLOW_COPY_AND_ASSIGN(CanvasParent);

  void Bind(Endpoint<PCanvasParent>&& aEndpoint);

  void PostStartTranslationTask(uint32_t aDispatchFlags);

  void StartTranslation();

  RefPtr<CanvasParent> mSelfRef;
  UniquePtr<CanvasTranslator> mTranslator;
  volatile bool mTranslating = false;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CanvasParent_h
