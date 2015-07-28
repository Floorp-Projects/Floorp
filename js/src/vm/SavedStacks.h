/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_SavedStacks_h
#define vm_SavedStacks_h

#include "jscntxt.h"
#include "jsmath.h"
#include "jswrapper.h"
#include "js/HashTable.h"
#include "vm/Stack.h"

// # Saved Stacks
//
// The `SavedStacks` class provides a compact way to capture and save JS stacks
// as `SavedFrame` `JSObject` subclasses. A single `SavedFrame` object
// represents one frame that was on the stack, and has a strong reference to its
// parent `SavedFrame` (the next youngest frame). This reference is null when
// the `SavedFrame` object is the oldest frame that was on the stack.
//
// This comment documents implementation. For usage documentation, see the
// `js/src/doc/SavedFrame/SavedFrame.md` file and relevant `SavedFrame`
// functions in `js/src/jsapi.h`.
//
// ## Compact
//
// Older saved stack frame tails are shared via hash consing, to deduplicate
// structurally identical data. `SavedStacks` contains a hash table of weakly
// held `SavedFrame` objects, and when the owning compartment is swept, it
// removes entries from this table that aren't held alive in any other way. When
// saving new stacks, we use this table to find pre-existing `SavedFrame`
// objects. If such an object is already extant, it is reused; otherwise a new
// `SavedFrame` is allocated and inserted into the table.
//
//    Naive         |   Hash Consing
//    --------------+------------------
//    c -> b -> a   |   c -> b -> a
//                  |        ^
//    d -> b -> a   |   d ---|
//                  |        |
//    e -> b -> a   |   e ---'
//
// This technique is effective because of the nature of the events that trigger
// capturing the stack. Currently, these events consist primarily of `JSObject`
// allocation (when an observing `Debugger` has such tracking), `Promise`
// settlement, and `Error` object creation. While these events may occur many
// times, they tend to occur only at a few locations in the JS source. For
// example, if we enable Object allocation tracking and run the esprima
// JavaScript parser on its own JavaScript source, there are approximately 54700
// total `Object` allocations, but just ~1400 unique JS stacks at allocation
// time. There's only ~200 allocation sites if we capture only the youngest
// stack frame.
//
// ## Security and Wrappers
//
// We save every frame on the stack, regardless of whether the `SavedStack`'s
// compartment's principals subsume the frame's compartment's principals or
// not. This gives us maximum flexibility down the line when accessing and
// presenting captured stacks, but at the price of some complication involved in
// preventing the leakage of privileged stack frames to unprivileged callers.
//
// When a `SavedFrame` method or accessor is called, we compare the caller's
// compartment's principals to each `SavedFrame`'s captured principals. We avoid
// using the usual `CallNonGenericMethod` and `nativeCall` machinery which
// enters the `SavedFrame` object's compartment before we can check these
// principals, because we need access to the original caller's compartment's
// principals (unlike other `CallNonGenericMethod` users) to determine what view
// of the stack to present. Instead, we take a similar approach to that used by
// DOM methods, and manually unwrap wrappers until we get the underlying
// `SavedFrame` object, find the first `SavedFrame` in its stack whose captured
// principals are subsumed by the caller's principals, access the reserved slots
// we care about, and then rewrap return values as necessary.
//
// Consider the following diagram:
//
//                                              Content Compartment
//                                    +---------------------------------------+
//                                    |                                       |
//                                    |           +------------------------+  |
//      Chrome Compartment            |           |                        |  |
//    +--------------------+          |           | SavedFrame C (content) |  |
//    |                    |          |           |                        |  |
//    |                  +--------------+         +------------------------+  |
//    |                  |              |                    ^                |
//    |     var x -----> | Xray Wrapper |-----.              |                |
//    |                  |              |     |              |                |
//    |                  +--------------+     |   +------------------------+  |
//    |                    |          |       |   |                        |  |
//    |                  +--------------+     |   | SavedFrame B (content) |  |
//    |                  |              |     |   |                        |  |
//    |     var y -----> | CCW (waived) |--.  |   +------------------------+  |
//    |                  |              |  |  |              ^                |
//    |                  +--------------+  |  |              |                |
//    |                    |          |    |  |              |                |
//    +--------------------+          |    |  |   +------------------------+  |
//                                    |    |  '-> |                        |  |
//                                    |    |      | SavedFrame A (chrome)  |  |
//                                    |    '----> |                        |  |
//                                    |           +------------------------+  |
//                                    |                      ^                |
//                                    |                      |                |
//                                    |           var z -----'                |
//                                    |                                       |
//                                    +---------------------------------------+
//
// CCW is a plain cross-compartment wrapper, yielded by waiving Xray vision. A
// is the youngest `SavedFrame` and represents a frame that was from the chrome
// compartment, while B and C are from frames from the content compartment. C is
// the oldest frame.
//
// Note that it is always safe to waive an Xray around a SavedFrame object,
// because SavedFrame objects and the SavedFrame prototype are always frozen you
// will never run untrusted code.
//
// Depending on who the caller is, the view of the stack will be different, and
// is summarized in the table below.
//
//    Var  | View
//    -----+------------
//    x    | A -> B -> C
//    y, z | B -> C
//
// In the case of x, the `SavedFrame` accessors are called with an Xray wrapper
// around the `SavedFrame` object as the `this` value, and the chrome
// compartment as the cx's current principals. Because the chrome compartment's
// principals subsume both itself and the content compartment's principals, x
// has the complete view of the stack.
//
// In the case of y, the cross-compartment machinery automatically enters the
// content compartment, and calls the `SavedFrame` accessors with the wrapped
// `SavedFrame` object as the `this` value. Because the cx's current compartment
// is the content compartment, and the content compartment's principals do not
// subsume the chrome compartment's principals, it can only see the B and C
// frames.
//
// In the case of z, the `SavedFrame` accessors are called with the `SavedFrame`
// object in the `this` value, and the content compartment as the cx's current
// compartment. Similar to the case of y, only the B and C frames are exposed
// because the cx's current compartment's principals do not subsume A's captured
// principals.


namespace js {

class SavedFrame;
typedef JS::Handle<SavedFrame*> HandleSavedFrame;
typedef JS::MutableHandle<SavedFrame*> MutableHandleSavedFrame;
typedef JS::Rooted<SavedFrame*> RootedSavedFrame;

class SavedFrame : public NativeObject {
    friend class SavedStacks;

  public:
    static const Class          class_;
    static void finalize(FreeOp* fop, JSObject* obj);
    static const JSPropertySpec protoAccessors[];
    static const JSFunctionSpec protoFunctions[];
    static const JSFunctionSpec staticFunctions[];

    // Prototype methods and properties to be exposed to JS.
    static bool construct(JSContext* cx, unsigned argc, Value* vp);
    static bool sourceProperty(JSContext* cx, unsigned argc, Value* vp);
    static bool lineProperty(JSContext* cx, unsigned argc, Value* vp);
    static bool columnProperty(JSContext* cx, unsigned argc, Value* vp);
    static bool functionDisplayNameProperty(JSContext* cx, unsigned argc, Value* vp);
    static bool asyncCauseProperty(JSContext* cx, unsigned argc, Value* vp);
    static bool asyncParentProperty(JSContext* cx, unsigned argc, Value* vp);
    static bool parentProperty(JSContext* cx, unsigned argc, Value* vp);
    static bool toStringMethod(JSContext* cx, unsigned argc, Value* vp);

    // Convenient getters for SavedFrame's reserved slots for use from C++.
    JSAtom*      getSource();
    uint32_t     getLine();
    uint32_t     getColumn();
    JSAtom*      getFunctionDisplayName();
    JSAtom*      getAsyncCause();
    SavedFrame*  getParent();
    JSPrincipals* getPrincipals();

    bool         isSelfHosted();

    static bool isSavedFrameAndNotProto(JSObject& obj) {
        return obj.is<SavedFrame>() &&
               !obj.as<SavedFrame>().getReservedSlot(JSSLOT_SOURCE).isNull();
    }

    struct Lookup;
    struct HashPolicy;

    typedef HashSet<js::ReadBarriered<SavedFrame*>,
                    HashPolicy,
                    SystemAllocPolicy> Set;

    class AutoLookupVector;

    class MOZ_STACK_CLASS HandleLookup {
        friend class AutoLookupVector;

        Lookup& lookup;

        explicit HandleLookup(Lookup& lookup) : lookup(lookup) { }

      public:
        inline Lookup& get() { return lookup; }
        inline Lookup* operator->() { return &lookup; }
    };

  private:
    static bool finishSavedFrameInit(JSContext* cx, HandleObject ctor, HandleObject proto);
    void initFromLookup(HandleLookup lookup);

    enum {
        // The reserved slots in the SavedFrame class.
        JSSLOT_SOURCE,
        JSSLOT_LINE,
        JSSLOT_COLUMN,
        JSSLOT_FUNCTIONDISPLAYNAME,
        JSSLOT_ASYNCCAUSE,
        JSSLOT_PARENT,
        JSSLOT_PRINCIPALS,
        JSSLOT_PRIVATE_PARENT,

        // The total number of reserved slots in the SavedFrame class.
        JSSLOT_COUNT
    };

    // Because we hash the parent pointer, we need to rekey a saved frame
    // whenever its parent was relocated by the GC. However, the GC doesn't
    // notify us when this occurs. As a work around, we keep a duplicate copy of
    // the parent pointer as a private value in a reserved slot. Whenever the
    // private value parent pointer doesn't match the regular parent pointer, we
    // know that GC moved the parent and we need to update our private value and
    // rekey the saved frame in its hash set. These two methods are helpers for
    // this process.
    bool parentMoved();
    void updatePrivateParent();

    static bool checkThis(JSContext* cx, CallArgs& args, const char* fnName,
                          MutableHandleObject frame);
};

struct SavedFrame::HashPolicy
{
    typedef SavedFrame::Lookup               Lookup;
    typedef PointerHasher<SavedFrame*, 3>   SavedFramePtrHasher;
    typedef PointerHasher<JSPrincipals*, 3> JSPrincipalsPtrHasher;

    static HashNumber hash(const Lookup& lookup);
    static bool       match(SavedFrame* existing, const Lookup& lookup);

    typedef ReadBarriered<SavedFrame*> Key;
    static void rekey(Key& key, const Key& newKey);
};

// Assert that if the given object is not null, that it must be either a
// SavedFrame object or wrapper (Xray or CCW) around a SavedFrame object.
inline void AssertObjectIsSavedFrameOrWrapper(JSContext* cx, HandleObject stack);

class SavedStacks {
    friend JSObject* SavedStacksMetadataCallback(JSContext* cx, JSObject* target);

  public:
    SavedStacks()
      : frames(),
        allocationSamplingProbability(1.0),
        allocationSkipCount(0),
        rngState(0),
        creatingSavedFrame(false)
    { }

    bool     init();
    bool     initialized() const { return frames.initialized(); }
    bool     saveCurrentStack(JSContext* cx, MutableHandleSavedFrame frame, unsigned maxFrameCount = 0);
    void     sweep(JSRuntime* rt);
    void     trace(JSTracer* trc);
    uint32_t count();
    void     clear();
    void     setRNGState(uint64_t state) { rngState = state; }

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);

  private:
    SavedFrame::Set frames;
    double          allocationSamplingProbability;
    uint32_t        allocationSkipCount;
    uint64_t        rngState;
    bool            creatingSavedFrame;

    // Similar to mozilla::ReentrancyGuard, but instead of asserting against
    // reentrancy, just change the behavior of SavedStacks::saveCurrentStack to
    // return a nullptr SavedFrame.
    struct MOZ_STACK_CLASS AutoReentrancyGuard {
        MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;
        SavedStacks& stacks;

        explicit AutoReentrancyGuard(SavedStacks& stacks MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
            : stacks(stacks)
        {
            MOZ_GUARD_OBJECT_NOTIFIER_INIT;
            stacks.creatingSavedFrame = true;
        }

        ~AutoReentrancyGuard()
        {
            stacks.creatingSavedFrame = false;
        }
    };

    bool       insertFrames(JSContext* cx, FrameIter& iter, MutableHandleSavedFrame frame,
                            unsigned maxFrameCount = 0);
    bool       adoptAsyncStack(JSContext* cx, HandleSavedFrame asyncStack,
                               HandleString asyncCause,
                               MutableHandleSavedFrame adoptedStack,
                               unsigned maxFrameCount);
    SavedFrame* getOrCreateSavedFrame(JSContext* cx, SavedFrame::HandleLookup lookup);
    SavedFrame* createFrameFromLookup(JSContext* cx, SavedFrame::HandleLookup lookup);
    void       chooseSamplingProbability(JSContext* cx);

    // Cache for memoizing PCToLineNumber lookups.

    struct PCKey {
        PCKey(JSScript* script, jsbytecode* pc) : script(script), pc(pc) { }

        PreBarrieredScript script;
        jsbytecode*        pc;
    };

    struct LocationValue {
        LocationValue() : source(nullptr), line(0), column(0) { }
        LocationValue(JSAtom* source, size_t line, uint32_t column)
            : source(source),
              line(line),
              column(column)
        { }

        void trace(JSTracer* trc) {
            if (source)
                TraceEdge(trc, &source, "SavedStacks::LocationValue::source");
        }

        PreBarrieredAtom source;
        size_t           line;
        uint32_t         column;
    };

    class MOZ_STACK_CLASS AutoLocationValueRooter : public JS::CustomAutoRooter
    {
      public:
        explicit AutoLocationValueRooter(JSContext* cx)
            : JS::CustomAutoRooter(cx),
              value() {}

        inline LocationValue* operator->() { return &value; }
        void set(LocationValue& loc) { value = loc; }
        LocationValue& get() { return value; }

      private:
        virtual void trace(JSTracer* trc) {
            value.trace(trc);
        }

        SavedStacks::LocationValue value;
    };

    class MOZ_STACK_CLASS MutableHandleLocationValue
    {
      public:
        inline MOZ_IMPLICIT MutableHandleLocationValue(AutoLocationValueRooter* location)
            : location(location) {}

        inline LocationValue* operator->() { return &location->get(); }
        void set(LocationValue& loc) { location->set(loc); }

      private:
        AutoLocationValueRooter* location;
    };

    struct PCLocationHasher : public DefaultHasher<PCKey> {
        typedef PointerHasher<JSScript*, 3>   ScriptPtrHasher;
        typedef PointerHasher<jsbytecode*, 3> BytecodePtrHasher;

        static HashNumber hash(const PCKey& key) {
            return mozilla::AddToHash(ScriptPtrHasher::hash(key.script),
                                      BytecodePtrHasher::hash(key.pc));
        }

        static bool match(const PCKey& l, const PCKey& k) {
            return l.script == k.script && l.pc == k.pc;
        }
    };

    typedef HashMap<PCKey, LocationValue, PCLocationHasher, SystemAllocPolicy> PCLocationMap;

    PCLocationMap pcLocationMap;

    void sweepPCLocationMap();
    bool getLocation(JSContext* cx, const FrameIter& iter, MutableHandleLocationValue locationp);
};

JSObject* SavedStacksMetadataCallback(JSContext* cx, JSObject* target);

} /* namespace js */

#endif /* vm_SavedStacks_h */
