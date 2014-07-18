/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComponentManagerUtils.h"
#include "nsITimer.h"
#include "RasterImage.h"
#include "DiscardTracker.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace image {

static const char* sDiscardTimeoutPref = "image.mem.min_discard_timeout_ms";

/* static */ LinkedList<DiscardTracker::Node> DiscardTracker::sDiscardableImages;
/* static */ nsCOMPtr<nsITimer> DiscardTracker::sTimer;
/* static */ bool DiscardTracker::sInitialized = false;
/* static */ bool DiscardTracker::sTimerOn = false;
/* static */ Atomic<bool> DiscardTracker::sDiscardRunnablePending(false);
/* static */ uint64_t DiscardTracker::sCurrentDecodedImageBytes = 0;
/* static */ uint32_t DiscardTracker::sMinDiscardTimeoutMs = 10000;
/* static */ uint32_t DiscardTracker::sMaxDecodedImageKB = 42 * 1024;
/* static */ uint32_t DiscardTracker::sHardLimitDecodedImageKB = 0;
/* static */ PRLock * DiscardTracker::sAllocationLock = nullptr;
/* static */ Mutex* DiscardTracker::sNodeListMutex = nullptr;
/* static */ Atomic<bool> DiscardTracker::sShutdown(false);

/*
 * When we notice we're using too much memory for decoded images, we enqueue a
 * DiscardRunnable, which runs this code.
 */
NS_IMETHODIMP
DiscardTracker::DiscardRunnable::Run()
{
  sDiscardRunnablePending = false;

  DiscardTracker::DiscardNow();
  return NS_OK;
}

void
DiscardTimeoutChangedCallback(const char* aPref, void *aClosure)
{
  DiscardTracker::ReloadTimeout();
}

nsresult
DiscardTracker::Reset(Node *node)
{
  // We shouldn't call Reset() with a null |img| pointer, on images which can't
  // be discarded, or on animated images (which should be marked as
  // non-discardable, anyway).
  MutexAutoLock lock(*sNodeListMutex);
  MOZ_ASSERT(sInitialized);
  MOZ_ASSERT(node->img);
  MOZ_ASSERT(node->img->CanDiscard());
  MOZ_ASSERT(!node->img->mAnim);

  // Insert the node at the front of the list and note when it was inserted.
  bool wasInList = node->isInList();
  if (wasInList) {
    node->remove();
  }
  node->timestamp = TimeStamp::Now();
  sDiscardableImages.insertFront(node);

  // If the node wasn't already in the list of discardable images, then we may
  // need to discard some images to stay under the sMaxDecodedImageKB limit.
  // Call MaybeDiscardSoon to do this check.
  if (!wasInList) {
    MaybeDiscardSoon();
  }

  // Make sure the timer is running.
  nsresult rv = EnableTimer();
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

void
DiscardTracker::Remove(Node *node)
{
  if (sShutdown) {
    // Already shutdown. List should be empty, so just return.
    return;
  }
  MutexAutoLock lock(*sNodeListMutex);

  if (node->isInList())
    node->remove();

  if (sDiscardableImages.isEmpty())
    DisableTimer();
}

/**
 * Shut down the tracker, deallocating the timer.
 */
void
DiscardTracker::Shutdown()
{
  sShutdown = true;

  if (sTimer) {
    sTimer->Cancel();
    sTimer = nullptr;
  }

  // Clear the sDiscardableImages linked list so that its destructor
  // (LinkedList.h) finds an empty array, which is required after bug 803688.
  DiscardAll();

  delete sNodeListMutex;
  sNodeListMutex = nullptr;
}

/*
 * Discard all the images we're tracking.
 */
void
DiscardTracker::DiscardAll()
{
  MutexAutoLock lock(*sNodeListMutex);

  if (!sInitialized)
    return;

  // Be careful: Calling Discard() on an image might cause it to be removed
  // from the list!
  Node *n;
  while ((n = sDiscardableImages.popFirst())) {
    n->img->Discard();
  }

  // The list is empty, so there's no need to leave the timer on.
  DisableTimer();
}

/* static */ bool
DiscardTracker::TryAllocation(uint64_t aBytes)
{
  MOZ_ASSERT(sInitialized);

  PR_Lock(sAllocationLock);
  bool enoughSpace =
    !sHardLimitDecodedImageKB ||
    (sHardLimitDecodedImageKB * 1024) - sCurrentDecodedImageBytes >= aBytes;

  if (enoughSpace) {
    sCurrentDecodedImageBytes += aBytes;
  }
  PR_Unlock(sAllocationLock);

  // If we're using too much memory for decoded images, MaybeDiscardSoon will
  // enqueue a callback to discard some images.
  MaybeDiscardSoon();

  return enoughSpace;
}

/* static */ void
DiscardTracker::InformDeallocation(uint64_t aBytes)
{
  // This function is called back e.g. from RasterImage::Discard(); be careful!

  MOZ_ASSERT(sInitialized);

  PR_Lock(sAllocationLock);
  MOZ_ASSERT(aBytes <= sCurrentDecodedImageBytes);
  sCurrentDecodedImageBytes -= aBytes;
  PR_Unlock(sAllocationLock);
}

/**
 * Initialize the tracker.
 */
nsresult
DiscardTracker::Initialize()
{
  // Watch the timeout pref for changes.
  Preferences::RegisterCallback(DiscardTimeoutChangedCallback,
                                sDiscardTimeoutPref);

  Preferences::AddUintVarCache(&sMaxDecodedImageKB,
                              "image.mem.max_decoded_image_kb",
                              50 * 1024);

  Preferences::AddUintVarCache(&sHardLimitDecodedImageKB,
                               "image.mem.hard_limit_decoded_image_kb",
                               0);
  // Create the timer.
  sTimer = do_CreateInstance("@mozilla.org/timer;1");

  // Create a lock for safegarding the 64-bit sCurrentDecodedImageBytes
  sAllocationLock = PR_NewLock();

  // Create a lock for the node list.
  MOZ_ASSERT(!sNodeListMutex);
  sNodeListMutex = new Mutex("image::DiscardTracker");

  // Mark us as initialized
  sInitialized = true;

  // Read the timeout pref and start the timer.
  ReloadTimeout();

  return NS_OK;
}

/**
 * Read the discard timeout from about:config.
 */
void
DiscardTracker::ReloadTimeout()
{
  // Read the timeout pref.
  int32_t discardTimeout;
  nsresult rv = Preferences::GetInt(sDiscardTimeoutPref, &discardTimeout);

  // If we got something bogus, return.
  if (!NS_SUCCEEDED(rv) || discardTimeout <= 0)
    return;

  // If the value didn't change, return.
  if ((uint32_t) discardTimeout == sMinDiscardTimeoutMs)
    return;

  // Update the value.
  sMinDiscardTimeoutMs = (uint32_t) discardTimeout;

  // Restart the timer so the new timeout takes effect.
  DisableTimer();
  EnableTimer();
}

/**
 * Enables the timer. No-op if the timer is already running.
 */
nsresult
DiscardTracker::EnableTimer()
{
  // Nothing to do if the timer's already on or we haven't yet been
  // initialized.  !sTimer probably means we've shut down, so just ignore that,
  // too.
  if (sTimerOn || !sInitialized || !sTimer)
    return NS_OK;

  sTimerOn = true;

  // Activate the timer.  Have it call us back in (sMinDiscardTimeoutMs / 2)
  // ms, so that an image is discarded between sMinDiscardTimeoutMs and
  // (3/2 * sMinDiscardTimeoutMs) ms after it's unlocked.
  return sTimer->InitWithFuncCallback(TimerCallback,
                                      nullptr,
                                      sMinDiscardTimeoutMs / 2,
                                      nsITimer::TYPE_REPEATING_SLACK);
}

/*
 * Disables the timer. No-op if the timer isn't running.
 */
void
DiscardTracker::DisableTimer()
{
  // Nothing to do if the timer's already off.
  if (!sTimerOn || !sTimer)
    return;
  sTimerOn = false;

  // Deactivate
  sTimer->Cancel();
}

/**
 * Routine activated when the timer fires. This discards everything that's
 * older than sMinDiscardTimeoutMs, and tries to discard enough images so that
 * we go under sMaxDecodedImageKB.
 */
void
DiscardTracker::TimerCallback(nsITimer *aTimer, void *aClosure)
{
  DiscardNow();
}

void
DiscardTracker::DiscardNow()
{
  // Assuming the list is ordered with oldest discard tracker nodes at the back
  // and newest ones at the front, iterate from back to front discarding nodes
  // until we encounter one which is new enough to keep and until we go under
  // our sMaxDecodedImageKB limit.

  TimeStamp now = TimeStamp::Now();
  Node* node;
  while ((node = sDiscardableImages.getLast())) {
    if ((now - node->timestamp).ToMilliseconds() > sMinDiscardTimeoutMs ||
        sCurrentDecodedImageBytes > sMaxDecodedImageKB * 1024) {

      // Discarding the image should cause sCurrentDecodedImageBytes to
      // decrease via a call to InformDeallocation().
      node->img->Discard();

      // Careful: Discarding may have caused the node to have been removed
      // from the list.
      Remove(node);
    }
    else {
      break;
    }
  }

  // If the list is empty, disable the timer.
  if (sDiscardableImages.isEmpty())
    DisableTimer();
}

void
DiscardTracker::MaybeDiscardSoon()
{
  // Are we carrying around too much decoded image data?  If so, enqueue an
  // event to try to get us down under our limit.
  if (sCurrentDecodedImageBytes > sMaxDecodedImageKB * 1024 &&
      !sDiscardableImages.isEmpty()) {
    // Check if the value of sDiscardRunnablePending used to be false
    if (!sDiscardRunnablePending.exchange(true)) {
      nsRefPtr<DiscardRunnable> runnable = new DiscardRunnable();
      NS_DispatchToMainThread(runnable);
    }
  }
}

} // namespace image
} // namespace mozilla
