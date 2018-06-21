/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Realm_inl_h
#define vm_Realm_inl_h

#include "vm/Realm.h"

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "vm/Iteration.h"

#include "vm/JSContext-inl.h"

inline void
JS::Realm::initGlobal(js::GlobalObject& global)
{
    MOZ_ASSERT(global.realm() == this);
    MOZ_ASSERT(!global_);
    global_.set(&global);
}

js::GlobalObject*
JS::Realm::maybeGlobal() const
{
    MOZ_ASSERT_IF(global_, global_->realm() == this);
    return global_;
}

js::GlobalObject*
JS::Realm::unsafeUnbarrieredMaybeGlobal() const
{
    return *global_.unsafeGet();
}

inline bool
JS::Realm::globalIsAboutToBeFinalized()
{
    MOZ_ASSERT(zone_->isGCSweeping());
    return global_ && js::gc::IsAboutToBeFinalizedUnbarriered(global_.unsafeGet());
}

/* static */ inline js::ObjectRealm&
js::ObjectRealm::get(const JSObject* obj)
{
    // Note: obj might be a CCW if we're accessing ObjectRealm::enumerators.
    // CCWs here are fine because we always return the same ObjectRealm for a
    // particular (CCW) object.
    return obj->maybeCCWRealm()->objects_;
}

template <typename T>
js::AutoRealm::AutoRealm(JSContext* cx, const T& target)
  : cx_(cx),
    origin_(cx->realm())
{
    cx_->enterRealmOf(target);
}

// Protected constructor that bypasses assertions in enterRealmOf.
js::AutoRealm::AutoRealm(JSContext* cx, JS::Realm* target)
  : cx_(cx),
    origin_(cx->realm())
{
    cx_->enterRealm(target);
}

js::AutoRealm::~AutoRealm()
{
    cx_->leaveRealm(origin_);
}

js::AutoAtomsZone::AutoAtomsZone(JSContext* cx, js::AutoLockForExclusiveAccess& lock)
  : cx_(cx),
    origin_(cx->realm()),
    lock_(lock)
{
    cx_->enterAtomsZone(lock);
}

js::AutoAtomsZone::~AutoAtomsZone()
{
    cx_->leaveAtomsZone(origin_, lock_);
}

js::AutoRealmUnchecked::AutoRealmUnchecked(JSContext* cx, JS::Realm* target)
  : AutoRealm(cx, target)
{}

MOZ_ALWAYS_INLINE bool
js::ObjectRealm::objectMaybeInIteration(JSObject* obj)
{
    MOZ_ASSERT(&ObjectRealm::get(obj) == this);

    // If the list is empty we're not iterating any objects.
    js::NativeIterator* next = enumerators->next();
    if (enumerators == next)
        return false;

    // If the list contains a single object, check if it's |obj|.
    if (next->next() == enumerators)
        return next->objectBeingIterated() == obj;

    return true;
}

#endif /* vm_Realm_inl_h */
