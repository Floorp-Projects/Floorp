/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_JavaScriptShared_h__
#define mozilla_jsipc_JavaScriptShared_h__

#include "mozilla/HashFunctions.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/jsipc/PJavaScript.h"
#include "js/GCHashTable.h"
#include "nsJSUtils.h"

namespace mozilla {
namespace jsipc {

class ObjectId {
  public:
    // Use 47 bits at most, to be safe, since jsval privates are encoded as
    // doubles. See bug 1065811 comment 12 for an explanation.
    static const size_t SERIAL_NUMBER_BITS = 47;
    static const size_t FLAG_BITS = 1;
    static const uint64_t SERIAL_NUMBER_MAX = (uint64_t(1) << SERIAL_NUMBER_BITS) - 1;

    explicit ObjectId(uint64_t serialNumber, bool hasXrayWaiver)
      : serialNumber_(serialNumber), hasXrayWaiver_(hasXrayWaiver)
    {
        if (MOZ_UNLIKELY(serialNumber == 0 || serialNumber > SERIAL_NUMBER_MAX))
            MOZ_CRASH("Bad CPOW Id");
    }

    bool operator==(const ObjectId& other) const {
        bool equal = serialNumber() == other.serialNumber();
        MOZ_ASSERT_IF(equal, hasXrayWaiver() == other.hasXrayWaiver());
        return equal;
    }

    bool isNull() { return !serialNumber_; }

    uint64_t serialNumber() const { return serialNumber_; }
    bool hasXrayWaiver() const { return hasXrayWaiver_; }
    uint64_t serialize() const {
        MOZ_ASSERT(serialNumber(), "Don't send a null ObjectId over IPC");
        return uint64_t((serialNumber() << FLAG_BITS) | ((hasXrayWaiver() ? 1 : 0) << 0));
    }

    static ObjectId nullId() { return ObjectId(); }
    static ObjectId deserialize(uint64_t data) {
        return ObjectId(data >> FLAG_BITS, data & 1);
    }

    // For use with StructGCPolicy.
    void trace(JSTracer*) const {}
    bool needsSweep() const { return false; }

  private:
    ObjectId() : serialNumber_(0), hasXrayWaiver_(false) {}

    uint64_t serialNumber_ : SERIAL_NUMBER_BITS;
    bool hasXrayWaiver_ : 1;
};

class JavaScriptShared;

// DefaultHasher<T> requires that T coerce to an integral type. We could make
// ObjectId do that, but doing so would weaken our type invariants, so we just
// reimplement it manually.
struct ObjectIdHasher
{
    typedef ObjectId Lookup;
    static js::HashNumber hash(const Lookup& l) {
        return mozilla::HashGeneric(l.serialize());
    }
    static bool match(const ObjectId& k, const ObjectId& l) {
        return k == l;
    }
    static void rekey(ObjectId& k, const ObjectId& newKey) {
        k = newKey;
    }
};

// Map ids -> JSObjects
class IdToObjectMap
{
    typedef js::HashMap<ObjectId, JS::Heap<JSObject*>, ObjectIdHasher, js::SystemAllocPolicy> Table;

  public:
    IdToObjectMap();

    bool init();
    void trace(JSTracer* trc, uint64_t minimumId = 0);
    void sweep();

    bool add(ObjectId id, JSObject* obj);
    JSObject* find(ObjectId id);
    JSObject* findPreserveColor(ObjectId id);
    void remove(ObjectId id);

    void clear();
    bool empty() const;

#ifdef DEBUG
    bool has(const ObjectId& id, const JSObject* obj) const;
#endif

  private:
    Table table_;
};

// Map JSObjects -> ids
class ObjectToIdMap
{
    using Hasher = js::MovableCellHasher<JS::Heap<JSObject*>>;
    using Table = JS::GCHashMap<JS::Heap<JSObject*>, ObjectId, Hasher, js::SystemAllocPolicy>;

  public:
    bool init();
    void trace(JSTracer* trc);
    void sweep();

    bool add(JSContext* cx, JSObject* obj, ObjectId id);
    ObjectId find(JSObject* obj);
    void remove(JSObject* obj);
    void clear();

  private:
    Table table_;
};

class Logging;

class JavaScriptShared : public CPOWManager
{
  public:
    JavaScriptShared();
    virtual ~JavaScriptShared();

    bool init();

    void decref();
    void incref();

    bool Unwrap(JSContext* cx, const InfallibleTArray<CpowEntry>& aCpows, JS::MutableHandleObject objp);
    bool Wrap(JSContext* cx, JS::HandleObject aObj, InfallibleTArray<CpowEntry>* outCpows);

  protected:
    bool toVariant(JSContext* cx, JS::HandleValue from, JSVariant* to);
    bool fromVariant(JSContext* cx, const JSVariant& from, JS::MutableHandleValue to);

    bool toJSIDVariant(JSContext* cx, JS::HandleId from, JSIDVariant* to);
    bool fromJSIDVariant(JSContext* cx, const JSIDVariant& from, JS::MutableHandleId to);

    bool toSymbolVariant(JSContext* cx, JS::Symbol* sym, SymbolVariant* symVarp);
    JS::Symbol* fromSymbolVariant(JSContext* cx, const SymbolVariant& symVar);

    bool fromDescriptor(JSContext* cx, JS::Handle<JS::PropertyDescriptor> desc,
                        PPropertyDescriptor* out);
    bool toDescriptor(JSContext* cx, const PPropertyDescriptor& in,
                      JS::MutableHandle<JS::PropertyDescriptor> out);

    bool toObjectOrNullVariant(JSContext* cx, JSObject* obj, ObjectOrNullVariant* objVarp);
    JSObject* fromObjectOrNullVariant(JSContext* cx, const ObjectOrNullVariant& objVar);

    bool convertIdToGeckoString(JSContext* cx, JS::HandleId id, nsString* to);
    bool convertGeckoStringToId(JSContext* cx, const nsString& from, JS::MutableHandleId id);

    virtual bool toObjectVariant(JSContext* cx, JSObject* obj, ObjectVariant* objVarp) = 0;
    virtual JSObject* fromObjectVariant(JSContext* cx, const ObjectVariant& objVar) = 0;

    static void ConvertID(const nsID& from, JSIID* to);
    static void ConvertID(const JSIID& from, nsID* to);

    JSObject* findCPOWById(const ObjectId& objId);
    JSObject* findCPOWByIdPreserveColor(const ObjectId& objId);
    JSObject* findObjectById(JSContext* cx, const ObjectId& objId);

#ifdef DEBUG
    bool hasCPOW(const ObjectId& objId, const JSObject* obj) {
        MOZ_ASSERT(obj);
        return findCPOWByIdPreserveColor(objId) == obj;
    }
#endif

    static bool LoggingEnabled() { return sLoggingEnabled; }
    static bool StackLoggingEnabled() { return sStackLoggingEnabled; }

    friend class Logging;

    virtual bool isParent() = 0;

    virtual JSObject* scopeForTargetObjects() = 0;

  protected:
    uintptr_t refcount_;

    IdToObjectMap objects_;
    IdToObjectMap cpows_;

    uint64_t nextSerialNumber_;

    // nextCPOWNumber_ should be the value of nextSerialNumber_ in the other
    // process. The next new CPOW we get should have this serial number.
    uint64_t nextCPOWNumber_;

    // CPOW references can be weak, and any object we store in a map may be
    // GCed (at which point the CPOW will report itself "dead" to the owner).
    // This means that we don't want to store any js::Wrappers in the CPOW map,
    // because CPOW will die if the wrapper is GCed, even if the underlying
    // object is still alive.
    //
    // This presents a tricky situation for Xray waivers, since they're normally
    // represented as a special same-compartment wrapper. We have to strip them
    // off before putting them in the id-to-object and object-to-id maps, so we
    // need a way of distinguishing them at lookup-time.
    //
    // For the id-to-object map, we encode waiver-or-not information into the id
    // itself, which lets us do the right thing when accessing the object.
    //
    // For the object-to-id map, we just keep two maps, one for each type.
    ObjectToIdMap unwaivedObjectIds_;
    ObjectToIdMap waivedObjectIds_;
    ObjectToIdMap& objectIdMap(bool waiver) {
        return waiver ? waivedObjectIds_ : unwaivedObjectIds_;
    }

    static bool sLoggingInitialized;
    static bool sLoggingEnabled;
    static bool sStackLoggingEnabled;
};

} // namespace jsipc
} // namespace mozilla

#endif
