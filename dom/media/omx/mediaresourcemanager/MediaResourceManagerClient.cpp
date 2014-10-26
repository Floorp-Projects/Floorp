/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define LOG_NDEBUG 0
#define LOG_TAG "MediaResourceManagerClient"

#include <utils/Log.h>

#include "MediaResourceManagerClient.h"

namespace android {

MediaResourceManagerClient::MediaResourceManagerClient(const wp<EventListener>& listener)
  : mEventListener(listener)
{
}

void MediaResourceManagerClient::statusChanged(int event)
{
  if (mEventListener != NULL) {
    sp<EventListener> listener = mEventListener.promote();
    if (listener != NULL) {
      listener->statusChanged(event);
    }
  }
}

void MediaResourceManagerClient::died()
{
  sp<EventListener> listener = mEventListener.promote();
  if (listener != NULL) {
    listener->statusChanged(CLIENT_STATE_SHUTDOWN);
  }
}

}; // namespace android

