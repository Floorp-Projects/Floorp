/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_RecordTupleBoxShared_h
#define vm_RecordTupleBoxShared_h

#include "vm/RecordTupleBoxShared.h"

#include "vm/RecordType.h"
#include "vm/TupleType.h"

namespace js {

bool IsExtendedPrimitive(JSObject& obj) {
  return obj.is<RecordType>() || obj.is<TupleType>();
}

}  // namespace js

#endif
