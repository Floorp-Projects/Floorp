/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsgc_internal_h___
#define jsgc_internal_h___

#include "jsapi.h"

namespace js {
namespace gc {

void
MarkRuntime(JSTracer *trc, bool useSavedRoots = false);

} /* namespace gc */
} /* namespace js */

#endif /* jsgc_internal_h___ */
