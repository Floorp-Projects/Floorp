/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryBlockCache.h"

#include "MediaPrefs.h"
#include "mozilla/Atomics.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Services.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"
#include "prsystem.h"

namespace mozilla {

#undef LOG
LazyLogModule gMemoryBlockCacheLog("MemoryBlockCache");
#define LOG(x, ...)                                                            \
  MOZ_LOG(gMemoryBlockCacheLog, LogLevel::Debug, ("%p " x, this, ##__VA_ARGS__))

// Combined sizes of all MemoryBlockCache buffers.
// Initialized to 0 by non-local static initialization.
// Increases when a buffer grows (during initialization or unexpected OOB
// writes), decreases when a MemoryBlockCache (with its buffer) is destroyed.
static Atomic<size_t> gCombinedSizes;

class MemoryBlockCacheTelemetry final
  : public nsIObserver
  , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // To be called when the combined size has grown, so that the watermark may
  // be updated if needed.
  // Ensures MemoryBlockCache telemetry will be reported at shutdown.
  // Returns current watermark.
  static size_t NotifyCombinedSizeGrown(size_t aNewSize);

private:
  MemoryBlockCacheTelemetry() {}
  ~MemoryBlockCacheTelemetry() {}

  // Singleton instance created when a first MediaCache is registered, and
  // released when the last MediaCache is unregistered.
  // The observer service will keep a weak reference to it, for notifications.
  static StaticRefPtr<MemoryBlockCacheTelemetry> gMemoryBlockCacheTelemetry;

  // Watermark for the combined sizes; can only increase when a buffer grows.
  static Atomic<size_t> gCombinedSizesWatermark;
};

// Initialized to nullptr by non-local static initialization.
/* static */ StaticRefPtr<MemoryBlockCacheTelemetry>
  MemoryBlockCacheTelemetry::gMemoryBlockCacheTelemetry;

// Initialized to 0 by non-local static initialization.
/* static */ Atomic<size_t> MemoryBlockCacheTelemetry::gCombinedSizesWatermark;

NS_IMPL_ISUPPORTS(MemoryBlockCacheTelemetry,
                  nsIObserver,
                  nsISupportsWeakReference)

/* static */ size_t
MemoryBlockCacheTelemetry::NotifyCombinedSizeGrown(size_t aNewSize)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  // Ensure gMemoryBlockCacheTelemetry exists.
  if (!gMemoryBlockCacheTelemetry) {
    gMemoryBlockCacheTelemetry = new MemoryBlockCacheTelemetry();

    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService) {
      observerService->AddObserver(
        gMemoryBlockCacheTelemetry, "profile-change-teardown", true);
    }

    // Clearing gMemoryBlockCacheTelemetry when handling
    // "profile-change-teardown" could run the risk of re-creating it (and then
    // leaking it) if some MediaCache work happened after that notification.
    // So instead we just request it to be cleared on final shutdown.
    ClearOnShutdown(&gMemoryBlockCacheTelemetry);
  }

  // Update watermark if needed, report current watermark.
  for (;;) {
    size_t oldSize = gMemoryBlockCacheTelemetry->gCombinedSizesWatermark;
    if (aNewSize < oldSize) {
      return oldSize;
    }
    if (gMemoryBlockCacheTelemetry->gCombinedSizesWatermark.compareExchange(
          oldSize, aNewSize)) {
      return aNewSize;
    }
  }
}

NS_IMETHODIMP
MemoryBlockCacheTelemetry::Observe(nsISupports* aSubject,
                                   char const* aTopic,
                                   char16_t const* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");

  if (strcmp(aTopic, "profile-change-teardown") == 0) {
    uint32_t watermark = static_cast<uint32_t>(gCombinedSizesWatermark);
    LOG("MemoryBlockCacheTelemetry::~Observe() "
        "MEDIACACHE_MEMORY_WATERMARK=%" PRIu32,
        watermark);
    Telemetry::Accumulate(Telemetry::HistogramID::MEDIACACHE_MEMORY_WATERMARK,
                          watermark);
    return NS_OK;
  }
  return NS_OK;
}

enum MemoryBlockCacheTelemetryErrors
{
  // Don't change order/numbers! Add new values at the end and update
  // MEMORYBLOCKCACHE_ERRORS description in Histograms.json.
  InitUnderuse = 0,
  InitAllocation = 1,
  ReadOverrun = 2,
  WriteBlockOverflow = 3,
  WriteBlockCannotGrow = 4,
  MoveBlockSourceOverrun = 5,
  MoveBlockDestOverflow = 6,
  MoveBlockCannotGrow = 7,
};

MemoryBlockCache::MemoryBlockCache(int64_t aContentLength)
  // Buffer whole blocks.
  : mInitialContentLength((aContentLength >= 0) ? size_t(aContentLength) : 0)
  , mMutex("MemoryBlockCache")
  , mHasGrown(false)
{
  if (aContentLength <= 0) {
    LOG("MemoryBlockCache() MEMORYBLOCKCACHE_ERRORS='InitUnderuse'");
    Telemetry::Accumulate(Telemetry::HistogramID::MEMORYBLOCKCACHE_ERRORS,
                          InitUnderuse);
  }
}

MemoryBlockCache::~MemoryBlockCache()
{
  size_t sizes = static_cast<size_t>(gCombinedSizes -= mBuffer.Length());
  LOG("~MemoryBlockCache() - destroying buffer of size %zu; combined sizes now "
      "%zu",
      mBuffer.Length(),
      sizes);
}

bool
MemoryBlockCache::EnsureBufferCanContain(size_t aContentLength)
{
  mMutex.AssertCurrentThreadOwns();
  if (aContentLength == 0) {
    return true;
  }
  const size_t initialLength = mBuffer.Length();
  const size_t desiredLength =
    ((aContentLength - 1) / BLOCK_SIZE + 1) * BLOCK_SIZE;
  if (initialLength >= desiredLength) {
    // Already large enough.
    return true;
  }
  // Need larger buffer. If we are allowed more memory, attempt to re-allocate.
  const size_t extra = desiredLength - initialLength;
  // Note: There is a small race between testing `atomic + extra > limit` and
  // committing to it with `atomic += extra` below; but this is acceptable, as
  // in the worst case it may allow a small number of buffers to go past the
  // limit.
  // The alternative would have been to reserve the space first with
  // `atomic += extra` and then undo it with `atomic -= extra` in case of
  // failure; but this would have meant potentially preventing other (small but
  // successful) allocations.
  static const size_t sysmem =
    std::max<size_t>(PR_GetPhysicalMemorySize(), 32 * 1024 * 1024);
  const size_t limit = std::min(
    size_t(MediaPrefs::MediaMemoryCachesCombinedLimitKb()) * 1024,
    sysmem * MediaPrefs::MediaMemoryCachesCombinedLimitPcSysmem() / 100);
  const size_t currentSizes = static_cast<size_t>(gCombinedSizes);
  if (currentSizes + extra > limit) {
    LOG("EnsureBufferCanContain(%zu) - buffer size %zu, wanted + %zu = %zu;"
        " combined sizes %zu + %zu > limit %zu",
        aContentLength,
        initialLength,
        extra,
        desiredLength,
        currentSizes,
        extra,
        limit);
    return false;
  }
  if (!mBuffer.SetLength(desiredLength, mozilla::fallible)) {
    LOG("EnsureBufferCanContain(%zu) - buffer size %zu, wanted + %zu = %zu, "
        "allocation failed",
        aContentLength,
        initialLength,
        extra,
        desiredLength);
    return false;
  }
  MOZ_ASSERT(mBuffer.Length() == desiredLength);
  const size_t capacity = mBuffer.Capacity();
  const size_t extraCapacity = capacity - desiredLength;
  if (extraCapacity != 0) {
    // Our buffer was given a larger capacity than the requested length, we may
    // as well claim that extra capacity, both for our accounting, and to
    // possibly bypass some future growths that would fit in this new capacity.
    mBuffer.SetLength(capacity);
  }
  size_t newSizes =
    static_cast<size_t>(gCombinedSizes += (extra + extraCapacity));
  size_t watermark =
    MemoryBlockCacheTelemetry::NotifyCombinedSizeGrown(newSizes);
  LOG("EnsureBufferCanContain(%zu) - buffer size %zu + requested %zu + bonus "
      "%zu = %zu; combined "
      "sizes %zu, watermark %zu",
      aContentLength,
      initialLength,
      extra,
      extraCapacity,
      capacity,
      newSizes,
      watermark);
  mHasGrown = true;
  return true;
}

nsresult
MemoryBlockCache::Init()
{
  MutexAutoLock lock(mMutex);
  if (mBuffer.IsEmpty()) {
    LOG("Init()");
    // Attempt to pre-allocate buffer for expected content length.
    if (!EnsureBufferCanContain(mInitialContentLength)) {
      LOG("Init() MEMORYBLOCKCACHE_ERRORS='InitAllocation'");
      Telemetry::Accumulate(Telemetry::HistogramID::MEMORYBLOCKCACHE_ERRORS,
                            InitAllocation);
      return NS_ERROR_FAILURE;
    }
  } else {
    LOG("Init() again");
    // Re-initialization - Just erase data.
    MOZ_ASSERT(mBuffer.Length() >= mInitialContentLength);
    memset(mBuffer.Elements(), 0, mBuffer.Length());
  }
  // Ignore initial growth.
  mHasGrown = false;
  return NS_OK;
}

nsresult
MemoryBlockCache::WriteBlock(uint32_t aBlockIndex,
                             Span<const uint8_t> aData1,
                             Span<const uint8_t> aData2)
{
  MutexAutoLock lock(mMutex);

  size_t offset = BlockIndexToOffset(aBlockIndex);
  if (offset + aData1.Length() + aData2.Length() > mBuffer.Length() &&
      !mHasGrown) {
    LOG("WriteBlock() MEMORYBLOCKCACHE_ERRORS='WriteBlockOverflow'");
    Telemetry::Accumulate(Telemetry::HistogramID::MEMORYBLOCKCACHE_ERRORS,
                          WriteBlockOverflow);
  }
  if (!EnsureBufferCanContain(offset + aData1.Length() + aData2.Length())) {
    LOG("WriteBlock() MEMORYBLOCKCACHE_ERRORS='WriteBlockCannotGrow'");
    Telemetry::Accumulate(Telemetry::HistogramID::MEMORYBLOCKCACHE_ERRORS,
                          WriteBlockCannotGrow);
    return NS_ERROR_FAILURE;
  }

  memcpy(mBuffer.Elements() + offset, aData1.Elements(), aData1.Length());
  if (aData2.Length() > 0) {
    memcpy(mBuffer.Elements() + offset + aData1.Length(),
           aData2.Elements(),
           aData2.Length());
  }

  return NS_OK;
}

nsresult
MemoryBlockCache::Read(int64_t aOffset,
                       uint8_t* aData,
                       int32_t aLength,
                       int32_t* aBytes)
{
  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(aOffset >= 0);
  if (aOffset + aLength > int64_t(mBuffer.Length())) {
    LOG("Read() MEMORYBLOCKCACHE_ERRORS='ReadOverrun'");
    Telemetry::Accumulate(Telemetry::HistogramID::MEMORYBLOCKCACHE_ERRORS,
                          ReadOverrun);
    return NS_ERROR_FAILURE;
  }

  memcpy(aData, mBuffer.Elements() + aOffset, aLength);
  *aBytes = aLength;

  return NS_OK;
}

nsresult
MemoryBlockCache::MoveBlock(int32_t aSourceBlockIndex, int32_t aDestBlockIndex)
{
  MutexAutoLock lock(mMutex);

  size_t sourceOffset = BlockIndexToOffset(aSourceBlockIndex);
  size_t destOffset = BlockIndexToOffset(aDestBlockIndex);
  if (sourceOffset + BLOCK_SIZE > mBuffer.Length()) {
    LOG("MoveBlock() MEMORYBLOCKCACHE_ERRORS='MoveBlockSourceOverrun'");
    Telemetry::Accumulate(Telemetry::HistogramID::MEMORYBLOCKCACHE_ERRORS,
                          MoveBlockSourceOverrun);
    return NS_ERROR_FAILURE;
  }
  if (destOffset + BLOCK_SIZE > mBuffer.Length() && !mHasGrown) {
    LOG("MoveBlock() MEMORYBLOCKCACHE_ERRORS='MoveBlockDestOverflow'");
    Telemetry::Accumulate(Telemetry::HistogramID::MEMORYBLOCKCACHE_ERRORS,
                          MoveBlockDestOverflow);
  }
  if (!EnsureBufferCanContain(destOffset + BLOCK_SIZE)) {
    LOG("MoveBlock() MEMORYBLOCKCACHE_ERRORS='MoveBlockCannotGrow'");
    Telemetry::Accumulate(Telemetry::HistogramID::MEMORYBLOCKCACHE_ERRORS,
                          MoveBlockCannotGrow);
    return NS_ERROR_FAILURE;
  }

  memcpy(mBuffer.Elements() + destOffset,
         mBuffer.Elements() + sourceOffset,
         BLOCK_SIZE);

  return NS_OK;
}

} // End namespace mozilla.

// avoid redefined macro in unified build
#undef LOG
