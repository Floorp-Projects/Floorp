/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Internal header for definitions shared between the verifier and jsapi tests.
 */

#ifndef gc_Verifier_h
#define gc_Verifier_h

#include <algorithm>

#include "gc/Cell.h"

namespace js {
namespace gc {

static constexpr CellColor AllCellColors[] = {CellColor::White, CellColor::Gray,
                                              CellColor::Black};

static constexpr CellColor MarkedCellColors[] = {CellColor::Gray,
                                                 CellColor::Black};

}  // namespace gc
}  // namespace js

#endif /* gc_Verifier_h */
