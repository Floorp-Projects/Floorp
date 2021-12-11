/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_KNOWSCOMPOSITOR
#define MOZILLA_LAYERS_KNOWSCOMPOSITOR

#include "mozilla/layers/LayersTypes.h"  // for LayersBackend
#include "mozilla/layers/CompositorTypes.h"
#include "nsExpirationTracker.h"
#include "mozilla/DataMutex.h"
#include "mozilla/layers/SyncObject.h"

namespace mozilla {
namespace layers {

class TextureForwarder;
class LayersIPCActor;
class ImageBridgeChild;

/**
 * See ActiveResourceTracker below.
 */
class ActiveResource {
 public:
  virtual void NotifyInactive() = 0;
  nsExpirationState* GetExpirationState() { return &mExpirationState; }
  bool IsActivityTracked() { return mExpirationState.IsTracked(); }

 private:
  nsExpirationState mExpirationState;
};

/**
 * A convenience class on top of nsExpirationTracker
 */
class ActiveResourceTracker : public nsExpirationTracker<ActiveResource, 3> {
 public:
  ActiveResourceTracker(uint32_t aExpirationCycle, const char* aName,
                        nsIEventTarget* aEventTarget)
      : nsExpirationTracker(aExpirationCycle, aName, aEventTarget) {}

  void NotifyExpired(ActiveResource* aResource) override {
    RemoveObject(aResource);
    aResource->NotifyInactive();
  }
};

/**
 * An abstract interface for classes that are tied to a specific Compositor
 * across IPDL and uses TextureFactoryIdentifier to describe this Compositor.
 */
class KnowsCompositor {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  KnowsCompositor();
  virtual ~KnowsCompositor();

  void IdentifyTextureHost(const TextureFactoryIdentifier& aIdentifier);

  // The sync object for the global content device.
  RefPtr<SyncObjectClient> GetSyncObject() {
    auto lock = mData.Lock();
    if (lock.ref().mSyncObject) {
      lock.ref().mSyncObject->EnsureInitialized();
    }
    return lock.ref().mSyncObject;
  }

  /// And by "thread-safe" here we merely mean "okay to hold strong references
  /// to from multiple threads". Not all methods actually are thread-safe.
  virtual bool IsThreadSafe() const { return true; }

  virtual RefPtr<KnowsCompositor> GetForMedia() {
    return RefPtr<KnowsCompositor>(this);
  }

  int32_t GetMaxTextureSize() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mMaxTextureSize;
  }

  /**
   * Returns the type of backend that is used off the main thread.
   * We only don't allow changing the backend type at runtime so this value can
   * be queried once and will not change until Gecko is restarted.
   */
  LayersBackend GetCompositorBackendType() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mParentBackend;
  }

  bool SupportsTextureBlitting() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mSupportsTextureBlitting;
  }

  bool SupportsPartialUploads() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mSupportsPartialUploads;
  }

  bool SupportsComponentAlpha() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mSupportsComponentAlpha;
  }

  bool SupportsTextureDirectMapping() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mSupportsTextureDirectMapping;
  }

  bool SupportsD3D11() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mParentBackend ==
               layers::LayersBackend::LAYERS_D3D11 ||
           (lock.ref().mTextureFactoryIdentifier.mParentBackend ==
                layers::LayersBackend::LAYERS_WR &&
            (lock.ref().mTextureFactoryIdentifier.mCompositorUseANGLE ||
             lock.ref().mTextureFactoryIdentifier.mWebRenderCompositor ==
                 layers::WebRenderCompositor::D3D11));
  }

  bool GetCompositorUseANGLE() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mCompositorUseANGLE;
  }

  bool GetCompositorUseDComp() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mCompositorUseDComp;
  }

  bool GetUseCompositorWnd() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mUseCompositorWnd;
  }

  WebRenderBackend GetWebRenderBackend() const {
    auto lock = mData.Lock();
    MOZ_ASSERT(lock.ref().mTextureFactoryIdentifier.mParentBackend ==
               layers::LayersBackend::LAYERS_WR);
    return lock.ref().mTextureFactoryIdentifier.mWebRenderBackend;
  }

  bool UsingSoftwareWebRender() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mParentBackend ==
               layers::LayersBackend::LAYERS_WR &&
           lock.ref().mTextureFactoryIdentifier.mWebRenderBackend ==
               WebRenderBackend::SOFTWARE;
  }

  bool UsingSoftwareWebRenderD3D11() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mParentBackend ==
               layers::LayersBackend::LAYERS_WR &&
           lock.ref().mTextureFactoryIdentifier.mWebRenderBackend ==
               WebRenderBackend::SOFTWARE &&
           lock.ref().mTextureFactoryIdentifier.mWebRenderCompositor ==
               layers::WebRenderCompositor::D3D11;
  }

  bool UsingSoftwareWebRenderOpenGL() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier.mParentBackend ==
               layers::LayersBackend::LAYERS_WR &&
           lock.ref().mTextureFactoryIdentifier.mWebRenderBackend ==
               WebRenderBackend::SOFTWARE &&
           lock.ref().mTextureFactoryIdentifier.mWebRenderCompositor ==
               layers::WebRenderCompositor::OPENGL;
  }

  TextureFactoryIdentifier GetTextureFactoryIdentifier() const {
    auto lock = mData.Lock();
    return lock.ref().mTextureFactoryIdentifier;
  }

  bool DeviceCanReset() const {
    return GetCompositorBackendType() != LayersBackend::LAYERS_BASIC;
  }

  int32_t GetSerial() const { return mSerial; }

  /**
   * Sends a synchronous ping to the compsoitor.
   *
   * This is bad for performance and should only be called as a last resort if
   * the compositor may be blocked for a long period of time, to avoid that the
   * content process accumulates resource allocations that the compositor is not
   * consuming and releasing.
   */
  virtual void SyncWithCompositor() { MOZ_ASSERT_UNREACHABLE("Unimplemented"); }

  /**
   * Helpers for finding other related interface. These are infallible.
   */
  virtual TextureForwarder* GetTextureForwarder() = 0;
  virtual LayersIPCActor* GetLayersIPCActor() = 0;
  virtual ActiveResourceTracker* GetActiveResourceTracker() {
    MOZ_ASSERT_UNREACHABLE("Unimplemented");
    return nullptr;
  }

 protected:
  struct SharedData {
    TextureFactoryIdentifier mTextureFactoryIdentifier;
    RefPtr<SyncObjectClient> mSyncObject;
  };
  mutable DataMutex<SharedData> mData;

  const int32_t mSerial;
  static mozilla::Atomic<int32_t> sSerialCounter;
};

/// Some implementations of KnowsCompositor can be used off their IPDL thread
/// like the ImageBridgeChild, and others just can't. Instead of passing them
/// we create a proxy KnowsCompositor that has information about compositor
/// backend but proxies allocations to the ImageBridge.
/// This is kind of specific to the needs of media which wants to allocate
/// textures, usually through the Image Bridge accessed by KnowsCompositor but
/// also wants access to the compositor backend information that ImageBridge
/// doesn't know about.
///
/// This is really a band aid to what turned into a class hierarchy horror show.
/// Hopefully we can come back and simplify this some way.
class KnowsCompositorMediaProxy : public KnowsCompositor {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(KnowsCompositorMediaProxy, override);

  explicit KnowsCompositorMediaProxy(
      const TextureFactoryIdentifier& aIdentifier);

  TextureForwarder* GetTextureForwarder() override;

  LayersIPCActor* GetLayersIPCActor() override;

  ActiveResourceTracker* GetActiveResourceTracker() override;

  void SyncWithCompositor() override;

 protected:
  virtual ~KnowsCompositorMediaProxy();

  RefPtr<ImageBridgeChild> mThreadSafeAllocator;
};

}  // namespace layers
}  // namespace mozilla

#endif
