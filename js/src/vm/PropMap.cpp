/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/PropMap-inl.h"

using namespace js;

void DictionaryPropMap::fixupAfterMovingGC() {}

void SharedPropMap::fixupAfterMovingGC() {}

void DictionaryPropMap::sweep(JSFreeOp* fop) {}

void SharedPropMap::sweep(JSFreeOp* fop) {}

void SharedPropMap::finalize(JSFreeOp* fop) {}

void DictionaryPropMap::finalize(JSFreeOp* fop) {}

JS::ubi::Node::Size JS::ubi::Concrete<PropMap>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return js::gc::Arena::thingSize(get().asTenured().getAllocKind());
}
