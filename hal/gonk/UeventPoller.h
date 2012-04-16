/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _mozilla_uevent_poller_h_
#define _mozilla_uevent_poller_h_

#include <sysutils/NetlinkEvent.h>
#include "mozilla/Observer.h"

class NetlinkEvent;

namespace mozilla {
namespace hal_impl {

typedef mozilla::Observer<NetlinkEvent> IUeventObserver;

/**
 * Register for uevent notification. Note that the method should run on the
 * <b> IO Thread </b>
 * @aObserver the observer to be added. The observer's Notify() is only called
 * on the <b> IO Thread </b>
 */
void RegisterUeventListener(IUeventObserver *aObserver);

/**
 * Unregister for uevent notification. Note that the method should run on the
 * <b> IO Thread </b>
 * @aObserver the observer to be removed
 */
void UnregisterUeventListener(IUeventObserver *aObserver);

}
}

#endif

