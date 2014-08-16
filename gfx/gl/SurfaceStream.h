/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SURFACESTREAM_H_
#define SURFACESTREAM_H_

#include <stack>
#include <set>
#include "mozilla/Monitor.h"
#include "mozilla/Attributes.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/GenericRefCounted.h"
#include "mozilla/UniquePtr.h"
#include "SurfaceTypes.h"
#include "SharedSurface.h"

namespace mozilla {

namespace gl {
class GLContext;
class SharedSurface;
class SurfaceFactory;

// Owned by: ScreenBuffer
class SurfaceStream : public GenericAtomicRefCounted
{
public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SurfaceStream)
    typedef enum {
        MainThread,
        OffMainThread
    } OMTC;

    static SurfaceStreamType ChooseGLStreamType(OMTC omtc,
                                                bool preserveBuffer);

    static TemporaryRef<SurfaceStream> CreateForType(SurfaceStreamType type,
                                                     mozilla::gl::GLContext* glContext,
                                                     SurfaceStream* prevStream = nullptr);

    const SurfaceStreamType mType;

    mozilla::gl::GLContext* GLContext() const { return mGLContext; }


protected:
    // |mProd| is owned by us, but can be ripped away when
    // creating a new GLStream from this one.
    UniquePtr<SharedSurface> mProducer;
#ifdef DEBUG
    std::set<SharedSurface*> mSurfaces;
#endif
    UniquePtrQueue<SharedSurface> mScraps;
    mutable Monitor mMonitor;
    bool mIsAlive;

    // Do not use this. It exists solely so we can ref it in CanvasClientWebGL::Update()
    // before sent up to the compositor. You have been warned (Bug 894405)
    mozilla::gl::GLContext* mGLContext;

    // |previous| can be null, indicating this is the first one.
    // Otherwise, we pull in |mProd| from |previous| an our initial surface.
    SurfaceStream(SurfaceStreamType type, SurfaceStream* prevStream);

public:
    virtual ~SurfaceStream();

protected:
    // These functions below are helpers to make trading buffers around easier.
    // For instance, using Move(a,b) instead of normal assignment assures that
    // we are never leaving anything hanging around, keeping things very safe.
    void MoveTo(UniquePtr<SharedSurface>* slotFrom,
                UniquePtr<SharedSurface>* slotTo);
    void New(SurfaceFactory* factory, const gfx::IntSize& size,
             UniquePtr<SharedSurface>* surfSlot);
    void Delete(UniquePtr<SharedSurface>* surfSlot);
    void Recycle(SurfaceFactory* factory,
                 UniquePtr<SharedSurface>* surfSlot);

    // Surrender control of a surface, and return it for use elsewhere.
    UniquePtr<SharedSurface> Surrender(UniquePtr<SharedSurface>* surfSlot);
    // Absorb control of a surface from elsewhere, clears its old location.
    UniquePtr<SharedSurface> Absorb(UniquePtr<SharedSurface>* surfSlot);

    // For holding on to surfaces we don't need until we can return them to the
    // Producer's factory via SurfaceFactory::Recycle.
    // Not thread-safe.
    void Scrap(UniquePtr<SharedSurface>* surfSlot);

    // Not thread-safe.
    void RecycleScraps(SurfaceFactory* factory);

public:
    /* Note that ownership of the returned surfaces below
     * transfers to the caller.
     * SwapProd returns null on failure. Returning null doesn't mean nothing
     * happened, but rather that a surface allocation failed. After returning
     * null, we must be able to call SwapProducer again with better args
     * and have everything work again.
     * One common failure is asking for a too-large |size|.
     */
    virtual SharedSurface* SwapProducer(SurfaceFactory* factory,
                                        const gfx::IntSize& size) = 0;

    virtual SharedSurface* Resize(SurfaceFactory* factory, const gfx::IntSize& size);

    virtual bool CopySurfaceToProducer(SharedSurface* src, SurfaceFactory* factory) { MOZ_ASSERT(0); return false; }

protected:
    // SwapCons will return the same surface more than once,
    // if nothing new has been published.
    virtual SharedSurface* SwapConsumer_NoWait() = 0;

public:
    virtual SharedSurface* SwapConsumer();

    virtual void SurrenderSurfaces(UniquePtr<SharedSurface>* out_producer,
                                   UniquePtr<SharedSurface>* out_consumer) = 0;
};

// Not thread-safe. Don't use cross-threads.
class SurfaceStream_SingleBuffer
    : public SurfaceStream
{
protected:
    UniquePtr<SharedSurface> mConsumer; // Only present after resize-swap.

public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SurfaceStream_SingleBuffer)

    explicit SurfaceStream_SingleBuffer(SurfaceStream* prevStream);
    virtual ~SurfaceStream_SingleBuffer();

    /* Since we're non-OMTC, we know the order of execution here:
     * SwapProd gets called in UpdateSurface, followed by
     * SwapCons being called in Render.
     */
    virtual SharedSurface* SwapProducer(SurfaceFactory* factory,
                                        const gfx::IntSize& size) MOZ_OVERRIDE;

    virtual SharedSurface* SwapConsumer_NoWait() MOZ_OVERRIDE;

    virtual void SurrenderSurfaces(UniquePtr<SharedSurface>* out_producer,
                                   UniquePtr<SharedSurface>* out_consumer) MOZ_OVERRIDE;
};

// Our hero for preserveDrawingBuffer=true.
class SurfaceStream_TripleBuffer_Copy
    : public SurfaceStream
{
protected:
    UniquePtr<SharedSurface> mStaging;
    UniquePtr<SharedSurface> mConsumer;

public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SurfaceStream_TripleBuffer_Copy)

    explicit SurfaceStream_TripleBuffer_Copy(SurfaceStream* prevStream);
    virtual ~SurfaceStream_TripleBuffer_Copy();

    virtual SharedSurface* SwapProducer(SurfaceFactory* factory,
                                        const gfx::IntSize& size) MOZ_OVERRIDE;

    virtual SharedSurface* SwapConsumer_NoWait() MOZ_OVERRIDE;

    virtual void SurrenderSurfaces(UniquePtr<SharedSurface>* out_producer,
                                   UniquePtr<SharedSurface>* out_consumer) MOZ_OVERRIDE;
};


class SurfaceStream_TripleBuffer
    : public SurfaceStream
{
protected:
    UniquePtr<SharedSurface> mStaging;
    UniquePtr<SharedSurface> mConsumer;

    // Returns true if we were able to wait, false if not
    virtual void WaitForCompositor() {}

    // To support subclasses initializing the mType.
    SurfaceStream_TripleBuffer(SurfaceStreamType type, SurfaceStream* prevStream);

public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SurfaceStream_TripleBuffer)

    explicit SurfaceStream_TripleBuffer(SurfaceStream* prevStream);
    virtual ~SurfaceStream_TripleBuffer();

    virtual bool CopySurfaceToProducer(SharedSurface* src,
                                       SurfaceFactory* factory) MOZ_OVERRIDE;

private:
    // Common constructor code.
    void Init(SurfaceStream* prevStream);

public:
    // Done writing to prod, swap prod and staging
    virtual SharedSurface* SwapProducer(SurfaceFactory* factory,
                                        const gfx::IntSize& size) MOZ_OVERRIDE;

    virtual SharedSurface* SwapConsumer_NoWait() MOZ_OVERRIDE;

    virtual void SurrenderSurfaces(UniquePtr<SharedSurface>* out_producer,
                                   UniquePtr<SharedSurface>* out_consumer) MOZ_OVERRIDE;
};

class SurfaceStream_TripleBuffer_Async
    : public SurfaceStream_TripleBuffer
{
protected:
    virtual void WaitForCompositor() MOZ_OVERRIDE;

public:
    MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SurfaceStream_TripleBuffer_Async)

    explicit SurfaceStream_TripleBuffer_Async(SurfaceStream* prevStream);
    virtual ~SurfaceStream_TripleBuffer_Async();
};


} // namespace gl
} // namespace mozilla

#endif // SURFACESTREAM_H_
