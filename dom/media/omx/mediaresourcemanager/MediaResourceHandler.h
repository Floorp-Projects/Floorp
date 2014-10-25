/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_RESOURCE_HANDLER_H
#define MEDIA_RESOURCE_HANDLER_H

#include <utils/threads.h>

#include <mozilla/Attributes.h>

#include "MediaResourceManagerClient.h"

namespace android {

class MediaResourceHandler : public MediaResourceManagerClient::EventListener
{
public:
  /* Resource notification listener.
   * All functions are called on the Binder thread.
   */
  struct ResourceListener : public virtual RefBase {
    /* The resource is reserved and can be granted.
     * The client can allocate the requested resource.
     */
    virtual void resourceReserved() = 0;
    /* The resource is not reserved any more.
     * The client should release the resource as soon as possible if the
     * resource is still being held.
     */
    virtual void resourceCanceled() = 0;
  };

  MediaResourceHandler(const wp<ResourceListener> &aListener);

  virtual ~MediaResourceHandler();

  // Request Resource
  bool requestResource(IMediaResourceManagerService::ResourceType aType);
  // Cancel Resource
  void cancelResource();

protected:
  // MediaResourceManagerClient::EventListener::statusChanged()
  virtual void statusChanged(int event);

private:
  // Forbidden
  MediaResourceHandler() MOZ_DELETE;
  MediaResourceHandler(const MediaResourceHandler &) MOZ_DELETE;
  const MediaResourceHandler &operator=(const MediaResourceHandler &) MOZ_DELETE;

  // Resource Notification Listener
  wp<ResourceListener> mListener;

  // Resource Management
  Mutex mLock;
  MediaResourceManagerClient::State mState;
  sp<IMediaResourceManagerClient> mClient;
  sp<IMediaResourceManagerService> mService;
  IMediaResourceManagerService::ResourceType mType;
};

} // namespace android

#endif // MEDIA_RESOURCE_HANDLER_H
