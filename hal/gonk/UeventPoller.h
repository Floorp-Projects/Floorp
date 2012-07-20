/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

