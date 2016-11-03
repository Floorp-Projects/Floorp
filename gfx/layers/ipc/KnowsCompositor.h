/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_KNOWSCOMPOSITOR
#define MOZILLA_LAYERS_KNOWSCOMPOSITOR

#include "mozilla/layers/LayersTypes.h"  // for LayersBackend
#include "mozilla/layers/CompositorTypes.h"

namespace mozilla {
namespace layers {

class SyncObject;
class TextureForwarder;
class LayersIPCActor;

/**
 * An abstract interface for classes that are tied to a specific Compositor across
 * IPDL and uses TextureFactoryIdentifier to describe this Compositor.
 */
class KnowsCompositor {
public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release(void) = 0;

  KnowsCompositor();
  ~KnowsCompositor();

  void IdentifyTextureHost(const TextureFactoryIdentifier& aIdentifier);

  SyncObject* GetSyncObject() { return mSyncObject; }

  int32_t GetMaxTextureSize() const
  {
    return mTextureFactoryIdentifier.mMaxTextureSize;
  }

  /**
   * Returns the type of backend that is used off the main thread.
   * We only don't allow changing the backend type at runtime so this value can
   * be queried once and will not change until Gecko is restarted.
   */
  LayersBackend GetCompositorBackendType() const
  {
    return mTextureFactoryIdentifier.mParentBackend;
  }

  bool SupportsTextureBlitting() const
  {
    return mTextureFactoryIdentifier.mSupportsTextureBlitting;
  }

  bool SupportsPartialUploads() const
  {
    return mTextureFactoryIdentifier.mSupportsPartialUploads;
  }

  bool SupportsComponentAlpha() const
  {
    return mTextureFactoryIdentifier.mSupportsComponentAlpha;
  }

  const TextureFactoryIdentifier& GetTextureFactoryIdentifier() const
  {
    return mTextureFactoryIdentifier;
  }

  int32_t GetSerial() { return mSerial; }

  /**
   * Helpers for finding other related interface. These are infallible.
   */
  virtual TextureForwarder* GetTextureForwarder() = 0;
  virtual LayersIPCActor* GetLayersIPCActor() = 0;

protected:
  TextureFactoryIdentifier mTextureFactoryIdentifier;
  RefPtr<SyncObject> mSyncObject;

  const int32_t mSerial;
  static mozilla::Atomic<int32_t> sSerialCounter;
};

} // namespace layers
} // namespace mozilla

#endif
