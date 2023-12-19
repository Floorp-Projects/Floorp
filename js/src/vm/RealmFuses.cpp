/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "vm/RealmFuses.h"

#include "vm/GlobalObject.h"
#include "vm/NativeObject.h"
#include "vm/Realm.h"
#include "vm/SelfHosting.h"

void js::ArrayPrototypeIteratorFuse::popFuse(JSContext* cx,
                                             RealmFuses& realmFuses) {
  // Pop Self.
  RealmFuse::popFuse(cx);

  // Pop associated fuse in same realm as current object.
  realmFuses.optimizeGetIteratorFuse.popFuse(cx, realmFuses);
}

void js::ArrayPrototypeIteratorNextFuse::popFuse(JSContext* cx,
                                                 RealmFuses& realmFuses) {
  // Pop Self.
  RealmFuse::popFuse(cx);

  // Pop associated fuse in same realm as current object.
  realmFuses.optimizeGetIteratorFuse.popFuse(cx, realmFuses);
}

int32_t js::RealmFuses::fuseOffsets[uint8_t(
    RealmFuses::FuseIndex::LastFuseIndex)] = {
#define FUSE(Name, LowerName) offsetof(RealmFuses, LowerName),
    FOR_EACH_REALM_FUSE(FUSE)
#undef FUSE
};

// static
int32_t js::RealmFuses::offsetOfFuseWordRelativeToRealm(
    RealmFuses::FuseIndex index) {
  int32_t base_offset = offsetof(Realm, realmFuses);
  int32_t fuse_offset = RealmFuses::fuseOffsets[uint8_t(index)];
  int32_t fuseWordOffset = GuardFuse::fuseOffset();

  return base_offset + fuse_offset + fuseWordOffset;
}

const char* js::RealmFuses::fuseNames[] = {
#define FUSE(Name, LowerName) #LowerName,
    FOR_EACH_REALM_FUSE(FUSE)
#undef FUSE
};

// TODO: It is not elegant that we have both this mechanism, but also
// GuardFuse::name, and all the overrides for naming fuses. The issue is
// that this method is static to handle consumers that don't have a
// RealmFuses around but work with indexes (e.g. spew code).
//
// I'd love it if we had a better answer.
const char* js::RealmFuses::getFuseName(RealmFuses::FuseIndex index) {
  uint8_t rawIndex = uint8_t(index);
  MOZ_ASSERT(rawIndex > 0 && index < RealmFuses::FuseIndex::LastFuseIndex);
  return fuseNames[rawIndex];
}

bool js::OptimizeGetIteratorFuse::checkInvariant(JSContext* cx) {
  // Simple invariant: this fuse merely reflects the conjunction of two fuses,
  // so if this fuse is intact, then the invariant it asserts is that these two
  // realm fuses are also intact.
  return cx->realm()->realmFuses.arrayPrototypeIteratorFuse.intact() &&
         cx->realm()->realmFuses.arrayPrototypeIteratorNextFuse.intact();
}

bool js::ArrayPrototypeIteratorFuse::checkInvariant(JSContext* cx) {
  // Prototype must be Array.prototype.
  auto* proto = cx->global()->maybeGetArrayPrototype();
  if (!proto) {
    // No proto, invariant still holds
    return true;
  }

  PropertyKey iteratorKey =
      PropertyKey::Symbol(cx->wellKnownSymbols().iterator);

  // Ensure that Array.prototype's @@iterator slot is unchanged.
  mozilla::Maybe<PropertyInfo> prop = proto->lookupPure(iteratorKey);
  if (prop.isNothing() || !prop->isDataProperty()) {
    return false;
  }

  auto slot = prop->slot();
  const Value& iterVal = proto->getSlot(slot);
  if (!iterVal.isObject() || !iterVal.toObject().is<JSFunction>()) {
    return false;
  }

  auto* iterFun = &iterVal.toObject().as<JSFunction>();
  return IsSelfHostedFunctionWithName(iterFun, cx->names().dollar_ArrayValues_);
}

/* static */
bool js::ArrayPrototypeIteratorNextFuse::checkInvariant(JSContext* cx) {
  auto* proto = cx->global()->maybeGetArrayIteratorPrototype();

  if (!proto) {
    // Invariant holds if there is no array iterator proto.
    return true;
  }

  // Ensure that %ArrayIteratorPrototype%'s "next" slot is unchanged.
  mozilla::Maybe<PropertyInfo> prop = proto->lookupPure(cx->names().next);
  if (prop.isNothing() || !prop->isDataProperty()) {
    // Next property has been modified, return false, invariant no longer holds.
    return false;
  }

  auto slot = prop->slot();

  const Value& nextVal = proto->getSlot(slot);
  if (!nextVal.isObject() || !nextVal.toObject().is<JSFunction>()) {
    // Next property has been modified, return false, invariant no longer holds.
    return false;
  }

  auto* nextFun = &nextVal.toObject().as<JSFunction>();
  return IsSelfHostedFunctionWithName(nextFun, cx->names().ArrayIteratorNext);
}
