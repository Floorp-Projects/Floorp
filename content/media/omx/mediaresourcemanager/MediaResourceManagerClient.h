/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ANDROID_MEDIARESOURCEMANAGERCLIENT_H
#define ANDROID_MEDIARESOURCEMANAGERCLIENT_H

#include "IMediaResourceManagerClient.h"
#include "IMediaResourceManagerDeathNotifier.h"

namespace android {

class MediaResourceManagerClient: public BnMediaResourceManagerClient,
                                  public virtual IMediaResourceManagerDeathNotifier
{
public:
  // Enumeration for the valid decoding states
  enum State {
    CLIENT_STATE_WAIT_FOR_RESOURCE,
    CLIENT_STATE_RESOURCE_ASSIGNED,
    CLIENT_STATE_SHUTDOWN
  };

  struct EventListener : public virtual RefBase {
    // Notifies a change of media resource request status.
    virtual void statusChanged(int event) = 0;
  };

  MediaResourceManagerClient(const wp<EventListener>& listener);

  // DeathRecipient
  void            died();

  // IMediaResourceManagerClient
  virtual void statusChanged(int event);

private:
  wp<EventListener> mEventListener;
};

}; // namespace android

#endif // ANDROID_IMEDIARESOURCEMANAGERCLIENT_H
