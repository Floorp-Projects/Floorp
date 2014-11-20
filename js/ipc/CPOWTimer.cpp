/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfriendapi.h"
#include "xpcprivate.h"
#include "CPOWTimer.h"

CPOWTimer::~CPOWTimer() {
    /* This is a best effort to find the compartment responsible for this CPOW call */
    xpc::CompartmentPrivate* compartment = xpc::CompartmentPrivate::Get(js::GetObjectCompartment(mozilla::dom::GetIncumbentGlobal()
                                                                                                 ->GetGlobalJSObject()));
    PRIntervalTime time = PR_IntervalNow() - startInterval;
    compartment->CPOWTime += time;
}
