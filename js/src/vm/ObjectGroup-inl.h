/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ObjectGroup_inl_h
#define vm_ObjectGroup_inl_h

#include "vm/ObjectGroup.h"

namespace js {

inline bool ObjectGroup::needsSweep() {
  // Note: this can be called off thread during compacting GCs, in which case
  // nothing will be running on the main thread.
  MOZ_ASSERT(!TlsContext.get()->inUnsafeCallWithABI);
  return generation() != zoneFromAnyThread()->types.generation;
}

inline ObjectGroupFlags ObjectGroup::flags(const AutoSweepObjectGroup& sweep) {
  MOZ_ASSERT(sweep.group() == this);
  return flagsDontCheckGeneration();
}

inline void ObjectGroup::addFlags(const AutoSweepObjectGroup& sweep,
                                  ObjectGroupFlags flags) {
  MOZ_ASSERT(sweep.group() == this);
  flags_ |= flags;
}

inline void ObjectGroup::clearFlags(const AutoSweepObjectGroup& sweep,
                                    ObjectGroupFlags flags) {
  MOZ_ASSERT(sweep.group() == this);
  flags_ &= ~flags;
}

inline bool ObjectGroup::hasAnyFlags(const AutoSweepObjectGroup& sweep,
                                     ObjectGroupFlags flags) {
  MOZ_ASSERT((flags & OBJECT_FLAG_DYNAMIC_MASK) == flags);
  return !!(this->flags(sweep) & flags);
}

inline bool ObjectGroup::hasAllFlags(const AutoSweepObjectGroup& sweep,
                                     ObjectGroupFlags flags) {
  MOZ_ASSERT((flags & OBJECT_FLAG_DYNAMIC_MASK) == flags);
  return (this->flags(sweep) & flags) == flags;
}

inline bool ObjectGroup::unknownProperties(const AutoSweepObjectGroup& sweep) {
  MOZ_ASSERT_IF(flags(sweep) & OBJECT_FLAG_UNKNOWN_PROPERTIES,
                hasAllFlags(sweep, OBJECT_FLAG_DYNAMIC_MASK));
  return !!(flags(sweep) & OBJECT_FLAG_UNKNOWN_PROPERTIES);
}

inline bool ObjectGroup::shouldPreTenure(const AutoSweepObjectGroup& sweep) {
  MOZ_ASSERT(sweep.group() == this);
  return shouldPreTenureDontCheckGeneration();
}

inline bool ObjectGroup::shouldPreTenureDontCheckGeneration() {
  return hasAnyFlagsDontCheckGeneration(OBJECT_FLAG_PRE_TENURE) &&
         !unknownPropertiesDontCheckGeneration();
}

inline bool ObjectGroup::canPreTenure(const AutoSweepObjectGroup& sweep) {
  return !unknownProperties(sweep);
}

inline bool ObjectGroup::fromAllocationSite(const AutoSweepObjectGroup& sweep) {
  return flags(sweep) & OBJECT_FLAG_FROM_ALLOCATION_SITE;
}

inline void ObjectGroup::setShouldPreTenure(const AutoSweepObjectGroup& sweep,
                                            JSContext* cx) {
  MOZ_ASSERT(canPreTenure(sweep));
  setFlags(sweep, cx, OBJECT_FLAG_PRE_TENURE);
}

inline TypeNewScript* ObjectGroup::newScript(
    const AutoSweepObjectGroup& sweep) {
  MOZ_ASSERT(sweep.group() == this);
  return newScriptDontCheckGeneration();
}

inline PreliminaryObjectArrayWithTemplate* ObjectGroup::maybePreliminaryObjects(
    const AutoSweepObjectGroup& sweep) {
  MOZ_ASSERT(sweep.group() == this);
  return maybePreliminaryObjectsDontCheckGeneration();
}

}  // namespace js

#endif /* vm_ObjectGroup_inl_h */
