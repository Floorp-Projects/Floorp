/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * FinalizationGroup objects allow a program to register to receive a callback
 * after a 'target' object dies. The callback is passed a 'holdings' value (that
 * hopefully doesn't entrain the target). An 'unregister token' is an object
 * which can be used to remove multiple previous registrations in one go.
 *
 * To arrange this, the following data structures are used:
 *
 *   +--------------------------------+----------------------------+
 *   |  FinalizationGroup compartment |  Target zone / compartment |
 *   |                                |                            |
 *   |      +-------------------+     |    +------------------+    |
 *   |  +-->+ FinalizationGroup |     |    |       Zone       |    |
 *   |  |   +---------+---------+     |    +---------+--------+    |
 *   |  |             |               |              |             |
 *   |  |             |               |              |             |
 *   |  |             v               |              v             |
 *   |  |  +----------+----------+    |   +----------+---------+   |
 *   |  |  |    Registrations    |    |   | FinalizationRecord |   |
 *   |  |  |      weak map       |    |   |        map         |   |
 *   |  |  +---------------------+    |   +--------------------+   |
 *   |  |  | Unregister : Record |    |   |  Target  : Record  |   |
 *   |  |  |   token    :  list  |    |   |          :  list   |   |
 *   |  |  +-----------------+---+    |   +-----+---------+----+   |
 *   |  |                    |        |         |         |        |
 *   |  |             +------+        |         |         |        |
 *   |  |           * v               |         |         |        |
 *   |  |  +----------+----------+ *  |         |         |        |
 *   |  |  | FinalizationRecord  +<-----------------------+        |
 *   |  |  +---------------------+    |         |                  |
 *   |  +--+ Group               |    |         v                  |
 *   |     +---------------------+    |     +---+---------+        |
 *   |     | Holdings            |    |     |   Target    |        |
 *   |     +---------------------+    |     +-------------+        |
 *   |                                |                            |
 *   +--------------------------------+----------------------------+
 *
 * Registering a target with a FinalizationGroup creates a FinalizationRecord
 * containing the group and the holdings. This is added to a vector of records
 * associated with the target, implemented as a map on the target's Zone. All
 * finalization records are treated as GC roots.
 *
 * When a target is registered an unregister token may be supplied. If so, this
 * is also recorded by the group and is stored in a weak map of
 * registrations. The values of this map are FinalizationRecordVector
 * objects. It's necessary to have another JSObject here because our weak map
 * implementation only supports JS types as values.
 *
 * After a target object has been registered with a finalization group it is
 * expected that its callback will be called for that object even if the
 * finalization group itself is no longer reachable from JS. Thus the values of
 * each zone's finalization record map are treated as roots and marked at the
 * start of GC.
 *
 * The finalization record maps are also swept during GC to check for any
 * targets that are dying. For such targets the associated record list is
 * processed and for each record the holdings is queued on finalization
 * group. At a later time this causes the client's callback to be run.
 *
 * When targets are unregistered, the registration is looked up in the weakmap
 * and the corresponding records are cleared. These are removed from the zone's
 * record map when it is next swept.
 */

#ifndef builtin_FinalizationGroupObject_h
#define builtin_FinalizationGroupObject_h

#include "gc/Barrier.h"
#include "js/GCVector.h"
#include "vm/NativeObject.h"

namespace js {

class FinalizationGroupObject;
class FinalizationIteratorObject;
class FinalizationRecordObject;
class ObjectWeakMap;

using HandleFinalizationGroupObject = Handle<FinalizationGroupObject*>;
using HandleFinalizationRecordObject = Handle<FinalizationRecordObject*>;
using RootedFinalizationGroupObject = Rooted<FinalizationGroupObject*>;
using RootedFinalizationIteratorObject = Rooted<FinalizationIteratorObject*>;
using RootedFinalizationRecordObject = Rooted<FinalizationRecordObject*>;

// A finalization record: a pair of finalization group and holdings value.
class FinalizationRecordObject : public NativeObject {
  enum { GroupSlot = 0, HoldingsSlot, SlotCount };

 public:
  static const JSClass class_;

  // The group can be a CCW to a FinalizationGroupObject.
  static FinalizationRecordObject* create(JSContext* cx,
                                          HandleFinalizationGroupObject group,
                                          HandleValue holdings);

  FinalizationGroupObject* group() const;
  Value holdings() const;
  bool wasCleared() const;
  void clear();
};

// A vector of FinalizationRecordObjects.
using FinalizationRecordVector =
    GCVector<HeapPtr<FinalizationRecordObject*>, 1, js::ZoneAllocPolicy>;

// A JS object that wraps a FinalizationRecordVector. Used as the values in the
// registration weakmap.
class FinalizationRecordVectorObject : public NativeObject {
  enum { RecordsSlot = 0, SlotCount };

 public:
  static const JSClass class_;

  static FinalizationRecordVectorObject* create(JSContext* cx);

  FinalizationRecordVector* records();
  const FinalizationRecordVector* records() const;

  bool isEmpty() const;

  bool append(HandleFinalizationRecordObject record);
  void remove(HandleFinalizationRecordObject record);

 private:
  static const JSClassOps classOps_;

  void* privatePtr() const;

  static void trace(JSTracer* trc, JSObject* obj);
  static void finalize(JSFreeOp* fop, JSObject* obj);
};

// The FinalizationGroup object itself.
class FinalizationGroupObject : public NativeObject {
  enum {
    CleanupCallbackSlot = 0,
    RegistrationsSlot,
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
  FinalizationRecordVector* recordsToBeCleanedUp() const;
  bool isQueuedForCleanup() const;
  bool isCleanupJobActive() const;

  void queueRecordToBeCleanedUp(FinalizationRecordObject* record);
  void setQueuedForCleanup(bool value);
  void setCleanupJobActive(bool value);

  static bool cleanupQueuedRecords(JSContext* cx,
                                   HandleFinalizationGroupObject group,
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
                              HandleFinalizationGroupObject group,
                              HandleObject unregisterToken,
                              HandleFinalizationRecordObject record);
  static void removeRegistrationOnError(HandleFinalizationGroupObject group,
                                        HandleObject unregisterToken,
                                        HandleFinalizationRecordObject record);

  static void trace(JSTracer* trc, JSObject* obj);
  static void finalize(JSFreeOp* fop, JSObject* obj);
};

// An iterator over a finalization group's queued holdings. In the spec this is
// called FinalizationGroupCleanupIterator.
class FinalizationIteratorObject : public NativeObject {
  enum { FinalizationGroupSlot = 0, IndexSlot, SlotCount };

 public:
  static const JSClass class_;

  static FinalizationIteratorObject* create(
      JSContext* cx, HandleFinalizationGroupObject group);

  FinalizationGroupObject* finalizationGroup() const;
  size_t index() const;

  void setIndex(size_t index);
  void clearFinalizationGroup();

 private:
  friend class GlobalObject;
  static const JSFunctionSpec methods_[];
  static const JSPropertySpec properties_[];

  static bool next(JSContext* cx, unsigned argc, Value* vp);
};

}  // namespace js

#endif /* builtin_FinalizationGroupObject_h */
