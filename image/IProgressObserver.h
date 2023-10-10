/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_IProgressObserver_h
#define mozilla_image_IProgressObserver_h

#include "mozilla/WeakPtr.h"
#include "nsISupports.h"
#include "nsRect.h"

class nsIEventTarget;

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
class IProgressObserver : public SupportsWeakPtr {
 public:
  // Subclasses may or may not be XPCOM classes, so we just require that they
  // implement AddRef and Release.
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // imgINotificationObserver methods:
  virtual void Notify(int32_t aType, const nsIntRect* aRect = nullptr) = 0;
  virtual void OnLoadComplete(bool aLastPart) = 0;

  // Other, internal-only methods:
  virtual void SetHasImage() = 0;
  virtual bool NotificationsDeferred() const = 0;
  virtual void MarkPendingNotify() = 0;
  virtual void ClearPendingNotify() = 0;

 protected:
  virtual ~IProgressObserver() = default;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_IProgressObserver_h
