/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ToSource_h
#define vm_ToSource_h

#include "js/TypeDecls.h"

namespace js {

// Try to convert a value to its source expression, returning null after
// reporting an error, otherwise returning a new string.
JSString* ValueToSource(JSContext* cx, JS::HandleValue v);

}  // namespace js

#endif /* vm_ToSource.h */
