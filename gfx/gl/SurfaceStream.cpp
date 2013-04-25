/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SurfaceStream.h"

#include "gfxPoint.h"
#include "SharedSurface.h"
#include "SurfaceFactory.h"
#include "GeckoProfiler.h"

namespace mozilla {
namespace gfx {

SurfaceStreamType
SurfaceStream::ChooseGLStreamType(SurfaceStream::OMTC omtc,
                                  bool preserveBuffer)
{
    if (omtc == SurfaceStream::OffMainThread) {
        if (preserveBuffer)
            return SurfaceStreamType::TripleBuffer_Copy;
        else
            return SurfaceStreamType::TripleBuffer_Async;
    } else {
        if (preserveBuffer)
            return SurfaceStreamType::SingleBuffer;
        else
            return SurfaceStreamType::TripleBuffer;
    }
}

SurfaceStream*
SurfaceStream::CreateForType(SurfaceStreamType type, SurfaceStream* prevStream)
{
    switch (type) {
        case SurfaceStreamType::SingleBuffer:
            return new SurfaceStream_SingleBuffer(prevStream);
        case SurfaceStreamType::TripleBuffer_Copy:
            return new SurfaceStream_TripleBuffer_Copy(prevStream);
        case SurfaceStreamType::TripleBuffer_Async:
            return new SurfaceStream_TripleBuffer_Async(prevStream);
        case SurfaceStreamType::TripleBuffer:
            return new SurfaceStream_TripleBuffer(prevStream);
        default:
            MOZ_NOT_REACHED("Invalid Type.");
            return nullptr;
    }
}

void
SurfaceStream::New(SurfaceFactory* factory, const gfxIntSize& size,
                   SharedSurface*& surf)
{
    MOZ_ASSERT(!surf);
    surf = factory->NewSharedSurface(size);

    if (surf)
        mSurfaces.insert(surf);
}

void
SurfaceStream::Recycle(SurfaceFactory* factory, SharedSurface*& surf)
{
    if (surf) {
        mSurfaces.erase(surf);
        factory->Recycle(surf);
    }
    MOZ_ASSERT(!surf);
}

void
SurfaceStream::Delete(SharedSurface*& surf)
{
    if (surf) {
        mSurfaces.erase(surf);
        delete surf;
        surf = nullptr;
    }
    MOZ_ASSERT(!surf);
}

SharedSurface*
SurfaceStream::Surrender(SharedSurface*& surf)
{
    SharedSurface* ret = surf;

    if (surf) {
        mSurfaces.erase(surf);
        surf = nullptr;
    }
    MOZ_ASSERT(!surf);

    return ret;
}

SharedSurface*
SurfaceStream::Absorb(SharedSurface*& surf)
{
    SharedSurface* ret = surf;

    if (surf) {
        mSurfaces.insert(surf);
        surf = nullptr;
    }
    MOZ_ASSERT(!surf);

    return ret;
}

void
SurfaceStream::Scrap(SharedSurface*& scrap)
{
    if (scrap) {
        mScraps.push(scrap);
        scrap = nullptr;
    }
    MOZ_ASSERT(!scrap);
}

void
SurfaceStream::RecycleScraps(SurfaceFactory* factory)
{
    while (!mScraps.empty()) {
        SharedSurface* cur = mScraps.top();
        mScraps.pop();

        Recycle(factory, cur);
    }
}



SurfaceStream::~SurfaceStream()
{
    Delete(mProducer);

    while (!mScraps.empty()) {
        SharedSurface* cur = mScraps.top();
        mScraps.pop();

        Delete(cur);
    }

    MOZ_ASSERT(mSurfaces.empty());
}

SharedSurface*
SurfaceStream::SwapConsumer()
{
    MOZ_ASSERT(mIsAlive);

    SharedSurface* ret = SwapConsumer_NoWait();
    if (!ret)
        return nullptr;

    if (!ret->WaitSync()) {
        return nullptr;
    }

    return ret;
}

SharedSurface*
SurfaceStream::Resize(SurfaceFactory* factory, const gfxIntSize& size)
{
    MonitorAutoLock lock(mMonitor);

    if (mProducer) {
        Scrap(mProducer);
    }

    New(factory, size, mProducer);
    return mProducer;
}

SurfaceStream_SingleBuffer::SurfaceStream_SingleBuffer(SurfaceStream* prevStream)
    : SurfaceStream(SurfaceStreamType::SingleBuffer, prevStream)
    , mConsumer(nullptr)
{
    if (!prevStream)
        return;

    SharedSurface* prevProducer = nullptr;
    SharedSurface* prevConsumer = nullptr;
    prevStream->SurrenderSurfaces(prevProducer, prevConsumer);

    if (prevConsumer == prevProducer)
        prevConsumer = nullptr;

    mProducer = Absorb(prevProducer);
    mConsumer = Absorb(prevConsumer);
}

SurfaceStream_SingleBuffer::~SurfaceStream_SingleBuffer()
{
    Delete(mConsumer);
}

void
SurfaceStream_SingleBuffer::SurrenderSurfaces(SharedSurface*& producer,
                                              SharedSurface*& consumer)
{
    mIsAlive = false;

    producer = Surrender(mProducer);
    consumer = Surrender(mConsumer);

    if (!consumer)
        consumer = producer;
}

SharedSurface*
SurfaceStream_SingleBuffer::SwapProducer(SurfaceFactory* factory,
                                         const gfxIntSize& size)
{
    MonitorAutoLock lock(mMonitor);
    if (mConsumer) {
        Recycle(factory, mConsumer);
    }

    if (mProducer) {
        // Fence now, before we start (maybe) juggling Prod around.
        mProducer->Fence();

        // Size mismatch means we need to squirrel the current Prod
        // into Cons, and leave Prod empty, so it gets a new surface below.
        bool needsNewBuffer = mProducer->Size() != size;

        // Even if we're the right size, if the type has changed, and we don't
        // need to preserve, we should switch out for (presumedly) better perf.
        if (mProducer->Type() != factory->Type() &&
            !factory->Caps().preserve)
        {
            needsNewBuffer = true;
        }

        if (needsNewBuffer) {
            Move(mProducer, mConsumer);
        }
    }

    // The old Prod (if there every was one) was invalid,
    // so we need a new one.
    if (!mProducer) {
        New(factory, size, mProducer);
    }

    return mProducer;
}

SharedSurface*
SurfaceStream_SingleBuffer::SwapConsumer_NoWait()
{
    MonitorAutoLock lock(mMonitor);

    // Use Cons, if present.
    // Otherwise, just use Prod directly.
    SharedSurface* toConsume = mConsumer;
    if (!toConsume)
        toConsume = mProducer;

    return toConsume;
}



SurfaceStream_TripleBuffer_Copy::SurfaceStream_TripleBuffer_Copy(SurfaceStream* prevStream)
    : SurfaceStream(SurfaceStreamType::TripleBuffer_Copy, prevStream)
    , mStaging(nullptr)
    , mConsumer(nullptr)
{
    if (!prevStream)
        return;

    SharedSurface* prevProducer = nullptr;
    SharedSurface* prevConsumer = nullptr;
    prevStream->SurrenderSurfaces(prevProducer, prevConsumer);

    if (prevConsumer == prevProducer)
      prevConsumer = nullptr;

    mProducer = Absorb(prevProducer);
    mConsumer = Absorb(prevConsumer);
}

SurfaceStream_TripleBuffer_Copy::~SurfaceStream_TripleBuffer_Copy()
{
    Delete(mStaging);
    Delete(mConsumer);
}

void
SurfaceStream_TripleBuffer_Copy::SurrenderSurfaces(SharedSurface*& producer,
                                                   SharedSurface*& consumer)
{
    mIsAlive = false;

    producer = Surrender(mProducer);
    consumer = Surrender(mConsumer);

    if (!consumer)
        consumer = Surrender(mStaging);
}

SharedSurface*
SurfaceStream_TripleBuffer_Copy::SwapProducer(SurfaceFactory* factory,
                                              const gfxIntSize& size)
{
    MonitorAutoLock lock(mMonitor);

    RecycleScraps(factory);
    if (mProducer) {
        if (mStaging && mStaging->Type() != factory->Type())
            Recycle(factory, mStaging);

        if (!mStaging)
            New(factory, mProducer->Size(), mStaging);

        if (!mStaging)
            return nullptr;

        SharedSurface::Copy(mProducer, mStaging, factory);
        // Fence now, before we start (maybe) juggling Prod around.
        mStaging->Fence();

        if (mProducer->Size() != size)
            Recycle(factory, mProducer);
    }

    // The old Prod (if there every was one) was invalid,
    // so we need a new one.
    if (!mProducer) {
        New(factory, size, mProducer);
    }

    return mProducer;
}


SharedSurface*
SurfaceStream_TripleBuffer_Copy::SwapConsumer_NoWait()
{
    MonitorAutoLock lock(mMonitor);

    if (mStaging) {
        Scrap(mConsumer);
        Move(mStaging, mConsumer);
    }

    return mConsumer;
}

void SurfaceStream_TripleBuffer::Init(SurfaceStream* prevStream)
{
    if (!prevStream)
        return;

    SharedSurface* prevProducer = nullptr;
    SharedSurface* prevConsumer = nullptr;
    prevStream->SurrenderSurfaces(prevProducer, prevConsumer);

    if (prevConsumer == prevProducer)
        prevConsumer = nullptr;

    mProducer = Absorb(prevProducer);
    mConsumer = Absorb(prevConsumer);
}


SurfaceStream_TripleBuffer::SurfaceStream_TripleBuffer(SurfaceStreamType type, SurfaceStream* prevStream)
    : SurfaceStream(type, prevStream)
    , mStaging(nullptr)
    , mConsumer(nullptr)
{
    SurfaceStream_TripleBuffer::Init(prevStream);
}

SurfaceStream_TripleBuffer::SurfaceStream_TripleBuffer(SurfaceStream* prevStream)
    : SurfaceStream(SurfaceStreamType::TripleBuffer, prevStream)
    , mStaging(nullptr)
    , mConsumer(nullptr)
{
    SurfaceStream_TripleBuffer::Init(prevStream);
}

SurfaceStream_TripleBuffer::~SurfaceStream_TripleBuffer()
{
    Delete(mStaging);
    Delete(mConsumer);
}

void
SurfaceStream_TripleBuffer::SurrenderSurfaces(SharedSurface*& producer,
                                              SharedSurface*& consumer)
{
    mIsAlive = false;

    producer = Surrender(mProducer);
    consumer = Surrender(mConsumer);

    if (!consumer)
        consumer = Surrender(mStaging);
}

SharedSurface*
SurfaceStream_TripleBuffer::SwapProducer(SurfaceFactory* factory,
                                         const gfxIntSize& size)
{
    PROFILER_LABEL("SurfaceStream_TripleBuffer", "SwapProducer");

    MonitorAutoLock lock(mMonitor);
    if (mProducer) {
        RecycleScraps(factory);

        // If WaitForCompositor succeeds, mStaging has moved to mConsumer.
        // If it failed, we might have to scrap it.
        if (mStaging && !WaitForCompositor())
            Scrap(mStaging);

        MOZ_ASSERT(!mStaging);
        Move(mProducer, mStaging);
        mStaging->Fence();
    }

    MOZ_ASSERT(!mProducer);
    New(factory, size, mProducer);

    return mProducer;
}

SharedSurface*
SurfaceStream_TripleBuffer::SwapConsumer_NoWait()
{
    MonitorAutoLock lock(mMonitor);
    if (mStaging) {
        Scrap(mConsumer);
        Move(mStaging, mConsumer);
        mMonitor.NotifyAll();
    }

    return mConsumer;
}

SurfaceStream_TripleBuffer_Async::SurfaceStream_TripleBuffer_Async(SurfaceStream* prevStream)
    : SurfaceStream_TripleBuffer(SurfaceStreamType::TripleBuffer_Async, prevStream)
{
}

SurfaceStream_TripleBuffer_Async::~SurfaceStream_TripleBuffer_Async()
{
}

bool
SurfaceStream_TripleBuffer_Async::WaitForCompositor()
{
    PROFILER_LABEL("SurfaceStream_TripleBuffer_Async", "WaitForCompositor");

    // We are assumed to be locked
    while (mStaging)
        mMonitor.Wait();

    return true;
}

} /* namespace gfx */
} /* namespace mozilla */
