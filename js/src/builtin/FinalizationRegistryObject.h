/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * FinalizationRegistry objects allow a program to register to receive a
 * callback after a 'target' object dies. The callback is passed a 'held value'
 * (that hopefully doesn't entrain the target). An 'unregister token' is an
 * object which can be used to remove multiple previous registrations in one go.
 *
 * To arrange this, the following data structures are used:
 *
 *   +-------------------------------------+---------------------------------+
 *   |   FinalizationRegistry compartment  |    Target zone / compartment    |
 *   |                                     |                                 |
 *   |        +----------------------+     |      +------------------+       |
 *   |  +---->+ FinalizationRegistry |     |      |       Zone       |       |
 *   |  |     +---------+------------+     |      +---------+--------+       |
 *   |  |               |                  |                |                |
 *   |  |               v                  |                v                |
 *   |  |  +------------+--------------+   |   +------------+------------+   |
 *   |  |  |       Registrations       |   |   |  FinalizationRecordMap  |   |
 *   |  |  |         weak map          |   |   |           map           |   |
 *   |  |  +---------------------------+   |   +-------------------------+   |
 *   |  |  | Unregister  :   Records   |   |   |  Target  : Finalization-|   |
 *   |  |  |   token     :   object    |   |   |  object  : RecordVector |   |
 *   |  |  +--------------------+------+   |   +----+-------------+------+   |
 *   |  |                       |          |        |             |          |
 *   |  |                       v          |        v             |          |
 *   |  |  +--------------------+------+   |   +----+-----+       |          |
 *   |  |  |       Finalization-       |   |   |  Target  |       |          |
 *   |  |  |    RegistrationsObject    |   |   | JSObject |       |          |
 *   |  |  +-------------+-------------+   |   +----------+       |          |
 *   |  |  |       RecordVector        |   |                      |          |
 *   |  |  +-------------+-------------+   |                      |          |
 *   |  |                |                 |                      |          |
 *   |  |              * v                 |                      |          |
 *   |  |  +-------------+-------------+ * |                      |          |
 *   |  |  | FinalizationRecordObject  +<-------------------------+          |
 *   |  |  +---------------------------+   |                                 |
 *   |  +--+ Registry                  |   |                                 |
 *   |     +---------------------------+   |                                 |
 *   |     | Held value                |   |                                 |
 *   |     +---------------------------+   |                                 |
 *   |                                     |                                 |
 *   +-------------------------------------+---------------------------------+
 *
 * Registering a target with a FinalizationRegistry creates a FinalizationRecord
 * containing the registry and the heldValue. This is added to a vector of
 * records associated with the target, implemented as a map on the target's
 * Zone. All finalization records are treated as GC roots.
 *
 * When a target is registered an unregister token may be supplied. If so, this
 * is also recorded by the registry and is stored in a weak map of
 * registrations. The values of this map are FinalizationRegistrationsObject
 * objects. It's necessary to have another JSObject here because our weak map
 * implementation only supports JS types as values.
 *
 * After a target object has been registered with a finalization registry it is
 * expected that its callback will be called for that object even if the
 * finalization registry itself is no longer reachable from JS. Thus the values
 * of each zone's finalization record map are treated as roots and marked at the
 * start of GC.
 *
 * The finalization record maps are also swept during GC to check for any
 * targets that are dying. For such targets the associated record list is
 * processed and for each record the heldValue is queued on finalization
 * registry. At a later time this causes the client's callback to be run.
 *
 * When targets are unregistered, the registration is looked up in the weakmap
 * and the corresponding records are cleared. These are removed from the zone's
 * record map when it is next swept.
 */

#ifndef builtin_FinalizationRegistryObject_h
#define builtin_FinalizationRegistryObject_h

#include "gc/Barrier.h"
#include "js/GCVector.h"
#include "vm/NativeObject.h"

namespace js {

class FinalizationRegistryObject;
class FinalizationIteratorObject;
class FinalizationRecordObject;
class ObjectWeakMap;

using HandleFinalizationRegistryObject = Handle<FinalizationRegistryObject*>;
using HandleFinalizationRecordObject = Handle<FinalizationRecordObject*>;
using RootedFinalizationRegistryObject = Rooted<FinalizationRegistryObject*>;
using RootedFinalizationIteratorObject = Rooted<FinalizationIteratorObject*>;
using RootedFinalizationRecordObject = Rooted<FinalizationRecordObject*>;

// A finalization record: a pair of finalization registry and held value.
//
// A finalization record represents the registered interest of a finalization
// registry in a target's finalization.
//
// Finalization records are initially 'active' but may be cleared and become
// inactive. This happens when:
//  - the heldValue is passed to the registry's cleanup callback
//  - the registry's unregister method removes the registration
//  - the FinalizationRegistry dies
class FinalizationRecordObject : public NativeObject {
  enum { WeakRegistrySlot = 0, HeldValueSlot, SlotCount };

 public:
  static const JSClass class_;

  // The registry can be a CCW to a FinalizationRegistryObject.
  static FinalizationRecordObject* create(
      JSContext* cx, HandleFinalizationRegistryObject registry,
      HandleValue heldValue);

  // Read weak registry pointer and perform read barrier during GC.
  FinalizationRegistryObject* registryDuringGC(gc::GCRuntime* gc) const;

  Value heldValue() const;
  bool isActive() const;
  void clear();
  bool sweep();

 private:
  static const JSClassOps classOps_;

  static void trace(JSTracer* trc, JSObject* obj);

  FinalizationRegistryObject* registryUnbarriered() const;
};

// A vector of weakly-held FinalizationRecordObjects.
using WeakFinalizationRecordVector =
    GCVector<WeakHeapPtr<FinalizationRecordObject*>, 1, js::ZoneAllocPolicy>;

// A JS object containing a vector of weakly-held FinalizationRecordObjects,
// which holds the records corresponding to the registrations for a particular
// registration token. These are used as the values in the registration
// weakmap. Since the contents of the vector are weak references they are not
// traced.
class FinalizationRegistrationsObject : public NativeObject {
  enum { RecordsSlot = 0, SlotCount };

 public:
  static const JSClass class_;

  static FinalizationRegistrationsObject* create(JSContext* cx);

  WeakFinalizationRecordVector* records();
  const WeakFinalizationRecordVector* records() const;

  bool isEmpty() const;

  bool append(HandleFinalizationRecordObject record);
  void remove(HandleFinalizationRecordObject record);

  void sweep();

 private:
  static const JSClassOps classOps_;

  void* privatePtr() const;

  static void trace(JSTracer* trc, JSObject* obj);
  static void finalize(JSFreeOp* fop, JSObject* obj);
};

using FinalizationRecordVector =
    GCVector<HeapPtr<FinalizationRecordObject*>, 1, js::ZoneAllocPolicy>;

using FinalizationRecordSet =
    GCHashSet<HeapPtrObject, MovableCellHasher<HeapPtrObject>, ZoneAllocPolicy>;

// The FinalizationRegistry object itself.
class FinalizationRegistryObject : public NativeObject {
  enum {
    CleanupCallbackSlot = 0,
    RegistrationsSlot,
    ActiveRecords,
    RecordsToBeCleanedUpSlot,
    IsQueuedForCleanupSlot,
    IsCleanupJobActiveSlot,
    SlotCount
  };

 public:
  static const JSClass class_;
  static const JSClass protoClass_;

  JSObject* cleanupCallback() const;
  ObjectWeakMap* registrations() const;
  FinalizationRecordSet* activeRecords() const;
  FinalizationRecordVector* recordsToBeCleanedUp() const;
  bool isQueuedForCleanup() const;
  bool isCleanupJobActive() const;

  void queueRecordToBeCleanedUp(FinalizationRecordObject* record);
  void setQueuedForCleanup(bool value);
  void setCleanupJobActive(bool value);

  void sweep();

  static bool cleanupQueuedRecords(JSContext* cx,
                                   HandleFinalizationRegistryObject registry,
                                   HandleObject callback = nullptr);

 private:
  static const JSClassOps classOps_;
  static const ClassSpec classSpec_;
  static const JSFunctionSpec methods_[];
  static const JSPropertySpec properties_[];

  static bool construct(JSContext* cx, unsigned argc, Value* vp);
  static bool register_(JSContext* cx, unsigned argc, Value* vp);
  static bool unregister(JSContext* cx, unsigned argc, Value* vp);
  static bool cleanupSome(JSContext* cx, unsigned argc, Value* vp);

  static bool addRegistration(JSContext* cx,
                              HandleFinalizationRegistryObject registry,
                              HandleObject unregisterToken,
                              HandleFinalizationRecordObject record);
  static void removeRegistrationOnError(
      HandleFinalizationRegistryObject registry, HandleObject unregisterToken,
      HandleFinalizationRecordObject record);

  static void trace(JSTracer* trc, JSObject* obj);
  static void finalize(JSFreeOp* fop, JSObject* obj);

  static bool hasRegisteredRecordsToBeCleanedUp(
      HandleFinalizationRegistryObject registry);
};

// An iterator over a finalization registry's queued held values. In the spec
// this is called FinalizationRegistryCleanupIterator.
class FinalizationIteratorObject : public NativeObject {
  enum { FinalizationRegistrySlot = 0, IndexSlot, SlotCount };

 public:
  static const JSClass class_;

  static FinalizationIteratorObject* create(
      JSContext* cx, HandleFinalizationRegistryObject registry);

  FinalizationRegistryObject* finalizationRegistry() const;
  size_t index() const;

  void setIndex(size_t index);
  void clearFinalizationRegistry();

 private:
  friend class GlobalObject;
  static const JSFunctionSpec methods_[];
  static const JSPropertySpec properties_[];

  static bool next(JSContext* cx, unsigned argc, Value* vp);
};

}  // namespace js

#endif /* builtin_FinalizationRegistryObject_h */
