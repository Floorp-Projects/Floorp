/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_SavedFrame_h
#define vm_SavedFrame_h

#include "js/UbiNode.h"

namespace js {

class SavedFrame : public NativeObject {
    friend class SavedStacks;

  public:
    static const Class          class_;
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

    static void finalize(FreeOp* fop, JSObject* obj);

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

        // The total number of reserved slots in the SavedFrame class.
        JSSLOT_COUNT
    };

    static bool checkThis(JSContext* cx, CallArgs& args, const char* fnName,
                          MutableHandleObject frame);
};

struct SavedFrame::HashPolicy
{
    typedef SavedFrame::Lookup              Lookup;
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

// When we reconstruct a SavedFrame stack from a JS::ubi::StackFrame, we may not
// have access to the principals that the original stack was captured
// with. Instead, we use these two singleton principals based on whether
// JS::ubi::StackFrame::isSystem or not. These singletons should never be passed
// to the subsumes callback, and should be special cased with a shortcut before
// that.
struct ReconstructedSavedFramePrincipals : public JSPrincipals
{
    explicit ReconstructedSavedFramePrincipals()
        : JSPrincipals()
    {
        MOZ_ASSERT(is(this));
        this->refcount = 1;
    }

    static ReconstructedSavedFramePrincipals IsSystem;
    static ReconstructedSavedFramePrincipals IsNotSystem;

    // Return true if the given JSPrincipals* points to one of the
    // ReconstructedSavedFramePrincipals singletons, false otherwise.
    static bool is(JSPrincipals* p) { return p == &IsSystem || p == &IsNotSystem;}

    // Get the appropriate ReconstructedSavedFramePrincipals singleton for the
    // given JS::ubi::StackFrame that is being reconstructed as a SavedFrame
    // stack.
    static JSPrincipals* getSingleton(JS::ubi::StackFrame& f) {
        return f.isSystem() ? &IsSystem : &IsNotSystem;
    }
};

} // namespace js

namespace JS {
namespace ubi {

using js::SavedFrame;

// A concrete JS::ubi::StackFrame that is backed by a live SavedFrame object.
template<>
class ConcreteStackFrame<SavedFrame> : public BaseStackFrame {
    explicit ConcreteStackFrame(SavedFrame* ptr) : BaseStackFrame(ptr) { }
    SavedFrame& get() const { return *static_cast<SavedFrame*>(ptr); }

  public:
    static void construct(void* storage, SavedFrame* ptr) { new (storage) ConcreteStackFrame(ptr); }

    StackFrame parent() const override { return get().getParent(); }
    uint32_t line() const override { return get().getLine(); }
    uint32_t column() const override { return get().getColumn(); }

    AtomOrTwoByteChars source() const override {
        auto source = get().getSource();
        return AtomOrTwoByteChars(source);
    }

    AtomOrTwoByteChars functionDisplayName() const override {
        auto name = get().getFunctionDisplayName();
        return AtomOrTwoByteChars(name);
    }

    void trace(JSTracer* trc) override {
        JSObject* prev = &get();
        JSObject* next = prev;
        js::TraceRoot(trc, &next, "ConcreteStackFrame<SavedFrame>::ptr");
        if (next != prev)
            ptr = next;
    }

    bool isSelfHosted() const override { return get().isSelfHosted(); }

    bool isSystem() const override;

    bool constructSavedFrameStack(JSContext* cx,
                                 MutableHandleObject outSavedFrameStack) const override;
};

} // namespace ubi
} // namespace JS

#endif // vm_SavedFrame_h
