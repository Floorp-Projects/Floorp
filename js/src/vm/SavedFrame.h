/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_SavedFrame_h
#define vm_SavedFrame_h

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

} // namespace js

#endif // vm_SavedFrame_h
