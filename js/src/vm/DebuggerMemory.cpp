/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/DebuggerMemory.h"

#include "mozilla/Maybe.h"
#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

#include <stdlib.h>

#include "jsalloc.h"
#include "jscompartment.h"

#include "builtin/MapObject.h"
#include "gc/Marking.h"
#include "js/Debug.h"
#include "js/TracingAPI.h"
#include "js/UbiNode.h"
#include "js/UbiNodeTraverse.h"
#include "js/Utility.h"
#include "vm/Debugger.h"
#include "vm/GlobalObject.h"
#include "vm/SavedStacks.h"

#include "vm/Debugger-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

using JS::ubi::BreadthFirst;
using JS::ubi::Edge;
using JS::ubi::Node;

using mozilla::Forward;
using mozilla::Maybe;
using mozilla::Move;
using mozilla::Nothing;
using mozilla::UniquePtr;

/* static */ DebuggerMemory*
DebuggerMemory::create(JSContext* cx, Debugger* dbg)
{
    Value memoryProtoValue = dbg->object->getReservedSlot(Debugger::JSSLOT_DEBUG_MEMORY_PROTO);
    RootedObject memoryProto(cx, &memoryProtoValue.toObject());
    RootedNativeObject memory(cx, NewNativeObjectWithGivenProto(cx, &class_, memoryProto));
    if (!memory)
        return nullptr;

    dbg->object->setReservedSlot(Debugger::JSSLOT_DEBUG_MEMORY_INSTANCE, ObjectValue(*memory));
    memory->setReservedSlot(JSSLOT_DEBUGGER, ObjectValue(*dbg->object));

    return &memory->as<DebuggerMemory>();
}

Debugger*
DebuggerMemory::getDebugger()
{
    const Value& dbgVal = getReservedSlot(JSSLOT_DEBUGGER);
    return Debugger::fromJSObject(&dbgVal.toObject());
}

/* static */ bool
DebuggerMemory::construct(JSContext* cx, unsigned argc, Value* vp)
{
    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                         "Debugger.Memory");
    return false;
}

/* static */ const Class DebuggerMemory::class_ = {
    "Memory",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_COUNT)
};

/* static */ DebuggerMemory*
DebuggerMemory::checkThis(JSContext* cx, CallArgs& args, const char* fnName)
{
    const Value& thisValue = args.thisv();

    if (!thisValue.isObject()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT, InformalValueTypeName(thisValue));
        return nullptr;
    }

    JSObject& thisObject = thisValue.toObject();
    if (!thisObject.is<DebuggerMemory>()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             class_.name, fnName, thisObject.getClass()->name);
        return nullptr;
    }

    // Check for Debugger.Memory.prototype, which has the same class as
    // Debugger.Memory instances, however doesn't actually represent an instance
    // of Debugger.Memory. It is the only object that is<DebuggerMemory>() but
    // doesn't have a Debugger instance.
    if (thisObject.as<DebuggerMemory>().getReservedSlot(JSSLOT_DEBUGGER).isUndefined()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             class_.name, fnName, "prototype object");
        return nullptr;
    }

    return &thisObject.as<DebuggerMemory>();
}

/**
 * Get the |DebuggerMemory*| from the current this value and handle any errors
 * that might occur therein.
 *
 * These parameters must already exist when calling this macro:
 * - JSContext* cx
 * - unsigned argc
 * - Value* vp
 * - const char* fnName
 * These parameters will be defined after calling this macro:
 * - CallArgs args
 * - DebuggerMemory* memory (will be non-null)
 */
#define THIS_DEBUGGER_MEMORY(cx, argc, vp, fnName, args, memory)        \
    CallArgs args = CallArgsFromVp(argc, vp);                           \
    Rooted<DebuggerMemory*> memory(cx, checkThis(cx, args, fnName));    \
    if (!memory)                                                        \
        return false

static bool
undefined(CallArgs& args)
{
    args.rval().setUndefined();
    return true;
}

/* static */ bool
DebuggerMemory::setTrackingAllocationSites(JSContext* cx, unsigned argc, Value* vp)
{
    THIS_DEBUGGER_MEMORY(cx, argc, vp, "(set trackingAllocationSites)", args, memory);
    if (!args.requireAtLeast(cx, "(set trackingAllocationSites)", 1))
        return false;

    Debugger* dbg = memory->getDebugger();
    bool enabling = ToBoolean(args[0]);

    if (enabling == dbg->trackingAllocationSites)
        return undefined(args);

    dbg->trackingAllocationSites = enabling;

    if (!dbg->enabled)
        return undefined(args);

    if (enabling) {
        if (!dbg->addAllocationsTrackingForAllDebuggees(cx)) {
            dbg->trackingAllocationSites = false;
            return false;
        }
    } else {
        dbg->removeAllocationsTrackingForAllDebuggees();
    }

    return undefined(args);
}

/* static */ bool
DebuggerMemory::getTrackingAllocationSites(JSContext* cx, unsigned argc, Value* vp)
{
    THIS_DEBUGGER_MEMORY(cx, argc, vp, "(get trackingAllocationSites)", args, memory);
    args.rval().setBoolean(memory->getDebugger()->trackingAllocationSites);
    return true;
}

/* static */ bool
DebuggerMemory::drainAllocationsLog(JSContext* cx, unsigned argc, Value* vp)
{
    THIS_DEBUGGER_MEMORY(cx, argc, vp, "drainAllocationsLog", args, memory);
    Debugger* dbg = memory->getDebugger();

    if (!dbg->trackingAllocationSites) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_NOT_TRACKING_ALLOCATIONS,
                             "drainAllocationsLog");
        return false;
    }

    size_t length = dbg->allocationsLogLength;

    RootedArrayObject result(cx, NewDenseFullyAllocatedArray(cx, length));
    if (!result)
        return false;
    result->ensureDenseInitializedLength(cx, 0, length);

    for (size_t i = 0; i < length; i++) {
        RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx));
        if (!obj)
            return false;

        // Don't pop the AllocationSite yet. The queue's links are followed by
        // the GC to find the AllocationSite, but are not barriered, so we must
        // edit them with great care. Use the queue entry in place, and then
        // pop and delete together.
        Debugger::AllocationSite* allocSite = dbg->allocationsLog.getFirst();
        RootedValue frame(cx, ObjectOrNullValue(allocSite->frame));
        if (!DefineProperty(cx, obj, cx->names().frame, frame))
            return false;

        RootedValue timestampValue(cx, NumberValue(allocSite->when));
        if (!DefineProperty(cx, obj, cx->names().timestamp, timestampValue))
            return false;

        RootedString className(cx, Atomize(cx, allocSite->className, strlen(allocSite->className)));
        if (!className)
            return false;
        RootedValue classNameValue(cx, StringValue(className));
        if (!DefineProperty(cx, obj, cx->names().class_, classNameValue))
            return false;

        RootedValue ctorName(cx, NullValue());
        if (allocSite->ctorName)
            ctorName.setString(allocSite->ctorName);
        if (!DefineProperty(cx, obj, cx->names().constructor, ctorName))
            return false;

        result->setDenseElement(i, ObjectValue(*obj));

        // Pop the front queue entry, and delete it immediately, so that
        // the GC sees the AllocationSite's RelocatablePtr barriers run
        // atomically with the change to the graph (the queue link).
        MOZ_ALWAYS_TRUE(dbg->allocationsLog.popFirst() == allocSite);
        js_delete(allocSite);
    }

    dbg->allocationsLogOverflowed = false;
    dbg->allocationsLogLength = 0;
    args.rval().setObject(*result);
    return true;
}

/* static */ bool
DebuggerMemory::getMaxAllocationsLogLength(JSContext* cx, unsigned argc, Value* vp)
{
    THIS_DEBUGGER_MEMORY(cx, argc, vp, "(get maxAllocationsLogLength)", args, memory);
    args.rval().setInt32(memory->getDebugger()->maxAllocationsLogLength);
    return true;
}

/* static */ bool
DebuggerMemory::setMaxAllocationsLogLength(JSContext* cx, unsigned argc, Value* vp)
{
    THIS_DEBUGGER_MEMORY(cx, argc, vp, "(set maxAllocationsLogLength)", args, memory);
    if (!args.requireAtLeast(cx, "(set maxAllocationsLogLength)", 1))
        return false;

    int32_t max;
    if (!ToInt32(cx, args[0], &max))
        return false;

    if (max < 1) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
                             "(set maxAllocationsLogLength)'s parameter",
                             "not a positive integer");
        return false;
    }

    Debugger* dbg = memory->getDebugger();
    dbg->maxAllocationsLogLength = max;

    while (dbg->allocationsLogLength > dbg->maxAllocationsLogLength) {
        js_delete(dbg->allocationsLog.getFirst());
        dbg->allocationsLogLength--;
    }

    args.rval().setUndefined();
    return true;
}

/* static */ bool
DebuggerMemory::getAllocationSamplingProbability(JSContext* cx, unsigned argc, Value* vp)
{
    THIS_DEBUGGER_MEMORY(cx, argc, vp, "(get allocationSamplingProbability)", args, memory);
    args.rval().setDouble(memory->getDebugger()->allocationSamplingProbability);
    return true;
}

/* static */ bool
DebuggerMemory::setAllocationSamplingProbability(JSContext* cx, unsigned argc, Value* vp)
{
    THIS_DEBUGGER_MEMORY(cx, argc, vp, "(set allocationSamplingProbability)", args, memory);
    if (!args.requireAtLeast(cx, "(set allocationSamplingProbability)", 1))
        return false;

    double probability;
    if (!ToNumber(cx, args[0], &probability))
        return false;

    if (probability < 0.0 || probability > 1.0) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
                             "(set allocationSamplingProbability)'s parameter",
                             "not a number between 0 and 1");
        return false;
    }

    memory->getDebugger()->allocationSamplingProbability = probability;
    args.rval().setUndefined();
    return true;
}

/* static */ bool
DebuggerMemory::getAllocationsLogOverflowed(JSContext* cx, unsigned argc, Value* vp)
{
    THIS_DEBUGGER_MEMORY(cx, argc, vp, "(get allocationsLogOverflowed)", args, memory);
    args.rval().setBoolean(memory->getDebugger()->allocationsLogOverflowed);
    return true;
}

/* static */ bool
DebuggerMemory::getOnGarbageCollection(JSContext* cx, unsigned argc, Value* vp)
{
    THIS_DEBUGGER_MEMORY(cx, argc, vp, "(get onGarbageCollection)", args, memory);
    return Debugger::getHookImpl(cx, args, *memory->getDebugger(), Debugger::OnGarbageCollection);
}

/* static */ bool
DebuggerMemory::setOnGarbageCollection(JSContext* cx, unsigned argc, Value* vp)
{
    THIS_DEBUGGER_MEMORY(cx, argc, vp, "(set onGarbageCollection)", args, memory);
    return Debugger::setHookImpl(cx, args, *memory->getDebugger(), Debugger::OnGarbageCollection);
}


/* Debugger.Memory.prototype.takeCensus */

JS_PUBLIC_API(void)
JS::dbg::SetDebuggerMallocSizeOf(JSRuntime* rt, mozilla::MallocSizeOf mallocSizeOf)
{
    rt->debuggerMallocSizeOf = mallocSizeOf;
}

JS_PUBLIC_API(mozilla::MallocSizeOf)
JS::dbg::GetDebuggerMallocSizeOf(JSRuntime* rt)
{
    return rt->debuggerMallocSizeOf;
}

namespace js {
namespace dbg {

// We want to summarize the results of a census with counts broken down
// according to criteria selected by the code that is requesting the census. For
// example, the following breakdown might give an interesting overview of the
// heap:
//
//   - all nodes
//     - objects
//       - objects with a specific [[Class]] *
//     - strings
//     - scripts
//     - all other Node types
//       - nodes with a specific ubi::Node::typeName *
//
// Obviously, the parts of this tree marked with * represent many separate
// counts, depending on how many distinct [[Class]] values and ubi::Node type
// names we encounter.
//
// We parse the 'breakdown' argument to takeCensus and build a tree of CountType
// nodes of the sort shown above. For example, for the breakdown shown in the
// Debugger.Memory.prototype.takeCensus, documentation:
//
//    {
//      by: "coarseType",
//      objects: { by: "objectClass" },
//      other:    { by: "internalType" }
//    }
//
// we would build the following tree of CountType subclasses:
//
//    ByCoarseType
//      objects: ByObjectClass
//        each class: SimpleCount
//      scripts: SimpleCount
//      strings: SimpleCount
//      other: ByUbinodeType
//        each type: SimpleCount
//
// The interior nodes are all breakdown types that categorize nodes according to
// one characteristics or another; and the leaf nodes are all SimpleType.
//
// Each CountType has its own concrete C++ type that holds the counts it
// produces. SimpleCount::Count just holds totals. ByObjectClass::Count has a
// hash table whose keys are object class names and whose values are counts of
// some other type (in the example above, SimpleCount).
//
// To keep actual count nodes small, they have no vtable. Instead, each count
// points to its CountType, which knows how to carry out all the operations we
// need on a Count. A CountType can produce new count nodes; process nodes as we
// visit them; build a JS object reporting the results; and destruct count
// nodes.
//
// The takeCensus function works in three phases:
//
// 1) We examine the 'breakdown' property of our 'options' argument, and
//    use that to build a CountType tree.
//
// 2) We create a count node for the root of our CountType tree, and then walk
//    the heap, counting each node we find, expanding our tree of counts as we
//    go.
//
// 3) We walk the tree of counts and produce JavaScript objects reporting the
//    accumulated results.

// Common data for a census traversal, shared across all CountType nodes.
struct Census {
    JSContext* const cx;
    JS::ZoneSet debuggeeZones;
    Zone* atomsZone;

    explicit Census(JSContext* cx) : cx(cx), atomsZone(nullptr) { }

    bool init() {
        AutoLockForExclusiveAccess lock(cx);
        atomsZone = cx->runtime()->atomsCompartment()->zone();
        return debuggeeZones.init();
    }

    // A 'new' work-alike that behaves like TempAllocPolicy: report OOM on this
    // census's context, but don't charge the memory allocated to our context's
    // GC pressure counters.
    template<typename T, typename... Args>
    T* new_(Args&&... args) MOZ_HEAP_ALLOCATOR {
        void* memory = js_malloc(sizeof(T));
        if (MOZ_UNLIKELY(!memory)) {
            memory = static_cast<T*>(cx->onOutOfMemory(AllocFunction::Malloc, sizeof(T)));
            if (!memory)
                return nullptr;
        }
        return new(memory) T(Forward<Args>(args)...);
    }
};

class CountBase;

struct CountDeleter {
    void operator()(CountBase*);
};

typedef UniquePtr<CountBase, CountDeleter> CountBasePtr;

// Abstract base class for CountType nodes.
struct CountType {
    explicit CountType(Census& census) : census(census) { }
    virtual ~CountType() { }

    // Return a fresh node for the count tree that categorizes nodes according
    // to this type. Return a nullptr on OOM.
    virtual CountBasePtr makeCount() = 0;

    // Trace |count| and all its children, for garbage collection.
    virtual void traceCount(CountBase& count, JSTracer* trc) = 0;

    // Destruct a count tree node that this type instance constructed.
    virtual void destructCount(CountBase& count) = 0;

    // Implement the 'count' method for counts returned by this CountType
    // instance's 'newCount' method.
    virtual bool count(CountBase& count, const Node& node) = 0;

    // Implement the 'report' method for counts returned by this CountType
    // instance's 'newCount' method.
    virtual bool report(CountBase& count, MutableHandleValue report) = 0;

  protected:
    Census& census;
};

typedef UniquePtr<CountType, JS::DeletePolicy<CountType>> CountTypePtr;

// An abstract base class for count tree nodes.
class CountBase {
    // In lieu of a vtable, each CountBase points to its type, which
    // carries not only the implementations of the CountBase methods, but also
    // additional parameters for the type's behavior, as specified in the
    // breakdown argument passed to takeCensus.
    CountType& type;

  protected:
    ~CountBase() { }

  public:
    explicit CountBase(CountType& type) : type(type), total_(0) { }

    // Categorize and count |node| as appropriate for this count's type.
    bool count(const Node& node) { return type.count(*this, node); }

    // Construct a JavaScript object reporting the counts recorded in this
    // count, and store it in |report|. Return true on success, or false on
    // failure.
    bool report(MutableHandleValue report) { return type.report(*this, report); }

    // Down-cast this CountBase to its true type, based on its type, and run
    // its destructor.
    void destruct() { return type.destructCount(*this); }

    // Trace this count for garbage collection.
    void trace(JSTracer* trc) { type.traceCount(*this, trc); }

    size_t total_;
};

class RootedCount : JS::CustomAutoRooter {
    CountBasePtr count;

    void trace(JSTracer* trc) override { count->trace(trc); }

  public:
    RootedCount(JSContext* cx, CountBasePtr&& count)
        : CustomAutoRooter(cx),
          count(Move(count))
          { }
    CountBase* operator->() const { return count.get(); }
    explicit operator bool() const { return count.get(); }
    operator CountBasePtr&() { return count; }
};

void
CountDeleter::operator()(CountBase* ptr)
{
    if (!ptr)
        return;

    // Downcast to our true type and destruct, as guided by our CountType
    // pointer.
    ptr->destruct();
    js_free(ptr);
}

// The simplest type: just count everything.
class SimpleCount : public CountType {

    struct Count : CountBase {
        size_t totalBytes_;

        explicit Count(SimpleCount& count)
          : CountBase(count),
            totalBytes_(0)
        { }
    };

    UniquePtr<char16_t[], JS::FreePolicy> label;
    bool reportCount : 1;
    bool reportBytes : 1;

  public:
    SimpleCount(Census& census,
                UniquePtr<char16_t[], JS::FreePolicy>& label,
                bool reportCount=true,
                bool reportBytes=true)
      : CountType(census),
        label(Move(label)),
        reportCount(reportCount),
        reportBytes(reportBytes)
    { }

    explicit SimpleCount(Census& census)
        : CountType(census),
          label(nullptr),
          reportCount(true),
          reportBytes(true)
    { }

    CountBasePtr makeCount() override {
        return CountBasePtr(census.new_<Count>(*this));
    }

    void traceCount(CountBase& countBase, JSTracer* trc) override { }

    void destructCount(CountBase& countBase) override {
        Count& count = static_cast<Count&>(countBase);
        count.~Count();
    }

    bool count(CountBase& countBase, const Node& node) override {
        Count& count = static_cast<Count&>(countBase);
        count.total_++;
        if (reportBytes)
            count.totalBytes_ += node.size(census.cx->runtime()->debuggerMallocSizeOf);
        return true;
    }

    bool report(CountBase& countBase, MutableHandleValue report) override {
        Count& count = static_cast<Count&>(countBase);

        RootedPlainObject obj(census.cx, NewBuiltinClassInstance<PlainObject>(census.cx));
        if (!obj)
            return false;

        RootedValue countValue(census.cx, NumberValue(count.total_));
        if (reportCount && !DefineProperty(census.cx, obj, census.cx->names().count, countValue))
            return false;

        RootedValue bytesValue(census.cx, NumberValue(count.totalBytes_));
        if (reportBytes && !DefineProperty(census.cx, obj, census.cx->names().bytes, bytesValue))
            return false;

        if (label) {
            JSString* labelString = JS_NewUCStringCopyZ(census.cx, label.get());
            if (!labelString)
                return false;
            RootedValue labelValue(census.cx, StringValue(labelString));
            if (!DefineProperty(census.cx, obj, census.cx->names().label, labelValue))
                return false;
        }

        report.setObject(*obj);
        return true;
    }
};

// A type that categorizes nodes by their JavaScript type --- 'objects',
// 'strings', 'scripts', and 'other' --- and then passes the nodes to child
// types.
//
// Implementation details of scripts like jitted code are counted under
// 'scripts'.
class ByCoarseType : public CountType {
    CountTypePtr objects;
    CountTypePtr scripts;
    CountTypePtr strings;
    CountTypePtr other;

    struct Count : CountBase {
        Count(CountType& type,
              CountBasePtr& objects,
              CountBasePtr& scripts,
              CountBasePtr& strings,
              CountBasePtr& other)
          : CountBase(type),
            objects(Move(objects)),
            scripts(Move(scripts)),
            strings(Move(strings)),
            other(Move(other))
        { }

        CountBasePtr objects;
        CountBasePtr scripts;
        CountBasePtr strings;
        CountBasePtr other;
    };

  public:
    ByCoarseType(Census& census,
                 CountTypePtr& objects,
                 CountTypePtr& scripts,
                 CountTypePtr& strings,
                 CountTypePtr& other)
      : CountType(census),
        objects(Move(objects)),
        scripts(Move(scripts)),
        strings(Move(strings)),
        other(Move(other))
    { }

    CountBasePtr makeCount() override {
        CountBasePtr objectsCount(objects->makeCount());
        CountBasePtr scriptsCount(scripts->makeCount());
        CountBasePtr stringsCount(strings->makeCount());
        CountBasePtr otherCount(other->makeCount());

        if (!objectsCount || !scriptsCount || !stringsCount || !otherCount)
            return CountBasePtr(nullptr);

        return CountBasePtr(census.new_<Count>(*this,
                                               objectsCount,
                                               scriptsCount,
                                               stringsCount,
                                               otherCount));
    }

    void traceCount(CountBase& countBase, JSTracer* trc) override {
        Count& count = static_cast<Count&>(countBase);
        count.objects->trace(trc);
        count.scripts->trace(trc);
        count.strings->trace(trc);
        count.other->trace(trc);
    }

    void destructCount(CountBase& countBase) override {
        Count& count = static_cast<Count&>(countBase);
        count.~Count();
    }

    bool count(CountBase& countBase, const Node& node) override {
        Count& count = static_cast<Count&>(countBase);
        count.total_++;

        if (node.is<JSObject>())
            return count.objects->count(node);
        if (node.is<JSScript>() || node.is<LazyScript>() || node.is<jit::JitCode>())
            return count.scripts->count(node);
        if (node.is<JSString>())
            return count.strings->count(node);
        return count.other->count(node);
    }

    bool report(CountBase& countBase, MutableHandleValue report) override {
        Count& count = static_cast<Count&>(countBase);
        JSContext* cx = census.cx;

        RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx));
        if (!obj)
            return false;

        RootedValue objectsReport(cx);
        if (!count.objects->report(&objectsReport) ||
            !DefineProperty(cx, obj, cx->names().objects, objectsReport))
            return false;

        RootedValue scriptsReport(cx);
        if (!count.scripts->report(&scriptsReport) ||
            !DefineProperty(cx, obj, cx->names().scripts, scriptsReport))
            return false;

        RootedValue stringsReport(cx);
        if (!count.strings->report(&stringsReport) ||
            !DefineProperty(cx, obj, cx->names().strings, stringsReport))
            return false;

        RootedValue otherReport(cx);
        if (!count.other->report(&otherReport) ||
            !DefineProperty(cx, obj, cx->names().other, otherReport))
            return false;

        report.setObject(*obj);
        return true;
    }
};

// Comparison function for sorting hash table entries by total count. The
// arguments are doubly indirect: they're pointers to elements in an array of
// pointers to table entries.
template<typename Entry>
static int compareEntries(const void* lhsVoid, const void* rhsVoid) {
    size_t lhs = (*static_cast<const Entry* const*>(lhsVoid))->value()->total_;
    size_t rhs = (*static_cast<const Entry* const*>(rhsVoid))->value()->total_;

    // qsort sorts in "ascending" order, so we should describe entries with
    // smaller counts as being "greater than" entries with larger counts. We
    // don't want to just subtract the counts, as they're unsigned.
    if (lhs < rhs)
        return 1;
    if (lhs > rhs)
        return -1;
    return 0;
}

// A type that categorizes nodes that are JSObjects by their class name,
// and places all other nodes in an 'other' category.
class ByObjectClass : public CountType {
    // A hash policy that compares C strings.
    struct HashPolicy {
        typedef const char* Lookup;
        static js::HashNumber hash(Lookup l) { return mozilla::HashString(l); }
        static bool match(const char* key, Lookup lookup) {
            return strcmp(key, lookup) == 0;
        }
    };

    // A table mapping class names to their counts. Note that we treat js::Class
    // instances with the same name as equal keys. If you have several
    // js::Classes with equal names (and we do; as of this writing there were
    // six named "Object"), you will get several different js::Classes being
    // counted in the same table entry.
    typedef HashMap<const char*, CountBasePtr, HashPolicy, SystemAllocPolicy> Table;
    typedef Table::Entry Entry;

    struct Count : public CountBase {
        Table table;
        CountBasePtr other;

        Count(CountType& type, CountBasePtr& other)
          : CountBase(type),
            other(Move(other))
        { }

        bool init() { return table.init(); }
    };

    CountTypePtr classesType;
    CountTypePtr otherType;

  public:
    ByObjectClass(Census& census,
                  CountTypePtr& classesType,
                  CountTypePtr& otherType)
        : CountType(census),
          classesType(Move(classesType)),
          otherType(Move(otherType))
    { }

    CountBasePtr makeCount() override {
        CountBasePtr otherCount(otherType->makeCount());
        if (!otherCount)
            return nullptr;

        UniquePtr<Count> count(census.new_<Count>(*this, otherCount));
        if (!count || !count->init())
            return nullptr;

        return CountBasePtr(count.release());
    }

    void traceCount(CountBase& countBase, JSTracer* trc) override {
        Count& count = static_cast<Count&>(countBase);
        for (Table::Range r = count.table.all(); !r.empty(); r.popFront())
            r.front().value()->trace(trc);
        count.other->trace(trc);
    }

    void destructCount(CountBase& countBase) override {
        Count& count = static_cast<Count&>(countBase);
        count.~Count();
    }

    bool count(CountBase& countBase, const Node& node) override {
        Count& count = static_cast<Count&>(countBase);
        count.total_++;

        const char* className = node.jsObjectClassName();
        if (!className)
            return count.other->count(node);

        Table::AddPtr p = count.table.lookupForAdd(className);
        if (!p) {
            CountBasePtr classCount(classesType->makeCount());
            if (!classCount || !count.table.add(p, className, Move(classCount)))
                return false;
        }
        return p->value()->count(node);
    }

    bool report(CountBase& countBase, MutableHandleValue report) override {
        Count& count = static_cast<Count&>(countBase);
        JSContext* cx = census.cx;

        // Build a vector of pointers to entries; sort by total; and then use
        // that to build the result object. This makes the ordering of entries
        // more interesting, and a little less non-deterministic.
        mozilla::Vector<Entry*> entries;
        if (!entries.reserve(count.table.count()))
            return false;
        for (Table::Range r = count.table.all(); !r.empty(); r.popFront())
            entries.infallibleAppend(&r.front());
        qsort(entries.begin(), entries.length(), sizeof(*entries.begin()), compareEntries<Entry>);

        // Now build the result by iterating over the sorted vector.
        RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx));
        if (!obj)
            return false;
        for (Entry** entryPtr = entries.begin(); entryPtr < entries.end(); entryPtr++) {
            Entry& entry = **entryPtr;
            CountBasePtr& classCount = entry.value();
            RootedValue classReport(cx);
            if (!classCount->report(&classReport))
                return false;

            const char* name = entry.key();
            MOZ_ASSERT(name);
            JSAtom* atom = Atomize(cx, name, strlen(name));
            if (!atom)
                return false;
            RootedId entryId(cx, AtomToId(atom));

#ifdef DEBUG
            // We have multiple js::Classes out there with the same name (for
            // example, JSObject::class_, Debugger.Object, and CollatorClass are
            // all "Object"), so let's make sure our hash table treats them all
            // as equivalent.
            bool has;
            if (!HasOwnProperty(cx, obj, entryId, &has))
                return false;
            if (has) {
                fprintf(stderr, "already has own property '%s'\n", name);
                MOZ_ASSERT(!has);
            }
#endif

            if (!DefineProperty(cx, obj, entryId, classReport))
                return false;
        }

        RootedValue otherReport(cx);
        if (!count.other->report(&otherReport) ||
            !DefineProperty(cx, obj, cx->names().other, otherReport))
            return false;

        report.setObject(*obj);
        return true;
    }
};


// A count type that categorizes nodes by their ubi::Node::typeName.
class ByUbinodeType : public CountType {
    // Note that, because ubi::Node::typeName promises to return a specific
    // pointer, not just any string whose contents are correct, we can use their
    // addresses as hash table keys.
    typedef HashMap<const char16_t*, CountBasePtr, DefaultHasher<const char16_t*>,
                    SystemAllocPolicy> Table;
    typedef Table::Entry Entry;

    struct Count: public CountBase {
        Table table;

        explicit Count(CountType& type) : CountBase(type) { }

        bool init() { return table.init(); }
    };

    CountTypePtr entryType;

  public:
    ByUbinodeType(Census& census, CountTypePtr& entryType)
      : CountType(census),
        entryType(Move(entryType))
    { }

    CountBasePtr makeCount() override {
        UniquePtr<Count> count(census.new_<Count>(*this));
        if (!count || !count->init())
            return nullptr;

        return CountBasePtr(count.release());
    }

    void traceCount(CountBase& countBase, JSTracer* trc) override {
        Count& count = static_cast<Count&>(countBase);
        for (Table::Range r = count.table.all(); !r.empty(); r.popFront())
            r.front().value()->trace(trc);
    }

    void destructCount(CountBase& countBase) override {
        Count& count = static_cast<Count&>(countBase);
        count.~Count();
    }

    bool count(CountBase& countBase, const Node& node) {
        Count& count = static_cast<Count&>(countBase);
        count.total_++;

        const char16_t* key = node.typeName();
        Table::AddPtr p = count.table.lookupForAdd(key);
        if (!p) {
            CountBasePtr typesCount(entryType->makeCount());
            if (!typesCount || !count.table.add(p, key, Move(typesCount)))
                return false;
        }
        return p->value()->count(node);
    }

    bool report(CountBase& countBase, MutableHandleValue report) override {
        Count& count = static_cast<Count&>(countBase);
        JSContext* cx = census.cx;

        // Build a vector of pointers to entries; sort by total; and then use
        // that to build the result object. This makes the ordering of entries
        // more interesting, and a little less non-deterministic.
        mozilla::Vector<Entry*> entries;
        if (!entries.reserve(count.table.count()))
            return false;
        for (Table::Range r = count.table.all(); !r.empty(); r.popFront())
            entries.infallibleAppend(&r.front());
        qsort(entries.begin(), entries.length(), sizeof(*entries.begin()), compareEntries<Entry>);

        // Now build the result by iterating over the sorted vector.
        RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx));
        if (!obj)
            return false;
        for (Entry** entryPtr = entries.begin(); entryPtr < entries.end(); entryPtr++) {
            Entry& entry = **entryPtr;
            CountBasePtr& typeCount = entry.value();
            RootedValue typeReport(cx);
            if (!typeCount->report(&typeReport))
                return false;

            const char16_t* name = entry.key();
            MOZ_ASSERT(name);
            JSAtom* atom = AtomizeChars(cx, name, js_strlen(name));
            if (!atom)
                return false;
            RootedId entryId(cx, AtomToId(atom));

            if (!DefineProperty(cx, obj, entryId, typeReport))
                return false;
        }

        report.setObject(*obj);
        return true;
    }
};


// A count type that categorizes nodes by the JS stack under which they were
// allocated.
class ByAllocationStack : public CountType {
    typedef HashMap<SavedFrame*, CountBasePtr, DefaultHasher<SavedFrame*>,
                    SystemAllocPolicy> Table;
    typedef Table::Entry Entry;

    struct Count : public CountBase {
        // NOTE: You may look up entries in this table by SavedFrame key only
        // during traversal, NOT ONCE TRAVERSAL IS COMPLETE. Once traversal is
        // complete, you may only iterate over it.
        //
        // In this hash table, keys are JSObjects, and we use JSObject identity
        // (that is, address identity) as key identity. The normal way to
        // support such a table is to make the trace function notice keys that
        // have moved and re-key them in the table. However, our trace function
        // does *not* rehash; the first GC may render the hash table
        // unsearchable.
        //
        // This is as it should be:
        //
        // First, the heap traversal phase needs lookups by key to work. But no
        // GC may ever occur during a traversal; this is enforced by the
        // JS::ubi::BreadthFirst template. So the traceCount function doesn't
        // need to do anything to help traversal; it never even runs then.
        //
        // Second, the report phase needs iteration over the table to work, but
        // never looks up entries by key. GC may well occur during this phase:
        // we allocate a Map object, and probably cross-compartment wrappers for
        // SavedFrame instances as well. If a GC were to occur, it would call
        // our traceCount function; if traceCount were to re-key, that would
        // ruin the traversal in progress.
        //
        // So depending on the phase, we either don't need re-keying, or
        // can't abide it.
        Table table;
        CountBasePtr noStack;

        Count(CountType& type, CountBasePtr& noStack)
          : CountBase(type),
            noStack(Move(noStack))
        { }
        bool init() { return table.init(); }
    };

    CountTypePtr entryType;
    CountTypePtr noStackType;

  public:
    ByAllocationStack(Census& census, CountTypePtr& entryType, CountTypePtr& noStackType)
      : CountType(census),
        entryType(Move(entryType)),
        noStackType(Move(noStackType))
    { }

    CountBasePtr makeCount() override {
        CountBasePtr noStackCount(noStackType->makeCount());
        if (!noStackCount)
            return nullptr;

        UniquePtr<Count> count(census.new_<Count>(*this, noStackCount));
        if (!count || !count->init())
            return nullptr;
        return CountBasePtr(count.release());
    }

    void traceCount(CountBase& countBase, JSTracer* trc) override {
        Count& count= static_cast<Count&>(countBase);
        for (Table::Range r = count.table.all(); !r.empty(); r.popFront()) {
            // Trace our child Counts.
            r.front().value()->trace(trc);

            // Trace the SavedFrame that is this entry's key. Do not re-key if
            // it has moved; see comments for ByAllocationStack::Count::table.
            SavedFrame** keyPtr = const_cast<SavedFrame**>(&r.front().key());
            TraceRoot(trc, keyPtr, "Debugger.Memory.prototype.census byAllocationStack count key");
        }
        count.noStack->trace(trc);
    }

    void destructCount(CountBase& countBase) override {
        Count& count = static_cast<Count&>(countBase);
        count.~Count();
    }

    bool count(CountBase& countBase, const Node& node) {
        Count& count = static_cast<Count&>(countBase);
        count.total_++;

        SavedFrame* allocationStack = nullptr;
        if (node.is<JSObject>()) {
            JSObject* metadata = GetObjectMetadata(node.as<JSObject>());
            if (metadata && metadata->is<SavedFrame>())
                allocationStack = &metadata->as<SavedFrame>();
        }
        // If any other types had allocation site data, we could retrieve it
        // here.

        // If we do have an allocation stack for this node, include it in the
        // count for that stack.
        if (allocationStack) {
            Table::AddPtr p = count.table.lookupForAdd(allocationStack);
            if (!p) {
                CountBasePtr stackCount(entryType->makeCount());
                if (!stackCount || !count.table.add(p, allocationStack, Move(stackCount)))
                    return false;
            }
            return p->value()->count(node);
        }

        // Otherwise, count it in the "no stack" category.
        return count.noStack->count(node);
    }

    bool report(CountBase& countBase, MutableHandleValue report) override {
        Count& count = static_cast<Count&>(countBase);
        JSContext* cx = census.cx;

#ifdef DEBUG
        // Check that nothing rehashes our table while we hold pointers into it.
        uint32_t generation = count.table.generation();
#endif

        // Build a vector of pointers to entries; sort by total; and then use
        // that to build the result object. This makes the ordering of entries
        // more interesting, and a little less non-deterministic.
        mozilla::Vector<Entry*> entries;
        if (!entries.reserve(count.table.count()))
            return false;
        for (Table::Range r = count.table.all(); !r.empty(); r.popFront())
            entries.infallibleAppend(&r.front());
        qsort(entries.begin(), entries.length(), sizeof(*entries.begin()), compareEntries<Entry>);

        // Now build the result by iterating over the sorted vector.
        Rooted<MapObject*> map(cx, MapObject::create(cx));
        if (!map)
            return false;
        for (Entry** entryPtr = entries.begin(); entryPtr < entries.end(); entryPtr++) {
            Entry& entry = **entryPtr;

            MOZ_ASSERT(entry.key());
            RootedValue stack(cx, ObjectValue(*entry.key()));
            if (!cx->compartment()->wrap(cx, &stack))
                return false;

            CountBasePtr& stackCount = entry.value();
            RootedValue stackReport(cx);
            if (!stackCount->report(&stackReport))
                return false;

            if (!MapObject::set(cx, map, stack, stackReport))
                return false;
        }

        if (count.noStack->total_ > 0) {
            RootedValue noStackReport(cx);
            if (!count.noStack->report(&noStackReport))
                return false;
            RootedValue noStack(cx, StringValue(cx->names().noStack));
            if (!MapObject::set(cx, map, noStack, noStackReport))
                return false;
        }

        MOZ_ASSERT(generation == count.table.generation());

        report.setObject(*map);
        return true;
    }
};


// A BreadthFirst handler type that conducts a census, using a CountBase to
// categorize and count each node.
class CensusHandler {
    Census& census;
    CountBasePtr& rootCount;

  public:
    CensusHandler(Census& census, CountBasePtr& rootCount)
      : census(census),
        rootCount(rootCount)
    { }

    bool report(MutableHandleValue report) {
        return rootCount->report(report);
    }

    // This class needs to retain no per-node data.
    class NodeData { };

    bool operator() (BreadthFirst<CensusHandler>& traversal,
                     Node origin, const Edge& edge,
                     NodeData* referentData, bool first)
    {
        // We're only interested in the first time we reach edge.referent, not
        // in every edge arriving at that node.
        if (!first)
            return true;

        // Don't count nodes outside the debuggee zones. Do count things in the
        // special atoms zone, but don't traverse their outgoing edges, on the
        // assumption that they are shared resources that debuggee is using.
        // Symbols are always allocated in the atoms zone, even if they were
        // created for exactly one compartment and never shared; this rule will
        // include such nodes in the count.
        const Node& referent = edge.referent;
        Zone* zone = referent.zone();

        if (census.debuggeeZones.has(zone)) {
            return rootCount->count(referent);
        }

        if (zone == census.atomsZone) {
            traversal.abandonReferent();
            return rootCount->count(referent);
        }

        traversal.abandonReferent();
        return true;
    }
};

typedef BreadthFirst<CensusHandler> CensusTraversal;

static CountTypePtr ParseBreakdown(Census& census, HandleValue breakdownValue);

static CountTypePtr
ParseChildBreakdown(Census& census, HandleObject breakdown, PropertyName* prop)
{
    JSContext* cx = census.cx;
    RootedValue v(cx);
    if (!GetProperty(cx, breakdown, breakdown, prop, &v))
        return nullptr;
    return ParseBreakdown(census, v);
}

static CountTypePtr
ParseBreakdown(Census& census, HandleValue breakdownValue)
{
    JSContext* cx = census.cx;

    if (breakdownValue.isUndefined()) {
        // Construct the default type, { by: 'count' }
        CountTypePtr simple(census.new_<SimpleCount>(census));
        return simple;
    }

    RootedObject breakdown(cx, ToObject(cx, breakdownValue));
    if (!breakdown)
        return nullptr;

    RootedValue byValue(cx);
    if (!GetProperty(cx, breakdown, breakdown, cx->names().by, &byValue))
        return nullptr;
    RootedString byString(cx, ToString(cx, byValue));
    if (!byString)
        return nullptr;
    RootedLinearString by(cx, byString->ensureLinear(cx));
    if (!by)
        return nullptr;

    if (StringEqualsAscii(by, "count")) {
        RootedValue countValue(cx), bytesValue(cx);
        if (!GetProperty(cx, breakdown, breakdown, cx->names().count, &countValue) ||
            !GetProperty(cx, breakdown, breakdown, cx->names().bytes, &bytesValue))
            return nullptr;

        // Both 'count' and 'bytes' default to true if omitted, but ToBoolean
        // naturally treats 'undefined' as false; fix this up.
        if (countValue.isUndefined()) countValue.setBoolean(true);
        if (bytesValue.isUndefined()) bytesValue.setBoolean(true);

        // Undocumented feature, for testing: { by: 'count' } breakdowns can have
        // a 'label' property whose value is converted to a string and included as
        // a 'label' property on the report object.
        RootedValue label(cx);
        if (!GetProperty(cx, breakdown, breakdown, cx->names().label, &label))
            return nullptr;

        UniquePtr<char16_t[], JS::FreePolicy> labelUnique(nullptr);
        if (!label.isUndefined()) {
            RootedString labelString(cx, ToString(cx, label));
            if (!labelString)
                return nullptr;

            JSFlatString* flat = labelString->ensureFlat(cx);
            if (!flat)
                return nullptr;

            AutoStableStringChars chars(cx);
            if (!chars.initTwoByte(cx, flat))
                return nullptr;

            // Since flat strings are null-terminated, and AutoStableStringChars
            // null- terminates if it needs to make a copy, we know that
            // chars.twoByteChars() is null-terminated.
            labelUnique = DuplicateString(cx, chars.twoByteChars());
            if (!labelUnique)
                return nullptr;
        }

        CountTypePtr simple(census.new_<SimpleCount>(census,
                                                     labelUnique,
                                                     ToBoolean(countValue),
                                                     ToBoolean(bytesValue)));
        return simple;
    }

    if (StringEqualsAscii(by, "objectClass")) {
        CountTypePtr thenType(ParseChildBreakdown(census, breakdown, cx->names().then));
        if (!thenType)
            return nullptr;

        CountTypePtr otherType(ParseChildBreakdown(census, breakdown, cx->names().other));
        if (!otherType)
            return nullptr;

        return CountTypePtr(census.new_<ByObjectClass>(census, thenType, otherType));
    }

    if (StringEqualsAscii(by, "coarseType")) {
        CountTypePtr objectsType(ParseChildBreakdown(census, breakdown, cx->names().objects));
        if (!objectsType)
            return nullptr;
        CountTypePtr scriptsType(ParseChildBreakdown(census, breakdown, cx->names().scripts));
        if (!scriptsType)
            return nullptr;
        CountTypePtr stringsType(ParseChildBreakdown(census, breakdown, cx->names().strings));
        if (!stringsType)
            return nullptr;
        CountTypePtr otherType(ParseChildBreakdown(census, breakdown, cx->names().other));
        if (!otherType)
            return nullptr;

        return CountTypePtr(census.new_<ByCoarseType>(census,
                                                      objectsType,
                                                      scriptsType,
                                                      stringsType,
                                                      otherType));
    }

    if (StringEqualsAscii(by, "internalType")) {
        CountTypePtr thenType(ParseChildBreakdown(census, breakdown, cx->names().then));
        if (!thenType)
            return nullptr;

        return CountTypePtr(census.new_<ByUbinodeType>(census, thenType));
    }

    if (StringEqualsAscii(by, "allocationStack")) {
        CountTypePtr thenType(ParseChildBreakdown(census, breakdown, cx->names().then));
        if (!thenType)
            return nullptr;
        CountTypePtr noStackType(ParseChildBreakdown(census, breakdown, cx->names().noStack));
        if (!noStackType)
            return nullptr;

        return CountTypePtr(census.new_<ByAllocationStack>(census, thenType, noStackType));
    }

    // We didn't recognize the breakdown type; complain.
    RootedString bySource(cx, ValueToSource(cx, byValue));
    if (!bySource)
        return nullptr;

    JSAutoByteString byBytes(cx, bySource);
    if (!byBytes)
        return nullptr;

    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_DEBUG_CENSUS_BREAKDOWN,
                         byBytes.ptr());
    return nullptr;
}

} // namespace dbg
} // namespace js

using dbg::Census;
using dbg::CountTypePtr;
using dbg::CountBasePtr;

bool
DebuggerMemory::takeCensus(JSContext* cx, unsigned argc, Value* vp)
{
    THIS_DEBUGGER_MEMORY(cx, argc, vp, "Debugger.Memory.prototype.census", args, memory);

    Census census(cx);
    if (!census.init())
        return false;
    CountTypePtr rootType;

    RootedObject options(cx);
    if (args.get(0).isObject())
        options = &args[0].toObject();

    {
        RootedValue breakdown(cx, UndefinedValue());
        if (options && !GetProperty(cx, options, options, cx->names().breakdown, &breakdown))
            return false;
        if (!breakdown.isUndefined()) {
            rootType = ParseBreakdown(census, breakdown);
        } else {
            // Manually build the default takeCensus query:
            // { by: "coarseType",
            //   objects: { by: "objectClass" },
            //   other:   { by: "internalType" }
            // }
            CountTypePtr byClass(census.new_<dbg::SimpleCount>(census));
            CountTypePtr byClassElse(census.new_<dbg::SimpleCount>(census));
            CountTypePtr objects(census.new_<dbg::ByObjectClass>(census,
                                                                 byClass,
                                                                 byClassElse));

            CountTypePtr scripts(census.new_<dbg::SimpleCount>(census));
            CountTypePtr strings(census.new_<dbg::SimpleCount>(census));

            CountTypePtr byType(census.new_<dbg::SimpleCount>(census));
            CountTypePtr other(census.new_<dbg::ByUbinodeType>(census, byType));

            rootType = CountTypePtr(census.new_<dbg::ByCoarseType>(census,
                                                                   objects,
                                                                   scripts,
                                                                   strings,
                                                                   other));
        }
    }

    if (!rootType)
        return false;

    dbg::RootedCount rootCount(cx, rootType->makeCount());
    if (!rootCount)
        return false;
    dbg::CensusHandler handler(census, rootCount);

    Debugger* dbg = memory->getDebugger();
    RootedObject dbgObj(cx, dbg->object);

    // Populate our target set of debuggee zones.
    for (WeakGlobalObjectSet::Range r = dbg->allDebuggees(); !r.empty(); r.popFront()) {
        if (!census.debuggeeZones.put(r.front()->zone()))
            return false;
    }

    {
        Maybe<JS::AutoCheckCannotGC> maybeNoGC;
        JS::ubi::RootList rootList(cx, maybeNoGC);
        if (!rootList.init(dbgObj))
            return false;

        dbg::CensusTraversal traversal(cx, handler, maybeNoGC.ref());
        if (!traversal.init())
            return false;
        traversal.wantNames = false;

        if (!traversal.addStart(JS::ubi::Node(&rootList)) ||
            !traversal.traverse())
        {
            return false;
        }
    }

    return handler.report(args.rval());
}


/* Debugger.Memory property and method tables. */


/* static */ const JSPropertySpec DebuggerMemory::properties[] = {
    JS_PSGS("trackingAllocationSites", getTrackingAllocationSites, setTrackingAllocationSites, 0),
    JS_PSGS("maxAllocationsLogLength", getMaxAllocationsLogLength, setMaxAllocationsLogLength, 0),
    JS_PSGS("allocationSamplingProbability", getAllocationSamplingProbability, setAllocationSamplingProbability, 0),
    JS_PSG("allocationsLogOverflowed", getAllocationsLogOverflowed, 0),
    JS_PSGS("onGarbageCollection", getOnGarbageCollection, setOnGarbageCollection, 0),
    JS_PS_END
};

/* static */ const JSFunctionSpec DebuggerMemory::methods[] = {
    JS_FN("drainAllocationsLog", DebuggerMemory::drainAllocationsLog, 0, 0),
    JS_FN("takeCensus", takeCensus, 0, 0),
    JS_FS_END
};
