/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_TemplateRegistry_h
#define vm_TemplateRegistry_h

#include "jsobj.h"
#include "gc/Marking.h"
#include "js/GCHashTable.h"

namespace js {

// Data structures to maintain unique template objects mapped to by lists of
// raw strings.
//
// See ES 12.2.9.3.

struct TemplateRegistryHashPolicy
{
    // For use as HashPolicy. Expects keys as arrays of atoms.
    using Key = JSObject*;
    using Lookup = JSObject*;

    static HashNumber hash(const Lookup& lookup);
    static bool match(const Key& key, const Lookup& lookup);
};

using TemplateRegistry = JS::GCHashMap<JSObject*,
                                       JSObject*,
                                       TemplateRegistryHashPolicy,
                                       SystemAllocPolicy>;

} // namespace js

#endif // vm_TemplateRegistery_h
