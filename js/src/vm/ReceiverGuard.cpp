/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ReceiverGuard.h"

#include "builtin/TypedObject.h"

#include "vm/JSObject-inl.h"

using namespace js;

void HeapReceiverGuard::trace(JSTracer* trc) {
  TraceNullableEdge(trc, &shape_, "receiver_guard_shape");
  TraceNullableEdge(trc, &group_, "receiver_guard_group");
}
