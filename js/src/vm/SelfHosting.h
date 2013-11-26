/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_SelfHosting_h_
#define vm_SelfHosting_h_

#include "jsapi.h"

namespace js {

/*
 * When wrapping objects for use by self-hosted code, we skip all checks and
 * always create an OpaqueWrapper that allows nothing.
 * When wrapping functions from the self-hosting compartment for use in other
 * compartments, we create an OpaqueWrapperWithCall that only allows calls,
 * nothing else.
 */
extern const JSWrapObjectCallbacks SelfHostingWrapObjectCallbacks;

} /* namespace js */

#endif /* vm_SelfHosting_h_ */
