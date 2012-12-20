/** -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_IMAGELIB_IMGDECODEROBSERVER_H_
#define MOZILLA_IMAGELIB_IMGDECODEROBSERVER_H_

#include "nsRect.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"

/**
 * imgDecoderObserver interface
 *
 * This interface is used to observe Decoder objects.
 *
 * We make the distinction here between "load" and "decode" notifications. Load
 * notifications are fired as the image is loaded from the network or
 * filesystem. Decode notifications are fired as the image is decoded. If an
 * image is decoded on load and not visibly discarded, decode notifications are
 * nested logically inside load notifications as one might expect. However, with
 * decode-on-draw, the set of decode notifications can come completely _after_
 * the load notifications, and can come multiple times if the image is
 * discardable. Moreover, they can be interleaved in various ways. In general,
 * any presumed ordering between load and decode notifications should not be
 * relied upon.
 *
 * Decode notifications may or may not be synchronous, depending on the
 * situation. If imgIDecoder::FLAG_SYNC_DECODE is passed to a function that
 * triggers a decode, all notifications that can be generated from the currently
 * loaded data fire before the call returns. If FLAG_SYNC_DECODE is not passed,
 * all, some, or none of the notifications may fire before the call returns.
 */
class imgDecoderObserver : public mozilla::RefCounted<imgDecoderObserver>,
                           public mozilla::SupportsWeakPtr<imgDecoderObserver>
{
public:
  virtual ~imgDecoderObserver() = 0;

  /**
   * Load notification.
   *
   * called at the same time that nsIRequestObserver::onStartRequest would be
   * (used only for observers of imgIRequest objects, which are nsIRequests,
   * not imgIDecoder objects)
   */
  virtual void OnStartRequest() = 0;

  /**
   * Decode notification.
   *
   * Called as soon as the image begins getting decoded. This does not include
   * "header-only" decodes used by decode-on-draw to parse the width/height
   * out of the image. Thus, it is a decode notification only.
   */
  virtual void OnStartDecode() = 0;

  /**
   * Load notification.
   *
   * Called once enough data has been loaded from the network that we were able
   * to parse the width/height from the image. By the time this callback is been
   * called, the size has been set on the container and STATUS_SIZE_AVAILABLE
   * has been set on the associated imgRequest.
   */
  virtual void OnStartContainer() = 0;

  /**
   * Decode notification.
   *
   * called when there is more to paint.
   */
  virtual void OnDataAvailable(const nsIntRect * aRect) = 0;

  virtual void FrameChanged(const nsIntRect * aDirtyRect) = 0;

  /**
   * Decode notification.
   *
   * called when a frame is finished decoding.
   */
  virtual void OnStopFrame() = 0;

  /**
   * Notification for when an image is known to be animated. This should be
   * fired at the earliest possible time.
   */
  virtual void OnImageIsAnimated() = 0;

  /**
   * Decode notification.
   *
   * Called when all decoding has terminated.
   */
  virtual void OnStopDecode(nsresult status) = 0;

  /**
   * Load notification.
   *
   * called at the same time that nsIRequestObserver::onStopRequest would be
   * (used only for observers of imgIRequest objects, which are nsIRequests,
   * not imgIDecoder objects)
   */
  virtual void OnStopRequest(bool aIsLastPart) = 0;

  /**
   * Called when the decoded image data is discarded. This means that the frames
   * no longer exist in decoded form, and any attempt to access or draw the
   * image will initiate a new series of progressive decode notifications.
   */
  virtual void OnDiscard() = 0;
};

// We must define a destructor because derived classes call our destructor from
// theirs.  Pure virtual destructors only requires that child classes implement
// a virtual destructor, not that we can't have one too!
inline imgDecoderObserver::~imgDecoderObserver()
{}

#endif // MOZILLA_IMAGELIB_IMGDECODEROBSERVER_H
