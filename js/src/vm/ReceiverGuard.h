/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ReceiverGuard_h
#define vm_ReceiverGuard_h

#include "vm/Shape.h"

namespace js {

// [SMDOC] Receiver Guard
//
// A ReceiverGuard encapsulates the information about an object that needs to
// be tested to determine if it has the same 'structure' as another object.
// The guard includes the shape and/or group of the object, and which of these
// is tested, as well as the meaning here of 'structure', depends on the kind
// of object being tested:
//
// NativeObject: The structure of a native object is determined by its shape.
//   Two objects with the same shape have the same class, prototype, flags,
//   and all properties except those stored in dense elements.
//
// ProxyObject: The structure of a proxy object is determined by its shape.
//   Proxies with the same shape have the same class and prototype, but no
//   other commonality is guaranteed.
//
// TypedObject: The structure of a typed object is determined by its group.
//   All typed objects with the same group have the same class, prototype, and
//   own properties.
//
// In all cases, a ReceiverGuard has *either* a shape or a group active, and
// never both.
class HeapReceiverGuard;

class ReceiverGuard {
  ObjectGroup* group_;
  Shape* shape_;

  void MOZ_ALWAYS_INLINE assertInvariants() {
    // Only one of group_ or shape_ may be active at a time.
    MOZ_ASSERT_IF(group_ || shape_, !!group_ != !!shape_);
  }

 public:
  ReceiverGuard() : group_(nullptr), shape_(nullptr) {}

  inline MOZ_IMPLICIT ReceiverGuard(const HeapReceiverGuard& guard);

  explicit MOZ_ALWAYS_INLINE ReceiverGuard(JSObject* obj);
  MOZ_ALWAYS_INLINE ReceiverGuard(ObjectGroup* group, Shape* shape);

  bool operator==(const ReceiverGuard& other) const {
    return group_ == other.group_ && shape_ == other.shape_;
  }

  bool operator!=(const ReceiverGuard& other) const {
    return !(*this == other);
  }

  uintptr_t hash() const {
    return (uintptr_t(group_) >> 3) ^ (uintptr_t(shape_) >> 3);
  }

  void setShape(Shape* shape) {
    shape_ = shape;
    assertInvariants();
  }

  void setGroup(ObjectGroup* group) {
    group_ = group;
    assertInvariants();
  }

  Shape* getShape() const { return shape_; }
  ObjectGroup* getGroup() const { return group_; }
};

// Heap storage for ReceiverGuards.
//
// This is a storage only class -- all computation is actually
// done by converting this back to a RecieverGuard, hence why
// there are no accessors.
class HeapReceiverGuard {
  friend class ReceiverGuard;

  GCPtrObjectGroup group_;
  GCPtrShape shape_;

 public:
  explicit HeapReceiverGuard(const ReceiverGuard& guard)
      : group_(guard.getGroup()), shape_(guard.getShape()) {}

  void trace(JSTracer* trc);
};

inline ReceiverGuard::ReceiverGuard(const HeapReceiverGuard& guard)
    : group_(guard.group_), shape_(guard.shape_) {
  assertInvariants();
}

}  // namespace js

#endif /* vm_ReceiverGuard_h */
