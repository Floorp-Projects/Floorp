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
#include "gfxPoint.h"
#include "SurfaceTypes.h"

namespace mozilla {
namespace gfx {
class SharedSurface;
class SurfaceFactory;

// Owned by: ScreenBuffer
class SurfaceStream
{
public:
    typedef enum {
        MainThread,
        OffMainThread
    } OMTC;

    static SurfaceStreamType ChooseGLStreamType(OMTC omtc,
                                                bool preserveBuffer);

    static SurfaceStream* CreateForType(SurfaceStreamType type,
                                        SurfaceStream* prevStream = nullptr);

    SurfaceStreamHandle GetShareHandle() {
        return reinterpret_cast<SurfaceStreamHandle>(this);
    }

    static SurfaceStream* FromHandle(SurfaceStreamHandle handle) {
        return (SurfaceStream*)handle;
    }

    const SurfaceStreamType mType;
protected:
    // |mProd| is owned by us, but can be ripped away when
    // creating a new GLStream from this one.
    SharedSurface* mProducer;
    std::set<SharedSurface*> mSurfaces;
    std::stack<SharedSurface*> mScraps;
    mutable Monitor mMonitor;
    bool mIsAlive;

    // |previous| can be null, indicating this is the first one.
    // Otherwise, we pull in |mProd| from |previous| an our initial surface.
    SurfaceStream(SurfaceStreamType type, SurfaceStream* prevStream)
        : mType(type)
        , mProducer(nullptr)
        , mMonitor("SurfaceStream monitor")
        , mIsAlive(true)
    {
        MOZ_ASSERT(!prevStream || mType != prevStream->mType,
                   "We should not need to create a SurfaceStream from another "
                   "of the same type.");
    }

public:
    virtual ~SurfaceStream();

protected:
    // These functions below are helpers to make trading buffers around easier.
    // For instance, using Move(a,b) instead of normal assignment assures that
    // we are never leaving anything hanging around, keeping things very safe.
    static void Move(SharedSurface*& from, SharedSurface*& to) {
        MOZ_ASSERT(!to);
        to = from;
        from = nullptr;
    }

    void New(SurfaceFactory* factory, const gfxIntSize& size,
             SharedSurface*& surf);
    void Delete(SharedSurface*& surf);
    void Recycle(SurfaceFactory* factory, SharedSurface*& surf);

    // Surrender control of a surface, and return it for use elsewhere.
    SharedSurface* Surrender(SharedSurface*& surf);
    // Absorb control of a surface from elsewhere, clears its old location.
    SharedSurface* Absorb(SharedSurface*& surf);

    // For holding on to surfaces we don't need until we can return them to the
    // Producer's factory via SurfaceFactory::Recycle.
    // Not thread-safe.
    void Scrap(SharedSurface*& scrap);

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
                                        const gfxIntSize& size) = 0;

    virtual SharedSurface* Resize(SurfaceFactory* factory, const gfxIntSize& size);

protected:
    // SwapCons will return the same surface more than once,
    // if nothing new has been published.
    virtual SharedSurface* SwapConsumer_NoWait() = 0;

public:
    virtual SharedSurface* SwapConsumer();

    virtual void SurrenderSurfaces(SharedSurface*& producer, SharedSurface*& consumer) = 0;
};

// Not thread-safe. Don't use cross-threads.
class SurfaceStream_SingleBuffer
    : public SurfaceStream
{
protected:
    SharedSurface* mConsumer; // Only present after resize-swap.

public:
    SurfaceStream_SingleBuffer(SurfaceStream* prevStream);
    virtual ~SurfaceStream_SingleBuffer();

    /* Since we're non-OMTC, we know the order of execution here:
     * SwapProd gets called in UpdateSurface, followed by
     * SwapCons being called in Render.
     */
    virtual SharedSurface* SwapProducer(SurfaceFactory* factory,
                                        const gfxIntSize& size);

    virtual SharedSurface* SwapConsumer_NoWait();

    virtual void SurrenderSurfaces(SharedSurface*& producer, SharedSurface*& consumer);
};

// Our hero for preserveDrawingBuffer=true.
class SurfaceStream_TripleBuffer_Copy
    : public SurfaceStream
{
protected:
    SharedSurface* mStaging;
    SharedSurface* mConsumer;

public:
    SurfaceStream_TripleBuffer_Copy(SurfaceStream* prevStream);
    virtual ~SurfaceStream_TripleBuffer_Copy();

    virtual SharedSurface* SwapProducer(SurfaceFactory* factory,
                                        const gfxIntSize& size);

    virtual SharedSurface* SwapConsumer_NoWait();

    virtual void SurrenderSurfaces(SharedSurface*& producer, SharedSurface*& consumer);
};


class SurfaceStream_TripleBuffer
    : public SurfaceStream
{
protected:
    SharedSurface* mStaging;
    SharedSurface* mConsumer;

    // Returns true if we were able to wait, false if not
    virtual bool WaitForCompositor() { return false; }

    // To support subclasses initializing the mType.
    SurfaceStream_TripleBuffer(SurfaceStreamType type, SurfaceStream* prevStream);

public:
    SurfaceStream_TripleBuffer(SurfaceStream* prevStream);
    virtual ~SurfaceStream_TripleBuffer();

private:
    // Common constructor code.
    void Init(SurfaceStream* prevStream);

public:
    // Done writing to prod, swap prod and staging
    virtual SharedSurface* SwapProducer(SurfaceFactory* factory,
                                        const gfxIntSize& size);

    virtual SharedSurface* SwapConsumer_NoWait();

    virtual void SurrenderSurfaces(SharedSurface*& producer, SharedSurface*& consumer);
};

class SurfaceStream_TripleBuffer_Async
    : public SurfaceStream_TripleBuffer
{
protected:
    virtual bool WaitForCompositor();

public:
    SurfaceStream_TripleBuffer_Async(SurfaceStream* prevStream);
    virtual ~SurfaceStream_TripleBuffer_Async();
};


} /* namespace gfx */
} /* namespace mozilla */

#endif /* SURFACESTREAM_H_ */
