/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProgressTracker_h__
#define ProgressTracker_h__

#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"
#include "nsCOMPtr.h"
#include "nsTObserverArray.h"
#include "nsThreadUtils.h"
#include "nsRect.h"
#include "imgRequestProxy.h"

class imgIContainer;
class nsIRunnable;

namespace mozilla {
namespace image {

class AsyncNotifyRunnable;
class AsyncNotifyCurrentStateRunnable;
class Image;

// Image progress bitflags.
enum {
  FLAG_SIZE_AVAILABLE     = 1u << 0,  // STATUS_SIZE_AVAILABLE
  FLAG_DECODE_STARTED     = 1u << 1,  // STATUS_DECODE_STARTED
  FLAG_DECODE_COMPLETE    = 1u << 2,  // STATUS_DECODE_COMPLETE
  FLAG_FRAME_COMPLETE     = 1u << 3,  // STATUS_FRAME_COMPLETE
  FLAG_LOAD_COMPLETE      = 1u << 4,  // STATUS_LOAD_COMPLETE
  FLAG_ONLOAD_BLOCKED     = 1u << 5,
  FLAG_ONLOAD_UNBLOCKED   = 1u << 6,
  FLAG_IS_ANIMATED        = 1u << 7,  // STATUS_IS_ANIMATED
  FLAG_HAS_TRANSPARENCY   = 1u << 8,  // STATUS_HAS_TRANSPARENCY
  FLAG_IS_MULTIPART       = 1u << 9,
  FLAG_LAST_PART_COMPLETE = 1u << 10,
  FLAG_HAS_ERROR          = 1u << 11  // STATUS_ERROR
};

typedef uint32_t Progress;

const uint32_t NoProgress = 0;

inline Progress LoadCompleteProgress(bool aLastPart,
                                     bool aError,
                                     nsresult aStatus)
{
  Progress progress = FLAG_LOAD_COMPLETE;
  if (aLastPart) {
    progress |= FLAG_LAST_PART_COMPLETE;
  }
  if (NS_FAILED(aStatus) || aError) {
    progress |= FLAG_HAS_ERROR;
  }
  return progress;
}

/**
 * ProgressTracker is a class that records an Image's progress through the
 * loading and decoding process, and makes it possible to send notifications to
 * imgRequestProxys, both synchronously and asynchronously.
 *
 * When a new proxy needs to be notified of the current progress of an image,
 * call the Notify() method on this class with the relevant proxy as its
 * argument, and the notifications will be replayed to the proxy asynchronously.
 */
class ProgressTracker : public mozilla::SupportsWeakPtr<ProgressTracker>
{
  virtual ~ProgressTracker() { }

public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(ProgressTracker)
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ProgressTracker)

  // aImage is the image that will be passed to the observers in SyncNotify()
  // and EmulateRequestFinished(), and must be alive as long as this instance
  // is, because we hold a weak reference to it.
  explicit ProgressTracker(Image* aImage)
    : mImage(aImage)
    , mProgress(NoProgress)
  { }

  bool HasImage() const { return mImage; }
  already_AddRefed<Image> GetImage() const
  {
    nsRefPtr<Image> image = mImage;
    return image.forget();
  }

  // Informs this ProgressTracker that it's associated with a multipart image.
  void SetIsMultipart();

  // Returns whether we are in the process of loading; that is, whether we have
  // not received OnStopRequest from Necko.
  bool IsLoading() const;

  // Get the current image status (as in imgIRequest).
  uint32_t GetImageStatus() const;

  // Get the current Progress.
  Progress GetProgress() const { return mProgress; }

  // Schedule an asynchronous "replaying" of all the notifications that would
  // have to happen to put us in the current state.
  // We will also take note of any notifications that happen between the time
  // Notify() is called and when we call SyncNotify on |proxy|, and replay them
  // as well.
  // Should be called on the main thread only, since imgRequestProxy and GetURI
  // are not threadsafe.
  void Notify(imgRequestProxy* proxy);

  // Schedule an asynchronous "replaying" of all the notifications that would
  // have to happen to put us in the state we are in right now.
  // Unlike Notify(), does *not* take into account future notifications.
  // This is only useful if you do not have an imgRequest, e.g., if you are a
  // static request returned from imgIRequest::GetStaticRequest().
  // Should be called on the main thread only, since imgRequestProxy and GetURI
  // are not threadsafe.
  void NotifyCurrentState(imgRequestProxy* proxy);

  // "Replay" all of the notifications that would have to happen to put us in
  // the state we're currently in.
  // Only use this if you're already servicing an asynchronous call (e.g.
  // OnStartRequest).
  // Should be called on the main thread only, since imgRequestProxy and GetURI
  // are not threadsafe.
  void SyncNotify(imgRequestProxy* proxy);

  // Get this ProgressTracker ready for a new request. This resets all the
  // state that doesn't persist between requests.
  void ResetForNewRequest();

  // Stateless notifications. These are dispatched and immediately forgotten
  // about. All except OnImageAvailable are main thread only.
  void OnDiscard();
  void OnUnlockedDraw();
  void OnImageAvailable();

  // Compute the difference between this our progress and aProgress. This allows
  // callers to predict whether SyncNotifyProgress will send any notifications.
  Progress Difference(Progress aProgress) const
  {
    return ~mProgress & aProgress;
  }

  // Update our state to incorporate the changes in aProgress and synchronously
  // notify our observers.
  //
  // Because this may result in recursive notifications, no decoding locks may
  // be held.  Called on the main thread only.
  void SyncNotifyProgress(Progress aProgress,
                          const nsIntRect& aInvalidRect = nsIntRect());

  // We manage a set of consumers that are using an image and thus concerned
  // with its loading progress. Weak pointers.
  void AddConsumer(imgRequestProxy* aConsumer);
  bool RemoveConsumer(imgRequestProxy* aConsumer, nsresult aStatus);
  size_t ConsumerCount() const {
    MOZ_ASSERT(NS_IsMainThread(), "Use mConsumers on main thread only");
    return mConsumers.Length();
  }

  // This is intentionally non-general because its sole purpose is to support
  // some obscure network priority logic in imgRequest. That stuff could
  // probably be improved, but it's too scary to mess with at the moment.
  bool FirstConsumerIs(imgRequestProxy* aConsumer);

  void AdoptConsumers(ProgressTracker* aTracker) {
    MOZ_ASSERT(NS_IsMainThread(), "Use mConsumers on main thread only");
    MOZ_ASSERT(aTracker);
    mConsumers = aTracker->mConsumers;
  }

private:
  typedef nsTObserverArray<mozilla::WeakPtr<imgRequestProxy>> ProxyArray;
  friend class AsyncNotifyRunnable;
  friend class AsyncNotifyCurrentStateRunnable;
  friend class ProgressTrackerInit;

  ProgressTracker(const ProgressTracker& aOther) MOZ_DELETE;

  // This method should only be called once, and only on an ProgressTracker
  // that was initialized without an image. ProgressTrackerInit automates this.
  void SetImage(Image* aImage);

  // Resets our weak reference to our image, for when mImage is about to go out
  // of scope.  ProgressTrackerInit automates this.
  void ResetImage();

  // Send some notifications that would be necessary to make |aProxy| believe
  // the request is finished downloading and decoding.  We only send
  // FLAG_REQUEST_* and FLAG_ONLOAD_UNBLOCKED, and only if necessary.
  void EmulateRequestFinished(imgRequestProxy* aProxy, nsresult aStatus);

  // Main thread only because it deals with the observer service.
  void FireFailureNotification();

  // Main thread only, since imgRequestProxy calls are expected on the main
  // thread, and mConsumers is not threadsafe.
  static void SyncNotifyInternal(ProxyArray& aProxies,
                                 bool aHasImage, Progress aProgress,
                                 const nsIntRect& aInvalidRect);

  nsCOMPtr<nsIRunnable> mRunnable;

  // This weak ref should be set null when the image goes out of scope.
  Image* mImage;

  // List of proxies attached to the image. Each proxy represents a consumer
  // using the image. Array and/or individual elements should only be accessed
  // on the main thread.
  ProxyArray mConsumers;

  Progress mProgress;
};

class ProgressTrackerInit
{
public:
  ProgressTrackerInit(Image* aImage, ProgressTracker* aTracker);
  ~ProgressTrackerInit();
private:
  ProgressTracker* mTracker;
};

} // namespace image
} // namespace mozilla

#endif
