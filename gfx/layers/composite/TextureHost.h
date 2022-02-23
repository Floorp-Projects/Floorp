/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREHOST_H
#define MOZILLA_GFX_TEXTUREHOST_H

#include <stddef.h>              // for size_t
#include <stdint.h>              // for uint64_t, uint32_t, uint8_t
#include "mozilla/Assertions.h"  // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"  // for override
#include "mozilla/RefPtr.h"      // for RefPtr, already_AddRefed, etc
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Point.h"  // for IntSize, IntPoint
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"  // for SurfaceFormat, etc
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags, etc
#include "mozilla/layers/LayersTypes.h"      // for LayerRenderState, etc
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/TextureSourceProvider.h"
#include "mozilla/mozalloc.h"  // for operator delete
#include "mozilla/Range.h"
#include "mozilla/UniquePtr.h"  // for UniquePtr
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsCOMPtr.h"         // for already_AddRefed
#include "nsDebug.h"          // for NS_WARNING
#include "nsISupportsImpl.h"  // for MOZ_COUNT_CTOR, etc
#include "nsRect.h"
#include "nsRegion.h"       // for nsIntRegion
#include "nsTraceRefcnt.h"  // for MOZ_COUNT_CTOR, etc
#include "nscore.h"         // for nsACString
#include "mozilla/layers/AtomicRefCountedWithFinalize.h"

class MacIOSurface;
namespace mozilla {
namespace gfx {
class DataSourceSurface;
}

namespace ipc {
class Shmem;
}  // namespace ipc

namespace wr {
class DisplayListBuilder;
class TransactionBuilder;
}  // namespace wr

namespace layers {

class AndroidHardwareBuffer;
class AndroidHardwareBufferTextureHost;
class BufferDescriptor;
class BufferTextureHost;
class Compositor;
class CompositableParentManager;
class ReadLockDescriptor;
class CompositorBridgeParent;
class SurfaceDescriptor;
class HostIPCAllocator;
class ISurfaceAllocator;
class MacIOSurfaceTextureHostOGL;
class SurfaceTextureHost;
class TextureHostOGL;
class TextureReadLock;
class TextureSourceOGL;
class TextureSourceD3D11;
class DataTextureSource;
class PTextureParent;
class TextureParent;
class WebRenderTextureHost;
class WrappingTextureSourceYCbCrBasic;

/**
 * A view on a TextureHost where the texture is internally represented as tiles
 * (contrast with a tiled buffer, where each texture is a tile). For iteration
 * by the texture's buffer host. This is only useful when the underlying surface
 * is too big to fit in one device texture, which forces us to split it in
 * smaller parts. Tiled Compositable is a different thing.
 */
class BigImageIterator {
 public:
  virtual void BeginBigImageIteration() = 0;
  virtual void EndBigImageIteration(){};
  virtual gfx::IntRect GetTileRect() = 0;
  virtual size_t GetTileCount() = 0;
  virtual bool NextTile() = 0;
};

/**
 * TextureSource is the interface for texture objects that can be composited
 * by a given compositor backend. Since the drawing APIs are different
 * between backends, the TextureSource interface is split into different
 * interfaces (TextureSourceOGL, etc.), and TextureSource mostly provide
 * access to these interfaces.
 *
 * This class is used on the compositor side.
 */
class TextureSource : public RefCounted<TextureSource> {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(TextureSource)

  TextureSource();

  virtual ~TextureSource();

  virtual const char* Name() const = 0;

  /**
   * Should be overridden in order to deallocate the data that is associated
   * with the rendering backend, such as GL textures.
   */
  virtual void DeallocateDeviceData() {}

  /**
   * Return the size of the texture in texels.
   * If this is a tile iterator, GetSize must return the size of the current
   * tile.
   */
  virtual gfx::IntSize GetSize() const = 0;

  /**
   * Return the pixel format of this texture
   */
  virtual gfx::SurfaceFormat GetFormat() const {
    return gfx::SurfaceFormat::UNKNOWN;
  }

  /**
   * Cast to a TextureSource for for each backend..
   */
  virtual TextureSourceOGL* AsSourceOGL() {
    gfxCriticalNote << "Failed to cast " << Name()
                    << " into a TextureSourceOGL";
    return nullptr;
  }
  virtual TextureSourceD3D11* AsSourceD3D11() { return nullptr; }
  /**
   * Cast to a DataTextureSurce.
   */
  virtual DataTextureSource* AsDataTextureSource() { return nullptr; }

  /**
   * Overload this if the TextureSource supports big textures that don't fit in
   * one device texture and must be tiled internally.
   */
  virtual BigImageIterator* AsBigImageIterator() { return nullptr; }

  virtual void Unbind() {}

  void SetNextSibling(TextureSource* aTexture) { mNextSibling = aTexture; }

  TextureSource* GetNextSibling() const { return mNextSibling; }

  /**
   * In some rare cases we currently need to consider a group of textures as one
   * TextureSource, that can be split in sub-TextureSources.
   */
  TextureSource* GetSubSource(int index) {
    switch (index) {
      case 0:
        return this;
      case 1:
        return GetNextSibling();
      case 2:
        return GetNextSibling() ? GetNextSibling()->GetNextSibling() : nullptr;
    }
    return nullptr;
  }

  void AddCompositableRef() { ++mCompositableCount; }

  void ReleaseCompositableRef() {
    --mCompositableCount;
    MOZ_ASSERT(mCompositableCount >= 0);
  }

  // When iterating as a BigImage, this creates temporary TextureSources
  // wrapping individual tiles.
  virtual RefPtr<TextureSource> ExtractCurrentTile() {
    NS_WARNING("Implementation does not expose tile sources");
    return nullptr;
  }

  int NumCompositableRefs() const { return mCompositableCount; }

  // The direct-map cpu buffer should be alive when gpu uses it. And it
  // should not be updated while gpu reads it. This Sync() function
  // implements this synchronized behavior by allowing us to check if
  // the GPU is done with the texture, and block on it if aBlocking is
  // true.
  virtual bool Sync(bool aBlocking) { return true; }

 protected:
  RefPtr<TextureSource> mNextSibling;
  int mCompositableCount;
};

/// Equivalent of a RefPtr<TextureSource>, that calls AddCompositableRef and
/// ReleaseCompositableRef in addition to the usual AddRef and Release.
///
/// The semantoics of these CompositableTextureRefs are important because they
/// are used both as a synchronization/safety mechanism, and as an optimization
/// mechanism. They are also tricky and subtle because we use them in a very
/// implicit way (assigning to a CompositableTextureRef is less visible than
/// explicitly calling a method or whatnot).
/// It is Therefore important to be careful about the way we use this tool.
///
/// CompositableTextureRef is a mechanism that lets us count how many
/// compositables are using a given texture (for TextureSource and TextureHost).
/// We use it to run specific code when a texture is not used anymore, and also
/// we trigger fast paths on some operations when we can see that the texture's
/// CompositableTextureRef counter is equal to 1 (the texture is not shared
/// between compositables).
/// This means that it is important to observe the following rules:
/// * CompositableHosts that receive UseTexture and similar messages *must*
/// store all of the TextureHosts they receive in CompositableTextureRef slots
/// for as long as they may be using them.
/// * CompositableHosts must store each texture in a *single*
/// CompositableTextureRef slot to ensure that the counter properly reflects how
/// many compositables are using the texture. If a compositable needs to hold
/// two references to a given texture (for example to have a pointer to the
/// current texture in a list of textures that may be used), it can hold its
/// extra references with RefPtr or whichever pointer type makes sense.
template <typename T>
class CompositableTextureRef {
 public:
  CompositableTextureRef() = default;

  explicit CompositableTextureRef(const CompositableTextureRef& aOther) {
    *this = aOther;
  }

  explicit CompositableTextureRef(T* aOther) { *this = aOther; }

  ~CompositableTextureRef() {
    if (mRef) {
      mRef->ReleaseCompositableRef();
    }
  }

  CompositableTextureRef& operator=(const CompositableTextureRef& aOther) {
    if (aOther.get()) {
      aOther->AddCompositableRef();
    }
    if (mRef) {
      mRef->ReleaseCompositableRef();
    }
    mRef = aOther.get();
    return *this;
  }

  CompositableTextureRef& operator=(T* aOther) {
    if (aOther) {
      aOther->AddCompositableRef();
    }
    if (mRef) {
      mRef->ReleaseCompositableRef();
    }
    mRef = aOther;
    return *this;
  }

  T* get() const { return mRef; }
  operator T*() const { return mRef; }
  T* operator->() const { return mRef; }
  T& operator*() const { return *mRef; }

 private:
  RefPtr<T> mRef;
};

typedef CompositableTextureRef<TextureSource> CompositableTextureSourceRef;
typedef CompositableTextureRef<TextureHost> CompositableTextureHostRef;

/**
 * Interface for TextureSources that can be updated from a DataSourceSurface.
 *
 * All backend should implement at least one DataTextureSource.
 */
class DataTextureSource : public TextureSource {
 public:
  DataTextureSource() : mOwner(0), mUpdateSerial(0) {}

  const char* Name() const override { return "DataTextureSource"; }

  DataTextureSource* AsDataTextureSource() override { return this; }

  /**
   * Upload a (portion of) surface to the TextureSource.
   *
   * The DataTextureSource doesn't own aSurface, although it owns and manage
   * the device texture it uploads to internally.
   */
  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr,
                      gfx::IntPoint* aDstOffset = nullptr) = 0;

  /**
   * A facility to avoid reuploading when it is not necessary.
   * The caller of Update can use GetUpdateSerial to see if the number has
   * changed since last update, and call SetUpdateSerial after each successful
   * update. The caller is responsible for managing the update serial except
   * when the texture data is deallocated in which case the TextureSource should
   * always reset the update serial to zero.
   */
  uint32_t GetUpdateSerial() const { return mUpdateSerial; }
  void SetUpdateSerial(uint32_t aValue) { mUpdateSerial = aValue; }

  // By default at least set the update serial to zero.
  // overloaded versions should do that too.
  void DeallocateDeviceData() override { SetUpdateSerial(0); }

#ifdef DEBUG
  /**
   * Provide read access to the data as a DataSourceSurface.
   *
   * This is expected to be very slow and should be used for mostly debugging.
   * XXX - implement everywhere and make it pure virtual.
   */
  virtual already_AddRefed<gfx::DataSourceSurface> ReadBack() {
    return nullptr;
  };
#endif

  void SetOwner(TextureHost* aOwner) {
    auto newOwner = (uintptr_t)aOwner;
    if (newOwner != mOwner) {
      mOwner = newOwner;
      SetUpdateSerial(0);
    }
  }

  bool IsOwnedBy(TextureHost* aOwner) const {
    return mOwner == (uintptr_t)aOwner;
  }

  bool HasOwner() const { return !IsOwnedBy(nullptr); }

 private:
  // We store mOwner as an integer rather than as a pointer to make it clear
  // it is not intended to be dereferenced.
  uintptr_t mOwner;
  uint32_t mUpdateSerial;
};

/**
 * TextureHost is a thin abstraction over texture data that need to be shared
 * between the content process and the compositor process. It is the
 * compositor-side half of a TextureClient/TextureHost pair. A corresponding
 * TextureClient lives on the content-side.
 *
 * TextureHost only knows how to deserialize or synchronize generic image data
 * (SurfaceDescriptor) and provide access to one or more TextureSource objects
 * (these provide the necessary APIs for compositor backends to composite the
 * image).
 *
 * A TextureHost implementation corresponds to one SurfaceDescriptor type, as
 * opposed to TextureSource that corresponds to device textures.
 * This means that for YCbCr planes, even though they are represented as
 * 3 textures internally (3 TextureSources), we use 1 TextureHost and not 3,
 * because the 3 planes are stored in the same buffer of shared memory, before
 * they are uploaded separately.
 *
 * There is always one and only one TextureHost per TextureClient, and the
 * TextureClient/Host pair only owns one buffer of image data through its
 * lifetime. This means that the lifetime of the underlying shared data
 * matches the lifetime of the TextureClient/Host pair. It also means
 * TextureClient/Host do not implement double buffering, which is the
 * reponsibility of the compositable (which would use two Texture pairs).
 *
 * The Lock/Unlock mecanism here mirrors Lock/Unlock in TextureClient.
 *
 */
class TextureHost : public AtomicRefCountedWithFinalize<TextureHost> {
  /**
   * Called once, just before the destructor.
   *
   * Here goes the shut-down code that uses virtual methods.
   * Must only be called by Release().
   */
  void Finalize();

  friend class AtomicRefCountedWithFinalize<TextureHost>;

 public:
  explicit TextureHost(TextureFlags aFlags);

 protected:
  virtual ~TextureHost();

 public:
  /**
   * Factory method.
   */
  static already_AddRefed<TextureHost> Create(
      const SurfaceDescriptor& aDesc, ReadLockDescriptor&& aReadLock,
      ISurfaceAllocator* aDeallocator, LayersBackend aBackend,
      TextureFlags aFlags, wr::MaybeExternalImageId& aExternalImageId);

  /**
   * Lock the texture host for compositing without using compositor.
   */
  virtual bool LockWithoutCompositor() { return true; }
  /**
   * Similar to Unlock(), but it should be called with LockWithoutCompositor().
   */
  virtual void UnlockWithoutCompositor() {}

  /**
   * Note that the texture host format can be different from its corresponding
   * texture source's. For example a ShmemTextureHost can have the ycbcr
   * format and produce 3 "alpha" textures sources.
   */
  virtual gfx::SurfaceFormat GetFormat() const = 0;
  /**
   * Return the format used for reading the texture.
   * Apple's YCBCR_422 is R8G8B8X8.
   */
  virtual gfx::SurfaceFormat GetReadFormat() const { return GetFormat(); }

  virtual gfx::YUVColorSpace GetYUVColorSpace() const {
    return gfx::YUVColorSpace::Identity;
  }

  /**
   * Return the color depth of the image. Used with YUV textures.
   */
  virtual gfx::ColorDepth GetColorDepth() const {
    return gfx::ColorDepth::COLOR_8;
  }

  /**
   * Return true if using full range values (0-255 if 8 bits YUV). Used with YUV
   * textures.
   */
  virtual gfx::ColorRange GetColorRange() const {
    return gfx::ColorRange::LIMITED;
  }

  /**
   * Called when another TextureHost will take over.
   */
  virtual void UnbindTextureSource();

  virtual bool IsValid() { return true; }

  /**
   * Should be overridden in order to deallocate the data that is associated
   * with the rendering backend, such as GL textures.
   */
  virtual void DeallocateDeviceData() {}

  /**
   * Should be overridden in order to deallocate the data that is shared with
   * the content side, such as shared memory.
   */
  virtual void DeallocateSharedData() {}

  /**
   * Should be overridden in order to force the TextureHost to drop all
   * references to it's shared data.
   *
   * This is important to ensure the correctness of the deallocation protocol.
   */
  virtual void ForgetSharedData() {}

  virtual gfx::IntSize GetSize() const = 0;

  /**
   * Should be overridden if TextureHost supports crop rect.
   */
  virtual void SetCropRect(nsIntRect aCropRect) {}

  /**
   * Debug facility.
   * XXX - cool kids use Moz2D. See bug 882113.
   */
  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() = 0;

  /**
   * XXX - Flags should only be set at creation time, this will be removed.
   */
  void SetFlags(TextureFlags aFlags) { mFlags = aFlags; }

  /**
   * XXX - Flags should only be set at creation time, this will be removed.
   */
  void AddFlag(TextureFlags aFlag) { mFlags |= aFlag; }

  TextureFlags GetFlags() { return mFlags; }

  /**
   * Allocate and deallocate a TextureParent actor.
   *
   * TextureParent< is an implementation detail of TextureHost that is not
   * exposed to the rest of the code base. CreateIPDLActor and DestroyIPDLActor
   * are for use with the managing IPDL protocols only (so that they can
   * implement AllocPTextureParent and DeallocPTextureParent).
   */
  static PTextureParent* CreateIPDLActor(
      HostIPCAllocator* aAllocator, const SurfaceDescriptor& aSharedData,
      ReadLockDescriptor&& aDescriptor, LayersBackend aLayersBackend,
      TextureFlags aFlags, uint64_t aSerial,
      const wr::MaybeExternalImageId& aExternalImageId);
  static bool DestroyIPDLActor(PTextureParent* actor);

  /**
   * Destroy the TextureChild/Parent pair.
   */
  static bool SendDeleteIPDLActor(PTextureParent* actor);

  static void ReceivedDestroy(PTextureParent* actor);

  /**
   * Get the TextureHost corresponding to the actor passed in parameter.
   */
  static TextureHost* AsTextureHost(PTextureParent* actor);

  static uint64_t GetTextureSerial(PTextureParent* actor);

  /**
   * Return a pointer to the IPDLActor.
   *
   * This is to be used with IPDL messages only. Do not store the returned
   * pointer.
   */
  PTextureParent* GetIPDLActor();

  // If a texture host holds a reference to shmem, it should override this
  // method to forget about the shmem _without_ releasing it.
  virtual void OnShutdown() {}

  // Forget buffer actor. Used only for hacky fix for bug 966446.
  virtual void ForgetBufferActor() {}

  virtual const char* Name() { return "TextureHost"; }

  /**
   * Returns true if the TextureHost can be released before the rendering is
   * completed, otherwise returns false.
   */
  virtual bool NeedsDeferredDeletion() const { return true; }

  void AddCompositableRef() {
    ++mCompositableCount;
    if (mCompositableCount == 1) {
      PrepareForUse();
    }
  }

  void ReleaseCompositableRef() {
    --mCompositableCount;
    MOZ_ASSERT(mCompositableCount >= 0);
    if (mCompositableCount == 0) {
      UnbindTextureSource();
      // Send mFwdTransactionId to client side if necessary.
      NotifyNotUsed();
    }
  }

  int NumCompositableRefs() const { return mCompositableCount; }

  void SetLastFwdTransactionId(uint64_t aTransactionId);

  void DeserializeReadLock(ReadLockDescriptor&& aDesc,
                           ISurfaceAllocator* aAllocator);
  void SetReadLocked();

  TextureReadLock* GetReadLock() { return mReadLock; }

  virtual BufferTextureHost* AsBufferTextureHost() { return nullptr; }
  virtual MacIOSurfaceTextureHostOGL* AsMacIOSurfaceTextureHost() {
    return nullptr;
  }
  virtual WebRenderTextureHost* AsWebRenderTextureHost() { return nullptr; }
  virtual SurfaceTextureHost* AsSurfaceTextureHost() { return nullptr; }
  virtual AndroidHardwareBufferTextureHost*
  AsAndroidHardwareBufferTextureHost() {
    return nullptr;
  }

  // Create the corresponding RenderTextureHost type of this texture, and
  // register the RenderTextureHost into render thread.
  virtual void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) {
    MOZ_RELEASE_ASSERT(
        false,
        "No CreateRenderTexture() implementation for this TextureHost type.");
  }

  void EnsureRenderTexture(const wr::MaybeExternalImageId& aExternalImageId);

  // Destroy RenderTextureHost when it was created by the TextureHost.
  // It is called in TextureHost::Finalize().
  virtual void MaybeDestroyRenderTexture();

  static void DestroyRenderTexture(const wr::ExternalImageId& aExternalImageId);

  /// Returns the number of actual textures that will be used to render this.
  /// For example in a lot of YUV cases it will be 3
  virtual uint32_t NumSubTextures() { return 1; }

  enum ResourceUpdateOp {
    ADD_IMAGE,
    UPDATE_IMAGE,
  };

  // Add all necessary TextureHost informations to the resource update queue.
  virtual void PushResourceUpdates(wr::TransactionBuilder& aResources,
                                   ResourceUpdateOp aOp,
                                   const Range<wr::ImageKey>& aImageKeys,
                                   const wr::ExternalImageId& aExtID) {
    MOZ_ASSERT_UNREACHABLE("Unimplemented");
  }

  enum class PushDisplayItemFlag {
    // Passed if the caller wants these display items to be promoted
    // to compositor surfaces if possible.
    PREFER_COMPOSITOR_SURFACE,

    // Passed in the RenderCompositor supports BufferTextureHosts
    // being used directly as external compositor surfaces.
    SUPPORTS_EXTERNAL_BUFFER_TEXTURES,
  };
  using PushDisplayItemFlagSet = EnumSet<PushDisplayItemFlag>;

  // Put all necessary WR commands into DisplayListBuilder for this textureHost
  // rendering.
  virtual void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                                const wr::LayoutRect& aBounds,
                                const wr::LayoutRect& aClip,
                                wr::ImageRendering aFilter,
                                const Range<wr::ImageKey>& aKeys,
                                PushDisplayItemFlagSet aFlags) {
    MOZ_ASSERT_UNREACHABLE(
        "No PushDisplayItems() implementation for this TextureHost type.");
  }

  /**
   * Some API's can use the cross-process IOSurface directly, such as OpenVR
   */
  virtual MacIOSurface* GetMacIOSurface() { return nullptr; }

  virtual bool NeedsYFlip() const;

  virtual void SetAcquireFence(mozilla::ipc::FileDescriptor&& aFenceFd) {}

  virtual void SetReleaseFence(mozilla::ipc::FileDescriptor&& aFenceFd) {}

  virtual mozilla::ipc::FileDescriptor GetAndResetReleaseFence() {
    return mozilla::ipc::FileDescriptor();
  }

  virtual AndroidHardwareBuffer* GetAndroidHardwareBuffer() const {
    return nullptr;
  }

  virtual bool SupportsExternalCompositing(WebRenderBackend aBackend) {
    return false;
  }

  // Our WebRender backend may impose restrictions on whether textures are
  // prepared as native textures or not, or it may have no restriction at
  // all. This enumerates those possibilities.
  enum NativeTexturePolicy {
    REQUIRE,
    FORBID,
    DONT_CARE,
  };

  static NativeTexturePolicy BackendNativeTexturePolicy(
      layers::WebRenderBackend aBackend, gfx::IntSize aSize) {
    static const int32_t SWGL_DIMENSION_MAX = 1 << 15;
    if (aBackend == WebRenderBackend::SOFTWARE) {
      return (aSize.width <= SWGL_DIMENSION_MAX &&
              aSize.height <= SWGL_DIMENSION_MAX)
                 ? REQUIRE
                 : FORBID;
    }
    return DONT_CARE;
  }

 protected:
  virtual void ReadUnlock();

  void RecycleTexture(TextureFlags aFlags);

  /**
   * Called when mCompositableCount becomes from 0 to 1.
   */
  virtual void PrepareForUse() {}

  /**
   * Called when mCompositableCount becomes 0.
   */
  virtual void NotifyNotUsed();

  // for Compositor.
  void CallNotifyNotUsed();

  PTextureParent* mActor;
  RefPtr<TextureReadLock> mReadLock;
  TextureFlags mFlags;
  int mCompositableCount;
  uint64_t mFwdTransactionId;
  bool mReadLocked;
  wr::MaybeExternalImageId mExternalImageId;

  friend class Compositor;
  friend class TextureParent;
  friend class TextureSourceProvider;
  friend class GPUVideoTextureHost;
  friend class WebRenderTextureHost;
};

/**
 * TextureHost that wraps a random access buffer such as a Shmem or some raw
 * memory.
 *
 * This TextureHost is backend-independent and the backend-specific bits are
 * in the TextureSource.
 * This class must be inherited to implement GetBuffer and DeallocSharedData
 * (see ShmemTextureHost and MemoryTextureHost)
 *
 * Uploads happen when Lock is called.
 *
 * BufferTextureHost supports YCbCr and flavours of RGBA images (RGBX, A, etc.).
 */
class BufferTextureHost : public TextureHost {
 public:
  BufferTextureHost(const BufferDescriptor& aDescriptor, TextureFlags aFlags);

  virtual ~BufferTextureHost();

  virtual uint8_t* GetBuffer() = 0;

  virtual size_t GetBufferSize() = 0;

  void UnbindTextureSource() override;

  void DeallocateDeviceData() override;

  /**
   * Return the format that is exposed to the compositor when calling
   * BindTextureSource.
   *
   * If the shared format is YCbCr and the compositor does not support it,
   * GetFormat will be RGB32 (even though mFormat is SurfaceFormat::YUV).
   */
  gfx::SurfaceFormat GetFormat() const override;

  gfx::YUVColorSpace GetYUVColorSpace() const override;

  gfx::ColorDepth GetColorDepth() const override;

  gfx::ColorRange GetColorRange() const override;

  gfx::IntSize GetSize() const override { return mSize; }

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  bool NeedsDeferredDeletion() const override {
    return TextureHost::NeedsDeferredDeletion() || UseExternalTextures();
  }

  BufferTextureHost* AsBufferTextureHost() override { return this; }

  const BufferDescriptor& GetBufferDescriptor() const { return mDescriptor; }

  void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  uint32_t NumSubTextures() override;

  void PushResourceUpdates(wr::TransactionBuilder& aResources,
                           ResourceUpdateOp aOp,
                           const Range<wr::ImageKey>& aImageKeys,
                           const wr::ExternalImageId& aExtID) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys,
                        PushDisplayItemFlagSet aFlags) override;

  void DisableExternalTextures() { mUseExternalTextures = false; }

 protected:
  bool UseExternalTextures() const { return mUseExternalTextures; }

  BufferDescriptor mDescriptor;
  RefPtr<Compositor> mCompositor;
  gfx::IntSize mSize;
  gfx::SurfaceFormat mFormat;
  bool mLocked;
  bool mUseExternalTextures;

  class DataTextureSourceYCbCrBasic;
};

/**
 * TextureHost that wraps shared memory.
 * the corresponding texture on the client side is ShmemTextureClient.
 * This TextureHost is backend-independent.
 */
class ShmemTextureHost : public BufferTextureHost {
 public:
  ShmemTextureHost(const mozilla::ipc::Shmem& aShmem,
                   const BufferDescriptor& aDesc,
                   ISurfaceAllocator* aDeallocator, TextureFlags aFlags);

 protected:
  ~ShmemTextureHost();

 public:
  void DeallocateSharedData() override;

  void ForgetSharedData() override;

  uint8_t* GetBuffer() override;

  size_t GetBufferSize() override;

  const char* Name() override { return "ShmemTextureHost"; }

  void OnShutdown() override;

 protected:
  UniquePtr<mozilla::ipc::Shmem> mShmem;
  RefPtr<ISurfaceAllocator> mDeallocator;
};

/**
 * TextureHost that wraps raw memory.
 * The corresponding texture on the client side is MemoryTextureClient.
 * Can obviously not be used in a cross process setup.
 * This TextureHost is backend-independent.
 */
class MemoryTextureHost : public BufferTextureHost {
 public:
  MemoryTextureHost(uint8_t* aBuffer, const BufferDescriptor& aDesc,
                    TextureFlags aFlags);

 protected:
  ~MemoryTextureHost();

 public:
  void DeallocateSharedData() override;

  void ForgetSharedData() override;

  uint8_t* GetBuffer() override;

  size_t GetBufferSize() override;

  const char* Name() override { return "MemoryTextureHost"; }

 protected:
  uint8_t* mBuffer;
};

class MOZ_STACK_CLASS AutoLockTextureHostWithoutCompositor {
 public:
  explicit AutoLockTextureHostWithoutCompositor(TextureHost* aTexture)
      : mTexture(aTexture) {
    mLocked = mTexture ? mTexture->LockWithoutCompositor() : false;
  }

  ~AutoLockTextureHostWithoutCompositor() {
    if (mTexture && mLocked) {
      mTexture->UnlockWithoutCompositor();
    }
  }

  bool Failed() { return mTexture && !mLocked; }

 private:
  RefPtr<TextureHost> mTexture;
  bool mLocked;
};

/**
 * This can be used as an offscreen rendering target by the compositor, and
 * subsequently can be used as a source by the compositor.
 */
class CompositingRenderTarget : public TextureSource {
 public:
  explicit CompositingRenderTarget(const gfx::IntPoint& aOrigin)
      : mClearOnBind(false),
        mOrigin(aOrigin),
        mZNear(0),
        mZFar(0),
        mHasComplexProjection(false),
        mEnableDepthBuffer(false) {}
  virtual ~CompositingRenderTarget() = default;

  const char* Name() const override { return "CompositingRenderTarget"; }

#ifdef MOZ_DUMP_PAINTING
  virtual already_AddRefed<gfx::DataSourceSurface> Dump(
      Compositor* aCompositor) {
    return nullptr;
  }
#endif

  /**
   * Perform a clear when recycling a non opaque surface.
   * The clear is deferred to when the render target is bound.
   */
  void ClearOnBind() { mClearOnBind = true; }

  const gfx::IntPoint& GetOrigin() const { return mOrigin; }
  gfx::IntRect GetRect() { return gfx::IntRect(GetOrigin(), GetSize()); }

  /**
   * If a Projection matrix is set, then it is used for rendering to
   * this render target instead of generating one.  If no explicit
   * projection is set, Compositors are expected to generate an
   * orthogonal maaping that maps 0..1 to the full size of the render
   * target.
   */
  bool HasComplexProjection() const { return mHasComplexProjection; }
  void ClearProjection() { mHasComplexProjection = false; }
  void SetProjection(const gfx::Matrix4x4& aNewMatrix, bool aEnableDepthBuffer,
                     float aZNear, float aZFar) {
    mProjectionMatrix = aNewMatrix;
    mEnableDepthBuffer = aEnableDepthBuffer;
    mZNear = aZNear;
    mZFar = aZFar;
    mHasComplexProjection = true;
  }
  void GetProjection(gfx::Matrix4x4& aMatrix, bool& aEnableDepth, float& aZNear,
                     float& aZFar) {
    MOZ_ASSERT(mHasComplexProjection);
    aMatrix = mProjectionMatrix;
    aEnableDepth = mEnableDepthBuffer;
    aZNear = mZNear;
    aZFar = mZFar;
  }

 protected:
  bool mClearOnBind;

 private:
  gfx::IntPoint mOrigin;

  gfx::Matrix4x4 mProjectionMatrix;
  float mZNear, mZFar;
  bool mHasComplexProjection;
  bool mEnableDepthBuffer;
};

/**
 * Creates a TextureHost that can be used with any of the existing backends
 * Not all SurfaceDescriptor types are supported
 */
already_AddRefed<TextureHost> CreateBackendIndependentTextureHost(
    const SurfaceDescriptor& aDesc, ISurfaceAllocator* aDeallocator,
    LayersBackend aBackend, TextureFlags aFlags);

}  // namespace layers
}  // namespace mozilla

#endif
