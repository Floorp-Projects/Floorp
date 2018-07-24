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

namespace mozilla {
namespace layers {

class SyncObjectClient;
class TextureForwarder;
class LayersIPCActor;
class ImageBridgeChild;

/**
 * See ActiveResourceTracker below.
 */
class ActiveResource
{
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
class ActiveResourceTracker : public nsExpirationTracker<ActiveResource, 3>
{
public:
  ActiveResourceTracker(uint32_t aExpirationCycle, const char* aName,
                        nsIEventTarget* aEventTarget)
  : nsExpirationTracker(aExpirationCycle, aName, aEventTarget)
  {}

  virtual void NotifyExpired(ActiveResource* aResource) override
  {
    RemoveObject(aResource);
    aResource->NotifyInactive();
  }
};

/**
 * An abstract interface for classes that are tied to a specific Compositor across
 * IPDL and uses TextureFactoryIdentifier to describe this Compositor.
 */
class KnowsCompositor
{
public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  KnowsCompositor();
  ~KnowsCompositor();

  void IdentifyTextureHost(const TextureFactoryIdentifier& aIdentifier);

  SyncObjectClient* GetSyncObject() { return mSyncObject; }

  /// And by "thread-safe" here we merely mean "okay to hold strong references to
  /// from multiple threads". Not all methods actually are thread-safe.
  virtual bool IsThreadSafe() const { return true; }

  virtual RefPtr<KnowsCompositor> GetForMedia() { return RefPtr<KnowsCompositor>(this); }

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

  bool SupportsTextureDirectMapping() const
  {
    return mTextureFactoryIdentifier.mSupportsTextureDirectMapping;
  }

  bool SupportsD3D11() const
  {
    return GetCompositorBackendType() == layers::LayersBackend::LAYERS_D3D11 ||
           (GetCompositorBackendType() == layers::LayersBackend::LAYERS_WR && GetCompositorUseANGLE());
  }

  bool GetCompositorUseANGLE() const
  {
    return mTextureFactoryIdentifier.mCompositorUseANGLE;
  }

  bool GetCompositorUseDComp() const
  {
    return mTextureFactoryIdentifier.mCompositorUseDComp;
  }

  const TextureFactoryIdentifier& GetTextureFactoryIdentifier() const
  {
    return mTextureFactoryIdentifier;
  }

  bool DeviceCanReset() const
  {
    return GetCompositorBackendType() != LayersBackend::LAYERS_BASIC;
  }

  int32_t GetSerial() const { return mSerial; }

  /**
   * Sends a synchronous ping to the compsoitor.
   *
   * This is bad for performance and should only be called as a last resort if the
   * compositor may be blocked for a long period of time, to avoid that the content
   * process accumulates resource allocations that the compositor is not consuming
   * and releasing.
   */
  virtual void SyncWithCompositor()
  {
    MOZ_ASSERT_UNREACHABLE("Unimplemented");
  }

  /**
   * Helpers for finding other related interface. These are infallible.
   */
   virtual TextureForwarder* GetTextureForwarder() = 0;
  virtual LayersIPCActor* GetLayersIPCActor() = 0;
  virtual ActiveResourceTracker* GetActiveResourceTracker()
  {
    MOZ_ASSERT_UNREACHABLE("Unimplemented");
    return nullptr;
  }

protected:
  TextureFactoryIdentifier mTextureFactoryIdentifier;
  RefPtr<SyncObjectClient> mSyncObject;

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
class KnowsCompositorMediaProxy: public KnowsCompositor
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(KnowsCompositorMediaProxy, override);

  explicit KnowsCompositorMediaProxy(const TextureFactoryIdentifier& aIdentifier);

  virtual TextureForwarder* GetTextureForwarder() override;


  virtual LayersIPCActor* GetLayersIPCActor() override;

  virtual ActiveResourceTracker* GetActiveResourceTracker() override;

  virtual void SyncWithCompositor() override;

protected:
  virtual ~KnowsCompositorMediaProxy();

  RefPtr<ImageBridgeChild> mThreadSafeAllocator;
};

} // namespace layers
} // namespace mozilla

#endif
