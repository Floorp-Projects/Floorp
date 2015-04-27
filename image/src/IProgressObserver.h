/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_src_IProgressObserver_h
#define mozilla_image_src_IProgressObserver_h

#include "mozilla/WeakPtr.h"
#include "nsISupports.h"
#include "nsRect.h"

namespace mozilla {
namespace image {

/**
 * An interface for observing changes to image state, as reported by
 * ProgressTracker.
 *
 * This is the ImageLib-internal version of imgINotificationObserver,
 * essentially, with implementation details that code outside of ImageLib
 * shouldn't see.
 *
 * XXX(seth): It's preferable to avoid adding anything to this interface if
 * possible.  In the long term, it would be ideal to get to a place where we can
 * just use the imgINotificationObserver interface internally as well.
 */
class IProgressObserver : public SupportsWeakPtr<IProgressObserver>
{
public:
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(IProgressObserver)

  // Subclasses may or may not be XPCOM classes, so we just require that they
  // implement AddRef and Release.
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release(void) = 0;

  // imgINotificationObserver methods:
  virtual void Notify(int32_t aType, const nsIntRect* aRect = nullptr) = 0;
  virtual void OnLoadComplete(bool aLastPart) = 0;

  // imgIOnloadBlocker methods:
  virtual void BlockOnload() = 0;
  virtual void UnblockOnload() = 0;

  // Other, internal-only methods:
  virtual void SetHasImage() = 0;
  virtual void OnStartDecode() = 0;
  virtual bool NotificationsDeferred() const = 0;
  virtual void SetNotificationsDeferred(bool aDeferNotifications) = 0;

protected:
  virtual ~IProgressObserver() { }
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_src_IProgressObserver_h
