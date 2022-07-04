/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4
 * -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Modules_h
#define vm_Modules_h

#include "js/Modules.h"

#include "NamespaceImports.h"

#include "builtin/ModuleObject.h"
#include "js/AllocPolicy.h"
#include "js/GCHashTable.h"
#include "js/HashTable.h"
#include "js/RootingAPI.h"

struct JSContext;

namespace js {

class ArrayObject;

using ModuleSet =
    GCHashSet<ModuleObject*, DefaultHasher<ModuleObject*>, SystemAllocPolicy>;

ArrayObject* ModuleGetExportedNames(JSContext* cx, Handle<ModuleObject*> module,
                                    MutableHandle<ModuleSet> exportStarSet);

}  // namespace js

#endif  // vm_Modules_h
