/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_imagelib_DiscardTracker_h_
#define mozilla_imagelib_DiscardTracker_h_

#include "mozilla/LinkedList.h"
#include "mozilla/TimeStamp.h"

class nsITimer;

namespace mozilla {
namespace image {

class RasterImage;

/**
 * This static class maintains a linked list of RasterImage objects which are
 * eligible for discarding.
 *
 * When Reset() is called, the node is removed from its position in the list
 * (if it was there before) and appended to the beginnings of the list.
 *
 * Periodically (on a timer and when we notice that we're using more memory
 * than we'd like for decoded images), we go through the list and discard
 * decoded data from images at the end of the list.
 */
class DiscardTracker
{
  public:
    /**
     * The DiscardTracker keeps a linked list of Node objects.  Each object
     * points to a RasterImage and contains a timestamp indicating when the
     * node was inserted into the tracker.
     *
     * This structure is embedded within each RasterImage object, and we do
     * |mDiscardTrackerNode.img = this| on RasterImage construction.  Thus, a
     * RasterImage must always call DiscardTracker::Remove() in its destructor
     * to avoid having the tracker point to bogus memory.
     */
    struct Node : public LinkedListElement<Node>
    {
      RasterImage *img;
      TimeStamp timestamp;
    };

    /**
     * Add an image to the front of the tracker's list, or move it to the front
     * if it's already in the list.
     */
    static nsresult Reset(struct Node* node);

    /**
     * Remove a node from the tracker; do nothing if the node is currently
     * untracked.
     */
    static void Remove(struct Node* node);

    /**
     * Shut the discard tracker down.  This should be called on XPCOM shutdown
     * so we destroy the discard timer's nsITimer.
     */
    static void Shutdown();

    /**
     * Discard the decoded image data for all images tracked by the discard
     * tracker.
     */
    static void DiscardAll();

    /**
     * Inform the discard tracker that we've allocated or deallocated some
     * memory for a decoded image.  We use this to determine when we've
     * allocated too much memory and should discard some images.
     */
    static void InformAllocation(PRInt64 bytes);

  private:
    /**
     * This is called when the discard timer fires; it calls into DiscardNow().
     */
    friend int DiscardTimeoutChangedCallback(const char* aPref, void *aClosure);

    /**
     * When run, this runnable sets sDiscardRunnablePending to false and calls
     * DiscardNow().
     */
    class DiscardRunnable : public nsRunnable
    {
      NS_IMETHOD Run();
    };

    static nsresult Initialize();
    static void ReloadTimeout();
    static nsresult EnableTimer();
    static void DisableTimer();
    static void MaybeDiscardSoon();
    static void TimerCallback(nsITimer *aTimer, void *aClosure);
    static void DiscardNow();

    static LinkedList<Node> sDiscardableImages;
    static nsCOMPtr<nsITimer> sTimer;
    static bool sInitialized;
    static bool sTimerOn;
    static bool sDiscardRunnablePending;
    static PRInt64 sCurrentDecodedImageBytes;
    static PRUint32 sMinDiscardTimeoutMs;
    static PRUint32 sMaxDecodedImageKB;
};

} // namespace image
} // namespace mozilla

#endif /* mozilla_imagelib_DiscardTracker_h_ */
