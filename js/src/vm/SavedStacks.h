
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_SavedStacks_h
#define vm_SavedStacks_h

#include "jscntxt.h"
#include "js/HashTable.h"
#include "vm/Stack.h"

namespace js {

class SavedFrame : public JSObject {
    friend class SavedStacks;

  public:
    static const Class          class_;
    static void finalize(FreeOp *fop, JSObject *obj);

    // Prototype methods and properties to be exposed to JS.
    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];
    static bool construct(JSContext *cx, unsigned argc, Value *vp);
    static bool sourceProperty(JSContext *cx, unsigned argc, Value *vp);
    static bool lineProperty(JSContext *cx, unsigned argc, Value *vp);
    static bool columnProperty(JSContext *cx, unsigned argc, Value *vp);
    static bool functionDisplayNameProperty(JSContext *cx, unsigned argc, Value *vp);
    static bool parentProperty(JSContext *cx, unsigned argc, Value *vp);
    static bool toStringMethod(JSContext *cx, unsigned argc, Value *vp);

    // Convenient getters for SavedFrame's reserved slots for use from C++.
    JSAtom       *getSource();
    size_t       getLine();
    size_t       getColumn();
    JSAtom       *getFunctionDisplayName();
    SavedFrame   *getParent();
    JSPrincipals *getPrincipals();

    bool         isSelfHosted();

    struct Lookup;
    struct HashPolicy;

    typedef HashSet<SavedFrame *,
                    HashPolicy,
                    SystemAllocPolicy> Set;

  private:
    void initFromLookup(Lookup &lookup);

    enum {
        // The reserved slots in the SavedFrame class.
        JSSLOT_SOURCE,
        JSSLOT_LINE,
        JSSLOT_COLUMN,
        JSSLOT_FUNCTIONDISPLAYNAME,
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
    bool         parentMoved();
    void         updatePrivateParent();

    static SavedFrame *checkThis(JSContext *cx, CallArgs &args, const char *fnName);
};

struct SavedFrame::Lookup {
    Lookup(JSAtom *source, size_t line, size_t column, JSAtom *functionDisplayName,
           Handle<SavedFrame*> parent, JSPrincipals *principals)
        : source(source),
          line(line),
          column(column),
          functionDisplayName(functionDisplayName),
          parent(parent),
          principals(principals)
    {
        JS_ASSERT(source);
    }

    JSAtom              *source;
    size_t              line;
    size_t              column;
    JSAtom              *functionDisplayName;
    Handle<SavedFrame*> parent;
    JSPrincipals        *principals;
};

struct SavedFrame::HashPolicy
{
    typedef SavedFrame::Lookup               Lookup;
    typedef PointerHasher<SavedFrame *, 3>   SavedFramePtrHasher;
    typedef PointerHasher<JSPrincipals *, 3> JSPrincipalsPtrHasher;

    static HashNumber hash(const Lookup &lookup);
    static bool       match(SavedFrame *existing, const Lookup &lookup);

    typedef SavedFrame* Key;
    static void rekey(Key &key, const Key &newKey);
};

class SavedStacks {
  public:
    SavedStacks() : frames(), savedFrameProto(nullptr) { }

    bool     init();
    bool     initialized() const { return frames.initialized(); }
    bool     saveCurrentStack(JSContext *cx, MutableHandle<SavedFrame*> frame);
    void     sweep(JSRuntime *rt);
    uint32_t count();
    void     clear();

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);

  private:
    SavedFrame::Set          frames;
    JSObject                 *savedFrameProto;

    bool       insertFrames(JSContext *cx, ScriptFrameIter &iter, MutableHandle<SavedFrame*> frame);
    SavedFrame *getOrCreateSavedFrame(JSContext *cx, SavedFrame::Lookup &lookup);
    // |SavedFrame.prototype| is created lazily and held weakly. It should only
    // be accessed through this method.
    JSObject   *getOrCreateSavedFramePrototype(JSContext *cx);
    SavedFrame *createFrameFromLookup(JSContext *cx, SavedFrame::Lookup &lookup);
};

bool SavedStacksMetadataCallback(JSContext *cx, JSObject **pmetadata);

} /* namespace js */

#endif /* vm_SavedStacks_h */
